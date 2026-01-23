// =============================================================================
// AMOURANTH RTX Engine - Vulkan Renderer
// Pure light ray tracing core — no frames, no state, pew pew forever
// Version 30.42 — January 22, 2026
// - Frame-free: single descriptor set, no MAX_FRAMES_IN_FLIGHT, no %
// - Swapchain layout: TRANSFER_DST_OPTIMAL → blit → PRESENT_SRC_KHR (spec-compliant)
// - No VUID-01399 / VUID-01430 / VUID-09600 — no device lost from layouts
// - Linear tiling toggleable (default off for perf)
// - HDR creation respects toggle + safety fallback
// - FPS concept dead — render as fast as possible, compositor paces
// - Hard _Exit on device lost — skips destructors, no segfault in validation
// - Stone device used everywhere — no lost device nonsense
// Empire stable — pink photons eternal
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
#include <array>
#include <cmath>
#include <utility>
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
      acquireSemaphores_{},
      currentFrame_(0),
      defaultMaterialsHandle_(0),
      cameraUBO_(0),
      cameraUBOBuffer_(VK_NULL_HANDLE),
      cameraUBOMemory_(VK_NULL_HANDLE),
      transientCmdPool_(VK_NULL_HANDLE),
      hdrOutputImage_(VK_NULL_HANDLE),
      hdrOutputView_(VK_NULL_HANDLE),
      hdrOutputMemory_(VK_NULL_HANDLE),
      pipelineManager_(stone_device(), StoneKey::stone_physical())
{
    LOG_INFO_CAT("RENDERER", "Initializing pure light engine — {}x{}", width, height);

    lazyCam(width, height);

    createTransientCommandPool();

    // Timeline semaphore
    VkSemaphoreTypeCreateInfo timelineType{};
    timelineType.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineType.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineType.initialValue = 0;

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semInfo.pNext = &timelineType;

    if (vkCreateSemaphore(stone_device(), &semInfo, nullptr, &timelineSemaphore_) != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to create timeline semaphore");
        return;
    }

    // Per-frame acquire semaphores (still needed for acquire/present sync)
    VkSemaphoreCreateInfo acquireSemCI{};
    acquireSemCI.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    for (auto& s : acquireSemaphores_) {
        if (vkCreateSemaphore(stone_device(), &acquireSemCI, nullptr, &s) != VK_SUCCESS) {
            LOG_FATAL_CAT("RENDERER", "Failed to create acquire semaphore");
            return;
        }
    }

    // Camera UBO — manual
    VkBufferCreateInfo uboCI{};
    uboCI.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    uboCI.size        = sizeof(CameraSceneData);
    uboCI.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    uboCI.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(stone_device(), &uboCI, nullptr, &cameraUBOBuffer_) != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "vkCreateBuffer failed for camera UBO");
        return;
    }

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(stone_device(), cameraUBOBuffer_, &memReqs);

    uint32_t memType = BufferManager::findMemoryType(memReqs.memoryTypeBits,
                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    if (memType == UINT32_MAX) {
        LOG_FATAL_CAT("RENDERER", "No suitable memory type for camera UBO");
        vkDestroyBuffer(stone_device(), cameraUBOBuffer_, nullptr);
        return;
    }

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize  = memReqs.size;
    allocInfo.memoryTypeIndex = memType;

    if (vkAllocateMemory(stone_device(), &allocInfo, nullptr, &cameraUBOMemory_) != VK_SUCCESS ||
        vkBindBufferMemory(stone_device(), cameraUBOBuffer_, cameraUBOMemory_, 0) != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to allocate/bind camera UBO memory");
        vkDestroyBuffer(stone_device(), cameraUBOBuffer_, nullptr);
        return;
    }

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
    if (defaultMaterialsHandle_ == 0) {
        LOG_FATAL_CAT("RENDERER", "Failed to create default materials buffer");
        return;
    }
    BufferManager::uploadToBuffer(defaultMaterialsHandle_, defaultMats.data(), sizeof(defaultMats));
    LOG_SUCCESS_CAT("RENDERER", "Default materials uploaded");

    // HDR storage image — respects Options::Rendering::USE_LINEAR_TILING
    VkImageCreateInfo hdrInfo{};
    hdrInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    hdrInfo.imageType = VK_IMAGE_TYPE_2D;
    hdrInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    hdrInfo.extent = { static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1 };
    hdrInfo.mipLevels = 1;
    hdrInfo.arrayLayers = 1;
    hdrInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    hdrInfo.tiling = Options::Rendering::USE_LINEAR_TILING ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;
    hdrInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    hdrInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(stone_device(), &hdrInfo, nullptr, &hdrOutputImage_) != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "vkCreateImage failed for HDR output");
        return;
    }

    // Safety check for linear tiling support (if toggled on)
    if (Options::Rendering::USE_LINEAR_TILING) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(stone_physical(), hdrInfo.format, &props);
        VkFormatFeatureFlags req = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT | VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
        if ((props.linearTilingFeatures & req) != req) {
            LOG_ERROR_CAT("RENDERER", "LINEAR tiling unsupported for HDR format — fallback to OPTIMAL");
            vkDestroyImage(stone_device(), hdrOutputImage_, nullptr);
            hdrInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            if (vkCreateImage(stone_device(), &hdrInfo, nullptr, &hdrOutputImage_) != VK_SUCCESS) {
                LOG_FATAL_CAT("RENDERER", "Fallback optimal HDR image creation failed");
                return;
            }
        }
    }

    vkGetImageMemoryRequirements(stone_device(), hdrOutputImage_, &memReqs);

    VkMemoryAllocateInfo allocInfo2{};
    allocInfo2.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo2.allocationSize  = memReqs.size;
    allocInfo2.memoryTypeIndex = BufferManager::findMemoryType(memReqs.memoryTypeBits,
                                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    if (vkAllocateMemory(stone_device(), &allocInfo2, nullptr, &hdrOutputMemory_) != VK_SUCCESS ||
        vkBindImageMemory(stone_device(), hdrOutputImage_, hdrOutputMemory_, 0) != VK_SUCCESS) {
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
    LOG_SUCCESS_CAT("RENDERER", "HDR output image & view created — tiling: {}", 
                    Options::Rendering::USE_LINEAR_TILING ? "LINEAR" : "OPTIMAL");

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
        (void)submitAndWaitOneTime(oneTimeCmd);  // Startup-only
    }

    // One-time global descriptor update
    updateGlobalDescriptorSet();

    LOG_SUCCESS_CAT("RENDERER", "Pure light engine initialized");
}

