#pragma once

// =============================================================================
// AMOURANTH RTX Engine — RayCanvas (pure raymarched 3D renderer)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
//
// Pure 3D raymarching — no 2D canvas
// Owns HDR pair, descriptors, materials, adaptive dispatch, timing, resize
// =============================================================================

#include "AMOURANTHRTX.hpp"
#include "ELLIE.hpp"
#include "Camera.hpp"
#include "OptionsMenu.hpp"
#include "Pipeline.hpp"
#include "Materials.hpp"
#include "InputManager.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <vector>
#include <cstdint>
#include <array>
#include <algorithm>
#include <cmath>

class RayCanvas {
public:
    RayCanvas(int initialWidth, int initialHeight, SDL_Window* window)
        : window_(window)
        , window_width_(initialWidth)
        , window_height_(initialHeight)
        , render_width_(initialWidth)
        , render_height_(initialHeight)
        , minimized_(false)
        , destroyed_(false)
        , firstFrame_(true)
        , materialsHandle_(0)
        , hdrOutputImage_(VK_NULL_HANDLE)
        , hdrOutputView_(VK_NULL_HANDLE)
        , hdrOutputMemory_(VK_NULL_HANDLE)
        , prevHdrOutputImage_(VK_NULL_HANDLE)
        , prevHdrOutputView_(VK_NULL_HANDLE)
        , prevHdrOutputMemory_(VK_NULL_HANDLE)
        , descriptorPool_(VK_NULL_HANDLE)
        , descriptorSet_(VK_NULL_HANDLE)
        , adaptiveScale_(1.0f)
        , lastAppliedScale_(1.0f)
        , lastPresentTime_s_(0.0)
        , measuredRefreshRateHz_(60.0f)
        , lastFpsLog_(0.0)
        , frameCount_(0)
        , lastAdaptiveAdjustTime_(0.0)
        , adaptiveFrameCount_(0)
        , needsRecreate_(false)
        , justResizedThisFrame_(false)
        , postResizeGraceFrames_(0)
        , timestampQueryPool_(VK_NULL_HANDLE)
        , timestampPeriodNs_(1.0)
        , smoothedGpuTimeMs_(16.67)
    {
        if (!Swapchain::get()) {
            LOG_FATAL_CAT("RAYCANVAS", "No valid swapchain — navigator must create it first");
            std::abort();
        }

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(rtx().physical, &props);
        timestampPeriodNs_ = props.limits.timestampPeriod;

        createTimestampQueryPool();
        buildMaterialLibrary();
        updateRenderResolution();
        createPersistentHDR();
        createPreviousHDR();
        createDescriptorPoolAndSet();

        Pipeline::initialize();
        Pipeline::create_pipeline_layout();
        Pipeline::create_raymarch_pipeline();

        updateDescriptorSet();
        clearHDRImages();

        LOG_SUCCESS_CAT("RAYCANVAS", "Initialized — {}x{} render target ready", render_width_, render_height_);
    }

    ~RayCanvas() {
        if (destroyed_) return;
        destroyed_ = true;

        vkDeviceWaitIdle(rtx().device);

        if (timestampQueryPool_ != VK_NULL_HANDLE)
            vkDestroyQueryPool(rtx().device, timestampQueryPool_, nullptr);

        if (descriptorSet_ != VK_NULL_HANDLE)
            vkFreeDescriptorSets(rtx().device, descriptorPool_, 1, &descriptorSet_);

        if (descriptorPool_ != VK_NULL_HANDLE)
            vkDestroyDescriptorPool(rtx().device, descriptorPool_, nullptr);

        destroyHDRResources();
        Memory::destroy(materialsHandle_);

        LOG_SUCCESS_CAT("RAYCANVAS", "Destroyed");
    }

