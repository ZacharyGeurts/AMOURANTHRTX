// src/engine/GLOBAL/PipelineManager.cpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// PipelineManager v18.3 — Clean, robust, and production-ready
// =============================================================================

#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <vector>
#include <atomic>
#include <fstream>

using namespace Logging::Color;
using StoneKey::stone_device;

namespace RTX {

std::atomic<bool>     PipelineManager::g_pipelineNeedsRebuild{false};
std::atomic<uint32_t> PipelineManager::g_rebuildRequestedFrame{UINT32_MAX};

// =============================================================================
// Descriptor set layout bindings for set 0 (main ray tracing resources)
// =============================================================================
constexpr std::array<VkDescriptorSetLayoutBinding, 11> kMainBindings{{
    {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
     VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
     VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
     VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
     VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
     VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR},
    {6, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    {7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
     VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
    {8, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    {9, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
    {10, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
    {31, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
     VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR |
     VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR}
}};

// =============================================================================
// Descriptor Pool Creation
// =============================================================================
void PipelineManager::createDescriptorPool() noexcept
{
    if (rtDescriptorPool_.valid()) {
        return;
    }

    LOG_INFO_CAT("PIPELINE", "Creating descriptor pool");

    std::array<VkDescriptorPoolSize, 7> poolSizes{{
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 8},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              64},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             32},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             32},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,              16},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     8192},
        {VK_DESCRIPTOR_TYPE_SAMPLER,                    16}
    }};

    VkDescriptorPoolCreateInfo poolInfo{
        .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets       = 128,
        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
        .pPoolSizes    = poolSizes.data()
    };

    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkResult result = vkCreateDescriptorPool(stone_device(), &poolInfo, nullptr, &pool);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create descriptor pool: {}", string_VkResult(result));
        return;
    }

    rtDescriptorPool_ = Handle<VkDescriptorPool>(pool, stone_device(), vkDestroyDescriptorPool, 0, "RT_DescriptorPool");
    LOG_SUCCESS_CAT("PIPELINE", "Descriptor pool created");
}

// =============================================================================
// Constructor
// =============================================================================
PipelineManager::PipelineManager(VkDevice device, VkPhysicalDevice phys)
{
    LOG_INFO_CAT("PIPELINE", "Constructing PipelineManager");

    RTX::loadRTExtensions(StoneKey::stone_instance(), device);

    if (!g_ext.vkCmdTraceRaysKHR ||
        !g_ext.vkCreateRayTracingPipelinesKHR ||
        !g_ext.vkGetRayTracingShaderGroupHandlesKHR ||
        !g_ext.vkCreateAccelerationStructureKHR ||
        !g_ext.vkDestroyAccelerationStructureKHR ||
        !g_ext.vkGetAccelerationStructureBuildSizesKHR ||
        !g_ext.vkGetBufferDeviceAddress) {
        LOG_FATAL_CAT("PIPELINE", "Required ray tracing extensions are missing");
        throw std::runtime_error("Ray tracing extensions missing");
    }

    dummyTLAS_ = Handle<VkAccelerationStructureKHR>(
        createDummyTLAS(), stone_device(), g_ext.vkDestroyAccelerationStructureKHR);

    LOG_SUCCESS_CAT("PIPELINE", "PipelineManager constructed – dummy TLAS ready");
}

// =============================================================================
// Descriptor Set Allocation
// =============================================================================
void PipelineManager::allocateDescriptorSets()
{
    LOG_TRACE_CAT("PIPELINE", "Allocating descriptor sets");

    const uint32_t frames = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    if (!rtDescriptorPool_.valid() ||
        !rtDescriptorSetLayout_.valid() ||
        !texDescriptorSetLayout_.valid() ||
        !emptyDescriptorSetLayout_.valid()) {
        LOG_FATAL_CAT("PIPELINE", "Missing required objects for descriptor set allocation");
        throw std::runtime_error("Descriptor allocation prerequisites missing");
    }

    auto allocateSets = [this, frames](std::vector<VkDescriptorSet>& sets,
                                       VkDescriptorSetLayout layout,
                                       const char* name) {
        sets.resize(frames);
        std::vector<VkDescriptorSetLayout> layouts(frames, layout);

        VkDescriptorSetAllocateInfo allocInfo{
            .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
            .descriptorPool     = rtDescriptorPool_.get(),
            .descriptorSetCount = frames,
            .pSetLayouts        = layouts.data()
        };

        VkResult result = vkAllocateDescriptorSets(stone_device(), &allocInfo, sets.data());
        if (result != VK_SUCCESS) {
            LOG_FATAL_CAT("PIPELINE", "Failed to allocate {} descriptor sets: {}", name, string_VkResult(result));
            throw std::runtime_error(std::string("Failed to allocate ") + name + " descriptor sets");
        }
        LOG_SUCCESS_CAT("PIPELINE", "Allocated {} {} descriptor sets", frames, name);
    };

    allocateSets(rtDescriptorSets_,   rtDescriptorSetLayout_.get(),   "main (set 0)");
    allocateSets(texDescriptorSets_,  texDescriptorSetLayout_.get(),  "texture array (set 2)");
    allocateSets(emptyDescriptorSets_, emptyDescriptorSetLayout_.get(), "empty (set 1 & 3)");
}

