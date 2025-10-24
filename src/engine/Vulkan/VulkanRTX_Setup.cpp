// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Vulkan ray-tracing setup and management.
// Dependencies: Vulkan 1.3+, VK_KHR_acceleration_structure, VK_KHR_ray_tracing_pipeline, GLM, C++20 standard library, logging.hpp.
// Supported platforms: Linux, Windows (AMD, NVIDIA, Intel GPUs only).
// Zachary Geurts 2025

#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>
#include <vector>
#include <array>
#include <thread>
#include <filesystem>
#include <fstream>
#include <source_location>
#include <dlfcn.h>
#include "engine/logging.hpp"

#define VK_CHECK(result, msg) if ((result) != VK_SUCCESS) { \
    LOG_ERROR_CAT("VulkanRTX", "Vulkan error: {} (VkResult: {})", (msg), static_cast<int>(result), std::source_location::current()); \
    throw VulkanRTXException((msg)); \
}

namespace VulkanRTX {

std::atomic<bool> VulkanRTX::functionPtrInitialized_{false};
std::atomic<bool> VulkanRTX::shaderModuleInitialized_{false};
std::mutex VulkanRTX::functionPtrMutex_;
std::mutex VulkanRTX::shaderModuleMutex_;

VulkanRTX::ShaderBindingTable::ShaderBindingTable(VkDevice device, VkBuffer buffer, VkDeviceMemory memory,
                                                 PFN_vkDestroyBuffer destroyBuffer, PFN_vkFreeMemory freeMemory)
    : buffer(device, buffer, destroyBuffer),
      memory(device, memory, freeMemory),
      raygen{0, 0, 0},
      miss{0, 0, 0},
      hit{0, 0, 0},
      callable{0, 0, 0} {}

VulkanRTX::VulkanRTX(VkDevice device, VkPhysicalDevice physicalDevice, const std::vector<std::string>& shaderPaths)
    : device_(device),
      physicalDevice_(physicalDevice),
      shaderPaths_(shaderPaths),
      dsLayout_(),
      dsPool_(),
      ds_(),
      rtPipelineLayout_(),
      rtPipeline_(),
      blasBuffer_(),
      blasMemory_(),
      tlasBuffer_(),
      tlasMemory_(),
      blas_(),
      tlas_(),
      extent_{0, 0},
      primitiveCounts_(),
      previousPrimitiveCounts_(),
      previousDimensionCache_(),
      supportsCompaction_(false),
      shaderFeatures_(ShaderFeatures::None),
      numShaderGroups_(0),
      sbt_(),
      scratchAlignment_(0) {
    if (!device || !physicalDevice) {
        LOG_ERROR_CAT("VulkanRTX", "Invalid device or physical device", std::source_location::current());
        throw VulkanRTXException("Invalid device or physical device");
    }

    for (const auto& path : shaderPaths_) {
        std::filesystem::path shaderPath = std::filesystem::absolute(path);

        if (!std::filesystem::exists(shaderPath)) {
            LOG_ERROR_CAT("VulkanRTX", "Shader file does not exist: {}", shaderPath.string(), std::source_location::current());
            throw VulkanRTXException("Shader file does not exist: " + shaderPath.string());
        }

        if (!std::filesystem::is_regular_file(shaderPath)) {
            LOG_ERROR_CAT("VulkanRTX", "Shader path '{}' is not a regular file (e.g., is a directory)", shaderPath.string(), std::source_location::current());
            throw VulkanRTXException("Shader path is not a regular file: " + shaderPath.string());
        }

        std::filesystem::file_status status = std::filesystem::status(shaderPath);
        auto perms = status.permissions();
        if ((perms & std::filesystem::perms::owner_read) == std::filesystem::perms::none) {
            LOG_ERROR_CAT("VulkanRTX", "Shader file '{}' lacks read permissions", shaderPath.string(), std::source_location::current());
            throw VulkanRTXException("Shader file lacks read permissions: " + shaderPath.string());
        }
    }

    shaderFeatures_ = ShaderFeatures::None;
    for (const auto& path : shaderPaths_) {
        std::string ext = std::filesystem::path(path).extension().string();
        if (ext == ".rgen") shaderFeatures_ |= ShaderFeatures::Raygen;
        if (ext == ".rmiss") shaderFeatures_ |= ShaderFeatures::Miss;
        if (ext == ".rchit") shaderFeatures_ |= ShaderFeatures::ClosestHit;
        if (ext == ".rahit") shaderFeatures_ |= ShaderFeatures::AnyHit;
        if (ext == ".rint") shaderFeatures_ |= ShaderFeatures::Intersection;
        if (ext == ".rcall") shaderFeatures_ |= ShaderFeatures::Callable;
    }

    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR,
        .pNext = nullptr,
        .maxGeometryCount = 0,
        .maxInstanceCount = 0,
        .maxPrimitiveCount = 0,
        .maxPerStageDescriptorAccelerationStructures = 0,
        .maxPerStageDescriptorUpdateAfterBindAccelerationStructures = 0,
        .maxDescriptorSetAccelerationStructures = 0,
        .maxDescriptorSetUpdateAfterBindAccelerationStructures = 0,
        .minAccelerationStructureScratchOffsetAlignment = 0
    };
    VkPhysicalDeviceProperties2 properties2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &asProperties,
        .properties = {}
    };
    vkGetPhysicalDeviceProperties2(physicalDevice_, &properties2);
    setSupportsCompaction(asProperties.maxGeometryCount > 0);
    scratchAlignment_ = asProperties.minAccelerationStructureScratchOffsetAlignment;
}

