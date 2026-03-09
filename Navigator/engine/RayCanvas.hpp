#pragma once

// =============================================================================
// AMOURANTH RTX Engine — RayCanvas (persistent compute renderer)
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
#include "InputManager.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <vector>
#include <cstdint>
#include <array>
#include <format>
#include <filesystem>
#include <algorithm>

bool needs_swapchain_recreate = false;

// =============================================================================
// RayCanvas — Persistent compute-based renderer
// =============================================================================
class RayCanvas {
public:
    RayCanvas(int windowWidth, int windowHeight, SDL_Window* window)
        : window_(window),
          window_width_(windowWidth),
          window_height_(windowHeight),
          internal_width_(windowWidth),
          internal_height_(windowHeight),
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
          adaptiveScale_(1.0f),
          lastPresentTime_s_(0.0),
          measuredRefreshRateHz_(60.0),
          lastFpsLog_(0.0),
          frameCount_(0)
    {
        updateInternalResolution();		

        Swapchain::create(window, window_width_, window_height_);

        if (!Swapchain::get()) {
            LOG_FATAL_CAT("RAYCANVAS", "Failed to create swapchain");
            std::abort();
        }

        buildMaterialLibrary();

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

        vkFreeDescriptorSets(rtx().device, descriptorPool_, 1, &descriptorSet_);
        vkDestroyDescriptorPool(rtx().device, descriptorPool_, nullptr);

        if (hdrOutputView_ != VK_NULL_HANDLE) vkDestroyImageView (rtx().device, hdrOutputView_,   nullptr);
        if (hdrOutputImage_ != VK_NULL_HANDLE) vkDestroyImage     (rtx().device, hdrOutputImage_,  nullptr);
        if (hdrOutputMemory_ != VK_NULL_HANDLE) vkFreeMemory       (rtx().device, hdrOutputMemory_, nullptr);

        if (prevHdrOutputView_ != VK_NULL_HANDLE) vkDestroyImageView (rtx().device, prevHdrOutputView_,   nullptr);
        if (prevHdrOutputImage_ != VK_NULL_HANDLE) vkDestroyImage     (rtx().device, prevHdrOutputImage_,  nullptr);
        if (prevHdrOutputMemory_ != VK_NULL_HANDLE) vkFreeMemory       (rtx().device, prevHdrOutputMemory_, nullptr);

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

        // Input polling & handling (WASD, mouse, ESC quit)
        bool quit = false, fullscreen_toggle = false;
        int w = 0, h = 0;
        sdl_poll_events(w, h, quit, fullscreen_toggle);

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

        bool needsResize = minimized_ ||
                           (currentW != window_width_) ||
                           (currentH != window_height_);

        if (needsResize) {
            vkDeviceWaitIdle(rtx().device);

            onResize(currentW, currentH);

            SDL_GetWindowSizeInPixels(window_, &currentW, &currentH);
            window_width_  = currentW;
            window_height_ = currentH;

            minimized_ = false;
        }

        if (!Swapchain::get() || !hdrOutputImage_ || !hdrOutputView_) {
            LOG_WARNING_CAT("RAYCANVAS", "Invalid state — skipping frame");
            return;
        }

        if (firstFrame_) {
            TotalTime::get().seal();
            firstFrame_ = false;
            LOG_AMOURANTH("Genesis sealed — eternal canvas begins 💖");
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
            LOG_WARNING_CAT("SWAPCHAIN", "Acquire out-of-date/suboptimal — recreate next frame");
            minimized_ = true;
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

        // Adaptive internal resolution — base = window size, scale UP only for quality (no zoom out)
        double delta_s = now - lastPresentTime_s_;
        lastPresentTime_s_ = now;

        double frameBudget = 1000000.0 / measuredRefreshRateHz_;
        double safetyMargin = frameBudget * 0.05;

        // Scale up when under budget (higher quality/supersampling)
        if (delta_s < frameBudget - safetyMargin) {
            adaptiveScale_ = std::min(adaptiveScale_ + 0.02f, 4.0f);  // max 4x for quality boost
        }
        // Never scale down — lock at min 1.0 (no zoom out)
        adaptiveScale_ = std::max(adaptiveScale_, 1.0f);

        // Respect Options::Rendering max caps (e.g. your 90% setting)
        uint32_t maxW = static_cast<uint32_t>(Options::Rendering::INTERNAL_WIDTH);
        uint32_t maxH = static_cast<uint32_t>(Options::Rendering::INTERNAL_HEIGHT);

        uint32_t targetW = static_cast<uint32_t>(std::clamp(static_cast<float>(window_width_) * adaptiveScale_, 320.0f, static_cast<float>(maxW)));
        uint32_t targetH = static_cast<uint32_t>(std::clamp(static_cast<float>(window_height_) * adaptiveScale_, 180.0f, static_cast<float>(maxH)));

        Pipeline::dispatch_canvas(cmd, static_cast<int>(targetW), static_cast<int>(targetH), static_cast<float>(now));

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
        flipBlit.srcOffsets[1] = { static_cast<int32_t>(targetW), static_cast<int32_t>(targetH), 1 };

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
        } else if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) {
            LOG_WARNING_CAT("SWAPCHAIN", "Present out-of-date/suboptimal — recreate next frame");
            minimized_ = true;
        } else if (pres != VK_SUCCESS) {
            LOG_ERROR_CAT("SWAPCHAIN", "Present failed: {}", vkh.result(pres));
            if (pres == VK_ERROR_SURFACE_LOST_KHR) destroyed_ = true;
        }