// =============================================================================
// Descriptor Set Updates
// =============================================================================
void PipelineManager::updateRTDescriptorSet(uint32_t frameIndex, const RTDescriptorUpdate& updateInfo) noexcept
{
    if (frameIndex >= rtDescriptorSets_.size() || rtDescriptorSets_[frameIndex] == VK_NULL_HANDLE) {
        return;
    }

    VkDescriptorSet set = rtDescriptorSets_[frameIndex];
    std::array<VkWriteDescriptorSet, 17> writes{};
    uint32_t writeCount = 0;

    auto addAccelStructure = [&](uint32_t binding, VkAccelerationStructureKHR as) {
        as = as ? as : dummyTLAS_.get();
        VkWriteDescriptorSetAccelerationStructureKHR accelInfo{
            .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .accelerationStructureCount = 1,
            .pAccelerationStructures    = &as
        };
        writes[writeCount++] = VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext           = &accelInfo,
            .dstSet          = set,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
        };
    };

    auto addStorageImage = [&](uint32_t binding, VkImageView view, VkImageLayout layout = VK_IMAGE_LAYOUT_GENERAL) {
        if (view == VK_NULL_HANDLE) return;
        VkDescriptorImageInfo imageInfo{.imageView = view, .imageLayout = layout};
        writes[writeCount++] = VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = set,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo      = &imageInfo
        };
    };

    auto addBuffer = [&](uint32_t binding, VkBuffer buffer, VkDeviceSize size, VkDescriptorType type) {
        if (buffer == VK_NULL_HANDLE) return;
        VkDescriptorBufferInfo bufferInfo{.buffer = buffer, .offset = 0, .range = size};
        writes[writeCount++] = VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = set,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = type,
            .pBufferInfo     = &bufferInfo
        };
    };

    auto addCombinedSampler = [&](uint32_t binding, VkSampler sampler, VkImageView view) {
        if (sampler == VK_NULL_HANDLE || view == VK_NULL_HANDLE) return;
        VkDescriptorImageInfo imageInfo{
            .sampler     = sampler,
            .imageView   = view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };
        writes[writeCount++] = VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = set,
            .dstBinding      = binding,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo      = &imageInfo
        };
    };

    addAccelStructure(0, updateInfo.tlas);
    addStorageImage(1, updateInfo.rtOutputView);

    if (Options::OptionsRTX::ENABLE_ACCUMULATION &&
        !updateInfo.accumulationViews.empty() &&
        frameIndex < updateInfo.accumulationViews.size()) {
        addStorageImage(3, updateInfo.accumulationViews[frameIndex]);
    }

    addBuffer(2, updateInfo.ubo, updateInfo.uboSize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

    if (updateInfo.materialsBuffer) {
        addBuffer(4, updateInfo.materialsBuffer, updateInfo.materialsSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }

    if (updateInfo.envSampler && updateInfo.envImageView) {
        addCombinedSampler(7, updateInfo.envSampler, updateInfo.envImageView);
    }

    if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING &&
        !updateInfo.nexusScoreViews.empty() &&
        frameIndex < updateInfo.nexusScoreViews.size()) {
        addStorageImage(6, updateInfo.nexusScoreViews[frameIndex]);
    }

    if (updateInfo.blueNoiseSampler && updateInfo.blueNoiseView) {
        addCombinedSampler(8, updateInfo.blueNoiseSampler, updateInfo.blueNoiseView);
    }

    if (updateInfo.densitySampler && updateInfo.densityView) {
        addCombinedSampler(9, updateInfo.densitySampler, updateInfo.densityView);
    }

    if (updateInfo.additionalStorageBuffer) {
        addBuffer(10, updateInfo.additionalStorageBuffer, updateInfo.additionalStorageSize, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
    }

    if (updateInfo.stoneKeyBuffer) {
        addBuffer(31, updateInfo.stoneKeyBuffer, updateInfo.stoneKeySize, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    }

    if (writeCount > 0) {
        vkUpdateDescriptorSets(stone_device(), writeCount, writes.data(), 0, nullptr);
    }
}

// =============================================================================
// Shader Loading
// =============================================================================
VkShaderModule PipelineManager::loadShader(const std::string& relativePath) const
{
    LOG_TRACE_CAT("PIPELINE", "Loading shader: {}", relativePath);

    const std::string fullPath = "build/bin/Linux/" + relativePath;

    std::ifstream file(fullPath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_CAT("PIPELINE", "Shader file not found: {}", fullPath);
        return VK_NULL_HANDLE;
    }

    const size_t fileSize = static_cast<size_t>(file.tellg());
    if (fileSize == 0 || fileSize % 4 != 0) {
        LOG_ERROR_CAT("PIPELINE", "Invalid SPIR-V size ({} bytes): {}", fileSize, fullPath);
        return VK_NULL_HANDLE;
    }

    std::vector<char> code(fileSize);
    file.seekg(0);
    file.read(code.data(), fileSize);

    VkShaderModuleCreateInfo createInfo{
        .sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fileSize,
        .pCode    = reinterpret_cast<const uint32_t*>(code.data())
    };

    VkShaderModule module = VK_NULL_HANDLE;
    VkResult result = vkCreateShaderModule(stone_device(), &createInfo, nullptr, &module);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create shader module: {}", string_VkResult(result));
        return VK_NULL_HANDLE;
    }

    LOG_SUCCESS_CAT("PIPELINE", "Shader loaded: {} ({} bytes)", relativePath, fileSize);
    return module;
}

// =============================================================================
// Dummy TLAS Creation
// =============================================================================
VkAccelerationStructureKHR PipelineManager::createDummyTLAS()
{
    VkAccelerationStructureGeometryKHR geometry{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry     = {.instances = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                                      .data = {.deviceAddress = 0}}}
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries   = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

    const uint32_t primitiveCount = 1;
    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        stone_device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primitiveCount,
        &sizeInfo);

    uint64_t bufferHandle = BufferManager::create(
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "DummyTLAS_Buffer");

    if (bufferHandle == 0) {
        return VK_NULL_HANDLE;
    }

    const auto* bufferInfo = BufferManager::get(bufferHandle);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = bufferInfo->buffer,
        .size   = sizeInfo.accelerationStructureSize,
        .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };

    VkAccelerationStructureKHR as = VK_NULL_HANDLE;
    VkResult result = g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createInfo, nullptr, &as);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create dummy TLAS: {}", string_VkResult(result));
        BufferManager::destroy(bufferHandle);
        return VK_NULL_HANDLE;
    }

    dummyAccelBuffer_ = Handle<VkBuffer>(bufferInfo->buffer, stone_device(), vkDestroyBuffer);
    dummyAccelMemory_ = Handle<VkDeviceMemory>(bufferInfo->memory, stone_device(), vkFreeMemory);

    return as;
}

