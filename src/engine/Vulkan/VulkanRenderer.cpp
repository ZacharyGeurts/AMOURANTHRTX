// src/engine/Vulkan/VulkanRenderer.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// RTX REMASTERED: 9-MODE CORE DISPATCH | 1-9 Keys | Wave Animations
// C++20: Designated Init + Ranges + std::format | FULL RAYTRACING + RASTER FUSION
// FIXED: Cmd buffer logging | RT output/accum ping-pong | Tonemap compute | Zero-init staging

/* -----------------------------------------------------------------------------
   GROK PROTIP: This ain't your grandma's renderer. This is AMOURANTH RTX.
   ----------------------------------------------------------------------------- */
/*
   GROK PROTIP: 9 MODES — press 1-9 to switch. Mode 1 = raster, 9 = full RT chaos.
   GROK PROTIP: std::format = type-safe, compile-time checked. No more %zu UB.
   GROK PROTIP: NO mutex. Vulkan is single-threaded per-queue. GPU don't wait.
   GROK PROTIP: Command buffers per swapchain image → zero secondary buffer drama.
   GROK PROTIP: Device-local RT output + accum → max bandwidth, zero host sync.
   GROK PROTIP: Zero-init via staging → no shader NaNs. Your rays thank you.
   GROK PROTIP: Accumulation reset on camera move → no ghosting, no blur.
   GROK PROTIP: GPU timestamps per frame → real ms, no CPU lies.
   GROK PROTIP: Batch descriptor updates → less CPU overhead, more FPS.
   GROK PROTIP: 16x16 dispatch tiles → perfect wave utilization on AMD/NVIDIA.
   GROK PROTIP: Tonemap in compute → no render pass, no barriers, pure speed.
   GROK PROTIP: Full resize recreate → no dangling handles, no crashes.
   GROK PROTIP: RAII + reverse cleanup → zero leaks, zero tears.
   GROK PROTIP: Stats every second: FPS, CPU, GPU, min/max, mode, accum.
   GROK PROTIP: EMA smoothing → buttery smooth FPS numbers.
   GROK PROTIP: Color-coded logs → green = good, red = bad, cyan = mode switch.
   GROK PROTIP: This renderer is for COOL KIDS who want STATS and SHIT.
   GROK PROTIP: Press 1-9. Watch the numbers fly. Feel the power.
   GROK PROTIP: You paid for the whole GPU. Use the whole GPU.
*/

#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/logging.hpp"
#include "engine/core.hpp"
#include "stb/stb_image.h"
#include <tinyobjloader/tiny_obj_loader.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <cstring>
#include <chrono>
#include <array>
#include <cmath>
#include <algorithm>
#include <memory>
#include <format>  // C++20: std::format — ENABLED

using namespace Logging::Color;
using namespace Dispose;

namespace VulkanRTX {

// =============================================================================
// GROK PROTIP: Memory type finder — device-local = fast RT
// =============================================================================
static uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t filter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        if ((filter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    throw std::runtime_error("No suitable memory type");
}

// =============================================================================
// CONSTRUCTOR — C++20 + RAII + std::format
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
      renderMode_(1),
      framesThisSecond_(0),
      timestampPeriod_(0.0),
      avgFrameTimeMs_(0.0f),
      minFrameTimeMs_(std::numeric_limits<float>::max()),
      maxFrameTimeMs_(0.0f),
      avgGpuTimeMs_(0.0f),
      minGpuTimeMs_(std::numeric_limits<float>::max()),
      maxGpuTimeMs_(0.0f)
{
    LOG_INFO_CAT("RENDERER", std::format("{}AMOURANTH RTX [{}x{}] - 9 MODES READY{}", EMERALD_GREEN, width, height, RESET));

    // GROK PROTIP: Timestamp period = GPU clock → accurate ms
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(context_->physicalDevice, &props);
    timestampPeriod_ = props.limits.timestampPeriod;

    // GROK PROTIP: Designated init + loop = clean sync objects
    VkSemaphoreCreateInfo semInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vkCreateSemaphore(context_->device, &semInfo, nullptr, &imageAvailableSemaphores_[i]), "Sem");
        VK_CHECK(vkCreateSemaphore(context_->device, &semInfo, nullptr, &renderFinishedSemaphores_[i]), "Sem");
        VK_CHECK(vkCreateFence(context_->device, &fenceInfo, nullptr, &inFlightFences_[i]), "Fence");
    }

