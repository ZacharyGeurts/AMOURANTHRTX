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
//   • Blits to swapchain when timing allows
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

        // Optional: default materials buffer (can be removed if not used in shader)
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

        // Upload default materials (persistent map preferred)
        if (auto* info = Memory::get(defaultMaterialsHandle_)) {
            if (info->mapped) {
                std::memcpy(info->mapped, defaultMats.data(), sizeof(defaultMats));
                LOG_SUCCESS_CAT("RAYCANVAS", "Default materials uploaded (persistent map)");
            } else {
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
        createDescriptorPoolAndSet();
        Pipeline::initialize();
        Pipeline::create_pipeline_layout();
        Pipeline::create_canvas_pipeline();

        LOG_SUCCESS_CAT("RAYCANVAS", "Single-shader compute canvas initialized");
    }

    ~RayCanvas() {
        if (destroyed_) return;
        destroyed_ = true;

        LOG_INFO_CAT("RAYCANVAS", "Shutting down — waiting for device idle");

        vkDeviceWaitIdle(rtx().device);

        // Destroy descriptor set / pool
        if (descriptorSet_ != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(rtx().device, descriptorPool_, 1, &descriptorSet_);
            descriptorSet_ = VK_NULL_HANDLE;
        }
        if (descriptorPool_ != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(rtx().device, descriptorPool_, nullptr);
            descriptorPool_ = VK_NULL_HANDLE;
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

        // If we created a dummy world buffer, clean it up
        if (dummyWorldBufferHandle_ != 0) {
            Memory::destroy(dummyWorldBufferHandle_);
        }

        // Shutdown the single pipeline
        Pipeline::shutdown();

        LOG_SUCCESS_CAT("RAYCANVAS", "Shutdown complete");
    }

    // ────────────────────────────────────────────────────────────────
    // Decide whether to dispatch canvas shader this frame
    // ────────────────────────────────────────────────────────────────
    void maybeUpdateCanvas() noexcept {
        if (destroyed_ || minimized_) return;

        TotalTime& tt = TotalTime::get();

        if (firstFrame_) {
            tt.seal();
            firstFrame_ = false;
            LOG_AMOURANTH("Genesis sealed — eternal single-shader canvas begins 💖");
            return;
        }

        if (!Swapchain::shouldPresentNow()) {
            return;
        }

        double now = tt.seconds();

        fprintf(stderr, "\033[38;2;255;147;41m[UPDATE] Canvas dispatch @ %.3f s\033[0m\n", now);

        updateCameraUBO(now);

        VkCommandBuffer cmd = beginTransientCommandBuffer();
        if (!cmd) return;

        // Update descriptor set with current camera UBO + HDR view
        updateDescriptorSet();

        // Transition HDR to GENERAL for shader write
        transitionImageLayout(cmd, hdrOutputImage_,
                              VK_IMAGE_LAYOUT_UNDEFINED,
                              VK_IMAGE_LAYOUT_GENERAL);

        // Bind descriptor set and dispatch the ONE shader
        VkDescriptorSet set = descriptorSet_;
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                Pipeline::pipeline_layout, 0, 1, &set, 0, nullptr);

        Pipeline::dispatch_canvas(cmd, static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), static_cast<float>(now));

        // Transition HDR to TRANSFER_SRC for blit
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

        // Re-update descriptor set with new HDR view
        updateDescriptorSet();
    }

    // Getters
    int    getWidth()  const noexcept { return width_; }
    int    getHeight() const noexcept { return height_; }
    bool   isMinimized() const noexcept { return minimized_; }
    bool   isDestroyed() const noexcept { return destroyed_; }

