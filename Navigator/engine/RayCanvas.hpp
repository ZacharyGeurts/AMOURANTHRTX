#pragma once

// =============================================================================
// AMOURANTH RTX Engine — RayCanvas (persistent compute renderer)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
//
// PHILOSOPHY:
//   - Perfect presentation timing is absolute priority — never miss VSync/present
//   - Aggressive subsampling BELOW window resolution when GPU load is high
//   - If GPU usage > MaxGPULoadPercent (default 90%), ALWAYS force subsampling
//   - Absolute minimum render resolution: 320×200 (safety floor)
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
#include <format>
#include <filesystem>
#include <algorithm>
#include <cmath>

class RayCanvas {
public:
    RayCanvas(int windowWidth, int windowHeight, SDL_Window* window)
        : window_(window),
          window_width_(windowWidth),
          window_height_(windowHeight),
          render_width_(windowWidth),
          render_height_(windowHeight),
          minimized_(false),
          destroyed_(false),
          firstFrame_(true),
          materialsHandle_(0),
          hdrOutputImage_(VK_NULL_HANDLE),
          hdrOutputView_(VK_NULL_HANDLE),
          hdrOutputMemory_(VK_NULL_HANDLE),
          prevHdrOutputImage_(VK_NULL_HANDLE),
          prevHdrOutputView_(VK_NULL_HANDLE),
          prevHdrOutputMemory_(VK_NULL_HANDLE),
          descriptorPool_(VK_NULL_HANDLE),
          descriptorSet_(VK_NULL_HANDLE),
          adaptiveScale_(1.2f),
          lastAppliedScale_(1.2f),
          lastPresentTime_s_(0.0),
          measuredRefreshRateHz_(60.0f),
          lastFpsLog_(0.0),
          frameCount_(0),
          lastAdaptiveAdjustTime_(0.0),
          adaptiveFrameCount_(0),
          lastAggressiveDownscaleTime_(0.0),
          needsRecreate_(false),
          justResizedThisFrame_(false)
    {
        Swapchain::create(window, window_width_, window_height_);

        if (!Swapchain::get()) {
            LOG_FATAL_CAT("RAYCANVAS", "Failed to create swapchain");
            std::abort();
        }

        buildMaterialLibrary();

        updateRenderResolution();
        createPersistentHDR();
        createPreviousHDR();
        createDescriptorPoolAndSet();

        Pipeline::initialize();
        Pipeline::create_pipeline_layout();
        Pipeline::create_canvas_pipeline();

        updateDescriptorSet();

        clearHDRImages();
    }

    ~RayCanvas() {
        if (destroyed_) return;
        destroyed_ = true;

        vkDeviceWaitIdle(rtx().device);

        if (descriptorSet_ != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(rtx().device, descriptorPool_, 1, &descriptorSet_);
        }
        if (descriptorPool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(rtx().device, descriptorPool_, nullptr);
        }

        destroyHDRResources();

        Memory::destroy(materialsHandle_);

        Pipeline::shutdown();
    }

    void buildMaterialLibrary() {
        std::vector<Material> materials;

        auto addBase = [&](const MaterialLayer& layer, const char* name = nullptr) {
            Material m{};
            m.layers[0]            = layer;
            m.layerCount           = 1;
            m.layerBlendFactors[0] = 1.0f;
            materials.push_back(m);
            if (name) LOG_DEBUG_CAT("MATERIALS", "Added base material: {}", name);
        };

        auto addFull = [&](const Material& mat, const char* name = nullptr) {
            materials.push_back(mat);
            if (name) LOG_DEBUG_CAT("MATERIALS", "Added full material: {}", name);
        };

        addBase(Materials::OpenPBR_DielectricBase,      "Dielectric (glass-like)");
        addBase(Materials::OpenPBR_Metal,               "Polished Metal / Gold");

        addBase(Materials::DisneyCartoonSkin,           "Cartoon Character Skin");
        addBase(Materials::DisneyVelvetFabric,          "Velvet / Fabric");
        addBase(Materials::PixarToyPlastic,             "Toy Plastic (glossy)");
        addBase(Materials::OpenPBR_GlossyPaint,         "Glossy Painted Surface");
        addBase(Materials::OpenPBR_FrostedGlass,        "Frosted / Translucent Glass");

        addFull(Materials::DisneyPrincessGown,          "Princess Gown (velvet + satin)");
        addFull(Materials::CartoonCharacterSkin,        "Enhanced Cartoon Skin");
        addFull(Materials::ShinyRetroRobot,             "Shiny Retro Robot (paint + chrome)");

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

        LOG_SUCCESS_CAT("RAYCANVAS", "Material library built with {} materials", materials.size());
    }

