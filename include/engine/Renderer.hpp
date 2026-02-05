#pragma once

#include "engine/AMOURANTHRTX.hpp"
#include "engine/ELLIE.hpp"
#include "engine/camera.hpp"
#include "engine/OptionsMenu.hpp"
#include "engine/Pipeline.hpp"

#include <glm/gtc/matrix_inverse.hpp>

#include <vector>
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
          currentRingIndex_(0),
          defaultMaterialsHandle_(0),
          cameraUBOHandle_(0),
          hdrOutputImage_(VK_NULL_HANDLE),
          hdrOutputView_(VK_NULL_HANDLE),
          hdrOutputMemory_(VK_NULL_HANDLE),
          needsDescriptorUpdate_(true),
          needsSwapchainRecreate_(false)
    {
        LOG_INFO_CAT("RENDERER", "Initializing pure light engine — {}x{}", width, height);

        // Command buffer ring — triple buffering using global transient pool
        constexpr uint32_t CMD_RING_SIZE = 3;
        cmdRing_.resize(CMD_RING_SIZE);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = rtx().transient_pool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(cmdRing_.size());

        VkResult res = vkAllocateCommandBuffers(rtx().device, &allocInfo, cmdRing_.data());
        if (res != VK_SUCCESS) {
            LOG_FATAL_CAT("RENDERER", "Failed to allocate command buffer ring: {}", string_VkResult(res));
            return;
        }

        LOG_SUCCESS_CAT("RENDERER", "Command buffer ring allocated — {} buffers", CMD_RING_SIZE);

        // Camera uniform buffer
        cameraUBOHandle_ = Memory::create(sizeof(CameraSceneData),
                                          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                                          VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                          "CameraUBO");

        if (cameraUBOHandle_ == 0) {
            LOG_FATAL_CAT("RENDERER", "Failed to create CameraUBO");
            return;
        }

        // Default materials (descriptor buffer)
        std::array<Material, 1> defaultMats{};
        defaultMaterialsHandle_ = Memory::createDescriptorBuffer(sizeof(defaultMats), "DefaultMaterials");
        void* mapped = Memory::lazyMapDescriptor(defaultMaterialsHandle_);
        if (mapped) {
            std::memcpy(mapped, defaultMats.data(), sizeof(defaultMats));
            LOG_SUCCESS_CAT("RENDERER", "Default materials uploaded to descriptor buffer");
        } else {
            LOG_FATAL_CAT("RENDERER", "Failed to map default materials descriptor buffer");
        }

        // HDR output storage image
        createOrRecreateHDRImage();

        // Initialize pipeline (layouts, pipelines, SBT)
        pipeline_initialize();

        // Initial descriptor write
        updateGlobalDescriptorBuffer();

        LOG_SUCCESS_CAT("RENDERER", "Pure light engine fully initialized and ready");
    }

    ~VulkanRenderer() {
        if (destroyed_) return;
        destroyed_ = true;

        LOG_INFO_CAT("RENDERER", "Shutting down pure light engine");

        vkDeviceWaitIdle(rtx().device);

        vkDestroyImageView(rtx().device, hdrOutputView_, nullptr);
        vkDestroyImage(rtx().device, hdrOutputImage_, nullptr);
        vkFreeMemory(rtx().device, hdrOutputMemory_, nullptr);

        Memory::destroy(cameraUBOHandle_);
        Memory::destroy(defaultMaterialsHandle_);

        if (!cmdRing_.empty()) {
            vkFreeCommandBuffers(rtx().device, rtx().transient_pool,
                                 static_cast<uint32_t>(cmdRing_.size()), cmdRing_.data());
            LOG_INFO_CAT("RENDERER", "Command buffer ring freed");
        }

        pipeline_shutdown();

        LOG_SUCCESS_CAT("RENDERER", "Shutdown complete");
    }