private:
    // ────────────────────────────────────────────────────────────────
    // Create descriptor pool + one set (called once in constructor)
    // ────────────────────────────────────────────────────────────────
    void createDescriptorPoolAndSet() noexcept {
        VkDescriptorPoolSize poolSizes[3] = {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  1},   // binding 0
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1},   // binding 1
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}    // binding 2
        };

        VkDescriptorPoolCreateInfo poolCI{};
        poolCI.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolCI.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolCI.maxSets       = 1;
        poolCI.poolSizeCount = 3;
        poolCI.pPoolSizes    = poolSizes;

        vkh.checker(vkCreateDescriptorPool(rtx().device, &poolCI, nullptr, &descriptorPool_),
                    "vkCreateDescriptorPool", "Failed");

        VkDescriptorSetAllocateInfo allocCI{};
        allocCI.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocCI.descriptorPool     = descriptorPool_;
        allocCI.descriptorSetCount = 1;
        allocCI.pSetLayouts        = &Pipeline::main_descriptor_layout;

        vkh.checker(vkAllocateDescriptorSets(rtx().device, &allocCI, &descriptorSet_),
                    "vkAllocateDescriptorSets", "Failed");

        LOG_SUCCESS_CAT("RAYCANVAS", "Descriptor pool + set created for canvas");
    }

    // ────────────────────────────────────────────────────────────────
    // Ensure we always have a valid storage buffer for binding 2
    // ────────────────────────────────────────────────────────────────
    void ensureValidWorldBuffer() noexcept {
        if (rtx().living_world_buffer_handle != 0) {
            return; // real world buffer already exists → use it
        }

        if (dummyWorldBufferHandle_ == 0) {
            // Create tiny dummy buffer — matches your previous range comment
            dummyWorldBufferHandle_ = Memory::createBuffer(
                64,
                VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                "DummyLivingWorld"
            );

            if (dummyWorldBufferHandle_ == 0) {
                LOG_ERROR_CAT("RAYCANVAS", "Failed to create dummy world buffer — validation errors incoming");
                return;
            }

            // Zero it out
            char zero[64] = {};
            auto [stagingBuf, stagingMem] = Memory::uploadToBuffer(
                dummyWorldBufferHandle_, zero, sizeof(zero));
            if (stagingBuf != VK_NULL_HANDLE) {
                vkDestroyBuffer(rtx().device, stagingBuf, nullptr);
                vkFreeMemory(rtx().device, stagingMem, nullptr);
            }

            LOG_INFO_CAT("RAYCANVAS", "Created dummy 64-byte world buffer to satisfy validation");
        }
    }

    // ────────────────────────────────────────────────────────────────
    // Update descriptor set — now always uses valid buffers
    // ────────────────────────────────────────────────────────────────
    void updateDescriptorSet() noexcept {
        ensureValidWorldBuffer();  // guarantees a valid buffer for binding 2

        VkWriteDescriptorSet writes[3]{};

        // 0: HDR storage image
        VkDescriptorImageInfo hdrImageInfo{};
        hdrImageInfo.imageView   = hdrOutputView_;
        hdrImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        hdrImageInfo.sampler     = VK_NULL_HANDLE;

        writes[0].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[0].dstSet           = descriptorSet_;
        writes[0].dstBinding       = 0;
        writes[0].dstArrayElement  = 0;
        writes[0].descriptorCount  = 1;
        writes[0].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[0].pImageInfo       = &hdrImageInfo;

        // 1: Camera UBO
        VkDescriptorBufferInfo camBufferInfo{};
        camBufferInfo.buffer = Memory::getBuffer(cameraUBOHandle_);
        camBufferInfo.offset = 0;
        camBufferInfo.range  = sizeof(CameraSceneData);

        writes[1].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1].dstSet           = descriptorSet_;
        writes[1].dstBinding       = 1;
        writes[1].dstArrayElement  = 0;
        writes[1].descriptorCount  = 1;
        writes[1].descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        writes[1].pBufferInfo      = &camBufferInfo;

        // 2: World storage buffer — always valid now
        VkDescriptorBufferInfo worldBufferInfo{};
        uint64_t worldHandle = (rtx().living_world_buffer_handle != 0)
            ? rtx().living_world_buffer_handle
            : dummyWorldBufferHandle_;

        worldBufferInfo.buffer = Memory::getBuffer(worldHandle);
        worldBufferInfo.offset = 0;
        worldBufferInfo.range  = 64;  // keep consistent with dummy size

        writes[2].sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[2].dstSet           = descriptorSet_;
        writes[2].dstBinding       = 2;
        writes[2].dstArrayElement  = 0;
        writes[2].descriptorCount  = 1;
        writes[2].descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[2].pBufferInfo      = &worldBufferInfo;

        vkUpdateDescriptorSets(rtx().device, 3, writes, 0, nullptr);

        LOG_DEBUG_CAT("RAYCANVAS", "Descriptor set updated for frame (bindings 0=img, 1=cam, 2=world)");
    }

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

        VkPipelineStageFlags src = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkPipelineStageFlags dst = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
            b.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            dst = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
            b.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            b.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            src = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
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
    uint64_t                        dummyWorldBufferHandle_     = 0;  // ← new: fallback buffer
    VkImage                         hdrOutputImage_             = VK_NULL_HANDLE;
    VkImageView                     hdrOutputView_              = VK_NULL_HANDLE;
    VkDeviceMemory                  hdrOutputMemory_            = VK_NULL_HANDLE;

    VkDescriptorPool                descriptorPool_            = VK_NULL_HANDLE;
    VkDescriptorSet                 descriptorSet_             = VK_NULL_HANDLE;
};