// =============================================================================
// Ray Tracing Pipeline Creation
// =============================================================================
void PipelineManager::createRayTracingPipeline()
{
    auto loadShaderChecked = [this](const char* path) -> VkShaderModule {
        VkShaderModule module = loadShader(path);
        if (module == VK_NULL_HANDLE) {
            throw std::runtime_error(std::string("Failed to load shader: ") + path);
        }
        return module;
    };

    VkShaderModule raygen = loadShaderChecked("assets/shaders/raytracing/raygen.spv");
    VkShaderModule miss   = loadShaderChecked("assets/shaders/raytracing/miss.spv");
    VkShaderModule chit   = loadShaderChecked("assets/shaders/raytracing/closest_hit.spv");
    VkShaderModule ahit   = loadShaderChecked("assets/shaders/raytracing/anyhit.spv");

    shaderModules_.clear();
    shaderModules_.emplace_back(raygen, stone_device(), vkDestroyShaderModule);
    shaderModules_.emplace_back(miss,   stone_device(), vkDestroyShaderModule);
    shaderModules_.emplace_back(chit,   stone_device(), vkDestroyShaderModule);
    shaderModules_.emplace_back(ahit,   stone_device(), vkDestroyShaderModule);

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

    uint32_t shaderIndex = 0;

    // Ray generation shader
    stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                      .stage  = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
                      .module = raygen,
                      .pName  = "main"});
    groups.push_back({.sType           = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                      .type            = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                      .generalShader   = shaderIndex++});

    // Miss shader
    stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                      .stage  = VK_SHADER_STAGE_MISS_BIT_KHR,
                      .module = miss,
                      .pName  = "main"});
    groups.push_back({.sType           = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                      .type            = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
                      .generalShader   = shaderIndex++});

    // Closest hit shader
    stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                      .stage  = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
                      .module = chit,
                      .pName  = "main"});
    uint32_t chitIndex = shaderIndex++;

    // Any hit shader
    stages.push_back({.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                      .stage  = VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
                      .module = ahit,
                      .pName  = "main"});
    uint32_t ahitIndex = shaderIndex++;

    // Hit group (triangles)
    groups.push_back({.sType           = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
                      .type            = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
                      .closestHitShader = chitIndex,
                      .anyHitShader     = ahitIndex});

    raygenGroupCount_ = 1;
    missGroupCount_   = 1;
    hitGroupCount_    = 1;

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{
        .sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount                   = static_cast<uint32_t>(stages.size()),
        .pStages                      = stages.data(),
        .groupCount                   = static_cast<uint32_t>(groups.size()),
        .pGroups                      = groups.data(),
        .maxPipelineRayRecursionDepth = Options::OptionsRTX::MAX_PIPELINE_RAY_RECURSION_DEPTH,
        .layout                       = rtPipelineLayout_.get()
    };

    VkPipeline pipeline = VK_NULL_HANDLE;
    VkResult result = g_ext.vkCreateRayTracingPipelinesKHR(
        stone_device(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);

    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create ray tracing pipeline: {}", string_VkResult(result));
        return;
    }

    rtPipeline_ = Handle<VkPipeline>(pipeline, stone_device(), vkDestroyPipeline);
    LOG_SUCCESS_CAT("PIPELINE", "Ray tracing pipeline created successfully");
}

