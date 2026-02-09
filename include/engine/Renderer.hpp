#pragma once

#include "engine/AMOURANTHRTX.hpp"
#include "engine/ELLIE.hpp"
#include "engine/camera.hpp"
#include "engine/OptionsMenu.hpp"
#include "engine/Pipeline.hpp"

#include <glm/gtc/matrix_inverse.hpp>

#include <chrono>

// Simple material (expand later)
struct Material {
    glm::vec4 albedo   {1.0f};
    glm::vec4 emissive {0.0f};
};

// Camera UBO sent to shaders
struct CameraSceneData {
    glm::mat4 viewInverse;
    glm::mat4 projInverse;
    glm::mat4 view;
    glm::mat4 proj;

    glm::vec4 cameraPos;
    glm::vec4 prevCameraPos;

    float     exposure     = 1.0f;
    float     totalTime    = 0.0f;
    uint32_t  randomSeed   = 12345u;
    uint32_t  maxDepth     = 12;

    uint32_t  padding[2]   = {0, 0};
};

// =============================================================================
// VulkanRenderer — main class
// =============================================================================
class VulkanRenderer {
public:
    VulkanRenderer(int width, int height, SDL_Window* window)
        : window_(window),
          width_(width),
          height_(height),
          minimized_(false),
          destroyed_(false),
          last_time_(std::chrono::steady_clock::now()),
          defaultMaterialsHandle_(0),
          cameraUBOHandle_(0),
          hdrOutputImage_(VK_NULL_HANDLE),
          hdrOutputView_(VK_NULL_HANDLE),
          hdrOutputMemory_(VK_NULL_HANDLE),
          needsDescriptorUpdate_(true),
          needsSwapchainRecreate_(false)
    {
        LOG_INFO_CAT("RENDERER", "Initializing — {}x{}", width, height);

        cameraUBOHandle_ = Memory::create(sizeof(CameraSceneData),
                                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                          "CameraUBO");

        vkh.checker(cameraUBOHandle_, "Memory::create (CameraUBO)", "Failed");

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

        LOG_SUCCESS_CAT("RENDERER", "Initialized");
    }

    ~VulkanRenderer() {
        if (destroyed_) return;
        destroyed_ = true;

        LOG_INFO_CAT("RENDERER", "Shutting down — draining queues before cleanup");

        // Drain **all** queues — prevents pending cmd buffers & fences
        vkDeviceWaitIdle(rtx().device);
        vkQueueWaitIdle(rtx().graphics_queue);
        vkQueueWaitIdle(rtx().present_queue);
        if (rtx().compute_queue != VK_NULL_HANDLE) vkQueueWaitIdle(rtx().compute_queue);
        if (rtx().transfer_queue != VK_NULL_HANDLE) vkQueueWaitIdle(rtx().transfer_queue);

        vkDestroyImageView(rtx().device, hdrOutputView_, nullptr);
        vkDestroyImage(rtx().device, hdrOutputImage_, nullptr);
        vkFreeMemory(rtx().device, hdrOutputMemory_, nullptr);

        Memory::destroy(cameraUBOHandle_);
        Memory::destroy(defaultMaterialsHandle_);

        pipeline_shutdown();

        LOG_SUCCESS_CAT("RENDERER", "Shutdown complete");
    }

void pew() noexcept {
    if (destroyed_) return;

    auto now = std::chrono::steady_clock::now();
    TotalTime::get().advance(now - last_time_);
    last_time_ = now;

    double total_sec = TotalTime::get().seconds();

    vkQueueWaitIdle(rtx().graphics_queue);  // Extra safety
    vkQueueWaitIdle(rtx().present_queue);

    updateCameraUBO(total_sec);

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = rtx().transient_pool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkResult res = vkAllocateCommandBuffers(rtx().device, &allocInfo, &cmd);
    if (res != VK_SUCCESS) return;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    res = vkBeginCommandBuffer(cmd, &beginInfo);
    if (res != VK_SUCCESS) {
        vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &cmd);
        return;
    }

    pipeline_dispatch_living_world(cmd, total_sec);

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

    transitionImageLayout(cmd, hdrOutputImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    pipeline_trace_rays(cmd, static_cast<uint32_t>(width_), static_cast<uint32_t>(height_));
    transitionImageLayout(cmd, hdrOutputImage_, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    res = vkEndCommandBuffer(cmd);
    if (res != VK_SUCCESS) {
        vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &cmd);
        return;
    }

    VkFence frameFence = VK_NULL_HANDLE;
    VkFenceCreateInfo fenceCI{};
    fenceCI.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    vkCreateFence(rtx().device, &fenceCI, nullptr, &frameFence);

    VkSubmitInfo submit{};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &cmd;

    res = vkQueueSubmit(rtx().graphics_queue, 1, &submit, frameFence);
    if (res != VK_SUCCESS) {
        const char* err = vkh.result(res);
        fprintf(stderr, "[RENDERER FATAL] vkQueueSubmit failed: %s\n", err ? err : "unknown");

        if (res == VK_ERROR_DEVICE_LOST) {
            fprintf(stderr, "[FATAL] DEVICE LOST — GPU context dead. Restart required.\n");
            destroyed_ = true;  // Stop all future rendering
        }

        vkDestroyFence(rtx().device, frameFence, nullptr);
        vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &cmd);
        return;
    }

