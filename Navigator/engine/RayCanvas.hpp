#pragma once

// =============================================================================
// AMOURANTH RTX Engine — RayCanvas (HYBRID: raymarched compute + full hardware RT with SBT)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
// =============================================================================

#include "AMOURANTHRTX.hpp"
#include "ELLIE.hpp"
#include "Camera.hpp"
#include "OptionsMenu.hpp"
#include "Pipeline.hpp"
#include "Materials.hpp"
#include "SDL3.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/rotate_vector.hpp>

#include <vector>
#include <cstdint>
#include <array>
#include <algorithm>
#include <cmath>

// =============================================================================
// RayCanvas — main hybrid renderer class
// =============================================================================
class RayCanvas {
public:
    RayCanvas(int initialWidth, int initialHeight, SDL_Window* window)
        : window_(window),
          window_width_(initialWidth),
          window_height_(initialHeight),
          render_width_(initialWidth),
          render_height_(initialHeight),
          minimized_(false),
          destroyed_(false),
          firstFrame_(true),
          materialsHandle_(0),
          tlas_(VK_NULL_HANDLE),
          hdrOutputImage_(VK_NULL_HANDLE),
          hdrOutputView_(VK_NULL_HANDLE),
          hdrOutputMemory_(VK_NULL_HANDLE),
          prevHdrOutputImage_(VK_NULL_HANDLE),
          prevHdrOutputView_(VK_NULL_HANDLE),
          prevHdrOutputMemory_(VK_NULL_HANDLE),
          descriptorPool_(VK_NULL_HANDLE),
          descriptorSet_(VK_NULL_HANDLE),
          adaptiveScale_(1.0),
          lastPresentTime_s_(0.0),
          measuredRefreshRateHz_(60.0),
          lastFpsLog_(0.0),
          frameCount_(0),
          lastAdaptiveAdjustTime_(0.0),
          needsRecreate_(false),
          timestampQueryPool_(VK_NULL_HANDLE),
          timestampPeriodNs_(1.0),
          smoothedGpuTimeMs_(16.67)
    {
        if (!Swapchain::get()) {
            LOG_FATAL_CAT("RAYCANVAS", "No valid swapchain — navigator must create it first");
            std::abort();
        }

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(rtx().physical, &props);
        timestampPeriodNs_ = props.limits.timestampPeriod;

        Pipeline::create_pipeline_layout();
        Pipeline::create_canvas_pipeline();

        createTimestampQueryPool();
        buildMaterialLibrary();
        updateRenderResolution();
        createPersistentHDR();
        createPreviousHDR();
        createDescriptorPoolAndSet();
        updateDescriptorSet();
        clearHDRImages();

        lastAdaptiveAdjustTime_ = TotalTime::get().seconds();

        LOG_SUCCESS_CAT("RAYCANVAS", "Initialized hybrid renderer — {}x{} render target ready (SBT + ray tracing ready when TLAS + shaders provided)", 
                        render_width_, render_height_);
    }

    ~RayCanvas() {
        if (destroyed_) return;
        destroyed_ = true;

        vkDeviceWaitIdle(rtx().device);

        if (tlas_ != VK_NULL_HANDLE) {
            ext().vkDestroyAccelerationStructureKHR(rtx().device, tlas_, nullptr);
            tlas_ = VK_NULL_HANDLE;
        }

        if (timestampQueryPool_ != VK_NULL_HANDLE) vkDestroyQueryPool(rtx().device, timestampQueryPool_, nullptr);
        if (descriptorSet_ != VK_NULL_HANDLE)      vkFreeDescriptorSets(rtx().device, descriptorPool_, 1, &descriptorSet_);
        if (descriptorPool_ != VK_NULL_HANDLE)     vkDestroyDescriptorPool(rtx().device, descriptorPool_, nullptr);

        destroyHDRResources();
        Memory::destroy(materialsHandle_);

        LOG_SUCCESS_CAT("RAYCANVAS", "Hybrid renderer destroyed (compute + RT + SBT cleaned up)");
    }

