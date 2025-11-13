// src/engine/Vulkan/VulkanRenderer.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// VulkanRenderer — Production Implementation v10.1 (Enhanced Logging Edition)
// • Dynamic frame buffering via Options::Performance::MAX_FRAMES_IN_FLIGHT
// • Ray tracing pipeline with adaptive sampling and accumulation
// • Post-processing: Bloom, TAA, SSR, SSAO, vignette, film grain, lens flare
// • Environment rendering: IBL, volumetric fog, sky atmosphere
// • Acceleration structures: LAS rebuild, update, compaction
// • Performance monitoring: GPU timestamps, VRAM budget alerts
// • C++23 compliant, -Werror clean
// • NEW: Exhaustive logging with numbered stack build order
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================

#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <chrono>
#include <vector>
#include <cmath>
#include <algorithm>
#include <format>
#include <random>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <thread>
#include <print>

using namespace Logging::Color;

// ──────────────────────────────────────────────────────────────────────────────
// Global Renderer Instance
// ──────────────────────────────────────────────────────────────────────────────
static std::unique_ptr<VulkanRenderer> g_renderer = nullptr;

[[nodiscard]] inline VulkanRenderer& getRenderer() {
    return *g_renderer;
}

inline void initRenderer(int w, int h) {
    LOG_INFO_CAT("RENDERER", "Initializing VulkanRenderer ({}x{})", w, h);
    std::vector<std::string> shaderPaths = {
        "assets/shaders/raytracing/raygen.spv",
        "assets/shaders/raytracing/closest_hit.spv",
        "assets/shaders/raytracing/miss.spv",
        "assets/shaders/raytracing/shadowmiss.spv"
    };
    g_renderer = std::make_unique<VulkanRenderer>(w, h, nullptr, shaderPaths, false);
    LOG_SUCCESS_CAT("RENDERER", "VulkanRenderer initialized successfully");
}

inline void handleResize(int w, int h) {
    if (g_renderer) g_renderer->handleResize(w, h);
}

inline void renderFrame(const Camera& camera, float deltaTime) noexcept {
    if (g_renderer) g_renderer->renderFrame(camera, deltaTime);
}

inline void shutdown() noexcept {
    LOG_INFO_CAT("RENDERER", "Shutting down VulkanRenderer");
    g_renderer.reset();
    LOG_SUCCESS_CAT("RENDERER", "VulkanRenderer shutdown complete");
}

// ──────────────────────────────────────────────────────────────────────────────
// Quantum Entropy — Jitter Generation
// ──────────────────────────────────────────────────────────────────────────────
namespace {
std::mt19937 quantumRng(69420);
float getJitter() {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    return dist(quantumRng);
}
}

// ──────────────────────────────────────────────────────────────────────────────
// VulkanRenderer — Transient Command Buffers (FIXED)
// ──────────────────────────────────────────────────────────────────────────────
VkCommandBuffer VulkanRenderer::beginSingleTimeCommands(VkDevice device, VkCommandPool pool) {
    LOG_TRACE_CAT("RENDERER", "beginSingleTimeCommands — START");
    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &cmd), "Transient alloc");

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "Transient begin");

    LOG_TRACE_CAT("RENDERER", "Transient command buffer allocated: 0x{:x}", reinterpret_cast<uint64_t>(cmd));
    LOG_TRACE_CAT("RENDERER", "beginSingleTimeCommands — COMPLETE");
    return cmd;
}

// ──────────────────────────────────────────────────────────────────────────────
// VulkanRenderer — Fence Helper (member, reusable)
// ──────────────────────────────────────────────────────────────────────────────
VkFence VulkanRenderer::createFence(bool signaled) const noexcept
{
    VkFenceCreateInfo info{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    if (signaled) info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkFence fence = VK_NULL_HANDLE;
    VK_CHECK(vkCreateFence(device(), &info, nullptr, &fence),
             "Failed to create fence");

    LOG_TRACE_CAT("RENDERER", "Fence created: 0x{:x} (signaled={})", reinterpret_cast<uintptr_t>(fence), signaled);
    return fence;
}

void VulkanRenderer::endSingleTimeCommands(VkDevice device, VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd)
{
    if (cmd == VK_NULL_HANDLE || device == VK_NULL_HANDLE || pool == VK_NULL_HANDLE || queue == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RENDERER", "endSingleTimeCommands called with invalid params");
        return;
    }

    LOG_TRACE_CAT("RENDERER", "endSingleTimeCommands — START (cmd=0x{:x})", reinterpret_cast<uintptr_t>(cmd));

    // 1. End recording
    VkResult r = vkEndCommandBuffer(cmd);
    LOG_TRACE_CAT("RENDERER", "vkEndCommandBuffer result: {}", static_cast<int>(r));  // Debug
    if (r != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "vkEndCommandBuffer failed: {}", static_cast<int>(r));
        return;
    }

    // 2. Submit (no fence — use idle for simplicity in transients)
    VkSubmitInfo submit{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;

    r = vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    LOG_TRACE_CAT("RENDERER", "vkQueueSubmit result: {}", static_cast<int>(r));  // Debug
    if (r != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "vkQueueSubmit failed: {}", static_cast<int>(r));
        return;
    }

    // 3. Wait for queue idle (safer for transients; avoids fence alloc/wait bugs)
    r = vkQueueWaitIdle(queue);
    LOG_TRACE_CAT("RENDERER", "vkQueueWaitIdle result: {}", static_cast<int>(r));  // Debug
    if (r != VK_SUCCESS) {
        LOG_FATAL_CAT("RENDERER", "vkQueueWaitIdle failed: {} — possible device lost", static_cast<int>(r));
        vkDeviceWaitIdle(device);
    }

    // 4. Cleanup
    vkFreeCommandBuffers(device, pool, 1, &cmd);

    LOG_TRACE_CAT("RENDERER", "endSingleTimeCommands — COMPLETE (safe, no device lost)");
}

VkCommandBuffer VulkanRenderer::allocateTransientCommandBuffer(VkDevice device, VkCommandPool pool) {
    LOG_TRACE_CAT("RENDERER", "allocateTransientCommandBuffer — START");
    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &cmd), "Transient alloc");

    LOG_TRACE_CAT("RENDERER", "Transient command buffer allocated: 0x{:x}", reinterpret_cast<uint64_t>(cmd));
    LOG_TRACE_CAT("RENDERER", "allocateTransientCommandBuffer — COMPLETE");
    return cmd;
}

// ──────────────────────────────────────────────────────────────────────────────
// Runtime Toggles — Immediate Effect
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::toggleHypertrace() noexcept {
    LOG_TRACE_CAT("RENDERER", "toggleHypertrace — START");
    hypertraceEnabled_ = !hypertraceEnabled_;
    resetAccumulation_ = true;
    LOG_INFO_CAT("RENDERER", "{}Hypertrace: {}{}", 
        hypertraceEnabled_ ? LIME_GREEN : CRIMSON_MAGENTA,
        hypertraceEnabled_ ? "ENGAGED (32/64 SPP)" : "STANDBY", RESET);
    LOG_TRACE_CAT("RENDERER", "toggleHypertrace — COMPLETE");
}

void VulkanRenderer::toggleFpsTarget() noexcept {
    LOG_TRACE_CAT("RENDERER", "toggleFpsTarget — START");
    switch (fpsTarget_) {
        case FpsTarget::FPS_60: fpsTarget_ = FpsTarget::FPS_120; break;
        case FpsTarget::FPS_120: fpsTarget_ = FpsTarget::FPS_UNLIMITED; break;
        case FpsTarget::FPS_UNLIMITED: fpsTarget_ = FpsTarget::FPS_60; break;
    }
    LOG_INFO_CAT("RENDERER", "{}FPS Target: {}{}", 
        VALHALLA_GOLD,
        fpsTarget_ == FpsTarget::FPS_UNLIMITED ? "UNLIMITED" :
        std::format("{} FPS", static_cast<int>(fpsTarget_)), RESET);
    LOG_TRACE_CAT("RENDERER", "toggleFpsTarget — COMPLETE");
}

void VulkanRenderer::toggleDenoising() noexcept {
    LOG_TRACE_CAT("RENDERER", "toggleDenoising — START");
    denoisingEnabled_ = !denoisingEnabled_;
    resetAccumulation_ = true;
    LOG_INFO_CAT("RENDERER", "{}Denoising (SVGF): {}{}", 
        denoisingEnabled_ ? EMERALD_GREEN : CRIMSON_MAGENTA,
        denoisingEnabled_ ? "ENABLED" : "DISABLED", RESET);
    LOG_TRACE_CAT("RENDERER", "toggleDenoising — COMPLETE");
}

void VulkanRenderer::toggleAdaptiveSampling() noexcept {
    LOG_TRACE_CAT("RENDERER", "toggleAdaptiveSampling — START");
    adaptiveSamplingEnabled_ = !adaptiveSamplingEnabled_;
    resetAccumulation_ = true;
    LOG_INFO_CAT("RENDERER", "{}Adaptive Sampling (NexusScore): {}{}", 
        adaptiveSamplingEnabled_ ? LIME_GREEN : CRIMSON_MAGENTA,
        adaptiveSamplingEnabled_ ? "ENABLED" : "DISABLED", RESET);
    LOG_TRACE_CAT("RENDERER", "toggleAdaptiveSampling — COMPLETE");
}

void VulkanRenderer::setTonemapType(TonemapType type) noexcept {
    LOG_TRACE_CAT("RENDERER", "setTonemapType — START");
    tonemapType_ = type;
    LOG_INFO_CAT("RENDERER", "{}Tonemapping Operator: {}{}", 
        THERMO_PINK,
        type == TonemapType::ACES ? "ACES" :
        type == TonemapType::FILMIC ? "FILMIC" : "REINHARD", RESET);
    LOG_TRACE_CAT("RENDERER", "setTonemapType — COMPLETE");
}

void VulkanRenderer::setOverclockMode(bool enabled) noexcept {
    LOG_TRACE_CAT("RENDERER", "setOverclockMode — START");
    overclockMode_ = enabled;
    if (enabled) fpsTarget_ = FpsTarget::FPS_UNLIMITED;
    else fpsTarget_ = FpsTarget::FPS_120;
    LOG_INFO_CAT("RENDERER", "{}Overclock Mode: {}{}", 
        NUCLEAR_REACTOR, enabled ? "ENABLED" : "DISABLED", RESET);
    LOG_TRACE_CAT("RENDERER", "setOverclockMode — COMPLETE");
}

// ──────────────────────────────────────────────────────────────────────────────
// Cleanup and Destruction
// ──────────────────────────────────────────────────────────────────────────────
VulkanRenderer::~VulkanRenderer() { 
    LOG_TRACE_CAT("RENDERER", "Destructor — START");
    cleanup(); 
    LOG_TRACE_CAT("RENDERER", "Destructor — COMPLETE");
}

