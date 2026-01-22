// =============================================================================
// AMOURANTH RTX Engine - Vulkan Renderer
// Pure light ray tracing core — no frames, no state, pew pew forever
// Version 30.31 — January 21, 2026
// - Switched ALL logging to LOG_*_CAT("RENDERER", ...) macros
// - Consistent Empire-themed style matching logging.hpp
// - Robust error paths: early returns + resource cleanup on failure
// - Added category-specific trace/info/success/error/fatal logs
// =============================================================================

#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/camera.hpp"
#include "engine/GLOBAL/camera_utils.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"

#include "engine/GLOBAL/StoneKey.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <array>
#include <cmath>
#include <utility>
#include <chrono>

using StoneKey::stone_device;
using StoneKey::stone_graphics_queue;
using StoneKey::stone_swapchain;

// =============================================================================
// Material struct — file scope
// =============================================================================
struct Material {
    glm::vec4 albedo;
    glm::vec4 emissive;
};

// =============================================================================
// CameraSceneData — UBO sent to shaders
// =============================================================================
struct CameraSceneData {
    glm::mat4 viewInverse;
    glm::mat4 projInverse;
    glm::mat4 view;
    glm::mat4 proj;

    glm::vec4 cameraPos;
    glm::vec4 prevCameraPos;

    float exposure = 1.0f;
    float totalTime = 0.0f;
    uint32_t randomSeed = 12345u;

    uint32_t maxDepth = 12;

    uint32_t padding[3] = {0, 0, 0};
};

// =============================================================================
// Living World — Breathing Empire
// =============================================================================
struct LivingWorld {
    float timeOfDay = 12.0f;
    float cycleSpeed = 0.05f;
    float temperature = 20.0f;
    float humidity = 0.6f;
    float windSpeed = 5.0f;
    glm::vec3 windDirection = glm::normalize(glm::vec3(1.0f, 0.0f, 0.3f));
    double totalTime = 0.0;

    void update(double dt) noexcept {
        totalTime += dt;
        timeOfDay += static_cast<float>(dt) * cycleSpeed;
        if (timeOfDay >= 24.0f) timeOfDay -= 24.0f;

        float dayFactor = std::sin((timeOfDay / 24.0f) * glm::pi<float>() * 2.0f);
        temperature = 15.0f + dayFactor * 15.0f;
        humidity = 0.5f + (1.0f - std::abs(dayFactor)) * 0.5f;

        windSpeed = 5.0f + std::sin(totalTime * 0.1) * 4.0f + std::sin(totalTime * 0.03) * 2.0f;
        float windAngle = static_cast<float>(totalTime * 0.01);
        windDirection = glm::normalize(glm::vec3(std::cos(windAngle), 0.0f, std::sin(windAngle)));
    }

    [[nodiscard]] float sunHeight() const noexcept {
        return std::sin((timeOfDay / 24.0f - 0.25f) * glm::two_pi<float>());
    }

    [[nodiscard]] glm::vec3 sunDirection() const noexcept {
        float t = timeOfDay / 24.0f;
        float azimuth = t * glm::two_pi<float>();
        float elevation = std::asin(sunHeight());
        return glm::normalize(glm::vec3(std::cos(elevation) * std::sin(azimuth),
                                        sunHeight(),
                                        std::cos(elevation) * std::cos(azimuth)));
    }
};

static LivingWorld g_world;

