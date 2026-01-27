// =============================================================================
// AMOURANTH RTX Engine - Vulkan Renderer
// Pure light ray tracing core — no frames, no state, pew forever
// Version 30.75 — January 27, 2026 — No semaphores/fences + totalTime driven
// - All buffers/images created via BM_CREATE / BM_CREATE_DESCRIPTOR
// - Uploads via BM_UPLOAD_TO_BUFFER
// - Destruction via BM_DESTROY
// - Renderer owns present transition barrier (after blit)
// - No semaphores/fences — totalTime monolith drives all timing
// - No logging on VK_NOT_READY — silent status check
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
#include "engine/GLOBAL/OptionsMenu.hpp"

#include "engine/GLOBAL/StoneKey.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
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
// VulkanRenderer — Pure light ray tracing engine (descriptor-buffer edition)
// =============================================================================
RTX::VulkanRenderer::VulkanRenderer(int width, int height, SDL_Window* window)
    : window_(window),
      width_(width),
      height_(height),
      minimized_(false),
      destroyed_(false),
      totalTime_(0.0),
      last_time_(std::chrono::steady_clock::now()),
      currentFrame_(0),
      defaultMaterialsHandle_(0),
      cameraUBOHandle_(0),  // BufferManager handle
      cameraUBOBuffer_(VK_NULL_HANDLE),  // raw VkBuffer cache
      cameraUBOMemory_(VK_NULL_HANDLE),
      transientCmdPool_(VK_NULL_HANDLE),
      hdrOutputImage_(VK_NULL_HANDLE),
      hdrOutputView_(VK_NULL_HANDLE),
      hdrOutputMemory_(VK_NULL_HANDLE),
      cmdRing_(),
      currentRingIndex_(0),
      pipelineManager_(),
      needsDescriptorUpdate_(true),
      needsSwapchainRecreate_(false)
{
    LOG_INFO_CAT("RENDERER", "Initializing pure light engine — {}x{}", width, height);

    lazyCam(width, height);
    createTransientCommandPool();

    cmdRing_.resize(CMD_RING_SIZE);
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = transientCmdPool_;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(CMD_RING_SIZE);

    VkResult allocRes = vkAllocateCommandBuffers(stone_device(), &allocInfo, cmdRing_.data());
    if (allocRes != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to allocate cmd ring");
    }

    // Camera UBO via BufferManager macro
    BM_CREATE(cameraUBOHandle_, sizeof(CameraSceneData),
              VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
              VK_BUFFER_USAGE_TRANSFER_DST_BIT |
              VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
              "CameraUBO");

    cameraUBOBuffer_ = BM_GET_BUFFER(cameraUBOHandle_);  // cache raw buffer

    // Default materials via Descriptor buffer macro
    std::array<Material, 1> defaultMats{};
    defaultMats[0].albedo = glm::vec4(1.0f);
    defaultMats[0].emissive = glm::vec4(0.0f);

    defaultMaterialsHandle_ = BufferManager::createDescriptorBuffer(sizeof(defaultMats), "DefaultMaterials");
    void* mapped = BufferManager::lazyMapDescriptorBuffer(defaultMaterialsHandle_);
    if (mapped) {
        std::memcpy(mapped, defaultMats.data(), sizeof(defaultMats));
    }

    // HDR output image — still manual (not buffer), but could be wrapped later
    VkImageCreateInfo hdrInfo{};
    hdrInfo.sType     = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    hdrInfo.imageType = VK_IMAGE_TYPE_2D;
    hdrInfo.format    = VK_FORMAT_R32G32B32A32_SFLOAT;
    hdrInfo.extent    = {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1};
    hdrInfo.mipLevels = 1;
    hdrInfo.arrayLayers = 1;
    hdrInfo.samples   = VK_SAMPLE_COUNT_1_BIT;
    hdrInfo.tiling    = VK_IMAGE_TILING_OPTIMAL;
    hdrInfo.usage     = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    hdrInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    vkCreateImage(stone_device(), &hdrInfo, nullptr, &hdrOutputImage_);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(stone_device(), hdrOutputImage_, &memReqs);

    uint32_t memType = BufferManager::findMemoryType(memReqs.memoryTypeBits,
                                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateInfo mai{};
    mai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize  = memReqs.size;
    mai.memoryTypeIndex = memType;

    vkAllocateMemory(stone_device(), &mai, nullptr, &hdrOutputMemory_);
    vkBindImageMemory(stone_device(), hdrOutputImage_, hdrOutputMemory_, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image            = hdrOutputImage_;
    viewInfo.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format           = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCreateImageView(stone_device(), &viewInfo, nullptr, &hdrOutputView_);

    SwapchainManager::ensureReady(width_, height_);
    LAS::instance().getTLAS();

    pipelineManager_.createPipelineLayout();
    pipelineManager_.createRayTracingPipeline();
    pipelineManager_.createComputePipeline();

    VkCommandBuffer oneTimeCmd = getOneTimeCommandBuffer();
    if (oneTimeCmd) {
        pipelineManager_.createShaderBindingTable(transientCmdPool_, stone_graphics_queue(), oneTimeCmd);
        vkFreeCommandBuffers(stone_device(), transientCmdPool_, 1, &oneTimeCmd);
    }

    updateGlobalDescriptorBuffer();

    LOG_SUCCESS_CAT("RENDERER", "Pure light engine ready");
}

RTX::VulkanRenderer::~VulkanRenderer() {
    destroyed_ = true;
    vkDeviceWaitIdle(stone_device());

    vkDestroyImageView(stone_device(), hdrOutputView_, nullptr);
    vkDestroyImage(stone_device(), hdrOutputImage_, nullptr);
    vkFreeMemory(stone_device(), hdrOutputMemory_, nullptr);

    BM_DESTROY(cameraUBOHandle_);
    BM_DESTROY(defaultMaterialsHandle_);

    vkDestroyCommandPool(stone_device(), transientCmdPool_, nullptr);
}

void RTX::VulkanRenderer::createTransientCommandPool() noexcept {
    if (transientCmdPool_) return;

    VkCommandPoolCreateInfo info{};
    info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = StoneKey::stone_graphics_family();

    vkCreateCommandPool(stone_device(), &info, nullptr, &transientCmdPool_);
}

VkCommandBuffer RTX::VulkanRenderer::getOneTimeCommandBuffer() noexcept {
    VkCommandBuffer cmd = VK_NULL_HANDLE;

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = transientCmdPool_;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(stone_device(), &allocInfo, &cmd) != VK_SUCCESS) return VK_NULL_HANDLE;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        vkFreeCommandBuffers(stone_device(), transientCmdPool_, 1, &cmd);
        return VK_NULL_HANDLE;
    }

    return cmd;
}

