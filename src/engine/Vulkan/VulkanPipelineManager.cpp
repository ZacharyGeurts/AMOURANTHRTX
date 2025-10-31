// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// FULLY IMPLEMENTED - TEMPORAL ACCUMULATION - SVGF DENOISING - DLSS-READY - NO MERCY
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
// EXTENDED PIPELINE MANAGER WITH TEMPORAL + SVGF
// -----------------------------------------------------------------------------
VulkanPipelineManager::VulkanPipelineManager(Vulkan::Context& context, int width, int height)
    : context_(context), width_(width), height_(height),
      rayTracingPipeline_(nullptr), rayTracingPipelineLayout_(nullptr),
      computePipeline_(nullptr), computePipelineLayout_(nullptr),
      graphicsPipeline_(nullptr), graphicsPipelineLayout_(nullptr),
      renderPass_(VK_NULL_HANDLE), pipelineCache_(VK_NULL_HANDLE),
      tlasHandle_(VK_NULL_HANDLE), blasHandle_(VK_NULL_HANDLE),
      computeDescriptorSetLayout_(VK_NULL_HANDLE), rayTracingDescriptorSetLayout_(VK_NULL_HANDLE),
      graphicsDescriptorSetLayout_(VK_NULL_HANDLE),
      sbtBuffer_(VK_NULL_HANDLE), sbtMemory_(VK_NULL_HANDLE),
      createAsFunc_(nullptr), destroyAsFunc_(nullptr),
      getRayTracingShaderGroupHandlesFunc_(nullptr),
      temporalAccumPipeline_(nullptr), temporalAccumPipelineLayout_(nullptr),
      svgfVariancePipeline_(nullptr), svgfVariancePipelineLayout_(nullptr),
      svgfFilterPipeline_(nullptr), svgfFilterPipelineLayout_(nullptr),
      temporalDescriptorSetLayout_(VK_NULL_HANDLE),
      varianceDescriptorSetLayout_(VK_NULL_HANDLE),
      filterDescriptorSetLayout_(VK_NULL_HANDLE),
      prevFrameImage_(VK_NULL_HANDLE), prevFrameImageMemory_(VK_NULL_HANDLE),
      prevFrameImageView_(VK_NULL_HANDLE),
      prevNormalImage_(VK_NULL_HANDLE), prevNormalImageMemory_(VK_NULL_HANDLE),
      prevNormalImageView_(VK_NULL_HANDLE),
      varianceImage_(VK_NULL_HANDLE), varianceImageMemory_(VK_NULL_HANDLE),
      varianceImageView_(VK_NULL_HANDLE),
      filteredImage_(VK_NULL_HANDLE), filteredImageMemory_(VK_NULL_HANDLE),
      filteredImageView_(VK_NULL_HANDLE),
      frameIndex_(0)
{
    LOG_INFO_CAT("Pipeline", "Initializing VulkanPipelineManager v11.0 - TEMPORAL + SVGF ARMED!");

    // Load RT extensions
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
        {"raygen",           "assets/shaders/raytracing/raygen.spv"},
        {"miss",             "assets/shaders/raytracing/miss.spv"},
        {"closesthit",       "assets/shaders/raytracing/closesthit.spv"},
        {"temporal_accum",   "assets/shaders/compute/temporal_accum.spv"},
        {"svgf_variance",    "assets/shaders/compute/svgf_variance.spv"},
        {"svgf_filter",      "assets/shaders/compute/svgf_filter.spv"},
        {"tonemap_vert",     "assets/shaders/graphics/tonemap_vert.spv"},
        {"tonemap_frag",     "assets/shaders/graphics/tonemap_frag.spv"}
    };

    createPipelineCache();
    createRayTracingDescriptorSetLayout();
    createTemporalDescriptorSetLayout();
    createVarianceDescriptorSetLayout();
    createFilterDescriptorSetLayout();
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

    LOG_INFO_CAT("Pipeline", "RT Properties: handleSize={}, baseAlign={}, handleAlign={}",
                 rtProps.shaderGroupHandleSize, rtProps.shaderGroupBaseAlignment, rtProps.shaderGroupHandleAlignment);

    // Create pipelines
    createRayTracingPipeline();
    createTemporalAccumulationPipeline();
    createSVGFVariancePipeline();
    createSVGFFilterPipeline();
    createGraphicsPipeline(width, height);
    createShaderBindingTable();

    // Create persistent G-buffers for temporal reuse
    createPersistentBuffers(width, height);

    LOG_INFO_CAT("Pipeline", "PipelineManager v11.0 initialized - SVGF + Temporal READY.");
}

