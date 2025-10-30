// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/types.hpp"  // <-- Added: for DenoisePushConstants
#include "engine/logging.hpp"
#include <stdexcept>
#include <fstream>
#include <algorithm>
#include <chrono>

namespace VulkanRTX {

#define VK_CHECK(x) do { VkResult r = (x); if (r != VK_SUCCESS) { LOG_ERROR_CAT("Pipeline", #x " failed: {}", static_cast<int>(r)); throw std::runtime_error(#x " failed"); } } while(0)

// -----------------------------------------------------------------------------
// CONSTRUCTOR
// -----------------------------------------------------------------------------
VulkanPipelineManager::VulkanPipelineManager(Vulkan::Context& context, int width, int height)
    : context_(context), width_(width), height_(height),
      rayTracingPipeline_(nullptr), rayTracingPipelineLayout_(nullptr),
      computePipeline_(nullptr), computePipelineLayout_(nullptr),
      graphicsPipeline_(nullptr), graphicsPipelineLayout_(nullptr),
      renderPass_(VK_NULL_HANDLE), pipelineCache_(VK_NULL_HANDLE),
      rasterPrepassPipeline_(VK_NULL_HANDLE), denoiserPostPipeline_(VK_NULL_HANDLE),
      tlasHandle_(VK_NULL_HANDLE), blasHandle_(VK_NULL_HANDLE),
      computeDescriptorSetLayout_(VK_NULL_HANDLE), rayTracingDescriptorSetLayout_(VK_NULL_HANDLE),
      sbtBuffer_(VK_NULL_HANDLE), sbtMemory_(VK_NULL_HANDLE),
      createAsFunc_(nullptr), destroyAsFunc_(nullptr),
      getRayTracingShaderGroupHandlesFunc_(nullptr)
{
    LOG_INFO_CAT("Pipeline", "Initializing VulkanPipelineManager...");

    createAsFunc_ = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
        vkGetDeviceProcAddr(context_.device, "vkCreateAccelerationStructureKHR"));
    destroyAsFunc_ = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
        vkGetDeviceProcAddr(context_.device, "vkDestroyAccelerationStructureKHR"));
    getRayTracingShaderGroupHandlesFunc_ = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
        vkGetDeviceProcAddr(context_.device, "vkGetRayTracingShaderGroupHandlesKHR"));

    if (!createAsFunc_ || !destroyAsFunc_ || !getRayTracingShaderGroupHandlesFunc_) {
        throw std::runtime_error("Failed to load required ray tracing function pointers");
    }

    shaderPaths_ = {
        {"raygen",      "assets/shaders/raytracing/raygen.spv"},
        {"miss",        "assets/shaders/raytracing/miss.spv"},
        {"closesthit",  "assets/shaders/raytracing/closesthit.spv"},
        {"compute_denoise", "assets/shaders/compute/denoise.spv"}
    };

    createPipelineCache();
    createRayTracingDescriptorSetLayout();  // This now creates pipeline layout
    createComputeDescriptorSetLayout();    // FIXED: Now defined for denoiser

    // === QUERY RT PROPERTIES HERE (needed for SBT) ===
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
    rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rtProps;
    vkGetPhysicalDeviceProperties2(context_.physicalDevice, &props2);

    context_.rtProperties = rtProps;

    if (rtProps.shaderGroupHandleSize == 0) {
        LOG_ERROR_CAT("Pipeline", "Driver reports zero shaderGroupHandleSize!");
        throw std::runtime_error("Ray tracing not supported");
    }

    LOG_INFO_CAT("Pipeline", "RT Properties: handleSize={}, baseAlign={}, handleAlign={}",
                 rtProps.shaderGroupHandleSize, rtProps.shaderGroupBaseAlignment, rtProps.shaderGroupHandleAlignment);
}

// -----------------------------------------------------------------------------
// DESTRUCTOR
// -----------------------------------------------------------------------------
VulkanPipelineManager::~VulkanPipelineManager() {
    LOG_INFO_CAT("Pipeline", "Destroying VulkanPipelineManager...");

    if (sbtBuffer_) vkDestroyBuffer(context_.device, sbtBuffer_, nullptr);
    if (sbtMemory_) vkFreeMemory(context_.device, sbtMemory_, nullptr);

    if (tlasHandle_ && destroyAsFunc_) destroyAsFunc_(context_.device, tlasHandle_, nullptr);
    if (blasHandle_ && destroyAsFunc_) destroyAsFunc_(context_.device, blasHandle_, nullptr);

    if (pipelineCache_) vkDestroyPipelineCache(context_.device, pipelineCache_, nullptr);
    if (renderPass_) vkDestroyRenderPass(context_.device, renderPass_, nullptr);

    if (rayTracingDescriptorSetLayout_) vkDestroyDescriptorSetLayout(context_.device, rayTracingDescriptorSetLayout_, nullptr);
    if (computeDescriptorSetLayout_) vkDestroyDescriptorSetLayout(context_.device, computeDescriptorSetLayout_, nullptr);
    if (context_.graphicsDescriptorSetLayout) vkDestroyDescriptorSetLayout(context_.device, context_.graphicsDescriptorSetLayout, nullptr);
}