    void maybeUpdateCanvas() noexcept {
        if (destroyed_) return;

        frameCount_++;
        adaptiveFrameCount_++;

        bool quit = false, fullscreen_toggle = false;
        int dummyW = 0, dummyH = 0;
        sdl_poll_events(dummyW, dummyH, quit, fullscreen_toggle);

        if (fullscreen_toggle) sdl_toggle_fullscreen();

        if (quit) {
            destroyed_ = true;
            LOG_INFO_CAT("RAYCANVAS", "Quit signal received");
            return;
        }

        int currentW = 0, currentH = 0;
        SDL_GetWindowSizeInPixels(window_, &currentW, &currentH);

        bool nowMinimized = (currentW <= 0 || currentH <= 0);
        if (nowMinimized) {
            minimized_ = true;
            return;
        }

        bool sizeChanged = (currentW != window_width_) || (currentH != window_height_);

        if (minimized_ || sizeChanged || needsRecreate_) {
            if (sizeChanged) vkDeviceWaitIdle(rtx().device);
            onResize(currentW, currentH, sizeChanged);
            minimized_ = false;
            needsRecreate_ = false;
            justResizedThisFrame_ = true;
            return;
        }

        if (justResizedThisFrame_) justResizedThisFrame_ = false;

        if (!Swapchain::get() || !hdrOutputImage_ || !hdrOutputView_) return;

        if (firstFrame_) {
            TotalTime::get().seal();
            firstFrame_ = false;
            LOG_AMOURANTH("Raymarch engine sealed — rendering begins 💖");
            lastAdaptiveAdjustTime_ = TotalTime::get().seconds();
        }

        double now = TotalTime::get().seconds();
        static double lastKnownTime = 0.0;
        if (now <= lastKnownTime) now = lastKnownTime + Swapchain::smoothedRefresh_s;
        lastKnownTime = now;

        uint32_t imageIndex = 0;

        VkFence fence = VK_NULL_HANDLE;
        VkFenceCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vkCreateFence(rtx().device, &fci, nullptr, &fence);

        VkResult acq = ext().vkAcquireNextImageKHR(rtx().device, Swapchain::get(), UINT64_MAX,
                                                   VK_NULL_HANDLE, fence, &imageIndex);

        vkWaitForFences(rtx().device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(rtx().device, fence, nullptr);

        if (acq == VK_ERROR_OUT_OF_DATE_KHR || acq == VK_SUBOPTIMAL_KHR) {
            needsRecreate_ = true;
            return;
        }
        if (acq != VK_SUCCESS) {
            LOG_ERROR_CAT("SWAPCHAIN", "Acquire failed: {}", vkh.result(acq));
            if (acq == VK_ERROR_SURFACE_LOST_KHR || acq == VK_ERROR_DEVICE_LOST) destroyed_ = true;
            return;
        }

        VkCommandBuffer cmd = beginTransientCommandBuffer();
        if (!cmd) return;

        vkCmdResetQueryPool(cmd, timestampQueryPool_, 0, 2);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, timestampQueryPool_, 0);

        transitionImageLayout(cmd, hdrOutputImage_,
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        VkDescriptorSet set = descriptorSet_;
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline::pipeline_layout,
                                0, 1, &set, 0, nullptr);

        lastPresentTime_s_ = now;

        updateRenderResolution();

        float dispatchScale = adaptiveScale_;
        if (postResizeGraceFrames_ > 0) {
            dispatchScale = std::min(dispatchScale, 0.85f);
            postResizeGraceFrames_--;
        }

        int dispatchW = static_cast<int>(std::round(static_cast<float>(window_width_) * dispatchScale));
        int dispatchH = static_cast<int>(std::round(static_cast<float>(window_height_) * dispatchScale));

        Pipeline::dispatch_raymarch(cmd, dispatchW, dispatchH, static_cast<float>(now));

        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampQueryPool_, 1);