VulkanRTX::~VulkanRTX() {
    // Resources are automatically cleaned up by VulkanResource destructors
}

bool VulkanRTX::shaderFileExists(const std::string& filename) const {
    return std::filesystem::exists(filename);
}

VkShaderModule VulkanRTX::createShaderModule(const std::string& filename) {
    std::filesystem::path shaderPath = std::filesystem::absolute(filename);

    if (!std::filesystem::exists(shaderPath)) {
        LOG_ERROR_CAT("VulkanRTX", "Shader file does not exist: {}", shaderPath.string(), std::source_location::current());
        throw VulkanRTXException("Shader file does not exist: " + shaderPath.string());
    }

    if (!std::filesystem::is_regular_file(shaderPath)) {
        LOG_ERROR_CAT("VulkanRTX", "Shader path '{}' is not a regular file (e.g., is a directory)", shaderPath.string(), std::source_location::current());
        throw VulkanRTXException("Shader path is not a regular file: " + shaderPath.string());
    }

    std::filesystem::file_status status = std::filesystem::status(shaderPath);
    auto perms = status.permissions();
    if ((perms & std::filesystem::perms::owner_read) == std::filesystem::perms::none) {
        LOG_ERROR_CAT("VulkanRTX", "Shader file '{}' lacks read permissions", shaderPath.string(), std::source_location::current());
        throw VulkanRTXException("Shader file lacks read permissions: " + shaderPath.string());
    }

    std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        std::string errorMsg = std::strerror(errno);
        LOG_ERROR_CAT("VulkanRTX", "Failed to open shader file '{}': {}", shaderPath.string(), errorMsg, std::source_location::current());
        throw VulkanRTXException("Failed to open shader file: " + shaderPath.string() + ", reason: " + errorMsg);
    }

    size_t fileSize = static_cast<size_t>(file.tellg());

    if (fileSize == 0) {
        file.close();
        LOG_ERROR_CAT("VulkanRTX", "Shader file '{}' is empty", shaderPath.string(), std::source_location::current());
        throw VulkanRTXException("Shader file is empty: " + shaderPath.string());
    }

    if (fileSize % 4 != 0) {
        file.close();
        LOG_ERROR_CAT("VulkanRTX", "Invalid SPIR-V file size (not multiple of 4): {} bytes for '{}'", fileSize, shaderPath.string(), std::source_location::current());
        throw VulkanRTXException("Invalid SPIR-V file size: " + shaderPath.string());
    }

    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    if (file.fail()) {
        std::string errorMsg = std::strerror(errno);
        file.close();
        LOG_ERROR_CAT("VulkanRTX", "Failed to read shader file '{}': {}", shaderPath.string(), errorMsg, std::source_location::current());
        throw VulkanRTXException("Failed to read shader file: " + shaderPath.string() + ", reason: " + errorMsg);
    }
    file.close();

    if (fileSize >= 4) {
        uint32_t magic = *reinterpret_cast<const uint32_t*>(buffer.data());
        if (magic != 0x07230203) {
            LOG_ERROR_CAT("VulkanRTX", "Invalid SPIR-V magic number (expected 0x07230203, got 0x{:08x}) for '{}'", magic, shaderPath.string(), std::source_location::current());
            throw VulkanRTXException("Invalid SPIR-V magic number for: " + shaderPath.string());
        }
    }

    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = buffer.size(),
        .pCode = reinterpret_cast<const uint32_t*>(buffer.data())
    };

    VkShaderModule shaderModule = VK_NULL_HANDLE;
    VkResult result = vkCreateShaderModule(device_, &createInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRTX", "Failed to create shader module for '{}': VkResult={}", shaderPath.string(), static_cast<int>(result), std::source_location::current());
        throw VulkanRTXException("Failed to create shader module for: " + shaderPath.string() + ", VkResult=" + std::to_string(static_cast<int>(result)));
    }

    return shaderModule;
}

void VulkanRTX::loadShadersAsync(std::vector<VkShaderModule>& modules, const std::vector<std::string>& paths) {
    std::vector<std::thread> threads;
    modules.resize(paths.size());
    for (size_t i = 0; i < paths.size(); ++i) {
        threads.emplace_back([this, &modules, i, &paths]() {
            modules[i] = createShaderModule(paths[i]);
        });
    }
    for (auto& thread : threads) {
        thread.join();
    }
}