// -----------------------------------------------------------------------------
// LOAD SHADER
// -----------------------------------------------------------------------------
VkShaderModule VulkanPipelineManager::loadShader(VkDevice device, const std::string& shaderType) {
    auto it = shaderPaths_.find(shaderType);
    if (it == shaderPaths_.end()) {
        throw std::runtime_error("Shader type not found: " + shaderType);
    }

    std::ifstream file(it->second, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open shader file: " + it->second);
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    VkShaderModule shaderModule;
    VK_CHECK(vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule));
    return shaderModule;
}

// -----------------------------------------------------------------------------
// CREATE PIPELINE CACHE
// -----------------------------------------------------------------------------
void VulkanPipelineManager::createPipelineCache() {
    VkPipelineCacheCreateInfo cacheInfo{};
    cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    VK_CHECK(vkCreatePipelineCache(context_.device, &cacheInfo, nullptr, &pipelineCache_));
}

// -----------------------------------------------------------------------------
// CREATE RAY TRACING DESCRIPTOR SET LAYOUT + PIPELINE LAYOUT
// -----------------------------------------------------------------------------
VkDescriptorSetLayout VulkanPipelineManager::createRayTracingDescriptorSetLayout() {
    if (rayTracingDescriptorSetLayout_ != VK_NULL_HANDLE) {
        return rayTracingDescriptorSetLayout_;
    }

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR},
        {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 26, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR},
        {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR},
        {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR},
        {7, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR},
        {8, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR},
        {9, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR}
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &rayTracingDescriptorSetLayout_));
    LOG_INFO_CAT("Pipeline", "Ray tracing descriptor set layout created.");

    // === CREATE PIPELINE LAYOUT NOW ===
    if (rayTracingPipelineLayout_ == nullptr) {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &rayTracingDescriptorSetLayout_;

        VkPipelineLayout layout;
        VK_CHECK(vkCreatePipelineLayout(context_.device, &pipelineLayoutInfo, nullptr, &layout));
        rayTracingPipelineLayout_.reset(new VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>(
            context_.device, layout, vkDestroyPipelineLayout));
        LOG_INFO_CAT("Pipeline", "Ray tracing pipeline layout created.");
    }

    return rayTracingDescriptorSetLayout_;
}

// -----------------------------------------------------------------------------
// CREATE COMPUTE DESCRIPTOR SET LAYOUT (Adjusted for Denoiser: Input as Combined Sampler, Output as Storage Image)
// -----------------------------------------------------------------------------
void VulkanPipelineManager::createComputeDescriptorSetLayout() {
    if (computeDescriptorSetLayout_ != VK_NULL_HANDLE) {
        return;
    }

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT},  // Noisy input image (sampled)
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},          // Denoised output image
        {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT}          // Uniform for image size/kernel radius
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &computeDescriptorSetLayout_));
    LOG_INFO_CAT("Pipeline", "Compute descriptor set layout created for denoiser.");
}

// -----------------------------------------------------------------------------
// CREATE GRAPHICS DESCRIPTOR SET LAYOUT (STUBBED FOR RAY TRACING FOCUS)
// -----------------------------------------------------------------------------
void VulkanPipelineManager::createGraphicsDescriptorSetLayout() {
    // Stubbed out for ray tracing only - no rasterization needed
    LOG_INFO_CAT("Pipeline", "Graphics descriptor set layout skipped (ray tracing mode).");
    context_.graphicsDescriptorSetLayout = VK_NULL_HANDLE;
}

