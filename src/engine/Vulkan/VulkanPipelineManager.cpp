// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// FULLY IMPLEMENTED - NO STUBS - CRANKED TO 11 - COMPETITION OBLITERATED
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/types.hpp"
#include "engine/logging.hpp"
#include <stdexcept>
#include <fstream>
#include <algorithm>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

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
      graphicsDescriptorSetLayout_(VK_NULL_HANDLE),
      sbtBuffer_(VK_NULL_HANDLE), sbtMemory_(VK_NULL_HANDLE),
      resourceManager_(),
      createAsFunc_(nullptr), destroyAsFunc_(nullptr),
      getRayTracingShaderGroupHandlesFunc_(nullptr),
      vkCreateDeferredOperationKHR_(nullptr), vkDeferredOperationJoinKHR_(nullptr),
      vkGetDeferredOperationResultKHR_(nullptr), vkDestroyDeferredOperationKHR_(nullptr)
{
    LOG_INFO_CAT("Pipeline", "Initializing VulkanPipelineManager - FULLY ARMED!");

    // Load RT extensions
    createAsFunc_ = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(
        vkGetDeviceProcAddr(context_.device, "vkCreateAccelerationStructureKHR"));
    destroyAsFunc_ = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(
        vkGetDeviceProcAddr(context_.device, "vkDestroyAccelerationStructureKHR"));
    getRayTracingShaderGroupHandlesFunc_ = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
        vkGetDeviceProcAddr(context_.device, "vkGetRayTracingShaderGroupHandlesKHR"));
    vkCreateDeferredOperationKHR_ = reinterpret_cast<PFN_vkCreateDeferredOperationKHR>(
        vkGetDeviceProcAddr(context_.device, "vkCreateDeferredOperationKHR"));
    vkDeferredOperationJoinKHR_ = reinterpret_cast<PFN_vkDeferredOperationJoinKHR>(
        vkGetDeviceProcAddr(context_.device, "vkDeferredOperationJoinKHR"));
    vkGetDeferredOperationResultKHR_ = reinterpret_cast<PFN_vkGetDeferredOperationResultKHR>(
        vkGetDeviceProcAddr(context_.device, "vkGetDeferredOperationResultKHR"));
    vkDestroyDeferredOperationKHR_ = reinterpret_cast<PFN_vkDestroyDeferredOperationKHR>(
        vkGetDeviceProcAddr(context_.device, "vkDestroyDeferredOperationKHR"));

    if (!createAsFunc_ || !destroyAsFunc_ || !getRayTracingShaderGroupHandlesFunc_ ||
        !vkCreateDeferredOperationKHR_ || !vkDeferredOperationJoinKHR_ || !vkGetDeferredOperationResultKHR_ || !vkDestroyDeferredOperationKHR_) {
        throw std::runtime_error("Failed to load required ray tracing function pointers");
    }

    // Give resource manager the device
    resourceManager_.setDevice(context_.device, context_.physicalDevice);

    shaderPaths_ = {
        {"raygen",      "assets/shaders/raytracing/raygen.spv"},
        {"miss",        "assets/shaders/raytracing/miss.spv"},
        {"closesthit",  "assets/shaders/raytracing/closesthit.spv"},
        {"compute_denoise", "assets/shaders/compute/denoise.spv"},
        {"tonemap_vert", "assets/shaders/graphics/tonemap_vert.spv"},
        {"tonemap_frag", "assets/shaders/graphics/tonemap_frag.spv"}
    };

    createPipelineCache();
    createRayTracingDescriptorSetLayout();
    createComputeDescriptorSetLayout();
    createGraphicsDescriptorSetLayout();
    createRenderPass();

    // Query RT props
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

    createRayTracingPipeline();
    createComputePipeline();
    createGraphicsPipeline(width, height);
    createShaderBindingTable();
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
    if (graphicsDescriptorSetLayout_) vkDestroyDescriptorSetLayout(context_.device, graphicsDescriptorSetLayout_, nullptr);
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

    // Match raygen.comp bindings exactly:
    // 0: TLAS (accelerationStructureEXT)
    // 1: outputImage (storage image)
    // 2: UBO
    // 3: materials (single SSBO, not array of 26)
    // 4: dimensionBuffer
    // 5: envMap
    // 6: (optional) extra sampler
    std::vector<VkDescriptorSetLayoutBinding> bindings(7);
    bindings[0] = {0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR};
    bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR};
    bindings[2] = {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR};
    
    // FIXED: Single material buffer (not 26 separate buffers)
    bindings[3] = {3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR};
    
    bindings[4] = {4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR};
    bindings[5] = {5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR};  // envMap
    bindings[6] = {6, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR};  // (optional)

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &rayTracingDescriptorSetLayout_));
    LOG_INFO_CAT("Pipeline", "Ray tracing descriptor set layout created (bindings 0-6).");

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
// CREATE COMPUTE DESCRIPTOR SET LAYOUT
// -----------------------------------------------------------------------------
void VulkanPipelineManager::createComputeDescriptorSetLayout() {
    if (computeDescriptorSetLayout_ != VK_NULL_HANDLE) return;

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT},  // Noisy input
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT},           // Denoised output
        {2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT}          // UBO (optional)
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &computeDescriptorSetLayout_));
    LOG_INFO_CAT("Pipeline", "Compute descriptor set layout created for denoiser.");
}

