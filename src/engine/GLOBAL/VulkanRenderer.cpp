// =============================================================================
// AMOURANTH RTX Engine - Vulkan Renderer
// Pure light ray tracing core — no frames, no state, pew forever
// Version 30.52 — January 23, 2026
// - Frame-free: single descriptor set, no MAX_FRAMES_IN_FLIGHT
// - Fixed ring of pre-allocated transient command buffers — reset before reuse
// - No per-pew allocation/free — self-disposing via vkResetCommandBuffer
// - Descriptor updates only on change (startup + TLAS rebuild)
// - Deferred swapchain recreate (flag at frame start — no mid-frame)
// - Present result checked to set recreate flag
// - Explicit PRESENT_SRC_KHR barrier before end/submit
// - Acquire semaphores cycled with ring size
// - HDR optimal tiling only
// - Empire stable — pink photons eternal
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
#include <cstdlib>

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
RTX::VulkanRenderer::VulkanRenderer(int width, int height, SDL_Window* window)
    : window_(window),
      width_(width),
      height_(height),
      minimized_(false),
      destroyed_(false),
      totalTime_(0.0),
      last_time_(std::chrono::steady_clock::now()),
      timelineSemaphore_(VK_NULL_HANDLE),
      currentTimelineValue_(0),
      acquireTimelineSemaphore_(VK_NULL_HANDLE),
      nextAcquireValue_(1),
      graphicsTimelineSemaphore_(VK_NULL_HANDLE),
      nextGraphicsValue_(1),
      currentFrame_(0),
      defaultMaterialsHandle_(0),
      cameraUBO_(0),
      cameraUBOBuffer_(VK_NULL_HANDLE),
      cameraUBOMemory_(VK_NULL_HANDLE),
      transientCmdPool_(VK_NULL_HANDLE),
      hdrOutputImage_(VK_NULL_HANDLE),
      hdrOutputView_(VK_NULL_HANDLE),
      hdrOutputMemory_(VK_NULL_HANDLE),
      cmdRing_(),
      currentRingIndex_(0),
      pipelineManager_(stone_device(), StoneKey::stone_physical()),
      needsDescriptorUpdate_(true),
      needsSwapchainRecreate_(false)
{
    LOG_INFO_CAT("RENDERER", "Initializing pure light engine — {}x{}", width, height);

    lazyCam(width, height);

    createTransientCommandPool();

    // Pre-allocate fixed ring of command buffers — self-disposing via reset
    cmdRing_.resize(CMD_RING_SIZE);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = transientCmdPool_;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(CMD_RING_SIZE);

    VkResult allocRes = vkAllocateCommandBuffers(stone_device(), &allocInfo, cmdRing_.data());
    if (allocRes != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to allocate command buffer ring: {}", string_VkResult(allocRes));
    } else {
        LOG_SUCCESS_CAT("RENDERER", "Allocated fixed ring of {} command buffers", CMD_RING_SIZE);
    }

    // Timeline semaphores for non-blocking tracking
    VkSemaphoreTypeCreateInfo timelineType{};
    timelineType.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineType.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineType.initialValue = 0;

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semInfo.pNext = &timelineType;

    vkCreateSemaphore(stone_device(), &semInfo, nullptr, &timelineSemaphore_);
    vkCreateSemaphore(stone_device(), &semInfo, nullptr, &acquireTimelineSemaphore_);
    vkCreateSemaphore(stone_device(), &semInfo, nullptr, &graphicsTimelineSemaphore_);

    // Per-frame acquire semaphores — binary, cycled
    VkSemaphoreCreateInfo acquireSemCI{};
    acquireSemCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (auto& s : acquireSemaphores_) {
        vkCreateSemaphore(stone_device(), &acquireSemCI, nullptr, &s);
    }

    // Camera UBO — manual
    VkBufferCreateInfo uboCI{};
    uboCI.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    uboCI.size        = sizeof(CameraSceneData);
    uboCI.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    uboCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    vkCreateBuffer(stone_device(), &uboCI, nullptr, &cameraUBOBuffer_);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(stone_device(), cameraUBOBuffer_, &memReqs);

    uint32_t memType = BufferManager::findMemoryType(memReqs.memoryTypeBits,
                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkMemoryAllocateInfo allocInfoMem{};
    allocInfoMem.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfoMem.allocationSize  = memReqs.size;
    allocInfoMem.memoryTypeIndex = memType;

    vkAllocateMemory(stone_device(), &allocInfoMem, nullptr, &cameraUBOMemory_);
    vkBindBufferMemory(stone_device(), cameraUBOBuffer_, cameraUBOMemory_, 0);

    cameraUBO_ = 0;  // manual buffer authoritative

    LOG_SUCCESS_CAT("RENDERER", "Camera UBO created manually");

    // Default materials
    std::array<Material, 1> defaultMats{};
    defaultMats[0].albedo = glm::vec4(1.0f);
    defaultMats[0].emissive = glm::vec4(0.0f);

    defaultMaterialsHandle_ = BufferManager::create(
        sizeof(defaultMats),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        "DefaultMaterials"
    );
    BufferManager::uploadToBuffer(defaultMaterialsHandle_, defaultMats.data(), sizeof(defaultMats));
    LOG_SUCCESS_CAT("RENDERER", "Default materials uploaded");

    // HDR storage image — optimal tiling only
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

    vkCreateImage(stone_device(), &hdrInfo, nullptr, &hdrOutputImage_);

    vkGetImageMemoryRequirements(stone_device(), hdrOutputImage_, &memReqs);

    allocInfoMem.allocationSize  = memReqs.size;
    allocInfoMem.memoryTypeIndex = BufferManager::findMemoryType(memReqs.memoryTypeBits,
                                                                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vkAllocateMemory(stone_device(), &allocInfoMem, nullptr, &hdrOutputMemory_);
    vkBindImageMemory(stone_device(), hdrOutputImage_, hdrOutputMemory_, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = hdrOutputImage_;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCreateImageView(stone_device(), &viewInfo, nullptr, &hdrOutputView_);
    LOG_SUCCESS_CAT("RENDERER", "HDR output image & view created — optimal tiling");

    // Ensure swapchain is ready before pipeline setup
    SwapchainManager::ensureReady(width_, height_);

    // Force initial TLAS build
    LAS::instance().getTLAS();
    LOG_INFO_CAT("RENDERER", "TLAS queried and ready");

    // Pipeline setup
    pipelineManager_.createPipelineLayout();
    pipelineManager_.allocateDescriptorSets();
    pipelineManager_.createRayTracingPipeline();

    // One-time SBT — pipeline manager handles submit/wait
    VkCommandBuffer oneTimeCmd = getOneTimeCommandBuffer();
    if (oneTimeCmd != VK_NULL_HANDLE) {
        pipelineManager_.createShaderBindingTable(transientCmdPool_, stone_graphics_queue(), oneTimeCmd);
        vkFreeCommandBuffers(stone_device(), transientCmdPool_, 1, &oneTimeCmd);
    }

    // One-time global descriptor update
    updateGlobalDescriptorSet();

    LOG_SUCCESS_CAT("RENDERER", "Pure light engine initialized");
}

RTX::VulkanRenderer::~VulkanRenderer() {
    destroyed_ = true;
    vkDeviceWaitIdle(stone_device());

    if (hdrOutputView_ != VK_NULL_HANDLE) vkDestroyImageView(stone_device(), hdrOutputView_, nullptr);
    if (hdrOutputImage_ != VK_NULL_HANDLE) vkDestroyImage(stone_device(), hdrOutputImage_, nullptr);
    if (hdrOutputMemory_ != VK_NULL_HANDLE) vkFreeMemory(stone_device(), hdrOutputMemory_, nullptr);

    if (cameraUBOBuffer_ != VK_NULL_HANDLE) vkDestroyBuffer(stone_device(), cameraUBOBuffer_, nullptr);
    if (cameraUBOMemory_ != VK_NULL_HANDLE) vkFreeMemory(stone_device(), cameraUBOMemory_, nullptr);

    BufferManager::destroy(defaultMaterialsHandle_);

    if (timelineSemaphore_ != VK_NULL_HANDLE) vkDestroySemaphore(stone_device(), timelineSemaphore_, nullptr);
    if (acquireTimelineSemaphore_ != VK_NULL_HANDLE) vkDestroySemaphore(stone_device(), acquireTimelineSemaphore_, nullptr);
    if (graphicsTimelineSemaphore_ != VK_NULL_HANDLE) vkDestroySemaphore(stone_device(), graphicsTimelineSemaphore_, nullptr);

    for (auto& s : acquireSemaphores_) {
        if (s != VK_NULL_HANDLE) vkDestroySemaphore(stone_device(), s, nullptr);
    }

    if (transientCmdPool_ != VK_NULL_HANDLE) vkDestroyCommandPool(stone_device(), transientCmdPool_, nullptr);

    LOG_INFO_CAT("RENDERER", "Renderer destroyed");
}

// =============================================================================
// createTransientCommandPool — with reset capability
// =============================================================================
void RTX::VulkanRenderer::createTransientCommandPool() noexcept {
    if (transientCmdPool_ != VK_NULL_HANDLE) return;

    VkCommandPoolCreateInfo info{};
    info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    info.queueFamilyIndex = StoneKey::stone_graphics_family();

    vkCreateCommandPool(stone_device(), &info, nullptr, &transientCmdPool_);
}

// =============================================================================
// getOneTimeCommandBuffer — only used for one-time startup commands
// =============================================================================
VkCommandBuffer RTX::VulkanRenderer::getOneTimeCommandBuffer() noexcept {
    VkCommandBuffer cmd = VK_NULL_HANDLE;

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = transientCmdPool_;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

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

    return cmd;
}

// =============================================================================
// transitionImageLayout
// =============================================================================
void RTX::VulkanRenderer::transitionImageLayout(VkCommandBuffer cmd,
                                                VkImage image,
                                                VkImageLayout oldLayout,
                                                VkImageLayout newLayout) noexcept {
    if (cmd == VK_NULL_HANDLE || image == VK_NULL_HANDLE || oldLayout == newLayout) return;

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
    } else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        srcAccess = 0;
        dstAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        srcAccess = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstAccess = VK_ACCESS_MEMORY_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else {
        return;
    }

    barrier.srcAccessMask = srcAccess;
    barrier.dstAccessMask = dstAccess;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// =============================================================================
// updateGlobalDescriptorSet — startup only
// =============================================================================
void RTX::VulkanRenderer::updateGlobalDescriptorSet() noexcept {
    LOG_INFO_CAT("RENDERER", "Updating global descriptor set (set 0)");

    VkAccelerationStructureKHR tlas = LAS::instance().getTLAS();
    if (tlas == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "TLAS is null");
        return;
    }

    if (hdrOutputView_ == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "HDR output view is null");
        return;
    }

    if (cameraUBOBuffer_ == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "Camera UBO buffer is null");
        return;
    }

    VkDescriptorSet globalSet = pipelineManager_.getDescriptorSet();
    if (globalSet == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "Global descriptor set is null");
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
    uboInfo.buffer = cameraUBOBuffer_;
    uboInfo.offset = 0;
    uboInfo.range = sizeof(CameraSceneData);

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = globalSet;
    writes[2].dstBinding = 2;
    writes[2].descriptorCount = 1;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[2].pBufferInfo = &uboInfo;

    vkUpdateDescriptorSets(stone_device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

    LOG_SUCCESS_CAT("RENDERER", "Global descriptor set updated");
}

// =============================================================================
// pew — main render loop (ring-based self-disposing cmd buffers)
// =============================================================================
void RTX::VulkanRenderer::pew() noexcept {
    if (minimized_ || destroyed_) return;

    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - last_time_).count();
    last_time_ = now;
    totalTime_ += dt;
    g_world.update(dt);

    // Deferred swapchain recreate from previous frame (safe — no recording yet)
    if (needsSwapchainRecreate_) {
        SwapchainManager::recreate(width_, height_, "previous frame invalid");
        needsSwapchainRecreate_ = false;
        SwapchainManager::ensureReady(width_, height_);
        if (!SwapchainManager::isReady()) {
            LOG_WARN_CAT("RENDERER", "Swapchain recreate failed — skipping pew");
            return;
        }
    }

    // Ensure swapchain is ready before acquire
    SwapchainManager::ensureReady(width_, height_);
    if (!SwapchainManager::isReady()) {
        LOG_WARN_CAT("RENDERER", "Swapchain not ready — skipping pew");
        return;
    }

    uint32_t imageIndex = UINT32_MAX;
    VkSemaphore currentAcquire = acquireSemaphores_[currentFrame_ % ACQUIRE_SEM_COUNT];
    currentFrame_++;

    VkResult acquireRes = SwapchainManager::acquireNextImage(&imageIndex, currentAcquire);
    if (acquireRes != VK_SUCCESS) {
        LOG_WARN_CAT("RENDERER", "Acquire failed: {}", string_VkResult(acquireRes));
        if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR || acquireRes == VK_SUBOPTIMAL_KHR || acquireRes == VK_ERROR_SURFACE_LOST_KHR) {
            needsSwapchainRecreate_ = true;
        }
        return;
    }

    // Cycle ring — self-disposing via reset
    VkCommandBuffer cmd = cmdRing_[currentRingIndex_];
    currentRingIndex_ = (currentRingIndex_ + 1) % CMD_RING_SIZE;

    VkResult resetRes = vkResetCommandBuffer(cmd, 0);
    if (resetRes != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "vkResetCommandBuffer failed in ring");
        return;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "vkBeginCommandBuffer failed in ring");
        return;
    }

    // Update descriptors only if needed
    if (needsDescriptorUpdate_) {
        RTDescriptorUpdate update{};
        update.tlas         = LAS::instance().getTLAS();
        update.rtOutputView = hdrOutputView_;
        update.ubo          = cameraUBOBuffer_;
        update.uboSize      = sizeof(CameraSceneData);
        update.materialsBuffer = BufferManager::get_buffer(defaultMaterialsHandle_);
        update.materialsSize   = BufferManager::get(defaultMaterialsHandle_)->size;

        pipelineManager_.updateRTDescriptorSet(update);
        needsDescriptorUpdate_ = false;
    }

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

    // Explicit present transition — critical
    VkImageMemoryBarrier presentBarrier{};
    presentBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    presentBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    presentBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    presentBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    presentBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    presentBarrier.image = swapImg;
    presentBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &presentBarrier);

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "vkEndCommandBuffer failed");
        return;
    }

    // Submit — no fence
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_TRANSFER_BIT };

    VkTimelineSemaphoreSubmitInfo timelineSI{};
    timelineSI.sType = VK_STRUCTURE_TYPE_TIMELINE_SEMAPHORE_SUBMIT_INFO;
    timelineSI.signalSemaphoreValueCount = 1;
    timelineSI.pSignalSemaphoreValues = &nextGraphicsValue_;

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = &timelineSI;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &currentAcquire;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &graphicsTimelineSemaphore_;

    vkQueueSubmit(stone_graphics_queue(), 1, &submitInfo, VK_NULL_HANDLE);

    nextGraphicsValue_++;  // advance for next submission

    // Present and check result
    VkResult presentRes = SwapchainManager::presentImage(stone_graphics_queue(), imageIndex, VK_NULL_HANDLE);
    if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR || presentRes == VK_ERROR_SURFACE_LOST_KHR) {
        needsSwapchainRecreate_ = true;
    }
}

// =============================================================================
// VulkanRenderer v30.52 — January 23, 2026
// =============================================================================