    bool maybeUpdateCanvas(bool isRunning) noexcept {
        if (destroyed_) {
            isRunning = false;
            return isRunning;
        }

        frameCount_++;
        double now = TotalTime::get().seconds();

        int currentW = window_width_;
        int currentH = window_height_;
        bool quit = false;
        bool fullscreen_toggle = false;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            INPUT.pump(ev);

            if (ev.type == SDL_EVENT_QUIT) {
                quit = true;
            }

            if (ev.type == SDL_EVENT_WINDOW_RESIZED) {
                currentW = ev.window.data1;
                currentH = ev.window.data2;
            }

            if (ev.type == SDL_EVENT_KEY_DOWN) {
                bool altPressed = (ev.key.mod & SDL_KMOD_ALT) != 0;

                // Fullscreen (F8, F11, or Alt+Enter)
                if (ev.key.scancode == SDL_SCANCODE_F11 ||
                    (ev.key.scancode == SDL_SCANCODE_RETURN && altPressed) ||
                    ev.key.scancode == SDL_SCANCODE_F8) {
                    fullscreen_toggle = true;
                }

                if (ev.key.scancode == SDL_SCANCODE_F1) {
                    toggleAdaptiveResolution();
                    needsRecreate_ = true;
                }
                if (ev.key.scancode == SDL_SCANCODE_F2) {
                    toggleRayTracing();
                    needsRecreate_ = true;
                }
                if (ev.key.scancode == SDL_SCANCODE_F3) {
                    toggleRTXGI();
                    needsRecreate_ = true;
                }
                if (ev.key.scancode == SDL_SCANCODE_F4) {
                    toggleAccumulation();
                    needsRecreate_ = true;
                }
                if (ev.key.scancode == SDL_SCANCODE_F5) {
                    toggleTonemapping();
                }
                if (ev.key.scancode == SDL_SCANCODE_F6) {
                    toggleBloom();
                }
                if (ev.key.scancode == SDL_SCANCODE_F7) {
                    resetCamera();
                }
            }
        }

        if (fullscreen_toggle) toggleFullscreen();

        if (quit) {
            destroyed_ = true;
            LOG_INFO_CAT("RAYCANVAS", "Quit signal received");
            return isRunning;
        }

        bool nowMinimized = (currentW <= 0 || currentH <= 0);
        if (nowMinimized) {
            minimized_ = true;
            return isRunning;
        }

        bool sizeChanged = (currentW != window_width_) || (currentH != window_height_);

        if (minimized_ || sizeChanged || needsRecreate_) {
            onResize(currentW, currentH, sizeChanged);
            minimized_ = false;
            return isRunning;
        }

        if (!Swapchain::get() || !hdrOutputImage_ || !hdrOutputView_) return isRunning;

        if (firstFrame_) {
            TotalTime::get().seal();
            firstFrame_ = false;
            LOG_AMOURANTH("Hybrid RTX engine sealed — rendering begins 💖");
            lastAdaptiveAdjustTime_ = now;
        }

        static double lastKnownTime = 0.0;
        if (now <= lastKnownTime) now = lastKnownTime + Swapchain::smoothedRefresh_s;
        lastKnownTime = now;

        // Acquire swapchain image
        uint32_t imageIndex = 0;
        VkFence fence = VK_NULL_HANDLE;
        VkFenceCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vkCreateFence(rtx().device, &fci, nullptr, &fence);

        VkResult acq = ext().vkAcquireNextImageKHR(rtx().device, Swapchain::get(), UINT64_MAX, VK_NULL_HANDLE, fence, &imageIndex);
        vkWaitForFences(rtx().device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(rtx().device, fence, nullptr);

        if (acq == VK_ERROR_OUT_OF_DATE_KHR || acq == VK_SUBOPTIMAL_KHR) {
            needsRecreate_ = true;
            return isRunning;
        }
        if (acq != VK_SUCCESS) {
            LOG_ERROR_CAT("SWAPCHAIN", "Acquire failed: {}", vkh.result(acq));
            if (acq == VK_ERROR_SURFACE_LOST_KHR || acq == VK_ERROR_DEVICE_LOST) destroyed_ = true;
            return isRunning;
        }

        // Main rendering pass
        VkCommandBuffer cmd = beginTransientCommandBuffer();
        if (!cmd) return isRunning;

        vkCmdResetQueryPool(cmd, timestampQueryPool_, 0, 2);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, timestampQueryPool_, 0);