// =============================================================================
// Cache Ray Tracing Device Properties
// =============================================================================
void PipelineManager::cacheDeviceProperties()
{
    VkPhysicalDevice physicalDevice = StoneKey::stone_physical();
    if (physicalDevice == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "No physical device available");
        throw std::runtime_error("No physical device");
    }

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };

    VkPhysicalDeviceProperties2 props2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
        .pNext = &rtProps
    };

    vkGetPhysicalDeviceProperties2(physicalDevice, &props2);

    if (rtProps.shaderGroupHandleSize == 0) {
        LOG_FATAL_CAT("PIPELINE", "Ray tracing not supported on this device");
        throw std::runtime_error("Ray tracing unsupported");
    }

    StoneKey::stone_seal_rtprops(rtProps);

    auto& ctx = RTX::g_ctx();
    ctx.physicalDeviceProperties_ = props2.properties;
    ctx.rayTracingProps_ = rtProps;
}

// =============================================================================
// Ray Tracing Execution
// =============================================================================
void PipelineManager::traceRays(VkCommandBuffer cmd, uint32_t frameIndex, uint32_t width, uint32_t height, uint32_t depth)
{
    if (g_pipelineNeedsRebuild.load(std::memory_order_acquire)) {
        createRayTracingPipeline();
        g_pipelineNeedsRebuild.store(false, std::memory_order_release);
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_.get());

    if (frameIndex >= rtDescriptorSets_.size() ||
        frameIndex >= texDescriptorSets_.size() ||
        frameIndex >= emptyDescriptorSets_.size()) {
        LOG_ERROR_CAT("PIPELINE", "Invalid frame index {} for descriptor binding", frameIndex);
        return;
    }

    const VkDescriptorSet descriptorSets[4] = {
        rtDescriptorSets_[frameIndex],
        emptyDescriptorSets_[frameIndex],
        texDescriptorSets_[frameIndex],
        emptyDescriptorSets_[frameIndex]
    };

    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            rtPipelineLayout_.get(),
                            0, 4, descriptorSets,
                            0, nullptr);

    g_ext.vkCmdTraceRaysKHR(cmd,
                            &raygenSbtRegion_,
                            &missSbtRegion_,
                            &hitSbtRegion_,
                            &callableSbtRegion_,
                            width, height, depth);
}