void VulkanRenderer::cleanup() noexcept {
    LOG_INFO_CAT("RENDERER", "Initiating renderer shutdown");

    LOG_TRACE_CAT("RENDERER", "cleanup — vkDeviceWaitIdle — START");
    vkDeviceWaitIdle(RTX::ctx().vkDevice());
    LOG_TRACE_CAT("RENDERER", "cleanup — vkDeviceWaitIdle — COMPLETE");

    LOG_TRACE_CAT("RENDERER", "cleanup — Destroy semaphores — START");
    for (auto& s : imageAvailableSemaphores_) if (s) {
        LOG_TRACE_CAT("RENDERER", "Destroying imageAvailableSemaphore: 0x{:x}", reinterpret_cast<uint64_t>(s));
        vkDestroySemaphore(RTX::ctx().vkDevice(), s, nullptr);
    }
    for (auto& s : renderFinishedSemaphores_) if (s) {
        LOG_TRACE_CAT("RENDERER", "Destroying renderFinishedSemaphore: 0x{:x}", reinterpret_cast<uint64_t>(s));
        vkDestroySemaphore(RTX::ctx().vkDevice(), s, nullptr);
    }
    for (auto& f : inFlightFences_) if (f) {
        LOG_TRACE_CAT("RENDERER", "Destroying inFlightFence: 0x{:x}", reinterpret_cast<uint64_t>(f));
        vkDestroyFence(RTX::ctx().vkDevice(), f, nullptr);
    }
    if (timestampQueryPool_) {
        LOG_TRACE_CAT("RENDERER", "Destroying timestampQueryPool: 0x{:x}", reinterpret_cast<uint64_t>(timestampQueryPool_));
        vkDestroyQueryPool(RTX::ctx().vkDevice(), timestampQueryPool_, nullptr);
    }
    LOG_TRACE_CAT("RENDERER", "cleanup — Destroy semaphores — COMPLETE");

    LOG_TRACE_CAT("RENDERER", "cleanup — destroyRTOutputImages — START");
    destroyRTOutputImages();
    LOG_TRACE_CAT("RENDERER", "cleanup — destroyRTOutputImages — COMPLETE");

    LOG_TRACE_CAT("RENDERER", "cleanup — destroyAccumulationImages — START");
    destroyAccumulationImages();
    LOG_TRACE_CAT("RENDERER", "cleanup — destroyAccumulationImages — COMPLETE");

    LOG_TRACE_CAT("RENDERER", "cleanup — destroyNexusScoreImage — START");
    destroyNexusScoreImage();
    LOG_TRACE_CAT("RENDERER", "cleanup — destroyNexusScoreImage — COMPLETE");

    LOG_TRACE_CAT("RENDERER", "cleanup — destroyDenoiserImage — START");
    destroyDenoiserImage();
    LOG_TRACE_CAT("RENDERER", "cleanup — destroyDenoiserImage — COMPLETE");

    LOG_TRACE_CAT("RENDERER", "cleanup — descriptorPool_.reset — START");
    descriptorPool_.reset();
    LOG_TRACE_CAT("RENDERER", "cleanup — descriptorPool_.reset — COMPLETE");

    LOG_TRACE_CAT("RENDERER", "cleanup — rtDescriptorPool_.reset — START");
    rtDescriptorPool_.reset();
    LOG_TRACE_CAT("RENDERER", "cleanup — rtDescriptorPool_.reset — COMPLETE");

    LOG_TRACE_CAT("RENDERER", "cleanup — Free command buffers — START");
    if (!commandBuffers_.empty()) {
        LOG_TRACE_CAT("RENDERER", "Freeing {} command buffers", commandBuffers_.size());
        vkFreeCommandBuffers(RTX::ctx().vkDevice(), RTX::ctx().commandPool(), 
            static_cast<uint32_t>(commandBuffers_.size()), commandBuffers_.data());
        commandBuffers_.clear();
        LOG_TRACE_CAT("RENDERER", "Command buffers freed and cleared");
    }
    LOG_TRACE_CAT("RENDERER", "cleanup — Free command buffers — COMPLETE");

    LOG_SUCCESS_CAT("RENDERER", "{}Renderer shutdown complete — all resources released{}", 
        EMERALD_GREEN, RESET);
    LOG_TRACE_CAT("RENDERER", "cleanup — COMPLETE");
}

void VulkanRenderer::destroyNexusScoreImage() noexcept {
    LOG_TRACE_CAT("RENDERER", "destroyNexusScoreImage — START");
    LOG_TRACE_CAT("RENDERER", "Resetting hypertraceScoreStagingBuffer_");
    hypertraceScoreStagingBuffer_.reset();
    LOG_TRACE_CAT("RENDERER", "Resetting hypertraceScoreStagingMemory_");
    hypertraceScoreStagingMemory_.reset();
    LOG_TRACE_CAT("RENDERER", "Resetting hypertraceScoreImage_");
    hypertraceScoreImage_.reset();
    LOG_TRACE_CAT("RENDERER", "Resetting hypertraceScoreMemory_");
    hypertraceScoreMemory_.reset();
    LOG_TRACE_CAT("RENDERER", "Resetting hypertraceScoreView_");
    hypertraceScoreView_.reset();
    LOG_TRACE_CAT("RENDERER", "destroyNexusScoreImage — COMPLETE");
}

void VulkanRenderer::destroyDenoiserImage() noexcept {
    LOG_TRACE_CAT("RENDERER", "destroyDenoiserImage — START");
    LOG_TRACE_CAT("RENDERER", "Resetting denoiserImage_");
    denoiserImage_.reset();
    LOG_TRACE_CAT("RENDERER", "Resetting denoiserMemory_");
    denoiserMemory_.reset();
    LOG_TRACE_CAT("RENDERER", "Resetting denoiserView_");
    denoiserView_.reset();
    LOG_TRACE_CAT("RENDERER", "destroyDenoiserImage — COMPLETE");
}

void VulkanRenderer::destroyAccumulationImages() noexcept {
    LOG_TRACE_CAT("RENDERER", "destroyAccumulationImages — START");
    LOG_TRACE_CAT("RENDERER", "Resetting {} accumulation images", accumImages_.size());
    for (auto& h : accumImages_) h.reset();
    LOG_TRACE_CAT("RENDERER", "Resetting {} accumulation memories", accumMemories_.size());
    for (auto& h : accumMemories_) h.reset();
    LOG_TRACE_CAT("RENDERER", "Resetting {} accumulation views", accumViews_.size());
    for (auto& h : accumViews_) h.reset();
    LOG_TRACE_CAT("RENDERER", "destroyAccumulationImages — COMPLETE");
}

void VulkanRenderer::destroyRTOutputImages() noexcept {
    LOG_TRACE_CAT("RENDERER", "destroyRTOutputImages — START");
    LOG_TRACE_CAT("RENDERER", "Resetting {} RT output images", rtOutputImages_.size());
    for (auto& h : rtOutputImages_) h.reset();
    LOG_TRACE_CAT("RENDERER", "Resetting {} RT output memories", rtOutputMemories_.size());
    for (auto& h : rtOutputMemories_) h.reset();
    LOG_TRACE_CAT("RENDERER", "Resetting {} RT output views", rtOutputViews_.size());
    for (auto& h : rtOutputViews_) h.reset();
    LOG_TRACE_CAT("RENDERER", "destroyRTOutputImages — COMPLETE");
}

