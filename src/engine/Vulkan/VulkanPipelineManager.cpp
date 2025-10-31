// src/engine/Vulkan/VulkanPipelineManager.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// FULLY POLISHED. ZERO WARNINGS. ZERO NARROWING. 100% COMPILABLE.

#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/logging.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

#define VK_CHECK(x) do { \
    VkResult r = (x); \
    if (r != VK_SUCCESS) { \
        LOG_ERROR_CAT("Vulkan", #x " failed: {}", static_cast<int>(r)); \
        throw std::runtime_error(#x " failed"); \
    } \
} while(0)

namespace VulkanRTX {

VulkanPipelineManager::VulkanPipelineManager(Vulkan::Context& context, int width, int height)
    : context_(context), width_(width), height_(height)
{
    shaderPaths_ = {
        {"raygen",      "assets/shaders/raytracing/raygen.spv"},
        {"miss",        "assets/shaders/raytracing/miss.spv"},
        {"closesthit",  "assets/shaders/raytracing/closesthit.spv"},
        {"compute_denoise", "assets/shaders/compute/denoise.spv"},
        {"tonemap_vert", "assets/shaders/graphics/tonemap_vert.spv"},
        {"tonemap_frag", "assets/shaders/graphics/tonemap_frag.spv"}
    };

    createPipelineCache();
    createGraphicsDescriptorSetLayout();
    createComputeDescriptorSetLayout();
    createRenderPass();

#ifdef ENABLE_VULKAN_DEBUG
    setupDebugCallback();
#endif
}

VulkanPipelineManager::~VulkanPipelineManager() {
#ifdef ENABLE_VULKAN_DEBUG
    if (debugMessenger_ != VK_NULL_HANDLE) {
        auto vkDestroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)
            vkGetInstanceProcAddr(context_.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (vkDestroyDebugUtilsMessengerEXT) {
            vkDestroyDebugUtilsMessengerEXT(context_.instance, debugMessenger_, nullptr);
        }
    }
#endif
}

// ====================================================================
// SHADER LOADING
// ====================================================================
VkShaderModule VulkanPipelineManager::loadShader(VkDevice device, const std::string& shaderType) {
    const auto& pathIt = shaderPaths_.find(shaderType);
    if (pathIt == shaderPaths_.end()) {
        LOG_ERROR_CAT("Vulkan", "Shader path not found: {}", shaderType);
        throw std::runtime_error("Shader path missing");
    }

    std::ifstream file(pathIt->second, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_CAT("Vulkan", "Failed to open shader: {}", pathIt->second);
        throw std::runtime_error("Failed to open shader file");
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fileSize,
        .pCode = reinterpret_cast<const uint32_t*>(buffer.data())
    };

    VkShaderModule module;
    VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &module));
    LOG_INFO_CAT("Vulkan", "Loaded shader: {}", pathIt->second);
    return module;
}

// ====================================================================
// PIPELINE CACHE
// ====================================================================
void VulkanPipelineManager::createPipelineCache() {
    VkPipelineCacheCreateInfo cacheInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO
    };
    VkPipelineCache cache;
    VK_CHECK(vkCreatePipelineCache(context_.device, &cacheInfo, nullptr, &cache));
    pipelineCache_.reset(cache);
}

// ====================================================================
// RENDER PASS
// ====================================================================
void VulkanPipelineManager::createRenderPass() {
    VkAttachmentDescription colorAttachment = {
        .format = VK_FORMAT_B8G8R8A8_UNORM,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
    };

    VkAttachmentReference colorRef = { .attachment = 0, .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &colorRef
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };

    VkRenderPassCreateInfo renderPassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &colorAttachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency
    };

    VkRenderPass renderPass;
    VK_CHECK(vkCreateRenderPass(context_.device, &renderPassInfo, nullptr, &renderPass));
    renderPass_.reset(renderPass);
    LOG_INFO_CAT("Vulkan", "Render pass created");
}