        VkImageMemoryBarrier postComputeBarrier{};
        postComputeBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        postComputeBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        postComputeBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        postComputeBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        postComputeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        postComputeBarrier.image = hdrOutputImage_;
        postComputeBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &postComputeBarrier);

        endSubmitAndWait(cmd);

        uint64_t timestamps[2]{};
        if (vkGetQueryPoolResults(rtx().device, timestampQueryPool_, 0, 2,
                                  sizeof(timestamps), timestamps, sizeof(uint64_t),
                                  VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) == VK_SUCCESS) {
            double gpuTimeMs = static_cast<double>(timestamps[1] - timestamps[0]) * timestampPeriodNs_ / 1'000'000.0;
            double alpha = (gpuTimeMs > smoothedGpuTimeMs_) ? 0.65 : 0.25;
            smoothedGpuTimeMs_ = (1.0 - alpha) * smoothedGpuTimeMs_ + alpha * gpuTimeMs;
        }

        if (Options::Rendering::EnableAdaptiveResolution) {
            adjustAdaptiveScale(now);
        } else {
            adaptiveScale_ = 1.0f;
        }

        VkCommandBuffer blitCmd = beginTransientCommandBuffer();
        if (!blitCmd) return;

        VkImage swapImg = Swapchain::images[imageIndex];

        VkImageMemoryBarrier swapBarrier{};
        swapBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        swapBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        swapBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        swapBarrier.srcAccessMask = 0;
        swapBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        swapBarrier.image = swapImg;
        swapBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCmdPipelineBarrier(blitCmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &swapBarrier);

        VkClearColorValue clearBlack = { .float32 = {0.0f, 0.0f, 0.0f, 1.0f} };
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(blitCmd, swapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearBlack, 1, &range);

        VkExtent2D swapExtent = Swapchain::getExtent();

        VkImageBlit flipBlit{};
        flipBlit.srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        flipBlit.srcOffsets[0] = { 0, 0, 0 };
        flipBlit.srcOffsets[1] = { render_width_, render_height_, 1 };
        flipBlit.dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
        flipBlit.dstOffsets[0] = { 0, static_cast<int32_t>(swapExtent.height), 0 };
        flipBlit.dstOffsets[1] = { static_cast<int32_t>(swapExtent.width), 0, 1 };

        vkCmdBlitImage(blitCmd,
                       hdrOutputImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       swapImg,         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &flipBlit,
                       VK_FILTER_LINEAR);

        VkImageMemoryBarrier presentBarrier{};
        presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        presentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        presentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        presentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        presentBarrier.image = swapImg;
        presentBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCmdPipelineBarrier(blitCmd,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &presentBarrier);

        endSubmitAndWait(blitCmd);

        VkPresentInfoKHR pi{};
        pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.swapchainCount = 1;
        pi.pSwapchains    = &Swapchain::swapchain.value;
        pi.pImageIndices  = &imageIndex;

        VkResult pres = ext().vkQueuePresentKHR(rtx().present_queue, &pi);

        if (pres == VK_SUCCESS) {
            Swapchain::updateRefreshEstimate(TotalTime::get().seconds());
            measuredRefreshRateHz_ = static_cast<float>(1.0 / Swapchain::getSmoothedRefresh());
        } else if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
            needsRecreate_ = true;
        } else if (pres != VK_SUCCESS) {
            LOG_ERROR_CAT("SWAPCHAIN", "Present failed: {}", vkh.result(pres));
            if (pres == VK_ERROR_SURFACE_LOST_KHR) destroyed_ = true;
        }

        // Periodic status log (every 5 seconds)
        if (now - lastFpsLog_ >= 5.0) {
            double elapsed = now - lastFpsLog_;
            double avgFps = (frameCount_ > 0) ? static_cast<double>(frameCount_) / elapsed : 0.0;
            double avgDt_us = (frameCount_ > 0) ? (elapsed * 1000000.0) / static_cast<double>(frameCount_) : 0.0;

            int winW = window_width_;
            int winH = window_height_;

            float scaleFactor = (winW > 0) ? static_cast<float>(render_width_) / static_cast<float>(winW) : 1.0f;
            const char* mode = (scaleFactor < 0.98f) ? "SUBSAMPLING" :
                               (scaleFactor > 1.02f) ? "SUPERSAMPLING" : "NATIVE";

            float targetFrameMs = 1000.0f / (measuredRefreshRateHz_ * 1.18f);
            float gpuLoadPercent = (targetFrameMs > 0.001f)
                                 ? static_cast<float>(smoothedGpuTimeMs_ / targetFrameMs) * 100.0f
                                 : 0.0f;

            const char* stateEmoji = minimized_                       ? "🟥 minimized" :
                                     (!Swapchain::get() || !hdrOutputImage_) ? "⚠️ invalid" :
                                     "✅ active";

            LOG_AMOURANTH("───────────────────────────────────────────────────────────────\n"
                          "              RayCanvas Status  •  t+{:.1f}s\n"
                          "  FPS:            {:.1f}     (avg frame {:.0f} µs)\n"
                          "  Refresh Rate:   {:.1f} Hz\n"
                          "  Window:         {} x {}\n"
                          "  Rendered:       {} x {}     ({:.2f}x — {})\n"
                          "  Adaptive scale: {:.2f}x\n"
                          "  GPU load:       {:.1f}%   (smoothed {:.1f} ms)\n"
                          "  Render Path:    Pure Raymarched 3D\n"
                          "  State:          {}\n"
                          "  Adaptive qual:  {}\n"
                          "  Accumulation:   {}\n"
                          "  Frames this log: {}\n"
                          "───────────────────────────────────────────────────────────────",
                          now * 0.1, avgFps, avgDt_us, measuredRefreshRateHz_,
                          winW, winH, render_width_, render_height_, scaleFactor, mode,
                          adaptiveScale_, gpuLoadPercent, smoothedGpuTimeMs_,
                          stateEmoji,
                          Options::Rendering::EnableAdaptiveResolution ? "enabled" : "disabled",
                          Options::Rendering::ACCUMULATION ? "on" : "off",
                          frameCount_);
            lastFpsLog_  = now;
            frameCount_  = 0;
        }
    }

    void onResize(int newWidth, int newHeight, bool fromUserResize = false) noexcept {
        if (newWidth <= 0 || newHeight <= 0) {
            minimized_ = true;
            return;
        }

        if (newWidth == window_width_ && newHeight == window_height_ && !needsRecreate_) return;

        if (fromUserResize) vkDeviceWaitIdle(rtx().device);

        destroyHDRResources();

        window_width_  = newWidth;
        window_height_ = newHeight;
        minimized_ = false;

        Swapchain::recreate(window_width_, window_height_);

        int actualW = 0, actualH = 0;
        SDL_GetWindowSizeInPixels(window_, &actualW, &actualH);
        window_width_  = actualW;
        window_height_ = actualH;

        updateRenderResolution();

        createPersistentHDR();
        createPreviousHDR();

        if (descriptorSet_ != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(rtx().device, descriptorPool_, 1, &descriptorSet_);
            descriptorSet_ = VK_NULL_HANDLE;
        }
        if (descriptorPool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(rtx().device, descriptorPool_, nullptr);
            descriptorPool_ = VK_NULL_HANDLE;
        }

        createDescriptorPoolAndSet();
        updateDescriptorSet();

        VkClearColorValue clearBlack = { .float32 = {0.0f, 0.0f, 0.0f, 1.0f} };
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkCommandBuffer cmd = beginTransientCommandBuffer();
        if (cmd) {
            transitionImageLayout(cmd, hdrOutputImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
            transitionImageLayout(cmd, prevHdrOutputImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
            vkCmdClearColorImage(cmd, hdrOutputImage_, VK_IMAGE_LAYOUT_GENERAL, &clearBlack, 1, &range);
            vkCmdClearColorImage(cmd, prevHdrOutputImage_, VK_IMAGE_LAYOUT_GENERAL, &clearBlack, 1, &range);
            endSubmitAndWait(cmd);
        }
    }

    int  getWidth()  const noexcept { return window_width_; }
    int  getHeight() const noexcept { return window_height_; }
    bool isMinimized() const noexcept { return minimized_; }
    bool isDestroyed() const noexcept { return destroyed_; }

private:
    void adjustAdaptiveScale(double now) noexcept {
        double elapsed = now - lastAdaptiveAdjustTime_;
        if (elapsed < 1.0) return;

        float targetFrameMs = 1000.0f / (measuredRefreshRateHz_ * 1.18f);
        float gpuLoadPercent = (targetFrameMs > 0.001f)
                             ? static_cast<float>(smoothedGpuTimeMs_ / targetFrameMs) * 100.0f
                             : 0.0f;

        float targetScale = adaptiveScale_;

        constexpr float CRITICAL = 150.0f;
        constexpr float SEVERE   = 220.0f;

        float downMult = 0.82f;
        float upMult   = (postResizeGraceFrames_ > 0) ? 1.05f : 1.18f;

        bool shouldAdjust = false;

        if (gpuLoadPercent > SEVERE) {
            downMult = 0.55f; shouldAdjust = true;
        } else if (gpuLoadPercent > CRITICAL) {
            downMult = 0.68f; shouldAdjust = true;
        } else if (gpuLoadPercent > Options::Rendering::MaxGPULoadPercent) {
            shouldAdjust = true;
        } else if (gpuLoadPercent < Options::Rendering::MaxGPULoadPercent * 0.80f) {
            targetScale *= upMult;
            shouldAdjust = true;
        }

        if (gpuLoadPercent > Options::Rendering::MaxGPULoadPercent) {
            targetScale *= downMult;
            if (gpuLoadPercent > 280.0f) targetScale = std::min(targetScale, 0.40f);
        }

        targetScale = std::clamp(targetScale,
                                 Options::Rendering::MinResolutionScale,
                                 Options::Rendering::MaxResolutionScale);

        float effectiveHysteresis = (targetScale > adaptiveScale_)
                                  ? 0.035f
                                  : Options::Rendering::ResolutionAdjustHysteresis;

        if (shouldAdjust && std::abs(targetScale - adaptiveScale_) > effectiveHysteresis) {
            adaptiveScale_ = targetScale;
            needsRecreate_ = true;
        }

        adaptiveFrameCount_ = 0;
        lastAdaptiveAdjustTime_ = now;
    }

    void updateRenderResolution() noexcept {
        float scale = Options::Rendering::EnableAdaptiveResolution ? adaptiveScale_ : 1.0f;

        float maxW = static_cast<float>(Options::Rendering::INTERNAL_WIDTH)  / static_cast<float>(window_width_);
        float maxH = static_cast<float>(Options::Rendering::INTERNAL_HEIGHT) / static_cast<float>(window_height_);
        scale = std::min(scale, std::min(maxW, maxH));

        render_width_  = static_cast<int>(std::round(static_cast<float>(window_width_)  * scale));
        render_height_ = static_cast<int>(std::round(static_cast<float>(window_height_) * scale));
    }

    void createTimestampQueryPool() noexcept {
        VkQueryPoolCreateInfo qci{};
        qci.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        qci.queryType  = VK_QUERY_TYPE_TIMESTAMP;
        qci.queryCount = 2;

        vkh.checker(vkCreateQueryPool(rtx().device, &qci, nullptr, &timestampQueryPool_),
                    "QUERY", "Timestamp Pool");
    }

    void createPersistentHDR() noexcept {
        vkDeviceWaitIdle(rtx().device);
        destroyHDRResources();

        VkImageCreateInfo ci{};
        ci.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.imageType   = VK_IMAGE_TYPE_2D;
        ci.format      = VK_FORMAT_R32G32B32A32_SFLOAT;
        ci.extent      = {static_cast<uint32_t>(render_width_), static_cast<uint32_t>(render_height_), 1};
        ci.mipLevels   = 1;
        ci.arrayLayers = 1;
        ci.samples     = VK_SAMPLE_COUNT_1_BIT;
        ci.tiling      = VK_IMAGE_TILING_OPTIMAL;
        ci.usage       = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        vkh.checker(vkCreateImage(rtx().device, &ci, nullptr, &hdrOutputImage_), "MEMORY", "HDR Image");

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(rtx().device, hdrOutputImage_, &req);

        uint32_t memType = Memory::findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo mai{};
        mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize  = req.size;
        mai.memoryTypeIndex = memType;

        vkh.checker(vkAllocateMemory(rtx().device, &mai, nullptr, &hdrOutputMemory_), "MEMORY", "HDR Memory");

        vkBindImageMemory(rtx().device, hdrOutputImage_, hdrOutputMemory_, 0);

        VkImageViewCreateInfo vi{};
        vi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image            = hdrOutputImage_;
        vi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        vi.format           = VK_FORMAT_R32G32B32A32_SFLOAT;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkh.checker(vkCreateImageView(rtx().device, &vi, nullptr, &hdrOutputView_), "MEMORY", "HDR View");
    }

    void createPreviousHDR() noexcept {
        vkDeviceWaitIdle(rtx().device);

        if (prevHdrOutputView_)   vkDestroyImageView (rtx().device, prevHdrOutputView_,   nullptr);
        if (prevHdrOutputImage_)  vkDestroyImage     (rtx().device, prevHdrOutputImage_,  nullptr);
        if (prevHdrOutputMemory_) vkFreeMemory       (rtx().device, prevHdrOutputMemory_, nullptr);

        prevHdrOutputView_   = VK_NULL_HANDLE;
        prevHdrOutputImage_  = VK_NULL_HANDLE;
        prevHdrOutputMemory_ = VK_NULL_HANDLE;

        VkImageCreateInfo ci{};
        ci.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.imageType   = VK_IMAGE_TYPE_2D;
        ci.format      = VK_FORMAT_R32G32B32A32_SFLOAT;
        ci.extent      = {static_cast<uint32_t>(render_width_), static_cast<uint32_t>(render_height_), 1};
        ci.mipLevels   = 1;
        ci.arrayLayers = 1;
        ci.samples     = VK_SAMPLE_COUNT_1_BIT;
        ci.tiling      = VK_IMAGE_TILING_OPTIMAL;
        ci.usage       = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        vkh.checker(vkCreateImage(rtx().device, &ci, nullptr, &prevHdrOutputImage_), "MEMORY", "Prev HDR");

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(rtx().device, prevHdrOutputImage_, &req);

        uint32_t memType = Memory::findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo mai{};
        mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize  = req.size;
        mai.memoryTypeIndex = memType;

        vkh.checker(vkAllocateMemory(rtx().device, &mai, nullptr, &prevHdrOutputMemory_), "MEMORY", "Prev HDR Memory");

        vkBindImageMemory(rtx().device, prevHdrOutputImage_, prevHdrOutputMemory_, 0);

        VkImageViewCreateInfo vi{};
        vi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image            = prevHdrOutputImage_;
        vi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        vi.format           = VK_FORMAT_R32G32B32A32_SFLOAT;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkh.checker(vkCreateImageView(rtx().device, &vi, nullptr, &prevHdrOutputView_), "MEMORY", "Prev HDR View");
    }

    void destroyHDRResources() noexcept {
        if (hdrOutputView_)   vkDestroyImageView (rtx().device, hdrOutputView_,   nullptr);
        if (hdrOutputImage_)  vkDestroyImage     (rtx().device, hdrOutputImage_,  nullptr);
        if (hdrOutputMemory_) vkFreeMemory       (rtx().device, hdrOutputMemory_, nullptr);

        if (prevHdrOutputView_)   vkDestroyImageView (rtx().device, prevHdrOutputView_,   nullptr);
        if (prevHdrOutputImage_)  vkDestroyImage     (rtx().device, prevHdrOutputImage_,  nullptr);
        if (prevHdrOutputMemory_) vkFreeMemory       (rtx().device, prevHdrOutputMemory_, nullptr);

        hdrOutputView_     = VK_NULL_HANDLE;
        hdrOutputImage_    = VK_NULL_HANDLE;
        hdrOutputMemory_   = VK_NULL_HANDLE;

        prevHdrOutputView_   = VK_NULL_HANDLE;
        prevHdrOutputImage_  = VK_NULL_HANDLE;
        prevHdrOutputMemory_ = VK_NULL_HANDLE;
    }

    void createDescriptorPoolAndSet() noexcept {
        VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   2},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  6}
        };

        VkDescriptorPoolCreateInfo pci{};
        pci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pci.maxSets       = 1;
        pci.poolSizeCount = std::size(poolSizes);
        pci.pPoolSizes    = poolSizes;

        vkh.checker(vkCreateDescriptorPool(rtx().device, &pci, nullptr, &descriptorPool_),
                    "DESCRIPTOR", "RayCanvas");

        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = descriptorPool_;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &Pipeline::main_descriptor_layout;

        vkh.checker(vkAllocateDescriptorSets(rtx().device, &ai, &descriptorSet_),
                    "DESCRIPTOR", "RayCanvas");
    }

    void updateDescriptorSet() noexcept {
        if (!hdrOutputView_ || !prevHdrOutputView_) return;

        std::array<VkWriteDescriptorSet, 3> writes{};

        VkDescriptorImageInfo imgInfo{};
        imgInfo.imageView   = hdrOutputView_;
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        writes[0].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet           = descriptorSet_;
        writes[0].dstBinding       = 0;
        writes[0].descriptorCount  = 1;
        writes[0].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[0].pImageInfo       = &imgInfo;

        VkDescriptorImageInfo prevImgInfo{};
        prevImgInfo.imageView   = prevHdrOutputView_;
        prevImgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        writes[1].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet           = descriptorSet_;
        writes[1].dstBinding       = 1;
        writes[1].descriptorCount  = 1;
        writes[1].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[1].pImageInfo       = &prevImgInfo;

        VkDescriptorBufferInfo matInfo{};
        matInfo.buffer = Memory::getBuffer(materialsHandle_);
        matInfo.offset = 0;
        matInfo.range  = VK_WHOLE_SIZE;

        writes[2].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet           = descriptorSet_;
        writes[2].dstBinding       = 4;
        writes[2].descriptorCount  = 1;
        writes[2].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[2].pBufferInfo      = &matInfo;

        vkUpdateDescriptorSets(rtx().device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    VkCommandBuffer beginTransientCommandBuffer() noexcept {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo alloc{};
        alloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.commandPool        = rtx().transient_pool;
        alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;

        VkResult res = vkAllocateCommandBuffers(rtx().device, &alloc, &cmd);
        if (res != VK_SUCCESS) return VK_NULL_HANDLE;

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        res = vkBeginCommandBuffer(cmd, &begin);
        if (res != VK_SUCCESS) {
            vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &cmd);
            return VK_NULL_HANDLE;
        }

        return cmd;
    }

    void endSubmitAndWait(VkCommandBuffer cmd) noexcept {
        if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
            vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &cmd);
            return;
        }

        VkFence fence = VK_NULL_HANDLE;
        VkFenceCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        vkCreateFence(rtx().device, &fci, nullptr, &fence);

        VkSubmitInfo submit{};
        submit.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers    = &cmd;

        vkQueueSubmit(rtx().graphics_queue, 1, &submit, fence);
        vkWaitForFences(rtx().device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(rtx().device, fence, nullptr);
        vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &cmd);
    }

    void transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout) noexcept {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        }

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0,
                             0, nullptr,
                             0, nullptr,
                             1, &barrier);
    }

    void clearHDRImages() noexcept {
        VkClearColorValue clearBlack = { .float32 = {0.0f, 0.0f, 0.0f, 1.0f} };
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkCommandBuffer cmd = beginTransientCommandBuffer();
        if (cmd) {
            vkCmdClearColorImage(cmd, hdrOutputImage_,   VK_IMAGE_LAYOUT_GENERAL, &clearBlack, 1, &range);
            vkCmdClearColorImage(cmd, prevHdrOutputImage_, VK_IMAGE_LAYOUT_GENERAL, &clearBlack, 1, &range);
            endSubmitAndWait(cmd);
        }
    }

    void buildMaterialLibrary() {
        std::vector<Material> materials;

        // Use the new full constexpr array from Materials.hpp
        materials.reserve(MAT_COUNT);

        for (size_t i = 0; i < MAT_COUNT; ++i) {
            if (Materials::AllMaterials[i].layerCount > 0) {
                materials.push_back(Materials::AllMaterials[i]);
            }
        }

        // Optional: log how many we actually loaded
        LOG_DEBUG_CAT("MATERIALS", "Loading {} materials from constexpr array", materials.size());

        VkDeviceSize size = materials.size() * sizeof(Material);

        materialsHandle_ = Memory::createBuffer(
            size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            "MaterialsLibrary",
            Memory::MemoryHint::HostVisible
        );

        if (size > 0) {
            auto [staging, mem] = Memory::uploadToBuffer(materialsHandle_, materials.data(), size);
            if (staging) {
                vkDestroyBuffer(rtx().device, staging, nullptr);
                vkFreeMemory(rtx().device, mem, nullptr);
            }
        }

        LOG_SUCCESS_CAT("RAYCANVAS", "Material library built ({} materials loaded)", materials.size());
    }