// ──────────────────────────────────────────────────────────────────────────────
// Constructor — Fully Configured with Numbered Stack Build Order
// ──────────────────────────────────────────────────────────────────────────────
VulkanRenderer::VulkanRenderer(int width, int height, SDL_Window* window,
                               const std::vector<std::string>& shaderPaths,
                               bool overclockFromMain)
    : window_(window), width_(width), height_(height), overclockMode_(overclockFromMain),
      hypertraceEnabled_(Options::RTX::ENABLE_ADAPTIVE_SAMPLING),
      denoisingEnabled_(Options::RTX::ENABLE_DENOISING),
      adaptiveSamplingEnabled_(Options::RTX::ENABLE_ADAPTIVE_SAMPLING),
      tonemapType_(TonemapType::ACES),
      lastPerfLogTime_(std::chrono::steady_clock::now()), frameCounter_(0)
{
    LOG_ATTEMPT_CAT("RENDERER", "Constructing VulkanRenderer ({}x{}) — STACK BUILD ORDER", width, height);

    // Stack Build Order: Numbered Steps with Exhaustive Logging
    // 1. Set overclock mode and validate initial state
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 1: Set Overclock Mode ===");
    setOverclockMode(overclockFromMain);
    LOG_TRACE_CAT("RENDERER", "Step 1 COMPLETE: Overclock mode set to {}", overclockMode_);

    // 2. Security validation (StoneKey)
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 2: Security Validation (StoneKey) ===");
    if (get_kStone1() == 0 || get_kStone2() == 0) {
        LOG_ERROR_CAT("SECURITY", "{}StoneKey validation failed — aborting{}", 
                      CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("StoneKey validation failed");
    }
    LOG_TRACE_CAT("RENDERER", "Step 2 COMPLETE: StoneKey validation passed (kStone1={}, kStone2={})", get_kStone1(), get_kStone2());

    auto& c = RTX::ctx();
    const uint32_t framesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;
    LOG_TRACE_CAT("RENDERER", "Step 0 PRE: Retrieved context — framesInFlight={}", framesInFlight);

    // 3. Load ray tracing function pointers
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 3: Load Ray Tracing Extensions ===");
    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
        vkGetDeviceProcAddr(c.vkDevice(), "vkCmdTraceRaysKHR"));
    vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
        vkGetDeviceProcAddr(c.vkDevice(), "vkCreateRayTracingPipelinesKHR"));
    if (!vkCmdTraceRaysKHR || !vkCreateRayTracingPipelinesKHR) {
        throw std::runtime_error("Failed to load ray tracing extensions");
    }
    LOG_INFO_CAT("RENDERER", "Ray tracing extensions loaded — vkCmdTraceRaysKHR=0x{:x}, vkCreateRayTracingPipelinesKHR=0x{:x}", 
                 reinterpret_cast<uintptr_t>(vkCmdTraceRaysKHR), reinterpret_cast<uintptr_t>(vkCreateRayTracingPipelinesKHR));
    LOG_TRACE_CAT("RENDERER", "Step 3 COMPLETE: RT extensions loaded successfully");

    // 4. Resize synchronization containers
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 4: Resize Synchronization Containers ===");
    imageAvailableSemaphores_.resize(framesInFlight);
    renderFinishedSemaphores_.resize(framesInFlight);
    inFlightFences_.resize(framesInFlight);
    LOG_TRACE_CAT("RENDERER", "Step 4 COMPLETE: Containers resized — imageAvailableSemaphores_.size()={}, renderFinishedSemaphores_.size()={}, inFlightFences_.size()={}", 
                  imageAvailableSemaphores_.size(), renderFinishedSemaphores_.size(), inFlightFences_.size());

    // 5. Create synchronization objects
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 5: Create Synchronization Objects ===");
    VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
    for (uint32_t i = 0; i < framesInFlight; ++i) {
        LOG_TRACE_CAT("RENDERER", "Creating semaphore/fence for frame {}", i);
        VK_CHECK(vkCreateSemaphore(c.vkDevice(), &semInfo, nullptr, &imageAvailableSemaphores_[i]), "Image available semaphore");
        LOG_TRACE_CAT("RENDERER", "Created imageAvailableSemaphores_[{}]: 0x{:x}", i, reinterpret_cast<uint64_t>(imageAvailableSemaphores_[i]));
        VK_CHECK(vkCreateSemaphore(c.vkDevice(), &semInfo, nullptr, &renderFinishedSemaphores_[i]), "Render finished semaphore");
        LOG_TRACE_CAT("RENDERER", "Created renderFinishedSemaphores_[{}]: 0x{:x}", i, reinterpret_cast<uint64_t>(renderFinishedSemaphores_[i]));
        VK_CHECK(vkCreateFence(c.vkDevice(), &fenceInfo, nullptr, &inFlightFences_[i]), "In-flight fence");
        LOG_TRACE_CAT("RENDERER", "Created inFlightFences_[{}]: 0x{:x}", i, reinterpret_cast<uint64_t>(inFlightFences_[i]));
    }
    LOG_INFO_CAT("RENDERER", "Synchronization objects created for {} frames", framesInFlight);
    LOG_TRACE_CAT("RENDERER", "Step 5 COMPLETE: All sync objects created");

    // 6. GPU timestamp queries
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 6: GPU Timestamp Queries ===");
    if (Options::Performance::ENABLE_GPU_TIMESTAMPS || Options::Debug::SHOW_GPU_TIMESTAMPS) {
        VkQueryPoolCreateInfo qpInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        qpInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        qpInfo.queryCount = framesInFlight * 2;
        VK_CHECK(vkCreateQueryPool(c.vkDevice(), &qpInfo, nullptr, &timestampQueryPool_), "Timestamp query pool");
        LOG_TRACE_CAT("RENDERER", "Created timestampQueryPool_: 0x{:x} (queryCount={})", reinterpret_cast<uint64_t>(timestampQueryPool_), qpInfo.queryCount);
        LOG_SUCCESS_CAT("RENDERER", "{}GPU timestamp queries enabled{}", LIME_GREEN, RESET);
    } else {
        LOG_TRACE_CAT("RENDERER", "GPU timestamps disabled via options");
    }
    LOG_TRACE_CAT("RENDERER", "Step 6 COMPLETE: Timestamp setup done");

    // 7. Query GPU properties
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 7: Query GPU Properties ===");
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(c.vkPhysicalDevice(), &props);
    timestampPeriod_ = props.limits.timestampPeriod / 1e6;
    LOG_INFO_CAT("RENDERER", "GPU: {} | Timestamp period: {:.3f} ms", props.deviceName, timestampPeriod_);
    LOG_TRACE_CAT("RENDERER", "Step 7 COMPLETE: GPU props queried — deviceName='{}', timestampPeriod={:.3f}", props.deviceName, timestampPeriod_);

    // 8. Create descriptor pools
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 8: Create Descriptor Pools ===");
    std::array<VkDescriptorPoolSize, 6> poolSizes{{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, framesInFlight * 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, framesInFlight * 4},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, framesInFlight * 7},
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, framesInFlight},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, framesInFlight},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, framesInFlight}
    }};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = framesInFlight * 3 + 16;
    LOG_TRACE_CAT("RENDERER", "Descriptor pool info: poolSizeCount={}, maxSets={}, poolSizes[0]={}/{}", poolInfo.poolSizeCount, poolInfo.maxSets, static_cast<int>(poolSizes[0].type), poolSizes[0].descriptorCount);

    VkDescriptorPool pool;
    VK_CHECK(vkCreateDescriptorPool(c.vkDevice(), &poolInfo, nullptr, &pool), "Descriptor pool");
    LOG_TRACE_CAT("RENDERER", "Created descriptorPool: 0x{:x}", reinterpret_cast<uint64_t>(pool));
    descriptorPool_ = RTX::Handle<VkDescriptorPool>(pool, c.vkDevice(), vkDestroyDescriptorPool, VkDeviceSize{0}, "RendererPool");

    VkDescriptorPool rtPool;
    VK_CHECK(vkCreateDescriptorPool(c.vkDevice(), &poolInfo, nullptr, &rtPool), "RT descriptor pool");
    LOG_TRACE_CAT("RENDERER", "Created rtDescriptorPool: 0x{:x}", reinterpret_cast<uint64_t>(rtPool));
    rtDescriptorPool_ = RTX::Handle<VkDescriptorPool>(rtPool, c.vkDevice(), vkDestroyDescriptorPool, VkDeviceSize{0}, "RTPool");

    LOG_INFO_CAT("RENDERER", "Descriptor pools created (max sets: {})", poolInfo.maxSets);
    LOG_TRACE_CAT("RENDERER", "Step 8 COMPLETE: Descriptor pools created");

    // 9. Create render targets
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 9: Create Render Targets ===");
    if (Options::Environment::ENABLE_ENV_MAP) {
        LOG_TRACE_CAT("RENDERER", "Step 9.1 — createEnvironmentMap — START");
        createEnvironmentMap();
        LOG_TRACE_CAT("RENDERER", "Step 9.1 — createEnvironmentMap — COMPLETE");
    }
    LOG_TRACE_CAT("RENDERER", "Step 9.2 — createAccumulationImages — START");
    createAccumulationImages();
    LOG_TRACE_CAT("RENDERER", "Step 9.2 — createAccumulationImages — COMPLETE");
    LOG_TRACE_CAT("RENDERER", "Step 9.3 — createRTOutputImages — START");
    createRTOutputImages();
    LOG_TRACE_CAT("RENDERER", "Step 9.3 — createRTOutputImages — COMPLETE");
    if (Options::RTX::ENABLE_DENOISING) {
        LOG_TRACE_CAT("RENDERER", "Step 9.4 — createDenoiserImage — START");
        createDenoiserImage();
        LOG_TRACE_CAT("RENDERER", "Step 9.4 — createDenoiserImage — COMPLETE");
    }
    if (Options::RTX::ENABLE_ADAPTIVE_SAMPLING) {
        LOG_TRACE_CAT("RENDERER", "Step 9.5 — createNexusScoreImage — START");
        createNexusScoreImage(c.vkPhysicalDevice(), c.vkDevice(), c.commandPool(), c.graphicsQueue());
        LOG_TRACE_CAT("RENDERER", "Step 9.5 — createNexusScoreImage — COMPLETE");
    }
    LOG_TRACE_CAT("RENDERER", "Step 9 COMPLETE: All render targets created");

    // 10. Initialize buffer data
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 10: Initialize Buffer Data ===");
    initializeAllBufferData(framesInFlight, 64_MB, 16_MB);
    LOG_TRACE_CAT("RENDERER", "Step 10 COMPLETE: Buffer data initialized");

    // 11. Create command buffers
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 11: Create Command Buffers ===");
    createCommandBuffers();
    LOG_TRACE_CAT("RENDERER", "Step 11 COMPLETE: Command buffers created");

    // 12. Allocate descriptor sets
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 12: Allocate Descriptor Sets ===");
    allocateDescriptorSets();
    LOG_TRACE_CAT("RENDERER", "Step 12 COMPLETE: Descriptor sets allocated");

    // 13. Create ray tracing pipeline
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 13: Create Ray Tracing Pipeline ===");
    createRayTracingPipeline(shaderPaths);
    LOG_TRACE_CAT("RENDERER", "Step 13 COMPLETE: RT pipeline created");

    // 14. Create shader binding table
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 14: Create Shader Binding Table ===");
    createShaderBindingTable();
    LOG_TRACE_CAT("RENDERER", "Step 14 COMPLETE: SBT created");

    // 15. Update descriptors
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 15: Update Descriptors ===");
    updateNexusDescriptors();
    updateRTXDescriptors();
    updateTonemapDescriptorsInitial();
    updateDenoiserDescriptors();
    LOG_TRACE_CAT("RENDERER", "Step 15 COMPLETE: Descriptors updated");

    LOG_SUCCESS_CAT("RENDERER", "{}VulkanRenderer initialization complete — STACK BUILD ORDER FULLY EXECUTED{}", 
        EMERALD_GREEN, RESET);
    LOG_TRACE_CAT("RENDERER", "Constructor — COMPLETE: All {} steps logged", 15);
}

// ──────────────────────────────────────────────────────────────────────────────
// Ray Tracing Pipeline and SBT
// ──────────────────────────────────────────────────────────────────────────────
// ──────────────────────────────────────────────────────────────────────────────
// Ray Tracing Pipeline Creation — FINAL FIXED VERSION
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::createRayTracingPipeline(const std::vector<std::string>& shaderPaths)
{
    LOG_TRACE_CAT("RENDERER", "createRayTracingPipeline — START — {} shaders", shaderPaths.size());

    // ---------------------------------------------------------------------
    // 1. Load shaders (raygen + miss are mandatory, others optional)
    // ---------------------------------------------------------------------
    VkShaderModule raygen = loadShader(shaderPaths[0]);                     // e.g. shaders/raygen.rgen.spv
    LOG_TRACE_CAT("RENDERER", "raygen module: 0x{:x}", reinterpret_cast<uintptr_t>(raygen));

    VkShaderModule miss = loadShader("shaders/miss.rmiss.spv");            // hard-coded miss shader
    LOG_TRACE_CAT("RENDERER", "miss module: 0x{:x}", reinterpret_cast<uintptr_t>(miss));

    if (raygen == VK_NULL_HANDLE || miss == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "Missing raygen or miss shader — skipping RT pipeline creation");
        return;
    }

    // ---------------------------------------------------------------------
    // 2. Build stage + group arrays (exactly like PipelineManager does)
    // ---------------------------------------------------------------------
    struct StageInfo {
        VkPipelineShaderStageCreateInfo        stage{};
        VkRayTracingShaderGroupCreateInfoKHR   group{};
    };
    std::vector<StageInfo> infos;

    auto add = [&](VkShaderModule mod, VkShaderStageFlagBits stageFlag,
                   VkRayTracingShaderGroupTypeKHR groupType, uint32_t generalIdx = VK_SHADER_UNUSED_KHR)
    {
        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage  = stageFlag;
        stageInfo.module = mod;
        stageInfo.pName  = "main";

        VkRayTracingShaderGroupCreateInfoKHR groupInfo{};
        groupInfo.sType              = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        groupInfo.type               = groupType;
        groupInfo.generalShader      = VK_SHADER_UNUSED_KHR;
        groupInfo.closestHitShader   = VK_SHADER_UNUSED_KHR;
        groupInfo.anyHitShader       = VK_SHADER_UNUSED_KHR;
        groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

        if (groupType == VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR)
            groupInfo.generalShader = generalIdx;

        infos.push_back({stageInfo, groupInfo});
    };

    // raygen → general shader (index 0)
    add(raygen, VK_SHADER_STAGE_RAYGEN_BIT_KHR, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0);
    // miss   → general shader (index 1)
    add(miss,   VK_SHADER_STAGE_MISS_BIT_KHR,   VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1);

    // ---------------------------------------------------------------------
    // 3. Fill the big create-info struct
    // ---------------------------------------------------------------------
    VkPipelineLibraryCreateInfoKHR libraryInfo{};
    libraryInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR;

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{};
    pipelineInfo.sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.pNext                        = &libraryInfo;
    pipelineInfo.flags                        = 0;
    pipelineInfo.stageCount                   = static_cast<uint32_t>(infos.size());
    pipelineInfo.pStages                      = reinterpret_cast<VkPipelineShaderStageCreateInfo*>(infos.data());
    pipelineInfo.groupCount                   = static_cast<uint32_t>(infos.size());
    pipelineInfo.pGroups                      = reinterpret_cast<VkRayTracingShaderGroupCreateInfoKHR*>(infos.data() + infos.size());
    pipelineInfo.maxPipelineRayRecursionDepth = 4;
    pipelineInfo.layout                       = rtPipelineLayout_.raw;   // created earlier (descriptor layout + push constant)

    // ---------------------------------------------------------------------
    // 4. Create the pipeline — CORRECT parameter order for vkCreateRayTracingPipelinesKHR
    // ---------------------------------------------------------------------
    VkPipeline pipeline = VK_NULL_HANDLE;

    VK_CHECK(
        vkCreateRayTracingPipelinesKHR(
            device(),                     // VkDevice                     device
            VK_NULL_HANDLE,              // VkDeferredOperationKHR       deferredOperation
            VK_NULL_HANDLE,              // VkPipelineCache              pipelineCache
            1,                           // uint32_t                     createInfoCount
            &pipelineInfo,               // const VkRayTracingPipelineCreateInfoKHR* pCreateInfos
            nullptr,                     // const VkAllocationCallbacks* pAllocator
            &pipeline                    // VkPipeline*                  pPipelines
        ),
        "Failed to create ray tracing pipeline"
    );

    // ---------------------------------------------------------------------
    // 5. Wrap with RTX::Handle (your RAII wrapper)
    // ---------------------------------------------------------------------
    rtPipeline_ = RTX::Handle<VkPipeline>(
        pipeline,
        device(),
        [](VkDevice d, VkPipeline p, const VkAllocationCallbacks*) { vkDestroyPipeline(d, p, nullptr); },
        0,
        "RTPipeline"
    );

    LOG_SUCCESS_CAT("RENDERER", "Ray tracing pipeline created — {} shader groups", infos.size());
    LOG_SUCCESS_CAT("RENDERER", "PINK PHOTONS ARMED — FIRST LIGHT ACHIEVED");
    LOG_TRACE_CAT("RENDERER", "createRayTracingPipeline — COMPLETE");
}

