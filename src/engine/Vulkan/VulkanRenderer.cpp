// src/engine/Vulkan/VulkanRenderer.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// FINAL: SBT via getSBT().raygen — NO getRaygenSBT()
//        Accumulation reset + proper layout transitions
//        Command buffer reset + begin/end
//        Null handle safety
//        VulkanRTX created in takeOwnership()
//        All .get() on Dispose handles
//        traceRays() with correct order: cmd, raygen, miss, hit, callable, w, h, d
//        SBT address validation + logging
//        COMPILER FIXED: Uses getSBT() → raygen/miss/hit/callable
//        ENHANCED: Detailed FPS metrics (CPU/GPU ms, accum, res, frame) — ROAR!

#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/logging.hpp"
#include "stb/stb_image.h"
#include <tinyobjloader/tiny_obj_loader.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <cstring>
#include <chrono>
#include <array>
#include <cmath>
#include <algorithm>
#include <format>  // For std::format in logs

using namespace Logging::Color;
using namespace Dispose;

namespace VulkanRTX {

// Helper: Find memory type
static uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t filter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        if ((filter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    throw std::runtime_error("No suitable memory type");
}

// =============================================================================
// CONSTRUCTOR — NO RAW POINTERS, NO VulkanRTX
// =============================================================================
VulkanRenderer::VulkanRenderer(int width, int height, SDL_Window* window,
                               const std::vector<std::string>& shaderPaths,
                               std::shared_ptr<Vulkan::Context> context)
    : window_(window),
      context_(std::move(context)),
      width_(width),
      height_(height),
      lastFPSTime_(std::chrono::steady_clock::now()),
      currentFrame_(0),
      currentRTIndex_(0),
      currentAccumIndex_(0),
      frameNumber_(0),
      resetAccumulation_(true),
      prevViewProj_(glm::mat4(1.0f)),
      currentMode_(1),
      framesThisSecond_(0),
      timestampPeriod_(0.0),
      avgFrameTimeMs_(0.0f),
      minFrameTimeMs_(std::numeric_limits<float>::max()),
      maxFrameTimeMs_(0.0f),
      avgGpuTimeMs_(0.0f),
      minGpuTimeMs_(std::numeric_limits<float>::max()),
      maxGpuTimeMs_(0.0f)
{
    LOG_INFO_CAT("RENDERER", "{}Init [{}x{}]{}", OCEAN_TEAL, width, height, RESET);

    // Query timestamp period for GPU timing
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(context_->physicalDevice, &props);
    timestampPeriod_ = props.limits.timestampPeriod;  // ns per tick

    // Default cube
    vertices_ = {
        {-1,-1,-1}, {1,-1,-1}, {1,1,-1}, {-1,1,-1},
        {-1,-1,1}, {1,-1,1}, {1,1,1}, {-1,1,1}
    };
    indices_ = {
        0,1,3, 3,1,2, 1,5,2, 2,5,6, 5,4,6, 6,4,7,
        4,0,7, 7,0,3, 3,2,7, 7,2,6, 4,5,0, 0,5,1
    };

    // Sync objects
    VkSemaphoreCreateInfo semInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vkCreateSemaphore(context_->device, &semInfo, nullptr, &imageAvailableSemaphores_[i]), "Sem");
        VK_CHECK(vkCreateSemaphore(context_->device, &semInfo, nullptr, &renderFinishedSemaphores_[i]), "Sem");
        VK_CHECK(vkCreateFence(context_->device, &fenceInfo, nullptr, &inFlightFences_[i]), "Fence");
    }

    // === SWAPCHAIN IS OWNED BY CONTEXT — COPY STATE ===
    swapchain_           = context_->swapchain;
    swapchainImages_     = context_->swapchainImages;
    swapchainImageViews_ = context_->swapchainImageViews;
    swapchainExtent_     = context_->swapchainExtent;
    swapchainImageFormat_= context_->swapchainImageFormat;

    if (swapchainImages_.empty()) {
        LOG_ERROR_CAT("RENDERER", "Swapchain has no images – ensure context->createSwapchain() was called before renderer");
        throw std::runtime_error("Empty swapchain images");
    }
    LOG_INFO_CAT("RENDERER", "Swapchain ready: {} images, {}x{}", swapchainImages_.size(), swapchainExtent_.width, swapchainExtent_.height);