    swapchain_           = context_->swapchain;
    swapchainImages_     = context_->swapchainImages;
    swapchainImageViews_ = context_->swapchainImageViews;
    swapchainExtent_     = context_->swapchainExtent;
    swapchainImageFormat_= context_->swapchainImageFormat;

    if (swapchainImages_.empty()) {
        LOG_ERROR_CAT("RENDERER", "Swapchain has no images");
        throw std::runtime_error("Empty swapchain images");
    }
    LOG_INFO_CAT("RENDERER", std::format("Swapchain: {} images {}x{}", swapchainImages_.size(), swapchainExtent_.width, swapchainExtent_.height));

    // GROK PROTIP: Query pools per frame → no sync, no stall
    for (auto& pool : queryPools_) {
        VkQueryPoolCreateInfo qi{.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                                 .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = 2};
        VK_CHECK(vkCreateQueryPool(context_->device, &qi, nullptr, &pool), "Query pool");
    }

    // GROK PROTIP: Over-allocate descriptors → future-proof
    std::array<VkDescriptorPoolSize, 5> poolSizes = {{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT * 2},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT * 6},
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
    }};
    VkDescriptorPoolCreateInfo poolInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
                                        .maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT + 2 + swapchainImages_.size()),
                                        .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
                                        .pPoolSizes = poolSizes.data()};
    VkDescriptorPool rawPool;
    VK_CHECK(vkCreateDescriptorPool(context_->device, &poolInfo, nullptr, &rawPool), "Desc pool");
    descriptorPool_ = makeHandle(context_->device, rawPool, "Renderer Pool");
}

// =============================================================================
// GROK PROTIP: Clamp mode 1-9. No invalid states.
// =============================================================================
void VulkanRenderer::setRenderMode(int mode) {
    if (mode < 1 || mode > 9) {
        LOG_WARNING_CAT("RENDERER", std::format("Invalid mode {} - clamping 1-9", mode));
        mode = 1;
    }
    LOG_INFO_CAT("RENDERER", std::format("{}AMOURANTH RTX -> MODE {}{}", ARCTIC_CYAN, mode, RESET));
    
    renderMode_ = mode;
    resetAccumulation_ = true;
    frameNumber_ = 0;
}

// =============================================================================
// DESTRUCTOR — RAII cleans up
// =============================================================================
VulkanRenderer::~VulkanRenderer() {
    cleanup();
}

// =============================================================================
// CLEANUP HELPERS — NOEXCEPT
// =============================================================================
void VulkanRenderer::destroyRTOutputImages() noexcept {
    for (int i = 0; i < 2; ++i) {
        rtOutputImages_[i].reset();
        rtOutputMemories_[i].reset();
        rtOutputViews_[i].reset();
    }
}

void VulkanRenderer::destroyAccumulationImages() noexcept {
    for (int i = 0; i < 2; ++i) {
        accumImages_[i].reset();
        accumMemories_[i].reset();
        accumViews_[i].reset();
    }
}

void VulkanRenderer::destroyAllBuffers() noexcept {
    for (size_t i = 0; i < uniformBuffers_.size(); ++i) {
        uniformBuffers_[i].reset();
        uniformBufferMemories_[i].reset();
    }
    for (size_t i = 0; i < materialBuffers_.size(); ++i) {
        materialBuffers_[i].reset();
        materialBufferMemory_[i].reset();
    }
    for (size_t i = 0; i < dimensionBuffers_.size(); ++i) {
        dimensionBuffers_[i].reset();
        dimensionBufferMemory_[i].reset();
    }
    tonemapDescriptorSets_.clear();
}