        transitionImageLayout(cmd, hdrOutputImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        VkDescriptorSet set = descriptorSet_;
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline::pipeline_layout, 0, 1, &set, 0, nullptr);

        updateRenderResolution();
        Pipeline::dispatch_canvas(cmd, render_width_, render_height_, static_cast<float>(now));

        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampQueryPool_, 1);

        // Barrier before blit
        VkImageMemoryBarrier postComputeBarrier{};
        postComputeBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        postComputeBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        postComputeBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        postComputeBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        postComputeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        postComputeBarrier.image = hdrOutputImage_;
        postComputeBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &postComputeBarrier);
        endSubmitAndWait(cmd);

        // GPU timing
        uint64_t timestamps[2]{};
        if (vkGetQueryPoolResults(rtx().device, timestampQueryPool_, 0, 2, sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT) == VK_SUCCESS) {
            double gpuTimeMs = static_cast<double>(timestamps[1] - timestamps[0]) * timestampPeriodNs_ / 1000000.0;
            double alpha = (gpuTimeMs > smoothedGpuTimeMs_) ? 0.70 : 0.22;
            smoothedGpuTimeMs_ = (1.0 - alpha) * smoothedGpuTimeMs_ + alpha * gpuTimeMs;
        }

        if (Options::Rendering::EnableAdaptiveResolution) {
            adjustAdaptiveScale(now);
        } else {
            adaptiveScale_ = 1.0;
        }

        // Blit to swapchain with vertical flip
        VkCommandBuffer blitCmd = beginTransientCommandBuffer();
        if (!blitCmd) return isRunning;

        VkImage swapImg = Swapchain::images[imageIndex];

        VkImageMemoryBarrier swapBarrier{};
        swapBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        swapBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        swapBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        swapBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        swapBarrier.image = swapImg;
        swapBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &swapBarrier);

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

        vkCmdBlitImage(blitCmd, hdrOutputImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       swapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &flipBlit, VK_FILTER_LINEAR);

        VkImageMemoryBarrier presentBarrier{};
        presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        presentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        presentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        presentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        presentBarrier.image = swapImg;
        presentBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCmdPipelineBarrier(blitCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &presentBarrier);
        endSubmitAndWait(blitCmd);

        // Present
        VkPresentInfoKHR pi{};
        pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.swapchainCount = 1;
        pi.pSwapchains = &Swapchain::swapchain.value;
        pi.pImageIndices = &imageIndex;

        VkResult pres = ext().vkQueuePresentKHR(rtx().present_queue, &pi);

        if (pres == VK_SUCCESS) {
            Swapchain::updateRefreshEstimate(TotalTime::get().seconds());
            measuredRefreshRateHz_ = 1.0 / Swapchain::getSmoothedRefresh();
        } else if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
            needsRecreate_ = true;
        } else if (pres != VK_SUCCESS) {
            LOG_ERROR_CAT("SWAPCHAIN", "Present failed: {}", vkh.result(pres));
            if (pres == VK_ERROR_SURFACE_LOST_KHR) destroyed_ = true;
        }

        // Periodic status log every 5 seconds
        if (now - lastFpsLog_ >= 5.0) {
            double elapsed        = now - lastFpsLog_;
            double avgFps         = frameCount_ > 0 ? static_cast<double>(frameCount_) / elapsed : 0.0;
            double avgDt_us       = frameCount_ > 0 ? (elapsed * 1000000.0) / static_cast<double>(frameCount_) : 0.0;

            int winW              = window_width_;
            int winH              = window_height_;

            double scaleFactor    = winW > 0 ? static_cast<double>(render_width_) / winW : 1.0;
            const char* mode      = scaleFactor < 0.98 ? "SUBSAMPLING" : scaleFactor > 1.02 ? "SUPERSAMPLING" : "NATIVE";

            double targetFrameMs  = 1000.0 / measuredRefreshRateHz_;
            double gpuLoadPercent = targetFrameMs > 0.001 ? (smoothedGpuTimeMs_ / targetFrameMs) * 100.0 : 0.0;

            VRAMReality vram = Memory::measureReality();

            double usedMB         = static_cast<double>(vram.driver_footprint) / (1024.0 * 1024.0);
            double totalMB        = static_cast<double>(vram.total) / (1024.0 * 1024.0);
            double freePercent    = totalMB > 0 ? 100.0 * (1.0 - usedMB / totalMB) : 0.0;

            LOG_AMOURANTH("───────────────────────────────────────────────────────────────\n"
                          "              RayCanvas Status  •  t+{:.4}s   (HYBRID RTX)\n"
                          "  FPS:            {}     (avg frame {} µs)\n"
                          "  Refresh Rate:   {} Hz\n"
                          "  Window:         {} x {}\n"
                          "  Rendered:       {} x {}     ({:.2f}x — {})\n"
                          "  Adaptive scale: {:.2f}x\n"
                          "  GPU load:       {:.3f}%   (smoothed {:.3f} ms)\n"
                          "  Features:       Adaptive {}  Accumulation {}  Supersample {}\n"
                          "  Advanced:       HardwareRayTracing {}  RTXReflections {}  RTXGI {}\n"
                          "  VRAM:           {:.3f} MB used / {:.3f} MB total ({:.3f}% free)\n"
                          "  Frames this log: {}\n"
                          "───────────────────────────────────────────────────────────────",
                          now, avgFps, avgDt_us, measuredRefreshRateHz_,
                          winW, winH, render_width_, render_height_, scaleFactor, mode,
                          adaptiveScale_, gpuLoadPercent, smoothedGpuTimeMs_,
                          Options::Rendering::EnableAdaptiveResolution ? "💖" : "❌",
                          Options::Rendering::EnableAccumulation ? "💖" : "❌",
                          scaleFactor > 1.0 ? "💖" : "❌",
                          Options::Rendering::EnableHardwareRayTracing ? "💖" : "❌",
                          Options::Rendering::EnableRTXReflections ? "💖" : "❌",
                          Options::Rendering::EnableRTXGI ? "💖" : "❌",                          
                          usedMB, totalMB, freePercent,
                          frameCount_);
            lastFpsLog_ = now;
            frameCount_ = 0;
        }

        return isRunning;
    }

    // Public toggles
    void toggleAdaptiveResolution() noexcept {
        Options::Rendering::EnableAdaptiveResolution = !Options::Rendering::EnableAdaptiveResolution;
        needsRecreate_ = true;
        LOG_INFO_CAT("RENDER", "Adaptive resolution toggled: {}", 
                     Options::Rendering::EnableAdaptiveResolution ? "ON 💖" : "OFF");
    }

    void toggleRayTracing() noexcept {
        Options::Rendering::EnableHardwareRayTracing = !Options::Rendering::EnableHardwareRayTracing;
        needsRecreate_ = true;
        LOG_INFO_CAT("RENDER", "Hardware Ray Tracing toggled: {}", 
                     Options::Rendering::EnableHardwareRayTracing ? "ON 💖" : "OFF");
    }

    void toggleRTXGI() noexcept {
        Options::Rendering::EnableRTXGI = !Options::Rendering::EnableRTXGI;
        needsRecreate_ = true;
        LOG_INFO_CAT("RENDER", "RTX GI toggled: {}", 
                     Options::Rendering::EnableRTXGI ? "ON 💖" : "OFF");
    }

    void toggleAccumulation() noexcept {
        Options::Rendering::EnableAccumulation = !Options::Rendering::EnableAccumulation;
        needsRecreate_ = true;
        LOG_INFO_CAT("RENDER", "Accumulation toggled: {}", 
                     Options::Rendering::EnableAccumulation ? "ON 💖" : "OFF");
    }

    void toggleTonemapping() noexcept {
        Options::Rendering::EnableTonemapping = !Options::Rendering::EnableTonemapping;
        LOG_INFO_CAT("RENDER", "Tonemapping toggled: {}", 
                     Options::Rendering::EnableTonemapping ? "ON 💖" : "OFF");
    }

    void toggleBloom() noexcept {
        Options::Rendering::BloomIntensity = (Options::Rendering::BloomIntensity > 0.01f) ? 0.0f : 0.35f;
        LOG_INFO_CAT("RENDER", "Bloom toggled: {}", 
                     Options::Rendering::BloomIntensity > 0.01f ? "ON 💖" : "OFF");
    }

    void resetCamera() noexcept {
        CAM.setPosition(Options::Camera::StartPosition);
        LOG_INFO_CAT("CAMERA", "Camera reset to default spawn position 💖");
    }

    [[nodiscard]] int  getWidth()  const noexcept { return window_width_; }
    [[nodiscard]] int  getHeight() const noexcept { return window_height_; }
    [[nodiscard]] bool isMinimized() const noexcept { return minimized_; }
    [[nodiscard]] bool isDestroyed() const noexcept { return destroyed_; }

