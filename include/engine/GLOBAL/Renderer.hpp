// =============================================================================
// AMOURANTH RTX Engine — Pure Light Ray Tracing Core
// Fully header-only | Uses central rtx() + Swapchain helpers
// Version v0.81 — February 2026
// AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include "engine/GLOBAL/AMOURANTHRTX.hpp"
#include "engine/GLOBAL/ELLIE.hpp"
#include "engine/GLOBAL/camera.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/Pipeline.hpp"

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

        // Command buffer ring — triple buffering
        constexpr uint32_t CMD_RING_SIZE = 3;
        cmdRing_.resize(CMD_RING_SIZE);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = rtx().transient_pool;
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(cmdRing_.size());

        vkAllocateCommandBuffers(rtx().device, &allocInfo, cmdRing_.data());

        // Camera uniform buffer
        BM_CREATE(cameraUBOHandle_, sizeof(CameraSceneData),
                  VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  "CameraUBO");

        // Default materials (descriptor buffer)
        std::array<Material, 1> defaultMats{};
        BM_CREATE_DESCRIPTOR(defaultMaterialsHandle_, sizeof(defaultMats), "DefaultMaterials");
        void* mapped = BM_LAZY_MAP_DESCRIPTOR(defaultMaterialsHandle_);
        if (mapped) {
            std::memcpy(mapped, defaultMats.data(), sizeof(defaultMats));
        }

        // HDR output storage image
        createOrRecreateHDRImage();

        // Initialize pipeline (layouts, pipelines, SBT)
        pipeline_initialize();

        // Initial descriptor write
        updateGlobalDescriptorBuffer();

        LOG_SUCCESS_CAT("RENDERER", "Pure light engine ready");
    }

    ~VulkanRenderer() {
        destroyed_ = true;
        vkDeviceWaitIdle(rtx().device);

        vkDestroyImageView(rtx().device, hdrOutputView_, nullptr);
        vkDestroyImage(rtx().device, hdrOutputImage_, nullptr);
        vkFreeMemory(rtx().device, hdrOutputMemory_, nullptr);

        BM_DESTROY(cameraUBOHandle_);
        BM_DESTROY(defaultMaterialsHandle_);

        if (!cmdRing_.empty()) {
            vkFreeCommandBuffers(rtx().device, rtx().transient_pool,
                                 static_cast<uint32_t>(cmdRing_.size()), cmdRing_.data());
        }

        pipeline_shutdown();
    }

    void pew() noexcept {
        if (minimized_ || destroyed_) return;

        auto now = std::chrono::steady_clock::now();
        TotalTime::get().advance(now - last_time_);
        last_time_ = now;

        float total_sec = static_cast<float>(TotalTime::get().seconds());

        // Handle swapchain invalidation / resize
        if (needsSwapchainRecreate_) {
            vkDeviceWaitIdle(rtx().device);
            Swapchain::recreate(static_cast<uint32_t>(width_), static_cast<uint32_t>(height_));
            createOrRecreateHDRImage();  // HDR must match new size
            needsSwapchainRecreate_ = false;
            updateGlobalDescriptorBuffer();  // Re-bind new output view
            if (Swapchain::minimized_) return;
        }

        // Update camera UBO every frame
        updateCameraUBO(total_sec);

        VkAccelerationStructureKHR tlas = getTLAS();
        if (tlas == VK_NULL_HANDLE) return;

        uint32_t imageIndex;
        VkSemaphore acquireSemaphore = VK_NULL_HANDLE;

        VkResult res = Swapchain::acquireNextImage(&imageIndex, &acquireSemaphore);
        if (res != VK_SUCCESS) {
            if (res == VK_ERROR_OUT_OF_DATE_KHR || res == VK_SUBOPTIMAL_KHR) {
                needsSwapchainRecreate_ = true;
            }
            return;
        }

        VkCommandBuffer cmd = cmdRing_[currentRingIndex_];
        currentRingIndex_ = (currentRingIndex_ + 1) % cmdRing_.size();

        vkResetCommandBuffer(cmd, 0);

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(cmd, &begin);

        // Optional compute prepass (living world / animation)
        pipeline_dispatch_living_world(cmd, total_sec);

        VkMemoryBarrier mb{};
        mb.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT;
        vkCmdPipelineBarrier(cmd,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                             0, 1, &mb, 0, nullptr, 0, nullptr);

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

        VkSubmitInfo submit{};
        submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount   = 1;
        submit.pCommandBuffers      = &cmd;

        if (acquireSemaphore != VK_NULL_HANDLE) {
            VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
            submit.waitSemaphoreCount   = 1;
            submit.pWaitSemaphores      = &acquireSemaphore;
            submit.pWaitDstStageMask    = waitStages;
        }

        vkQueueSubmit(rtx().graphics_queue, 1, &submit, VK_NULL_HANDLE);

        Swapchain::presentImage(rtx().graphics_queue, imageIndex,
                                acquireSemaphore, rtx().swapchain);
    }

    // Called from SDL resize event
    void onResize(int newWidth, int newHeight) noexcept {
        if (newWidth <= 0 || newHeight <= 0) {
            minimized_ = true;
            return;
        }

        width_  = newWidth;
        height_ = newHeight;
        minimized_ = false;
        needsSwapchainRecreate_ = true;
    }

private:
    void createOrRecreateHDRImage() noexcept {
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

        vkCreateImage(rtx().device, &hdrInfo, nullptr, &hdrOutputImage_);

        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(rtx().device, hdrOutputImage_, &memReqs);

        uint32_t memType = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkMemoryAllocateInfo mai{};
        mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        mai.allocationSize  = memReqs.size;
        mai.memoryTypeIndex = memType;

        vkAllocateMemory(rtx().device, &mai, nullptr, &hdrOutputMemory_);
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

        BM_UPLOAD_TO_BUFFER(cameraUBOHandle_, &data, sizeof(data));
    }

    void updateGlobalDescriptorBuffer() noexcept {
        VkAccelerationStructureKHR tlas = getTLAS();
        if (tlas == VK_NULL_HANDLE) return;

        RTDescriptorUpdate update{};
        update.tlas            = tlas;
        update.rtOutputView    = hdrOutputView_;
        update.ubo             = BM_GET_BUFFER(cameraUBOHandle_);
        update.uboSize         = sizeof(CameraSceneData);
        update.materialsBuffer = BM_GET_BUFFER(defaultMaterialsHandle_);
        update.materialsSize   = sizeof(Material) * 1;

        pipeline_write_rt_descriptors(update);
        needsDescriptorUpdate_ = false;
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