// -----------------------------------------------------------------------------
// CREATE GRAPHICS DESCRIPTOR SET LAYOUT
// -----------------------------------------------------------------------------
void VulkanPipelineManager::createGraphicsDescriptorSetLayout() {
    if (graphicsDescriptorSetLayout_ != VK_NULL_HANDLE) return;

    std::vector<VkDescriptorSetLayoutBinding> bindings = {
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT}  // Denoised image
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings = bindings.data();

    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &graphicsDescriptorSetLayout_));
    LOG_INFO_CAT("Pipeline", "Graphics descriptor set layout created for tonemap.");
}

// -----------------------------------------------------------------------------
// CREATE RENDER PASS
// -----------------------------------------------------------------------------
void VulkanPipelineManager::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = context_.swapchainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorRef{};
    colorRef.attachment = 0;
    colorRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    VK_CHECK(vkCreateRenderPass(context_.device, &renderPassInfo, nullptr, &renderPass_));
    LOG_INFO_CAT("Pipeline", "Render pass created for tonemapping.");
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

    LOG_INFO_CAT("Pipeline", "Creating Shader Binding Table (aligned)...");

    const uint32_t handleSize = context_.rtProperties.shaderGroupHandleSize;
    const uint32_t handleAlignment = context_.rtProperties.shaderGroupHandleAlignment;
    const uint32_t baseAlignment = context_.rtProperties.shaderGroupBaseAlignment;

    const uint32_t alignedHandleSize = (handleSize + handleAlignment - 1) & ~(handleAlignment - 1);

    const uint32_t groupCount = 3;
    shaderHandles_.resize(groupCount * handleSize);

    VK_CHECK(context_.vkGetRayTracingShaderGroupHandlesKHR(
        context_.device, rayTracingPipeline_->get(), 0, groupCount,
        shaderHandles_.size(), shaderHandles_.data()));

    const VkDeviceSize raygenSize = alignedHandleSize;
    const VkDeviceSize missSize   = alignedHandleSize;
    const VkDeviceSize hitSize    = alignedHandleSize;

    VkDeviceSize offset = 0;
    const VkDeviceSize raygenOffset = offset;
    offset += raygenSize;
    offset = (offset + baseAlignment - 1) & ~(baseAlignment - 1);

    const VkDeviceSize missOffset = offset;
    offset += missSize;
    offset = (offset + baseAlignment - 1) & ~(baseAlignment - 1);

    const VkDeviceSize hitOffset = offset;
    offset += hitSize;

    const VkDeviceSize totalSize = offset;
    const VkDeviceSize bufferSize = (totalSize + baseAlignment - 1) & ~(baseAlignment - 1);

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = bufferSize;
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
    VK_CHECK(vkMapMemory(context_.device, sbtMemory_, 0, bufferSize, 0, &data));
    uint8_t* pData = static_cast<uint8_t*>(data);

    std::memcpy(pData + raygenOffset, shaderHandles_.data() + 0 * handleSize, handleSize);
    std::memcpy(pData + missOffset,   shaderHandles_.data() + 1 * handleSize, handleSize);
    std::memcpy(pData + hitOffset,    shaderHandles_.data() + 2 * handleSize, handleSize);

    vkUnmapMemory(context_.device, sbtMemory_);

    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = sbtBuffer_;
    VkDeviceAddress baseAddr = context_.vkGetBufferDeviceAddressKHR(context_.device, &addrInfo);

    sbt_.raygen = { baseAddr + raygenOffset, alignedHandleSize, raygenSize };
    sbt_.miss   = { baseAddr + missOffset,   alignedHandleSize, missSize };
    sbt_.hit    = { baseAddr + hitOffset,    alignedHandleSize, hitSize };
    sbt_.callable = {0, 0, 0};

    context_.raygenSbtAddress = sbt_.raygen.deviceAddress;
    context_.missSbtAddress   = sbt_.miss.deviceAddress;
    context_.hitSbtAddress    = sbt_.hit.deviceAddress;
    context_.sbtRecordSize    = alignedHandleSize;

    LOG_INFO_CAT("Pipeline", "SBT Created (Aligned):");
    LOG_INFO_CAT("Pipeline", "  Raygen: addr=0x{:x}, stride={}, size={}", sbt_.raygen.deviceAddress, sbt_.raygen.stride, sbt_.raygen.size);
    LOG_INFO_CAT("Pipeline", "  Miss:   addr=0x{:x}, stride={}, size={}", sbt_.miss.deviceAddress,   sbt_.miss.stride,   sbt_.miss.size);
    LOG_INFO_CAT("Pipeline", "  Hit:    addr=0x{:x}, stride={}, size={}", sbt_.hit.deviceAddress,    sbt_.hit.stride,    sbt_.hit.size);
}

