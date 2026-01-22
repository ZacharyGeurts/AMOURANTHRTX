// =============================================================================
// AMOURANTH RTX Engine - Vulkan Renderer
// Pure light ray tracing core — no frames, no state, pew pew forever
// Version 30.28 — January 20, 2026
// Naked: direct LAS → swapchain beams, single set, transient cmd per present
// No warm-up, no accumulation, no per-frame anything — crash loud, render turf
// C++23 — production ready, empire unbroken
// Renderer owns lifetime clock — double totalTime_ + steady_clock dt
// No SDL3 timer, no g_deltaTime — pure chrono steady_clock
// Member order fixed to kill -Werror=reorder
// FIXED: All missing functions defined, linker errors gone
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
    double totalTime = 0.0;  // sync with renderer clock

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
      transientCmdPool_(VK_NULL_HANDLE)
{
    LOG_INFO("RENDERER", "Initializing pure light engine — {}x{}", width, height);

    lazyCam(width, height);

    createTransientCommandPool();

    VkSemaphoreTypeCreateInfo timelineType{};
    timelineType.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    timelineType.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
    timelineType.initialValue = 0;

    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semInfo.pNext = &timelineType;
    VK_CHECK(vkCreateSemaphore(stone_device(), &semInfo, nullptr, &timelineSemaphore_));

    // Automagic UBO with TRANSFER_DST
    cameraUBO_ = BufferManager::create(
        sizeof(CameraSceneData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        "CameraUBO"
    );
    if (cameraUBO_ == 0) {
        LOG_FATAL("RENDERER", "Failed to create camera UBO");
        return;
    }

    // Force initial TLAS build
    LAS::instance().getTLAS();  // Synchronous — TLAS ready

    // Pipeline setup — once, forever
    pipelineManager_.createPipelineLayout();
    pipelineManager_.allocateDescriptorSets();
    pipelineManager_.createRayTracingPipeline();

    // One-time SBT creation
    VkCommandBuffer oneTimeCmd = getOneTimeCommandBuffer();
    pipelineManager_.createShaderBindingTable(transientCmdPool_, stone_graphics_queue(), oneTimeCmd);
    submitAndWaitOneTime(oneTimeCmd);

    // One-time global descriptor set update — initial TLAS + view + UBO
    updateGlobalDescriptorSet();

    LOG_INFO("RENDERER", "Pure light engine initialized — ready to pew pew");
}

RTX::VulkanRenderer::~VulkanRenderer() {
    destroyed_ = true;
    vkDeviceWaitIdle(stone_device());

    if (timelineSemaphore_ != VK_NULL_HANDLE)
        vkDestroySemaphore(stone_device(), timelineSemaphore_, nullptr);

    if (transientCmdPool_ != VK_NULL_HANDLE) {
        vkDestroyCommandPool(stone_device(), transientCmdPool_, nullptr);
    }

    BufferManager::destroy(defaultMaterialsHandle_);
    BufferManager::destroy(cameraUBO_);

    LOG_INFO("RENDERER", "Cleanup complete");
}

// =============================================================================
// createTransientCommandPool — transient pool for one-shot cmds
// =============================================================================
void RTX::VulkanRenderer::createTransientCommandPool() noexcept {
    if (transientCmdPool_ != VK_NULL_HANDLE) return;

    VkCommandPoolCreateInfo info{};
    info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    info.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    info.queueFamilyIndex = StoneKey::stone_graphics_family();

    VK_CHECK(vkCreateCommandPool(stone_device(), &info, nullptr, &transientCmdPool_));
}

// =============================================================================
// getOneTimeCommandBuffer — transient one-shot cmd buffer
// =============================================================================
VkCommandBuffer RTX::VulkanRenderer::getOneTimeCommandBuffer() noexcept {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = transientCmdPool_;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(stone_device(), &allocInfo, &cmd));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    return cmd;
}