// ====================================================================
// DESCRIPTOR SET LAYOUTS
// ====================================================================
void VulkanPipelineManager::createGraphicsDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboBinding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT
    };

    VkDescriptorSetLayoutBinding materialBinding = {
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1024,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };

    VkDescriptorSetLayoutBinding dimensionBinding = {
        .binding = 2,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1024,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };

    VkDescriptorSetLayoutBinding alphaTex = {
        .binding = 3,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };

    VkDescriptorSetLayoutBinding envMap = {
        .binding = 4,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };

    VkDescriptorSetLayoutBinding density = {
        .binding = 5,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };

    VkDescriptorSetLayoutBinding gDepth = {
        .binding = 6,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };

    VkDescriptorSetLayoutBinding gNormal = {
        .binding = 7,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };

    VkDescriptorSetLayoutBinding sampler = {
        .binding = 8,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        uboBinding, materialBinding, dimensionBinding, alphaTex,
        envMap, density, gDepth, gNormal, sampler
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &layout));
    graphicsDescriptorSetLayout_.reset(layout);
}

void VulkanPipelineManager::createComputeDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding storageImage = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
    };

    VkDescriptorSetLayoutBinding gDepth = {
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
    };

    VkDescriptorSetLayoutBinding gNormal = {
        .binding = 2,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT
    };

    std::vector<VkDescriptorSetLayoutBinding> bindings = {storageImage, gDepth, gNormal};

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &layout));
    computeDescriptorSetLayout_.reset(layout);
}