void RTX::VulkanRenderer::transitionImageLayout(VkCommandBuffer cmd,
                                                VkImage image,
                                                VkImageLayout oldLayout,
                                                VkImageLayout newLayout) noexcept {
    if (!cmd || !image || oldLayout == newLayout) return;

    VkImageMemoryBarrier barrier{};
    barrier.sType                   = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout               = oldLayout;
    barrier.newLayout               = newLayout;
    barrier.srcQueueFamilyIndex     = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex     = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                   = image;
    barrier.subresourceRange        = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    VkAccessFlags srcAccess = 0;
    VkAccessFlags dstAccess = 0;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        dstAccess = VK_ACCESS_SHADER_WRITE_BIT;
        dstStage  = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    } else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL) {
        srcAccess = VK_ACCESS_SHADER_WRITE_BIT;
        dstAccess = VK_ACCESS_TRANSFER_READ_BIT;
        srcStage  = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        dstStage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        srcAccess = VK_ACCESS_TRANSFER_READ_BIT;
        dstAccess = VK_ACCESS_MEMORY_READ_BIT;
        srcStage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstStage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstAccess = VK_ACCESS_MEMORY_READ_BIT;
        srcStage  = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else {
        return;
    }

    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void RTX::VulkanRenderer::updateGlobalDescriptorBuffer() noexcept {
    VkAccelerationStructureKHR tlas = LAS::instance().getTLAS();
    if (!tlas) return;

    if (!hdrOutputView_) return;

    RTDescriptorUpdate update{};
    update.tlas             = tlas;
    update.rtOutputView     = hdrOutputView_;
    update.ubo              = BM_GET_BUFFER(cameraUBOHandle_);  // correct buffer from BM handle
    update.uboSize          = sizeof(CameraSceneData);
    update.materialsBuffer  = BM_GET_BUFFER(defaultMaterialsHandle_);
    update.materialsSize    = BM_GET(defaultMaterialsHandle_)->size;

    pipelineManager_.writeRTDescriptorsToBuffer(update);
    needsDescriptorUpdate_ = false;
}

void RTX::VulkanRenderer::pew() noexcept {
    if (minimized_ || destroyed_) return;

    // Use global totalTime monolith — no manual dt
    totalTime_ = RTX::TotalTime::get().seconds();

    if (needsSwapchainRecreate_) {
        SwapchainManager::recreate(width_, height_, "previous pew invalid");
        needsSwapchainRecreate_ = false;
        SwapchainManager::ensureReady(width_, height_);
        if (!SwapchainManager::isReady()) return;
    }

    SwapchainManager::ensureReady(width_, height_);
    if (!SwapchainManager::isReady()) return;

    uint32_t imageIndex;
    VkResult acquireRes = SwapchainManager::acquireNextImage(&imageIndex, VK_NULL_HANDLE);  // no semaphore
    if (acquireRes != VK_SUCCESS) {
        if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR || acquireRes == VK_SUBOPTIMAL_KHR || acquireRes == VK_ERROR_SURFACE_LOST_KHR) {
            needsSwapchainRecreate_ = true;
        }
        return;
    }

    VkCommandBuffer cmd = cmdRing_[currentRingIndex_];
    currentRingIndex_ = (currentRingIndex_ + 1) % CMD_RING_SIZE;

    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) return;

    pipelineManager_.dispatchLivingWorld(cmd, static_cast<float>(totalTime_));

    VkMemoryBarrier mb{};
    mb.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    mb.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    mb.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_UNIFORM_READ_BIT;

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         0, 1, &mb, 0, nullptr, 0, nullptr);

    if (needsDescriptorUpdate_) updateGlobalDescriptorBuffer();

    transitionImageLayout(cmd, hdrOutputImage_, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    pipelineManager_.traceRays(cmd, width_, height_);
    transitionImageLayout(cmd, hdrOutputImage_, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    VkImage swapImg = SwapchainManager::image(imageIndex);
    transitionImageLayout(cmd, swapImg, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkImageBlit blit{};
    blit.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {width_, height_, 1};
    blit.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = {width_, height_, 1};

    vkCmdBlitImage(cmd, hdrOutputImage_, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   swapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &blit, VK_FILTER_LINEAR);

    // NO PRESENT_SRC_KHR barrier here anymore — SwapchainManager owns the final transition

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) return;

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &cmd;

    vkQueueSubmit(stone_graphics_queue(), 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(stone_graphics_queue());  // block until done — simple for no-sync mode

    // Present with no wait semaphore
    SwapchainManager::presentImage(stone_graphics_queue(), imageIndex, VK_NULL_HANDLE);
}