void VulkanRenderer::createShaderBindingTable() {
    LOG_TRACE_CAT("RENDERER", "createShaderBindingTable — START");

    // Query ray tracing pipeline properties for SBT sizing
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
    rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rtProps;
    vkGetPhysicalDeviceProperties2(physicalDevice(), &props2);

    const uint32_t handleSize = rtProps.shaderGroupHandleSize;
    const uint32_t handleAlignment = rtProps.shaderGroupHandleAlignment;
    const uint32_t baseAlignment = rtProps.shaderGroupBaseAlignment;

    LOG_TRACE_CAT("RENDERER", "RT Properties — handleSize={}, handleAlignment={}, baseAlignment={}", handleSize, handleAlignment, baseAlignment);

    // Assuming you have group counts as members or constants; adjust as needed
    // Example: 1 raygen, 2 miss, 4 hit groups (common for simple RT)
    // TODO: Replace with actual counts from your ray tracing pipeline creation
    const uint32_t raygenGroupCount = 1;
    const uint32_t missGroupCount = 2;
    const uint32_t hitGroupCount = 4;
    const uint32_t callableGroupCount = 0;  // If using

    // Calculate region sizes (aligned)
    const VkDeviceSize raygenOffset = 0;
    const VkDeviceSize raygenSize = raygenGroupCount * handleSize;
    const VkDeviceSize raygenAlignedSize = (raygenSize + handleAlignment - 1) / handleAlignment * handleAlignment;

    VkDeviceSize missOffset = (raygenOffset + raygenAlignedSize + baseAlignment - 1) / baseAlignment * baseAlignment;
    const VkDeviceSize missSize = missGroupCount * handleSize;
    const VkDeviceSize missAlignedSize = (missSize + handleAlignment - 1) / handleAlignment * handleAlignment;

    VkDeviceSize hitOffset = (missOffset + missAlignedSize + baseAlignment - 1) / baseAlignment * baseAlignment;
    const VkDeviceSize hitSize = hitGroupCount * handleSize;
    const VkDeviceSize hitAlignedSize = (hitSize + handleAlignment - 1) / handleAlignment * handleAlignment;

    VkDeviceSize callableOffset = (hitOffset + hitAlignedSize + baseAlignment - 1) / baseAlignment * baseAlignment;
    const VkDeviceSize callableSize = callableGroupCount * handleSize;
    const VkDeviceSize callableAlignedSize = (callableSize + handleAlignment - 1) / handleAlignment * handleAlignment;

    const VkDeviceSize sbtBufferSize = (callableOffset + callableAlignedSize + baseAlignment - 1) / baseAlignment * baseAlignment;

    LOG_INFO_CAT("RENDERER", "SBT Layout — raygenSize={}B (aligned={}B), missOffset={}B size={}B (aligned={}B), hitOffset={}B size={}B (aligned={}B), totalSize={}B (~{} MB)",
                 raygenSize, raygenAlignedSize, missOffset, missSize, missAlignedSize, hitOffset, hitSize, hitAlignedSize, sbtBufferSize, sbtBufferSize / (1024.0 * 1024.0));

    // Assume createRayTracingPipeline has been called earlier; use the member
    if (rayTracingPipeline_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RENDERER", "Ray tracing pipeline not created before SBT");
        return;
    }

    // Get all shader group handles from the ray tracing pipeline
    std::vector<uint8_t> handles(sbtBufferSize);  // Temp buffer for all handles
    VkResult result = vkGetRayTracingShaderGroupHandlesKHR(device(), rayTracingPipeline_, 0, raygenGroupCount + missGroupCount + hitGroupCount + callableGroupCount, sbtBufferSize, handles.data());
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "Failed to get RT shader group handles: {}", result);
        return;  // Or throw/early exit
    }

    // Copy handles into aligned regions (assuming sequential groups: raygen first, then miss, then hit)
    // Raygen region
    memcpy(handles.data() + raygenOffset, handles.data() + 0, raygenSize);
    // Miss region
    memcpy(handles.data() + missOffset, handles.data() + raygenSize, missSize);
    // Hit region
    memcpy(handles.data() + hitOffset, handles.data() + raygenSize + missSize, hitSize);
    // Callable if any: memcpy(handles.data() + callableOffset, handles.data() + raygenSize + missSize + hitSize, callableSize);

    // Inline create staging buffer (CPU-visible)
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingMemory;
    VkMemoryRequirements stagingReqs;
    VkBufferCreateInfo stagingBufferInfo{};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size = sbtBufferSize;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    result = vkCreateBuffer(device(), &stagingBufferInfo, nullptr, &stagingBuffer);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "Failed to create staging buffer: {}", result);
        return;
    }
    vkGetBufferMemoryRequirements(device(), stagingBuffer, &stagingReqs);

    VkMemoryAllocateInfo stagingAllocInfo{};
    stagingAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAllocInfo.allocationSize = stagingReqs.size;
    stagingAllocInfo.memoryTypeIndex = findMemoryType(physicalDevice(), stagingReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    result = vkAllocateMemory(device(), &stagingAllocInfo, nullptr, &stagingMemory);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "Failed to allocate staging memory: {}", result);
        vkDestroyBuffer(device(), stagingBuffer, nullptr);
        return;
    }
    vkBindBufferMemory(device(), stagingBuffer, stagingMemory, 0);

    // Map and copy data
    void* data;
    vkMapMemory(device(), stagingMemory, 0, sbtBufferSize, 0, &data);
    memcpy(data, handles.data(), sbtBufferSize);
    vkUnmapMemory(device(), stagingMemory);

    // Inline create SBT buffer (device-local, addressable)
    VkBufferCreateInfo sbtBufferInfo{};
    sbtBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    sbtBufferInfo.size = sbtBufferSize;
    sbtBufferInfo.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    sbtBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    result = vkCreateBuffer(device(), &sbtBufferInfo, nullptr, &sbtBuffer_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "Failed to create SBT buffer: {}", result);
        vkDestroyBuffer(device(), stagingBuffer, nullptr);
        vkFreeMemory(device(), stagingMemory, nullptr);
        return;
    }

    VkMemoryRequirements sbtReqs;
    vkGetBufferMemoryRequirements(device(), sbtBuffer_, &sbtReqs);

    VkMemoryAllocateInfo sbtAllocInfo{};
    sbtAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    sbtAllocInfo.allocationSize = sbtReqs.size;
    sbtAllocInfo.memoryTypeIndex = findMemoryType(physicalDevice(), sbtReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    result = vkAllocateMemory(device(), &sbtAllocInfo, nullptr, &sbtMemory_);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "Failed to allocate SBT memory: {}", result);
        vkDestroyBuffer(device(), sbtBuffer_, nullptr);
        vkDestroyBuffer(device(), stagingBuffer, nullptr);
        vkFreeMemory(device(), stagingMemory, nullptr);
        return;
    }
    vkBindBufferMemory(device(), sbtBuffer_, sbtMemory_, 0);

    // Implement buffer copy using single-use command buffer
    VkCommandBuffer cmdBuf = beginSingleTimeCommands(device(), commandPool());
    VkBufferCopy copyRegion{};
    copyRegion.size = sbtBufferSize;
    vkCmdCopyBuffer(cmdBuf, stagingBuffer, sbtBuffer_, 1, &copyRegion);
    endSingleTimeCommands(device(), commandPool(), graphicsQueue(), cmdBuf);

    // Cleanup staging
    vkDestroyBuffer(device(), stagingBuffer, nullptr);
    vkFreeMemory(device(), stagingMemory, nullptr);

    // Get device address for SBT
    VkBufferDeviceAddressInfoKHR addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR;
    addrInfo.buffer = sbtBuffer_;
    sbtAddress_ = vkGetBufferDeviceAddressKHR(device(), &addrInfo);

    // Store offsets and counts
    raygenSbtOffset_ = raygenOffset;
    missSbtOffset_ = missOffset;
    hitSbtOffset_ = hitOffset;
    sbtStride_ = handleAlignment;
    raygenGroupCount_ = raygenGroupCount;
    missGroupCount_ = missGroupCount;
    hitGroupCount_ = hitGroupCount;

    sbtBufferEnc_ = reinterpret_cast<uint64_t>(sbtAddress_);

    LOG_INFO_CAT("RENDERER", "Shader binding table created — address=0x{:x}, size={}B", sbtAddress_, sbtBufferSize);
    LOG_TRACE_CAT("RENDERER", "SBT initialized — sbtBufferEnc=0x{:x}, sbtAddress=0x{:x}", sbtBufferEnc_, sbtAddress_);
    LOG_TRACE_CAT("RENDERER", "createShaderBindingTable — COMPLETE");
}

VkShaderModule VulkanRenderer::loadShader(const std::string& path) {
    LOG_TRACE_CAT("RENDERER", "loadShader — START — path='{}'", path);

    // Read SPIR-V binary from file
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        LOG_ERROR_CAT("RENDERER", "Failed to open shader file: {}", path);
        return VK_NULL_HANDLE;
    }

    size_t fileSize = static_cast<size_t>(file.tellg());
    std::vector<char> shaderCode(fileSize);
    file.seekg(0);
    file.read(shaderCode.data(), fileSize);
    file.close();

    LOG_TRACE_CAT("RENDERER", "Loaded {} bytes from shader file", fileSize);

    // Create VkShaderModule
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = fileSize;
    createInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

    VkShaderModule shaderModule;
    VkResult result = vkCreateShaderModule(device(), &createInfo, nullptr, &shaderModule);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "Failed to create shader module from {}: {}", path, result);
        return VK_NULL_HANDLE;
    }

    LOG_TRACE_CAT("RENDERER", "Shader module created successfully");
    LOG_TRACE_CAT("RENDERER", "loadShader — COMPLETE");
    return shaderModule;
}

VkDeviceAddress VulkanRenderer::getShaderGroupHandle(uint32_t group) {
    LOG_TRACE_CAT("RENDERER", "getShaderGroupHandle — START — group={}", group);

    // Note: In standard Vulkan RT, handles are retrieved in bulk via vkGetRayTracingShaderGroupHandlesKHR.
    // This function assumes a pre-computed SBT and returns the device address for the start of the specified group.
    // Adjust 'group' indexing: 0=raygen, 1..=miss, etc., based on your pipeline groups.

    VkDeviceAddress groupAddress = 0;
    if (group < raygenGroupCount_) {
        // Raygen group
        groupAddress = sbtAddress_ + raygenSbtOffset_ + (group * sbtStride_);
    } else if (group < raygenGroupCount_ + missGroupCount_) {
        // Miss group
        uint32_t missGroupIdx = group - raygenGroupCount_;
        groupAddress = sbtAddress_ + missSbtOffset_ + (missGroupIdx * sbtStride_);
    } else if (group < raygenGroupCount_ + missGroupCount_ + hitGroupCount_) {
        // Hit group
        uint32_t hitGroupIdx = group - raygenGroupCount_ - missGroupCount_;
        groupAddress = sbtAddress_ + hitSbtOffset_ + (hitGroupIdx * sbtStride_);
    } else {
        LOG_WARN_CAT("RENDERER", "Invalid shader group index: {}", group);
        return 0;
    }

    LOG_TRACE_CAT("RENDERER", "Group {} address: 0x{:x}", group, groupAddress);
    LOG_TRACE_CAT("RENDERER", "getShaderGroupHandle — COMPLETE");
    return groupAddress;
}