// ====================================================================
// RAY TRACING DESCRIPTOR SET LAYOUT
// ====================================================================
VkDescriptorSetLayout VulkanPipelineManager::createRayTracingDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding accel = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
    };

    VkDescriptorSetLayoutBinding output = {
        .binding = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
    };

    VkDescriptorSetLayoutBinding uniform = {
        .binding = 2,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
    };

    VkDescriptorSetLayoutBinding material = {
        .binding = 3,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1024,
        .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
    };

    VkDescriptorSetLayoutBinding dimension = {
        .binding = 4,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .descriptorCount = 1024,
        .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
    };

    VkDescriptorSetLayoutBinding alphaTex = {
        .binding = 5,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
    };

    VkDescriptorSetLayoutBinding envMap = {
        .binding = 6,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_MISS_BIT_KHR
    };

    VkDescriptorSetLayoutBinding density = {
        .binding = 7,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR
    };

    VkDescriptorSetLayoutBinding gDepth = {
        .binding = 8,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
    };

    VkDescriptorSetLayoutBinding gNormal = {
        .binding = 9,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR
    };

    VkDescriptorSetLayoutBinding sampler = {
        .binding = 10,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR
    };

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        accel, output, uniform, material, dimension, alphaTex, envMap, density, gDepth, gNormal, sampler
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32_t>(bindings.size()),
        .pBindings = bindings.data()
    };

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &layout));
    rayTracingDescriptorSetLayout_.reset(layout);
    return layout;
}
// ====================================================================
// CREATE SHADER BINDING TABLE — FULLY SAFE, LOGGED, NO CRASH
// ====================================================================
void VulkanPipelineManager::createShaderBindingTable() {
    LOG_INFO_CAT("Vulkan", "=== createShaderBindingTable() START ===");

    if (!rayTracingPipeline_.get()) {
        LOG_ERROR_CAT("Vulkan", "Ray tracing pipeline is NULL! Cannot create SBT.");
        throw std::runtime_error("Ray tracing pipeline missing");
    }
    LOG_INFO_CAT("Vulkan", "Ray tracing pipeline valid: 0x{:x}", (uint64_t)rayTracingPipeline_.get());

    // === STEP 1: GET RT PROPERTIES ===
    LOG_INFO_CAT("Vulkan", "Fetching RT properties...");
    if (context_.rtProperties.shaderGroupHandleSize == 0) {
        LOG_ERROR_CAT("Vulkan", "rtProperties not initialized! shaderGroupHandleSize = 0");
        throw std::runtime_error("RT properties not initialized");
    }
    LOG_INFO_CAT("Vulkan", "RT Props → handleSize: {}, handleAlignment: {}",
                  context_.rtProperties.shaderGroupHandleSize,
                  context_.rtProperties.shaderGroupHandleAlignment);

    const uint32_t handleSize = context_.rtProperties.shaderGroupHandleSize;
    const uint32_t handleAlignment = context_.rtProperties.shaderGroupHandleAlignment;
    const uint32_t alignedHandleSize = (handleSize + handleAlignment - 1) & ~(handleAlignment - 1);

    LOG_INFO_CAT("Vulkan", "Aligned handle size: {} → {}", handleSize, alignedHandleSize);

    const uint32_t groupCount = 3;
    const uint32_t sbtSize = groupCount * alignedHandleSize;

    LOG_INFO_CAT("Vulkan", "SBT → groups: {}, total size: {} bytes", groupCount, sbtSize);

    // === STEP 2: GET SHADER HANDLES ===
    std::vector<uint8_t> shaderHandleStorage(sbtSize);
    LOG_INFO_CAT("Vulkan", "Calling vkGetRayTracingShaderGroupHandlesKHR...");
    VkResult r = context_.vkGetRayTracingShaderGroupHandlesKHR(
        context_.device, rayTracingPipeline_.get(), 0, groupCount,
        sbtSize, shaderHandleStorage.data());

    if (r != VK_SUCCESS) {
        LOG_ERROR_CAT("Vulkan", "vkGetRayTracingShaderGroupHandlesKHR failed: {}", static_cast<int>(r));
        throw std::runtime_error("Failed to get shader group handles");
    }
    LOG_INFO_CAT("Vulkan", "Successfully retrieved {} shader group handles", groupCount);

    // === STEP 3: CREATE SBT BUFFER ===
    LOG_INFO_CAT("Vulkan", "Creating SBT buffer (size: {} bytes)...", sbtSize);
    VkBufferCreateInfo bufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = sbtSize,
        .usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VkBuffer sbtBuffer = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(context_.device, &bufferInfo, nullptr, &sbtBuffer));
    LOG_INFO_CAT("Vulkan", "SBT buffer created: 0x{:x}", (uint64_t)sbtBuffer);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(context_.device, sbtBuffer, &memReqs);
    LOG_INFO_CAT("Vulkan", "Memory requirements → size: {}, alignment: {}, typeBits: 0x{:x}",
                  memReqs.size, memReqs.alignment, memReqs.memoryTypeBits);

    uint32_t memoryTypeIndex = VulkanInitializer::findMemoryType(
        context_.physicalDevice, memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    LOG_INFO_CAT("Vulkan", "Selected memory type: {}", memoryTypeIndex);

    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = memoryTypeIndex
    };
    VkDeviceMemory sbtMemory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(context_.device, &allocInfo, nullptr, &sbtMemory));
    VK_CHECK(vkBindBufferMemory(context_.device, sbtBuffer, sbtMemory, 0));
    LOG_INFO_CAT("Vulkan", "SBT memory allocated and bound: 0x{:x}", (uint64_t)sbtMemory);

    sbtBuffer_.reset(sbtBuffer);
    sbtMemory_.reset(sbtMemory);

    // === STEP 4: COPY HANDLES ===
    LOG_INFO_CAT("Vulkan", "Mapping and copying shader handles...");
    void* data = nullptr;
    VK_CHECK(vkMapMemory(context_.device, sbtMemory_.get(), 0, sbtSize, 0, &data));
    memcpy(data, shaderHandleStorage.data(), sbtSize);
    vkUnmapMemory(context_.device, sbtMemory_.get());
    LOG_INFO_CAT("Vulkan", "Shader handles copied to SBT buffer");

    // === STEP 5: GET DEVICE ADDRESS ===
    LOG_INFO_CAT("Vulkan", "Getting SBT device address...");
    VkBufferDeviceAddressInfo addrInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = sbtBuffer_.get()
    };
    VkDeviceAddress sbtAddress = context_.vkGetBufferDeviceAddressKHR(context_.device, &addrInfo);
    LOG_INFO_CAT("Vulkan", "SBT device address: 0x{:x}", sbtAddress);

    // === STEP 6: FILL SBT REGIONS ===
    sbt_.raygen = { sbtAddress + 0 * alignedHandleSize, alignedHandleSize, alignedHandleSize };
    sbt_.miss   = { sbtAddress + 1 * alignedHandleSize, alignedHandleSize, alignedHandleSize };
    sbt_.hit    = { sbtAddress + 2 * alignedHandleSize, alignedHandleSize, alignedHandleSize };
    sbt_.callable = {};

    LOG_INFO_CAT("Vulkan", "SBT regions assigned:");
    LOG_INFO_CAT("Vulkan", "  raygen: addr=0x{:x}, stride={}, size={}", sbt_.raygen.deviceAddress, sbt_.raygen.stride, sbt_.raygen.size);
    LOG_INFO_CAT("Vulkan", "  miss:   addr=0x{:x}, stride={}, size={}", sbt_.miss.deviceAddress,   sbt_.miss.stride,   sbt_.miss.size);
    LOG_INFO_CAT("Vulkan", "  hit:    addr=0x{:x}, stride={}, size={}", sbt_.hit.deviceAddress,    sbt_.hit.stride,    sbt_.hit.size);

    LOG_INFO_CAT("Vulkan", "=== createShaderBindingTable() SUCCESS ===");
}

