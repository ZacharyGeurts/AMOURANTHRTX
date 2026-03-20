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
          commandBuffer_(VK_NULL_HANDLE),
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
        Swapchain::get();

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
        createCommandBuffer();
        updateRenderResolution();
        createHDRResources();
        createDescriptorPoolAndSet();
        updateDescriptorSet();
        clearHDRImages();

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

        if (commandBuffer_ != VK_NULL_HANDLE)
            vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &commandBuffer_);

        destroyHDRResources();
        Memory::destroy(materialsHandle_);

        LOG_SUCCESS_CAT("RAYCANVAS", "Destroyed");
    }

void maybeUpdateCanvas() noexcept {
    if (destroyed_) return;

    Pipeline::processInput(window_, window_width_, window_height_);

    Swapchain::get();

    if (firstFrame_) {
        TotalTime::get().seal();
        firstFrame_ = false;
        LOG_AMOURANTH("Raymarch engine sealed — rendering begins 💖");
        lastAdaptiveAdjustTime_ = TotalTime::get().seconds();
    }

	uint32_t acquiredImageIndex = 0;

    vkResetFences(rtx().device, 1, &acquireFence_);

    ext().vkAcquireNextImageKHR(rtx().device, Swapchain::get(), UINT64_MAX,
                                    VK_NULL_HANDLE, acquireFence_, &acquiredImageIndex);
    vkWaitForFences(rtx().device, 1, &acquireFence_, VK_TRUE, UINT64_MAX);

    VkCommandBuffer cmd = beginCommandBuffer();
    if (!cmd) return;

    vkCmdResetQueryPool(cmd, timestampQueryPool_, 0, 2);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, timestampQueryPool_, 0);

    transitionImageLayout(cmd, mainHDR_.image,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    updateRenderResolution();

    bool isRt = Options::Rendering::EnableHardwareRayTracing &&
                (Options::Rendering::CurrentTechnique == Options::Rendering::RenderTechnique::HardwareRayTracing) &&
                rtx().rayTracingSupported;

    VkDescriptorSet set = descriptorSet_;
    vkCmdBindDescriptorSets(cmd,
                            isRt ? VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR : VK_PIPELINE_BIND_POINT_COMPUTE,
                            Pipeline::pipeline_layout,
                            0, 1, &set, 0, nullptr);

    Pipeline::dispatch(cmd, render_width_, render_height_, static_cast<float>(TotalTime::get().seconds()));

    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, timestampQueryPool_, 1);

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

    VkImage swapImg = Swapchain::images[acquiredImageIndex];

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
    uint64_t ts[2]{};
    vkGetQueryPoolResults(rtx().device, timestampQueryPool_, 0, 2,
                 sizeof(ts), ts, sizeof(uint64_t),
                 VK_QUERY_RESULT_64_BIT);
    double gpuMs = double(ts[1] - ts[0]) * timestampPeriodNs_ / 1'000'000.0;
    double alpha = (gpuMs > smoothedGpuTimeMs_) ? 0.70 : 0.22;
    smoothedGpuTimeMs_ = (1.0 - alpha) * smoothedGpuTimeMs_ + alpha * gpuMs;

    adjustAdaptiveScale();

    VkPresentInfoKHR pi{};
    pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    pi.swapchainCount = 1;
    pi.pSwapchains    = &Swapchain::swapchain.value;
    pi.pImageIndices  = &acquiredImageIndex;

    ext().vkQueuePresentKHR(rtx().present_queue, &pi);
    Swapchain::updateRefreshEstimate();
    measuredRefreshRateHz_ = 1.0 / Swapchain::getSmoothedRefresh();
}

    [[nodiscard]] int  getWidth()  const noexcept { return window_width_; }
    [[nodiscard]] int  getHeight() const noexcept { return window_height_; }
    [[nodiscard]] bool isMinimized() const noexcept { return minimized_; }
    [[nodiscard]] bool isDestroyed() const noexcept { return destroyed_; }