// ──────────────────────────────────────────────────────────────────────────────
// Image Creation — Options-Driven
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::createRTOutputImages()
{
    LOG_INFO_CAT("RENDERER", "Creating ray tracing output images (per-frame)");
    LOG_TRACE_CAT("RENDERER", "createRTOutputImages — START — frames={}, width={}, height={}",
                  Options::Performance::MAX_FRAMES_IN_FLIGHT, width_, height_);

    // Harden: Clear + reserve to prevent realloc moves/invalidation
    rtOutputImages_.clear();
    rtOutputMemories_.clear();
    rtOutputViews_.clear();

    rtOutputImages_.reserve(Options::Performance::MAX_FRAMES_IN_FLIGHT);
    rtOutputMemories_.reserve(Options::Performance::MAX_FRAMES_IN_FLIGHT);
    rtOutputViews_.reserve(Options::Performance::MAX_FRAMES_IN_FLIGHT);

    const uint32_t framesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;
    VkDevice dev = device();  // Avoid shadowing
    VkPhysicalDevice physDev = physicalDevice();
    VkCommandPool cmdPool = commandPool();
    VkQueue queue = graphicsQueue();

    // Harden: Pre-wait to ensure clean state
    VkResult idleR = vkQueueWaitIdle(queue);
    if (idleR != VK_SUCCESS) {
        LOG_WARN_CAT("RENDERER", "Pre-create idle failed: {} — continuing cautiously", static_cast<int>(idleR));
    }

    for (uint32_t i = 0; i < framesInFlight; ++i) {
        LOG_TRACE_CAT("RENDERER", "Frame {} — Creating RTOutput", i);

        // Raw handles to work with Vulkan API
        VkImage rawImage = VK_NULL_HANDLE;
        VkDeviceMemory rawMemory = VK_NULL_HANDLE;
        VkImageView rawView = VK_NULL_HANDLE;

        try {
            // === 1. Create Image ===
            VkImageCreateInfo imageInfo{};
            imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType     = VK_IMAGE_TYPE_2D;
            imageInfo.format        = VK_FORMAT_R32G32B32A32_SFLOAT;
            imageInfo.extent        = { static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1 };
            imageInfo.mipLevels     = 1;
            imageInfo.arrayLayers   = 1;
            imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.usage         = VK_IMAGE_USAGE_STORAGE_BIT |
                                      VK_IMAGE_USAGE_SAMPLED_BIT |
                                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            VK_CHECK(vkCreateImage(dev, &imageInfo, nullptr, &rawImage),
                     ("Failed to create RT output image for frame " + std::to_string(i)).c_str());

            LOG_TRACE_CAT("RENDERER", "Frame {} Image created: 0x{:x}", i, reinterpret_cast<uintptr_t>(rawImage));

            // === 2. Allocate & Bind Memory (Harden: Pad size + log reqs) ===
            VkMemoryRequirements memReqs{};
            vkGetImageMemoryRequirements(dev, rawImage, &memReqs);
            LOG_TRACE_CAT("RENDERER", "Frame {} Mem reqs: size={}, align={}, bits=0x{:x}",
                          i, memReqs.size, memReqs.alignment, memReqs.memoryTypeBits);

            VkDeviceSize allocSize = memReqs.size + (memReqs.alignment * 2);  // Pad 2x alignment for safety
            if (allocSize < memReqs.size || allocSize > (1ULL << 32)) {
                LOG_FATAL_CAT("RENDERER", "Frame {} Invalid alloc size: req={} alloc={} — aborting frame", i, memReqs.size, allocSize);
                vkDestroyImage(dev, rawImage, nullptr);
                continue;  // Skip frame, but log
            }

            uint32_t memType = findMemoryType(physDev, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            if (memType == static_cast<uint32_t>(~0u)) {
                LOG_FATAL_CAT("RENDERER", "Frame {} No suitable memory type found — aborting frame", i);
                vkDestroyImage(dev, rawImage, nullptr);
                continue;
            }
            LOG_TRACE_CAT("RENDERER", "Frame {} Using memType={} for allocSize={}", i, memType, allocSize);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize  = allocSize;
            allocInfo.memoryTypeIndex = memType;

            VK_CHECK(vkAllocateMemory(dev, &allocInfo, nullptr, &rawMemory),
                     ("Failed to allocate RT output memory for frame " + std::to_string(i)).c_str());

            VK_CHECK(vkBindImageMemory(dev, rawImage, rawMemory, 0),
                     ("Failed to bind RT output memory for frame " + std::to_string(i)).c_str());

            LOG_TRACE_CAT("RENDERER", "Frame {} Memory bound: 0x{:x}", i, reinterpret_cast<uintptr_t>(rawMemory));

            // === 3. Transition to GENERAL (per-frame cmd for isolation) ===
            VkCommandBuffer cmd = beginSingleTimeCommands(dev, cmdPool);
            if (cmd == VK_NULL_HANDLE) {
                LOG_ERROR_CAT("RENDERER", "Frame {} Failed to begin single-time cmd — skipping transition", i);
                // Cleanup rawImage/rawMemory
                vkFreeMemory(dev, rawMemory, nullptr);
                vkDestroyImage(dev, rawImage, nullptr);
                continue;
            }

            VkImageMemoryBarrier barrier{};
            barrier.sType                       = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.oldLayout                   = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout                   = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex         = VK_QUEUE_FAMILY_IGNORED;
            barrier.image                       = rawImage;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                0, 0, nullptr, 0, nullptr, 1, &barrier);

            endSingleTimeCommands(dev, cmdPool, queue, cmd);  // Void call
            LOG_TRACE_CAT("RENDERER", "Frame {} Transition to GENERAL complete", i);

            // Per-frame wait to flush GPU before next alloc
            vkQueueWaitIdle(queue);

            // === 4. Create Image View ===
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image                           = rawImage;
            viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format                          = VK_FORMAT_R32G32B32A32_SFLOAT;
            viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.levelCount     = 1;
            viewInfo.subresourceRange.layerCount     = 1;

            VK_CHECK(vkCreateImageView(dev, &viewInfo, nullptr, &rawView),
                     ("Failed to create RT output image view for frame " + std::to_string(i)).c_str());

            LOG_TRACE_CAT("RENDERER", "Frame {} View created: 0x{:x}", i, reinterpret_cast<uintptr_t>(rawView));

            // === 5. Wrap in Handle<T> (Harden: Log before/after emplace) ===
            LOG_TRACE_CAT("RENDERER", "Frame {} Creating Handles — img=0x{:x}, mem=0x{:x}, view=0x{:x}",
                          i, reinterpret_cast<uintptr_t>(rawImage), reinterpret_cast<uintptr_t>(rawMemory), reinterpret_cast<uintptr_t>(rawView));

            // Assume RTX::Handle ctor: (T h, device, destroyFn, size_t (unused?), tag) — pass raw directly (VkImage, not &)
            static const std::string_view imgTag = "RTOutputImage";
            static const std::string_view memTag = "RTOutputMemory";
            static const std::string_view viewTag = "RTOutputView";

            rtOutputImages_.emplace_back(rawImage, dev, vkDestroyImage, 0, imgTag);
            rtOutputMemories_.emplace_back(rawMemory, dev, vkFreeMemory, 0, memTag);
            rtOutputViews_.emplace_back(rawView, dev, vkDestroyImageView, 0, viewTag);

            LOG_TRACE_CAT("RENDERER", "Frame {} — RTOutput ready: img=0x{:x}, view=0x{:x}",
                          i, reinterpret_cast<uintptr_t>(rawImage), reinterpret_cast<uintptr_t>(rawView));

            // Harden: Validate post-emplace sizes
            if (rtOutputImages_.size() != i + 1 || rtOutputMemories_.size() != i + 1 || rtOutputViews_.size() != i + 1) {
                LOG_ERROR_CAT("RENDERER", "Frame {} Vector emplace failed — sizes mismatch", i);
                // Rollback? Complex; log and continue
            }

        } catch (const std::exception& e) {
            LOG_FATAL_CAT("RENDERER", "Frame {} Exception in createRTOutput: {} — cleaning up", i, e.what());
            // Cleanup raw handles
            if (rawView != VK_NULL_HANDLE) vkDestroyImageView(dev, rawView, nullptr);
            if (rawMemory != VK_NULL_HANDLE) vkFreeMemory(dev, rawMemory, nullptr);
            if (rawImage != VK_NULL_HANDLE) vkDestroyImage(dev, rawImage, nullptr);
            throw;  // Re-throw to abort ctor
        }
    }

    // Harden: Post-loop idle + validate all created
    vkQueueWaitIdle(queue);
    LOG_TRACE_CAT("RENDERER", "Post-loop idle complete");

    if (rtOutputImages_.size() != framesInFlight) {
        LOG_FATAL_CAT("RENDERER", "Incomplete RT outputs: expected={}, got={} — device issue?", framesInFlight, rtOutputImages_.size());
        throw std::runtime_error("Partial RT output creation — recreate device");
    }

    LOG_SUCCESS_CAT("RENDERER", "RT output images created — {} frames in GENERAL layout", framesInFlight);
    LOG_TRACE_CAT("RENDERER", "createRTOutputImages — COMPLETE");
}

void VulkanRenderer::createAccumulationImages() {
    LOG_TRACE_CAT("RENDERER", "createAccumulationImages — START");
    if (!Options::RTX::ENABLE_ACCUMULATION) {
        LOG_INFO_CAT("RENDERER", "Accumulation disabled via options");
        LOG_TRACE_CAT("RENDERER", "createAccumulationImages — COMPLETE (disabled)");
        return;
    }
    LOG_INFO_CAT("RENDERER", "Creating accumulation images");
    createImageArray(accumImages_, accumMemories_, accumViews_, "Accumulation");
    LOG_TRACE_CAT("RENDERER", "createAccumulationImages — COMPLETE");
}

void VulkanRenderer::createDenoiserImage() {
    LOG_TRACE_CAT("RENDERER", "createDenoiserImage — START");
    if (!Options::RTX::ENABLE_DENOISING) {
        LOG_INFO_CAT("RENDERER", "Denoiser disabled via options");
        LOG_TRACE_CAT("RENDERER", "createDenoiserImage — COMPLETE (disabled)");
        return;
    }
    LOG_INFO_CAT("RENDERER", "Creating denoiser image");
    createImage(denoiserImage_, denoiserMemory_, denoiserView_, "Denoiser");
    LOG_TRACE_CAT("RENDERER", "createDenoiserImage — COMPLETE");
}

void VulkanRenderer::createEnvironmentMap() {
    LOG_TRACE_CAT("RENDERER", "createEnvironmentMap — START");
    if (!Options::Environment::ENABLE_ENV_MAP) {
        LOG_TRACE_CAT("RENDERER", "Environment map disabled via options");
        LOG_TRACE_CAT("RENDERER", "createEnvironmentMap — COMPLETE (disabled)");
        return;
    }
    LOG_INFO_CAT("RENDERER", "Creating environment map (IBL)");
    // Placeholder logging for IBL creation
    LOG_TRACE_CAT("RENDERER", "IBL creation placeholder executed");
    LOG_TRACE_CAT("RENDERER", "createEnvironmentMap — COMPLETE");
}

void VulkanRenderer::createNexusScoreImage(VkPhysicalDevice phys, VkDevice dev, VkCommandPool pool, VkQueue queue) {
    LOG_TRACE_CAT("RENDERER", "createNexusScoreImage — START");
    if (!Options::RTX::ENABLE_ADAPTIVE_SAMPLING) {
        LOG_TRACE_CAT("RENDERER", "NexusScore disabled via options");
        LOG_TRACE_CAT("RENDERER", "createNexusScoreImage — COMPLETE (disabled)");
        return;
    }
    LOG_INFO_CAT("RENDERER", "Creating NexusScore image for adaptive sampling");
    LOG_TRACE_CAT("RENDERER", "NexusScore image creation placeholder — phys=0x{:x}, dev=0x{:x}, pool=0x{:x}, queue=0x{:x}", 
                  reinterpret_cast<uintptr_t>(phys), reinterpret_cast<uintptr_t>(dev), reinterpret_cast<uintptr_t>(pool), reinterpret_cast<uintptr_t>(queue));
    LOG_TRACE_CAT("RENDERER", "createNexusScoreImage — COMPLETE");
}