// =============================================================================
// Pipeline Layout Creation
// =============================================================================
void PipelineManager::createPipelineLayout()
{
    if (rtPipelineLayout_.valid()) {
        LOG_TRACE_CAT("PIPELINE", "Pipeline layout already exists");
        return;
    }

    LOG_INFO_CAT("PIPELINE", "Creating ray tracing pipeline layout (4 descriptor sets)");

    createDescriptorPool();

    // Set 0 – main ray tracing bindings
    VkDescriptorSetLayoutCreateInfo mainLayoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(kMainBindings.size()),
        .pBindings    = kMainBindings.data()
    };

    VkDescriptorSetLayout mainLayout = VK_NULL_HANDLE;
    VkResult result = vkCreateDescriptorSetLayout(stone_device(), &mainLayoutInfo, nullptr, &mainLayout);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create main descriptor set layout: {}", string_VkResult(result));
        return;
    }
    rtDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(mainLayout, stone_device(), vkDestroyDescriptorSetLayout);

    // Set 2 – texture array
    VkDescriptorSetLayoutBinding texBinding{
        .binding            = 0,
        .descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount    = 1024,
        .stageFlags         = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_ANY_HIT_BIT_KHR
    };

    VkDescriptorSetLayoutCreateInfo texLayoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings    = &texBinding
    };

    VkDescriptorSetLayout texLayout = VK_NULL_HANDLE;
    result = vkCreateDescriptorSetLayout(stone_device(), &texLayoutInfo, nullptr, &texLayout);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create texture array descriptor set layout: {}", string_VkResult(result));
        return;
    }
    texDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(texLayout, stone_device(), vkDestroyDescriptorSetLayout);

    // Empty layouts for sets 1 and 3
    VkDescriptorSetLayoutCreateInfo emptyLayoutInfo{
        .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 0,
        .pBindings    = nullptr
    };

    VkDescriptorSetLayout emptyLayout = VK_NULL_HANDLE;
    result = vkCreateDescriptorSetLayout(stone_device(), &emptyLayoutInfo, nullptr, &emptyLayout);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create empty descriptor set layout: {}", string_VkResult(result));
        return;
    }
    emptyDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(emptyLayout, stone_device(), vkDestroyDescriptorSetLayout);

    // Final pipeline layout with 4 sets
    const VkDescriptorSetLayout layouts[4] = {
        rtDescriptorSetLayout_.get(),
        emptyDescriptorSetLayout_.get(),
        texDescriptorSetLayout_.get(),
        emptyDescriptorSetLayout_.get()
    };

    VkPushConstantRange pushConstant{
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR |
                      VK_SHADER_STAGE_MISS_BIT_KHR |
                      VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR |
                      VK_SHADER_STAGE_ANY_HIT_BIT_KHR,
        .offset     = 0,
        .size       = 32
    };

    VkPipelineLayoutCreateInfo layoutInfo{
        .sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount         = 4,
        .pSetLayouts            = layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges    = &pushConstant
    };

    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    result = vkCreatePipelineLayout(stone_device(), &layoutInfo, nullptr, &pipelineLayout);
    if (result != VK_SUCCESS) {
        LOG_FATAL_CAT("PIPELINE", "Failed to create pipeline layout: {}", string_VkResult(result));
        return;
    }

    rtPipelineLayout_ = Handle<VkPipelineLayout>(pipelineLayout, stone_device(), vkDestroyPipelineLayout);
    LOG_SUCCESS_CAT("PIPELINE", "Ray tracing pipeline layout created");
}