    // Query pools for GPU timestamps
    for (auto& pool : queryPools_) {
        VkQueryPoolCreateInfo qi{.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                                 .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = 2};
        VK_CHECK(vkCreateQueryPool(context_->device, &qi, nullptr, &pool), "Query pool");
    }

    // Descriptor pool
    std::array<VkDescriptorPoolSize, 4> poolSizes = {{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT * 2},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT * 4},
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1}
    }};
    VkDescriptorPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                        .maxSets = MAX_FRAMES_IN_FLIGHT + 2,
                                        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                                        .pPoolSizes = poolSizes.data()};
    VkDescriptorPool rawPool;
    VK_CHECK(vkCreateDescriptorPool(context_->device, &poolInfo, nullptr, &rawPool), "Desc pool");
    descriptorPool_ = makeHandle(context_->device, rawPool, "Renderer Pool");

    LOG_INFO_CAT("RENDERER", "{}VulkanRenderer initialized — awaiting takeOwnership()...{}", OCEAN_TEAL, RESET);
}

// =============================================================================
// DESTRUCTOR
// =============================================================================
VulkanRenderer::~VulkanRenderer() {
    cleanup();
}

// =============================================================================
// TAKE OWNERSHIP — CREATE VulkanRTX HERE
// =============================================================================
void VulkanRenderer::takeOwnership(std::unique_ptr<VulkanPipelineManager> pm,
                                   std::unique_ptr<VulkanBufferManager> bm)
{
    LOG_INFO_CAT("RENDERER", "{}takeOwnership() — acquiring PipelineManager & BufferManager{}", ARCTIC_CYAN, RESET);
    pipelineManager_ = std::move(pm);
    bufferManager_   = std::move(bm);

    // NOW SAFE: create VulkanRTX
    rtx_ = std::make_unique<VulkanRTX>(context_, width_, height_, pipelineManager_.get());
    rtxDescriptorSet_ = rtx_->getDescriptorSet();
    if (!rtxDescriptorSet_) throw std::runtime_error("RTX descriptor set missing");

    createRTOutputImages();
    createAccumulationImages();
    createEnvironmentMap();
    createComputeDescriptorSets();
    createFramebuffers();

    // Upload mesh AFTER buffer manager is owned
    bufferManager_->uploadMesh(vertices_.data(), vertices_.size(), indices_.data(), indices_.size());

    // CRITICAL: Build initial AS after mesh upload
    rtx_->updateRTX(context_->physicalDevice, context_->commandPool, context_->graphicsQueue,
                    bufferManager_->getGeometries(), bufferManager_->getDimensionStates());

    constexpr VkDeviceSize kMatSize = 256 * sizeof(MaterialData);
    constexpr VkDeviceSize kDimSize = 1024 * sizeof(float);
    initializeAllBufferData(MAX_FRAMES_IN_FLIGHT, kMatSize, kDimSize);

    pipelineManager_->createRayTracingPipeline();
    rtPipeline_ = pipelineManager_->getRayTracingPipeline();
    rtPipelineLayout_ = pipelineManager_->getRayTracingPipelineLayout();
    if (!rtPipeline_) throw std::runtime_error("RT pipeline missing");

    rtx_->setRayTracingPipeline(rtPipeline_, rtPipelineLayout_);
    rtx_->createShaderBindingTable(context_->physicalDevice);

    createCommandBuffers();
    updateRTDescriptors();

    LOG_INFO_CAT("RENDERER", "{}takeOwnership() complete. VulkanRTX READY.{}", EMERALD_GREEN, RESET);
}