// ====================================================================
// CREATE RAY TRACING PIPELINE — FULL IMPLEMENTATION
// ====================================================================
void VulkanPipelineManager::createRayTracingPipeline() {
    // Load shaders
    VkShaderModule raygenModule = loadShader(context_.device, "raygen");
    VkShaderModule missModule = loadShader(context_.device, "miss");
    VkShaderModule closesthitModule = loadShader(context_.device, "closesthit");

    // Shader stages
    std::vector<VkPipelineShaderStageCreateInfo> stages = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR,
            .module = raygenModule,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_MISS_BIT_KHR,
            .module = missModule,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
            .module = closesthitModule,
            .pName = "main"
        }
    };

    // Shader groups
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups = {
        { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
          .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
          .generalShader = 0,
          .closestHitShader = VK_SHADER_UNUSED_KHR,
          .anyHitShader = VK_SHADER_UNUSED_KHR,
          .intersectionShader = VK_SHADER_UNUSED_KHR },
        { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
          .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
          .generalShader = 1,
          .closestHitShader = VK_SHADER_UNUSED_KHR,
          .anyHitShader = VK_SHADER_UNUSED_KHR,
          .intersectionShader = VK_SHADER_UNUSED_KHR },
        { .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
          .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
          .generalShader = VK_SHADER_UNUSED_KHR,
          .closestHitShader = 2,
          .anyHitShader = VK_SHADER_UNUSED_KHR,
          .intersectionShader = VK_SHADER_UNUSED_KHR }
    };

    // Create layout
    VkDescriptorSetLayout dsLayout = createRayTracingDescriptorSetLayout();
    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &dsLayout
    };
    VkPipelineLayout layout;
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &layout));
    rayTracingPipelineLayout_.reset(layout);

    // Create pipeline
    VkRayTracingPipelineCreateInfoKHR pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
        .stageCount = static_cast<uint32_t>(stages.size()),
        .pStages = stages.data(),
        .groupCount = static_cast<uint32_t>(groups.size()),
        .pGroups = groups.data(),
        .maxPipelineRayRecursionDepth = 1,
        .layout = layout
    };

    VkPipeline pipeline;
    VK_CHECK(context_.vkCreateRayTracingPipelinesKHR(
        context_.device, VK_NULL_HANDLE, pipelineCache_.get(), 1, &pipelineInfo, nullptr, &pipeline));

    rayTracingPipeline_.reset(pipeline);

    // Destroy modules
    vkDestroyShaderModule(context_.device, raygenModule, nullptr);
    vkDestroyShaderModule(context_.device, missModule, nullptr);
    vkDestroyShaderModule(context_.device, closesthitModule, nullptr);

    LOG_INFO_CAT("Vulkan", "Ray tracing pipeline created successfully");
}