void pew() noexcept {
    if (destroyed_) return;

    // Always advance time — zero cost, keeps everything alive
    auto now = std::chrono::steady_clock::now();
    TotalTime::get().advance(now - last_time_);
    last_time_ = now;

    float total_sec = static_cast<float>(TotalTime::get().seconds());

    // Quick check: is the swapchain actually ready for rendering?
    // If not → skip **everything** except time advance
    if (minimized_ || !Swapchain::swapchain_.valid()) {
        LOG_INFO_CAT("RENDERER", "Swapchain not ready/minimized — time only ({:.3f}s), zero cost frame", total_sec);
        return;
    }

    // Acquire image — this is the real gate. If acquire fails, we pay almost nothing
    uint32_t imageIndex;
    VkSemaphore renderSemaphore = VK_NULL_HANDLE;

    VkResult res = Swapchain::acquireNextImage(&imageIndex, &renderSemaphore);
    if (res != VK_SUCCESS && res != VK_SUBOPTIMAL_KHR) {
        if (renderSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(rtx().device, renderSemaphore, nullptr);
        }
        if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
            needsSwapchainRecreate_ = true;
        }
        return;
    }

    // At this point: swapchain wants a frame → we pay the cost
    LOG_INFO_CAT("RENDERER", "Frame active — time {:.3f}s, acquired image {}", total_sec, imageIndex);

    // Update UBO (cheap)
    updateCameraUBO(total_sec);

    // Only rebuild LAS if dirty **and** we have a valid frame to render
    VkAccelerationStructureKHR tlas = getTLAS();
    if (tlas == VK_NULL_HANDLE) {
        LOG_WARNING_CAT("RENDERER", "No valid TLAS — skipping trace");
        Swapchain::presentImage(rtx().present_queue, imageIndex, renderSemaphore);
        return;
    }

    // Command buffer from ring
    VkCommandBuffer cmd = cmdRing_[currentRingIndex_];
    currentRingIndex_ = (currentRingIndex_ + 1) % cmdRing_.size();

    vkResetCommandBuffer(cmd, VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);

    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmd, &begin);

    // Living world (cheap dispatch)
    pipeline_dispatch_living_world(cmd, total_sec);

    VkMemoryBarrier mb{};
    mb.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT;
    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 1, &mb, 0, nullptr, 0, nullptr);

    // Descriptor update only if needed
    if (needsDescriptorUpdate_) {
        updateGlobalDescriptorBuffer();
    }

    // HDR → trace → blit → present chain
    transitionImageLayout(cmd, hdrOutputImage_,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_GENERAL);

    pipeline_trace_rays(cmd, static_cast<uint32_t>(width_), static_cast<uint32_t>(height_));

    transitionImageLayout(cmd, hdrOutputImage_,
                          VK_IMAGE_LAYOUT_GENERAL,
                          VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    VkImage swapImg = Swapchain::swapchainImages_[imageIndex];

    transitionImageLayout(cmd, swapImg,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkImageBlit blit{};
    blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.srcOffsets[0]  = {0, 0, 0};
    blit.srcOffsets[1]  = {width_, height_, 1};
    blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.dstOffsets[0]  = {0, 0, 0};
    blit.dstOffsets[1]  = {width_, height_, 1};

    vkCmdBlitImage(cmd,
                   hdrOutputImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   swapImg,         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &blit, VK_FILTER_LINEAR);

    transitionImageLayout(cmd, swapImg,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    vkEndCommandBuffer(cmd);

    // Submit — wait on acquire semaphore
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

    VkSubmitInfo submit{};
    submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount   = 1;
    submit.pWaitSemaphores      = &renderSemaphore;
    submit.pWaitDstStageMask    = waitStages;
    submit.commandBufferCount   = 1;
    submit.pCommandBuffers      = &cmd;

    res = vkQueueSubmit(rtx().graphics_queue, 1, &submit, VK_NULL_HANDLE);
    if (res != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "vkQueueSubmit failed: {}", string_VkResult(res));
        vkDestroySemaphore(rtx().device, renderSemaphore, nullptr);
        return;
    }

    // Present consumes the semaphore
    Swapchain::presentImage(rtx().present_queue, imageIndex, renderSemaphore);
    // semaphore destroyed inside presentImage
}

    void onResize(int newWidth, int newHeight) noexcept {
        if (newWidth <= 0 || newHeight <= 0) {
            minimized_ = true;
            LOG_WARNING_CAT("RENDERER", "Window minimized — paused rendering");
            return;
        }

        if (newWidth == width_ && newHeight == height_) return;

        width_  = newWidth;
        height_ = newHeight;
        minimized_ = false;
        needsSwapchainRecreate_ = true;

        LOG_INFO_CAT("RENDERER", "Resize detected — new size {}x{}", width_, height_);
    }

private:
    void createOrRecreateHDRImage() noexcept {
        LOG_INFO_CAT("RENDERER", "Creating/recreating HDR output image — {}x{}", width_, height_);

        vkDestroyImageView(rtx().device, hdrOutputView_, nullptr);
        vkDestroyImage(rtx().device, hdrOutputImage_, nullptr);
        vkFreeMemory(rtx().device, hdrOutputMemory_, nullptr);

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

        VkResult res = vkCreateImage(rtx().device, &hdrInfo, nullptr, &hdrOutputImage_);
        if (res != VK_SUCCESS) {
            LOG_FATAL_CAT("RENDERER", "vkCreateImage(HDR) failed: {}", string_VkResult(res));
            return;
        }

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(rtx().device, hdrOutputImage_, &memReqs);

        uint32_t memType = Memory::findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memType == ~0u) {
            LOG_FATAL_CAT("RENDERER", "No device-local memory for HDR image");
            return;
        }

        VkMemoryAllocateInfo mai{};
        mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize  = memReqs.size;
        mai.memoryTypeIndex = memType;

        res = vkAllocateMemory(rtx().device, &mai, nullptr, &hdrOutputMemory_);
        if (res != VK_SUCCESS) {
            LOG_FATAL_CAT("RENDERER", "vkAllocateMemory(HDR) failed: {}", string_VkResult(res));
            return;
        }

        vkBindImageMemory(rtx().device, hdrOutputImage_, hdrOutputMemory_, 0);

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image            = hdrOutputImage_;
        viewInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format           = VK_FORMAT_R32G32B32A32_SFLOAT;
        viewInfo.components       = {VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                     VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        vkCreateImageView(rtx().device, &viewInfo, nullptr, &hdrOutputView_);

        needsDescriptorUpdate_ = true;

        LOG_SUCCESS_CAT("RENDERER", "HDR output image ready — view {:016x}", (uintptr_t)hdrOutputView_);
    }

    void updateCameraUBO(float totalTime) noexcept {
        CameraSceneData data{};

        data.view       = CAM.view();
        data.proj       = CAM.projection(static_cast<float>(width_) / static_cast<float>(height_));
        data.viewInverse = glm::inverse(data.view);
        data.projInverse = glm::inverse(data.proj);

        data.cameraPos     = glm::vec4(CAM.position(), 1.0f);
        data.prevCameraPos = data.cameraPos;  // TODO: track previous frame if needed for TAA/reproj

        data.exposure   = 1.0f;
        data.totalTime  = totalTime;
        data.randomSeed = static_cast<uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count() & 0xFFFFFFFFu);
        data.maxDepth   = 12;

        Memory::uploadToBuffer(cameraUBOHandle_, &data, sizeof(data));

        LOG_INFO_CAT("RENDERER", "Camera UBO updated — exposure {}, time {:.3f}s", data.exposure, data.totalTime);
    }

    void updateGlobalDescriptorBuffer() noexcept {
        VkAccelerationStructureKHR tlas = getTLAS();
        if (tlas == VK_NULL_HANDLE) {
            LOG_WARNING_CAT("RENDERER", "No valid TLAS for descriptor update — skipping");
            return;
        }

        RTDescriptorUpdate update{};
        update.tlas            = tlas;
        update.rtOutputView    = hdrOutputView_;
        update.ubo             = rtx().buffers[cameraUBOHandle_].buffer;
        update.uboSize         = sizeof(CameraSceneData);
        update.materialsBuffer = rtx().buffers[defaultMaterialsHandle_].buffer;
        update.materialsSize   = sizeof(Material) * 1;

        pipeline_write_rt_descriptors(update);
        needsDescriptorUpdate_ = false;

        LOG_SUCCESS_CAT("RENDERER", "Global descriptor buffer updated — TLAS {:016x}, HDR view {:016x}",
                        (uintptr_t)tlas, (uintptr_t)hdrOutputView_);
    }

    void transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout) noexcept {
        if (oldLayout == newLayout) return;

        VkImageMemoryBarrier barrier{};
        barrier.sType            = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout        = oldLayout;
        barrier.newLayout        = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image            = image;
        barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkPipelineStageFlags src = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        VkPipelineStageFlags dst = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            dst = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        } else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            src = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
            dst = VK_PIPELINE_STAGE_TRANSFER_BIT;
        } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            src = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dst = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        }

        vkCmdPipelineBarrier(cmd, src, dst, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        LOG_INFO_CAT("RENDERER", "Image layout transition — {} → {} for image {:016x}",
                     string_VkImageLayout(oldLayout), string_VkImageLayout(newLayout), (uintptr_t)image);
    }

    // Member variables
    SDL_Window*                     window_;
    int                             width_, height_;
    bool                            minimized_;
    bool                            destroyed_;
    std::chrono::steady_clock::time_point last_time_;
    uint32_t                        currentRingIndex_;
    uint64_t                        defaultMaterialsHandle_;
    uint64_t                        cameraUBOHandle_;
    VkImage                         hdrOutputImage_;
    VkImageView                     hdrOutputView_;
    VkDeviceMemory                  hdrOutputMemory_;
    std::vector<VkCommandBuffer>    cmdRing_;
    bool                            needsDescriptorUpdate_;
    bool                            needsSwapchainRecreate_;
};