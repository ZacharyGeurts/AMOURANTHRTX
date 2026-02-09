#pragma once

#include "AMOURANTHRTX.hpp"
#include "ELLIE.hpp"
#include "camera.hpp"
#include "OptionsMenu.hpp"
#include "Pipeline.hpp"

#include <glm/gtc/matrix_inverse.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// Simple material (expand later)
// ─────────────────────────────────────────────────────────────────────────────
struct Material {
    glm::vec4 albedo   {1.0f};
    glm::vec4 emissive {0.0f};
};

// ─────────────────────────────────────────────────────────────────────────────
// Camera UBO sent to shaders — time is genesis-relative
// ─────────────────────────────────────────────────────────────────────────────
struct CameraSceneData {
    glm::mat4 viewInverse;
    glm::mat4 projInverse;
    glm::mat4 view;
    glm::mat4 proj;

    glm::vec4 cameraPos;
    glm::vec4 prevCameraPos;

    float     exposure     = 1.0f;
    float     genesisTime  = 0.0f;     // seconds since genesis
    uint32_t  randomSeed   = 12345u;
    uint32_t  maxDepth     = 12;

    uint32_t  padding[2]   = {0, 0};
};

// =============================================================================
// VulkanRenderer — Genesis Time Philosophy Edition
// Single-image swapchain — no acquire, direct blit + present index 0 on VSync
// =============================================================================
class VulkanRenderer {
public:
    VulkanRenderer(int width, int height, SDL_Window* window)
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
          needsDescriptorUpdate_(true),
          needsSwapchainRecreate_(false)
    {
        LOG_INFO_CAT("RENDERER", "Initializing — {}x{}", width, height);

        // One true buffer for camera
        cameraUBOHandle_ = Memory::create(sizeof(CameraSceneData),
                                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                          "CameraUBO");

        vkh.checker(cameraUBOHandle_, "Memory::create (CameraUBO)", "Failed");

        // Default single material
        std::array<Material, 1> defaultMats{};
        defaultMaterialsHandle_ = Memory::createDescriptorBuffer(sizeof(defaultMats), "DefaultMaterials");

        vkh.checker(defaultMaterialsHandle_, "Memory::createDescriptorBuffer (Materials)", "Failed");

        void* mapped = Memory::lazyMapDescriptor(defaultMaterialsHandle_);
        vkh.checker(mapped, "Memory::lazyMapDescriptor (Materials)", "Failed to map");
        std::memcpy(mapped, defaultMats.data(), sizeof(defaultMats));
        LOG_SUCCESS_CAT("RENDERER", "Default materials uploaded");

        createOrRecreateHDRImage();

        pipeline_initialize();

        updateGlobalDescriptorBuffer();

        LOG_SUCCESS_CAT("RENDERER", "Renderer genesis complete");
    }

    ~VulkanRenderer() {
        if (destroyed_) return;
        destroyed_ = true;

        LOG_INFO_CAT("RENDERER", "Shutting down — draining queues");

        vkDeviceWaitIdle(rtx().device);

        vkDestroyImageView(rtx().device, hdrOutputView_, nullptr);
        vkDestroyImage(rtx().device, hdrOutputImage_, nullptr);
        vkFreeMemory(rtx().device, hdrOutputMemory_, nullptr);

        Memory::destroy(cameraUBOHandle_);
        Memory::destroy(defaultMaterialsHandle_);

        pipeline_shutdown();

        LOG_SUCCESS_CAT("RENDERER", "Shutdown complete — returning to the void");
    }

    // ── Single render tick — everything relative to genesis ──────────────────
    void pew() noexcept {
        if (destroyed_ || minimized_) return;

        TotalTime& totalTime = TotalTime::get();

        // Seal on first real frame (genesis moment captured)
        if (firstFrame_) {
            totalTime.seal();
            firstFrame_ = false;
            LOG_AMOURANTH("Genesis sealed — eternal clock starts now 💖");
        }

        float genesis_sec = static_cast<float>(totalTime.seconds());

        fprintf(stderr, "\033[38;2;255;147;41m[HOT] Frame @ genesis %.3f s\033[0m\n", genesis_sec);

        updateCameraUBO(genesis_sec);

        VkCommandBuffer cmd = beginTransientCommandBuffer();
        if (cmd == VK_NULL_HANDLE) return;

        pipeline_dispatch_living_world(cmd, genesis_sec);

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

        // Blit to swapchain image (index 0) and present on VSync
        if (rtx().images != VK_NULL_HANDLE) {
            blitToSwapchainAndPresent();
        }

        fprintf(stderr, "\033[38;2;255;147;41m[HOT] Frame complete @ %.3f s\033[0m\n", genesis_sec);
    }

    void onResize(int newWidth, int newHeight) noexcept {
        if (newWidth <= 0 || newHeight <= 0) {
            minimized_ = true;
            LOG_WARNING_CAT("RENDERER", "Window minimized");
            return;
        }

        if (newWidth == width_ && newHeight == height_) return;

        vkDeviceWaitIdle(rtx().device);

        width_  = newWidth;
        height_ = newHeight;
        minimized_ = false;
        needsSwapchainRecreate_ = true;

        LOG_INFO_CAT("RENDERER", "Resize — {}x{}", width_, height_);

        createOrRecreateHDRImage();
        needsDescriptorUpdate_ = true;
    }

private:
    // ── Helpers ──────────────────────────────────────────────────────────────