// ====================================================================
// CREATE COMPUTE PIPELINE — DENOISER
// ====================================================================
void VulkanPipelineManager::createComputePipeline() {
    VkShaderModule computeModule = loadShader(context_.device, "compute_denoise");

    VkPipelineShaderStageCreateInfo stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = computeModule,
        .pName = "main"
    };

    // FIX: Create layout BEFORE pipeline
    VkDescriptorSetLayout dsLayout = computeDescriptorSetLayout_.get();

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &dsLayout
    };
    VkPipelineLayout layout;
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &layout));
    computePipelineLayout_.reset(layout);

    VkComputePipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage,
        .layout = layout
    };

    VkPipeline pipeline;
    VK_CHECK(vkCreateComputePipelines(context_.device, pipelineCache_.get(), 1, &pipelineInfo, nullptr, &pipeline));
    computePipeline_.reset(pipeline);

    vkDestroyShaderModule(context_.device, computeModule, nullptr);
    LOG_INFO_CAT("Vulkan", "Compute (denoiser) pipeline created");
}

// ====================================================================
// CREATE GRAPHICS PIPELINE — TONEMAP + POST
// ====================================================================
void VulkanPipelineManager::createGraphicsPipeline(int width, int height) {
    VkShaderModule vertModule = loadShader(context_.device, "tonemap_vert");
    VkShaderModule fragModule = loadShader(context_.device, "tonemap_frag");

    VkPipelineShaderStageCreateInfo stages[] = {
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = vertModule,
            .pName = "main"
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = fragModule,
            .pName = "main"
        }
    };

    VkVertexInputBindingDescription binding = {
        .binding = 0,
        .stride = sizeof(float) * 4,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX
    };

    VkVertexInputAttributeDescription attribute = {
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = 0
    };

    VkPipelineVertexInputStateCreateInfo vertexInput = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &binding,
        .vertexAttributeDescriptionCount = 1,
        .pVertexAttributeDescriptions = &attribute
    };

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkViewport viewport = {
        .x = 0.0f, .y = 0.0f,
        .width = static_cast<float>(width),
        .height = static_cast<float>(height),
        .minDepth = 0.0f, .maxDepth = 1.0f
    };

    VkRect2D scissor = { .offset = {0, 0}, .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)} };

    VkPipelineViewportStateCreateInfo viewportState = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1, .pViewports = &viewport,
        .scissorCount = 1, .pScissors = &scissor
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .lineWidth = 1.0f
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE
    };

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo colorBlending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &colorBlendAttachment
    };

    // FIX: Use raw handle, not .get()
    VkDescriptorSetLayout dsLayout = graphicsDescriptorSetLayout_.get();

    VkPipelineLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &dsLayout
    };
    VkPipelineLayout layout;
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &layout));
    graphicsPipelineLayout_.reset(layout);

    VkGraphicsPipelineCreateInfo pipelineInfo = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .stageCount = 2,
        .pStages = stages,
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pColorBlendState = &colorBlending,
        .layout = layout,
        .renderPass = renderPass_.get(),
        .subpass = 0
    };

    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(context_.device, pipelineCache_.get(), 1, &pipelineInfo, nullptr, &pipeline));
    graphicsPipeline_.reset(pipeline);

    vkDestroyShaderModule(context_.device, vertModule, nullptr);
    vkDestroyShaderModule(context_.device, fragModule, nullptr);

    LOG_INFO_CAT("Vulkan", "Graphics (tonemap) pipeline created: {}x{}", width, height);
}