void VulkanRTX::buildShaderGroups(std::vector<VkRayTracingShaderGroupCreateInfoKHR>& groups) {
    groups.clear();
    numShaderGroups_ = 0;

    // Raygen group
    VkRayTracingShaderGroupCreateInfoKHR raygenGroup = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .pNext = nullptr,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = 0,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
        .pShaderGroupCaptureReplayHandle = nullptr
    };
    groups.push_back(raygenGroup);
    numShaderGroups_++;

    // Miss groups (primary and shadow)
    VkRayTracingShaderGroupCreateInfoKHR missGroup = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .pNext = nullptr,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = 1,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
        .pShaderGroupCaptureReplayHandle = nullptr
    };
    groups.push_back(missGroup);
    numShaderGroups_++;

    VkRayTracingShaderGroupCreateInfoKHR shadowMissGroup = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .pNext = nullptr,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = 2,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
        .pShaderGroupCaptureReplayHandle = nullptr
    };
    groups.push_back(shadowMissGroup);
    numShaderGroups_++;

    // Triangle hit group
    VkRayTracingShaderGroupCreateInfoKHR hitGroup = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .pNext = nullptr,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = 3,
        .anyHitShader = hasShaderFeature(ShaderFeatures::AnyHit) ? 4 : VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
        .pShaderGroupCaptureReplayHandle = nullptr
    };
    groups.push_back(hitGroup);
    numShaderGroups_++;

    // Procedural hit group
    VkRayTracingShaderGroupCreateInfoKHR proceduralHitGroup = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .pNext = nullptr,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_PROCEDURAL_HIT_GROUP_KHR,
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = 3, // Reuse closest hit shader
        .anyHitShader = hasShaderFeature(ShaderFeatures::AnyHit) ? 4 : VK_SHADER_UNUSED_KHR,
        .intersectionShader = hasShaderFeature(ShaderFeatures::Intersection) ? 5 : VK_SHADER_UNUSED_KHR,
        .pShaderGroupCaptureReplayHandle = nullptr
    };
    if (hasShaderFeature(ShaderFeatures::Intersection)) {
        groups.push_back(proceduralHitGroup);
        numShaderGroups_++;
    }

    // Callable group
    if (hasShaderFeature(ShaderFeatures::Callable)) {
        VkRayTracingShaderGroupCreateInfoKHR callableGroup = {
            .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
            .pNext = nullptr,
            .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
            .generalShader = static_cast<uint32_t>(hasShaderFeature(ShaderFeatures::AnyHit) ? (hasShaderFeature(ShaderFeatures::Intersection) ? 6 : 5) : 4),
            .closestHitShader = VK_SHADER_UNUSED_KHR,
            .anyHitShader = VK_SHADER_UNUSED_KHR,
            .intersectionShader = VK_SHADER_UNUSED_KHR,
            .pShaderGroupCaptureReplayHandle = nullptr
        };
        groups.push_back(callableGroup);
        numShaderGroups_++;
    }
}

void VulkanRTX::createDescriptorSetLayout() {
    const VkShaderStageFlags allRTStages = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                                           VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                                           VK_SHADER_STAGE_ANY_HIT_BIT_KHR |
                                           VK_SHADER_STAGE_INTERSECTION_BIT_KHR |
                                           VK_SHADER_STAGE_MISS_BIT_KHR |
                                           VK_SHADER_STAGE_CALLABLE_BIT_KHR;

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {
            .binding = static_cast<uint32_t>(DescriptorBindings::TLAS),
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .descriptorCount = 1,
            .stageFlags = allRTStages,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = static_cast<uint32_t>(DescriptorBindings::StorageImage),
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = allRTStages,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = static_cast<uint32_t>(DescriptorBindings::CameraUBO),
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = allRTStages,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = static_cast<uint32_t>(DescriptorBindings::MaterialSSBO),
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 26,
            .stageFlags = allRTStages,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = static_cast<uint32_t>(DescriptorBindings::DimensionDataSSBO),
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = allRTStages,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = static_cast<uint32_t>(DescriptorBindings::DenoiseImage),
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = allRTStages,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = static_cast<uint32_t>(DescriptorBindings::EnvMap),
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = allRTStages,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = static_cast<uint32_t>(DescriptorBindings::DensityVolume),
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = allRTStages,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = static_cast<uint32_t>(DescriptorBindings::GDepth),
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = allRTStages,
            .pImmutableSamplers = nullptr
        },
        {
            .binding = static_cast<uint32_t>(DescriptorBindings::GNormal),
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = allRTStages,
            .pImmutableSamplers = nullptr
        }
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(device_, &layoutInfo, nullptr, &layout), "Failed to create descriptor set layout");
    setDescriptorSetLayout(layout);
}

void VulkanRTX::createDescriptorPoolAndSet() {
    std::vector<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 }, // StorageImage, DenoiseImage, GDepth, GNormal
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 26 + 1 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 2 } // EnvMap, DensityVolume
    };

    VkDescriptorPoolCreateInfo poolInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes = poolSizes.data()
    };

    VkDescriptorPool pool;
    VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &pool), "Failed to create descriptor pool");
    setDescriptorPool(pool);

    VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = dsPool_.get(),
        .descriptorSetCount = 1,
        .pSetLayouts = dsLayout_.getPtr()
    };

    VkDescriptorSet descriptorSet;
    VK_CHECK(vkAllocateDescriptorSets(device_, &allocInfo, &descriptorSet), "Failed to allocate descriptor set");
    setDescriptorSet(descriptorSet);
}