// =============================================================================
// VulkanRenderer — Pure light ray tracing engine
// =============================================================================
RTX::VulkanRenderer::VulkanRenderer(int width, int height, SDL_Window* window, bool overclock)
    : window_(window),
      width_(width),
      height_(height),
      minimized_(false),
      destroyed_(false),
      totalTime_(0.0),
      last_time_(std::chrono::steady_clock::now()),
      timelineSemaphore_(VK_NULL_HANDLE),
      currentTimelineValue_(0),
      defaultMaterialsHandle_(0),
      cameraUBO_(0),
      transientCmdPool_(VK_NULL_HANDLE),
      hdrOutputImage_(VK_NULL_HANDLE),
      hdrOutputView_(VK_NULL_HANDLE)
{
    LOG_INFO_CAT("RENDERER", "Initializing pure light engine — {}x{}", width, height);

    lazyCam(width, height);

    createTransientCommandPool();

    VkSemaphoreTypeCreateInfo timelineType{};
    timelineType.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineType.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineType.initialValue = 0;

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semInfo.pNext = &timelineType;

    if (vkCreateSemaphore(stone_device(), &semInfo, nullptr, &timelineSemaphore_) != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to create timeline semaphore — empire cannot proceed");
        return;
    }

    // Camera UBO
    cameraUBO_ = BufferManager::create(
        sizeof(CameraSceneData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        "CameraUBO"
    );
    if (cameraUBO_ == 0) {
        LOG_FATAL_CAT("RENDERER", "Failed to create camera UBO — initialization aborted");
        return;
    }

    // Default materials (single white diffuse placeholder)
    std::array<Material, 1> defaultMats{};
    defaultMats[0].albedo = glm::vec4(1.0f);
    defaultMats[0].emissive = glm::vec4(0.0f);

    defaultMaterialsHandle_ = BufferManager::create(
        sizeof(defaultMats),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        "DefaultMaterials"
    );
    if (defaultMaterialsHandle_ == 0) {
        LOG_FATAL_CAT("RENDERER", "Failed to create default materials buffer");
        return;
    }
    BufferManager::uploadToBuffer(defaultMaterialsHandle_, defaultMats.data(), sizeof(defaultMats));
    LOG_SUCCESS_CAT("RENDERER", "Default materials uploaded");

    // HDR storage image
    VkImageCreateInfo hdrInfo{};
    hdrInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    hdrInfo.imageType = VK_IMAGE_TYPE_2D;
    hdrInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    hdrInfo.extent = { static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1 };
    hdrInfo.mipLevels = 1;
    hdrInfo.arrayLayers = 1;
    hdrInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    hdrInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    hdrInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    hdrInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(stone_device(), &hdrInfo, nullptr, &hdrOutputImage_) != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "vkCreateImage failed for HDR output");
        return;
    }

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(stone_device(), hdrOutputImage_, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = BufferManager::findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceMemory hdrMem = VK_NULL_HANDLE;
    if (vkAllocateMemory(stone_device(), &allocInfo, nullptr, &hdrMem) != VK_SUCCESS ||
        vkBindImageMemory(stone_device(), hdrOutputImage_, hdrMem, 0) != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to allocate/bind HDR image memory");
        vkDestroyImage(stone_device(), hdrOutputImage_, nullptr);
        return;
    }

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = hdrOutputImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    if (vkCreateImageView(stone_device(), &viewInfo, nullptr, &hdrOutputView_) != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to create HDR image view");
        return;
    }
    LOG_SUCCESS_CAT("RENDERER", "HDR output image & view created");

    // Force initial TLAS build
    LAS::instance().getTLAS();
    LOG_INFO_CAT("RENDERER", "TLAS queried and ready");

    // Pipeline setup
    pipelineManager_.createPipelineLayout();
    pipelineManager_.allocateDescriptorSets();
    pipelineManager_.createRayTracingPipeline();

    // One-time SBT
    VkCommandBuffer oneTimeCmd = getOneTimeCommandBuffer();
    if (oneTimeCmd != VK_NULL_HANDLE) {
        pipelineManager_.createShaderBindingTable(transientCmdPool_, stone_graphics_queue(), oneTimeCmd);
        submitAndWaitOneTime(oneTimeCmd);
    }

    // One-time descriptor update
    updateGlobalDescriptorSet();

    LOG_SUCCESS_CAT("RENDERER", "Pure light engine initialized — pink photons ready to scream eternal");
}

RTX::VulkanRenderer::~VulkanRenderer() {
    destroyed_ = true;
    vkDeviceWaitIdle(stone_device());

    if (timelineSemaphore_ != VK_NULL_HANDLE)
        vkDestroySemaphore(stone_device(), timelineSemaphore_, nullptr);

    if (transientCmdPool_ != VK_NULL_HANDLE)
        vkDestroyCommandPool(stone_device(), transientCmdPool_, nullptr);

    if (hdrOutputView_ != VK_NULL_HANDLE)
        vkDestroyImageView(stone_device(), hdrOutputView_, nullptr);

    if (hdrOutputImage_ != VK_NULL_HANDLE)
        vkDestroyImage(stone_device(), hdrOutputImage_, nullptr);

    BufferManager::destroy(defaultMaterialsHandle_);
    BufferManager::destroy(cameraUBO_);

    LOG_INFO_CAT("RENDERER", "Renderer destroyed — empire rests in silence");
}