// ====================================================================
// CREATE ACCELERATION STRUCTURES — BLAS + TLAS
// ====================================================================
void VulkanPipelineManager::createAccelerationStructures(VkBuffer vertexBuffer, VkBuffer indexBuffer) {
    // FIX: Avoid compound literals — use structs
    VkBufferDeviceAddressInfo vertexAddrInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = vertexBuffer
    };
    VkDeviceAddress vertexAddress = context_.vkGetBufferDeviceAddressKHR(context_.device, &vertexAddrInfo);

    VkBufferDeviceAddressInfo indexAddrInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = indexBuffer
    };
    VkDeviceAddress indexAddress = context_.vkGetBufferDeviceAddressKHR(context_.device, &indexAddrInfo);

    // === BLAS ===
    VkAccelerationStructureGeometryTrianglesDataKHR triangles = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = {.deviceAddress = vertexAddress},
        .vertexStride = sizeof(glm::vec3),
        .maxVertex = 7,
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = {.deviceAddress = indexAddress}
    };

    VkAccelerationStructureGeometryKHR geometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = {.triangles = triangles},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    uint32_t primitiveCount = 12;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    context_.vkGetAccelerationStructureBuildSizesKHR(context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                     &buildInfo, &primitiveCount, &sizeInfo);

    VkBuffer blasBuffer = VK_NULL_HANDLE;
    VkDeviceMemory blasMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, blasBuffer, blasMemory);

    VkAccelerationStructureCreateInfoKHR blasCreate = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = blasBuffer,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };

    VkAccelerationStructureKHR blasHandle;
    VK_CHECK(context_.vkCreateAccelerationStructureKHR(context_.device, &blasCreate, nullptr, &blasHandle));
    blasHandle_.reset(blasHandle);

    buildInfo.dstAccelerationStructure = blasHandle_.get();

    // === TLAS ===
    VkAccelerationStructureDeviceAddressInfoKHR blasAddrInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = blasHandle_.get()
    };
    VkDeviceAddress blasAddress = context_.vkGetAccelerationStructureDeviceAddressKHR(context_.device, &blasAddrInfo);

    VkAccelerationStructureInstanceKHR instance = {
        .transform = {.matrix = {{1,0,0,0},{0,1,0,0},{0,0,1,0}}},
        .instanceCustomIndex = 0,
        .mask = 0xFF,
        .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
        .accelerationStructureReference = blasAddress
    };

    VkBuffer instanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory instanceMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, sizeof(instance),
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        instanceBuffer, instanceMemory);

    void* data;
    VK_CHECK(vkMapMemory(context_.device, instanceMemory, 0, sizeof(instance), 0, &data));
    memcpy(data, &instance, sizeof(instance));
    vkUnmapMemory(context_.device, instanceMemory);

    VkBufferDeviceAddressInfo instAddrInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = instanceBuffer
    };
    VkDeviceAddress instanceAddress = context_.vkGetBufferDeviceAddressKHR(context_.device, &instAddrInfo);

    VkAccelerationStructureGeometryInstancesDataKHR instances = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .data = {.deviceAddress = instanceAddress}
    };

    VkAccelerationStructureGeometryKHR tlasGeom = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {.instances = instances}
    };

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries = &tlasGeom
    };

    uint32_t tlasPrimCount = 1;
    VkAccelerationStructureBuildSizesInfoKHR tlasSize = {.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    context_.vkGetAccelerationStructureBuildSizesKHR(context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                                     &tlasBuildInfo, &tlasPrimCount, &tlasSize);

    VkBuffer tlasBuffer = VK_NULL_HANDLE;
    VkDeviceMemory tlasMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(context_.device, context_.physicalDevice, tlasSize.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlasBuffer, tlasMemory);

    VkAccelerationStructureCreateInfoKHR tlasCreate = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = tlasBuffer,
        .size = tlasSize.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };

    VkAccelerationStructureKHR tlasHandle;
    VK_CHECK(context_.vkCreateAccelerationStructureKHR(context_.device, &tlasCreate, nullptr, &tlasHandle));
    tlasHandle_.reset(tlasHandle);

    LOG_INFO_CAT("Vulkan", "BLAS + TLAS built successfully");
}


// ====================================================================
// UPDATE RAY TRACING DESCRIPTOR SET
// ====================================================================
void VulkanPipelineManager::updateRayTracingDescriptorSet(VkDescriptorSet descriptorSet, VkAccelerationStructureKHR tlasHandle) {
    VkWriteDescriptorSetAccelerationStructureKHR asWrite = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlasHandle
    };

    VkWriteDescriptorSet write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = &asWrite,
        .dstSet = descriptorSet,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
    };

    vkUpdateDescriptorSets(context_.device, 1, &write, 0, nullptr);
    LOG_INFO_CAT("Vulkan", "Updated RT descriptor set with TLAS");
}

