#pragma once

#include "AMOURANTHRTX.hpp"
#include "ELLIE.hpp"
#include "camera.hpp"
#include "OptionsMenu.hpp"
#include "Pipeline.hpp"

#include <glm/gtc/matrix_inverse.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// Simple material (expand later as needed)
// ─────────────────────────────────────────────────────────────────────────────
struct Material {
    glm::vec4 albedo   {1.0f, 1.0f, 1.0f, 1.0f};
    glm::vec4 emissive {0.0f, 0.0f, 0.0f, 0.0f};
};

// ─────────────────────────────────────────────────────────────────────────────
// Camera uniform data block — sent to shaders
// ─────────────────────────────────────────────────────────────────────────────
struct CameraSceneData {
    glm::mat4 viewInverse;
    glm::mat4 projInverse;
    glm::mat4 view;
    glm::mat4 proj;

    glm::vec4 cameraPos;
    glm::vec4 prevCameraPos;

    double    exposure     = 1.0;
    double    genesisTime  = 0.0;     // seconds since engine start
    uint32_t  randomSeed   = 12345u;
    uint32_t  maxDepth     = 12;

    uint32_t  padding[2]   = {0, 0};
};

// =============================================================================
// RayCanvas — Persistent progressive path tracing canvas
//   • Renders to HDR storage image
//   • Blits to swapchain image when timing allows
//   • Single-image swapchain + controlled present timing
//   • Procedural AABB geometry only (no triangle meshes)
// =============================================================================
class RayCanvas {
public:
    RayCanvas(int width, int height, SDL_Window* window)
        : window_(window),
          width_(width),
          height_(height),
          minimized_(false),
          destroyed_(false),
          firstFrame_(true),
          defaultMaterialsHandle_(0),
          cameraUBOHandle_(0),
          hdrOutputImage_(VK_NULL_HANDLE),
          hdrOutputView_(VK_NULL_HANDLE),
          hdrOutputMemory_(VK_NULL_HANDLE),
          needsDescriptorUpdate_(true)
    {
        LOG_INFO_CAT("RAYCANVAS", "Initializing canvas — {}x{}", width, height);

        // Camera uniform buffer (typically host-visible + coherent)
        cameraUBOHandle_ = Memory::createBuffer(
            sizeof(CameraSceneData),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            "CameraUBO"
        );

        if (cameraUBOHandle_ == 0) {
            LOG_FATAL_CAT("RAYCANVAS", "Failed to allocate Camera UBO");
            std::abort();
        }

        // Default materials — descriptor buffer style
        std::array<Material, 1> defaultMats{};
        defaultMaterialsHandle_ = Memory::createBuffer(
            sizeof(defaultMats),
            VK_BUFFER_USAGE_RESOURCE_DESCRIPTOR_BUFFER_BIT_EXT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            "DefaultMaterials",
            Memory::MemoryHint::DescriptorBuffer
        );

        if (defaultMaterialsHandle_ == 0) {
            LOG_FATAL_CAT("RAYCANVAS", "Failed to allocate default materials buffer");
            std::abort();
        }

        // Upload materials (prefer persistent map if available)
        if (auto* info = Memory::get(defaultMaterialsHandle_)) {
            if (info->mapped) {
                std::memcpy(info->mapped, defaultMats.data(), sizeof(defaultMats));
                LOG_SUCCESS_CAT("RAYCANVAS", "Default materials uploaded (persistent mapping)");
            } else {
                // Fallback: staging copy
                auto [stagingBuf, stagingMem] = Memory::uploadToBuffer(
                    defaultMaterialsHandle_, defaultMats.data(), sizeof(defaultMats));
                if (stagingBuf != VK_NULL_HANDLE) {
                    vkDestroyBuffer(rtx().device, stagingBuf, nullptr);
                    vkFreeMemory(rtx().device, stagingMem, nullptr);
                }
                LOG_SUCCESS_CAT("RAYCANVAS", "Default materials uploaded via staging");
            }
        }

        createPersistentHDR();

        pipeline_initialize();

        updateGlobalDescriptorBuffer();

        LOG_SUCCESS_CAT("RAYCANVAS", "Persistent canvas initialized successfully");
    }