// =============================================================================
// createTransientCommandPool
// =============================================================================
void RTX::VulkanRenderer::createTransientCommandPool() noexcept {
    if (transientCmdPool_ != VK_NULL_HANDLE) return;

    VkCommandPoolCreateInfo info{};
    info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    info.queueFamilyIndex = StoneKey::stone_graphics_family();

    if (vkCreateCommandPool(stone_device(), &info, nullptr, &transientCmdPool_) != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to create transient command pool");
    } else {
        LOG_SUCCESS_CAT("RENDERER", "Transient command pool created");
    }
}

// =============================================================================
// getOneTimeCommandBuffer
// =============================================================================
VkCommandBuffer RTX::VulkanRenderer::getOneTimeCommandBuffer() noexcept {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = transientCmdPool_;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(stone_device(), &allocInfo, &cmd) != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to allocate one-time command buffer");
        return VK_NULL_HANDLE;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to begin one-time command buffer");
        vkFreeCommandBuffers(stone_device(), transientCmdPool_, 1, &cmd);
        return VK_NULL_HANDLE;
    }

    LOG_TRACE_CAT("RENDERER", "One-time command buffer acquired and begun");
    return cmd;
}

// =============================================================================
// submitAndWaitOneTime
// =============================================================================
void RTX::VulkanRenderer::submitAndWaitOneTime(VkCommandBuffer cmd) noexcept {
    if (cmd == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RENDERER", "Cannot submit null command buffer");
        return;
    }

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "vkEndCommandBuffer failed in one-time submit");
        vkFreeCommandBuffers(stone_device(), transientCmdPool_, 1, &cmd);
        return;
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &cmd;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(stone_device(), &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to create fence for one-time submit");
        vkFreeCommandBuffers(stone_device(), transientCmdPool_, 1, &cmd);
        return;
    }

    VkResult submitRes = vkQueueSubmit(stone_graphics_queue(), 1, &submitInfo, fence);
    if (submitRes != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "vkQueueSubmit failed: {}", string_VkResult(submitRes));
        vkDestroyFence(stone_device(), fence, nullptr);
        vkFreeCommandBuffers(stone_device(), transientCmdPool_, 1, &cmd);
        return;
    }

    VkResult waitRes = vkWaitForFences(stone_device(), 1, &fence, VK_TRUE, UINT64_MAX);
    if (waitRes != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "vkWaitForFences failed: {}", string_VkResult(waitRes));
    }

    vkDestroyFence(stone_device(), fence, nullptr);
    vkFreeCommandBuffers(stone_device(), transientCmdPool_, 1, &cmd);

    LOG_TRACE_CAT("RENDERER", "One-time command buffer submitted and waited");
}

