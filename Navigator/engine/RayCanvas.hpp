#pragma once

// =============================================================================
// AMOURANTH RTX Engine — RayCanvas (High-DPI 4K Ready)
// Pure raymarching + hardware ray tracing
// (C) 2025-2026 Zachary Robert Geurts <gzac5314@gmail.com>
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
#include <string>

class RayCanvas {
public:
    RayCanvas(int initialLogicalWidth, int initialLogicalHeight, SDL_Window* window)
        : window_(window),
          timestampQueryPool_(VK_NULL_HANDLE),
          acquireFence_(VK_NULL_HANDLE),
          descriptorPool_(VK_NULL_HANDLE),
          descriptorSet_(VK_NULL_HANDLE),
          materialsHandle_(0),
          minimized_(false),
          destroyed_(false),
          firstFrame_(true),
          adaptiveScale_(1.0),
          lastPresentTime_s_(0.0),
          measuredRefreshRateHz_(60.0),
          lastFpsLog_(0.0),
          frameCount_(0),
          lastAdaptiveAdjustTime_(0.0),
          needsRecreate_(false),
          timestampPeriodNs_(1.0),
          smoothedGpuTimeMs_(16.67)
    {
        if (!Swapchain::get()) {
            LOG_FATAL_CAT("RAYCANVAS", "No valid swapchain — navigator must create it first");
            std::abort();
        }

        // Use physical pixel size from the start (4K support)
        int physicalW = 0, physicalH = 0;
        SDL_GetWindowSizeInPixels(window_, &physicalW, &physicalH);
        if (physicalW <= 0 || physicalH <= 0) {
            physicalW = initialLogicalWidth;
            physicalH = initialLogicalHeight;
        }

        window_width_  = physicalW;
        window_height_ = physicalH;
        render_width_  = physicalW;
        render_height_ = physicalH;

        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(rtx().physical, &props);
        timestampPeriodNs_ = props.limits.timestampPeriod ? static_cast<double>(props.limits.timestampPeriod) : 1.0;

        Pipeline::initialize_descriptors_and_layout();
        Pipeline::create_canvas_pipeline();

        createTimestampQueryPool();
        buildMaterialLibrary();
        createAcquireFence();
        updateRenderResolution();
        createHDRResources();
        createDescriptorPoolAndSet();
        updateDescriptorSet();
        clearHDRImages();  // Initial clear

        LOG_SUCCESS_CAT("RAYCANVAS", "Initialized — physical {}x{} (logical {}x{}) render target ready",
                        physicalW, physicalH, initialLogicalWidth, initialLogicalHeight);
    }