    ~RayCanvas() {
        if (destroyed_) return;
        destroyed_ = true;

        LOG_INFO_CAT("RAYCANVAS", "Shutting down — waiting for device idle");

        vkDeviceWaitIdle(rtx().device);

        if (hdrOutputView_ != VK_NULL_HANDLE) {
            vkDestroyImageView(rtx().device, hdrOutputView_, nullptr);
        }
        if (hdrOutputImage_ != VK_NULL_HANDLE) {
            vkDestroyImage(rtx().device, hdrOutputImage_, nullptr);
        }
        if (hdrOutputMemory_ != VK_NULL_HANDLE) {
            vkFreeMemory(rtx().device, hdrOutputMemory_, nullptr);
        }

        Memory::destroy(cameraUBOHandle_);
        Memory::destroy(defaultMaterialsHandle_);

        pipeline_shutdown();

        LOG_SUCCESS_CAT("RAYCANVAS", "Shutdown complete");
    }

    // ────────────────────────────────────────────────────────────────
    // Decide whether to trace + blit this frame
    // ────────────────────────────────────────────────────────────────
    void maybeUpdateCanvas() noexcept {
        if (destroyed_ || minimized_) return;

        TotalTime& tt = TotalTime::get();

        if (firstFrame_) {
            tt.seal();
            firstFrame_ = false;
            LOG_AMOURANTH("Genesis sealed — eternal canvas begins 💖");
            return;
        }

        if (!Swapchain::shouldPresentNow()) {
            // Skip trace/blit — not time yet
            return;
        }

        double now = tt.seconds();

        fprintf(stderr, "\033[38;2;255;147;41m[UPDATE] Canvas trace @ %.3f s\033[0m\n", now);

        updateCameraUBO(now);

        VkCommandBuffer cmd = beginTransientCommandBuffer();
        if (!cmd) return;

        pipeline_dispatch_living_world(cmd, static_cast<float>(now));

        VkMemoryBarrier barrier{};
        barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT;

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                             0, 1, &barrier, 0, nullptr, 0, nullptr);

        if (needsDescriptorUpdate_) {
            updateGlobalDescriptorBuffer();
        }

        transitionImageLayout(cmd, hdrOutputImage_,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_GENERAL);

        pipeline_trace_rays(cmd, static_cast<uint32_t>(width_), static_cast<uint32_t>(height_));

        transitionImageLayout(cmd, hdrOutputImage_,
                              VK_IMAGE_LAYOUT_GENERAL,
                              VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        endSubmitAndWait(cmd);

        blitToSwapchain();

        Swapchain::tryPresent(rtx().present_queue);
    }

    void onResize(int newWidth, int newHeight) noexcept {
        if (newWidth <= 0 || newHeight <= 0) {
            minimized_ = true;
            LOG_WARNING_CAT("RAYCANVAS", "Window minimized");
            return;
        }

        if (newWidth == width_ && newHeight == height_) return;

        vkDeviceWaitIdle(rtx().device);

        width_  = newWidth;
        height_ = newHeight;
        minimized_ = false;

        LOG_INFO_CAT("RAYCANVAS", "Resize → {}x{}", width_, height_);

        Swapchain::recreate(width_, height_);

        createPersistentHDR();

        needsDescriptorUpdate_ = true;
    }