// =============================================================================
// TAKE OWNERSHIP — LAZY INIT
// =============================================================================
void VulkanRenderer::takeOwnership(std::unique_ptr<VulkanPipelineManager> pm,
                                   std::unique_ptr<VulkanBufferManager> bm)
{
    LOG_INFO_CAT("RENDERER", std::format("{}AMOURANTH RTX - TAKING OWNERSHIP{}", ARCTIC_CYAN, RESET));
    pipelineManager_ = std::move(pm);
    bufferManager_   = std::move(bm);

    rtx_ = std::make_unique<VulkanRTX>(context_, width_, height_, pipelineManager_.get());
    rtxDescriptorSet_ = rtx_->getDescriptorSet();
    if (!rtxDescriptorSet_) throw std::runtime_error("RTX descriptor set missing");

    createRTOutputImages();
    createAccumulationImages();
    createEnvironmentMap();
    createComputeDescriptorSets();
    createFramebuffers();

    constexpr VkDeviceSize kMatSize = 256 * sizeof(MaterialData);
    constexpr VkDeviceSize kDimSize = 1024 * sizeof(float);
    initializeAllBufferData(MAX_FRAMES_IN_FLIGHT, kMatSize, kDimSize);

    // GROK PROTIP: Compute pipeline FIRST — tonemap needs it
    pipelineManager_->createComputePipeline();
    VkPipeline computePipe = pipelineManager_->getComputePipeline();
    VkPipelineLayout computeLayout = pipelineManager_->getComputePipelineLayout();
    if (!computePipe || !computeLayout) {
        throw std::runtime_error("Failed to create tonemap compute pipeline");
    }

    // RT pipeline after
    pipelineManager_->createRayTracingPipeline();
    rtPipeline_ = pipelineManager_->getRayTracingPipeline();
    rtPipelineLayout_ = pipelineManager_->getRayTracingPipelineLayout();
    if (!rtPipeline_) throw std::runtime_error("RT pipeline missing");

    rtx_->setRayTracingPipeline(rtPipeline_, rtPipelineLayout_);
    rtx_->createShaderBindingTable(context_->physicalDevice);

    createCommandBuffers();
    updateRTDescriptors();

    LOG_INFO_CAT("RENDERER", std::format("{}AMOURANTH RTX FULLY ARMED - PRESS 1-9{}", EMERALD_GREEN, RESET));
}