        // 5-second FPS logger
        if (now - lastFpsLog_ > 5.0) {
            LOG_INFO_CAT("MAIN", "Average FPS: ~{:.1f} (dt avg = {:.3f}s)", 
                         static_cast<double>(frameCount_) / (now - lastFpsLog_), (now - lastFpsLog_) / static_cast<double>(frameCount_));
            lastFpsLog_ = now;
            frameCount_ = 0;
        }
    }

    void onResize(int newWidth, int newHeight) noexcept {
        if (newWidth <= 0 || newHeight <= 0) {
            minimized_ = true;
            return;
        }

        if (newWidth == window_width_ && newHeight == window_height_) {
            return;
        }

        LOG_INFO_CAT("WINDOW", "Resizing to {}x{}", newWidth, newHeight);

        vkDeviceWaitIdle(rtx().device);

        // Destroy old HDR images & views
        if (hdrOutputView_ != VK_NULL_HANDLE)   vkDestroyImageView (rtx().device, hdrOutputView_, nullptr);
        if (hdrOutputImage_ != VK_NULL_HANDLE)  vkDestroyImage     (rtx().device, hdrOutputImage_, nullptr);
        if (hdrOutputMemory_ != VK_NULL_HANDLE) vkFreeMemory       (rtx().device, hdrOutputMemory_, nullptr);

        if (prevHdrOutputView_ != VK_NULL_HANDLE)   vkDestroyImageView (rtx().device, prevHdrOutputView_, nullptr);
        if (prevHdrOutputImage_ != VK_NULL_HANDLE)  vkDestroyImage     (rtx().device, prevHdrOutputImage_, nullptr);
        if (prevHdrOutputMemory_ != VK_NULL_HANDLE) vkFreeMemory       (rtx().device, prevHdrOutputMemory_, nullptr);

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

        updateInternalResolution();

        createPersistentHDR();
        createPreviousHDR();

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
                        window_width_, window_height_, internal_width_, internal_height_);
    }

    int  getWidth()  const noexcept { return window_width_; }
    int  getHeight() const noexcept { return window_height_; }
    bool isMinimized() const noexcept { return minimized_; }
    bool isDestroyed() const noexcept { return destroyed_; }