    ~RayCanvas() {
        if (destroyed_) return;
        destroyed_ = true;

        if (acquireFence_ != VK_NULL_HANDLE)
            vkDestroyFence(rtx().device, acquireFence_, nullptr);

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

    bool maybeUpdateCanvas(bool isRunning) noexcept {
        if (destroyed_) {
            isRunning = false;
            return isRunning;
        }

        frameCount_++;

        double now = TotalTime::get().seconds() * static_cast<double>(Options::Debug::TimeScale);

        Pipeline::processInput(window_width_, window_height_);

        bool quit = Pipeline::shouldQuit();
        bool fullscreen_toggle = Pipeline::wantsFullscreenToggle();
        int newWidth = Pipeline::getRequestedWidth();
        int newHeight = Pipeline::getRequestedHeight();
        bool sizeChanged = (newWidth != window_width_) || (newHeight != window_height_);

        if (fullscreen_toggle) toggleFullscreen();

        if (quit) {
            destroyed_ = true;
            LOG_INFO_CAT("RAYCANVAS", "Quit signal received from Pipeline");
            return isRunning;
        }

        bool nowMinimized = (newWidth <= 0 || newHeight <= 0);
        if (nowMinimized) {
            minimized_ = true;
            return isRunning;
        }

        if (minimized_ || sizeChanged || needsRecreate_) {
            onResize(newWidth, newHeight);
            minimized_ = false;
            return isRunning;
        }

        if (firstFrame_) {
            TotalTime::get().seal();
            firstFrame_ = false;
            LOG_AMOURANTH("Raymarch engine sealed — rendering begins 💖");
            lastAdaptiveAdjustTime_ = now;
        }

        static double lastKnownTime = 0.0;
        if (now <= lastKnownTime) now = lastKnownTime + Swapchain::smoothedRefresh_s;
        lastKnownTime = now;

        uint32_t imageIndex = 0;
        vkResetFences(rtx().device, 1, &acquireFence_);

        VkResult acq = ext().vkAcquireNextImageKHR(rtx().device, Swapchain::get(), UINT64_MAX,
                                                   VK_NULL_HANDLE, acquireFence_, &imageIndex);
        vkWaitForFences(rtx().device, 1, &acquireFence_, VK_TRUE, UINT64_MAX);

        if (acq == VK_ERROR_OUT_OF_DATE_KHR || acq == VK_SUBOPTIMAL_KHR) {
            needsRecreate_ = true;
            return isRunning;
        }
        if (acq != VK_SUCCESS) {
            LOG_ERROR_CAT("SWAPCHAIN", "Acquire failed: {}", vkh.result(acq));
            if (acq == VK_ERROR_SURFACE_LOST_KHR || acq == VK_ERROR_DEVICE_LOST) destroyed_ = true;
            return isRunning;
        }

        VkCommandBuffer cmd = beginTransientCommandBuffer();
        if (!cmd) return isRunning;

        // Always transition HDR images to GENERAL before dispatch
        transitionImageLayout(cmd, mainHDR_.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        transitionImageLayout(cmd, prevHDR_.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        vkCmdResetQueryPool(cmd, timestampQueryPool_, 0, 2);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, timestampQueryPool_, 0);

        lastPresentTime_s_ = now;
        updateRenderResolution();

        bool isRt = Options::Rendering::EnableHardwareRayTracing &&
                    (Options::Rendering::CurrentTechnique == Options::Rendering::RenderTechnique::HardwareRayTracing) &&
                    rtx().rayTracingSupported;

        VkDescriptorSet set = descriptorSet_;
        vkCmdBindDescriptorSets(cmd,
                                isRt ? VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR : VK_PIPELINE_BIND_POINT_COMPUTE,
                                Pipeline::pipeline_layout,
                                0, 1, &set, 0, nullptr);

        Pipeline::dispatch(cmd, render_width_, render_height_, static_cast<float>(now));

        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampQueryPool_, 1);

        // Transition HDR to TRANSFER_SRC for blit/copy
        VkImageMemoryBarrier postCompute{};
        postCompute.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        postCompute.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        postCompute.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        postCompute.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        postCompute.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        postCompute.image = mainHDR_.image;
        postCompute.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &postCompute);

        VkImage swapImg = Swapchain::images[imageIndex];

        VkImageMemoryBarrier swapBarrier{};
        swapBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        swapBarrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        swapBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        swapBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        swapBarrier.image = swapImg;
        swapBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &swapBarrier);

        VkExtent2D swapExtent = Swapchain::getExtent();

        if (render_width_ == static_cast<int>(swapExtent.width) &&
            render_height_ == static_cast<int>(swapExtent.height)) {
            VkImageCopy copy{};
            copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.srcOffset      = {0, 0, 0};
            copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.dstOffset      = {0, static_cast<int32_t>(swapExtent.height), 0};
            copy.extent         = {static_cast<uint32_t>(render_width_),
                                   static_cast<uint32_t>(render_height_), 1};

            vkCmdCopyImage(cmd,
                           mainHDR_.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           swapImg,        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &copy);
        } else {
            VkImageBlit flipBlit{};
            flipBlit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            flipBlit.srcOffsets[0]  = {0, 0, 0};
            flipBlit.srcOffsets[1]  = {render_width_, render_height_, 1};
            flipBlit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            flipBlit.dstOffsets[0]  = {0, static_cast<int32_t>(swapExtent.height), 0};
            flipBlit.dstOffsets[1]  = {static_cast<int32_t>(swapExtent.width), 0, 1};

            vkCmdBlitImage(cmd,
                           mainHDR_.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           swapImg,        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           1, &flipBlit,
                           VK_FILTER_LINEAR);
        }