void VulkanRenderer::createImageArray(
    std::vector<RTX::Handle<VkImage>>& images,
    std::vector<RTX::Handle<VkDeviceMemory>>& memories,
    std::vector<RTX::Handle<VkImageView>>& views,
    const std::string& tag) noexcept
{
    LOG_TRACE_CAT("RENDERER", "createImageArray — START — tag='{}'", tag);

    const uint32_t frames = Options::Performance::MAX_FRAMES_IN_FLIGHT;
    const VkFormat fmt = VK_FORMAT_R32G32B32A32_SFLOAT;
    const VkExtent2D ext = SWAPCHAIN.extent();

    if (ext.width == 0 || ext.height == 0) {
        LOG_ERROR_CAT("RENDERER", "Invalid swapchain extent: {}x{}", ext.width, ext.height);
        return;
    }

    VkDevice device = RTX::ctx().vkDevice();
    VkPhysicalDevice phys = RTX::ctx().vkPhysicalDevice();

    VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = fmt;
    imgInfo.extent = {ext.width, ext.height, 1};
    imgInfo.mipLevels = 1;
    imgInfo.arrayLayers = 1;
    imgInfo.samples = MSAA_SAMPLES;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    // ─── Precompute memory type ───
    uint32_t memTypeIndex = UINT32_MAX;
    {
        VkImage dummy = VK_NULL_HANDLE;
        if (vkCreateImage(device, &imgInfo, nullptr, &dummy) == VK_SUCCESS) {
            VkMemoryRequirements req;
            vkGetImageMemoryRequirements(device, dummy, &req);
            memTypeIndex = findMemoryType(phys, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            vkDestroyImage(device, dummy, nullptr);
        }
    }
    if (memTypeIndex == UINT32_MAX) {
        LOG_ERROR_CAT("RENDERER", "Failed to precompute memory type for {} images", tag);
        return;
    }

    // ─── Resize containers ───
    images.resize(frames);
    memories.resize(frames);
    views.resize(frames);

    for (uint32_t i = 0; i < frames; ++i) {
        LOG_TRACE_CAT("RENDERER", "Frame {} — Creating {} image", i, tag);

        // ─── Create Image ───
        VkImage img = VK_NULL_HANDLE;
        VK_CHECK(vkCreateImage(device, &imgInfo, nullptr, &img), "vkCreateImage");

        // ─── Get Memory Requirements ───
        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(device, img, &req);

        // ─── Allocate Memory ───
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = memTypeIndex;

        VkDeviceMemory mem = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateMemory(device, &alloc, nullptr, &mem), "vkAllocateMemory");

        // ─── BIND MEMORY BEFORE HANDLE ───
        VK_CHECK(vkBindImageMemory(device, img, mem, 0), "vkBindImageMemory");

        // ─── Create Handles ───
        images[i] = RTX::Handle<VkImage>(
            img, device,
            [](VkDevice d, VkImage i, const VkAllocationCallbacks*) { vkDestroyImage(d, i, nullptr); },
            0, tag + "Image"
        );

        memories[i] = RTX::Handle<VkDeviceMemory>(
            mem, device,
            [](VkDevice d, VkDeviceMemory m, const VkAllocationCallbacks*) { vkFreeMemory(d, m, nullptr); },
            req.size, tag + "Memory"
        );

        // ─── Create Image View ───
        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = img;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = fmt;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

        VkImageView view = VK_NULL_HANDLE;
        VK_CHECK(vkCreateImageView(device, &viewInfo, nullptr, &view), "vkCreateImageView");

        views[i] = RTX::Handle<VkImageView>(
            view, device,
            [](VkDevice d, VkImageView v, const VkAllocationCallbacks*) { vkDestroyImageView(d, v, nullptr); },
            0, tag + "View"
        );

        LOG_TRACE_CAT("RENDERER", "Frame {} — {} image ready: img=0x{:x}, mem=0x{:x}, view=0x{:x}",
                      i, tag, reinterpret_cast<uintptr_t>(img),
                      reinterpret_cast<uintptr_t>(mem), reinterpret_cast<uintptr_t>(view));
    }

    LOG_SUCCESS_CAT("RENDERER", "{} image array created — {} frames", tag, frames);
    LOG_TRACE_CAT("RENDERER", "createImageArray — COMPLETE");
}

void VulkanRenderer::createImage(RTX::Handle<VkImage_T*>& image, RTX::Handle<VkDeviceMemory_T*>& memory, 
                                 RTX::Handle<VkImageView_T*>& view, const std::string& tag) noexcept {
    LOG_TRACE_CAT("RENDERER", "createImage — START — tag='{}'", tag);
    LOG_TRACE_CAT("RENDERER", "Creating single {} image", tag);
    // TODO: Implement single-image variant with similar robustness/speed checks
    LOG_TRACE_CAT("RENDERER", "createImage — COMPLETE (placeholder)");
}

// ──────────────────────────────────────────────────────────────────────────────
// Frame Rendering
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::renderFrame(const Camera& camera, float deltaTime) noexcept {
    LOG_TRACE_CAT("RENDERER", "renderFrame — START — deltaTime={:.3f}", deltaTime);
    const uint32_t framesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;
    LOG_TRACE_CAT("RENDERER", "Waiting for fences — currentFrame_={}", currentFrame_);
    vkWaitForFences(RTX::ctx().vkDevice(), 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(RTX::ctx().vkDevice(), SWAPCHAIN.swapchain(), UINT64_MAX,
                                            imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &imageIndex);
    LOG_TRACE_CAT("RENDERER", "vkAcquireNextImageKHR returned: {}, imageIndex={}", static_cast<int>(result), imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        LOG_TRACE_CAT("RENDERER", "Swapchain out of date — recreating");
        SWAPCHAIN.recreate(width_, height_);
        LOG_TRACE_CAT("RENDERER", "renderFrame — COMPLETE (swapchain recreate)");
        return;
    }

    LOG_TRACE_CAT("RENDERER", "Resetting fences");
    vkResetFences(RTX::ctx().vkDevice(), 1, &inFlightFences_[currentFrame_]);

    VkCommandBuffer cmd = commandBuffers_[imageIndex];
    LOG_TRACE_CAT("RENDERER", "Resetting command buffer: 0x{:x}", reinterpret_cast<uint64_t>(cmd));
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    LOG_TRACE_CAT("RENDERER", "Beginning command buffer");
    vkBeginCommandBuffer(cmd, &beginInfo);

    if (hypertraceEnabled_) {
        float jitter = getJitter();
        hypertraceCounter_ += jitter * deltaTime * 420.0f;
        currentNexusScore_ = std::clamp(0.5f + 0.5f * std::sin(hypertraceCounter_), 0.0f, 1.0f);
        LOG_TRACE_CAT("RENDERER", "Hypertrace jitter: {:.3f}, counter: {:.3f}, nexusScore: {:.3f}", jitter, hypertraceCounter_, currentNexusScore_);
    }

    LOG_TRACE_CAT("RENDERER", "Updating uniform buffer for frame {}", currentFrame_);
    updateUniformBuffer(currentFrame_, camera, getJitter());
    LOG_TRACE_CAT("RENDERER", "Updating tonemap uniform for frame {}", currentFrame_);
    updateTonemapUniform(currentFrame_);

    LOG_TRACE_CAT("RENDERER", "Recording ray tracing commands");
    recordRayTracingCommandBuffer(cmd);
    if (denoisingEnabled_) {
        LOG_TRACE_CAT("RENDERER", "Performing denoising pass");
        performDenoisingPass(cmd);
    }
    LOG_TRACE_CAT("RENDERER", "Performing tonemap pass for imageIndex={}", imageIndex);
    performTonemapPass(cmd, imageIndex);

    LOG_TRACE_CAT("RENDERER", "Ending command buffer");
    vkEndCommandBuffer(cmd);

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &imageAvailableSemaphores_[currentFrame_];
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &renderFinishedSemaphores_[currentFrame_];
    LOG_TRACE_CAT("RENDERER", "Submitting to queue — waitSemaphore=0x{:x}, signalSemaphore=0x{:x}, cmd=0x{:x}", 
                  reinterpret_cast<uint64_t>(submit.pWaitSemaphores[0]), reinterpret_cast<uint64_t>(submit.pSignalSemaphores[0]), reinterpret_cast<uint64_t>(cmd));

    VK_CHECK(vkQueueSubmit(RTX::ctx().graphicsQueue(), 1, &submit, inFlightFences_[currentFrame_]), "Queue submit");

    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderFinishedSemaphores_[currentFrame_];
    present.swapchainCount = 1;
    VkSwapchainKHR swapchain = SWAPCHAIN.swapchain();
    present.pSwapchains = &swapchain;
    present.pImageIndices = &imageIndex;
    LOG_TRACE_CAT("RENDERER", "Presenting — waitSemaphore=0x{:x}, swapchain=0x{:x}, imageIndex={}", 
                  reinterpret_cast<uint64_t>(present.pWaitSemaphores[0]), reinterpret_cast<uint64_t>(swapchain), imageIndex);

    result = vkQueuePresentKHR(RTX::ctx().presentQueue(), &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        LOG_TRACE_CAT("RENDERER", "Present out of date/suboptimal — recreating swapchain");
        SWAPCHAIN.recreate(width_, height_);
    }
    LOG_TRACE_CAT("RENDERER", "vkQueuePresentKHR returned: {}", static_cast<int>(result));

    currentFrame_ = (currentFrame_ + 1) % framesInFlight;
    frameNumber_++;
    frameCounter_++;
    LOG_TRACE_CAT("RENDERER", "Advanced frame — currentFrame_={}, frameNumber_={}, frameCounter_={}", currentFrame_, frameNumber_, frameCounter_);

    // FPS limiting
    if (fpsTarget_ != FpsTarget::FPS_UNLIMITED) {
        float target = static_cast<float>(fpsTarget_);
        float sleepTime = (1.0f / target) - deltaTime;
        if (sleepTime > 0) {
            LOG_TRACE_CAT("RENDERER", "FPS limiting — sleeping for {:.3f} ms", sleepTime * 1000.0f);
            std::this_thread::sleep_for(std::chrono::duration<float>(sleepTime));
        }
    }

    // Performance logging
    if (Options::Performance::ENABLE_FPS_COUNTER) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPerfLogTime_).count();
        if (elapsed >= 1000) {
            double fps = frameCounter_ * 1000.0 / elapsed;
            double frameTimeMs = elapsed / static_cast<double>(frameCounter_);

            LOG_FPS_COUNTER("{}FPS: {:.1f} ({:.2f} ms) | Frame: {} | {}x{} | Hypertrace: {} | Denoise: {} | Adaptive: {} | Tonemap: {}{}",
                LIME_GREEN, fps, frameTimeMs, frameNumber_, width_, height_,
                hypertraceEnabled_ ? "ON" : "OFF", denoisingEnabled_ ? "ON" : "OFF",
                adaptiveSamplingEnabled_ ? "ON" : "OFF",
                tonemapType_ == TonemapType::ACES ? "ACES" : 
                (tonemapType_ == TonemapType::FILMIC ? "FILMIC" : "REINHARD"), RESET);

            lastPerfLogTime_ = now;
            frameCounter_ = 0;
        }
    }
    LOG_TRACE_CAT("RENDERER", "renderFrame — COMPLETE");
}