// -----------------------------------------------------------------------------
// CREATE RAY TRACING PIPELINE
// -----------------------------------------------------------------------------
void VulkanPipelineManager::createRayTracingPipeline() {
    VkShaderModule raygenModule = loadShader(context_.device, "raygen");
    VkShaderModule missModule = loadShader(context_.device, "miss");
    VkShaderModule chitModule = loadShader(context_.device, "closesthit");

    std::vector<VkPipelineShaderStageCreateInfo> stages = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_RAYGEN_BIT_KHR, raygenModule, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_MISS_BIT_KHR, missModule, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, chitModule, "main", nullptr}
    };

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups = {
        {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR},
        {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR},
        {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 2, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR}
    };

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.pNext = &context_.rtProperties;
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.groupCount = static_cast<uint32_t>(groups.size());
    pipelineInfo.pGroups = groups.data();
    pipelineInfo.maxPipelineRayRecursionDepth = 1;
    pipelineInfo.layout = getRayTracingPipelineLayout();

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(context_.vkCreateRayTracingPipelinesKHR(
        context_.device, VK_NULL_HANDLE, pipelineCache_, 1, &pipelineInfo, nullptr, &pipeline));

    rayTracingPipeline_.reset(new VulkanResource<VkPipeline, PFN_vkDestroyPipeline>(
        context_.device, pipeline, vkDestroyPipeline));

    vkDestroyShaderModule(context_.device, raygenModule, nullptr);
    vkDestroyShaderModule(context_.device, missModule, nullptr);
    vkDestroyShaderModule(context_.device, chitModule, nullptr);

    LOG_INFO_CAT("Pipeline", "Ray tracing pipeline created successfully.");
}

// -----------------------------------------------------------------------------
// CREATE SHADER BINDING TABLE
// -----------------------------------------------------------------------------
void VulkanPipelineManager::createShaderBindingTable() {
    if (!rayTracingPipeline_) {
        LOG_ERROR_CAT("Pipeline", "Ray tracing pipeline not created before SBT!");
        throw std::runtime_error("Ray tracing pipeline missing");
    }

    LOG_INFO_CAT("Pipeline", "Creating Shader Binding Table...");

    const uint32_t handleSize = context_.rtProperties.shaderGroupHandleSize;
    const uint32_t handleAlignment = context_.rtProperties.shaderGroupHandleAlignment;
    const uint32_t baseAlignment = context_.rtProperties.shaderGroupBaseAlignment;

    const uint32_t alignedHandleSize = (handleSize + handleAlignment - 1) & ~(handleAlignment - 1);

    const uint32_t raygenCount = 1;
    const uint32_t missCount   = 1;
    const uint32_t hitCount    = 1;
    const uint32_t totalGroups = raygenCount + missCount + hitCount;

    shaderHandles_.resize(totalGroups * handleSize);

    VK_CHECK(context_.vkGetRayTracingShaderGroupHandlesKHR(
        context_.device, rayTracingPipeline_->get(), 0, totalGroups,
        shaderHandles_.size(), shaderHandles_.data()));

    const VkDeviceSize raygenSize = alignedHandleSize * raygenCount;
    const VkDeviceSize missSize   = alignedHandleSize * missCount;
    const VkDeviceSize hitSize    = alignedHandleSize * hitCount;
    const VkDeviceSize totalSize  = raygenSize + missSize + hitSize;
    const VkDeviceSize alignedTotalSize = (totalSize + baseAlignment - 1) & ~(baseAlignment - 1);

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = alignedTotalSize;
    bufferInfo.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(context_.device, &bufferInfo, nullptr, &sbtBuffer_));

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(context_.device, sbtBuffer_, &memReqs);

    VkMemoryAllocateFlagsInfo flagsInfo{};
    flagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
    flagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &flagsInfo;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = VulkanInitializer::findMemoryType(
        context_.physicalDevice, memReqs.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK(vkAllocateMemory(context_.device, &allocInfo, nullptr, &sbtMemory_));
    VK_CHECK(vkBindBufferMemory(context_.device, sbtBuffer_, sbtMemory_, 0));

    void* data;
    VK_CHECK(vkMapMemory(context_.device, sbtMemory_, 0, alignedTotalSize, 0, &data));
    std::memset(data, 0, alignedTotalSize);
    uint8_t* pData = static_cast<uint8_t*>(data);
    VkDeviceSize offset = 0;

    std::memcpy(pData + offset, shaderHandles_.data() + 0 * handleSize, handleSize);
    offset += alignedHandleSize;
    std::memcpy(pData + offset, shaderHandles_.data() + 1 * handleSize, handleSize);
    offset += alignedHandleSize;
    std::memcpy(pData + offset, shaderHandles_.data() + 2 * handleSize, handleSize);

    vkUnmapMemory(context_.device, sbtMemory_);

    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = sbtBuffer_;
    VkDeviceAddress sbtAddress = context_.vkGetBufferDeviceAddressKHR(context_.device, &addrInfo);

    sbt_.raygen = { sbtAddress,                 alignedHandleSize, raygenSize };
    sbt_.miss   = { sbtAddress + raygenSize,    alignedHandleSize, missSize   };
    sbt_.hit    = { sbtAddress + raygenSize + missSize, alignedHandleSize, hitSize };
    sbt_.callable = { 0, 0, 0 };

    LOG_INFO_CAT("Pipeline", "SBT created successfully:");
    LOG_INFO_CAT("Pipeline", "  Raygen: addr=0x{:x}, size={}, stride={}", sbt_.raygen.deviceAddress, sbt_.raygen.size, sbt_.raygen.stride);
    LOG_INFO_CAT("Pipeline", "  Miss:   addr=0x{:x}, size={}, stride={}", sbt_.miss.deviceAddress,   sbt_.miss.size,   sbt_.miss.stride);
    LOG_INFO_CAT("Pipeline", "  Hit:    addr=0x{:x}, size={}, stride={}", sbt_.hit.deviceAddress,    sbt_.hit.size,    sbt_.hit.stride);
}