        // Transition swapchain back to PRESENT
        VkImageMemoryBarrier presentBarrier{};
        presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        presentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        presentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        presentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        presentBarrier.image = swapImg;
        presentBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &presentBarrier);

        endSubmitAndWait(cmd);

        if (frameCount_ % 30 == 0) {
            uint64_t ts[2]{};
            VkResult res = vkGetQueryPoolResults(rtx().device, timestampQueryPool_, 0, 2,
                                                 sizeof(ts), ts, sizeof(uint64_t),
                                                 VK_QUERY_RESULT_64_BIT);
            if (res == VK_SUCCESS) {
                double gpuMs = double(ts[1] - ts[0]) * timestampPeriodNs_ / 1'000'000.0;
                double alpha = (gpuMs > smoothedGpuTimeMs_) ? 0.70 : 0.22;
                smoothedGpuTimeMs_ = (1.0 - alpha) * smoothedGpuTimeMs_ + alpha * gpuMs;
            } else {
                LOG_WARNING_CAT("GPU", "Query pool results failed: {}", vkh.result(res));
            }
        }

        if (frameCount_ % 30 == 0 && Options::Rendering::EnableAdaptiveResolution) {
            adjustAdaptiveScale(now);
        }

        VkPresentInfoKHR pi{};
        pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        pi.swapchainCount = 1;
        pi.pSwapchains    = &Swapchain::swapchain.value;
        pi.pImageIndices  = &imageIndex;

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

        if (now - lastFpsLog_ >= 5.0) {
            double elapsed = now - lastFpsLog_;
            double fps = frameCount_ ? double(frameCount_) / elapsed : 0.0;
            double dt_us = frameCount_ ? (elapsed * 1'000'000.0) / double(frameCount_) : 0.0;

            int ww = window_width_, wh = window_height_;
            double sf = ww ? double(render_width_) / ww : 1.0;
            const char* mode = sf < 0.98 ? "SUB" : (sf > 1.02 ? "SUPER" : "NATIVE");

            double tgtMs = 1000.0 / measuredRefreshRateHz_;
            double loadPct = tgtMs > 0.001 ? (smoothedGpuTimeMs_ / tgtMs) * 100.0 : 0.0;

            const char* state = minimized_ ? "🟥 min" :
                                (!Swapchain::get() || !mainHDR_.image) ? "⚠️ invalid" : "✅ active";

            VRAMReality vram = Memory::measureReality();
            double usedMB  = double(vram.driver_footprint) / (1024.0 * 1024.0);
            double totalMB = double(vram.total) / (1024.0 * 1024.0);
            double freePct = totalMB ? 100.0 * (1.0 - usedMB / totalMB) : 0.0;

            std::string pathStr;
            switch (Options::Rendering::CurrentTechnique) {
                case Options::Rendering::RenderTechnique::Pure2DCanvas: pathStr = "Pure 2D Canvas"; break;
                case Options::Rendering::RenderTechnique::PureRaymarching: pathStr = "Pure Raymarching"; break;
                case Options::Rendering::RenderTechnique::HybridRasterMarch: pathStr = "Hybrid Raster + March"; break;
                case Options::Rendering::RenderTechnique::SoftwareRayTracing: pathStr = "Software Ray Tracing"; break;
                case Options::Rendering::RenderTechnique::HardwareRayTracing: pathStr = "Hardware Ray Tracing"; break;
                case Options::Rendering::RenderTechnique::ProgressivePathTracing: pathStr = "Progressive Path Tracing"; break;
            }

            std::string adaptiveStr = Options::Rendering::EnableAdaptiveResolution ? "✅" : "❌";
            std::string accumStr    = Options::Rendering::EnableAccumulation ? "✅" : "❌";
            std::string supersampleStr = (sf > 1.0) ? "⚡" : "❌";

            LOG_AMOURANTH(
                "───────────────────────────────────────────────────────────────\n"
                "              RayCanvas Status  •  t+{:.4}s\n"
                "  FPS:            {}     (avg {} µs)\n"
                "  Refresh:        {} Hz\n"
                "  Window:         {}x{}\n"
                "  Render:         {}x{}     ({:.4f}x — {})\n"
                "  Adaptive scale: {:.4f}x\n"
                "  GPU load:       {:.4f}%   (smoothed {:.4f} ms)\n"
                "  Path:           {}\n"
                "  State:          {}\n"
                "  Features:       Adaptive {}  Accum {}  Supersample {}\n"
                "  VRAM:           {:.4f}/{:.4f} MB ({:.4f}% free)\n"
                "  Frames logged:  {}\n"
                "───────────────────────────────────────────────────────────────",
                now, fps, dt_us, measuredRefreshRateHz_,
                ww, wh, render_width_, render_height_, sf, mode,
                adaptiveScale_, loadPct, smoothedGpuTimeMs_,
                pathStr.c_str(),
                state,
                adaptiveStr.c_str(),
                accumStr.c_str(),
                supersampleStr.c_str(),
                usedMB, totalMB, freePct, frameCount_
            );

            lastFpsLog_ = now;
            frameCount_ = 0;
        }

        return isRunning;
    }

    [[nodiscard]] int  getWidth()  const noexcept { return window_width_; }
    [[nodiscard]] int  getHeight() const noexcept { return window_height_; }
    [[nodiscard]] bool isMinimized() const noexcept { return minimized_; }
    [[nodiscard]] bool isDestroyed() const noexcept { return destroyed_; }