    void maybeUpdateCanvas() noexcept {
        if (destroyed_) return;

        frameCount_++;
        adaptiveFrameCount_++;

        bool quit = false, fullscreen_toggle = false;
        int dummyW = 0, dummyH = 0;
        sdl_poll_events(dummyW, dummyH, quit, fullscreen_toggle);

        if (fullscreen_toggle) {
            sdl_toggle_fullscreen();
        }

        if (quit) {
            destroyed_ = true;
            LOG_INFO_CAT("RAYCANVAS", "Quit signal received — exiting");
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
            vkDeviceWaitIdle(rtx().device);
            onResize(currentW, currentH);
            minimized_ = false;
            needsRecreate_ = false;
            justResizedThisFrame_ = true;
            return;
        }

        if (justResizedThisFrame_) {
            justResizedThisFrame_ = false;
        }

        if (!Swapchain::get() || !hdrOutputImage_ || !hdrOutputView_) {
            LOG_WARNING_CAT("RAYCANVAS", "Invalid state — skipping frame");
            return;
        }

        if (firstFrame_) {
            TotalTime::get().seal();
            firstFrame_ = false;
            LOG_AMOURANTH("Genesis sealed — eternal canvas begins 💖");
            lastAdaptiveAdjustTime_ = TotalTime::get().seconds();
        }

        double now = TotalTime::get().seconds();
        static double lastKnownTime = 0.0;
        if (now <= lastKnownTime) now = lastKnownTime + Swapchain::smoothedRefresh_s;
        lastKnownTime = now;

        uint32_t imageIndex = 0;

        VkFenceCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VkFence fence = VK_NULL_HANDLE;
        vkCreateFence(rtx().device, &fci, nullptr, &fence);

        VkResult acq = ext().vkAcquireNextImageKHR(rtx().device, Swapchain::get(), UINT64_MAX,
                                                    VK_NULL_HANDLE, fence, &imageIndex);

        vkWaitForFences(rtx().device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(rtx().device, fence, nullptr);

        if (acq == VK_ERROR_OUT_OF_DATE_KHR || acq == VK_SUBOPTIMAL_KHR) {
            LOG_WARNING_CAT("SWAPCHAIN", "Acquire out-of-date/suboptimal — forcing recreate next frame");
            needsRecreate_ = true;
            return;
        }

        if (acq != VK_SUCCESS) {
            LOG_ERROR_CAT("SWAPCHAIN", "Acquire failed: {}", vkh.result(acq));
            if (acq == VK_ERROR_SURFACE_LOST_KHR || acq == VK_ERROR_DEVICE_LOST) {
                destroyed_ = true;
            }
            return;
        }

        VkCommandBuffer cmd = beginTransientCommandBuffer();
        if (!cmd) return;

        transitionImageLayout(cmd, hdrOutputImage_,
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        VkDescriptorSet set = descriptorSet_;
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, Pipeline::pipeline_layout,
                                0, 1, &set, 0, nullptr);

        lastPresentTime_s_ = now;

        // ───────────────────────────────────────────────────────────────
        // Adaptive resolution scaling — respect OptionsMenu values
        // ───────────────────────────────────────────────────────────────
        if (Options::Rendering::EnableAdaptiveResolution) {
            if (now - lastAdaptiveAdjustTime_ >= 0.8 && adaptiveFrameCount_ >= 6) {
                double elapsed = now - lastAdaptiveAdjustTime_;
                double currentFPS = static_cast<double>(adaptiveFrameCount_) / elapsed;
                double currentFrametime_ms = 1000.0 / currentFPS;

                // Runtime-calculated thresholds
                const float targetHeadroom       = 1.05f;
                const float safeFrametimeMs      = 1000.0f / (measuredRefreshRateHz_ * targetHeadroom);
                const float maxAllowedFrametime  = safeFrametimeMs * (Options::Rendering::MaxGPULoadPercent / 100.0f);

                const float warningFrametimeMs   = safeFrametimeMs * 1.15f;
                const float dangerFrametimeMs    = safeFrametimeMs * 1.40f;

                const float upscalingThreshold   = 1.25f;
                const float downscaleNormal      = 0.94f;
                const float downscaleStrong      = 0.82f;
                const float downscaleEmergency   = 0.65f;

                double targetFPS = measuredRefreshRateHz_ * targetHeadroom;

                bool changed = false;

                // If GPU load > MaxGPULoadPercent → force subsampling
                if (currentFrametime_ms > maxAllowedFrametime) {
                    float targetScale = Options::Rendering::MinResolutionScale * 1.15f;
                    if (adaptiveScale_ > targetScale) {
                        adaptiveScale_ = std::max(targetScale, adaptiveScale_ * 0.82f);
                        changed = true;
                        LOG_WARN_CAT("ADAPTIVE", "GPU > {:.0f}% — forcing subsampling to {:.2f}x (ft {:.1f}ms > {:.1f}ms)",
                                     Options::Rendering::MaxGPULoadPercent, adaptiveScale_, currentFrametime_ms, maxAllowedFrametime);
                    }
                }

                // Danger zone
                if (currentFrametime_ms > dangerFrametimeMs) {
                    adaptiveScale_ = std::min(adaptiveScale_, 0.70f);
                    if (adaptiveScale_ > 0.55f) adaptiveScale_ *= 0.60f;
                    adaptiveScale_ = std::max(adaptiveScale_, Options::Rendering::MinResolutionScale);
                    lastAggressiveDownscaleTime_ = now;
                    LOG_WARN_CAT("ADAPTIVE", "DANGER ZONE — forced subsampling to {:.2f}x (ft {:.1f}ms)", adaptiveScale_, currentFrametime_ms);
                    changed = true;
                }
                // Strong downscale
                else if (currentFrametime_ms > warningFrametimeMs * 1.20f || currentFPS < targetFPS * downscaleEmergency) {
                    adaptiveScale_ *= 0.72f;
                    adaptiveScale_ = std::max(adaptiveScale_, Options::Rendering::MinResolutionScale);
                    lastAggressiveDownscaleTime_ = now;
                    LOG_WARN_CAT("ADAPTIVE", "Strong subsampling to {:.2f}x (FPS {:.1f})", adaptiveScale_, currentFPS);
                    changed = true;
                }
                // Normal downscale
                else if (currentFPS < targetFPS * downscaleStrong) {
                    adaptiveScale_ -= Options::Rendering::ResolutionStepSize * 3.5f;
                    adaptiveScale_ = std::max(adaptiveScale_, Options::Rendering::MinResolutionScale);
                    LOG_INFO_CAT("ADAPTIVE", "↓ Subsampling to {:.2f}x (FPS {:.1f})", adaptiveScale_, currentFPS);
                    changed = true;
                }
                else if (currentFPS < targetFPS * downscaleNormal) {
                    adaptiveScale_ -= Options::Rendering::ResolutionStepSize * 2.0f;
                    adaptiveScale_ = std::max(adaptiveScale_, Options::Rendering::MinResolutionScale);
                    LOG_INFO_CAT("ADAPTIVE", "↓ Mild down to {:.2f}x (FPS {:.1f})", adaptiveScale_, currentFPS);
                    changed = true;
                }
                // Upscale only when very safe
                else if (currentFPS > targetFPS * upscalingThreshold &&
                         now - lastAggressiveDownscaleTime_ > 5.0 &&
                         currentFrametime_ms < safeFrametimeMs * 0.80f) {
                    adaptiveScale_ += Options::Rendering::ResolutionStepSize;
                    adaptiveScale_ = std::min(adaptiveScale_, Options::Rendering::MaxResolutionScale);
                    LOG_INFO_CAT("ADAPTIVE", "↑ Quality up to {:.2f}x (FPS {:.1f})", adaptiveScale_, currentFPS);
                    changed = true;
                }

                adaptiveFrameCount_ = 0;
                lastAdaptiveAdjustTime_ = now;

                if (changed && std::abs(adaptiveScale_ - lastAppliedScale_) > 0.06f) {
                    updateRenderResolution();
                    destroyHDRResources();
                    createPersistentHDR();
                    createPreviousHDR();
                    updateDescriptorSet();
                    lastAppliedScale_ = adaptiveScale_;
                }
            }
        }

        updateRenderResolution();

        Pipeline::dispatch_canvas(cmd, render_width_, render_height_, static_cast<float>(now));

        lastDispatchedW_ = static_cast<uint32_t>(render_width_);
        lastDispatchedH_ = static_cast<uint32_t>(render_height_);

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
                             0, 0, nullptr, 0, nullptr, 1, &postComputeBarrier);

        endSubmitAndWait(cmd);

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
                             0, 0, nullptr, 0, nullptr, 1, &swapBarrier);

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
                             0, 0, nullptr, 0, nullptr, 1, &presentBarrier);

        endSubmitAndWait(blitCmd);

        VkPresentInfoKHR pi{};
        pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.swapchainCount = 1;
        pi.pSwapchains    = &Swapchain::swapchain.value;
        pi.pImageIndices  = &imageIndex;

        VkResult pres = ext().vkQueuePresentKHR(rtx().present_queue, &pi);

        if (pres == VK_SUCCESS) {
            Swapchain::updateRefreshEstimate(TotalTime::get().seconds());
            measuredRefreshRateHz_ = 1.0f / Swapchain::getSmoothedRefresh();
        } else if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
            LOG_WARNING_CAT("SWAPCHAIN", "Present out-of-date/suboptimal — forcing recreate next frame");
            needsRecreate_ = true;
        } else if (pres != VK_SUCCESS) {
            LOG_ERROR_CAT("SWAPCHAIN", "Present failed: {}", vkh.result(pres));
            if (pres == VK_ERROR_SURFACE_LOST_KHR) destroyed_ = true;
        }

        if (now - lastFpsLog_ >= 5.0) {
            double elapsed     = now - lastFpsLog_;
            double avgFps      = (frameCount_ > 0) ? static_cast<double>(frameCount_) / elapsed : 0.0;
            double avgDt_us    = (frameCount_ > 0) ? (elapsed * 1000000.0) / static_cast<double>(frameCount_) : 0.0;

            int winW = window_width_;
            int winH = window_height_;

            uint32_t dispW = lastDispatchedW_;
            uint32_t dispH = lastDispatchedH_;

            float supersampleFactor = (winW > 0) ? static_cast<float>(dispW) / static_cast<float>(winW) : 1.0f;
            const char* mode = (supersampleFactor < 0.98f) ? "SUBSAMPLING" :
                               (supersampleFactor > 1.02f) ? "SUPERSAMPLING" : "NATIVE";

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
                          "  State:          {}\n"
                          "  Adaptive qual:  {}\n"
                          "  Accumulation:   {}\n"
                          "  Frames this log: {}\n"
                          "───────────────────────────────────────────────────────────────",
                          now * 0.1,
                          avgFps, avgDt_us,
                          measuredRefreshRateHz_,
                          winW, winH,
                          dispW, dispH, supersampleFactor, mode,
                          adaptiveScale_,
                          stateEmoji,
                          Options::Rendering::EnableAdaptiveResolution ? "enabled" : "disabled",
                          Options::Rendering::ACCUMULATION ? "on" : "off",
                          frameCount_
            );

            lastFpsLog_  = now;
            frameCount_  = 0;
        }
    }

    void onResize(int newWidth, int newHeight) noexcept {
        if (newWidth <= 0 || newHeight <= 0) {
            minimized_ = true;
            return;
        }

        if (newWidth == window_width_ && newHeight == window_height_ && !needsRecreate_) {
            return;
        }

        LOG_INFO_CAT("WINDOW", "Resizing to {}x{}", newWidth, newHeight);

        vkDeviceWaitIdle(rtx().device);

        if (hdrOutputView_)   vkDestroyImageView (rtx().device, hdrOutputView_, nullptr);
        if (hdrOutputImage_)  vkDestroyImage     (rtx().device, hdrOutputImage_, nullptr);
        if (hdrOutputMemory_) vkFreeMemory       (rtx().device, hdrOutputMemory_, nullptr);

        if (prevHdrOutputView_)   vkDestroyImageView (rtx().device, prevHdrOutputView_, nullptr);
        if (prevHdrOutputImage_)  vkDestroyImage     (rtx().device, prevHdrOutputImage_, nullptr);
        if (prevHdrOutputMemory_) vkFreeMemory       (rtx().device, prevHdrOutputMemory_, nullptr);

        hdrOutputView_   = VK_NULL_HANDLE;
        hdrOutputImage_  = VK_NULL_HANDLE;
        hdrOutputMemory_ = VK_NULL_HANDLE;
        prevHdrOutputView_   = VK_NULL_HANDLE;
        prevHdrOutputImage_  = VK_NULL_HANDLE;
        prevHdrOutputMemory_ = VK_NULL_HANDLE;

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
            transitionImageLayout(cmd, hdrOutputImage_,
                                  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
            transitionImageLayout(cmd, prevHdrOutputImage_,
                                  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

            vkCmdClearColorImage(cmd, hdrOutputImage_,   VK_IMAGE_LAYOUT_GENERAL, &clearBlack, 1, &range);
            vkCmdClearColorImage(cmd, prevHdrOutputImage_, VK_IMAGE_LAYOUT_GENERAL, &clearBlack, 1, &range);

            endSubmitAndWait(cmd);
        }

        LOG_SUCCESS_CAT("RAYCANVAS", "Resize complete — {}x{} (internal {}x{})",
                        window_width_, window_height_, render_width_, render_height_);
    }

    int  getWidth()  const noexcept { return window_width_; }
    int  getHeight() const noexcept { return window_height_; }
    bool isMinimized() const noexcept { return minimized_; }
    bool isDestroyed() const noexcept { return destroyed_; }

private:
    void updateRenderResolution() noexcept {
        float targetSupersample = Options::Rendering::EnableAdaptiveResolution
                                ? std::clamp(adaptiveScale_,
                                             Options::Rendering::MinResolutionScale,
                                             Options::Rendering::MaxResolutionScale)
                                : 1.0f;

        // Respect any hard internal resolution cap from options
        float maxAllowedW = static_cast<float>(Options::Rendering::INTERNAL_WIDTH)  / static_cast<float>(window_width_);
        float maxAllowedH = static_cast<float>(Options::Rendering::INTERNAL_HEIGHT) / static_cast<float>(window_height_);
        float maxAllowed = std::min(maxAllowedW, maxAllowedH);

        float effectiveSupersample = std::min(targetSupersample, maxAllowed);

        render_width_  = static_cast<int>(std::round(static_cast<float>(window_width_)  * effectiveSupersample));
        render_height_ = static_cast<int>(std::round(static_cast<float>(window_height_) * effectiveSupersample));

        // Absolute minimum resolution safety floor — never go below 320×200
        render_width_  = std::max(render_width_,  320);
        render_height_ = std::max(render_height_, 200);
    }

    void createPersistentHDR() noexcept {
        vkDeviceWaitIdle(rtx().device);

        if (hdrOutputView_)   vkDestroyImageView (rtx().device, hdrOutputView_,   nullptr);
        if (hdrOutputImage_)  vkDestroyImage     (rtx().device, hdrOutputImage_,  nullptr);
        if (hdrOutputMemory_) vkFreeMemory       (rtx().device, hdrOutputMemory_, nullptr);

        hdrOutputView_   = VK_NULL_HANDLE;
        hdrOutputImage_  = VK_NULL_HANDLE;
        hdrOutputMemory_ = VK_NULL_HANDLE;

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

        vkh.checker(vkCreateImage(rtx().device, &ci, nullptr, &prevHdrOutputImage_), "MEMORY", "Prev HDR Image");

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
        vkDeviceWaitIdle(rtx().device);

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
                             0, 0, nullptr, 0, nullptr, 1, &barrier);
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

private:
    SDL_Window*    window_           = nullptr;
    int            window_width_     = 0;
    int            window_height_    = 0;

    int            render_width_     = 0;
    int            render_height_    = 0;

    bool           minimized_        = false;
    bool           destroyed_        = false;
    bool           firstFrame_       = true;

    uint64_t       materialsHandle_          = 0;

    VkImage        hdrOutputImage_           = VK_NULL_HANDLE;
    VkImageView    hdrOutputView_            = VK_NULL_HANDLE;
    VkDeviceMemory hdrOutputMemory_          = VK_NULL_HANDLE;

    VkImage        prevHdrOutputImage_       = VK_NULL_HANDLE;
    VkImageView    prevHdrOutputView_        = VK_NULL_HANDLE;
    VkDeviceMemory prevHdrOutputMemory_      = VK_NULL_HANDLE;

    VkDescriptorPool descriptorPool_         = VK_NULL_HANDLE;
    VkDescriptorSet  descriptorSet_          = VK_NULL_HANDLE;

    float          adaptiveScale_            = 1.2f;
    float          lastAppliedScale_         = 1.2f;
    double         lastPresentTime_s_        = 0.0;
    float          measuredRefreshRateHz_    = 60.0f;

    double         lastFpsLog_               = 0.0;
    uint64_t       frameCount_               = 0;

    double         lastAdaptiveAdjustTime_   = 0.0;
    uint64_t       adaptiveFrameCount_       = 0;

    double         lastAggressiveDownscaleTime_ = 0.0;

    uint32_t       lastDispatchedW_          = 0;
    uint32_t       lastDispatchedH_          = 0;

    bool           needsRecreate_            = false;
    bool           justResizedThisFrame_     = false;
};

inline RayCanvas* rayCanvas = nullptr;