// -----------------------------------------------------------------------------
// LOG FRAME TIME
// -----------------------------------------------------------------------------
void VulkanPipelineManager::logFrameTimeIfSlow(std::chrono::steady_clock::time_point start) {
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    const int64_t threshold = 16667;
    if (duration > threshold) {
        LOG_WARNING_CAT("Performance", "Frame took {} microseconds (>{} microseconds) — possible stall!", duration, threshold);
    }
}

// -----------------------------------------------------------------------------
// CREATE COMPUTE PIPELINE
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
    pushRange.size = sizeof(DenoisePushConstants);

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
// CREATE GRAPHICS PIPELINE
// -----------------------------------------------------------------------------
void VulkanPipelineManager::createGraphicsPipeline(int width, int height) {
    VkShaderModule vertModule = loadShader(context_.device, "tonemap_vert");
    VkShaderModule fragModule = loadShader(context_.device, "tonemap_frag");

    std::vector<VkPipelineShaderStageCreateInfo> stages = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertModule, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragModule, "main", nullptr}
    };

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkViewport viewport = {0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f};
    VkRect2D scissor = {{0, 0}, {(uint32_t)width, (uint32_t)height}};

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &graphicsDescriptorSetLayout_;

    VkPipelineLayout layout;
    VK_CHECK(vkCreatePipelineLayout(context_.device, &pipelineLayoutInfo, nullptr, &layout));
    graphicsPipelineLayout_.reset(new VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>(
        context_.device, layout, vkDestroyPipelineLayout));

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = layout;
    pipelineInfo.renderPass = renderPass_;
    pipelineInfo.subpass = 0;

    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(context_.device, pipelineCache_, 1, &pipelineInfo, nullptr, &pipeline));
    graphicsPipeline_.reset(new VulkanResource<VkPipeline, PFN_vkDestroyPipeline>(
        context_.device, pipeline, vkDestroyPipeline));

    vkDestroyShaderModule(context_.device, vertModule, nullptr);
    vkDestroyShaderModule(context_.device, fragModule, nullptr);

    LOG_INFO_CAT("Pipeline", "Graphics tonemapping pipeline created successfully.");
}