private:

    void updateRenderResolution() noexcept {
        if (!Options::Rendering::EnableAdaptiveResolution) {
            render_width_ = window_width_;
            render_height_ = window_height_;
            return;
        }

        double s = adaptiveScale_;
        auto w = static_cast<int64_t>(std::round(static_cast<double>(window_width_) * s));
        auto h = static_cast<int64_t>(std::round(static_cast<double>(window_height_) * s));

        w = std::clamp(w, int64_t(1), int64_t(15360)); // Autoadaptive up to 16K too boring?
        h = std::clamp(h, int64_t(1), int64_t(8640));

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
            "MaterialsLibrary"
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

    void createCommandBuffer() noexcept {
        VkCommandBufferAllocateInfo a{};
        a.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        a.commandPool        = rtx().transient_pool;
        a.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        a.commandBufferCount = 1;
        vkAllocateCommandBuffers(rtx().device, &a, &commandBuffer_);
    }

    VkCommandBuffer beginCommandBuffer() noexcept {
        VkResult resetRes = vkResetCommandBuffer(
            commandBuffer_,
            VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT
        );

		vkh.checker(resetRes, "vkResetCommandBuffer", "Failed to reset command buffer");

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        VkResult beginRes = vkBeginCommandBuffer(commandBuffer_, &beginInfo);

		vkh.checker(beginRes, "vkBeginCommandBuffer", "Failed to begin command buffer");

        return commandBuffer_;
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
        if (!mainHDR_.view || !prevHDR_.view) return;

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

        VkCommandBuffer cmd = beginCommandBuffer();
        if (!cmd) return;

        transitionImageLayout(cmd, mainHDR_.image,  VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
        transitionImageLayout(cmd, prevHDR_.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        vkCmdClearColorImage(cmd, mainHDR_.image,  VK_IMAGE_LAYOUT_GENERAL, &black, 1, &rng);
        vkCmdClearColorImage(cmd, prevHDR_.image, VK_IMAGE_LAYOUT_GENERAL, &black, 1, &rng);

        endSubmitAndWait(cmd);
    }

    void adjustAdaptiveScale() noexcept {
        double elapsed = TotalTime::get().seconds() - lastAdaptiveAdjustTime_;
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
        }

        lastAdaptiveAdjustTime_ = TotalTime::get().seconds();
    }

    void applyGameStyleDefaults() noexcept {
        switch (Options::GameStyle::CurrentDimension) {
            case Options::GameStyle::DimensionMode::TextOnly:
                Options::Camera::CurrentFOV = 90.0f;
                Options::Rendering::CurrentTechnique = Options::Rendering::RenderTechnique::Pure2DCanvas;
                break;
            case Options::GameStyle::DimensionMode::Pure2D:
                Options::Camera::CurrentFOV = 90.0f;
                Options::Rendering::CurrentTechnique = Options::Rendering::RenderTechnique::Pure2DCanvas;
                break;
            case Options::GameStyle::DimensionMode::TwoPointFiveD:
                Options::Camera::CurrentFOV = 75.0f;
                Options::Rendering::CurrentTechnique = Options::Rendering::RenderTechnique::HybridRasterMarch;
                break;
            case Options::GameStyle::DimensionMode::Full3D:
                Options::Camera::CurrentFOV = 75.0f;
                Options::Rendering::CurrentTechnique = Options::Rendering::RenderTechnique::PureRaymarching;
                break;
        }

        switch (Options::GameStyle::CurrentPerspective) {
            case Options::GameStyle::CameraPerspective::FirstPerson:
                Options::Camera::CurrentFOV = 85.0f;
                break;
            case Options::GameStyle::CameraPerspective::ThirdPerson:
                Options::Camera::CurrentFOV = 70.0f;
                break;
            case Options::GameStyle::CameraPerspective::TopDown:
            case Options::GameStyle::CameraPerspective::Isometric:
            case Options::GameStyle::CameraPerspective::SideScroller:
            case Options::GameStyle::CameraPerspective::Orthographic2D:
            case Options::GameStyle::CameraPerspective::TextAdventure:
                // No special FOV override for these cases
                break;
        }
    }

private:
    SDL_Window*    window_                    = nullptr;

    int            window_width_              = 0;
    int            window_height_             = 0;
    int            render_width_              = 0;
    int            render_height_             = 0;

    VkCommandBuffer commandBuffer_            = VK_NULL_HANDLE;
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

    // HDR resources (after scalars)
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