void VulkanRTX::createRayTracingPipeline(uint32_t maxRayRecursionDepth) {
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkShaderModule> modules;
    loadShadersAsync(modules, shaderPaths_);

    uint32_t shaderIndex = 0;
    for (const auto& path : shaderPaths_) {
        VkShaderStageFlagBits stage;
        std::string ext = std::filesystem::path(path).extension().string();
        if (ext == ".rgen") stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        else if (ext == ".rmiss") stage = VK_SHADER_STAGE_MISS_BIT_KHR;
        else if (ext == ".rchit") stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        else if (ext == ".rahit") stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        else if (ext == ".rint") stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        else if (ext == ".rcall") stage = VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        else continue;

        stages.push_back({
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = stage,
            .module = modules[shaderIndex++],
            .pName = "main",
            .pSpecializationInfo = nullptr
        });
    }

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;
    buildShaderGroups(shaderGroups);

    VkPushConstantRange pushConstantRange = {
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                      VK_SHADER_STAGE_ANY_HIT_BIT_KHR | VK_SHADER_STAGE_INTERSECTION_BIT_KHR |
                      VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CALLABLE_BIT_KHR,
        .offset = 0,
        .size = sizeof(MaterialData::PushConstants)
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = dsLayout_.getPtr(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstantRange
    };

    VkPipelineLayout pipelineLayout;
    VK_CHECK(vkCreatePipelineLayout(device_, &pipelineLayoutInfo, nullptr, &pipelineLayout), "Failed to create pipeline layout");
    setPipelineLayout(pipelineLayout);

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
        .pNext = nullptr,
        .shaderGroupHandleSize = 0,
        .maxRayRecursionDepth = 0,
        .maxShaderGroupStride = 0,
        .shaderGroupBaseAlignment = 0,
        .shaderGroupHandleCaptureReplaySize = 0,
        .maxRayDispatchInvocationCount = 0,
        .shaderGroupHandleAlignment = 0,
        .maxRayHitAttributeSize = 0
    };
    VkPhysicalDeviceProperties2 properties2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProperties,
        .properties = {}
    };
    vkGetPhysicalDeviceProperties2(physicalDevice_, &properties2);

    VkRayTracingPipelineCreateInfoKHR pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .groupCount = static_cast<uint32_t>(shaderGroups.size()),
        .pGroups = shaderGroups.data(),
        .maxPipelineRayRecursionDepth = std::min(maxRayRecursionDepth, rtProperties.maxRayRecursionDepth),
        .pLibraryInfo = nullptr,
        .pLibraryInterface = nullptr,
        .pDynamicState = nullptr,
        .layout = rtPipelineLayout_.get(),
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1
    };

    VkPipeline pipeline;
    VK_CHECK(vkCreateRayTracingPipelinesKHR(device_, VK_NULL_HANDLE, nullptr, 1, &pipelineInfo, nullptr, &pipeline), "Failed to create ray tracing pipeline");
    setPipeline(pipeline);

    for (auto module : modules) {
        vkDestroyShaderModule(device_, module, nullptr);
    }
}

void VulkanRTX::createShaderBindingTable(VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR,
        .pNext = nullptr,
        .shaderGroupHandleSize = 0,
        .maxRayRecursionDepth = 0,
        .maxShaderGroupStride = 0,
        .shaderGroupBaseAlignment = 0,
        .shaderGroupHandleCaptureReplaySize = 0,
        .maxRayDispatchInvocationCount = 0,
        .shaderGroupHandleAlignment = 0,
        .maxRayHitAttributeSize = 0
    };
    VkPhysicalDeviceProperties2 properties = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProperties,
        .properties = {}
    };
    vkGetPhysicalDeviceProperties2(physicalDevice, &properties);

    const uint32_t handleSize = rtProperties.shaderGroupHandleSize;
    const uint32_t handleAlignment = rtProperties.shaderGroupHandleAlignment;
    const uint32_t handleSizeAligned = (handleSize + handleAlignment - 1) & ~(handleAlignment - 1);
    const uint32_t groupCount = numShaderGroups_;
    const VkDeviceSize sbtSize = handleSizeAligned * groupCount;

    if (groupCount == 0) {
        LOG_ERROR_CAT("VulkanRTX", "No shader groups defined for SBT", std::source_location::current());
        throw VulkanRTXException("No shader groups defined for SBT");
    }

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> sbtBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> sbtMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, sbtSize,
                 VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sbtBuffer, sbtMemory);

    std::vector<uint8_t> shaderGroupHandles(sbtSize);
    VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(device_, rtPipeline_.get(), 0, groupCount, sbtSize, shaderGroupHandles.data()),
             "Failed to get shader group handles");

    void* mappedData;
    VK_CHECK(vkMapMemory(device_, sbtMemory.get(), 0, sbtSize, 0, &mappedData), "Failed to map SBT memory");
    uint64_t currentOffset = 0;
    for (uint32_t i = 0; i < groupCount; ++i) {
        memcpy(static_cast<uint8_t*>(mappedData) + currentOffset, shaderGroupHandles.data() + (i * handleSize), handleSize);
        currentOffset += handleSizeAligned;
    }
    vkUnmapMemory(device_, sbtMemory.get());

    VkBufferDeviceAddressInfo addressInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = sbtBuffer.get()
    };
    VkDeviceAddress sbtAddress = vkGetBufferDeviceAddress(device_, &addressInfo);

    sbt_.buffer = std::move(sbtBuffer);
    sbt_.memory = std::move(sbtMemory);
    sbt_.raygen = { sbtAddress, handleSizeAligned, handleSizeAligned };
    sbt_.miss = { sbtAddress + handleSizeAligned, 2 * handleSizeAligned, handleSizeAligned }; // Two miss shaders
    sbt_.hit = { sbtAddress + 3 * handleSizeAligned, (hasShaderFeature(ShaderFeatures::Intersection) ? 2 : 1) * handleSizeAligned, handleSizeAligned };
    sbt_.callable = { hasShaderFeature(ShaderFeatures::Callable) ? sbtAddress + (hasShaderFeature(ShaderFeatures::Intersection) ? 5 : 4) * handleSizeAligned : 0, handleSizeAligned, handleSizeAligned };
}