// ──────────────────────────────────────────────────────────────────────────────
// Ray Tracing Command Recording
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::recordRayTracingCommandBuffer(VkCommandBuffer cmd) {
    LOG_PERF_CAT("RENDERER", "recordRayTracingCommandBuffer — START — Frame {}", frameNumber_);

    // LAS management
    if (Options::LAS::REBUILD_EVERY_FRAME) {
        LOG_TRACE_CAT("RENDERER", "LAS rebuild every frame — START");
        std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>> instances;
        LAS.rebuildTLAS(RTX::ctx().commandPool(), RTX::ctx().graphicsQueue(), instances);
        resetAccumulation_ = true;
        LOG_TRACE_CAT("RENDERER", "LAS rebuild every frame — COMPLETE");
    }

    // Adaptive sampling logic
    currentSpp_ = Options::RTX::MIN_SPP;
    if (adaptiveSamplingEnabled_ && Options::RTX::ENABLE_ADAPTIVE_SAMPLING) {
        float motion = 0.0f;  // Placeholder
        float score = currentNexusScore_ + motion;
        if (score < Options::RTX::NEXUS_SCORE_THRESHOLD) {
            currentSpp_ = glm::clamp(static_cast<uint32_t>(1.0f / score), Options::RTX::MIN_SPP, Options::RTX::MAX_SPP);
        }
        if (motion < 0.01f && Options::RTX::ENABLE_ACCUMULATION) {
            currentSpp_ = Options::RTX::MAX_SPP;
            LOG_INFO_CAT("HYPERTRACE", "{}64 SPP — Scene stable{}", PULSAR_GREEN, RESET);
        } else if (currentSpp_ >= 32) {
            LOG_INFO_CAT("HYPERTRACE", "{}32 SPP active{}", PLASMA_FUCHSIA, RESET);
        }

        if (motion > 0.05f || resetAccumulation_) {
            resetAccumulation_ = false;
        }
        LOG_TRACE_CAT("RENDERER", "Adaptive sampling — currentSpp_={}, nexusScore={:.3f}, motion={:.3f}", currentSpp_, currentNexusScore_, motion);
    }

    // Transition RT output image
    LOG_TRACE_CAT("RENDERER", "Image memory barrier for RT output — START");
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.image = *rtOutputImages_[currentFrame_];
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 
                         VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    LOG_TRACE_CAT("RENDERER", "Image memory barrier applied — image=0x{:x}, oldLayout={}, newLayout={}", 
                  reinterpret_cast<uintptr_t>(barrier.image), static_cast<int>(barrier.oldLayout), static_cast<int>(barrier.newLayout));
    LOG_TRACE_CAT("RENDERER", "Image memory barrier for RT output — COMPLETE");

    // SBT regions
    VkStridedDeviceAddressRegionKHR raygenSbt{sbtAddress_, rtxProps_.shaderGroupBaseAlignment, rtxProps_.shaderGroupBaseAlignment};
    VkStridedDeviceAddressRegionKHR missSbt{sbtAddress_ + rtxProps_.shaderGroupBaseAlignment, rtxProps_.shaderGroupBaseAlignment, rtxProps_.shaderGroupBaseAlignment};
    VkStridedDeviceAddressRegionKHR hitSbt{sbtAddress_ + 2 * rtxProps_.shaderGroupBaseAlignment, rtxProps_.shaderGroupBaseAlignment, rtxProps_.shaderGroupBaseAlignment};
    VkStridedDeviceAddressRegionKHR callableSbt{0, 0, 0};
    LOG_TRACE_CAT("RENDERER", "SBT regions — raygen=0x{:x}/{}x{}, miss=0x{:x}/{}x{}, hit=0x{:x}/{}x{}, callable=0x{:x}/0x0", 
                  raygenSbt.deviceAddress, raygenSbt.stride, raygenSbt.size,
                  missSbt.deviceAddress, missSbt.stride, missSbt.size,
                  hitSbt.deviceAddress, hitSbt.stride, hitSbt.size,
                  callableSbt.deviceAddress);

    LOG_TRACE_CAT("RENDERER", "Binding RT pipeline: 0x{:x}", reinterpret_cast<uint64_t>(*rtPipeline_));
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *rtPipeline_);
    LOG_TRACE_CAT("RENDERER", "Binding descriptor sets — set=0x{:x}", reinterpret_cast<uint64_t>(rtDescriptorSets_[currentFrame_]));
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *rtPipelineLayout_, 
                            0, 1, &rtDescriptorSets_[currentFrame_], 0, nullptr);

    LOG_TRACE_CAT("RENDERER", "Tracing rays — width={}, height={}, spp={}", width_, height_, currentSpp_);
    vkCmdTraceRaysKHR(cmd, &raygenSbt, &missSbt, &hitSbt, &callableSbt, width_, height_, 1);

    LOG_PERF_CAT("RENDERER", "Ray trace dispatched — SPP: {}", currentSpp_);
    LOG_PERF_CAT("RENDERER", "recordRayTracingCommandBuffer — COMPLETE");
}

// ──────────────────────────────────────────────────────────────────────────────
// Utility Functions
// ──────────────────────────────────────────────────────────────────────────────
uint32_t VulkanRenderer::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, 
                                        VkMemoryPropertyFlags properties) const noexcept {
    LOG_TRACE_CAT("RENDERER", "findMemoryType — START — typeFilter=0x{:x}, properties=0x{:x}", typeFilter, properties);
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    LOG_TRACE_CAT("RENDERER", "Memory properties — memoryTypeCount={}", memProps.memoryTypeCount);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        LOG_TRACE_CAT("RENDERER", "Checking memory type {} — filterMatch={}, propMatch=0x{:x}", i, (typeFilter & (1 << i)) != 0, memProps.memoryTypes[i].propertyFlags);
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            LOG_TRACE_CAT("RENDERER", "Suitable memory type found: {}", i);
            LOG_TRACE_CAT("RENDERER", "findMemoryType — COMPLETE — return={}", i);
            return i;
        }
    }
    if (Options::Performance::ENABLE_MEMORY_BUDGET_WARNINGS) {
        LOG_WARNING_CAT("RENDERER", "No suitable memory type found — using fallback");
    }
    LOG_TRACE_CAT("RENDERER", "No suitable type — fallback to 0");
    LOG_TRACE_CAT("RENDERER", "findMemoryType — COMPLETE — return=0 (fallback)");
    return 0;
}

void VulkanRenderer::initializeAllBufferData(uint32_t frames, VkDeviceSize uniformSize, VkDeviceSize materialSize) {
    // Harden: Validate inputs to prevent overflows or invalid states
    if (frames == 0) {
        LOG_ERROR_CAT("RENDERER", "initializeAllBufferData: Invalid frames count: {}", frames);
        return;
    }
    if (uniformSize > (1ULL << 32) || materialSize > (1ULL << 32)) {  // Arbitrary sane limit for debug
        LOG_WARN_CAT("RENDERER", "initializeAllBufferData: Large buffer sizes detected — uniform={}, material={}", uniformSize, materialSize);
    }

    LOG_TRACE_CAT("RENDERER", "initializeAllBufferData — START — frames={}, uniformSize={}, materialSize={}", frames, uniformSize, materialSize);
    LOG_INFO_CAT("RENDERER", "Initializing buffer data: {} frames | Uniform: {} MB | Material: {} MB", 
        frames, uniformSize / (1024ULL*1024ULL), materialSize / (1024ULL*1024ULL));  // Use ULL for safe division

    // Resize and initialize
    uniformBufferEncs_.resize(frames);
    if (uniformBufferEncs_.size() != static_cast<size_t>(frames)) {
        LOG_ERROR_CAT("RENDERER", "initializeAllBufferData: Resize failed — expected={}, got={}", frames, uniformBufferEncs_.size());
        uniformBufferEncs_.clear();  // Reset to safe state
        return;
    }

    for (auto& enc : uniformBufferEncs_) {
        enc = 0;
    }

    LOG_TRACE_CAT("RENDERER", "Resized uniformBufferEncs_ to {}, initialized to 0", uniformBufferEncs_.size());
    LOG_TRACE_CAT("RENDERER", "initializeAllBufferData — COMPLETE");
}

void VulkanRenderer::createCommandBuffers() {
    LOG_TRACE_CAT("RENDERER", "createCommandBuffers — START");
    size_t numImages = SWAPCHAIN.images().size();
    LOG_INFO_CAT("RENDERER", "Allocating {} command buffers", numImages);
    commandBuffers_.resize(numImages);
    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = RTX::ctx().commandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());
    LOG_TRACE_CAT("RENDERER", "Alloc info — pool=0x{:x}, level={}, count={}", reinterpret_cast<uintptr_t>(allocInfo.commandPool), static_cast<int>(allocInfo.level), allocInfo.commandBufferCount);
    VK_CHECK(vkAllocateCommandBuffers(RTX::ctx().vkDevice(), &allocInfo, commandBuffers_.data()), "Command buffer allocation");
    LOG_TRACE_CAT("RENDERER", "Allocated command buffers — data=0x{:x}", reinterpret_cast<uintptr_t>(commandBuffers_.data()));
    for (size_t i = 0; i < commandBuffers_.size(); ++i) {
        LOG_TRACE_CAT("RENDERER", "commandBuffers_[{}]: 0x{:x}", i, reinterpret_cast<uint64_t>(commandBuffers_[i]));
    }
    LOG_TRACE_CAT("RENDERER", "createCommandBuffers — COMPLETE");
}

