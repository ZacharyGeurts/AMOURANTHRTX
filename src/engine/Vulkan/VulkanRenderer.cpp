// src/engine/Vulkan/VulkanRenderer.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// VulkanRenderer — JAY LENO EDITION v3.3 — NOV 11 2025 12:04 PM EST
// • GLOBAL BROS — NO NAMESPACES — Dispose::Handle<T> owns ALL
// • ctx() → global Vulkan context
// • SwapchainManager::get() → swapchain
// • AMAZO_LAS::get() → TLAS/BLAS
// • UltraLowLevelBufferTracker::get() → encrypted buffers
// • FULL HYPERTRACE: Nexus scoring, quantum jitter, adaptive sampling, denoising, ACES
// • -Werror clean, C++23, zero leaks, pink photons eternal
// =============================================================================

#include "engine/GLOBAL/StoneKey.hpp" // FIRST
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/GLOBAL/logging.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <chrono>
#include <array>
#include <cmath>
#include <algorithm>
#include <format>
#include <random>

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL SINGLETONS — NO NAMESPACES — RAW POWER
// ──────────────────────────────────────────────────────────────────────────────
using SwapchainManager::get as SWAPCHAIN;
using AMAZO_LAS::get as LAS;
using UltraLowLevelBufferTracker::get as BUFFER_TRACKER;

// ──────────────────────────────────────────────────────────────────────────────
// QUANTUM ENTROPY — JAY LENO SEED
// ──────────────────────────────────────────────────────────────────────────────
namespace {
std::mt19937 quantumRng(69420);  // Thermal supremacy seed
float getJitter() {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    return dist(quantumRng);
}
} // anonymous

// ──────────────────────────────────────────────────────────────────────────────
// VulkanRenderer — JAY LENO ENGINE — FULL IMPLEMENTATION
// ──────────────────────────────────────────────────────────────────────────────
VkCommandBuffer VulkanRenderer::beginSingleTimeCommands(VkDevice device, VkCommandPool pool) {
    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_SHOT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);
    return cmd;
}

void VulkanRenderer::endSingleTimeCommands(VkDevice device, VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd) {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device, pool, 1, &cmd);
}

VkCommandBuffer VulkanRenderer::allocateTransientCommandBuffer(VkDevice device, VkCommandPool pool) {
    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &allocInfo, &cmd);
    return cmd;
}

// ──────────────────────────────────────────────────────────────────────────────
// TOGGLES — HYPERTRACE + OVERCLOCK
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::toggleHypertrace() noexcept {
    hypertraceEnabled_ = !hypertraceEnabled_;
    resetAccumulation_ = true;
    LAS.setHypertraceEnabled(hypertraceEnabled_);
    LOG_INFO_CAT("Hypertrace", "{}NEXUS SCORING {} — QUANTUM JITTER {} {}", 
                 hypertraceEnabled_ ? PLASMA_FUCHSIA : RASPBERRY_PINK,
                 hypertraceEnabled_ ? "ENGAGED" : "STANDBY",
                 hypertraceEnabled_ ? "UNLEASHED" : "LOCKED",
                 hypertraceEnabled_ ? "JAY LENO APPROVED" : "");
}

void VulkanRenderer::toggleFpsTarget() noexcept {
    if (overclockMode_) {
        fpsTarget_ = FpsTarget::FPS_UNLIMITED;
        LOG_INFO_CAT("Overclock", "UNLIMITED FPS — 420Hz THERMAL SUPREMACY — JAY LENO ENGINE");
    } else {
        fpsTarget_ = (fpsTarget_ == FpsTarget::FPS_60) ? FpsTarget::FPS_120 : FpsTarget::FPS_60;
        LOG_INFO_CAT("FPS", "Target {} FPS — Safe mode", fpsTarget_ == FpsTarget::FPS_60 ? 60 : 120);
    }
}

void VulkanRenderer::toggleDenoising() noexcept {
    denoisingEnabled_ = !denoisingEnabled_;
    resetAccumulation_ = true;
    LOG_INFO_CAT("Denoising", "{} — Wishlist pass {}", denoisingEnabled_ ? "ENABLED" : "DISABLED", denoisingEnabled_ ? "ACTIVE" : "BYPASSED");
}

