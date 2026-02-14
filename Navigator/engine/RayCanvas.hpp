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
// Camera uniform data block — sent to the single compute shader
// ─────────────────────────────────────────────────────────────────────────────
struct CameraSceneData {
    glm::mat4 viewInverse;
    glm::mat4 projInverse;
    glm::mat4 view;
    glm::mat4 proj;

    glm::vec4 cameraPos;
    glm::vec4 prevCameraPos;

    double    exposure     = 1.0;
    double    genesisTime  = 0.0;
    uint32_t  randomSeed   = 12345u;
    uint32_t  maxDepth     = 12;

    uint32_t  padding[2]   = {0, 0};
};

// =============================================================================
// RayCanvas — Persistent progressive compute canvas
//   • Single compute shader renders to HDR storage image
//   • Blits directly to the acquired swapchain image
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
          descriptorPool_(VK_NULL_HANDLE),
          descriptorSet_(VK_NULL_HANDLE)
    {
        LOG_INFO_CAT("RAYCANVAS", "Initializing single-shader compute canvas — {}x{}", width, height);

        // Camera uniform buffer (host-visible + coherent)
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

        // Optional default materials buffer
        std::array<Material, 1> defaultMats{};
        defaultMaterialsHandle_ = Memory::createBuffer(
            sizeof(defaultMats),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            "DefaultMaterials"
        );

        if (defaultMaterialsHandle_ == 0) {
            LOG_FATAL_CAT("RAYCANVAS", "Failed to allocate default materials buffer");
            std::abort();
        }

        // Upload defaults (prefer persistent map)
        if (auto* info = Memory::get(defaultMaterialsHandle_)) {
            if (info->mapped) {
                std::memcpy(info->mapped, defaultMats.data(), sizeof(defaultMats));
            } else {
                auto [stagingBuf, stagingMem] = Memory::uploadToBuffer(
                    defaultMaterialsHandle_, defaultMats.data(), sizeof(defaultMats));
                if (stagingBuf != VK_NULL_HANDLE) {
                    vkDestroyBuffer(rtx().device, stagingBuf, nullptr);
                    vkFreeMemory(rtx().device, stagingMem, nullptr);
                }
            }
        }

        createPersistentHDR();
        createDescriptorPoolAndSet();

        // Pipeline setup now safe (main_descriptor_layout created in Pipeline::initialize)
        Pipeline::initialize();
        Pipeline::create_pipeline_layout();
        Pipeline::create_canvas_pipeline();

        LOG_SUCCESS_CAT("RAYCANVAS", "Single-shader compute canvas initialized");
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

        if (dummyWorldBufferHandle_ != 0) {
            Memory::destroy(dummyWorldBufferHandle_);
        }

        Pipeline::shutdown();

        LOG_SUCCESS_CAT("RAYCANVAS", "Shutdown complete");
    }

    void maybeUpdateCanvas() noexcept {
        if (destroyed_ || minimized_) return;

        TotalTime& tt = TotalTime::get();

        if (firstFrame_) {
            tt.seal();
            firstFrame_ = false;
            LOG_AMOURANTH("Genesis sealed — eternal single-shader canvas begins 💖");
            return;
        }

        double now = tt.seconds();

        updateCameraUBO(now);
        updateDescriptorSet();

        // ── ACQUIRE NEXT IMAGE ──────────────────────────────────────────────
        uint32_t imageIndex;

        VkFence acquireFence = VK_NULL_HANDLE;
        VkFenceCreateInfo fenceCI{};
        fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        vkCreateFence(rtx().device, &fenceCI, nullptr, &acquireFence);

        VkResult acquireRes = ext().vkAcquireNextImageKHR(
            rtx().device,
            Swapchain::get(),
            UINT64_MAX,
            VK_NULL_HANDLE,
            acquireFence,
            &imageIndex
        );

        if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR || acquireRes == VK_SUBOPTIMAL_KHR) {
            minimized_ = true;
            LOG_WARNING_CAT("SWAPCHAIN", "Acquire out-of-date/suboptimal — needs recreate");
            vkDestroyFence(rtx().device, acquireFence, nullptr);
            return;
        }

        if (acquireRes != VK_SUCCESS) {
            LOG_ERROR_CAT("SWAPCHAIN", "vkAcquireNextImageKHR failed: {}", vkh.result(acquireRes));
            vkDestroyFence(rtx().device, acquireFence, nullptr);
            return;
        }

        // Wait for acquire fence before using the image
        vkWaitForFences(rtx().device, 1, &acquireFence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(rtx().device, acquireFence, nullptr);

        // ── Render to HDR ───────────────────────────────────────────────────
        VkCommandBuffer cmd = beginTransientCommandBuffer();
        if (!cmd) return;

        transitionImageLayout(cmd, hdrOutputImage_,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_GENERAL);

        VkDescriptorSet set = descriptorSet_;
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                Pipeline::pipeline_layout, 0, 1, &set, 0, nullptr);

        Pipeline::dispatch_canvas(cmd, static_cast<uint32_t>(width_),
                                  static_cast<uint32_t>(height_), static_cast<float>(now));

        VkImageMemoryBarrier postBarrier{};
        postBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        postBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        postBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        postBarrier.oldLayout     = VK_IMAGE_LAYOUT_GENERAL;
        postBarrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        postBarrier.image         = hdrOutputImage_;
        postBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             0, 0, nullptr, 0, nullptr, 1, &postBarrier);

        endSubmitAndWait(cmd);

        // ── Blit HDR → acquired swapchain image ─────────────────────────────
        VkCommandBuffer blitCmd = beginTransientCommandBuffer();
        if (!blitCmd) return;

        VkImage swapImage = Swapchain::images[imageIndex];

        transitionImageLayout(blitCmd, swapImage,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkImageBlit blit{};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {width_, height_, 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {width_, height_, 1};

        vkCmdBlitImage(blitCmd,
                       hdrOutputImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       swapImage,       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit, VK_FILTER_LINEAR);

        transitionImageLayout(blitCmd, swapImage,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        endSubmitAndWait(blitCmd);

        // ── Present ─────────────────────────────────────────────────────────
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains    = &Swapchain::swapchain.value;
        presentInfo.pImageIndices  = &imageIndex;

        VkResult presentRes = ext().vkQueuePresentKHR(rtx().present_queue, &presentInfo);

        if (presentRes == VK_SUCCESS) {
            Swapchain::updateRefreshEstimate(now);
        } else if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR) {
            minimized_ = true;
            LOG_WARNING_CAT("SWAPCHAIN", "Present out-of-date/suboptimal");
        } else {
            LOG_ERROR_CAT("SWAPCHAIN", "Present failed: {}", vkh.result(presentRes));
        }
    }

    void onResize(int newWidth, int newHeight) noexcept {
        if (newWidth <= 0 || newHeight <= 0) {
            minimized_ = true;
            return;
        }

        if (newWidth == width_ && newHeight == height_) return;

        vkDeviceWaitIdle(rtx().device);

        width_  = newWidth;
        height_ = newHeight;
        minimized_ = false;

        Swapchain::recreate(width_, height_);
        createPersistentHDR();

        updateDescriptorSet();
    }

    int    getWidth()  const noexcept { return width_; }
    int    getHeight() const noexcept { return height_; }
    bool   isMinimized() const noexcept { return minimized_; }
    bool   isDestroyed() const noexcept { return destroyed_; }

private:
    void createDescriptorPoolAndSet() noexcept {
        VkDescriptorPoolSize poolSizes[3] = {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,   1},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,  1},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,  1}
        };

        VkDescriptorPoolCreateInfo poolCI{};
        poolCI.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCI.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolCI.maxSets       = 1;
        poolCI.poolSizeCount = 3;
        poolCI.pPoolSizes    = poolSizes;

        vkh.checker(vkCreateDescriptorPool(rtx().device, &poolCI, nullptr, &descriptorPool_),
                    "vkCreateDescriptorPool", "RayCanvas");

        VkDescriptorSetAllocateInfo allocCI{};
        allocCI.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocCI.descriptorPool     = descriptorPool_;
        allocCI.descriptorSetCount = 1;
        allocCI.pSetLayouts        = &Pipeline::main_descriptor_layout;

        vkh.checker(vkAllocateDescriptorSets(rtx().device, &allocCI, &descriptorSet_),
                    "vkAllocateDescriptorSets", "RayCanvas");
    }

    void ensureValidWorldBuffer() noexcept {
        if (rtx().living_world_buffer_handle != 0) return;

        if (dummyWorldBufferHandle_ == 0) {
            dummyWorldBufferHandle_ = Memory::createBuffer(
                64,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                "DummyWorld"
            );

            char zero[64] = {};
            auto [sb, sm] = Memory::uploadToBuffer(dummyWorldBufferHandle_, zero, sizeof(zero));
            if (sb != VK_NULL_HANDLE) {
                vkDestroyBuffer(rtx().device, sb, nullptr);
                vkFreeMemory(rtx().device, sm, nullptr);
            }
        }
    }

    void updateDescriptorSet() noexcept {
        ensureValidWorldBuffer();

        VkWriteDescriptorSet writes[3]{};

        VkDescriptorImageInfo imgInfo{};
        imgInfo.imageView   = hdrOutputView_;
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        writes[0].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet           = descriptorSet_;
        writes[0].dstBinding       = 0;
        writes[0].descriptorCount  = 1;
        writes[0].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[0].pImageInfo       = &imgInfo;

        VkDescriptorBufferInfo uboInfo{};
        uboInfo.buffer = Memory::getBuffer(cameraUBOHandle_);
        uboInfo.range  = sizeof(CameraSceneData);

        writes[1].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet           = descriptorSet_;
        writes[1].dstBinding       = 1;
        writes[1].descriptorCount  = 1;
        writes[1].descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[1].pBufferInfo      = &uboInfo;

        VkDescriptorBufferInfo bufInfo{};
        uint64_t worldHandle = rtx().living_world_buffer_handle ? rtx().living_world_buffer_handle : dummyWorldBufferHandle_;
        bufInfo.buffer = Memory::getBuffer(worldHandle);
        bufInfo.range  = 64;

        writes[2].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet           = descriptorSet_;
        writes[2].dstBinding       = 2;
        writes[2].descriptorCount  = 1;
        writes[2].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[2].pBufferInfo      = &bufInfo;

        vkUpdateDescriptorSets(rtx().device, 3, writes, 0, nullptr);
    }

    void createPersistentHDR() noexcept {
        vkDeviceWaitIdle(rtx().device);

        if (hdrOutputView_)   vkDestroyImageView  (rtx().device, hdrOutputView_,   nullptr);
        if (hdrOutputImage_)  vkDestroyImage      (rtx().device, hdrOutputImage_,  nullptr);
        if (hdrOutputMemory_) vkFreeMemory        (rtx().device, hdrOutputMemory_, nullptr);

        hdrOutputView_ = VK_NULL_HANDLE;
        hdrOutputImage_ = VK_NULL_HANDLE;
        hdrOutputMemory_ = VK_NULL_HANDLE;

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
            LOG_ERROR_CAT("RAYCANVAS", "vkCreateImage (HDR) failed");
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
            LOG_ERROR_CAT("RAYCANVAS", "vkAllocateMemory (HDR) failed");
            vkDestroyImage(rtx().device, hdrOutputImage_, nullptr);
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
            LOG_ERROR_CAT("RAYCANVAS", "vkCreateImageView (HDR) failed");
            return;
        }

        LOG_SUCCESS_CAT("RAYCANVAS", "Persistent HDR canvas created — {}x{}", width_, height_);
    }

    void updateCameraUBO(double genesisTime) noexcept {
        CameraSceneData data{};

        data.view        = CAM.view();
        data.proj        = CAM.projection(static_cast<float>(width_) / static_cast<float>(height_));
        data.viewInverse = glm::inverse(data.view);
        data.projInverse = glm::inverse(data.proj);

        data.cameraPos    = glm::vec4(CAM.position(), 1.0f);
        data.exposure     = 1.0;
        data.genesisTime  = genesisTime;
        data.randomSeed   = static_cast<uint32_t>(genesisTime * 1'000'000.0) ^ 0xCAFEBABEu;

        auto [stagingBuf, stagingMem] = Memory::uploadToBuffer(cameraUBOHandle_, &data, sizeof(data));
        if (stagingBuf != VK_NULL_HANDLE) {
            vkDestroyBuffer(rtx().device, stagingBuf, nullptr);
            vkFreeMemory(rtx().device, stagingMem, nullptr);
        }
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

        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
            b.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
            b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
            b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            b.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            b.srcAccessMask = 0;
            b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }

        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &b);
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
    uint64_t                        dummyWorldBufferHandle_     = 0;
    VkImage                         hdrOutputImage_             = VK_NULL_HANDLE;
    VkImageView                     hdrOutputView_              = VK_NULL_HANDLE;
    VkDeviceMemory                  hdrOutputMemory_            = VK_NULL_HANDLE;

    VkDescriptorPool                descriptorPool_            = VK_NULL_HANDLE;
    VkDescriptorSet                 descriptorSet_             = VK_NULL_HANDLE;
};