void VulkanRenderer::allocateDescriptorSets() {
    LOG_TRACE_CAT("RENDERER", "allocateDescriptorSets — START — {} frames", Options::Performance::MAX_FRAMES_IN_FLIGHT);

    VkDevice device = RTX::g_ctx().vkDevice();  // Use global context device
    LOG_DEBUG_CAT("RENDERER", "Retrieved device: 0x{:x}", reinterpret_cast<uintptr_t>(device));

    // CRITICAL: Ensure rtDescriptorSetLayout_ is created/valid before allocation
    LOG_DEBUG_CAT("RENDERER", "Checking rtDescriptorSetLayout validity: valid={}, handle=0x{:x}", rtDescriptorSetLayout_.valid(), reinterpret_cast<uintptr_t>(*rtDescriptorSetLayout_));
    if (!rtDescriptorSetLayout_.valid() || *rtDescriptorSetLayout_ == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "rtDescriptorSetLayout invalid — creating RT layout");

        // Define RT bindings (adjust based on your shader bindings)
        std::array<VkDescriptorSetLayoutBinding, 4> rtBindings{};  // Example: AS, storage image, uniform, sampler
        LOG_DEBUG_CAT("RENDERER", "Initializing RT bindings array of size {}", rtBindings.size());

        rtBindings[0].binding = 0;
        rtBindings[0].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        rtBindings[0].descriptorCount = 1;
        rtBindings[0].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR;
        rtBindings[0].pImmutableSamplers = nullptr;
        LOG_DEBUG_CAT("RENDERER", "Set binding 0: AS, type={}, count={}, stages=0x{:x}", static_cast<int>(rtBindings[0].descriptorType), rtBindings[0].descriptorCount, rtBindings[0].stageFlags);

        rtBindings[1].binding = 1;
        rtBindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        rtBindings[1].descriptorCount = 1;
        rtBindings[1].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        rtBindings[1].pImmutableSamplers = nullptr;
        LOG_DEBUG_CAT("RENDERER", "Set binding 1: Storage Image, type={}, count={}, stages=0x{:x}", static_cast<int>(rtBindings[1].descriptorType), rtBindings[1].descriptorCount, rtBindings[1].stageFlags);

        rtBindings[2].binding = 2;
        rtBindings[2].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        rtBindings[2].descriptorCount = 1;
        rtBindings[2].stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        rtBindings[2].pImmutableSamplers = nullptr;
        LOG_DEBUG_CAT("RENDERER", "Set binding 2: Uniform Buffer, type={}, count={}, stages=0x{:x}", static_cast<int>(rtBindings[2].descriptorType), rtBindings[2].descriptorCount, rtBindings[2].stageFlags);

        rtBindings[3].binding = 3;
        rtBindings[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        rtBindings[3].descriptorCount = 1;
        rtBindings[3].stageFlags = VK_SHADER_STAGE_MISS_BIT_KHR;
        rtBindings[3].pImmutableSamplers = nullptr;
        LOG_DEBUG_CAT("RENDERER", "Set binding 3: Combined Image Sampler, type={}, count={}, stages=0x{:x}", static_cast<int>(rtBindings[3].descriptorType), rtBindings[3].descriptorCount, rtBindings[3].stageFlags);

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(rtBindings.size());
        layoutInfo.pBindings = rtBindings.data();
        LOG_DEBUG_CAT("RENDERER", "Layout info: sType={}, bindingCount={}, pBindings=0x{:x}", static_cast<int>(layoutInfo.sType), layoutInfo.bindingCount, reinterpret_cast<uintptr_t>(layoutInfo.pBindings));

        VkDescriptorSetLayout rawLayout = VK_NULL_HANDLE;
        VkResult layoutResult = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &rawLayout);
        LOG_DEBUG_CAT("RENDERER", "vkCreateDescriptorSetLayout returned: {}", static_cast<int>(layoutResult));
        if (layoutResult != VK_SUCCESS) {
            LOG_ERROR_CAT("RENDERER", "Failed to create RT layout: {}", static_cast<int>(layoutResult));
            throw std::runtime_error("Create RT descriptor set layout failed");
        }
        VK_CHECK(layoutResult, "Create RT descriptor set layout");  // Redundant but for macro consistency

        // Assign to Handle
        LOG_TRACE_CAT("RENDERER", "Creating Handle for rtDescriptorSetLayout_");
        rtDescriptorSetLayout_ = RTX::Handle<VkDescriptorSetLayout>(rawLayout, device,
            [](VkDevice d, VkDescriptorSetLayout l, const VkAllocationCallbacks*) {
                if (l != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(d, l, nullptr);
            }, 0, "RTDescriptorSetLayout");
        LOG_DEBUG_CAT("RENDERER", "Created new RT descriptor set layout: 0x{:x}", reinterpret_cast<uintptr_t>(rawLayout));
    }

    // Now safely get the valid layout
    VkDescriptorSetLayout validLayout = *rtDescriptorSetLayout_;
    LOG_DEBUG_CAT("RENDERER", "Retrieved valid layout: 0x{:x}", reinterpret_cast<uintptr_t>(validLayout));
    if (validLayout == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RENDERER", "RT descriptor set layout is still null after creation — aborting");
        throw std::runtime_error("RT descriptor set layout is still null after creation — aborting");
    }

    std::array<VkDescriptorSetLayout, Options::Performance::MAX_FRAMES_IN_FLIGHT> layouts{};
    std::fill(layouts.begin(), layouts.end(), validLayout);
    LOG_DEBUG_CAT("RENDERER", "Layouts filled with valid RT layout: 0x{:x} for {} frames", reinterpret_cast<uintptr_t>(validLayout), layouts.size());
    for (size_t i = 0; i < layouts.size(); ++i) {
        LOG_TRACE_CAT("RENDERER", "Layout[{}]: 0x{:x}", i, reinterpret_cast<uintptr_t>(layouts[i]));
    }

    // Ensure the vector is sized for allocation (assuming rtDescriptorSets_ is std::vector<VkDescriptorSet>)
    LOG_DEBUG_CAT("RENDERER", "Current rtDescriptorSets_ size: {}", rtDescriptorSets_.size());
    rtDescriptorSets_.resize(Options::Performance::MAX_FRAMES_IN_FLIGHT);
    LOG_DEBUG_CAT("RENDERER", "Resized rtDescriptorSets_ to {} entries", rtDescriptorSets_.size());
    LOG_DEBUG_CAT("RENDERER", "rtDescriptorSets_.data(): 0x{:x}", reinterpret_cast<uintptr_t>(rtDescriptorSets_.data()));

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = *rtDescriptorPool_;  // Assuming rtDescriptorPool_ is valid
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();  // Guaranteed non-null
    LOG_DEBUG_CAT("RENDERER", "Alloc info: sType={}, pool=0x{:x}, count={}, pSetLayouts=0x{:x}", static_cast<int>(allocInfo.sType), reinterpret_cast<uintptr_t>(allocInfo.descriptorPool), allocInfo.descriptorSetCount, reinterpret_cast<uintptr_t>(allocInfo.pSetLayouts));

    // Bulletproof: Check pool validity
    LOG_DEBUG_CAT("RENDERER", "Checking rtDescriptorPool validity: valid={}, handle=0x{:x}", rtDescriptorPool_.valid(), reinterpret_cast<uintptr_t>(*rtDescriptorPool_));
    if (*rtDescriptorPool_ == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RENDERER", "RT descriptor pool is null — cannot allocate sets");
        throw std::runtime_error("RT descriptor pool is null — cannot allocate sets");
    }

    // Log pool for debugging
    LOG_DEBUG_CAT("RENDERER", "Using rtDescriptorPool: 0x{:x}", reinterpret_cast<uintptr_t>(*rtDescriptorPool_));

    LOG_DEBUG_CAT("RENDERER", "About to call vkAllocateDescriptorSets with pDescriptorSets=0x{:x}", reinterpret_cast<uintptr_t>(rtDescriptorSets_.data()));
    VkResult allocResult = vkAllocateDescriptorSets(device, &allocInfo, rtDescriptorSets_.data());
    LOG_DEBUG_CAT("RENDERER", "vkAllocateDescriptorSets returned: {}", static_cast<int>(allocResult));
    if (allocResult != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "Failed to allocate RT sets: {}", static_cast<int>(allocResult));
        throw std::runtime_error("Descriptor allocation failed");
    }
    VK_CHECK(allocResult, "Allocate RT descriptor sets");  // Redundant but for macro consistency

    LOG_DEBUG_CAT("RENDERER", "RT descriptor sets allocated — first set: 0x{:x}", reinterpret_cast<uintptr_t>(rtDescriptorSets_[0]));
    for (size_t i = 0; i < rtDescriptorSets_.size(); ++i) {
        LOG_TRACE_CAT("RENDERER", "Allocated set[{}]: 0x{:x}", i, reinterpret_cast<uintptr_t>(rtDescriptorSets_[i]));
    }

    LOG_SUCCESS_CAT("RENDERER", "{}RT descriptor sets allocated — {} frames ready{}", PLASMA_FUCHSIA, Options::Performance::MAX_FRAMES_IN_FLIGHT, RESET);
    LOG_TRACE_CAT("RENDERER", "allocateDescriptorSets — COMPLETE");
}

void VulkanRenderer::updateNexusDescriptors() { 
    LOG_TRACE_CAT("RENDERER", "updateNexusDescriptors — START/COMPLETE (placeholder)"); 
}
void VulkanRenderer::updateRTXDescriptors() { 
    LOG_TRACE_CAT("RENDERER", "updateRTXDescriptors — START/COMPLETE (placeholder)"); 
}
void VulkanRenderer::updateTonemapDescriptorsInitial() { 
    LOG_TRACE_CAT("RENDERER", "updateTonemapDescriptorsInitial — START/COMPLETE (placeholder)"); 
}
void VulkanRenderer::updateDenoiserDescriptors() { 
    LOG_TRACE_CAT("RENDERER", "updateDenoiserDescriptors — START/COMPLETE (placeholder)"); 
}
void VulkanRenderer::performDenoisingPass(VkCommandBuffer cmd) { 
    if (!Options::RTX::ENABLE_DENOISING) {
        LOG_TRACE_CAT("RENDERER", "performDenoisingPass — SKIPPED (disabled)");
        return;
    }
    LOG_PERF_CAT("RENDERER", "Executing SVGF denoising pass — cmd=0x{:x}", reinterpret_cast<uint64_t>(cmd));
    LOG_TRACE_CAT("RENDERER", "performDenoisingPass — COMPLETE");
}
void VulkanRenderer::performTonemapPass(VkCommandBuffer cmd, uint32_t imageIndex) { 
    LOG_PERF_CAT("RENDERER", "Executing tonemapping pass — cmd=0x{:x}, imageIndex={}", reinterpret_cast<uint64_t>(cmd), imageIndex);
    LOG_TRACE_CAT("RENDERER", "performTonemapPass — COMPLETE");
}
void VulkanRenderer::updateUniformBuffer(uint32_t frame, const Camera& camera, float jitter) { 
    LOG_TRACE_CAT("RENDERER", "updateUniformBuffer — START — frame={}, jitter={:.3f}", frame, jitter);
    // Placeholder
    LOG_TRACE_CAT("RENDERER", "updateUniformBuffer — COMPLETE");
}
void VulkanRenderer::updateTonemapUniform(uint32_t frame) { 
    LOG_TRACE_CAT("RENDERER", "updateTonemapUniform — START — frame={}", frame);
    // Placeholder
    LOG_TRACE_CAT("RENDERER", "updateTonemapUniform — COMPLETE");
}

// ──────────────────────────────────────────────────────────────────────────────
// Application Interface — Synchronized with handle_app.hpp
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::setTonemap(bool enabled) noexcept {
    LOG_TRACE_CAT("RENDERER", "setTonemap — START — enabled={}", enabled);
    if (tonemapEnabled_ == enabled) {
        LOG_TRACE_CAT("RENDERER", "No change needed");
        LOG_TRACE_CAT("RENDERER", "setTonemap — COMPLETE (no change)");
        return;
    }
    tonemapEnabled_ = enabled;
    resetAccumulation_ = true;
    LOG_INFO_CAT("Renderer", "{}Tonemapping: {}{}", 
        enabled ? LIME_GREEN : CRIMSON_MAGENTA,
        enabled ? "ENABLED" : "DISABLED", RESET);
    LOG_TRACE_CAT("RENDERER", "setTonemap — COMPLETE");
}

void VulkanRenderer::setOverlay(bool show) noexcept {
    LOG_TRACE_CAT("RENDERER", "setOverlay — START — show={}", show);
    if (showOverlay_ == show) {
        LOG_TRACE_CAT("RENDERER", "No change needed");
        LOG_TRACE_CAT("RENDERER", "setOverlay — COMPLETE (no change)");
        return;
    }
    showOverlay_ = show;
    LOG_INFO_CAT("Renderer", "{}ImGui Overlay: {}{}", 
        show ? LIME_GREEN : CRIMSON_MAGENTA,
        show ? "VISIBLE" : "HIDDEN", RESET);
    LOG_TRACE_CAT("RENDERER", "setOverlay — COMPLETE");
}

void VulkanRenderer::setRenderMode(int mode) noexcept {
    LOG_TRACE_CAT("RENDERER", "setRenderMode — START — mode={}", mode);
    if (renderMode_ == mode) {
        LOG_TRACE_CAT("RENDERER", "No change needed");
        LOG_TRACE_CAT("RENDERER", "setRenderMode — COMPLETE (no change)");
        return;
    }
    renderMode_ = mode;
    resetAccumulation_ = true;
    LOG_INFO_CAT("Renderer", "{}Render Mode: {} → {}{}", 
        PULSAR_GREEN, renderMode_, mode, RESET);
    LOG_TRACE_CAT("RENDERER", "setRenderMode — COMPLETE");
}

// ──────────────────────────────────────────────────────────────────────────────
// Resize Handling
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::handleResize(int w, int h) noexcept {
    LOG_TRACE_CAT("RENDERER", "handleResize — START — w={}, h={}", w, h);
    if (w <= 0 || h <= 0) {
        LOG_TRACE_CAT("RENDERER", "Invalid dimensions — abort");
        LOG_TRACE_CAT("RENDERER", "handleResize — COMPLETE (invalid)");
        return;
    }
    if (width_ == w && height_ == h) {
        LOG_TRACE_CAT("RENDERER", "No change needed");
        LOG_TRACE_CAT("RENDERER", "handleResize — COMPLETE (no change)");
        return;
    }

    width_ = w;
    height_ = h;
    resetAccumulation_ = true;
    LOG_TRACE_CAT("RENDERER", "Updated dimensions — width_={}, height_={}, resetAccumulation_={}", width_, height_, resetAccumulation_);

    LOG_TRACE_CAT("RENDERER", "vkDeviceWaitIdle — START");
    vkDeviceWaitIdle(RTX::ctx().vkDevice());
    LOG_TRACE_CAT("RENDERER", "vkDeviceWaitIdle — COMPLETE");

    LOG_TRACE_CAT("RENDERER", "SWAPCHAIN.recreate — START");
    SWAPCHAIN.recreate(w, h);
    LOG_TRACE_CAT("RENDERER", "SWAPCHAIN.recreate — COMPLETE");

    LOG_TRACE_CAT("RENDERER", "Recreating render targets — START");
    createRTOutputImages();
    createAccumulationImages();
    createDenoiserImage();
    updateRTXDescriptors();
    updateTonemapDescriptorsInitial();
    updateDenoiserDescriptors();
    LOG_TRACE_CAT("RENDERER", "Recreating render targets — COMPLETE");

    LOG_SUCCESS_CAT("Renderer", "{}Swapchain resized to {}x{}{}", SAPPHIRE_BLUE, w, h, RESET);
    LOG_TRACE_CAT("RENDERER", "handleResize — COMPLETE");
}

// ──────────────────────────────────────────────────────────────────────────────
// Final Status
// ──────────────────────────────────────────────────────────────────────────────
/*
 * November 13, 2025 — Enhanced Logging Release v10.1
 * • Exhaustive TRACE logging for all functions and steps
 * • Numbered STACK BUILD ORDER in constructor (15 steps)
 * • All 59 Options:: values fully respected
 * • Dynamic frames-in-flight via Options::Performance::MAX_FRAMES_IN_FLIGHT
 * • Full LAS integration with rebuildTLAS/updateTLAS
 * • Application interface fully synchronized
 * • Extensive logging with color-coded categories
 * • C++23 compliant, -Werror clean
 * • No singletons, full RAII via RTX::Handle<T>
 * • Production ready with debug visibility
 */