void VulkanRTX::createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                                    const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries) {
    struct Vertex {
        glm::vec3 pos;    // VK_FORMAT_R32G32B32_SFLOAT
        glm::vec3 normal; // VK_FORMAT_R32G32B32_SFLOAT
        glm::vec2 uv;     // VK_FORMAT_R32G32_SFLOAT
    };

    std::vector<VkAccelerationStructureGeometryKHR> asGeometries;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRanges;
    for (const auto& [vertexBuffer, indexBuffer, vertexCount, indexCount, instanceCustomIndex] : geometries) {
        VkAccelerationStructureGeometryKHR geometry = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .pNext = nullptr,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry = {
                .triangles = {
                    .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                    .pNext = nullptr,
                    .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT, // Position
                    .vertexData = { .deviceAddress = getBufferDeviceAddress(vertexBuffer) },
                    .vertexStride = sizeof(Vertex),
                    .maxVertex = vertexCount - 1,
                    .indexType = VK_INDEX_TYPE_UINT32,
                    .indexData = { .deviceAddress = getBufferDeviceAddress(indexBuffer) },
                    .transformData = { .deviceAddress = 0 }
                }
            },
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
        };
        asGeometries.push_back(geometry);
        buildRanges.push_back({
            .primitiveCount = indexCount / 3,
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        });
    }
    setPrimitiveCounts(buildRanges);

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .pNext = nullptr,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = VK_NULL_HANDLE,
        .dstAccelerationStructure = VK_NULL_HANDLE,
        .geometryCount = static_cast<uint32_t>(asGeometries.size()),
        .pGeometries = asGeometries.data(),
        .ppGeometries = nullptr,
        .scratchData = { .deviceAddress = 0 }
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructureSize = 0,
        .updateScratchSize = 0,
        .buildScratchSize = 0
    };
    std::vector<uint32_t> primitiveCounts;
    for (const auto& range : buildRanges) {
        primitiveCounts.push_back(range.primitiveCount);
    }
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, primitiveCounts.data(), &sizeInfo);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> blasBufferTemp(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> blasMemoryTemp(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, blasBufferTemp, blasMemoryTemp);

    VkAccelerationStructureCreateInfoKHR asCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .createFlags = 0,
        .buffer = blasBufferTemp.get(),
        .offset = 0,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .deviceAddress = 0
    };

    VkAccelerationStructureKHR tempAS;
    VK_CHECK(vkCreateAccelerationStructureKHR(device_, &asCreateInfo, nullptr, &tempAS), "Failed to create bottom-level AS");
    setBLAS(tempAS);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> scratchBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> scratchMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    VkDeviceSize scratchSize = (sizeInfo.buildScratchSize + scratchAlignment_ - 1) & ~(scratchAlignment_ - 1);
    createBuffer(physicalDevice, scratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);

    buildInfo.dstAccelerationStructure = blas_.get();
    buildInfo.scratchData.deviceAddress = getBufferDeviceAddress(scratchBuffer.get());

    VkCommandBuffer cmdBuffer = allocateTransientCommandBuffer(commandPool);
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Failed to begin command buffer");

    const VkAccelerationStructureBuildRangeInfoKHR* rangePtr = buildRanges.data();
    vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildInfo, &rangePtr);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer");
    submitAndWaitTransient(cmdBuffer, queue, commandPool);

    blasBuffer_ = std::move(blasBufferTemp);
    blasMemory_ = std::move(blasMemoryTemp);
}

void VulkanRTX::createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                                 const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances) {
    std::vector<VkAccelerationStructureInstanceKHR> asInstances;
    for (const auto& [as, transform] : instances) {
        VkTransformMatrixKHR vkTransform;
        glm::mat4 transposed = glm::transpose(transform);
        memcpy(&vkTransform.matrix, glm::value_ptr(transposed), sizeof(VkTransformMatrixKHR));
        asInstances.push_back({
            .transform = vkTransform,
            .instanceCustomIndex = 0,
            .mask = 0xFF,
            .instanceShaderBindingTableRecordOffset = 0,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference = getAccelerationStructureDeviceAddress(as)
        });
    }

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> instanceBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> instanceMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, sizeof(VkAccelerationStructureInstanceKHR) * asInstances.size(),
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, instanceBuffer, instanceMemory);

    void* mappedData;
    VK_CHECK(vkMapMemory(device_, instanceMemory.get(), 0, sizeof(VkAccelerationStructureInstanceKHR) * asInstances.size(), 0, &mappedData),
             "Failed to map instance buffer memory");
    memcpy(mappedData, asInstances.data(), sizeof(VkAccelerationStructureInstanceKHR) * asInstances.size());
    vkUnmapMemory(device_, instanceMemory.get());

    VkAccelerationStructureGeometryKHR geometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .pNext = nullptr,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {
            .instances = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                .pNext = nullptr,
                .arrayOfPointers = VK_FALSE,
                .data = { .deviceAddress = getBufferDeviceAddress(instanceBuffer.get()) }
            }
        },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildRangeInfoKHR buildRange = {
        .primitiveCount = static_cast<uint32_t>(asInstances.size()),
        .primitiveOffset = 0,
        .firstVertex = 0,
        .transformOffset = 0
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .pNext = nullptr,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = VK_NULL_HANDLE,
        .dstAccelerationStructure = VK_NULL_HANDLE,
        .geometryCount = 1,
        .pGeometries = &geometry,
        .ppGeometries = nullptr,
        .scratchData = { .deviceAddress = 0 }
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructureSize = 0,
        .updateScratchSize = 0,
        .buildScratchSize = 0
    };
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &buildRange.primitiveCount, &sizeInfo);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> tlasBufferTemp(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> tlasMemoryTemp(device_, VK_NULL_HANDLE, vkFreeMemory);
    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlasBufferTemp, tlasMemoryTemp);

    VkAccelerationStructureCreateInfoKHR asCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .pNext = nullptr,
        .createFlags = 0,
        .buffer = tlasBufferTemp.get(),
        .offset = 0,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .deviceAddress = 0
    };

    VkAccelerationStructureKHR tempAS;
    VK_CHECK(vkCreateAccelerationStructureKHR(device_, &asCreateInfo, nullptr, &tempAS), "Failed to create top-level AS");
    setTLAS(tempAS);

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> scratchBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> scratchMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
    VkDeviceSize scratchSize = (sizeInfo.buildScratchSize + scratchAlignment_ - 1) & ~(scratchAlignment_ - 1);
    createBuffer(physicalDevice, scratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer, scratchMemory);

    buildInfo.dstAccelerationStructure = tlas_.get();
    buildInfo.scratchData.deviceAddress = getBufferDeviceAddress(scratchBuffer.get());

    VkCommandBuffer cmdBuffer = allocateTransientCommandBuffer(commandPool);
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Failed to begin command buffer");

    const VkAccelerationStructureBuildRangeInfoKHR* rangePtr = &buildRange;
    vkCmdBuildAccelerationStructuresKHR(cmdBuffer, 1, &buildInfo, &rangePtr);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer");
    submitAndWaitTransient(cmdBuffer, queue, commandPool);

    tlasBuffer_ = std::move(tlasBufferTemp);
    tlasMemory_ = std::move(tlasMemoryTemp);
}