// =============================================================================
// submitAndWaitOneTime — submit and wait for transient cmd
// =============================================================================
void RTX::VulkanRenderer::submitAndWaitOneTime(VkCommandBuffer cmd) noexcept {
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &cmd;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

    VkFence fence = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFence(stone_device(), &fenceInfo, nullptr, &fence));

    VK_CHECK(vkQueueSubmit(stone_graphics_queue(), 1, &submitInfo, fence));
    VK_CHECK(vkWaitForFences(stone_device(), 1, &fence, VK_TRUE, UINT64_MAX));

    vkDestroyFence(stone_device(), fence, nullptr);
    vkFreeCommandBuffers(stone_device(), transientCmdPool_, 1, &cmd);
}

// =============================================================================
// updateGlobalDescriptorSet — one-time global set update
// =============================================================================
void RTX::VulkanRenderer::updateGlobalDescriptorSet() noexcept {
    LOG_INFO("RENDERER", "Updating global descriptor set");

    if (RTX::SwapchainManager::views().empty()) {
        LOG_FATAL("RENDERER", "No swapchain views — cannot update set");
        return;
    }

    RTDescriptorUpdate update{};
    update.tlas = LAS::instance().getTLAS();
    update.rtOutputView = RTX::SwapchainManager::view(0);
    update.ubo = BufferManager::get_buffer(cameraUBO_);
    update.uboSize = sizeof(CameraSceneData);
    update.materialsBuffer = BufferManager::get_buffer(defaultMaterialsHandle_);
    update.materialsSize = sizeof(std::array<Material, 1>);

    pipelineManager_.updateRTDescriptorSet(0, update);

    VkDescriptorSet set = pipelineManager_.getDescriptorSet(0);
    if (set == VK_NULL_HANDLE) {
        LOG_FATAL("RENDERER", "Global descriptor set null after update");
        return;
    }

    LOG_SUCCESS("RENDERER", "Global descriptor set updated");
}

// =============================================================================
// Pew Pew — acquire image, trace rays, present (no frames)
// =============================================================================
void RTX::VulkanRenderer::pewPew() noexcept {
    if (minimized_ || destroyed_) return;

    auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - last_time_).count();
    last_time_ = now;

    totalTime_ += dt;
    g_world.update(dt);

    // Acquire next image
    uint32_t imageIndex = UINT32_MAX;
    VkResult result = vkAcquireNextImageKHR(stone_device(), stone_swapchain(), UINT64_MAX,
                                            VK_NULL_HANDLE, VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_SURFACE_LOST_KHR) {
        RTX::SwapchainManager::recreate(width_, height_);
        return;
    }

    if (result != VK_SUCCESS) {
        LOG_ERROR("RENDERER", "Acquire failed: {}", string_VkResult(result));
        return;
    }

    // Transient cmd buffer per pew pew
    VkCommandBuffer cmd = getOneTimeCommandBuffer();

    transitionImageLayout(cmd, RTX::SwapchainManager::image(imageIndex),
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    pipelineManager_.traceRays(cmd, imageIndex, width_, height_);

    transitionImageLayout(cmd, RTX::SwapchainManager::image(imageIndex),
                          VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    submitAndWaitOneTime(cmd);

    // Present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.swapchainCount = 1;
    VkSwapchainKHR currentSwapchain = stone_swapchain();
    presentInfo.pSwapchains = &currentSwapchain;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(stone_graphics_queue(), &presentInfo);
}

// =============================================================================
// Image layout transitions
// =============================================================================
void RTX::VulkanRenderer::transitionImageLayout(VkCommandBuffer cmd,
                                                VkImage image,
                                                VkImageLayout oldLayout,
                                                VkImageLayout newLayout) noexcept {
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkPipelineStageFlags srcStage, dstStage;

    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_GENERAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        dstStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
    } else if (oldLayout == VK_IMAGE_LAYOUT_GENERAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        srcStage = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;
        dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    } else {
        return;
    }

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// =============================================================================
// VulkanRenderer v30.28 — January 20, 2026
// - No warm-up race — set updated once at startup
// - No frame state — pew pew forever
// - Direct LAS to swapchain, beams every present
// - totalTime_ owned by renderer — double-precision lifetime clock
// - No SDL3 timer, no g_deltaTime — pure chrono steady_clock
// - Member order fixed to kill -Werror=reorder
// - Zero cost — crash loud if bad
// - Production ready
// =============================================================================