// =============================================================================
// RENDER FRAME — FIXED: SBT via getSBT(), no dangling ptrs
// =============================================================================
void VulkanRenderer::renderFrame(const Camera& camera) {
    auto frameStart = std::chrono::high_resolution_clock::now();  // CPU start

    auto now = std::chrono::steady_clock::now();
    bool updateMetrics = (std::chrono::duration_cast<std::chrono::seconds>(now - lastFPSTime_).count() >= 1);

    vkWaitForFences(context_->device, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    auto res = vkAcquireNextImageKHR(context_->device, swapchain_, UINT64_MAX,
                                     imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        handleResize(width_, height_);
        return;
    } else if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "Failed to acquire swapchain image: {}", res);
        return;
    }
    if (imageIndex >= static_cast<uint32_t>(commandBuffers_.size())) {
        LOG_ERROR_CAT("RENDERER", "Invalid imageIndex {} (max {})", imageIndex, commandBuffers_.size() - 1);
        return;
    }
    vkResetFences(context_->device, 1, &inFlightFences_[currentFrame_]);

    // Accumulation reset logic
    glm::mat4 currVP = camera.getProjectionMatrix() * camera.getViewMatrix();
    float diff = 0.0f;
    for (int i = 0; i < 16; ++i)
        diff = std::max(diff, std::abs(currVP[i/4][i%4] - prevViewProj_[i/4][i%4]));
    if (diff > 1e-4f || resetAccumulation_) {
        resetAccumulation_ = true;
        frameNumber_ = 0;
    } else {
        resetAccumulation_ = false;
        frameNumber_++;
    }
    prevViewProj_ = currVP;

    updateUniformBuffer(currentFrame_, camera);

    VkCommandBuffer cmd = commandBuffers_[imageIndex];
    if (cmd == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RENDERER", "Null command buffer for imageIndex {}", imageIndex);
        return;
    }

    // Reset and begin command buffer
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "Begin cmd");

    // GPU timestamp start
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPools_[currentFrame_], 0);

    // Transition RT output image
    transitionImageLayout(cmd, rtOutputImages_[currentRTIndex_].get(),
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0, VK_ACCESS_SHADER_WRITE_BIT);

    // Transition accumulation image — only UNDEFINED on first frame
    VkImageLayout accumOldLayout = resetAccumulation_ ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_GENERAL;
    transitionImageLayout(cmd, accumImages_[currentAccumIndex_].get(),
        accumOldLayout, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

    updateRTDescriptors();

    // Bind pipeline and descriptor set
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                            rtPipelineLayout_, 0, 1, &rtxDescriptorSet_, 0, nullptr);

    // FIX: Use getSBT() → direct access to raygen/miss/hit/callable
    const auto& sbt = rtx_->getSBT();
    const VkStridedDeviceAddressRegionKHR* rgen = &sbt.raygen;
    const VkStridedDeviceAddressRegionKHR* miss = &sbt.miss;
    const VkStridedDeviceAddressRegionKHR* hit  = &sbt.hit;
    const VkStridedDeviceAddressRegionKHR* call = &sbt.callable;

    // Validate SBT
    if (rgen->deviceAddress == 0 || miss->deviceAddress == 0 || hit->deviceAddress == 0) {
        LOG_ERROR_CAT("RENDERER", "SBT region invalid (deviceAddress=0) — cannot trace rays");
        vkEndCommandBuffer(cmd);
        return;
    }

    // DEBUG LOG SBT (once)
    static bool logged_sbt = false;
    if (!logged_sbt) {
        LOG_INFO_CAT("RENDERER", "SBT Addresses: raygen={} miss={} hit={} call={}",
                     ptr_to_hex((void*)rgen->deviceAddress),
                     ptr_to_hex((void*)miss->deviceAddress),
                     ptr_to_hex((void*)hit->deviceAddress),
                     ptr_to_hex((void*)call->deviceAddress));
        logged_sbt = true;
    }

    // Trace rays
    rtx_->traceRays(cmd, rgen, miss, hit, call,
                    static_cast<uint32_t>(swapchainExtent_.width),
                    static_cast<uint32_t>(swapchainExtent_.height), 1);

    // Blit result to swapchain
    blitRTOutputToSwapchain(cmd, imageIndex);

    // GPU timestamp end
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPools_[currentFrame_], 1);

    VK_CHECK(vkEndCommandBuffer(cmd), "End cmd");

    // Submit command buffer
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    VkSubmitInfo submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1, .pWaitSemaphores = &imageAvailableSemaphores_[currentFrame_],
        .pWaitDstStageMask = &waitStage,
        .commandBufferCount = 1, .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1, .pSignalSemaphores = &renderFinishedSemaphores_[currentFrame_]
    };
    VK_CHECK(vkQueueSubmit(context_->graphicsQueue, 1, &submit, inFlightFences_[currentFrame_]), "Submit");

    // Present
    VkPresentInfoKHR present{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1, .pWaitSemaphores = &renderFinishedSemaphores_[currentFrame_],
        .swapchainCount = 1, .pSwapchains = &swapchain_, .pImageIndices = &imageIndex
    };
    res = vkQueuePresentKHR(context_->presentQueue, &present);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
        handleResize(width_, height_);
    else if (res != VK_SUCCESS)
        LOG_ERROR_CAT("RENDERER", "Failed to present: {}", res);

    // Retrieve GPU timestamp
    uint64_t timestamps[2] = {0};
    auto gpuRes = vkGetQueryPoolResults(context_->device, queryPools_[currentFrame_], 0, 2, sizeof(timestamps),
                                        timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    double gpuTimeNs = 0.0;
    if (gpuRes == VK_SUCCESS) {
        gpuTimeNs = static_cast<double>(timestamps[1] - timestamps[0]) * timestampPeriod_;
    }
    double gpuTimeMs = gpuTimeNs / 1e6;

    auto frameEnd = std::chrono::high_resolution_clock::now();  // CPU end
    auto cpuTimeMs = std::chrono::duration<double, std::chrono::milliseconds::period>(frameEnd - frameStart).count();

    // Update running averages/min/max for CPU and GPU
    avgFrameTimeMs_ = (avgFrameTimeMs_ * 0.9f) + (cpuTimeMs * 0.1f);
    minFrameTimeMs_ = std::min(minFrameTimeMs_, static_cast<float>(cpuTimeMs));
    maxFrameTimeMs_ = std::max(maxFrameTimeMs_, static_cast<float>(cpuTimeMs));

    avgGpuTimeMs_ = (avgGpuTimeMs_ * 0.9f) + (static_cast<float>(gpuTimeMs) * 0.1f);
    minGpuTimeMs_ = std::min(minGpuTimeMs_, static_cast<float>(gpuTimeMs));
    maxGpuTimeMs_ = std::max(maxGpuTimeMs_, static_cast<float>(gpuTimeMs));

    framesThisSecond_++;

    if (updateMetrics) {
        int fps = framesThisSecond_;
        std::string accumStr = resetAccumulation_ ? "RESET" : std::to_string(frameNumber_);
        LOG_INFO_CAT("FPS", "{}FPS: {}/CPU:{:.1f}ms[{}~{:.1f}] / GPU:{:.1f}ms[{}~{:.1f}] / ACCUM:{} / RES:{}x{} / FRAME:{} {}", OCEAN_TEAL,
                     fps,
                     cpuTimeMs, avgFrameTimeMs_, minFrameTimeMs_, maxFrameTimeMs_,  // CPU: current/avg[min~max]
                     gpuTimeMs, avgGpuTimeMs_, minGpuTimeMs_, maxGpuTimeMs_,      // GPU: current/avg[min~max]
                     accumStr,
                     swapchainExtent_.width, swapchainExtent_.height,
                     currentFrame_,
                     RESET);
        framesThisSecond_ = 0;
        lastFPSTime_ = now;

        // Reset min/max every second for fresh stats
        minFrameTimeMs_ = std::numeric_limits<float>::max();
        maxFrameTimeMs_ = 0.0f;
        minGpuTimeMs_ = std::numeric_limits<float>::max();
        maxGpuTimeMs_ = 0.0f;
    }

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    currentRTIndex_ = (currentRTIndex_ + 1) % 2;
    currentAccumIndex_ = (currentAccumIndex_ + 1) % 2;
}