    // Longer timeout — give GPU time (especially with ray tracing)
    res = vkWaitForFences(rtx().device, 1, &frameFence, VK_TRUE, 30ULL * 1000'000'000ULL);  // 30 seconds
    if (res != VK_SUCCESS) {
        fprintf(stderr, "[RENDERER] Fence wait failed: %s\n", vkh.result(res));
        if (res == VK_TIMEOUT) {
            fprintf(stderr, "[WARNING] Fence timeout — GPU may be hung\n");
        } else if (res == VK_ERROR_DEVICE_LOST) {
            fprintf(stderr, "[FATAL] DEVICE LOST during wait\n");
            destroyed_ = true;
        }
    }

    vkResetFences(rtx().device, 1, &frameFence);  // Safe now that we waited
    vkDestroyFence(rtx().device, frameFence, nullptr);
    vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &cmd);

    // Blit & present (best effort)
    if (rtx().images != VK_NULL_HANDLE) {
        VkCommandBuffer blitCmd = VK_NULL_HANDLE;
        vkAllocateCommandBuffers(rtx().device, &allocInfo, &blitCmd);

        vkBeginCommandBuffer(blitCmd, &beginInfo);

        VkImage swapImage = rtx().images;
        transitionImageLayout(blitCmd, swapImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkImageBlit blit{};
        blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.srcOffsets[0] = {0, 0, 0};
        blit.srcOffsets[1] = {width_, height_, 1};
        blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        blit.dstOffsets[0] = {0, 0, 0};
        blit.dstOffsets[1] = {width_, height_, 1};

        vkCmdBlitImage(blitCmd,
                       hdrOutputImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       swapImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &blit, VK_FILTER_LINEAR);

        transitionImageLayout(blitCmd, swapImage,
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        vkEndCommandBuffer(blitCmd);

        VkSubmitInfo blitSubmit{};
        blitSubmit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        blitSubmit.commandBufferCount   = 1;
        blitSubmit.pCommandBuffers      = &blitCmd;

        vkQueueSubmit(rtx().graphics_queue, 1, &blitSubmit, VK_NULL_HANDLE);

        VkSemaphore dummySem = VK_NULL_HANDLE;
        Swapchain::presentImage(rtx().graphics_queue, 0, dummySem);

        vkFreeCommandBuffers(rtx().device, rtx().transient_pool, 1, &blitCmd);
    }
}

    void onResize(int newWidth, int newHeight) noexcept {
        if (newWidth <= 0 || newHeight <= 0) {
            minimized_ = true;
            LOG_WARNING_CAT("RENDERER", "Window minimized");
            return;
        }

        if (newWidth == width_ && newHeight == height_) return;

        // Drain queues before touching swapchain or HDR image
        vkDeviceWaitIdle(rtx().device);
        vkQueueWaitIdle(rtx().graphics_queue);
        vkQueueWaitIdle(rtx().present_queue);

        width_  = newWidth;
        height_ = newHeight;
        minimized_ = false;
        needsSwapchainRecreate_ = true;

        LOG_INFO_CAT("RENDERER", "Resize — {}x{}", width_, height_);

        // Recreate HDR target
        createOrRecreateHDRImage();

        // Force descriptor update next frame
        needsDescriptorUpdate_ = true;
    }

private:
    void createOrRecreateHDRImage() noexcept {
        LOG_INFO_CAT("RENDERER", "Creating/recreating HDR image — {}x{}", width_, height_);

        // Already waited in onResize — but belt-and-suspenders
        vkDeviceWaitIdle(rtx().device);

        vkDestroyImageView(rtx().device, hdrOutputView_, nullptr);
        vkDestroyImage(rtx().device, hdrOutputImage_, nullptr);
        vkFreeMemory(rtx().device, hdrOutputMemory_, nullptr);

        hdrOutputView_ = VK_NULL_HANDLE;
        hdrOutputImage_ = VK_NULL_HANDLE;
        hdrOutputMemory_ = VK_NULL_HANDLE;

        VkImageCreateInfo hdrInfo{};
        hdrInfo.sType       = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        hdrInfo.imageType   = VK_IMAGE_TYPE_2D;
        hdrInfo.format      = VK_FORMAT_R32G32B32A32_SFLOAT;
        hdrInfo.extent      = {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1};
        hdrInfo.mipLevels   = 1;
        hdrInfo.arrayLayers = 1;
        hdrInfo.samples     = VK_SAMPLE_COUNT_1_BIT;
        hdrInfo.tiling      = VK_IMAGE_TILING_OPTIMAL;
        hdrInfo.usage       = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        hdrInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        vkh.checker(
            vkCreateImage(rtx().device, &hdrInfo, nullptr, &hdrOutputImage_),
            "vkCreateImage (HDR)",
            "Failed"
        );

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(rtx().device, hdrOutputImage_, &memReqs);

        uint32_t memType = Memory::findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        vkh.checker(memType != ~0u, "findMemoryType (HDR)", "No device-local memory type");

        VkMemoryAllocateInfo mai{};
        mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize  = memReqs.size;
        mai.memoryTypeIndex = memType;

        vkh.checker(
            vkAllocateMemory(rtx().device, &mai, nullptr, &hdrOutputMemory_),
            "vkAllocateMemory (HDR)",
            "Failed"
        );

        vkh.checker(
            vkBindImageMemory(rtx().device, hdrOutputImage_, hdrOutputMemory_, 0),
            "vkBindImageMemory (HDR)",
            "Failed"
        );

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image            = hdrOutputImage_;
        viewInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format           = VK_FORMAT_R32G32B32A32_SFLOAT;
        viewInfo.components       = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                     VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkh.checker(
            vkCreateImageView(rtx().device, &viewInfo, nullptr, &hdrOutputView_),
            "vkCreateImageView (HDR)",
            "Failed"
        );

        needsDescriptorUpdate_ = true;

        LOG_SUCCESS_CAT("RENDERER", "HDR image ready — view {}", (uintptr_t)hdrOutputView_);
    }

    void updateCameraUBO(float totalTime) noexcept {
        CameraSceneData data{};

        data.view       = CAM.view();
        data.proj       = CAM.projection(static_cast<float>(width_) / static_cast<float>(height_));
        data.viewInverse = glm::inverse(data.view);
        data.projInverse = glm::inverse(data.proj);

        data.cameraPos     = glm::vec4(CAM.position(), 1.0f);
        data.prevCameraPos = data.cameraPos;

        data.exposure   = 1.0f;
        data.totalTime  = totalTime;
        data.randomSeed = static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count() & 0xFFFFFFFFu);
        data.maxDepth   = 12;

        // Fixed: capture staging (even though camera UBO is small/host-visible, we follow the contract)
        auto [stgBuf, stgMem] = Memory::uploadToBuffer(cameraUBOHandle_, &data, sizeof(data));

        // Since this is typically called inside a recording scope or without external cmd,
        // and uploadToBuffer handles internal cleanup for owned mode, staging should be null here.
        // But for consistency/safety, clean if returned (rare case)
        if (stgBuf != VK_NULL_HANDLE) {
            vkDestroyBuffer(rtx().device, stgBuf, nullptr);
            vkFreeMemory(rtx().device, stgMem, nullptr);
        }
    }

    void updateGlobalDescriptorBuffer() noexcept {
        VkAccelerationStructureKHR tlas = getTLAS();
        vkh.checker(tlas != VK_NULL_HANDLE, "getTLAS (descriptor update)",
                    "No valid TLAS");

        RTDescriptorUpdate update{};
        update.tlas            = tlas;
        update.rtOutputView    = hdrOutputView_;
        update.ubo             = rtx().buffers[cameraUBOHandle_].buffer;
        update.uboSize         = sizeof(CameraSceneData);
        update.materialsBuffer = rtx().buffers[defaultMaterialsHandle_].buffer;
        update.materialsSize   = sizeof(Material) * 1;

        pipeline_write_rt_descriptors(update);
        needsDescriptorUpdate_ = false;

        LOG_SUCCESS_CAT("RENDERER", "Descriptor buffer updated — TLAS {}, HDR view {}",
                        (uintptr_t)tlas, (uintptr_t)hdrOutputView_);
    }

    void transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout) noexcept {
        if (oldLayout == newLayout || image == VK_NULL_HANDLE) return;

        VkImageMemoryBarrier barrier{};
        barrier.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout        = oldLayout;
        barrier.newLayout        = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image            = image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            dstStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
            dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }

        vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0,
                             0, nullptr, 0, nullptr, 1, &barrier);
    }

private:
    SDL_Window*                     window_;
    int                             width_, height_;
    bool                            minimized_;
    bool                            destroyed_;
    std::chrono::steady_clock::time_point last_time_;
    uint64_t                        defaultMaterialsHandle_;
    uint64_t                        cameraUBOHandle_;
    VkImage                         hdrOutputImage_;
    VkImageView                     hdrOutputView_;
    VkDeviceMemory                  hdrOutputMemory_;
    bool                            needsDescriptorUpdate_;
    bool                            needsSwapchainRecreate_;
};