// -----------------------------------------------------------------------------
// CREATE ACCELERATION STRUCTURES
// -----------------------------------------------------------------------------
void VulkanPipelineManager::createAccelerationStructures(VkBuffer vertexBuffer, VkBuffer indexBuffer) {
    if (blasHandle_ || tlasHandle_) {
        LOG_WARNING_CAT("Pipeline", "Acceleration structures already exist. Skipping.");
        return;
    }

    LOG_INFO_CAT("Pipeline", "Creating BLAS and TLAS...");

    // === BLAS ===
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
    triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = VulkanInitializer::getBufferDeviceAddress(context_, vertexBuffer);
    triangles.vertexStride = sizeof(glm::vec3);
    triangles.maxVertex = 0;
    triangles.indexType = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = VulkanInitializer::getBufferDeviceAddress(context_, indexBuffer);

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.geometry.triangles = triangles;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    const uint32_t primitiveCount = context_.indexCount / 3;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    context_.vkGetAccelerationStructureBuildSizesKHR(
        context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &primitiveCount, &sizeInfo);

    // Create BLAS buffer
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        sizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        context_.blasBuffer, context_.blasMemory,
        nullptr, resourceManager_);

    // Create BLAS
    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = context_.blasBuffer;
    createInfo.size = sizeInfo.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    VK_CHECK(createAsFunc_(context_.device, &createInfo, nullptr, &blasHandle_));

    // Build command
    VkAccelerationStructureBuildGeometryInfoKHR buildInfoFinal = buildInfo;
    buildInfoFinal.dstAccelerationStructure = blasHandle_;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{};
    rangeInfo.primitiveCount = primitiveCount;
    rangeInfo.primitiveOffset = 0;
    rangeInfo.firstVertex = 0;
    rangeInfo.transformOffset = 0;
    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &rangeInfo;

    VkCommandBuffer cmd = VulkanInitializer::beginSingleTimeCommands(context_);
    context_.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfoFinal, &pRange);
    VulkanInitializer::endSingleTimeCommands(context_, cmd);

    // === TLAS ===
    VkTransformMatrixKHR transformMatrix = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f
    };

    VkAccelerationStructureInstanceKHR instance{};
    instance.transform = transformMatrix;
    instance.instanceCustomIndex = 0;
    instance.mask = 0xFF;
    instance.instanceShaderBindingTableRecordOffset = 0;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instance.accelerationStructureReference = VulkanInitializer::getAccelerationStructureDeviceAddress(context_, blasHandle_);

    // Staging buffer
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        stagingBuffer, stagingMemory, nullptr, resourceManager_);

    void* data;
    vkMapMemory(context_.device, stagingMemory, 0, sizeof(VkAccelerationStructureInstanceKHR), 0, &data);
    memcpy(data, &instance, sizeof(VkAccelerationStructureInstanceKHR));
    vkUnmapMemory(context_.device, stagingMemory);

    // Instance buffer
    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        sizeof(VkAccelerationStructureInstanceKHR),
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        context_.instanceBuffer, context_.instanceMemory, nullptr, resourceManager_);

    VulkanInitializer::copyBuffer(
        context_.device, context_.commandPool, context_.graphicsQueue,
        stagingBuffer, context_.instanceBuffer, sizeof(VkAccelerationStructureInstanceKHR));

    vkDestroyBuffer(context_.device, stagingBuffer, nullptr);
    vkFreeMemory(context_.device, stagingMemory, nullptr);

    // TLAS geometry
    VkAccelerationStructureGeometryInstancesDataKHR instancesData{};
    instancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    instancesData.data.deviceAddress = VulkanInitializer::getBufferDeviceAddress(context_, context_.instanceBuffer);

    VkAccelerationStructureGeometryKHR tlasGeometry{};
    tlasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    tlasGeometry.geometry.instances = instancesData;

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfo{};
    tlasBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    tlasBuildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    tlasBuildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
    tlasBuildInfo.geometryCount = 1;
    tlasBuildInfo.pGeometries = &tlasGeometry;

    const uint32_t instanceCount = 1;
    VkAccelerationStructureBuildSizesInfoKHR tlasSizeInfo{};
    tlasSizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    context_.vkGetAccelerationStructureBuildSizesKHR(
        context_.device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &tlasBuildInfo, &instanceCount, &tlasSizeInfo);

    VulkanInitializer::createBuffer(
        context_.device, context_.physicalDevice,
        tlasSizeInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        context_.tlasBuffer, context_.tlasMemory, nullptr, resourceManager_);

    VkAccelerationStructureCreateInfoKHR tlasCreateInfo{};
    tlasCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    tlasCreateInfo.buffer = context_.tlasBuffer;
    tlasCreateInfo.size = tlasSizeInfo.accelerationStructureSize;
    tlasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    VK_CHECK(createAsFunc_(context_.device, &tlasCreateInfo, nullptr, &tlasHandle_));

    VkAccelerationStructureBuildGeometryInfoKHR tlasBuildInfoFinal = tlasBuildInfo;
    tlasBuildInfoFinal.dstAccelerationStructure = tlasHandle_;

    VkAccelerationStructureBuildRangeInfoKHR tlasRangeInfo{};
    tlasRangeInfo.primitiveCount = instanceCount;
    const VkAccelerationStructureBuildRangeInfoKHR* pTlasRange = &tlasRangeInfo;

    VkCommandBuffer cmd2 = VulkanInitializer::beginSingleTimeCommands(context_);
    context_.vkCmdBuildAccelerationStructuresKHR(cmd2, 1, &tlasBuildInfoFinal, &pTlasRange);
    VulkanInitializer::endSingleTimeCommands(context_, cmd2);

    LOG_INFO_CAT("Pipeline", "BLAS and TLAS created successfully.");
}