private:
    void toggleFullscreen() noexcept {
        bool isFS = (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) != 0;
        SDL_SetWindowFullscreen(window_, !isFS ? SDL_WINDOW_FULLSCREEN : 0);
        LOG_INFO_CAT("WINDOW", "Fullscreen {}", isFS ? "off" : "on");

        int physicalW = 0, physicalH = 0;
        SDL_GetWindowSizeInPixels(window_, &physicalW, &physicalH);
        onResize(physicalW, physicalH);
    }

    void onResize(int newWidth, int newHeight) noexcept {
        if (newWidth <= 0 || newHeight <= 0) {
            minimized_ = true;
            return;
        }

        if (newWidth == window_width_ && newHeight == window_height_ && !needsRecreate_) return;

        if (needsRecreate_) vkDeviceWaitIdle(rtx().device);

        int actualW = 0, actualH = 0;
        SDL_GetWindowSizeInPixels(window_, &actualW, &actualH);
        window_width_  = actualW;
        window_height_ = actualH;

        minimized_ = false;
        needsRecreate_ = false;

        Swapchain::recreate(window_width_, window_height_);

        updateRenderResolution();

        destroyHDRResources();
        createHDRResources();

        // Re-create descriptor pool/set after HDR recreate
        if (descriptorPool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(rtx().device, descriptorPool_, nullptr);
            descriptorPool_ = VK_NULL_HANDLE;
        }
        createDescriptorPoolAndSet();
        updateDescriptorSet();

        // Re-clear HDR images after resize/recreate
        clearHDRImages();
    }

    void updateRenderResolution() noexcept {
        if (!Options::Rendering::EnableAdaptiveResolution) {
            render_width_ = window_width_;
            render_height_ = window_height_;
            return;
        }

        double s = adaptiveScale_;
        auto w = static_cast<int64_t>(std::round(static_cast<double>(window_width_) * s));
        auto h = static_cast<int64_t>(std::round(static_cast<double>(window_height_) * s));

        w = std::clamp(w, int64_t(1), int64_t(32768));
        h = std::clamp(h, int64_t(1), int64_t(32768));

        render_width_  = static_cast<int>(w);
        render_height_ = static_cast<int>(h);
    }

    void createTimestampQueryPool() noexcept {
        VkQueryPoolCreateInfo q{};
        q.sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        q.queryType  = VK_QUERY_TYPE_TIMESTAMP;
        q.queryCount = 2;
        vkCreateQueryPool(rtx().device, &q, nullptr, &timestampQueryPool_);
    }

    void buildMaterialLibrary() noexcept {
        VkDeviceSize sz = sizeof(Materials::AllMaterials);

        materialsHandle_ = Memory::createBuffer(
            sz,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            "MaterialsLibrary",
            Memory::MemoryHint::HostVisible
        );

        if (sz > 0) {
            auto [staging, mem] = Memory::uploadToBuffer(materialsHandle_, Materials::AllMaterials.data(), sz);
            if (staging) {
                vkDestroyBuffer(rtx().device, staging, nullptr);
                vkFreeMemory(rtx().device, mem, nullptr);
            }
        }

        LOG_SUCCESS_CAT("RAYCANVAS", "Material library uploaded — {} materials", static_cast<int>(MAT_COUNT));
    }

    void createAcquireFence() noexcept {
        VkFenceCreateInfo f{};
        f.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vkCreateFence(rtx().device, &f, nullptr, &acquireFence_);
    }

    void createDescriptorPoolAndSet() noexcept {
        VkDescriptorPoolSize sizes[] = {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   2},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  6}
        };

        VkDescriptorPoolCreateInfo p{};
        p.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        p.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        p.maxSets       = 1;
        p.poolSizeCount = static_cast<uint32_t>(std::size(sizes));
        p.pPoolSizes    = sizes;

        vkCreateDescriptorPool(rtx().device, &p, nullptr, &descriptorPool_);

        VkDescriptorSetAllocateInfo a{};
        a.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        a.descriptorPool     = descriptorPool_;
        a.descriptorSetCount = 1;
        a.pSetLayouts        = &Pipeline::main_descriptor_layout;

        vkAllocateDescriptorSets(rtx().device, &a, &descriptorSet_);
    }

    void updateDescriptorSet() noexcept {
        if (!mainHDR_.view || !prevHDR_.view) {
            LOG_ERROR_CAT("RAYCANVAS", "Cannot update descriptor set — HDR views invalid");
            return;
        }

        std::array<VkWriteDescriptorSet, 3> writes{};

        VkDescriptorImageInfo img{};
        img.imageView   = mainHDR_.view;
        img.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        writes[0] = {};
        writes[0].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet           = descriptorSet_;
        writes[0].dstBinding       = 0;
        writes[0].descriptorCount  = 1;
        writes[0].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[0].pImageInfo       = &img;

        VkDescriptorImageInfo pimg{};
        pimg.imageView   = prevHDR_.view;
        pimg.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        writes[1] = {};
        writes[1].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet           = descriptorSet_;
        writes[1].dstBinding       = 1;
        writes[1].descriptorCount  = 1;
        writes[1].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[1].pImageInfo       = &pimg;

        VkDescriptorBufferInfo mat{};
        mat.buffer = Memory::getBuffer(materialsHandle_);
        mat.range  = VK_WHOLE_SIZE;

        writes[2] = {};
        writes[2].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet           = descriptorSet_;
        writes[2].dstBinding       = 4;
        writes[2].descriptorCount  = 1;
        writes[2].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[2].pBufferInfo      = &mat;

        vkUpdateDescriptorSets(rtx().device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    void transitionImageLayout(VkCommandBuffer cmd, VkImage img,
                               VkImageLayout oldL, VkImageLayout newL) noexcept {
        VkImageMemoryBarrier b{};
        b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.oldLayout = oldL;
        b.newLayout = newL;
        b.image     = img;
        b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkPipelineStageFlags src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        VkPipelineStageFlags dst = VK_PIPELINE_STAGE_TRANSFER_BIT;

        if (oldL == VK_IMAGE_LAYOUT_UNDEFINED && newL == VK_IMAGE_LAYOUT_GENERAL) {
            b.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            dst = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        } else if (oldL == VK_IMAGE_LAYOUT_GENERAL && newL == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
            b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        }

        vkCmdPipelineBarrier(cmd, src, dst, 0, 0, nullptr, 0, nullptr, 1, &b);
    }

    void clearHDRImages() noexcept {
        VkClearColorValue black{{0.0f, 0.0f, 0.0f, 1.0f}};
        VkImageSubresourceRange rng{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkCommandBuffer cmd = beginTransientCommandBuffer();
        if (!cmd) return;

        transitionImageLayout(cmd, mainHDR_.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        transitionImageLayout(cmd, prevHDR_.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        vkCmdClearColorImage(cmd, mainHDR_.image, VK_IMAGE_LAYOUT_GENERAL, &black, 1, &rng);
        vkCmdClearColorImage(cmd, prevHDR_.image, VK_IMAGE_LAYOUT_GENERAL, &black, 1, &rng);

        endSubmitAndWait(cmd);
    }

    void adjustAdaptiveScale(double now) noexcept {
        double elapsed = now - lastAdaptiveAdjustTime_;
        if (elapsed < 0.6) return;

        double tgtMs = 1000.0 / measuredRefreshRateHz_;
        double load = tgtMs > 0.001 ? (smoothedGpuTimeMs_ / tgtMs) * 100.0 : 0.0;

        double tgtScale = adaptiveScale_;

        if      (load > 220) tgtScale *= 0.60;
        else if (load > 180) tgtScale *= 0.75;
        else if (load > 140) tgtScale *= 0.85;
        else if (load >  95) tgtScale *= 0.92;
        else if (load <  55) tgtScale *= 1.15;
        else if (load <  70) tgtScale *= 1.08;

        tgtScale = std::clamp(tgtScale,
                              double(Options::Rendering::MinResolutionScale),
                              double(Options::Rendering::MaxResolutionScale));

        double hyst = (tgtScale > adaptiveScale_) ? 0.025 : 0.08;
        if (std::abs(tgtScale - adaptiveScale_) > hyst) {
            adaptiveScale_ = tgtScale;
            needsRecreate_ = true;
            LOG_INFO_CAT("RAYCANVAS", "Adaptive scale changed to {:.4f}x", adaptiveScale_);
        }

        lastAdaptiveAdjustTime_ = now;
    }

private:
    SDL_Window*    window_                    = nullptr;

    int            window_width_              = 0;
    int            window_height_             = 0;
    int            render_width_              = 0;
    int            render_height_             = 0;

    VkQueryPool     timestampQueryPool_       = VK_NULL_HANDLE;
    VkFence         acquireFence_             = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_          = VK_NULL_HANDLE;
    VkDescriptorSet  descriptorSet_           = VK_NULL_HANDLE;

    uint64_t       materialsHandle_           = 0;

    bool           minimized_                 = false;
    bool           destroyed_                 = false;
    bool           firstFrame_                = true;

    double         adaptiveScale_             = 1.0;
    double         lastPresentTime_s_         = 0.0;
    double         measuredRefreshRateHz_     = 60.0;
    double         lastFpsLog_                = 0.0;

    uint64_t       frameCount_                = 0;

    double         lastAdaptiveAdjustTime_    = 0.0;

    bool           needsRecreate_             = false;

    double         timestampPeriodNs_         = 1.0;
    double         smoothedGpuTimeMs_         = 16.67;

    // HDR resources
    struct HDRResource {
        VkImage        image  = VK_NULL_HANDLE;
        VkImageView    view   = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
    };

    HDRResource mainHDR_;
    HDRResource prevHDR_;

    void createHDRResource(HDRResource& r) noexcept {
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

        vkCreateImage(rtx().device, &ci, nullptr, &r.image);

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(rtx().device, r.image, &req);

        uint32_t mt = Memory::findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo mai{};
        mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize  = req.size;
        mai.memoryTypeIndex = mt;

        vkAllocateMemory(rtx().device, &mai, nullptr, &r.memory);
        vkBindImageMemory(rtx().device, r.image, r.memory, 0);

        VkImageViewCreateInfo vi{};
        vi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image            = r.image;
        vi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        vi.format           = VK_FORMAT_R32G32B32A32_SFLOAT;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCreateImageView(rtx().device, &vi, nullptr, &r.view);
    }

    void createHDRResources() noexcept {
        createHDRResource(mainHDR_);
        createHDRResource(prevHDR_);
    }

    void destroyHDRResource(HDRResource& r) noexcept {
        if (r.view)   vkDestroyImageView (rtx().device, r.view,   nullptr);
        if (r.image)  vkDestroyImage     (rtx().device, r.image,  nullptr);
        if (r.memory) vkFreeMemory       (rtx().device, r.memory, nullptr);
        r = {};
    }

    void destroyHDRResources() noexcept {
        destroyHDRResource(mainHDR_);
        destroyHDRResource(prevHDR_);
    }
};

inline RayCanvas* rayCanvas = nullptr;