// -----------------------------------------------------------------------------
// DESTRUCTOR
// -----------------------------------------------------------------------------
VulkanPipelineManager::~VulkanPipelineManager() {
    LOG_INFO_CAT("Pipeline", "Destroying VulkanPipelineManager v11.0...");

    if (sbtBuffer_) vkDestroyBuffer(context_.device, sbtBuffer_, nullptr);
    if (sbtMemory_) vkFreeMemory(context_.device, sbtMemory_, nullptr);

    if (tlasHandle_ && destroyAsFunc_) destroyAsFunc_(context_.device, tlasHandle_, nullptr);
    if (blasHandle_ && destroyAsFunc_) destroyAsFunc_(context_.device, blasHandle_, nullptr);

    if (pipelineCache_) vkDestroyPipelineCache(context_.device, pipelineCache_, nullptr);
    if (renderPass_) vkDestroyRenderPass(context_.device, renderPass_, nullptr);

    // Destroy persistent images
    if (prevFrameImageView_) vkDestroyImageView(context_.device, prevFrameImageView_, nullptr);
    if (prevFrameImage_) vkDestroyImage(context_.device, prevFrameImage_, nullptr);
    if (prevFrameImageMemory_) vkFreeMemory(context_.device, prevFrameImageMemory_, nullptr);

    if (prevNormalImageView_) vkDestroyImageView(context_.device, prevNormalImageView_, nullptr);
    if (prevNormalImage_) vkDestroyImage(context_.device, prevNormalImage_, nullptr);
    if (prevNormalImageMemory_) vkFreeMemory(context_.device, prevNormalImageMemory_, nullptr);

    if (varianceImageView_) vkDestroyImageView(context_.device, varianceImageView_, nullptr);
    if (varianceImage_) vkDestroyImage(context_.device, varianceImage_, nullptr);
    if (varianceImageMemory_) vkFreeMemory(context_.device, varianceImageMemory_, nullptr);

    if (filteredImageView_) vkDestroyImageView(context_.device, filteredImageView_, nullptr);
    if (filteredImage_) vkDestroyImage(context_.device, filteredImage_, nullptr);
    if (filteredImageMemory_) vkFreeMemory(context_.device, filteredImageMemory_, nullptr);

    // Destroy descriptor set layouts
    if (rayTracingDescriptorSetLayout_) vkDestroyDescriptorSetLayout(context_.device, rayTracingDescriptorSetLayout_, nullptr);
    if (temporalDescriptorSetLayout_) vkDestroyDescriptorSetLayout(context_.device, temporalDescriptorSetLayout_, nullptr);
    if (varianceDescriptorSetLayout_) vkDestroyDescriptorSetLayout(context_.device, varianceDescriptorSetLayout_, nullptr);
    if (filterDescriptorSetLayout_) vkDestroyDescriptorSetLayout(context_.device, filterDescriptorSetLayout_, nullptr);
    if (graphicsDescriptorSetLayout_) vkDestroyDescriptorSetLayout(context_.device, graphicsDescriptorSetLayout_, nullptr);
}