void VulkanRTX::createStorageImage(VkPhysicalDevice physicalDevice, VkExtent2D extent, VkFormat format,
                                   VulkanResource<VkImage, PFN_vkDestroyImage>& image,
                                   VulkanResource<VkImageView, PFN_vkDestroyImageView>& imageView,
                                   VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory) {
    VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = { .width = extent.width, .height = extent.height, .depth = 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage tempImage;
    VK_CHECK(vkCreateImage(device_, &imageInfo, nullptr, &tempImage), "Failed to create storage image");
    image = VulkanResource<VkImage, PFN_vkDestroyImage>(device_, tempImage, vkDestroyImage);

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device_, image.get(), &memRequirements);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VkDeviceMemory tempMemory;
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &tempMemory), "Failed to allocate image memory");
    memory = VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>(device_, tempMemory, vkFreeMemory);

    VK_CHECK(vkBindImageMemory(device_, image.get(), memory.get(), 0), "Failed to bind image memory");

    VkImageViewCreateInfo viewInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .image = image.get(),
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .components = {
            .r = VK_COMPONENT_SWIZZLE_IDENTITY,
            .g = VK_COMPONENT_SWIZZLE_IDENTITY,
            .b = VK_COMPONENT_SWIZZLE_IDENTITY,
            .a = VK_COMPONENT_SWIZZLE_IDENTITY
        },
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };

    VkImageView tempImageView;
    VK_CHECK(vkCreateImageView(device_, &viewInfo, nullptr, &tempImageView), "Failed to create image view");
    imageView = VulkanResource<VkImageView, PFN_vkDestroyImageView>(device_, tempImageView, vkDestroyImageView);
}

void VulkanRTX::updateDescriptors(VkBuffer cameraBuffer, VkBuffer materialBuffer, VkBuffer dimensionBuffer,
                                 VkImageView storageImageView, VkImageView denoiseImageView, VkImageView envMapView, VkSampler envMapSampler,
                                 VkImageView densityVolumeView, VkImageView gDepthView, VkImageView gNormalView) {

    constexpr uint32_t EXPECTED_MATERIAL_COUNT = 26;
    VkDescriptorBufferInfo cameraBufferInfo = {
        .buffer = cameraBuffer,
        .offset = 0,
        .range = VK_WHOLE_SIZE
    };
    std::vector<VkDescriptorBufferInfo> materialBufferInfos(EXPECTED_MATERIAL_COUNT);
    VkDeviceSize materialSize = sizeof(MaterialData);
    for (uint32_t i = 0; i < EXPECTED_MATERIAL_COUNT; ++i) {
        materialBufferInfos[i] = {
            .buffer = materialBuffer,
            .offset = i * materialSize,
            .range = materialSize
        };
    }

    VkDescriptorBufferInfo dimensionBufferInfo = {
        .buffer = dimensionBuffer,
        .offset = 0,
        .range = sizeof(UE::DimensionData)
    };

    VkDescriptorImageInfo storageImageInfo = {
        .sampler = VK_NULL_HANDLE,
        .imageView = storageImageView,
        .imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    };

    VkDescriptorImageInfo denoiseImageInfo = {
        .sampler = VK_NULL_HANDLE,
        .imageView = denoiseImageView,
        .imageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    };

    VkDescriptorImageInfo envMapImageInfo = {
        .sampler = envMapSampler,
        .imageView = envMapView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo densityVolumeInfo = {
        .sampler = envMapSampler, // Reuse sampler
        .imageView = densityVolumeView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo gDepthImageInfo = {
        .sampler = VK_NULL_HANDLE,
        .imageView = gDepthView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo gNormalImageInfo = {
        .sampler = VK_NULL_HANDLE,
        .imageView = gNormalView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkWriteDescriptorSetAccelerationStructureKHR asInfo = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .pNext = nullptr,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = tlas_.getPtr()
    };

    std::vector<VkWriteDescriptorSet> descriptorWrites = {
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &asInfo,
            .dstSet = ds_.get(),
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
            .pImageInfo = nullptr,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = ds_.get(),
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::StorageImage),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &storageImageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = ds_.get(),
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::CameraUBO),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &cameraBufferInfo,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = ds_.get(),
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::MaterialSSBO),
            .dstArrayElement = 0,
            .descriptorCount = EXPECTED_MATERIAL_COUNT,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = materialBufferInfos.data(),
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = ds_.get(),
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::DimensionDataSSBO),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .pImageInfo = nullptr,
            .pBufferInfo = &dimensionBufferInfo,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = ds_.get(),
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::DenoiseImage),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &denoiseImageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = ds_.get(),
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::EnvMap),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &envMapImageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = ds_.get(),
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::DensityVolume),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &densityVolumeInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = ds_.get(),
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::GDepth),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &gDepthImageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = nullptr,
            .dstSet = ds_.get(),
            .dstBinding = static_cast<uint32_t>(DescriptorBindings::GNormal),
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &gNormalImageInfo,
            .pBufferInfo = nullptr,
            .pTexelBufferView = nullptr
        }
    };

    vkUpdateDescriptorSets(device_, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
}