// =============================================================================
// Shader Binding Table Creation
// =============================================================================
void PipelineManager::createShaderBindingTable(VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd)
{
    if (rtPipeline_.get() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("PIPELINE", "Cannot create SBT – pipeline not created");
        return;
    }

    cacheDeviceProperties();

    const auto& rtProps = StoneKey::stone_rtprops();
    const VkDeviceSize handleSize = rtProps.shaderGroupHandleSize;
    const VkDeviceSize handleAlign = std::max(rtProps.shaderGroupHandleAlignment, 32u);
    const VkDeviceSize baseAlign = std::max(rtProps.shaderGroupBaseAlignment, 64u);
    const VkDeviceSize stride = ((handleSize + handleAlign - 1) / handleAlign) * handleAlign;

    const uint32_t totalGroups = raygenGroupCount_ + missGroupCount_ + hitGroupCount_;
    if (totalGroups == 0) {
        LOG_FATAL_CAT("PIPELINE", "No shader groups defined");
        return;
    }

    const VkDeviceSize raygenSize = ((raygenGroupCount_ * stride) + baseAlign - 1) / baseAlign * baseAlign;
    const VkDeviceSize missSize   = missGroupCount_ * stride;
    const VkDeviceSize hitSize    = hitGroupCount_ * stride;
    const VkDeviceSize sbtSize    = raygenSize + missSize + hitSize;

    uint64_t sbtBufferHandle = BufferManager::create(
        sbtSize,
        VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        "RTX_SBT_Buffer");

    if (sbtBufferHandle == 0) {
        LOG_FATAL_CAT("PIPELINE", "Failed to allocate SBT buffer");
        return;
    }

    const auto* bufferInfo = BufferManager::get(sbtBufferHandle);

    std::vector<uint8_t> shaderHandles(totalGroups * handleSize);
    VkResult result = g_ext.vkGetRayTracingShaderGroupHandlesKHR(
        stone_device(), rtPipeline_.get(), 0, totalGroups, shaderHandles.size(), shaderHandles.data());

    if (result != VK_SUCCESS) {
        BufferManager::destroy(sbtBufferHandle);
        LOG_FATAL_CAT("PIPELINE", "Failed to retrieve shader group handles: {}", string_VkResult(result));
        return;
    }

    void* stagingPtr = BufferManager::mapStaging(shaderHandles.size());
    if (!stagingPtr) {
        BufferManager::destroy(sbtBufferHandle);
        LOG_FATAL_CAT("PIPELINE", "Staging buffer overflow during SBT creation");
        return;
    }

    std::memcpy(stagingPtr, shaderHandles.data(), shaderHandles.size());

    VkBufferCopy copyRegion{
        .srcOffset = BufferManager::getStagingOffset() - shaderHandles.size(),
        .dstOffset = 0,
        .size      = shaderHandles.size()
    };
    vkCmdCopyBuffer(cmd, BufferManager::getStagingBuffer(), bufferInfo->buffer, 1, &copyRegion);

    VkMemoryBarrier2 barrier{
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask  = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstStageMask  = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
    };

    VkDependencyInfo depInfo{
        .sType              = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1,
        .pMemoryBarriers    = &barrier
    };
    g_ext.vkCmdPipelineBarrier2(cmd, &depInfo);

    VkDeviceAddress sbtAddress = BufferManager::get_device_address(sbtBufferHandle);

    raygenSbtRegion_   = {sbtAddress,                     stride, raygenSize};
    missSbtRegion_     = {sbtAddress + raygenSize,        stride, missSize};
    hitSbtRegion_      = {sbtAddress + raygenSize + missSize, stride, hitSize};
    callableSbtRegion_ = {0, 0, 0};

    sbtBuffer_  = Handle<VkBuffer>(bufferInfo->buffer, stone_device(), vkDestroyBuffer);
    sbtMemory_  = Handle<VkDeviceMemory>(bufferInfo->memory, stone_device(), vkFreeMemory);
    sbtAddress_ = sbtAddress;
    sbtSize_    = sbtSize;

    LOG_SUCCESS_CAT("PIPELINE", "Shader binding table created – {} groups, {} bytes", totalGroups, sbtSize);
}

// =============================================================================
// Full Pipeline Construction
// =============================================================================
void PipelineManager::forgeRTXPipeline(VkCommandPool commandPool, VkQueue graphicsQueue, VkCommandBuffer mainCmd)
{
    if (s_crownForged) {
        return;
    }

    createDescriptorPool();
    createPipelineLayout();
    allocateDescriptorSets();
    createRayTracingPipeline();
    createShaderBindingTable(commandPool, graphicsQueue, mainCmd);

    StoneKey::stone_seal_pipeline(this);
    s_crownForged = true;

    LOG_SUCCESS_CAT("PIPELINE", "RTX pipeline fully constructed and ready");
}

// =============================================================================
// Public Accessors
// =============================================================================
VkDescriptorSet PipelineManager::getDescriptorSet(uint32_t frameIndex) const
{
    return (frameIndex < rtDescriptorSets_.size()) ? rtDescriptorSets_[frameIndex] : VK_NULL_HANDLE;
}

VkPipeline PipelineManager::getPipeline() const
{
    return rtPipeline_.get();
}

VkPipelineLayout PipelineManager::getPipelineLayout() const
{
    return rtPipelineLayout_.get();
}

PipelineManager::~PipelineManager() = default;

} // namespace RTX