// -----------------------------------------------------------------------------
// CREATE PERSISTENT BUFFERS (Prev Frame, Variance, Filtered)
// -----------------------------------------------------------------------------
void VulkanPipelineManager::createPersistentBuffers(int w, int h) {
    auto createStorageImage = [&](VkImage& img, VkDeviceMemory& mem, VkImageView& view, VkFormat format) {
        VkImageCreateInfo imgInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = { (uint32_t)w, (uint32_t)h, 1 },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        VK_CHECK(vkCreateImage(context_.device, &imgInfo, nullptr, &img));

        VkMemoryRequirements reqs;
        vkGetImageMemoryRequirements(context_.device, img, &reqs);
        VkMemoryAllocateInfo alloc = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = reqs.size,
            .memoryTypeIndex = VulkanInitializer::findMemoryType(context_.physicalDevice, reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };
        VK_CHECK(vkAllocateMemory(context_.device, &alloc, nullptr, &mem));
        VK_CHECK(vkBindImageMemory(context_.device, img, mem, 0));

        VkImageViewCreateInfo viewInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = img,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = format,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
        VK_CHECK(vkCreateImageView(context_.device, &viewInfo, nullptr, &view));
    };

    createStorageImage(prevFrameImage_, prevFrameImageMemory_, prevFrameImageView_, VK_FORMAT_R32G32B32A32_SFLOAT);
    createStorageImage(prevNormalImage_, prevNormalImageMemory_, prevNormalImageView_, VK_FORMAT_R16G16B16A16_SFLOAT);
    createStorageImage(varianceImage_, varianceImageMemory_, varianceImageView_, VK_FORMAT_R32_SFLOAT);
    createStorageImage(filteredImage_, filteredImageMemory_, filteredImageView_, VK_FORMAT_R32G32B32A32_SFLOAT);

    LOG_INFO_CAT("Pipeline", "Persistent G-buffers created: {}x{}", w, h);
}

// -----------------------------------------------------------------------------
// TEMPORAL ACCUMULATION PIPELINE
// -----------------------------------------------------------------------------
void VulkanPipelineManager::createTemporalAccumulationPipeline() {
    VkShaderModule module = loadShader(context_.device, "temporal_accum");

    VkPipelineShaderStageCreateInfo stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = module,
        .pName = "main"
    };

    VkPushConstantRange push = { VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(TemporalPushConstants) };

    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}, // currColor
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}, // prevColor
        {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}, // currNormal
        {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}, // prevNormal
        {4, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}, // output
        {5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT}
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 6,
        .pBindings = bindings
    };
    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &temporalDescriptorSetLayout_));

    VkPipelineLayoutCreateInfo plInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &temporalDescriptorSetLayout_,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push
    };
    VkPipelineLayout layout;
    VK_CHECK(vkCreatePipelineLayout(context_.device, &plInfo, nullptr, &layout));
    temporalAccumPipelineLayout_.reset(new VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>(
        context_.device, layout, vkDestroyPipelineLayout));

    VkComputePipelineCreateInfo cpInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage,
        .layout = layout
    };
    VkPipeline pipeline;
    VK_CHECK(vkCreateComputePipelines(context_.device, pipelineCache_, 1, &cpInfo, nullptr, &pipeline));
    temporalAccumPipeline_.reset(new VulkanResource<VkPipeline, PFN_vkDestroyPipeline>(
        context_.device, pipeline, vkDestroyPipeline));

    vkDestroyShaderModule(context_.device, module, nullptr);
    LOG_INFO_CAT("Pipeline", "Temporal Accumulation Pipeline created.");
}

// -----------------------------------------------------------------------------
// SVGF VARIANCE PIPELINE
// -----------------------------------------------------------------------------
void VulkanPipelineManager::createSVGFVariancePipeline() {
    VkShaderModule module = loadShader(context_.device, "svgf_variance");

    VkPipelineShaderStageCreateInfo stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = module,
        .pName = "main"
    };

    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}, // input
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}, // variance
        {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}  // moments
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 3,
        .pBindings = bindings
    };
    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &varianceDescriptorSetLayout_));

    VkPipelineLayoutCreateInfo plInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &varianceDescriptorSetLayout_
    };
    VkPipelineLayout layout;
    VK_CHECK(vkCreatePipelineLayout(context_.device, &plInfo, nullptr, &layout));
    svgfVariancePipelineLayout_.reset(new VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>(
        context_.device, layout, vkDestroyPipelineLayout));

    VkComputePipelineCreateInfo cpInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage,
        .layout = layout
    };
    VkPipeline pipeline;
    VK_CHECK(vkCreateComputePipelines(context_.device, pipelineCache_, 1, &cpInfo, nullptr, &pipeline));
    svgfVariancePipeline_.reset(new VulkanResource<VkPipeline, PFN_vkDestroyPipeline>(
        context_.device, pipeline, vkDestroyPipeline));

    vkDestroyShaderModule(context_.device, module, nullptr);
    LOG_INFO_CAT("Pipeline", "SVGF Variance Pipeline created.");
}