void VulkanRTX::initializeRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                              const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                              uint32_t maxRayRecursionDepth, const std::vector<UE::DimensionData>& dimensionCache) {

    createDescriptorSetLayout();
    createDescriptorPoolAndSet();
    createRayTracingPipeline(maxRayRecursionDepth);
    createShaderBindingTable(physicalDevice);
    createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, geometries);
    createTopLevelAS(physicalDevice, commandPool, graphicsQueue, {{blas_.get(), glm::mat4(1.0f)}});
    setPreviousDimensionCache(dimensionCache);
}

void VulkanRTX::updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                          const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                          const std::vector<UE::DimensionData>& dimensionCache) {

    if (dimensionCache != previousDimensionCache_) {
        createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, geometries);
        createTopLevelAS(physicalDevice, commandPool, graphicsQueue, {{blas_.get(), glm::mat4(1.0f)}});
        setPreviousDimensionCache(dimensionCache);
    }
}

void VulkanRTX::recordRayTracingCommands(VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage,
                                        VkImageView /*outputImageView*/, const MaterialData::PushConstants& pc) {

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Failed to begin command buffer");

    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = outputImage,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_.get());
    VkDescriptorSet descriptorSet = ds_.get();
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineLayout_.get(),
                            0, 1, &descriptorSet, 0, nullptr);
    vkCmdPushConstants(cmdBuffer, rtPipelineLayout_.get(),
                       VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
                       0, sizeof(MaterialData::PushConstants), &pc);
    vkCmdTraceRaysKHR(cmdBuffer, &sbt_.raygen, &sbt_.miss, &sbt_.hit, &sbt_.callable, extent.width, extent.height, 1);

    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer");
}

void VulkanRTX::updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas) {

    VkWriteDescriptorSetAccelerationStructureKHR asInfo = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .pNext = nullptr,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlas
    };

    VkWriteDescriptorSet descriptorWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &asInfo,
        .dstSet = ds_.get(),
        .dstBinding = static_cast<uint32_t>(DescriptorBindings::TLAS),
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .pImageInfo = nullptr,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr
    };

    vkUpdateDescriptorSets(device_, 1, &descriptorWrite, 0, nullptr);
}