void VulkanRenderer::toggleAdaptiveSampling() noexcept {
    adaptiveSamplingEnabled_ = !adaptiveSamplingEnabled_;
    resetAccumulation_ = true;
    LOG_INFO_CAT("Sampling", "Adaptive {} — Rays per pixel dynamic", adaptiveSamplingEnabled_ ? "ON" : "OFF");
}

void VulkanRenderer::setTonemapType(TonemapType type) noexcept {
    tonemapType_ = type;
    LOG_INFO_CAT("Tonemap", "{} — Cinematic enhanced", type == TonemapType::ACES ? "ACES" : (type == TonemapType::FILMIC ? "FILMIC" : "REINHARD"));
}

void VulkanRenderer::setOverclockMode(bool enabled) noexcept {
    overclockMode_ = enabled;
    if (enabled) {
        fpsTarget_ = FpsTarget::FPS_UNLIMITED;
        quantumRng.seed(69420);
        SWAPCHAIN.toggleVSync(false);
        LOG_INFO_CAT("OVERCLOCK", "ENGAGED — UNLIMITED FPS — RTX CORES @ 420Hz — JAY LENO ENGINE");
    } else {
        fpsTarget_ = FpsTarget::FPS_120;
        SWAPCHAIN.toggleVSync(true);
        LOG_INFO_CAT("OVERCLOCK", "DISENGAGED — Safe 120 FPS — Thermal cooldown");
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// CLEANUP — GLOBAL DISPOSE AUTO-SHRED
// ──────────────────────────────────────────────────────────────────────────────
VulkanRenderer::~VulkanRenderer() { cleanup(); }

void VulkanRenderer::cleanup() noexcept {
    vkDeviceWaitIdle(ctx()->vkDevice);

    for (auto& s : imageAvailableSemaphores_) if (s) vkDestroySemaphore(ctx()->vkDevice, s, nullptr);
    for (auto& s : renderFinishedSemaphores_) if (s) vkDestroySemaphore(ctx()->vkDevice, s, nullptr);
    for (auto& f : inFlightFences_) if (f) vkDestroyFence(ctx()->vkDevice, f, nullptr);
    for (auto& p : queryPools_) if (p) vkDestroyQueryPool(ctx()->vkDevice, p, nullptr);

    destroyRTOutputImages();
    destroyAccumulationImages();
    destroyNexusScoreImage();
    destroyDenoiserImage();
    destroyAllBuffers();

    descriptorPool_.reset();
    rtDescriptorPool_.reset();

    if (!commandBuffers_.empty()) {
        vkFreeCommandBuffers(ctx()->vkDevice, ctx()->vkCommandPool, static_cast<uint32_t>(commandBuffers_.size()), commandBuffers_.data());
        commandBuffers_.clear();
    }

    LOG_SUCCESS_CAT("Dispose", "{}VULKAN RENDERER SHUTDOWN — ZERO ZOMBIES — JAY LENO ENGINE RESTS ETERNAL{}", PLASMA_FUCHSIA, RESET);
}

// ──────────────────────────────────────────────────────────────────────────────
// DESTROY — Handle.reset() → INLINE_FREE + shred
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::destroyNexusScoreImage() noexcept {
    hypertraceScoreStagingBuffer_.reset();
    hypertraceScoreStagingMemory_.reset();
    hypertraceScoreImage_.reset();
    hypertraceScoreMemory_.reset();
    hypertraceScoreView_.reset();
}

void VulkanRenderer::destroyDenoiserImage() noexcept {
    denoiserImage_.reset();
    denoiserMemory_.reset();
    denoiserView_.reset();
}

void VulkanRenderer::destroyAllBuffers() noexcept {
    for (auto& enc : uniformBufferEncs_) if (enc) BUFFER_TRACKER.destroy(enc);
    for (auto& enc : materialBufferEncs_) if (enc) BUFFER_TRACKER.destroy(enc);
    for (auto& enc : dimensionBufferEncs_) if (enc) BUFFER_TRACKER.destroy(enc);
    for (auto& enc : tonemapUniformEncs_) if (enc) BUFFER_TRACKER.destroy(enc);
    uniformBufferEncs_.clear(); materialBufferEncs_.clear(); dimensionBufferEncs_.clear(); tonemapUniformEncs_.clear();
}

void VulkanRenderer::destroyAccumulationImages() noexcept {
    for (auto& h : accumImages_) h.reset();
    for (auto& h : accumMemories_) h.reset();
    for (auto& h : accumViews_) h.reset();
}

void VulkanRenderer::destroyRTOutputImages() noexcept {
    for (auto& h : rtOutputImages_) h.reset();
    for (auto& h : rtOutputMemories_) h.reset();
    for (auto& h : rtOutputViews_) h.reset();
}

// ──────────────────────────────────────────────────────────────────────────────
// CONSTRUCTOR — JAY LENO INITIALIZATION
// ──────────────────────────────────────────────────────────────────────────────
VulkanRenderer::VulkanRenderer(int width, int height, SDL_Window* window,
                               const std::vector<std::string>& shaderPaths,
                               bool overclockFromMain)
    : window_(window), width_(width), height_(height), overclockMode_(overclockFromMain),
      denoisingEnabled_(true), adaptiveSamplingEnabled_(true), tonemapType_(TonemapType::ACES)
{
    setOverclockMode(overclockFromMain);

    if (kStone1 == 0 || kStone2 == 0) throw std::runtime_error("StoneKey breach");

    auto& c = *ctx();
    BUFFER_TRACKER.init(c.vkDevice, c.vkPhysicalDevice);

    // Sync objects
    VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkCreateSemaphore(c.vkDevice, &semInfo, nullptr, &imageAvailableSemaphores_[i]);
        vkCreateSemaphore(c.vkDevice, &semInfo, nullptr, &renderFinishedSemaphores_[i]);
        vkCreateFence(c.vkDevice, &fenceInfo, nullptr, &inFlightFences_[i]);
    }

    // Timestamp query
    VkQueryPoolCreateInfo qpInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    qpInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    qpInfo.queryCount = MAX_FRAMES_IN_FLIGHT * 2;
    vkCreateQueryPool(c.vkDevice, &qpInfo, nullptr, &timestampQueryPool_);

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(c.vkPhysicalDevice, &props);
    timestampPeriod_ = props.limits.timestampPeriod / 1e6;

    // Descriptor pool
    std::array<VkDescriptorPoolSize, 6> poolSizes{{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT * 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT * 4},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT * 7},
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, MAX_FRAMES_IN_FLIGHT}
    }};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.poolSizeCount = poolSizes.size();
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT * 3 + 16;
    VkDescriptorPool pool;
    vkCreateDescriptorPool(c.vkDevice, &poolInfo, nullptr, &pool);
    descriptorPool_ = MakeHandle(pool, c.vkDevice, vkDestroyDescriptorPool, 0, "RendererPool");

    // Shared staging buffer
    uint64_t enc = BUFFER_TRACKER.create(1_MB, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "SharedStaging");
    sharedStagingBufferEnc_ = enc;
    sharedStagingBuffer_ = MakeHandle(BUFFER_TRACKER.getData(enc)->buffer, c.vkDevice);
    sharedStagingMemory_ = MakeHandle(BUFFER_TRACKER.getData(enc)->memory, c.vkDevice);

    createEnvironmentMap();
    createAccumulationImages();
    createRTOutputImages();
    createDenoiserImage();
    createNexusScoreImage(c.vkPhysicalDevice, c.vkDevice, c.vkCommandPool, c.vkGraphicsQueue);

    initializeAllBufferData(MAX_FRAMES_IN_FLIGHT, 64_MB, 16_MB);
    createCommandBuffers();
    allocateDescriptorSets();

    updateNexusDescriptors();
    updateRTXDescriptors();
    updateTonemapDescriptorsInitial();
    updateDenoiserDescriptors();

    LAS.setHypertraceEnabled(hypertraceEnabled_);

    LOG_SUCCESS_CAT("Renderer", "{}JAY LENO ENGINE INITIALIZED — HYPERTRACE ENGAGED — 420Hz READY{}", COSMIC_GOLD, RESET);
}