private:
    SDL_Window*    window_                    = nullptr;
    int            window_width_              = 0;
    int            window_height_             = 0;
    int            render_width_              = 0;
    int            render_height_             = 0;

    bool           minimized_                 = false;
    bool           destroyed_                 = false;
    bool           firstFrame_                = true;

    uint64_t       materialsHandle_           = 0;

    VkImage        hdrOutputImage_            = VK_NULL_HANDLE;
    VkImageView    hdrOutputView_             = VK_NULL_HANDLE;
    VkDeviceMemory hdrOutputMemory_           = VK_NULL_HANDLE;

    VkImage        prevHdrOutputImage_        = VK_NULL_HANDLE;
    VkImageView    prevHdrOutputView_         = VK_NULL_HANDLE;
    VkDeviceMemory prevHdrOutputMemory_       = VK_NULL_HANDLE;

    VkDescriptorPool descriptorPool_          = VK_NULL_HANDLE;
    VkDescriptorSet  descriptorSet_           = VK_NULL_HANDLE;

    float          adaptiveScale_             = 1.0f;
    float          lastAppliedScale_          = 1.0f;

    double         lastPresentTime_s_         = 0.0;
    float          measuredRefreshRateHz_     = 60.0f;
    double         lastFpsLog_                = 0.0;
    uint64_t       frameCount_                = 0;
    double         lastAdaptiveAdjustTime_    = 0.0;
    uint64_t       adaptiveFrameCount_        = 0;

    bool           needsRecreate_             = false;
    bool           justResizedThisFrame_      = false;
    int            postResizeGraceFrames_     = 0;

    VkQueryPool    timestampQueryPool_        = VK_NULL_HANDLE;
    double         timestampPeriodNs_         = 1.0;
    double         smoothedGpuTimeMs_         = 16.67;
};

inline RayCanvas* rayCanvas = nullptr;