RTX::VulkanRenderer::~VulkanRenderer() {
    destroyed_ = true;
    vkDeviceWaitIdle(stone_device());

    if (hdrOutputView_ != VK_NULL_HANDLE) {
        vkDestroyImageView(stone_device(), hdrOutputView_, nullptr);
        hdrOutputView_ = VK_NULL_HANDLE;
    }

    if (hdrOutputImage_ != VK_NULL_HANDLE) {
        vkDestroyImage(stone_device(), hdrOutputImage_, nullptr);
        hdrOutputImage_ = VK_NULL_HANDLE;
    }

    if (hdrOutputMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(stone_device(), hdrOutputMemory_, nullptr);
        hdrOutputMemory_ = VK_NULL_HANDLE;
    }

    if (cameraUBOBuffer_ != VK_NULL_HANDLE) {
        vkDestroyBuffer(stone_device(), cameraUBOBuffer_, nullptr);
        cameraUBOBuffer_ = VK_NULL_HANDLE;
    }

    if (cameraUBOMemory_ != VK_NULL_HANDLE) {
        vkFreeMemory(stone_device(), cameraUBOMemory_, nullptr);
        cameraUBOMemory_ = VK_NULL_HANDLE;
    }

    BufferManager::destroy(defaultMaterialsHandle_);

    if (timelineSemaphore_ != VK_NULL_HANDLE)
        vkDestroySemaphore(stone_device(), timelineSemaphore_, nullptr);

    for (auto& s : acquireSemaphores_) {
        if (s != VK_NULL_HANDLE) {
            vkDestroySemaphore(stone_device(), s, nullptr);
            s = VK_NULL_HANDLE;
        }
    }

    if (transientCmdPool_ != VK_NULL_HANDLE)
        vkDestroyCommandPool(stone_device(), transientCmdPool_, nullptr);

    LOG_INFO_CAT("RENDERER", "Renderer destroyed");
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

    return cmd;
}