// =============================================================================
// RENDER FRAME — 9-MODE DISPATCH + TONEMAP + STATS + CRASH-PROOF
// =============================================================================
void VulkanRenderer::renderFrame(const Camera& camera, float deltaTime) {
    auto frameStart = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::steady_clock::now();
    bool updateMetrics = (std::chrono::duration_cast<std::chrono::seconds>(now - lastFPSTime_).count() >= 1.0);

    vkWaitForFences(context_->device, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    auto res = vkAcquireNextImageKHR(context_->device, swapchain_, UINT64_MAX,
                                     imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &imageIndex);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
        handleResize(width_, height_);
        return;
    } else if (res != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", std::format("Acquire failed: {}", static_cast<int>(res)));
        return;
    }
    if (imageIndex >= commandBuffers_.size()) {
        LOG_ERROR_CAT("RENDERER", std::format("Invalid imageIndex {}", imageIndex));
        return;
    }
    vkResetFences(context_->device, 1, &inFlightFences_[currentFrame_]);

    // GROK PROTIP: Only reset accumulation on real camera motion
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
    updateRTDescriptors();

    VkCommandBuffer cmd = commandBuffers_[imageIndex];
    if (cmd == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RENDERER", std::format("Null cmd buffer {}", imageIndex));
        return;
    }

    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "Begin cmd");

    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPools_[currentFrame_], 0);

    // === RTX DISPATCH ===
    rtx_->dispatchRenderMode(
        imageIndex,
        bufferManager_->getVertexBuffer(),
        cmd,
        bufferManager_->getIndexBuffer(),
        1.0f,
        static_cast<int>(swapchainExtent_.width),
        static_cast<int>(swapchainExtent_.height),
        0.0f,
        rtPipelineLayout_,
        rtxDescriptorSet_,
        context_->device,
        uniformBufferMemories_[currentFrame_].get(),
        rtPipeline_,
        deltaTime,
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
        *context_,
        renderMode_
    );

    // === TONEMAP COMPUTE PASS (CRASH-PROOF) ===
    VkImageMemoryBarrier preTonemapBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = swapchainImages_[imageIndex],
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &preTonemapBarrier);

    VkPipeline tonemapPipeline = pipelineManager_->getComputePipeline();
    VkPipelineLayout tonemapLayout = pipelineManager_->getComputePipelineLayout();

    if (!tonemapPipeline || !tonemapLayout) {
        LOG_ERROR_CAT("RENDERER", "Tonemap pipeline or layout missing! Skipping tonemap pass.");
        VkImageMemoryBarrier fallbackPresentBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = 0,
            .dstAccessMask = 0,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swapchainImages_[imageIndex],
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &fallbackPresentBarrier);
    } else {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tonemapPipeline);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tonemapLayout, 0, 1,
                                &tonemapDescriptorSets_[imageIndex], 0, nullptr);

        uint32_t groupCountX = (swapchainExtent_.width + 15u) / 16u;
        uint32_t groupCountY = (swapchainExtent_.height + 15u) / 16u;
        vkCmdDispatch(cmd, groupCountX, groupCountY, 1);

        VkImageMemoryBarrier postTonemapBarrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = 0,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = swapchainImages_[imageIndex],
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &postTonemapBarrier);
    }

    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPools_[currentFrame_], 1);
    VK_CHECK(vkEndCommandBuffer(cmd), "End cmd");

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR |
                                     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT |
                                     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkSubmitInfo submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &imageAvailableSemaphores_[currentFrame_],
        .pWaitDstStageMask = &waitStage,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &renderFinishedSemaphores_[currentFrame_]
    };
    VK_CHECK(vkQueueSubmit(context_->graphicsQueue, 1, &submit, inFlightFences_[currentFrame_]), "Submit");

    VkPresentInfoKHR present{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinishedSemaphores_[currentFrame_],
        .swapchainCount = 1,
        .pSwapchains = &swapchain_,
        .pImageIndices = &imageIndex
    };
    res = vkQueuePresentKHR(context_->presentQueue, &present);
    if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR)
        handleResize(width_, height_);
    else if (res != VK_SUCCESS)
        LOG_ERROR_CAT("RENDERER", std::format("Present failed: {}", static_cast<int>(res)));

    // GROK PROTIP: GPU timing with WAIT_BIT = accurate
    uint64_t timestamps[2] = {0};
    auto gpuRes = vkGetQueryPoolResults(context_->device, queryPools_[currentFrame_], 0, 2, sizeof(timestamps),
                                        timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    double gpuTimeMs = 0.0;
    if (gpuRes == VK_SUCCESS) {
        gpuTimeMs = static_cast<double>(timestamps[1] - timestamps[0]) * timestampPeriod_ / 1e6;
    }

    auto frameEnd = std::chrono::high_resolution_clock::now();
    auto cpuTimeMs = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();

    // GROK PROTIP: EMA = smooth stats
    avgFrameTimeMs_ = (avgFrameTimeMs_ * 0.9f) + (static_cast<float>(cpuTimeMs) * 0.1f);
    minFrameTimeMs_ = std::min(minFrameTimeMs_, static_cast<float>(cpuTimeMs));
    maxFrameTimeMs_ = std::max(maxFrameTimeMs_, static_cast<float>(cpuTimeMs));
    avgGpuTimeMs_ = (avgGpuTimeMs_ * 0.9f) + (static_cast<float>(gpuTimeMs) * 0.1f);
    minGpuTimeMs_ = std::min(minGpuTimeMs_, static_cast<float>(gpuTimeMs));
    maxGpuTimeMs_ = std::max(maxGpuTimeMs_, static_cast<float>(gpuTimeMs));

    framesThisSecond_++;

    if (updateMetrics) {
        std::string accumStr = resetAccumulation_ ? "RESET" : std::to_string(frameNumber_);
        LOG_INFO_CAT("FPS", std::format(
            "{}FPS: {} | CPU: {:.1f}ms (avg: {:.1f}) [{:.1f}~{:.1f}] | GPU: {:.1f}ms (avg: {:.1f}) [{:.1f}~{:.1f}] | MODE: {} | ACCUM: {} | {}x{}{}",
            OCEAN_TEAL,
            framesThisSecond_,
            cpuTimeMs, avgFrameTimeMs_, minFrameTimeMs_, maxFrameTimeMs_,
            gpuTimeMs, avgGpuTimeMs_, minGpuTimeMs_, maxGpuTimeMs_,
            renderMode_, accumStr,
            swapchainExtent_.width, swapchainExtent_.height,
            RESET
        ));
        
        framesThisSecond_ = 0;
        lastFPSTime_ = now;
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
// RESIZE — FULL RECREATE
// =============================================================================
void VulkanRenderer::handleResize(int newWidth, int newHeight) {
    if (newWidth == 0 || newHeight == 0) return;
    vkDeviceWaitIdle(context_->device);

    destroyRTOutputImages();
    destroyAccumulationImages();
    destroyAllBuffers();
    commandBuffers_.clear();

    context_->destroySwapchain();
    context_->createSwapchain();

    width_ = newWidth;
    height_ = newHeight;
    swapchain_ = context_->swapchain;
    swapchainImages_ = context_->swapchainImages;
    swapchainImageViews_ = context_->swapchainImageViews;
    swapchainExtent_ = context_->swapchainExtent;
    swapchainImageFormat_ = context_->swapchainImageFormat;

    createRTOutputImages();
    createAccumulationImages();
    createFramebuffers();
    createCommandBuffers();
    createComputeDescriptorSets();

    constexpr VkDeviceSize kMatSize = 256 * sizeof(MaterialData);
    constexpr VkDeviceSize kDimSize = 1024 * sizeof(float);
    initializeAllBufferData(MAX_FRAMES_IN_FLIGHT, kMatSize, kDimSize);

    rtx_->updateRTX(context_->physicalDevice, context_->commandPool, context_->graphicsQueue,
                    bufferManager_->getGeometries(), bufferManager_->getDimensionStates());

    updateRTDescriptors();
    resetAccumulation_ = true;
    frameNumber_ = 0;
}

// =============================================================================
// ENVIRONMENT MAP — BLACK FALLBACK
// =============================================================================
void VulkanRenderer::createEnvironmentMap() {
    const std::array<uint8_t, 4> blackPixel{0, 0, 0, 255};
    auto cmd = VulkanInitializer::beginSingleTimeCommands(*context_);

    VkImageCreateInfo ici{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                          .imageType = VK_IMAGE_TYPE_2D,
                          .format = VK_FORMAT_R8G8B8A8_UNORM,
                          .extent = {1, 1, 1},
                          .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
                          .tiling = VK_IMAGE_TILING_OPTIMAL,
                          .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                          .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
    VkImage img;
    VK_CHECK(vkCreateImage(context_->device, &ici, nullptr, &img), "Env image");

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(context_->device, img, &req);
    VkMemoryAllocateInfo mai{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                             .allocationSize = req.size,
                             .memoryTypeIndex = findMemoryType(context_->physicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
    VkDeviceMemory mem;
    VK_CHECK(vkAllocateMemory(context_->device, &mai, nullptr, &mem), "Env mem");
    VK_CHECK(vkBindImageMemory(context_->device, img, mem, 0), "Bind");

    transitionImageLayout(cmd, img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
                          VK_IMAGE_ASPECT_COLOR_BIT);

    VkBuffer staging;
    VkDeviceMemory stagingMem;
    bufferManager_->createBuffer(context_->device, context_->physicalDevice, 4,
                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 staging, stagingMem, nullptr, *context_);

    void* data;
    vkMapMemory(context_->device, stagingMem, 0, 4, 0, &data);
    std::memcpy(data, blackPixel.data(), 4);
    vkUnmapMemory(context_->device, stagingMem);

    VkBufferImageCopy copy{.bufferOffset = 0,
                           .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                           .imageExtent = {1, 1, 1}};
    vkCmdCopyBufferToImage(cmd, staging, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    transitionImageLayout(cmd, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
                          VK_IMAGE_ASPECT_COLOR_BIT);

    VulkanInitializer::endSingleTimeCommands(*context_, cmd);

    VkImageViewCreateInfo vci{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                              .image = img,
                              .viewType = VK_IMAGE_VIEW_TYPE_2D,
                              .format = VK_FORMAT_R8G8B8A8_UNORM,
                              .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    VkImageView view;
    VK_CHECK(vkCreateImageView(context_->device, &vci, nullptr, &view), "Env view");

    VkSamplerCreateInfo sci{.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                            .magFilter = VK_FILTER_LINEAR,
                            .minFilter = VK_FILTER_LINEAR,
                            .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
                            .addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                            .addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT,
                            .addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT};
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
// BUFFER INIT — ZERO-INIT VIA STAGING
// =============================================================================
void VulkanRenderer::initializeAllBufferData(uint32_t frameCount, VkDeviceSize matSize, VkDeviceSize dimSize) {
    uniformBuffers_.resize(frameCount);
    uniformBufferMemories_.resize(frameCount);
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

    materialBuffers_.resize(frameCount);
    materialBufferMemory_.resize(frameCount);
    for (uint32_t i = 0; i < frameCount; ++i) {
        VkBuffer rawMat;
        VkDeviceMemory rawMatMem;
        bufferManager_->createBuffer(context_->device, context_->physicalDevice,
                                     matSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     rawMat, rawMatMem, nullptr, *context_);
        materialBuffers_[i] = makeHandle(context_->device, rawMat, "Material Buffer");
        materialBufferMemory_[i] = makeHandle(context_->device, rawMatMem, "Material Memory");

        VkBuffer stagingMat;
        VkDeviceMemory stagingMatMem;
        bufferManager_->createBuffer(context_->device, context_->physicalDevice, matSize,
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     stagingMat, stagingMatMem, nullptr, *context_);
        void* matData;
        vkMapMemory(context_->device, stagingMatMem, 0, matSize, 0, &matData);
        std::memset(matData, 0, static_cast<std::size_t>(matSize));
        vkUnmapMemory(context_->device, stagingMatMem);
        auto cmdMat = VulkanInitializer::beginSingleTimeCommands(*context_);
        VkBufferCopy copyMat{0, 0, matSize};
        vkCmdCopyBuffer(cmdMat, stagingMat, rawMat, 1, &copyMat);
        VulkanInitializer::endSingleTimeCommands(*context_, cmdMat);
        vkDestroyBuffer(context_->device, stagingMat, nullptr);
        vkFreeMemory(context_->device, stagingMatMem, nullptr);
    }

    dimensionBuffers_.resize(frameCount);
    dimensionBufferMemory_.resize(frameCount);
    for (uint32_t i = 0; i < frameCount; ++i) {
        VkBuffer rawDim;
        VkDeviceMemory rawDimMem;
        bufferManager_->createBuffer(context_->device, context_->physicalDevice,
                                     dimSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     rawDim, rawDimMem, nullptr, *context_);
        dimensionBuffers_[i] = makeHandle(context_->device, rawDim, "Dimension Buffer");
        dimensionBufferMemory_[i] = makeHandle(context_->device, rawDimMem, "Dimension Memory");

        VkBuffer stagingDim;
        VkDeviceMemory stagingDimMem;
        bufferManager_->createBuffer(context_->device, context_->physicalDevice, dimSize,
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     stagingDim, stagingDimMem, nullptr, *context_);
        void* dimData;
        vkMapMemory(context_->device, stagingDimMem, 0, dimSize, 0, &dimData);
        std::memset(dimData, 0, static_cast<std::size_t>(dimSize));
        vkUnmapMemory(context_->device, stagingDimMem);
        auto cmdDim = VulkanInitializer::beginSingleTimeCommands(*context_);
        VkBufferCopy copyDim{0, 0, dimSize};
        vkCmdCopyBuffer(cmdDim, stagingDim, rawDim, 1, &copyDim);
        VulkanInitializer::endSingleTimeCommands(*context_, cmdDim);
        vkDestroyBuffer(context_->device, stagingDim, nullptr);
        vkFreeMemory(context_->device, stagingDimMem, nullptr);
    }
}

// =============================================================================
// COMMAND BUFFERS — LOG SIZE
// =============================================================================
void VulkanRenderer::createCommandBuffers() {
    commandBuffers_.resize(swapchainImages_.size(), VK_NULL_HANDLE);
    if (swapchainImages_.empty()) {
        LOG_WARN_CAT("RENDERER", "No swapchain images");
        return;
    }
    VkCommandBufferAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                                          .commandPool = context_->commandPool,
                                          .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                                          .commandBufferCount = static_cast<uint32_t>(swapchainImages_.size())};
    VK_CHECK(vkAllocateCommandBuffers(context_->device, &allocInfo, commandBuffers_.data()), "Cmd buffers");
    LOG_INFO_CAT("RENDERER", std::format("Cmd buffers: {}", commandBuffers_.size()));
}

// =============================================================================
// UNIFORM UPDATE
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
    std::memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(context_->device, uniformBufferMemories_[currentImage].get());
}

// =============================================================================
// RT OUTPUT IMAGES
// =============================================================================
void VulkanRenderer::createRTOutputImages() {
    for (int i = 0; i < 2; ++i) {
        VkImageCreateInfo ici{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                              .imageType = VK_IMAGE_TYPE_2D,
                              .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                              .extent = {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1},
                              .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
                              .tiling = VK_IMAGE_TILING_OPTIMAL,
                              .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
        VkImage img;
        VK_CHECK(vkCreateImage(context_->device, &ici, nullptr, &img), "RT image");

        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(context_->device, img, &req);
        VkMemoryAllocateInfo mai{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                 .allocationSize = req.size,
                                 .memoryTypeIndex = findMemoryType(context_->physicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
        VkDeviceMemory mem;
        VK_CHECK(vkAllocateMemory(context_->device, &mai, nullptr, &mem), "RT memory");
        VK_CHECK(vkBindImageMemory(context_->device, img, mem, 0), "Bind image");

        VkImageViewCreateInfo vci{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                  .image = img,
                                  .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                  .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                  .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
        VkImageView view;
        VK_CHECK(vkCreateImageView(context_->device, &vci, nullptr, &view), "RT view");

        rtOutputImages_[i] = makeHandle(context_->device, img, "RT Output");
        rtOutputMemories_[i] = makeHandle(context_->device, mem, "RT Mem");
        rtOutputViews_[i] = makeHandle(context_->device, view, "RT View");

        auto cmd = VulkanInitializer::beginSingleTimeCommands(*context_);
        transitionImageLayout(cmd, img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, VK_ACCESS_SHADER_WRITE_BIT,
                              VK_IMAGE_ASPECT_COLOR_BIT);
        VulkanInitializer::endSingleTimeCommands(*context_, cmd);
    }
}

// =============================================================================
// ACCUMULATION IMAGES
// =============================================================================
void VulkanRenderer::createAccumulationImages() {
    for (int i = 0; i < 2; ++i) {
        VkImageCreateInfo ici{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                              .imageType = VK_IMAGE_TYPE_2D,
                              .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                              .extent = {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1},
                              .mipLevels = 1, .arrayLayers = 1, .samples = VK_SAMPLE_COUNT_1_BIT,
                              .tiling = VK_IMAGE_TILING_OPTIMAL,
                              .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                              .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
        VkImage img;
        VK_CHECK(vkCreateImage(context_->device, &ici, nullptr, &img), "Accum image");

        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(context_->device, img, &req);
        VkMemoryAllocateInfo mai{.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                 .allocationSize = req.size,
                                 .memoryTypeIndex = findMemoryType(context_->physicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)};
        VkDeviceMemory mem;
        VK_CHECK(vkAllocateMemory(context_->device, &mai, nullptr, &mem), "Accum memory");
        VK_CHECK(vkBindImageMemory(context_->device, img, mem, 0), "Bind image");

        VkImageViewCreateInfo vci{.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                  .image = img,
                                  .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                  .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                                  .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
        VkImageView view;
        VK_CHECK(vkCreateImageView(context_->device, &vci, nullptr, &view), "Accum view");

        accumImages_[i] = makeHandle(context_->device, img, "Accum Image");
        accumMemories_[i] = makeHandle(context_->device, mem, "Accum Mem");
        accumViews_[i] = makeHandle(context_->device, view, "Accum View");

        auto cmd = VulkanInitializer::beginSingleTimeCommands(*context_);
        transitionImageLayout(cmd, img, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                              0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
                              VK_IMAGE_ASPECT_COLOR_BIT);
        VulkanInitializer::endSingleTimeCommands(*context_, cmd);
    }
}

// =============================================================================
// IMAGE LAYOUT TRANSITION
// =============================================================================
void VulkanRenderer::transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                           VkImageLayout oldLayout, VkImageLayout newLayout,
                                           VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                                           VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                                           VkImageAspectFlags aspect) {
    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = srcAccess,
        .dstAccessMask = dstAccess,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = aspect ? aspect : VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1
        }
    };
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// =============================================================================
// COMPUTE DESCRIPTOR SETS
// =============================================================================
void VulkanRenderer::createComputeDescriptorSets() {
    tonemapDescriptorSets_.resize(swapchainImages_.size());
    VkDescriptorSetLayout layout = pipelineManager_->getComputeDescriptorSetLayout();
    std::vector<VkDescriptorSetLayout> layouts(swapchainImages_.size(), layout);

    VkDescriptorSetAllocateInfo allocInfo{.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
                                          .descriptorPool = descriptorPool_.get(),
                                          .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
                                          .pSetLayouts = layouts.data()};
    VK_CHECK(vkAllocateDescriptorSets(context_->device, &allocInfo, tonemapDescriptorSets_.data()), "Alloc tonemap DS");

    for (size_t i = 0; i < swapchainImages_.size(); ++i) {
        VkDescriptorImageInfo hdrInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = rtOutputViews_[0].get(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };
        VkDescriptorImageInfo ldrInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = swapchainImageViews_[i],
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };

        std::array<VkWriteDescriptorSet, 2> writes = {{
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = tonemapDescriptorSets_[i], .dstBinding = 0,
             .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &hdrInfo},
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = tonemapDescriptorSets_[i], .dstBinding = 1,
             .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &ldrInfo}
        }};

        vkUpdateDescriptorSets(context_->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

// =============================================================================
// UPDATE RT DESCRIPTORS — FULL BINDINGS
// =============================================================================
void VulkanRenderer::updateRTDescriptors() {
    VkAccelerationStructureKHR tlas = rtx_->getTLAS();
    VkWriteDescriptorSetAccelerationStructureKHR asWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlas
    };

    VkDescriptorImageInfo outInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = rtOutputViews_[currentRTIndex_].get(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkDescriptorImageInfo accumInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = accumViews_[currentAccumIndex_].get(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkDescriptorBufferInfo uboInfo{.buffer = uniformBuffers_[currentFrame_].get(), .offset = 0, .range = VK_WHOLE_SIZE};
    VkDescriptorImageInfo envInfo{
        .sampler = envMapSampler_.get(),
        .imageView = envMapImageView_.get(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };
    VkDescriptorBufferInfo matInfo{.buffer = materialBuffers_[currentFrame_].get(), .offset = 0, .range = VK_WHOLE_SIZE};
    VkDescriptorBufferInfo dimInfo{.buffer = dimensionBuffers_[currentFrame_].get(), .offset = 0, .range = VK_WHOLE_SIZE};

    std::array<VkWriteDescriptorSet, 7> writes = {{
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .pNext = &asWrite, .dstSet = rtxDescriptorSet_, .dstBinding = 0,
         .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSet_, .dstBinding = 1,
         .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSet_, .dstBinding = 2,
         .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &uboInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSet_, .dstBinding = 3,
         .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &matInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSet_, .dstBinding = 4,
         .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &dimInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSet_, .dstBinding = 5,
         .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &envInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSet_, .dstBinding = 6,
         .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &accumInfo}
    }};

    vkUpdateDescriptorSets(context_->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    for (size_t i = 0; i < tonemapDescriptorSets_.size(); ++i) {
        VkDescriptorImageInfo hdrInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = rtOutputViews_[currentRTIndex_].get(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };
        VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = tonemapDescriptorSets_[i],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo = &hdrInfo
        };
        vkUpdateDescriptorSets(context_->device, 1, &write, 0, nullptr);
    }
}

void VulkanRenderer::createFramebuffers() {}

void VulkanRenderer::cleanup() noexcept {
    vkDeviceWaitIdle(context_->device);

    for (auto p : queryPools_) if (p) vkDestroyQueryPool(context_->device, p, nullptr);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (imageAvailableSemaphores_[i]) vkDestroySemaphore(context_->device, imageAvailableSemaphores_[i], nullptr);
        if (renderFinishedSemaphores_[i]) vkDestroySemaphore(context_->device, renderFinishedSemaphores_[i], nullptr);
        if (inFlightFences_[i]) vkDestroyFence(context_->device, inFlightFences_[i], nullptr);
    }

    destroyRTOutputImages();
    destroyAccumulationImages();
    destroyAllBuffers();
    if (descriptorPool_) descriptorPool_.reset();
    if (envMapImage_) envMapImage_.reset();
    if (envMapImageMemory_) envMapImageMemory_.reset();
    if (envMapImageView_) envMapImageView_.reset();
    if (envMapSampler_) envMapSampler_.reset();
    if (rtPipeline_) { vkDestroyPipeline(context_->device, rtPipeline_, nullptr); rtPipeline_ = VK_NULL_HANDLE; }
    if (rtPipelineLayout_) { vkDestroyPipelineLayout(context_->device, rtPipelineLayout_, nullptr); rtPipelineLayout_ = VK_NULL_HANDLE; }
    commandBuffers_.clear();

    rtx_.reset();
    pipelineManager_.reset();
    bufferManager_.reset();
}

} // namespace VulkanRTX