// -----------------------------------------------------------------------------
// LOG FRAME TIME IF > 16.67ms (60 FPS)
// -----------------------------------------------------------------------------
void VulkanPipelineManager::logFrameTimeIfSlow(std::chrono::steady_clock::time_point start) {
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    const int64_t threshold = 16667;
    if (duration > threshold) {
        LOG_WARNING_CAT("Performance", "Frame took {} µs (>{} µs) — possible stall!", duration, threshold);
    }
}

// -----------------------------------------------------------------------------
// CREATE COMPUTE PIPELINE (For Denoiser with Push Constants)
// -----------------------------------------------------------------------------
void VulkanPipelineManager::createComputePipeline() {
    VkShaderModule computeModule = loadShader(context_.device, "compute_denoise");

    VkPipelineShaderStageCreateInfo stageInfo{};
    stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = computeModule;
    stageInfo.pName = "main";

    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(DenoisePushConstants);  // Now valid: defined in types.hpp

    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &computeDescriptorSetLayout_;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &pushRange;

    VkPipelineLayout layout;
    VK_CHECK(vkCreatePipelineLayout(context_.device, &layoutInfo, nullptr, &layout));
    computePipelineLayout_.reset(new VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>(
        context_.device, layout, vkDestroyPipelineLayout));

    VkComputePipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage = stageInfo;
    pipelineInfo.layout = layout;

    VkPipeline pipeline;
    VK_CHECK(vkCreateComputePipelines(context_.device, pipelineCache_, 1, &pipelineInfo, nullptr, &pipeline));
    computePipeline_.reset(new VulkanResource<VkPipeline, PFN_vkDestroyPipeline>(
        context_.device, pipeline, vkDestroyPipeline));

    vkDestroyShaderModule(context_.device, computeModule, nullptr);

    LOG_INFO_CAT("Pipeline", "Compute denoise pipeline created successfully.");
}

// -----------------------------------------------------------------------------
// CREATE GRAPHICS PIPELINE (STUBBED FOR RAY TRACING FOCUS)
// -----------------------------------------------------------------------------
void VulkanPipelineManager::createGraphicsPipeline(int width, int height) {
    // Stubbed out for ray tracing only - no vertex/fragment rasterization needed
    LOG_INFO_CAT("Pipeline", "Graphics pipeline skipped (ray tracing mode).");
}

// -----------------------------------------------------------------------------
// COMMAND RECORDING STUBS
// -----------------------------------------------------------------------------
void VulkanPipelineManager::recordGraphicsCommands(VkCommandBuffer cmd, VkFramebuffer fb, VkDescriptorSet ds,
                                                   uint32_t w, uint32_t h, VkImage denoiseImage) {
    // Stubbed for ray tracing focus
}
void VulkanPipelineManager::recordRayTracingCommands(VkCommandBuffer cmd, VkImage outputImage,
                                                    VkDescriptorSet descSet, uint32_t width, uint32_t height,
                                                    VkImage gDepth, VkImage gNormal) {
    // Implement RT command recording here if needed
}
void VulkanPipelineManager::recordComputeCommands(VkCommandBuffer cmd, VkImage outputImage,
                                                  VkDescriptorSet ds, uint32_t w, uint32_t h,
                                                  VkImage gDepth, VkImage gNormal, VkImage denoiseImage) {
    // Implement denoise dispatch here if needed
}
void VulkanPipelineManager::createAccelerationStructures(VkBuffer vertexBuffer, VkBuffer indexBuffer) {
    // Implement AS build if needed
}
void VulkanPipelineManager::updateRayTracingDescriptorSet(VkDescriptorSet descriptorSet, VkAccelerationStructureKHR tlasHandle) {
    // Implement descriptor updates if needed
}

} // namespace VulkanRTX