    // Getters
    int    getWidth()           const noexcept { return width_; }
    int    getHeight()          const noexcept { return height_; }
    bool   isMinimized()        const noexcept { return minimized_; }
    bool   isDestroyed()        const noexcept { return destroyed_; }

private:
    void blitToSwapchain() noexcept {
        VkCommandBuffer cmd = beginTransientCommandBuffer();
        if (!cmd) return;

        transitionImageLayout(cmd, rtx().images,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkImageBlit blit{};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {width_, height_, 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {width_, height_, 1};

        vkCmdBlitImage(cmd,
                       hdrOutputImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       rtx().images,    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit, VK_FILTER_LINEAR);

        transitionImageLayout(cmd, rtx().images,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        endSubmitAndWait(cmd);
    }

    void createPersistentHDR() noexcept {
        vkDeviceWaitIdle(rtx().device);

        // Cleanup previous HDR resources
        if (hdrOutputView_ != VK_NULL_HANDLE) {
            vkDestroyImageView(rtx().device, hdrOutputView_, nullptr);
            hdrOutputView_ = VK_NULL_HANDLE;
        }
        if (hdrOutputImage_ != VK_NULL_HANDLE) {
            vkDestroyImage(rtx().device, hdrOutputImage_, nullptr);
            hdrOutputImage_ = VK_NULL_HANDLE;
        }
        if (hdrOutputMemory_ != VK_NULL_HANDLE) {
            vkFreeMemory(rtx().device, hdrOutputMemory_, nullptr);
            hdrOutputMemory_ = VK_NULL_HANDLE;
        }

        VkImageCreateInfo ci{};
        ci.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        ci.imageType   = VK_IMAGE_TYPE_2D;
        ci.format      = VK_FORMAT_R32G32B32A32_SFLOAT;
        ci.extent      = {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1};
        ci.mipLevels   = 1;
        ci.arrayLayers = 1;
        ci.samples     = VK_SAMPLE_COUNT_1_BIT;
        ci.tiling      = VK_IMAGE_TILING_OPTIMAL;
        ci.usage       = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (vkCreateImage(rtx().device, &ci, nullptr, &hdrOutputImage_) != VK_SUCCESS) {
            LOG_ERROR_CAT("RAYCANVAS", "vkCreateImage (HDR output) failed");
            return;
        }

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(rtx().device, hdrOutputImage_, &req);

        uint32_t memType = Memory::findMemoryType(req.memoryTypeBits,
                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo mai{};
        mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize  = req.size;
        mai.memoryTypeIndex = memType;

        if (vkAllocateMemory(rtx().device, &mai, nullptr, &hdrOutputMemory_) != VK_SUCCESS) {
            LOG_ERROR_CAT("RAYCANVAS", "vkAllocateMemory (HDR output) failed");
            vkDestroyImage(rtx().device, hdrOutputImage_, nullptr);
            hdrOutputImage_ = VK_NULL_HANDLE;
            return;
        }

        vkBindImageMemory(rtx().device, hdrOutputImage_, hdrOutputMemory_, 0);

        VkImageViewCreateInfo vi{};
        vi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image            = hdrOutputImage_;
        vi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        vi.format           = VK_FORMAT_R32G32B32A32_SFLOAT;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        if (vkCreateImageView(rtx().device, &vi, nullptr, &hdrOutputView_) != VK_SUCCESS) {
            LOG_ERROR_CAT("RAYCANVAS", "vkCreateImageView (HDR output) failed");
            return;
        }

        needsDescriptorUpdate_ = true;
        LOG_SUCCESS_CAT("RAYCANVAS", "Persistent HDR target created — {}x{}", width_, height_);
    }

    void updateCameraUBO(double genesisTime) noexcept {
        CameraSceneData data{};

        data.view       = CAM.view();
        data.proj       = CAM.projection(static_cast<float>(width_) / static_cast<float>(height_));
        data.viewInverse = glm::inverse(data.view);
        data.projInverse = glm::inverse(data.proj);

        data.cameraPos     = glm::vec4(CAM.position(), 1.0f);
        // prevCameraPos intentionally left unchanged here — update if reprojection needed

        data.exposure      = 1.0;
        data.genesisTime   = genesisTime;
        data.randomSeed    = static_cast<uint32_t>(genesisTime * 1'000'000.0) ^ 0xCAFEBABEu;

        // Properly handle the return value (staging buffer/memory) to avoid warning
        auto [stagingBuf, stagingMem] = Memory::uploadToBuffer(cameraUBOHandle_, &data, sizeof(data));
        if (stagingBuf != VK_NULL_HANDLE) {
            vkDestroyBuffer(rtx().device, stagingBuf, nullptr);
            vkFreeMemory(rtx().device, stagingMem, nullptr);
        }
    }

    void updateGlobalDescriptorBuffer() noexcept {
        VkAccelerationStructureKHR tlas = getTLAS();
        if (tlas == VK_NULL_HANDLE) {
            LOG_ERROR_CAT("RAYCANVAS", "Cannot update RT descriptors — TLAS invalid");
            return;
        }

        RTDescriptorUpdate upd{};
        upd.tlas            = tlas;
        upd.rtOutputView    = hdrOutputView_;
        upd.ubo             = Memory::getBuffer(cameraUBOHandle_);
        upd.uboSize         = sizeof(CameraSceneData);
        upd.materialsBuffer = Memory::getBuffer(defaultMaterialsHandle_);
        upd.materialsSize   = sizeof(Material);   // × number of materials if array grows

        pipeline_write_rt_descriptors(upd);
        needsDescriptorUpdate_ = false;

        LOG_SUCCESS_CAT("RAYCANVAS", "Global ray-tracing descriptors refreshed");
    }

    void transitionImageLayout(VkCommandBuffer cmd, VkImage img,
                               VkImageLayout oldLayout, VkImageLayout newLayout) noexcept {
        if (oldLayout == newLayout || img == VK_NULL_HANDLE) return;

        VkImageMemoryBarrier b{};
        b.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        b.oldLayout        = oldLayout;
        b.newLayout        = newLayout;
        b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        b.image            = img;
        b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkPipelineStageFlags src = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkPipelineStageFlags dst = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
            b.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            dst = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        } else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
            b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            src = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
            dst = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
            b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            src = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dst = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }

        vkCmdPipelineBarrier(cmd, src, dst, 0, 0, nullptr, 0, nullptr, 1, &b);
    }

    VkCommandBuffer beginTransientCommandBuffer() noexcept {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo alloc{};
        alloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc.commandPool        = rtx().transient_pool;
        alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(rtx().device, &alloc, &cmd) != VK_SUCCESS) {
            return VK_NULL_HANDLE;
        }

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS) {
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
        VkFenceCreateInfo fenceCI{};
        fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

        vkCreateFence(rtx().device, &fenceCI, nullptr, &fence);

        VkSubmitInfo submit{};
        submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount   = 1;
        submit.pCommandBuffers      = &cmd;

        VkResult res = vkQueueSubmit(rtx().graphics_queue, 1, &submit, fence);
        if (res != VK_SUCCESS) {
            if (res == VK_ERROR_DEVICE_LOST) destroyed_ = true;
            vkDestroyFence(rtx().device, fence, nullptr);
            vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &cmd);
            return;
        }

        vkWaitForFences(rtx().device, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(rtx().device, fence, nullptr);
        vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &cmd);
    }

private:
    SDL_Window*                     window_                     = nullptr;
    int                             width_                      = 0;
    int                             height_                     = 0;
    bool                            minimized_                  = false;
    bool                            destroyed_                  = false;
    bool                            firstFrame_                 = true;
    uint64_t                        defaultMaterialsHandle_     = 0;
    uint64_t                        cameraUBOHandle_            = 0;
    VkImage                         hdrOutputImage_             = VK_NULL_HANDLE;
    VkImageView                     hdrOutputView_              = VK_NULL_HANDLE;
    VkDeviceMemory                  hdrOutputMemory_            = VK_NULL_HANDLE;
    bool                            needsDescriptorUpdate_      = true;
};