private:
    void updateInternalResolution() noexcept {
        // Base on actual window pixel size
        internal_width_  = window_width_;
        internal_height_ = window_height_;

        // Respect Options::Rendering max caps (your 90% setting)
        internal_width_  = std::min(internal_width_, Options::Rendering::INTERNAL_WIDTH);
        internal_height_ = std::min(internal_height_, Options::Rendering::INTERNAL_HEIGHT);

        // Optional game style min size (no zoom effect)
        using namespace Options::GameStyle;
        if (CurrentDimension == DimensionMode::Pure2D || CurrentDimension == DimensionMode::TwoPointFiveD) {
            internal_width_  = std::max(320, internal_width_);
            internal_height_ = std::max(180, internal_height_);
        }

        LOG_INFO_CAT("RAYCANVAS", "Internal resolution conformed to window: {}x{} (capped by Options::Rendering)", internal_width_, internal_height_);
    }

    void createDescriptorPoolAndSet() noexcept {
        VkDescriptorPoolSize poolSizes[] = {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   1},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1},  // materials
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  5}   // future expansion
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
        if (!hdrOutputView_) return;

        std::array<VkWriteDescriptorSet, 2> writes{};

        VkDescriptorImageInfo imgInfo{};
        imgInfo.imageView   = hdrOutputView_;
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        writes[0].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet           = descriptorSet_;
        writes[0].dstBinding       = 0;
        writes[0].descriptorCount  = 1;
        writes[0].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[0].pImageInfo       = &imgInfo;

        VkDescriptorBufferInfo matInfo{};
        matInfo.buffer = Memory::getBuffer(materialsHandle_);
        matInfo.offset = 0;
        matInfo.range  = VK_WHOLE_SIZE;

        writes[1].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet           = descriptorSet_;
        writes[1].dstBinding       = 4;
        writes[1].descriptorCount  = 1;
        writes[1].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[1].pBufferInfo      = &matInfo;

        vkUpdateDescriptorSets(rtx().device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }

    void createPersistentHDR() noexcept {
        vkDeviceWaitIdle(rtx().device);

        if (hdrOutputView_ != VK_NULL_HANDLE)   vkDestroyImageView(rtx().device, hdrOutputView_, nullptr);
        if (hdrOutputImage_ != VK_NULL_HANDLE)  vkDestroyImage(rtx().device, hdrOutputImage_, nullptr);
        if (hdrOutputMemory_ != VK_NULL_HANDLE) vkFreeMemory(rtx().device, hdrOutputMemory_, nullptr);

        hdrOutputView_   = VK_NULL_HANDLE;
        hdrOutputImage_  = VK_NULL_HANDLE;
        hdrOutputMemory_ = VK_NULL_HANDLE;

        VkImageCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.imageType = VK_IMAGE_TYPE_2D;
        ci.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        ci.extent = {static_cast<uint32_t>(internal_width_), static_cast<uint32_t>(internal_height_), 1};
        ci.mipLevels = 1;
        ci.arrayLayers = 1;
        ci.samples = VK_SAMPLE_COUNT_1_BIT;
        ci.tiling = VK_IMAGE_TILING_OPTIMAL;
        ci.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        vkh.checker(vkCreateImage(rtx().device, &ci, nullptr, &hdrOutputImage_), "MEMORY", "HDR Image");

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(rtx().device, hdrOutputImage_, &req);

        uint32_t memType = Memory::findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo mai{};
        mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize = req.size;
        mai.memoryTypeIndex = memType;

        vkh.checker(vkAllocateMemory(rtx().device, &mai, nullptr, &hdrOutputMemory_), "MEMORY", "HDR Memory");

        vkBindImageMemory(rtx().device, hdrOutputImage_, hdrOutputMemory_, 0);

        VkImageViewCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image = hdrOutputImage_;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkh.checker(vkCreateImageView(rtx().device, &vi, nullptr, &hdrOutputView_), "MEMORY", "HDR View");
    }

    void createPreviousHDR() noexcept {
        vkDeviceWaitIdle(rtx().device);

        if (prevHdrOutputView_ != VK_NULL_HANDLE)   vkDestroyImageView(rtx().device, prevHdrOutputView_, nullptr);
        if (prevHdrOutputImage_ != VK_NULL_HANDLE)  vkDestroyImage(rtx().device, prevHdrOutputImage_, nullptr);
        if (prevHdrOutputMemory_ != VK_NULL_HANDLE) vkFreeMemory(rtx().device, prevHdrOutputMemory_, nullptr);

        prevHdrOutputView_   = VK_NULL_HANDLE;
        prevHdrOutputImage_  = VK_NULL_HANDLE;
        prevHdrOutputMemory_ = VK_NULL_HANDLE;

        VkImageCreateInfo ci{};
        ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.imageType = VK_IMAGE_TYPE_2D;
        ci.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        ci.extent = {static_cast<uint32_t>(internal_width_), static_cast<uint32_t>(internal_height_), 1};
        ci.mipLevels = 1;
        ci.arrayLayers = 1;
        ci.samples = VK_SAMPLE_COUNT_1_BIT;
        ci.tiling = VK_IMAGE_TILING_OPTIMAL;
        ci.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        vkh.checker(vkCreateImage(rtx().device, &ci, nullptr, &prevHdrOutputImage_), "MEMORY", "Prev HDR Image");

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(rtx().device, prevHdrOutputImage_, &req);

        uint32_t memType = Memory::findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo mai{};
        mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize = req.size;
        mai.memoryTypeIndex = memType;

        vkh.checker(vkAllocateMemory(rtx().device, &mai, nullptr, &prevHdrOutputMemory_), "MEMORY", "Prev HDR Memory");

        vkBindImageMemory(rtx().device, prevHdrOutputImage_, prevHdrOutputMemory_, 0);

        VkImageViewCreateInfo vi{};
        vi.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image = prevHdrOutputImage_;
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkh.checker(vkCreateImageView(rtx().device, &vi, nullptr, &prevHdrOutputView_), "MEMORY", "Prev HDR View");
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
        fci.pNext = nullptr;
        fci.flags = 0;

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
            vkCmdClearColorImage(cmd, hdrOutputImage_, VK_IMAGE_LAYOUT_GENERAL, &clearBlack, 1, &range);
            vkCmdClearColorImage(cmd, prevHdrOutputImage_, VK_IMAGE_LAYOUT_GENERAL, &clearBlack, 1, &range);
            endSubmitAndWait(cmd);
        }
    }

private:
    SDL_Window*    window_           = nullptr;
    int            window_width_     = 0;
    int            window_height_    = 0;
    int            internal_width_   = 0;
    int            internal_height_  = 0;
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

    // Adaptive resolution state
    float adaptiveScale_;
    double lastPresentTime_s_;
    double measuredRefreshRateHz_;

    // FPS logger state
    double lastFpsLog_;
    uint64_t frameCount_;
};

inline RayCanvas* rayCanvas = nullptr;