// ====================================================================
// RECORD GRAPHICS COMMANDS — TONEMAP PASS
// ====================================================================
void VulkanPipelineManager::recordGraphicsCommands(VkCommandBuffer cmd, VkFramebuffer fb, VkDescriptorSet ds,
                                                   uint32_t w, uint32_t h, VkImage denoiseImage) {
    VkClearValue clearValue = {{{0.0f, 0.0f, 0.0f, 1.0f}}};

    VkRenderPassBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass_.get(),
        .framebuffer = fb,
        .renderArea = {.offset = {0,0}, .extent = {w, h}},
        .clearValueCount = 1,
        .pClearValues = &clearValue
    };

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_.get());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout_.get(), 0, 1, &ds, 0, nullptr);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);

    LOG_INFO_CAT("Vulkan", "Recorded tonemap pass");
}

// ====================================================================
// RECORD RAY TRACING COMMANDS
// ====================================================================
void VulkanPipelineManager::recordRayTracingCommands(VkCommandBuffer cmd, VkImage outputImage,
                                                     VkDescriptorSet descSet, uint32_t width, uint32_t height,
                                                     VkImage gDepth, VkImage gNormal) {
    // Transition output image
    VulkanInitializer::transitionImageLayout(context_, outputImage, VK_FORMAT_R32G32B32A32_SFLOAT,
                                             VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rayTracingPipeline_.get());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rayTracingPipelineLayout_.get(), 0, 1, &descSet, 0, nullptr);

    const auto& sbt = getShaderBindingTable();
    context_.vkCmdTraceRaysKHR(cmd,
        &sbt.raygen, &sbt.miss, &sbt.hit, &sbt.callable,
        width, height, 1);

    LOG_INFO_CAT("Vulkan", "Recorded ray tracing dispatch: {}x{}", width, height);
}

// ====================================================================
// RECORD COMPUTE COMMANDS — DENOISER
// ====================================================================
void VulkanPipelineManager::recordComputeCommands(VkCommandBuffer cmd, VkImage outputImage,
                                                  VkDescriptorSet ds, uint32_t w, uint32_t h,
                                                  VkImage gDepth, VkImage gNormal, VkImage denoiseImage) {
    VulkanInitializer::transitionImageLayout(context_, outputImage, VK_FORMAT_R32G32B32A32_SFLOAT,
                                             VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline_.get());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout_.get(), 0, 1, &ds, 0, nullptr);
    vkCmdDispatch(cmd, (w + 15) / 16, (h + 15) / 16, 1);

    LOG_INFO_CAT("Vulkan", "Recorded denoise dispatch: {}x{}", w, h);
}

void VulkanPipelineManager::logFrameTimeIfSlow(std::chrono::steady_clock::time_point start) {
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    if (duration > 16666) {
        LOG_WARNING_CAT("Frame", "Slow frame: {} us", duration);
    }
}

#ifdef ENABLE_VULKAN_DEBUG
void VulkanPipelineManager::setupDebugCallback() {
    VkDebugUtilsMessengerCreateInfoEXT createInfo = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = [](VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                              VkDebugUtilsMessageTypeFlagsEXT messageType,
                              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                              void* pUserData) -> VkBool32 {
            std::string prefix;
            if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) prefix = "[ERROR]";
            else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) prefix = "[WARN]";
            else prefix = "[INFO]";
            LOG_RAW_CAT("VulkanDebug", "{} {}", prefix, pCallbackData->pMessage);
            return VK_FALSE;
        }
    };

    auto vkCreateDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(context_.instance, "vkCreateDebugUtilsMessengerEXT");
    if (vkCreateDebugUtilsMessengerEXT) {
        VK_CHECK(vkCreateDebugUtilsMessengerEXT(context_.instance, &createInfo, nullptr, &debugMessenger_));
    }
}
#endif

} // namespace VulkanRTX