// -----------------------------------------------------------------------------
// UPDATE RAY TRACING DESCRIPTOR SET
// -----------------------------------------------------------------------------
void VulkanPipelineManager::updateRayTracingDescriptorSet(VkDescriptorSet descriptorSet, VkAccelerationStructureKHR tlasHandle) {
    VkWriteDescriptorSetAccelerationStructureKHR asWrite{};
    asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    asWrite.accelerationStructureCount = 1;
    asWrite.pAccelerationStructures = &tlasHandle;

    VkDescriptorImageInfo outputImageInfo{};
    outputImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    outputImageInfo.imageView = context_.rtOutputImageView;

    VkDescriptorBufferInfo uniformBufferInfo{};
    uniformBufferInfo.buffer = context_.uniformBuffers[0];
    uniformBufferInfo.offset = 0;
    uniformBufferInfo.range = sizeof(UniformBufferObject);

    VkDescriptorImageInfo envMapInfo{};
    envMapInfo.sampler = context_.envMapSampler;
    envMapInfo.imageView = context_.envMapImageView;
    envMapInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    std::vector<VkWriteDescriptorSet> writes = {
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, &asWrite, descriptorSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, nullptr, nullptr, nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 1, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, &outputImageInfo, nullptr, nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 2, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, nullptr, &uniformBufferInfo, nullptr},
        {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr, descriptorSet, 5, 0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &envMapInfo, nullptr, nullptr}
    };

    vkUpdateDescriptorSets(context_.device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    LOG_INFO_CAT("Pipeline", "Ray tracing descriptor set updated: TLAS, output image, UBO, envMap (binding 5).");
}

// -----------------------------------------------------------------------------
// RECORD RAY TRACING COMMANDS
// -----------------------------------------------------------------------------
void VulkanPipelineManager::recordRayTracingCommands(VkCommandBuffer cmd, VkImage outputImage,
                                                    VkDescriptorSet descSet, uint32_t width, uint32_t height,
                                                    VkImage gDepth, VkImage gNormal) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = outputImage;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rayTracingPipeline_->get());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, getRayTracingPipelineLayout(), 0, 1, &descSet, 0, nullptr);

    const VkStridedDeviceAddressRegionKHR* regions[4] = {
        &sbt_.raygen, &sbt_.miss, &sbt_.hit, &sbt_.callable
    };

    context_.vkCmdTraceRaysKHR(cmd, regions[0], regions[1], regions[2], regions[3], width, height, 1);

    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// -----------------------------------------------------------------------------
// RECORD COMPUTE COMMANDS
// -----------------------------------------------------------------------------
void VulkanPipelineManager::recordComputeCommands(VkCommandBuffer cmd, VkImage outputImage,
                                                  VkDescriptorSet ds, uint32_t w, uint32_t h,
                                                  VkImage gDepth, VkImage gNormal, VkImage denoiseImage) {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    // Transition noisy RT output to shader read
    barrier.image = context_.rtOutputImage;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Transition denoise output to general
    barrier.image = denoiseImage;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline_->get());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipelineLayout_->get(), 0, 1, &ds, 0, nullptr);

    DenoisePushConstants push{};
    push.width = w;
    push.height = h;
    push.kernelRadius = 3;  // Increased from 1 to reduce banding
    vkCmdPushConstants(cmd, computePipelineLayout_->get(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);

    vkCmdDispatch(cmd, (w + 15) / 16, (h + 15) / 16, 1);

    barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// -----------------------------------------------------------------------------
// RECORD GRAPHICS COMMANDS
// -----------------------------------------------------------------------------
void VulkanPipelineManager::recordGraphicsCommands(VkCommandBuffer cmd, VkFramebuffer fb, VkDescriptorSet ds,
                                                   uint32_t w, uint32_t h, VkImage denoiseImage) {
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = renderPass_;
    renderPassInfo.framebuffer = fb;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {w, h};

    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;

    vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline_->get());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipelineLayout_->get(), 0, 1, &ds, 0, nullptr);

    // Full-screen triangle
    vkCmdDraw(cmd, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmd);
}

} // namespace VulkanRTX