    VkCommandBuffer beginTransientCommandBuffer() noexcept {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = rtx().transient_pool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        if (vkAllocateCommandBuffers(rtx().device, &allocInfo, &cmd) != VK_SUCCESS) {
            return VK_NULL_HANDLE;
        }

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
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

        vkWaitForFences(rtx().device, 1, &fence, VK_TRUE, 60'000'000'000ULL);
        vkResetFences(rtx().device, 1, &fence);
        vkDestroyFence(rtx().device, fence, nullptr);
        vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &cmd);
    }

    void blitToSwapchainAndPresent() noexcept {
        VkCommandBuffer blitCmd = beginTransientCommandBuffer();
        if (blitCmd == VK_NULL_HANDLE) return;

        VkImage swapImage = rtx().images;

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

        // Direct present — no acquire, index 0 only
        Swapchain::presentImage(rtx().graphics_queue);
    }

    void createOrRecreateHDRImage() noexcept {
        vkDeviceWaitIdle(rtx().device);

        vkDestroyImageView(rtx().device, hdrOutputView_, nullptr);
        vkDestroyImage(rtx().device, hdrOutputImage_, nullptr);
        vkFreeMemory(rtx().device, hdrOutputMemory_, nullptr);

        hdrOutputView_   = VK_NULL_HANDLE;
        hdrOutputImage_  = VK_NULL_HANDLE;
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

        vkh.checker(vkCreateImage(rtx().device, &ci, nullptr, &hdrOutputImage_),
                    "vkCreateImage (HDR)", "Failed");

        VkMemoryRequirements req{};
        vkGetImageMemoryRequirements(rtx().device, hdrOutputImage_, &req);

        uint32_t memType = Memory::findMemoryType(req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo mai{};
        mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize  = req.size;
        mai.memoryTypeIndex = memType;

        vkh.checker(vkAllocateMemory(rtx().device, &mai, nullptr, &hdrOutputMemory_),
                    "vkAllocateMemory (HDR)", "Failed");

        vkBindImageMemory(rtx().device, hdrOutputImage_, hdrOutputMemory_, 0);

        VkImageViewCreateInfo vi{};
        vi.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vi.image            = hdrOutputImage_;
        vi.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        vi.format           = VK_FORMAT_R32G32B32A32_SFLOAT;
        vi.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkh.checker(vkCreateImageView(rtx().device, &vi, nullptr, &hdrOutputView_),
                    "vkCreateImageView (HDR)", "Failed");

        needsDescriptorUpdate_ = true;
        LOG_SUCCESS_CAT("RENDERER", "HDR output ready {}x{}", width_, height_);
    }

    void updateCameraUBO(float genesisTime) noexcept {
        CameraSceneData data{};

        data.view       = CAM.view();
        data.proj       = CAM.projection(static_cast<float>(width_) / static_cast<float>(height_));
        data.viewInverse = glm::inverse(data.view);
        data.projInverse = glm::inverse(data.proj);

        data.cameraPos     = glm::vec4(CAM.position(), 1.0f);

        data.exposure      = 1.0f;
        data.genesisTime   = genesisTime;
        data.randomSeed    = static_cast<uint32_t>(genesisTime * 1'000'000.0f) ^ 0xCAFEBABEu;

        auto [stagingBuf, stagingMem] = Memory::uploadToBuffer(cameraUBOHandle_, &data, sizeof(data));
        if (stagingBuf != VK_NULL_HANDLE) {
            vkDestroyBuffer(rtx().device, stagingBuf, nullptr);
            vkFreeMemory(rtx().device, stagingMem, nullptr);
        }
    }

    void updateGlobalDescriptorBuffer() noexcept {
        VkAccelerationStructureKHR tlas = getTLAS();
        if (tlas == VK_NULL_HANDLE) {
            LOG_ERROR_CAT("RENDERER", "Cannot update descriptors — no valid TLAS");
            return;
        }

        RTDescriptorUpdate upd{};
        upd.tlas            = tlas;
        upd.rtOutputView    = hdrOutputView_;
        upd.ubo             = rtx().buffers[cameraUBOHandle_].buffer;
        upd.uboSize         = sizeof(CameraSceneData);
        upd.materialsBuffer = rtx().buffers[defaultMaterialsHandle_].buffer;
        upd.materialsSize   = sizeof(Material) * 1;

        pipeline_write_rt_descriptors(upd);
        needsDescriptorUpdate_ = false;

        LOG_SUCCESS_CAT("RENDERER", "Global RT descriptors updated");
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

private:
    SDL_Window*                     window_;
    int                             width_, height_;
    bool                            minimized_;
    bool                            destroyed_;
    bool                            firstFrame_;
    uint64_t                        defaultMaterialsHandle_;
    uint64_t                        cameraUBOHandle_;
    VkImage                         hdrOutputImage_     = VK_NULL_HANDLE;
    VkImageView                     hdrOutputView_      = VK_NULL_HANDLE;
    VkDeviceMemory                  hdrOutputMemory_    = VK_NULL_HANDLE;
    bool                            needsDescriptorUpdate_;
    bool                            needsSwapchainRecreate_;
};