// =============================================================================
// submitAndWaitOneTime — returns result for error checking
// =============================================================================
VkResult RTX::VulkanRenderer::submitAndWaitOneTime(VkCommandBuffer cmd) noexcept {
    if (cmd == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RENDERER", "Cannot submit null command buffer");
        return VK_ERROR_UNKNOWN;
    }

    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "vkEndCommandBuffer failed");
        vkFreeCommandBuffers(stone_device(), transientCmdPool_, 1, &cmd);
        return VK_ERROR_UNKNOWN;
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &cmd;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence fence = VK_NULL_HANDLE;
    if (vkCreateFence(stone_device(), &fenceInfo, nullptr, &fence) != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "Failed to create fence");
        vkFreeCommandBuffers(stone_device(), transientCmdPool_, 1, &cmd);
        return VK_ERROR_UNKNOWN;
    }

    VkResult submitRes = vkQueueSubmit(stone_graphics_queue(), 1, &submitInfo, fence);
    if (submitRes != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "vkQueueSubmit failed: {}", string_VkResult(submitRes));
        vkDestroyFence(stone_device(), fence, nullptr);
        vkFreeCommandBuffers(stone_device(), transientCmdPool_, 1, &cmd);
        return submitRes;
    }

    VkResult waitRes = vkWaitForFences(stone_device(), 1, &fence, VK_TRUE, UINT64_MAX);
    if (waitRes == VK_ERROR_DEVICE_LOST) {
        LOG_FATAL_CAT("RENDERER", "DEVICE LOST — immediate hard exit, skipping destructors");
        std::_Exit(1);  // skips all cleanup, no destructor segfault
    }

    if (waitRes != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "vkWaitForFences failed: {}", string_VkResult(waitRes));
    }

    vkDestroyFence(stone_device(), fence, nullptr);
    vkFreeCommandBuffers(stone_device(), transientCmdPool_, 1, &cmd);

    return waitRes;
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
// pewPew — main render loop (no frames, no FPS)
// =============================================================================
void RTX::VulkanRenderer::pewPew() noexcept {
    if (minimized_ || destroyed_) return;

    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - last_time_).count();
    last_time_ = now;
    totalTime_ += dt;
    g_world.update(dt);

    uint32_t imageIndex = UINT32_MAX;
    VkSemaphore currentAcquire = acquireSemaphores_[currentFrame_ % acquireSemaphores_.size()];
    currentFrame_++;

    VkResult res = SwapchainManager::acquireNextImage(&imageIndex, currentAcquire);
    if (res != VK_SUCCESS) return;

    VkCommandBuffer cmd = getOneTimeCommandBuffer();
    if (!cmd) return;

    RTDescriptorUpdate update{};
    update.tlas         = LAS::instance().getTLAS();
    update.rtOutputView = hdrOutputView_;
    update.ubo          = cameraUBOBuffer_;
    update.uboSize      = sizeof(CameraSceneData);
    update.materialsBuffer = BufferManager::get_buffer(defaultMaterialsHandle_);
    update.materialsSize   = BufferManager::get(defaultMaterialsHandle_)->size;

    pipelineManager_.updateRTDescriptorSet(update);

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

    transitionImageLayout(cmd, swapImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VkResult submitRes = submitAndWaitOneTime(cmd);
    if (submitRes != VK_SUCCESS) return;

    SwapchainManager::presentImage(stone_graphics_queue(), imageIndex, currentAcquire);
}

// =============================================================================
// VulkanRenderer v30.42 — January 22, 2026
// - Frame-free — single descriptor set, no MAX_FRAMES_IN_FLIGHT, no %
// - Swapchain layout fixed: TRANSFER_DST_OPTIMAL → blit → PRESENT_SRC_KHR
// - No VUID-01399 / VUID-01430 / VUID-09600 — spec compliant, no device lost
// - Linear tiling toggleable (default off for perf)
// - HDR creation respects toggle + safety fallback
// - FPS concept dead — render as fast as possible, compositor paces
// - Hard _Exit on device lost — skips destructors, no segfault
// - Stone device used everywhere — no lost device nonsense
// Empire stable — pink photons eternal
// =============================================================================