void VulkanRTX::compactAccelerationStructures(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue) {
    if (!supportsCompaction_) {
        return;
    }

    VkQueryPoolCreateInfo queryPoolInfo = {
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queryType = VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR,
        .queryCount = 2,
        .pipelineStatistics = 0
    };

    VkQueryPool queryPool;
    VK_CHECK(vkCreateQueryPool(device_, &queryPoolInfo, nullptr, &queryPool), "Failed to create query pool");
    VulkanResource<VkQueryPool, PFN_vkDestroyQueryPool> queryPoolResource(device_, queryPool, vkDestroyQueryPool);

    VkCommandBuffer cmdBuffer = allocateTransientCommandBuffer(commandPool);
    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
        .pInheritanceInfo = nullptr
    };
    VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Failed to begin command buffer");

    std::vector<VkAccelerationStructureKHR> structures = { blas_.get(), tlas_.get() };
    vkCmdResetQueryPool(cmdBuffer, queryPool, 0, 2);
    vkCmdWriteAccelerationStructuresPropertiesKHR(cmdBuffer, 2, structures.data(),
                                                 VK_QUERY_TYPE_ACCELERATION_STRUCTURE_COMPACTED_SIZE_KHR, queryPool, 0);

    VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer");
    submitAndWaitTransient(cmdBuffer, queue, commandPool);

    std::vector<VkDeviceSize> compactedSizes(2);
    VK_CHECK(vkGetQueryPoolResults(device_, queryPool, 0, 2, sizeof(VkDeviceSize) * 2, compactedSizes.data(),
                                   sizeof(VkDeviceSize), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT),
             "Failed to get query pool results");

    for (uint32_t i = 0; i < 2; ++i) {
        if (compactedSizes[i] == 0) continue;

        VulkanResource<VkBuffer, PFN_vkDestroyBuffer> newBuffer(device_, VK_NULL_HANDLE, vkDestroyBuffer);
        VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> newMemory(device_, VK_NULL_HANDLE, vkFreeMemory);
        createBuffer(physicalDevice, compactedSizes[i],
                     VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, newBuffer, newMemory);

        VkAccelerationStructureCreateInfoKHR asCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .createFlags = 0,
            .buffer = newBuffer.get(),
            .offset = 0,
            .size = compactedSizes[i],
            .type = (i == 0) ? VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR : VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            .deviceAddress = 0
        };

        VkAccelerationStructureKHR newAS;
        VK_CHECK(vkCreateAccelerationStructureKHR(device_, &asCreateInfo, nullptr, &newAS), "Failed to create compacted AS");
        VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR> newASResource(device_, newAS, vkDestroyAccelerationStructureKHR);

        cmdBuffer = allocateTransientCommandBuffer(commandPool);
        VK_CHECK(vkBeginCommandBuffer(cmdBuffer, &beginInfo), "Failed to begin command buffer");

        VkCopyAccelerationStructureInfoKHR copyInfo = {
            .sType = VK_STRUCTURE_TYPE_COPY_ACCELERATION_STRUCTURE_INFO_KHR,
            .pNext = nullptr,
            .src = structures[i],
            .dst = newAS,
            .mode = VK_COPY_ACCELERATION_STRUCTURE_MODE_COMPACT_KHR
        };
        vkCmdCopyAccelerationStructureKHR(cmdBuffer, &copyInfo);

        VK_CHECK(vkEndCommandBuffer(cmdBuffer), "Failed to end command buffer");
        submitAndWaitTransient(cmdBuffer, queue, commandPool);

        if (i == 0) {
            blas_ = std::move(newASResource);
            blasBuffer_ = std::move(newBuffer);
            blasMemory_ = std::move(newMemory);
        } else {
            tlas_ = std::move(newASResource);
            tlasBuffer_ = std::move(newBuffer);
            tlasMemory_ = std::move(newMemory);
            updateDescriptorSetForTLAS(tlas_.get());
        }
    }
}

VkCommandBuffer VulkanRTX::allocateTransientCommandBuffer(VkCommandPool commandPool) {
    VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .pNext = nullptr,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer cmdBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device_, &allocInfo, &cmdBuffer), "Failed to allocate command buffer");
    return cmdBuffer;
}

void VulkanRTX::submitAndWaitTransient(VkCommandBuffer cmdBuffer, VkQueue queue, VkCommandPool commandPool) {
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmdBuffer,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr
    };

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE), "Failed to submit command buffer");
    VK_CHECK(vkQueueWaitIdle(queue), "Failed to wait for queue idle");
    vkFreeCommandBuffers(device_, commandPool, 1, &cmdBuffer);
}

void VulkanRTX::createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags properties, VulkanResource<VkBuffer, PFN_vkDestroyBuffer>& buffer,
                             VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory) {
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr
    };

    VkBuffer tempBuffer;
    VK_CHECK(vkCreateBuffer(device_, &bufferInfo, nullptr, &tempBuffer), "Failed to create buffer");

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, tempBuffer, &memRequirements);

    VkMemoryAllocateFlagsInfo allocFlagsInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO,
        .pNext = nullptr,
        .flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT,
        .deviceMask = 0
    };
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = &allocFlagsInfo,
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memRequirements.memoryTypeBits, properties)
    };

    VkDeviceMemory tempMemory;
    VK_CHECK(vkAllocateMemory(device_, &allocInfo, nullptr, &tempMemory), "Failed to allocate buffer memory");
    VK_CHECK(vkBindBufferMemory(device_, tempBuffer, tempMemory, 0), "Failed to bind buffer memory");

    buffer = VulkanResource<VkBuffer, PFN_vkDestroyBuffer>(device_, tempBuffer, vkDestroyBuffer);
    memory = VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>(device_, tempMemory, vkFreeMemory);
}

uint32_t VulkanRTX::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    LOG_ERROR_CAT("VulkanRTX", "Failed to find suitable memory type", std::source_location::current());
    throw VulkanRTXException("Failed to find suitable memory type");
}

VkDeviceAddress VulkanRTX::getBufferDeviceAddress(VkBuffer buffer) {
    VkBufferDeviceAddressInfo addressInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .pNext = nullptr,
        .buffer = buffer
    };
    VkDeviceAddress address = vkGetBufferDeviceAddress(device_, &addressInfo);
    if (address == 0) {
        LOG_ERROR_CAT("VulkanRTX", "Failed to get valid buffer device address", std::source_location::current());
        throw VulkanRTXException("Failed to get valid buffer device address");
    }
    return address;
}

VkDeviceAddress VulkanRTX::getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR as) {
    VkAccelerationStructureDeviceAddressInfoKHR addressInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .pNext = nullptr,
        .accelerationStructure = as
    };
    VkDeviceAddress address = vkGetAccelerationStructureDeviceAddressKHR(device_, &addressInfo);
    if (address == 0) {
        LOG_ERROR_CAT("VulkanRTX", "Failed to get valid acceleration structure device address", std::source_location::current());
        throw VulkanRTXException("Failed to get valid acceleration structure device address");
    }
    return address;
}

} // namespace VulkanRTX