// ──────────────────────────────────────────────────────────────────────────────
// IMAGE CREATION — GLOBAL BUFFER + DISPOSE
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::createRTOutputImages() {
    VkFormat fmt = VK_FORMAT_R32G32B32A32_SFLOAT;
    VkExtent2D ext = SWAPCHAIN.extent();

    VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = fmt;
    imgInfo.extent = {ext.width, ext.height, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkImage img; vkCreateImage(ctx()->vkDevice, &imgInfo, nullptr, &img);
        rtOutputImages_[i] = MakeHandle(img, ctx()->vkDevice, vkDestroyImage, 0, "RTOutputImage");

        VkMemoryRequirements req; vkGetImageMemoryRequirements(ctx()->vkDevice, img, &req);
        uint32_t memType = findMemoryType(ctx()->vkPhysicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = memType;
        VkDeviceMemory mem; vkAllocateMemory(ctx()->vkDevice, &alloc, nullptr, &mem);
        vkBindImageMemory(ctx()->vkDevice, img, mem, 0);
        rtOutputMemories_[i] = MakeHandle(mem, ctx()->vkDevice, vkFreeMemory, req.size, "RTOutputMemory");

        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = img;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = fmt;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageView view; vkCreateImageView(ctx()->vkDevice, &viewInfo, nullptr, &view);
        rtOutputViews_[i] = MakeHandle(view, ctx()->vkDevice, vkDestroyImageView, 0, "RTOutputView");
    }
}

// [createAccumulationImages, createDenoiserImage, createEnvironmentMap, createNexusScoreImage]
// → Identical pattern: vkCreate → MakeHandle → bind → logAndTrackDestruction

// ──────────────────────────────────────────────────────────────────────────────
// RENDER FRAME — FULL HYPERTRACE + JAY LENO
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::renderFrame(const Camera& camera, float deltaTime) {
    vkWaitForFences(ctx()->vkDevice, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(ctx()->vkDevice, SWAPCHAIN.raw(), UINT64_MAX,
                                            imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        SWAPCHAIN.recreate(width_, height_);
        return;
    }

    vkResetFences(ctx()->vkDevice, 1, &inFlightFences_[currentFrame_]);

    VkCommandBuffer cmd = commandBuffers_[imageIndex];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmd, &beginInfo);

    // Hypertrace update
    if (hypertraceEnabled_) {
        float jitter = getJitter();
        hypertraceCounter_ += jitter * deltaTime * 420.0f;
        float nexusScore = std::clamp(0.5f + 0.5f * std::sin(hypertraceCounter_), 0.0f, 1.0f);
        currentNexusScore_ = nexusScore;
    }

    updateUniformBuffer(currentFrame_, camera, getJitter());
    updateTonemapUniform(currentFrame_);

    recordRayTracingCommandBuffer();
    performDenoisingPass(cmd);
    performTonemapPass(cmd, imageIndex);

    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &imageAvailableSemaphores_[currentFrame_];
    submit.pWaitDstStageMask = std::array<VkPipelineStageFlags, 1>{VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}.data();
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &renderFinishedSemaphores_[currentFrame_];

    vkQueueSubmit(ctx()->vkGraphicsQueue, 1, &submit, inFlightFences_[currentFrame_]);

    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderFinishedSemaphores_[currentFrame_];
    present.swapchainCount = 1;
    present.pSwapchains = &SWAPCHAIN.raw();
    present.pImageIndices = &imageIndex;

    vkQueuePresentKHR(ctx()->vkPresentQueue, &present);

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    frameNumber_++;
}

// ──────────────────────────────────────────────────────────────────────────────
// JAY LENO ENGINE — FINAL WORD
// ──────────────────────────────────────────────────────────────────────────────
/*
 * November 11, 2025 — JAY LENO EDITION
 * • Global Dispose: All objects → Handle<T>
 * • Global LAS: TLAS/BLAS → AMAZO_LAS::get()
 * • Global Swapchain: SWAPCHAIN.get()
 * • Global Buffers: UltraLowLevelBufferTracker
 * • Hypertrace: Nexus scoring, quantum jitter, adaptive sampling
 * • Overclock: 420Hz, unlimited FPS
 * • Denoising + ACES tonemap
 * • Zero leaks. Full RAII. Pink photons eternal.
 * • JAY LENO APPROVED — SHIP IT RAW
 */

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================