private:
    void toggleFullscreen() noexcept {
        const bool wasFullscreen = (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) != 0;
        const bool nowFullscreen = !wasFullscreen;
        SDL_SetWindowFullscreen(window_, nowFullscreen);
        SDL_SetWindowRelativeMouseMode(window_, nowFullscreen);
        LOG_INFO_CAT("WINDOW", "Fullscreen {}", nowFullscreen ? "enabled" : "disabled");
    }

    void onResize(int newWidth, int newHeight, bool fromUserResize) noexcept {
        if (newWidth <= 0 || newHeight <= 0) { 
            minimized_ = true; 
            return; 
        }

        int oldW = window_width_;
        int oldH = window_height_;
        double oldArea = static_cast<double>(oldW) * oldH;

        if (newWidth == oldW && newHeight == oldH && !needsRecreate_) return;
        if (fromUserResize || needsRecreate_) vkDeviceWaitIdle(rtx().device);

        int actualW = 0, actualH = 0;
        SDL_GetWindowSizeInPixels(window_, &actualW, &actualH);
        window_width_  = actualW;
        window_height_ = actualH;

        double newArea = static_cast<double>(window_width_) * window_height_;
        bool windowGrewSignificantly = (newArea > oldArea * 1.08);

        minimized_ = false;
        needsRecreate_ = false;

        Swapchain::recreate(window_width_, window_height_);

        if (windowGrewSignificantly && Options::Rendering::EnableAdaptiveResolution && adaptiveScale_ > 0.35) {
            double areaRatio = oldArea / newArea;
            adaptiveScale_ = std::max(0.42, adaptiveScale_ * areaRatio * 0.72);
            LOG_INFO_CAT("RAYCANVAS", "Window grew significantly — pre-downscaled to {:.2f}x", adaptiveScale_);
        }

        updateRenderResolution();
        destroyHDRResources();
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
        clearHDRImages();

        lastAdaptiveAdjustTime_ = TotalTime::get().seconds() + 1.25;
    }

    void adjustAdaptiveScale(double now) noexcept {
        double elapsed = now - lastAdaptiveAdjustTime_;
        if (elapsed < 1.15) return;

        double targetFrameMs = 1000.0 / measuredRefreshRateHz_;
        double gpuLoadPercent = targetFrameMs > 0.001 
            ? (smoothedGpuTimeMs_ / targetFrameMs) * 100.0 
            : 0.0;

        double targetScale = adaptiveScale_;

        if (gpuLoadPercent > 108.0)      targetScale *= 0.79;
        else if (gpuLoadPercent > 97.0)  targetScale *= 0.88;
        else if (gpuLoadPercent > 89.0)  targetScale *= 0.95;
        else if (gpuLoadPercent < 64.0)  targetScale *= 1.11;
        else if (gpuLoadPercent < 73.0)  targetScale *= 1.055;

        targetScale = std::clamp(targetScale,
            static_cast<double>(Options::Rendering::MinResolutionScale),
            static_cast<double>(Options::Rendering::MaxResolutionScale));

        double hysteresis = (targetScale > adaptiveScale_) ? 0.038 : 0.072;

        if (std::abs(targetScale - adaptiveScale_) > hysteresis) {
            adaptiveScale_ = targetScale;
            needsRecreate_ = true;
        }

        lastAdaptiveAdjustTime_ = now;
    }

    void updateRenderResolution() noexcept {
        if (!Options::Rendering::EnableAdaptiveResolution) {
            render_width_  = window_width_;
            render_height_ = window_height_;
            return;
        }

        double scale = adaptiveScale_;

        int64_t w = static_cast<int64_t>(std::round(static_cast<double>(window_width_) * scale));
        int64_t h = static_cast<int64_t>(std::round(static_cast<double>(window_height_) * scale));

        if (w > 32768) w = 32768;
        if (h > 32768) h = 32768;
        if (w < 1) w = 1;
        if (h < 1) h = 1;

        render_width_  = static_cast<int>(w);
        render_height_ = static_cast<int>(h);
    }

    void createTimestampQueryPool() noexcept {
        VkQueryPoolCreateInfo qci{};
        qci.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        qci.queryType  = VK_QUERY_TYPE_TIMESTAMP;
        qci.queryCount = 2;
        vkCreateQueryPool(rtx().device, &qci, nullptr, &timestampQueryPool_);
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

        vkCreateImage(rtx().device, &ci, nullptr, &hdrOutputImage_);

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(rtx().device, hdrOutputImage_, &req);
        uint32_t memType = Memory::findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo mai{};
        mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize  = req.size;
        mai.memoryTypeIndex = memType;

        vkAllocateMemory(rtx().device, &mai, nullptr, &hdrOutputMemory_);
        vkBindImageMemory(rtx().device, hdrOutputImage_, hdrOutputMemory_, 0);

        VkImageViewCreateInfo vi{};
        vi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image            = hdrOutputImage_;
        vi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        vi.format           = VK_FORMAT_R32G32B32A32_SFLOAT;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCreateImageView(rtx().device, &vi, nullptr, &hdrOutputView_);
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

        vkCreateImage(rtx().device, &ci, nullptr, &prevHdrOutputImage_);

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(rtx().device, prevHdrOutputImage_, &req);
        uint32_t memType = Memory::findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo mai{};
        mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize  = req.size;
        mai.memoryTypeIndex = memType;

        vkAllocateMemory(rtx().device, &mai, nullptr, &prevHdrOutputMemory_);
        vkBindImageMemory(rtx().device, prevHdrOutputImage_, prevHdrOutputMemory_, 0);

        VkImageViewCreateInfo vi{};
        vi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image            = prevHdrOutputImage_;
        vi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        vi.format           = VK_FORMAT_R32G32B32A32_SFLOAT;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCreateImageView(rtx().device, &vi, nullptr, &prevHdrOutputView_);
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
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              2},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             5},
            {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1}
        };

        VkDescriptorPoolCreateInfo pci{};
        pci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pci.maxSets       = 1;
        pci.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
        pci.pPoolSizes    = poolSizes;

        vkCreateDescriptorPool(rtx().device, &pci, nullptr, &descriptorPool_);

        VkDescriptorSetAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        ai.descriptorPool     = descriptorPool_;
        ai.descriptorSetCount = 1;
        ai.pSetLayouts        = &Pipeline::main_descriptor_layout;

        vkAllocateDescriptorSets(rtx().device, &ai, &descriptorSet_);
    }

    void updateDescriptorSet() noexcept {
        if (!hdrOutputView_ || !prevHdrOutputView_) return;

        std::array<VkWriteDescriptorSet, 4> writes{};

        // Binding 0: output HDR
        VkDescriptorImageInfo imgInfo{};
        imgInfo.imageView   = hdrOutputView_;
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        writes[0].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet           = descriptorSet_;
        writes[0].dstBinding       = 0;
        writes[0].descriptorCount  = 1;
        writes[0].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[0].pImageInfo       = &imgInfo;

        // Binding 1: previous frame
        VkDescriptorImageInfo prevImgInfo{};
        prevImgInfo.imageView   = prevHdrOutputView_;
        prevImgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        writes[1].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet           = descriptorSet_;
        writes[1].dstBinding       = 1;
        writes[1].descriptorCount  = 1;
        writes[1].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[1].pImageInfo       = &prevImgInfo;

        // Binding 4: materials
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

        // Binding 7: TLAS
        VkWriteDescriptorSetAccelerationStructureKHR asInfo{};
        asInfo.sType                       = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        asInfo.accelerationStructureCount  = 1;
        asInfo.pAccelerationStructures     = &tlas_;
        writes[3].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[3].dstSet           = descriptorSet_;
        writes[3].dstBinding       = 7;
        writes[3].descriptorCount  = 1;
        writes[3].descriptorType   = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        writes[3].pNext            = &asInfo;

        vkUpdateDescriptorSets(rtx().device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    VkCommandBuffer beginTransientCommandBuffer() noexcept {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo alloc{};
        alloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.commandPool        = rtx().transient_pool;
        alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;
        vkAllocateCommandBuffers(rtx().device, &alloc, &cmd);

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(cmd, &begin);

        return cmd;
    }

    void endSubmitAndWait(VkCommandBuffer cmd) noexcept {
        if (!cmd) return;
        vkEndCommandBuffer(cmd);

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

    void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) noexcept {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        }

        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    void clearHDRImages() noexcept {
        VkClearColorValue clearBlack = { .float32 = {0.0f, 0.0f, 0.0f, 1.0f} };
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkCommandBuffer cmd = beginTransientCommandBuffer();
        if (!cmd) return;

        transitionImageLayout(cmd, hdrOutputImage_,   VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        transitionImageLayout(cmd, prevHdrOutputImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        vkCmdClearColorImage(cmd, hdrOutputImage_,   VK_IMAGE_LAYOUT_GENERAL, &clearBlack, 1, &range);
        vkCmdClearColorImage(cmd, prevHdrOutputImage_, VK_IMAGE_LAYOUT_GENERAL, &clearBlack, 1, &range);

        endSubmitAndWait(cmd);
    }

    void buildMaterialLibrary() noexcept {
        VkDeviceSize size = sizeof(Materials::AllMaterials);

        materialsHandle_ = Memory::createBuffer(
            size,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            "MaterialsLibrary",
            Memory::MemoryHint::HostVisible
        );

        if (size > 0) {
            auto [staging, mem] = Memory::uploadToBuffer(materialsHandle_, Materials::AllMaterials.data(), size);
            if (staging) {
                vkDestroyBuffer(rtx().device, staging, nullptr);
                vkFreeMemory(rtx().device, mem, nullptr);
            }
        }

        LOG_SUCCESS_CAT("RAYCANVAS", "Material library uploaded — {} materials", static_cast<int>(MAT_COUNT));
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
    VkAccelerationStructureKHR tlas_          = VK_NULL_HANDLE;

    VkImage        hdrOutputImage_            = VK_NULL_HANDLE;
    VkImageView    hdrOutputView_             = VK_NULL_HANDLE;
    VkDeviceMemory hdrOutputMemory_           = VK_NULL_HANDLE;

    VkImage        prevHdrOutputImage_        = VK_NULL_HANDLE;
    VkImageView    prevHdrOutputView_         = VK_NULL_HANDLE;
    VkDeviceMemory prevHdrOutputMemory_       = VK_NULL_HANDLE;

    VkDescriptorPool descriptorPool_          = VK_NULL_HANDLE;
    VkDescriptorSet  descriptorSet_           = VK_NULL_HANDLE;

    double         adaptiveScale_             = 1.0;

    double         lastPresentTime_s_         = 0.0;
    double         measuredRefreshRateHz_     = 60.0;
    double         lastFpsLog_                = 0.0;
    uint64_t       frameCount_                = 0;
    double         lastAdaptiveAdjustTime_    = 0.0;

    bool           needsRecreate_             = false;

    VkQueryPool    timestampQueryPool_        = VK_NULL_HANDLE;
    double         timestampPeriodNs_         = 1.0;
    double         smoothedGpuTimeMs_         = 16.67;
};

inline RayCanvas* rayCanvas = nullptr;