// =============================================================================
// HANDLE RESIZE
// =============================================================================
void VulkanRenderer::handleResize(int newWidth, int newHeight) {
    if (newWidth == 0 || newHeight == 0) return;

    vkDeviceWaitIdle(context_->device);

    context_->destroySwapchain();
    context_->createSwapchain();

    width_ = newWidth;
    height_ = newHeight;

    swapchain_           = context_->swapchain;
    swapchainImages_     = context_->swapchainImages;
    swapchainImageViews_ = context_->swapchainImageViews;
    swapchainExtent_     = context_->swapchainExtent;
    swapchainImageFormat_= context_->swapchainImageFormat;

    createRTOutputImages();
    createAccumulationImages();
    createFramebuffers();
    createCommandBuffers();

    rtx_->updateRTX(context_->physicalDevice, context_->commandPool, context_->graphicsQueue,
                    bufferManager_->getGeometries(), bufferManager_->getDimensionStates());

    updateRTDescriptors();
    resetAccumulation_ = true;
    frameNumber_ = 0;
}

// =============================================================================
// CREATE ENVIRONMENT MAP (1x1 black texture fallback)
// =============================================================================
void VulkanRenderer::createEnvironmentMap() {
    const std::vector<uint8_t> blackPixel = {0, 0, 0, 255};
    auto cmd = VulkanInitializer::beginSingleTimeCommands(*context_);

    VkImageCreateInfo ici{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {1, 1, 1},
        .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VkImage img;
    VK_CHECK(vkCreateImage(context_->device, &ici, nullptr, &img), "Env image");

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(context_->device, img, &req);
    VkMemoryAllocateInfo mai{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = req.size,
        .memoryTypeIndex = findMemoryType(context_->physicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VkDeviceMemory mem;
    VK_CHECK(vkAllocateMemory(context_->device, &mai, nullptr, &mem), "Env mem");
    VK_CHECK(vkBindImageMemory(context_->device, img, mem, 0), "Bind");

    transitionImageLayout(cmd, img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT);

    VkBuffer staging;
    VkDeviceMemory stagingMem;
    bufferManager_->createBuffer(context_->device, context_->physicalDevice, 4,
                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 staging, stagingMem, nullptr, *context_);

    void* data;
    vkMapMemory(context_->device, stagingMem, 0, 4, 0, &data);
    memcpy(data, blackPixel.data(), 4);
    vkUnmapMemory(context_->device, stagingMem);

    VkBufferImageCopy copy{
        .bufferOffset = 0,
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageExtent = {1, 1, 1}
    };
    vkCmdCopyBufferToImage(cmd, staging, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    transitionImageLayout(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

    VulkanInitializer::endSingleTimeCommands(*context_, cmd);

    VkImageViewCreateInfo vci{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = img,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    VkImageView view;
    VK_CHECK(vkCreateImageView(context_->device, &vci, nullptr, &view), "Env view");

    VkSamplerCreateInfo sci{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT
    };
    VkSampler sampler;
    VK_CHECK(vkCreateSampler(context_->device, &sci, nullptr, &sampler), "Env sampler");

    envMapImage_ = makeHandle(context_->device, img, "EnvMap");
    envMapImageMemory_ = makeHandle(context_->device, mem, "EnvMem");
    envMapImageView_ = makeHandle(context_->device, view, "EnvView");
    envMapSampler_ = makeHandle(context_->device, sampler, "EnvSampler");

    vkDestroyBuffer(context_->device, staging, nullptr);
    vkFreeMemory(context_->device, stagingMem, nullptr);
}

// =============================================================================
// STUBS
// =============================================================================
void VulkanRenderer::createComputeDescriptorSets() { }
void VulkanRenderer::createFramebuffers() { }

// =============================================================================
// INITIALIZE ALL BUFFER DATA
// =============================================================================
void VulkanRenderer::initializeAllBufferData(uint32_t frameCount, VkDeviceSize matSize, VkDeviceSize dimSize) {
    for (uint32_t i = 0; i < frameCount; ++i) {
        VkBuffer rawUniform;
        VkDeviceMemory rawUniformMem;
        bufferManager_->createBuffer(context_->device, context_->physicalDevice,
                                     sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     rawUniform, rawUniformMem, nullptr, *context_);
        uniformBuffers_[i] = makeHandle(context_->device, rawUniform, "Uniform Buffer");
        uniformBufferMemories_[i] = makeHandle(context_->device, rawUniformMem, "Uniform Memory");
    }

    for (uint32_t i = 0; i < frameCount; ++i) {
        VkBuffer rawMat;
        VkDeviceMemory rawMatMem;
        bufferManager_->createBuffer(context_->device, context_->physicalDevice,
                                     matSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     rawMat, rawMatMem, nullptr, *context_);
        materialBuffers_[i] = makeHandle(context_->device, rawMat, "Material Buffer");
        materialBufferMemory_[i] = makeHandle(context_->device, rawMatMem, "Material Memory");
    }

    for (uint32_t i = 0; i < frameCount; ++i) {
        VkBuffer rawDim;
        VkDeviceMemory rawDimMem;
        bufferManager_->createBuffer(context_->device, context_->physicalDevice,
                                     dimSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     rawDim, rawDimMem, nullptr, *context_);
        dimensionBuffers_[i] = makeHandle(context_->device, rawDim, "Dimension Buffer");
        dimensionBufferMemory_[i] = makeHandle(context_->device, rawDimMem, "Dimension Memory");
    }
}

// =============================================================================
// CREATE COMMAND BUFFERS
// =============================================================================
void VulkanRenderer::createCommandBuffers() {
    commandBuffers_.resize(swapchainImages_.size(), VK_NULL_HANDLE);
    if (swapchainImages_.empty()) {
        LOG_WARN_CAT("RENDERER", "No swapchain images – skipping command buffer allocation");
        return;
    }
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context_->commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(swapchainImages_.size())
    };
    VK_CHECK(vkAllocateCommandBuffers(context_->device, &allocInfo, commandBuffers_.data()), "Cmd buffers");
    LOG_INFO_CAT("RENDERER", "Command buffers allocated: {} (extent: {}x{})", swapchainImages_.size(), swapchainExtent_.width, swapchainExtent_.height);
}

// =============================================================================
// UPDATE UNIFORM BUFFER
// =============================================================================
void VulkanRenderer::updateUniformBuffer(uint32_t currentImage, const Camera& camera) {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto currentTime = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

    UniformBufferObject ubo{};

    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix();
    ubo.viewInverse = glm::inverse(view);
    ubo.projInverse = glm::inverse(proj);
    ubo.camPos = glm::vec4(camera.getPosition(), 1.0f);
    ubo.time = time;
    ubo.frame = static_cast<uint32_t>(frameNumber_);

    void* data;
    vkMapMemory(context_->device, uniformBufferMemories_[currentImage].get(), 0, sizeof(ubo), 0, &data);
    memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(context_->device, uniformBufferMemories_[currentImage].get());
}

// =============================================================================
// CREATE RT OUTPUT IMAGES (ping-pong)
// =============================================================================
void VulkanRenderer::createRTOutputImages() {
    for (int i = 0; i < 2; ++i) {
        VkImageCreateInfo ici{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .extent = {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1},
            .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        VkImage img;
        VK_CHECK(vkCreateImage(context_->device, &ici, nullptr, &img), "RT image");

        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(context_->device, img, &req);
        VkMemoryAllocateInfo mai{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = req.size,
            .memoryTypeIndex = findMemoryType(context_->physicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };
        VkDeviceMemory mem;
        VK_CHECK(vkAllocateMemory(context_->device, &mai, nullptr, &mem), "RT memory");
        VK_CHECK(vkBindImageMemory(context_->device, img, mem, 0), "Bind image");

        VkImageViewCreateInfo vci{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = img,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        VkImageView view;
        VK_CHECK(vkCreateImageView(context_->device, &vci, nullptr, &view), "RT view");

        rtOutputImages_[i] = makeHandle(context_->device, img, "RT Output");
        rtOutputMemories_[i] = makeHandle(context_->device, mem, "RT Mem");
        rtOutputViews_[i] = makeHandle(context_->device, view, "RT View");

        auto cmd = VulkanInitializer::beginSingleTimeCommands(*context_);
        transitionImageLayout(cmd, img,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            0, VK_ACCESS_SHADER_WRITE_BIT);
        VulkanInitializer::endSingleTimeCommands(*context_, cmd);
    }
}

// =============================================================================
// CREATE ACCUMULATION IMAGES (ping-pong)
// =============================================================================
void VulkanRenderer::createAccumulationImages() {
    for (int i = 0; i < 2; ++i) {
        VkImageCreateInfo ici{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .extent = {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1},
            .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        VkImage img;
        VK_CHECK(vkCreateImage(context_->device, &ici, nullptr, &img), "Accum image");

        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(context_->device, img, &req);
        VkMemoryAllocateInfo mai{
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = req.size,
            .memoryTypeIndex = findMemoryType(context_->physicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };
        VkDeviceMemory mem;
        VK_CHECK(vkAllocateMemory(context_->device, &mai, nullptr, &mem), "Accum memory");
        VK_CHECK(vkBindImageMemory(context_->device, img, mem, 0), "Bind image");

        VkImageViewCreateInfo vci{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = img,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R32G32B32A32_SFLOAT,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        VkImageView view;
        VK_CHECK(vkCreateImageView(context_->device, &vci, nullptr, &view), "Accum view");

        accumImages_[i] = makeHandle(context_->device, img, "Accum Image");
        accumMemories_[i] = makeHandle(context_->device, mem, "Accum Mem");
        accumViews_[i] = makeHandle(context_->device, view, "Accum View");

        auto cmd = VulkanInitializer::beginSingleTimeCommands(*context_);
        transitionImageLayout(cmd, img,
            VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        VulkanInitializer::endSingleTimeCommands(*context_, cmd);
    }
}

// =============================================================================
// UPDATE RT DESCRIPTORS
// =============================================================================
void VulkanRenderer::updateRTDescriptors() {
    VkAccelerationStructureKHR tlas = rtx_->getTLAS();
    VkWriteDescriptorSetAccelerationStructureKHR asWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlas
    };

    VkDescriptorImageInfo outInfo{
        .imageView = rtOutputViews_[currentRTIndex_].get(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkDescriptorImageInfo accumInfo{
        .imageView = accumViews_[currentAccumIndex_].get(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkDescriptorBufferInfo uboInfo{
        .buffer = uniformBuffers_[currentFrame_].get(),
        .offset = 0,
        .range = VK_WHOLE_SIZE
    };
    VkDescriptorImageInfo envInfo{
        .sampler = envMapSampler_.get(),
        .imageView = envMapImageView_.get(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    std::array<VkWriteDescriptorSet, 5> writes = {{
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &asWrite,
            .dstSet = rtxDescriptorSet_,
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = rtxDescriptorSet_,
            .dstBinding = 1,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &outInfo
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = rtxDescriptorSet_,
            .dstBinding = 2,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &uboInfo
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = rtxDescriptorSet_,
            .dstBinding = 5,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &envInfo
        },
        {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = rtxDescriptorSet_,
            .dstBinding = 6,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &accumInfo
        }
    }};

    vkUpdateDescriptorSets(context_->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

// =============================================================================
// TRANSITION & BLIT
// =============================================================================
void VulkanRenderer::transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                           VkImageLayout oldLayout, VkImageLayout newLayout,
                                           VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                                           VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                                           VkImageAspectFlags aspect) {
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void VulkanRenderer::blitRTOutputToSwapchain(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkImage swapImg = swapchainImages_[imageIndex];

    transitionImageLayout(cmd, rtOutputImages_[currentRTIndex_].get(),
        VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

    transitionImageLayout(cmd, swapImg,
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, VK_ACCESS_TRANSFER_WRITE_BIT);

    VkImageBlit blit = {};
    blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.srcOffsets[1] = {static_cast<int32_t>(width_), static_cast<int32_t>(height_), 1};
    blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.dstOffsets[1] = {static_cast<int32_t>(width_), static_cast<int32_t>(height_), 1};

    vkCmdBlitImage(cmd,
        rtOutputImages_[currentRTIndex_].get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        swapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &blit, VK_FILTER_NEAREST);

    transitionImageLayout(cmd, swapImg,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, 0);
}

// =============================================================================
// CLEANUP
// =============================================================================
void VulkanRenderer::cleanup() noexcept {
    vkDeviceWaitIdle(context_->device);
    for (auto p : queryPools_) if (p) vkDestroyQueryPool(context_->device, p, nullptr);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (imageAvailableSemaphores_[i]) vkDestroySemaphore(context_->device, imageAvailableSemaphores_[i], nullptr);
        if (renderFinishedSemaphores_[i]) vkDestroySemaphore(context_->device, renderFinishedSemaphores_[i], nullptr);
        if (inFlightFences_[i]) vkDestroyFence(context_->device, inFlightFences_[i], nullptr);
    }
    rtx_.reset();
}

} // namespace VulkanRTX