// =============================================================================
// transitionImageLayout
// =============================================================================
void RTX::VulkanRenderer::transitionImageLayout(VkCommandBuffer cmd,
                                                VkImage image,
                                                VkImageLayout oldLayout,
                                                VkImageLayout newLayout) noexcept {
    if (cmd == VK_NULL_HANDLE || image == VK_NULL_HANDLE) {
        LOG_WARNING_CAT("RENDERER", "Invalid cmd or image in transitionImageLayout");
        return;
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    VkAccessFlags srcAccess = 0;
    VkAccessFlags dstAccess = 0;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        srcAccess = 0;
        dstAccess = VK_ACCESS_SHADER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    } else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
        dstAccess = VK_ACCESS_TRANSFER_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        srcAccess = VK_ACCESS_TRANSFER_READ_BIT;
        dstAccess = VK_ACCESS_MEMORY_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else {
        LOG_WARNING_CAT("RENDERER", "Unsupported image layout transition ignored");
        return;
    }

    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// =============================================================================
// updateGlobalDescriptorSet
// =============================================================================
void RTX::VulkanRenderer::updateGlobalDescriptorSet() noexcept {
    LOG_INFO_CAT("RENDERER", "Updating global descriptor set (set 0)");

    VkAccelerationStructureKHR tlas = LAS::instance().getTLAS();
    if (tlas == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "TLAS is null — descriptor update aborted");
        return;
    }

    if (hdrOutputView_ == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "HDR output view is null");
        return;
    }

    VkBuffer uboBuffer = BufferManager::get_buffer(cameraUBO_);
    if (uboBuffer == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "Camera UBO buffer is null");
        return;
    }

    VkBuffer materialsBuffer = BufferManager::get_buffer(defaultMaterialsHandle_);
    if (materialsBuffer == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "Materials buffer is null");
        return;
    }

    VkDescriptorSet globalSet = pipelineManager_.getDescriptorSet(0);
    if (globalSet == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "Global descriptor set (frame 0) is null");
        return;
    }

    VkWriteDescriptorSetAccelerationStructureKHR asWrite{};
    asWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
    asWrite.accelerationStructureCount = 1;
    asWrite.pAccelerationStructures = &tlas;

    std::array<VkWriteDescriptorSet, 3> writes{};

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = globalSet;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    writes[0].pNext = &asWrite;

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageView = hdrOutputView_;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = globalSet;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &imageInfo;

    VkDescriptorBufferInfo uboInfo{};
    uboInfo.buffer = uboBuffer;
    uboInfo.offset = 0;
    uboInfo.range = sizeof(CameraSceneData);

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = globalSet;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[2].pBufferInfo = &uboInfo;

    vkUpdateDescriptorSets(stone_device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    LOG_SUCCESS_CAT("RENDERER", "Global descriptor set updated — TLAS, HDR, UBO bound");
}

// =============================================================================
// pewPew — main rendering loop
// =============================================================================
void RTX::VulkanRenderer::pewPew() noexcept {
    if (minimized_ || destroyed_) return;

    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - last_time_).count();
    last_time_ = now;

    totalTime_ += dt;
    g_world.update(dt);

    uint32_t imageIndex = UINT32_MAX;
    VkResult acquireRes = vkAcquireNextImageKHR(stone_device(), stone_swapchain(), UINT64_MAX,
                                                 VK_NULL_HANDLE, VK_NULL_HANDLE, &imageIndex);

    if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR || acquireRes == VK_SUBOPTIMAL_KHR || acquireRes == VK_ERROR_SURFACE_LOST_KHR) {
        LOG_WARNING_CAT("RENDERER", "Swapchain out of date/suboptimal — recreating");
        RTX::SwapchainManager::recreate(width_, height_);
        return;
    }

    if (acquireRes != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "vkAcquireNextImageKHR failed: {}", string_VkResult(acquireRes));
        return;
    }

    LOG_TRACE_CAT("RENDERER", "Acquired swapchain image {}", imageIndex);

    VkCommandBuffer cmd = getOneTimeCommandBuffer();
    if (cmd == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RENDERER", "Failed to get command buffer for frame");
        return;
    }

    transitionImageLayout(cmd, hdrOutputImage_,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    pipelineManager_.traceRays(cmd, imageIndex, width_, height_);

    VkImageCopy copyRegion{};
    copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copyRegion.srcOffset = {0, 0, 0};
    copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copyRegion.dstOffset = {0, 0, 0};
    copyRegion.extent = {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1};

    transitionImageLayout(cmd, hdrOutputImage_,
                          VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    transitionImageLayout(cmd, RTX::SwapchainManager::image(imageIndex),
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkCmdCopyImage(cmd,
                   hdrOutputImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   RTX::SwapchainManager::image(imageIndex), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &copyRegion);

    transitionImageLayout(cmd, RTX::SwapchainManager::image(imageIndex),
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    submitAndWaitOneTime(cmd);

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    VkSwapchainKHR currentSwapchain = stone_swapchain();
    presentInfo.pSwapchains = &currentSwapchain;
    presentInfo.pImageIndices = &imageIndex;

    VkResult presentRes = vkQueuePresentKHR(stone_graphics_queue(), &presentInfo);
    if (presentRes != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "vkQueuePresentKHR failed: {}", string_VkResult(presentRes));
    } else {
        LOG_TRACE_CAT("RENDERER", "Frame presented successfully");
    }
}

// =============================================================================
// VulkanRenderer v30.31 — January 21, 2026
// - All logging now uses LOG_*_CAT("RENDERER", ...) — Empire style unified
// - Robust failure handling with early returns
// - Pink photons eternal — ready when traversal is fixed
// =============================================================================