// -----------------------------------------------------------------------------
// SVGF FILTER PIPELINE
// -----------------------------------------------------------------------------
void VulkanPipelineManager::createSVGFFilterPipeline() {
    VkShaderModule module = loadShader(context_.device, "svgf_filter");

    VkPipelineShaderStageCreateInfo stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = module,
        .pName = "main"
    };

    VkDescriptorSetLayoutBinding bindings[] = {
        {0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}, // input
        {1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}, // variance
        {2, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}, // normal
        {3, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_COMPUTE_BIT}  // output
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 4,
        .pBindings = bindings
    };
    VK_CHECK(vkCreateDescriptorSetLayout(context_.device, &layoutInfo, nullptr, &filterDescriptorSetLayout_));

    VkPipelineLayoutCreateInfo plInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &filterDescriptorSetLayout_
    };
    VkPipelineLayout layout;
    VK_CHECK(vkCreatePipelineLayout(context_.device, &plInfo, nullptr, &layout));
    svgfFilterPipelineLayout_.reset(new VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>(
        context_.device, layout, vkDestroyPipelineLayout));

    VkComputePipelineCreateInfo cpInfo = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = stage,
        .layout = layout
    };
    VkPipeline pipeline;
    VK_CHECK(vkCreateComputePipelines(context_.device, pipelineCache_, 1, &cpInfo, nullptr, &pipeline));
    svgfFilterPipeline_.reset(new VulkanResource<VkPipeline, PFN_vkDestroyPipeline>(
        context_.device, pipeline, vkDestroyPipeline));

    vkDestroyShaderModule(context_.device, module, nullptr);
    LOG_INFO_CAT("Pipeline", "SVGF Filter Pipeline created.");
}

// -----------------------------------------------------------------------------
// RECORD FULL DENOISING PASS (Temporal → Variance → Filter)
// -----------------------------------------------------------------------------
void VulkanPipelineManager::recordDenoisingPass(
    VkCommandBuffer cmd,
    VkImage currColor, VkImage currNormal,
    VkDescriptorSet temporalSet, VkDescriptorSet varianceSet, VkDescriptorSet filterSet,
    uint32_t w, uint32_t h)
{
    // 1. Temporal Accumulation
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, temporalAccumPipeline_->get());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, temporalAccumPipelineLayout_->get(), 0, 1, &temporalSet, 0, nullptr);

    TemporalPushConstants tpc = { .frameIndex = frameIndex_++, .alpha = 0.2f, .momentsAlpha = 0.2f };
    vkCmdPushConstants(cmd, temporalAccumPipelineLayout_->get(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(tpc), &tpc);
    vkCmdDispatch(cmd, (w + 15)/16, (h + 15)/16, 1);

    // Barrier: wait for temporal write
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image = prevFrameImage_,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    // 2. Variance Estimation
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, svgfVariancePipeline_->get());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, svgfVariancePipelineLayout_->get(), 0, 1, &varianceSet, 0, nullptr);
    vkCmdDispatch(cmd, (w + 15)/16, (h + 15)/16, 1);

    // 3. A-Trous Filter
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, svgfFilterPipeline_->get());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, svgfFilterPipelineLayout_->get(), 0, 1, &filterSet, 0, nullptr);
    vkCmdDispatch(cmd, (w + 15)/16, (h + 15)/16, 1);

    LOG_INFO_CAT("Pipeline", "SVGF Denoising Pass recorded (Frame {})", frameIndex_);
}

// -----------------------------------------------------------------------------
// GETTERS
// -----------------------------------------------------------------------------
VkDescriptorSetLayout VulkanPipelineManager::getTemporalDescriptorSetLayout() const { return temporalDescriptorSetLayout_; }
VkDescriptorSetLayout VulkanPipelineManager::getVarianceDescriptorSetLayout() const { return varianceDescriptorSetLayout_; }
VkDescriptorSetLayout VulkanPipelineManager::getFilterDescriptorSetLayout() const { return filterDescriptorSetLayout_; }

VkImageView VulkanPipelineManager::getPrevFrameImageView() const { return prevFrameImageView_; }
VkImageView VulkanPipelineManager::getPrevNormalImageView() const { return prevNormalImageView_; }
VkImageView VulkanPipelineManager::getVarianceImageView() const { return varianceImageView_; }
VkImageView VulkanPipelineManager::getFilteredImageView() const { return filteredImageView_; }

} // namespace VulkanRTX