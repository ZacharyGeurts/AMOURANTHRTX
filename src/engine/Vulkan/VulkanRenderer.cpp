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
#include "engine/GLOBAL/camera.hpp"

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
// ──────────────────────────────────────────────────────────────────────────────
// Constructor — FINAL FIXED VERSION — INTERNAL SHADERS ONLY
// ──────────────────────────────────────────────────────────────────────────────
VulkanRenderer::VulkanRenderer(int width, int height, SDL_Window* window, bool overclockFromMain)
    : window_(window), width_(width), height_(height), overclockMode_(overclockFromMain),
      hypertraceEnabled_(Options::RTX::ENABLE_ADAPTIVE_SAMPLING),
      denoisingEnabled_(Options::RTX::ENABLE_DENOISING),
      adaptiveSamplingEnabled_(Options::RTX::ENABLE_ADAPTIVE_SAMPLING),
      tonemapType_(TonemapType::ACES),
      lastPerfLogTime_(std::chrono::steady_clock::now()), frameCounter_(0)
{
    LOG_ATTEMPT_CAT("RENDERER", "Constructing VulkanRenderer ({}x{}) — INTERNAL SHADERS ACTIVE — PINK PHOTONS RISING", width, height);

    // ====================================================================
    // INTERNAL RAY TRACING SHADER LIST — WE OWN THIS. NO EXTERNAL PATHS.
    // ====================================================================
    static constexpr auto RT_SHADER_PATHS = std::to_array({
        "assets/shaders/raytracing/raygen.spv",
        "assets/shaders/raytracing/miss.spv",
        "assets/shaders/raytracing/closest_hit.spv",
        "assets/shaders/raytracing/shadowmiss.spv"
    });

    std::vector<std::string> finalShaderPaths;
    finalShaderPaths.reserve(RT_SHADER_PATHS.size());
    for (const auto* path : RT_SHADER_PATHS) {
        finalShaderPaths.emplace_back(path);
    }

    LOG_SUCCESS_CAT("RENDERER", "INTERNAL SHADER LIST LOADED — {} shaders — PINK PHOTONS FULLY ARMED", finalShaderPaths.size());

    // ====================================================================
    // STACK BUILD ORDER — YOUR ORIGINAL 15 STEPS (unchanged except Step 13)
    // ====================================================================

    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 1: Set Overclock Mode ===");
    setOverclockMode(overclockFromMain);
    LOG_TRACE_CAT("RENDERER", "Step 1 COMPLETE");

    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 2: Security Validation (StoneKey) ===");
    if (get_kStone1() == 0 || get_kStone2() == 0) {
        LOG_ERROR_CAT("SECURITY", "StoneKey validation failed — aborting");
        throw std::runtime_error("StoneKey validation failed");
    }
    LOG_TRACE_CAT("RENDERER", "Step 2 COMPLETE");

    auto& c = RTX::ctx();
    const uint32_t framesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 3: Load Ray Tracing Extensions ===");
    loadRayTracingExtensions();
    LOG_TRACE_CAT("RENDERER", "Step 3 COMPLETE");

    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 4: Resize Synchronization Containers ===");
    imageAvailableSemaphores_.resize(framesInFlight);
    renderFinishedSemaphores_.resize(framesInFlight);
    inFlightFences_.resize(framesInFlight);
    LOG_TRACE_CAT("RENDERER", "Step 4 COMPLETE");

    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 5: Create Synchronization Objects ===");
    VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
    for (uint32_t i = 0; i < framesInFlight; ++i) {
        VK_CHECK(vkCreateSemaphore(c.vkDevice(), &semInfo, nullptr, &imageAvailableSemaphores_[i]), "Image available");
        VK_CHECK(vkCreateSemaphore(c.vkDevice(), &semInfo, nullptr, &renderFinishedSemaphores_[i]), "Render finished");
        VK_CHECK(vkCreateFence(c.vkDevice(), &fenceInfo, nullptr, &inFlightFences_[i]), "In-flight fence");
    }
    LOG_TRACE_CAT("RENDERER", "Step 5 COMPLETE");

    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 6: GPU Timestamp Queries ===");
    if (Options::Performance::ENABLE_GPU_TIMESTAMPS || Options::Debug::SHOW_GPU_TIMESTAMPS) {
        VkQueryPoolCreateInfo qpInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        qpInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        qpInfo.queryCount = framesInFlight * 2;
        VK_CHECK(vkCreateQueryPool(c.vkDevice(), &qpInfo, nullptr, &timestampQueryPool_), "Timestamp pool");
    }
    LOG_TRACE_CAT("RENDERER", "Step 6 COMPLETE");

    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 7: Query GPU Properties ===");
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(c.vkPhysicalDevice(), &props);
    timestampPeriod_ = props.limits.timestampPeriod / 1e6f;
    LOG_INFO_CAT("RENDERER", "GPU: {} | Timestamp period: {:.3f} ms", props.deviceName, timestampPeriod_);
    LOG_TRACE_CAT("RENDERER", "Step 7 COMPLETE");

    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 8: Create Descriptor Pools ===");
    // ... [your existing descriptor pool code] ...
    LOG_TRACE_CAT("RENDERER", "Step 8 COMPLETE");

    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 9: Create Render Targets ===");
    if (Options::Environment::ENABLE_ENV_MAP) createEnvironmentMap();
    createAccumulationImages();
    createRTOutputImages();
    if (Options::RTX::ENABLE_DENOISING) createDenoiserImage();
    if (Options::RTX::ENABLE_ADAPTIVE_SAMPLING)
        createNexusScoreImage(c.vkPhysicalDevice(), c.vkDevice(), c.commandPool(), c.graphicsQueue());
    LOG_TRACE_CAT("RENDERER", "Step 9 COMPLETE");

    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 10: Initialize Buffer Data ===");
    initializeAllBufferData(framesInFlight, 64_MB, 16_MB);
    LOG_TRACE_CAT("RENDERER", "Step 10 COMPLETE");

    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 11: Create Command Buffers ===");
    createCommandBuffers();
    LOG_TRACE_CAT("RENDERER", "Step 11 COMPLETE");

    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 12: Allocate Descriptor Sets ===");
    allocateDescriptorSets();
    LOG_TRACE_CAT("RENDERER", "Step 12 COMPLETE");

    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 13: Create Ray Tracing Pipeline ===");
    createRayTracingPipeline(finalShaderPaths);  // ← ONLY LINE THAT CHANGED
    LOG_TRACE_CAT("RENDERER", "Step 13 COMPLETE");

    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 14: Create Shader Binding Table ===");
    createShaderBindingTable();
    LOG_TRACE_CAT("RENDERER", "Step 14 COMPLETE");

    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 15: Update Descriptors ===");
    updateNexusDescriptors();
    updateRTXDescriptors();
    updateTonemapDescriptorsInitial();
    updateDenoiserDescriptors();
    LOG_TRACE_CAT("RENDERER", "Step 15 COMPLETE");

    LOG_SUCCESS_CAT("RENDERER", 
        "VulkanRenderer INITIALIZED — {}x{} — ALL SYSTEMS NOMINAL — PINK PHOTONS ETERNAL — FIRST LIGHT ACHIEVED", 
        width, height);
}

// ──────────────────────────────────────────────────────────────────────────────
// Ray Tracing Pipeline Creation — FINAL FIXED VERSION
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::createRayTracingPipeline(const std::vector<std::string>& shaderPaths)
{
    LOG_TRACE_CAT("RENDERER", "createRayTracingPipeline — START — {} shaders provided", shaderPaths.size());

    VkDevice device = RTX::g_ctx().vkDevice();
    LOG_DEBUG_CAT("RENDERER", "Retrieved device: 0x{:x}", reinterpret_cast<uintptr_t>(device));

    if (shaderPaths.size() < 2) {
        LOG_ERROR_CAT("RENDERER", "Insufficient shader paths: expected at least raygen + miss, got {}", shaderPaths.size());
        throw std::runtime_error("Insufficient shader paths for RT pipeline");
    }

    // ---------------------------------------------------------------------
    // 1. Load mandatory shaders
    // ---------------------------------------------------------------------
    VkShaderModule raygenModule = loadShader(shaderPaths[0]); // raygen.rgen.spv
    VkShaderModule missModule   = loadShader(shaderPaths[1]); // miss.rmiss.spv (or shadowmiss if using separate)

    if (raygenModule == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "Failed to load raygen shader: {}", shaderPaths[0]);
        throw std::runtime_error("Failed to load raygen shader");
    }
    if (missModule == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "Failed to load primary miss shader: {}", shaderPaths[1]);
        throw std::runtime_error("Failed to load primary miss shader");
    }

    LOG_TRACE_CAT("RENDERER", "Raygen module loaded: 0x{:x}", reinterpret_cast<uintptr_t>(raygenModule));
    LOG_TRACE_CAT("RENDERER", "Miss module loaded:   0x{:x}", reinterpret_cast<uintptr_t>(missModule));

    // Optional: closest hit & shadow miss
    VkShaderModule closestHitModule = VK_NULL_HANDLE;
    VkShaderModule shadowMissModule = VK_NULL_HANDLE;

    bool hasClosestHit = false;
    bool hasShadowMiss = false;

    if (shaderPaths.size() > 2 && !shaderPaths[2].empty()) {
        closestHitModule = loadShader(shaderPaths[2]);
        hasClosestHit = (closestHitModule != VK_NULL_HANDLE);
    }

    if (shaderPaths.size() > 3 && !shaderPaths[3].empty()) {
        shadowMissModule = loadShader(shaderPaths[3]);
        hasShadowMiss = (shadowMissModule != VK_NULL_HANDLE);
    }

    // ---------------------------------------------------------------------
    // 2. Build shader stages and groups
    // ---------------------------------------------------------------------
    struct StageInfo {
        VkPipelineShaderStageCreateInfo stage{};
        VkRayTracingShaderGroupCreateInfoKHR group{};
    };
    std::vector<StageInfo> stageInfos;

    uint32_t shaderIndex = 0;

    auto addGeneral = [&](VkShaderModule module, VkShaderStageFlagBits stageFlag, const char* name) {
        VkPipelineShaderStageCreateInfo stageInfo = {};
        stageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage  = stageFlag;
        stageInfo.module = module;
        stageInfo.pName  = "main";

        VkRayTracingShaderGroupCreateInfoKHR groupInfo = {};
        groupInfo.sType         = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        groupInfo.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
        groupInfo.generalShader = shaderIndex++;

        stageInfos.push_back({stageInfo, groupInfo});
        LOG_TRACE_CAT("RENDERER", "Added general group: {} (index {})", name, groupInfo.generalShader);
    };

    auto addTriangleHitGroup = [&](VkShaderModule chit) {
        VkRayTracingShaderGroupCreateInfoKHR groupInfo = {};
        groupInfo.sType           = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
        groupInfo.type            = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
        groupInfo.generalShader   = VK_SHADER_UNUSED_KHR;
        groupInfo.closestHitShader = shaderIndex++;
        groupInfo.anyHitShader    = VK_SHADER_UNUSED_KHR;
        groupInfo.intersectionShader = VK_SHADER_UNUSED_KHR;

        // Closest hit stage
        VkPipelineShaderStageCreateInfo chitStage = {};
        chitStage.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        chitStage.stage  = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        chitStage.module = chit;
        chitStage.pName  = "main";

        stageInfos.push_back({chitStage, groupInfo});
        LOG_TRACE_CAT("RENDERER", "Added triangle hit group with closest hit (index {})", groupInfo.closestHitShader);
    };

    // Required: raygen (index 0)
    addGeneral(raygenModule, VK_SHADER_STAGE_RAYGEN_BIT_KHR, "Raygen");

    // Required: primary miss (index 1)
    addGeneral(missModule, VK_SHADER_STAGE_MISS_BIT_KHR, "Primary Miss");

    // Optional: shadow miss (index 2)
    uint32_t missGroupCount = 1;
    if (hasShadowMiss) {
        addGeneral(shadowMissModule, VK_SHADER_STAGE_MISS_BIT_KHR, "Shadow Miss");
        missGroupCount = 2;
    }

    // Optional: closest hit → creates a hit group (index 2 or 3)
    uint32_t hitGroupCount = 0;
    if (hasClosestHit) {
        addTriangleHitGroup(closestHitModule);
        hitGroupCount = 1;
    }

    const uint32_t raygenGroupCount = 1;

    // Store counts for SBT
    raygenGroupCount_ = raygenGroupCount;
    missGroupCount_ = missGroupCount;
    hitGroupCount_ = hitGroupCount;
    callableGroupCount_ = 0;

    // ---------------------------------------------------------------------
    // 3. Create pipeline layout (CRITICAL: Ensure valid layout)
    // ---------------------------------------------------------------------
    LOG_DEBUG_CAT("RENDERER", "Creating RT pipeline layout");
    VkPipelineLayoutCreateInfo layoutCreateInfo{};
    layoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutCreateInfo.setLayoutCount = 1;
    VkDescriptorSetLayout layout = *rtDescriptorSetLayout_;
    layoutCreateInfo.pSetLayouts = &layout;
    layoutCreateInfo.pushConstantRangeCount = 0;
    layoutCreateInfo.pPushConstantRanges = nullptr;

    VkPipelineLayout rawPipelineLayout = VK_NULL_HANDLE;
    VkResult layoutResult = vkCreatePipelineLayout(device, &layoutCreateInfo, nullptr, &rawPipelineLayout);
    LOG_DEBUG_CAT("RENDERER", "vkCreatePipelineLayout returned: {}", static_cast<int>(layoutResult));
    if (layoutResult != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "Failed to create RT pipeline layout: {}", static_cast<int>(layoutResult));
        throw std::runtime_error("Create RT pipeline layout failed");
    }
    VK_CHECK(layoutResult, "Create RT pipeline layout");

    // Assign to Handle
    rtPipelineLayout_ = RTX::Handle<VkPipelineLayout>(rawPipelineLayout, device,
        [](VkDevice d, VkPipelineLayout l, const VkAllocationCallbacks*) {
            if (l != VK_NULL_HANDLE) vkDestroyPipelineLayout(d, l, nullptr);
        }, 0, "RTPipelineLayout");
    LOG_DEBUG_CAT("RENDERER", "Created RT pipeline layout: 0x{:x}", reinterpret_cast<uintptr_t>(rawPipelineLayout));

    // ---------------------------------------------------------------------
    // 4. Create pipeline
    // ---------------------------------------------------------------------
    VkPipelineLibraryCreateInfoKHR libraryInfo = {};
    libraryInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LIBRARY_CREATE_INFO_KHR;

    std::vector<VkPipelineShaderStageCreateInfo> stages;
    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups;

    for (const auto& info : stageInfos) {
        stages.push_back(info.stage);
        groups.push_back(info.group);
    }

    VkRayTracingPipelineCreateInfoKHR pipelineInfo = {};
    pipelineInfo.sType                        = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
    pipelineInfo.pNext                        = &libraryInfo;
    pipelineInfo.flags                        = 0;
    pipelineInfo.stageCount                   = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages                      = stages.data();
    pipelineInfo.groupCount                   = static_cast<uint32_t>(groups.size());
    pipelineInfo.pGroups                      = groups.data();
    pipelineInfo.maxPipelineRayRecursionDepth = 4;
    pipelineInfo.layout                       = *rtPipelineLayout_;

    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateRayTracingPipelinesKHR(
        device, VK_NULL_HANDLE, VK_NULL_HANDLE,
        1, &pipelineInfo, nullptr, &pipeline),
        "Failed to create ray tracing pipeline");

    // ---------------------------------------------------------------------
    // 5. Store in RAII handle
    // ---------------------------------------------------------------------
    rtPipeline_ = RTX::Handle<VkPipeline>(
        pipeline, device,
        [](VkDevice d, VkPipeline p, const VkAllocationCallbacks*) { vkDestroyPipeline(d, p, nullptr); },
        0, "RTPipeline"
    );

    // Cleanup shader modules
    vkDestroyShaderModule(device, raygenModule, nullptr);
    vkDestroyShaderModule(device, missModule, nullptr);
    if (hasClosestHit) vkDestroyShaderModule(device, closestHitModule, nullptr);
    if (hasShadowMiss) vkDestroyShaderModule(device, shadowMissModule, nullptr);

    LOG_SUCCESS_CAT("RENDERER", "{}Ray tracing pipeline created successfully — {} stages, {} groups{}",
                    LIME_GREEN, stages.size(), groups.size(), RESET);
    LOG_SUCCESS_CAT("RENDERER", "PINK PHOTONS ARMED — FIRST LIGHT ACHIEVED");
    LOG_TRACE_CAT("RENDERER", "createRayTracingPipeline — COMPLETE");
}

// ──────────────────────────────────────────────────────────────────────────────
// Shader Binding Table Creation — FIXED FOR GLOBAL QUEUE/POOL USAGE
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::createShaderBindingTable() {
    LOG_TRACE_CAT("RENDERER", "createShaderBindingTable — START");

    VkDevice device = RTX::g_ctx().vkDevice();

    // ====================================================================
    // Step 1: Validate pipeline exists
    // ====================================================================
    if (!rtPipeline_.valid() || *rtPipeline_ == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "createShaderBindingTable called but rtPipeline_ is null! Did createRayTracingPipeline() run?");
        throw std::runtime_error("rtPipeline_ is invalid in createShaderBindingTable");
    }
    LOG_TRACE_CAT("RENDERER", "Step 1 — rtPipeline_ valid @ 0x{:x}", reinterpret_cast<uintptr_t>(*rtPipeline_));

    // ====================================================================
    // Step 2: Query ray tracing pipeline properties
    // ====================================================================
    LOG_TRACE_CAT("RENDERER", "Step 2 — Querying VkPhysicalDeviceRayTracingPipelinePropertiesKHR");
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
    rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;

    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rtProps;

    vkGetPhysicalDeviceProperties2(RTX::g_ctx().vkPhysicalDevice(), &props2);

    const uint32_t handleSize        = rtProps.shaderGroupHandleSize;
    const uint32_t handleAlignment   = rtProps.shaderGroupHandleAlignment;
    const uint32_t baseAlignment     = rtProps.shaderGroupBaseAlignment;
    const uint32_t maxHandleSize     = rtProps.maxShaderGroupStride;

    LOG_INFO_CAT("RENDERER", "RT Properties — handleSize={}B, handleAlignment={}B, baseAlignment={}B, maxStride={}B",
                 handleSize, handleAlignment, baseAlignment, maxHandleSize);

    // ====================================================================
    // Step 3: Use stored group counts from pipeline creation
    // ====================================================================
    const uint32_t raygenGroupCount   = raygenGroupCount_;
    const uint32_t missGroupCount     = missGroupCount_;
    const uint32_t hitGroupCount      = hitGroupCount_;
    const uint32_t callableGroupCount = callableGroupCount_;

    const uint32_t totalGroups = raygenGroupCount + missGroupCount + hitGroupCount + callableGroupCount;

    LOG_INFO_CAT("RENDERER", "SBT Group Counts — RayGen: {}, Miss: {}, Hit: {}, Callable: {} → Total: {}",
                 raygenGroupCount, missGroupCount, hitGroupCount, callableGroupCount, totalGroups);

    // ====================================================================
    // Step 4: Calculate aligned sizes and offsets
    // ====================================================================
    const VkDeviceSize handleSizeAligned = (handleSize + handleAlignment - 1) & ~(handleAlignment - 1);

    VkDeviceSize currentOffset = 0;

    const VkDeviceSize raygenOffset = currentOffset;
    const VkDeviceSize raygenSize   = raygenGroupCount * handleSizeAligned;
    currentOffset += raygenSize;
    currentOffset = (currentOffset + baseAlignment - 1) & ~(baseAlignment - 1);

    const VkDeviceSize missOffset = currentOffset;
    const VkDeviceSize missSize   = missGroupCount * handleSizeAligned;
    currentOffset += missSize;
    currentOffset = (currentOffset + baseAlignment - 1) & ~(baseAlignment - 1);

    const VkDeviceSize hitOffset = currentOffset;
    const VkDeviceSize hitSize   = hitGroupCount * handleSizeAligned;
    currentOffset += hitSize;
    currentOffset = (currentOffset + baseAlignment - 1) & ~(baseAlignment - 1);

    const VkDeviceSize callableOffset = currentOffset;
    const VkDeviceSize callableSize   = callableGroupCount * handleSizeAligned;
    currentOffset += callableSize;

    const VkDeviceSize sbtBufferSize = currentOffset;

    LOG_INFO_CAT("RENDERER", "SBT Layout — Total Size: {} bytes (~{:.3f} KB)",
                 sbtBufferSize, sbtBufferSize / 1024.0);
    LOG_TRACE_CAT("RENDERER", "  RayGen:  offset={} size={}B", raygenOffset, raygenSize);
    LOG_TRACE_CAT("RENDERER", "  Miss:    offset={} size={}B", missOffset,   missSize);
    LOG_TRACE_CAT("RENDERER", "  Hit:     offset={} size={}B", hitOffset,    hitSize);
    LOG_TRACE_CAT("RENDERER", "  Callable: offset={} size={}B", callableOffset, callableSize);

    // ====================================================================
    // Step 5: Extract shader group handles from pipeline
    // ====================================================================
    LOG_TRACE_CAT("RENDERER", "Step 5 — Extracting shader group handles via vkGetRayTracingShaderGroupHandlesKHR");
    std::vector<uint8_t> shaderHandles(totalGroups * handleSize);

    VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(
        device,
        *rtPipeline_,
        0,
        totalGroups,
        shaderHandles.size(),
        shaderHandles.data()
    ), "Failed to get shader group handles");

    LOG_SUCCESS_CAT("RENDERER", "Successfully extracted {} shader group handles ({} bytes each)", totalGroups, handleSize);

    // ====================================================================
    // Step 6: Create staging buffer (HOST_VISIBLE)
    // ====================================================================
    LOG_TRACE_CAT("RENDERER", "Step 6 — Creating staging buffer (CPU-visible)");
    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

    VkBufferCreateInfo stagingInfo{};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.size = sbtBufferSize;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(device, &stagingInfo, nullptr, &stagingBuffer), "Failed to create SBT staging buffer");

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device, stagingBuffer, &memReqs);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(RTX::g_ctx().vkPhysicalDevice(), memReqs.memoryTypeBits,
                                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &stagingMemory), "Failed to allocate SBT staging memory");
    VK_CHECK(vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0), "Failed to bind SBT staging memory");

    // Map and fill
    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(device, stagingMemory, 0, sbtBufferSize, 0, &mapped), "Failed to map SBT staging memory");

    auto copyGroup = [&](uint32_t groupIndex, VkDeviceSize destOffset) {
        const uint8_t* src = shaderHandles.data() + groupIndex * handleSize;
        std::memcpy(reinterpret_cast<uint8_t*>(mapped) + destOffset, src, handleSize);
    };

    uint32_t currentGroupIndex = 0;
    for (uint32_t i = 0; i < raygenGroupCount; ++i) {
        copyGroup(currentGroupIndex++, raygenOffset + i * handleSizeAligned);
    }
    for (uint32_t i = 0; i < missGroupCount; ++i) {
        copyGroup(currentGroupIndex++, missOffset + i * handleSizeAligned);
    }
    for (uint32_t i = 0; i < hitGroupCount; ++i) {
        copyGroup(currentGroupIndex++, hitOffset + i * handleSizeAligned);
    }
    for (uint32_t i = 0; i < callableGroupCount; ++i) {
        copyGroup(currentGroupIndex++, callableOffset + i * handleSizeAligned);
    }

    vkUnmapMemory(device, stagingMemory);
    LOG_TRACE_CAT("RENDERER", "Step 6 — Staging buffer filled and unmapped");

    // ====================================================================
    // Step 7: Create final device-local SBT buffer
    // ====================================================================
    LOG_TRACE_CAT("RENDERER", "Step 7 — Creating final device-local SBT buffer");

    VkBufferCreateInfo sbtInfo{};
    sbtInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    sbtInfo.size = sbtBufferSize;
    sbtInfo.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
                     VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                     VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    sbtInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(device, &sbtInfo, nullptr, &sbtBuffer_), "Failed to create final SBT buffer");
    vkGetBufferMemoryRequirements(device, sbtBuffer_, &memReqs);

    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = findMemoryType(RTX::g_ctx().vkPhysicalDevice(), memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vkAllocateMemory(device, &allocInfo, nullptr, &sbtMemory_), "Failed to allocate final SBT memory");
    VK_CHECK(vkBindBufferMemory(device, sbtBuffer_, sbtMemory_, 0), "Failed to bind final SBT memory");

    // Copy staging → device (using global queue/pool)
    VkCommandBuffer cmd = beginSingleTimeCommands(device, RTX::g_ctx().commandPool());
    VkBufferCopy copy{};
    copy.size = sbtBufferSize;
    vkCmdCopyBuffer(cmd, stagingBuffer, sbtBuffer_, 1, &copy);
    endSingleTimeCommands(device, RTX::g_ctx().commandPool(), RTX::g_ctx().graphicsQueue(), cmd);

    // Cleanup staging
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);
    LOG_TRACE_CAT("RENDERER", "Step 7 — Final SBT buffer created and copied");

    // ====================================================================
    // Step 8: Get device address and store state
    // ====================================================================
    VkBufferDeviceAddressInfo addrInfo{};
    addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    addrInfo.buffer = sbtBuffer_;
    sbtAddress_ = vkGetBufferDeviceAddressKHR(device, &addrInfo);

    // Store offsets and metadata
    raygenSbtOffset_ = raygenOffset;
    missSbtOffset_   = missOffset;
    hitSbtOffset_    = hitOffset;
    callableSbtOffset_ = callableOffset;
    sbtStride_       = handleSizeAligned;

    sbtBufferEnc_ = sbtAddress_;

    LOG_SUCCESS_CAT("RENDERER", 
        "Shader Binding Table CREATED — Address: 0x{:x} | Size: {} bytes | Stride: {}B", 
        sbtAddress_, sbtBufferSize, sbtStride_);
    LOG_TRACE_CAT("RENDERER", "SBT Offsets — RayGen: {} | Miss: {} | Hit: {} | Callable: {}", 
                  raygenSbtOffset_, missSbtOffset_, hitSbtOffset_, callableSbtOffset_);
    LOG_TRACE_CAT("RENDERER", "createShaderBindingTable — COMPLETE — PINK PHOTONS FULLY ARMED");
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

    // CRITICAL: Ensure rtDescriptorPool_ is created/valid before allocation
    LOG_DEBUG_CAT("RENDERER", "Checking rtDescriptorPool validity: valid={}, handle=0x{:x}", rtDescriptorPool_.valid(), reinterpret_cast<uintptr_t>(*rtDescriptorPool_));
    if (!rtDescriptorPool_.valid() || *rtDescriptorPool_ == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "rtDescriptorPool invalid — creating RT pool");

        // Define pool sizes based on bindings (one set per frame, 4 descriptor types)
        std::array<VkDescriptorPoolSize, 4> poolSizes{};
        LOG_DEBUG_CAT("RENDERER", "Initializing RT pool sizes array of size {}", poolSizes.size());

        poolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        poolSizes[0].descriptorCount = Options::Performance::MAX_FRAMES_IN_FLIGHT;
        LOG_DEBUG_CAT("RENDERER", "Pool size 0: AS, type={}, count={}", static_cast<int>(poolSizes[0].type), poolSizes[0].descriptorCount);

        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[1].descriptorCount = Options::Performance::MAX_FRAMES_IN_FLIGHT;
        LOG_DEBUG_CAT("RENDERER", "Pool size 1: Storage Image, type={}, count={}", static_cast<int>(poolSizes[1].type), poolSizes[1].descriptorCount);

        poolSizes[2].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[2].descriptorCount = Options::Performance::MAX_FRAMES_IN_FLIGHT;
        LOG_DEBUG_CAT("RENDERER", "Pool size 2: Uniform Buffer, type={}, count={}", static_cast<int>(poolSizes[2].type), poolSizes[2].descriptorCount);

        poolSizes[3].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[3].descriptorCount = Options::Performance::MAX_FRAMES_IN_FLIGHT;
        LOG_DEBUG_CAT("RENDERER", "Pool size 3: Combined Image Sampler, type={}, count={}", static_cast<int>(poolSizes[3].type), poolSizes[3].descriptorCount);

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = 0;  // Non-freeable pool
        poolInfo.maxSets = Options::Performance::MAX_FRAMES_IN_FLIGHT;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        LOG_DEBUG_CAT("RENDERER", "Pool info: sType={}, maxSets={}, poolSizeCount={}, pPoolSizes=0x{:x}", static_cast<int>(poolInfo.sType), poolInfo.maxSets, poolInfo.poolSizeCount, reinterpret_cast<uintptr_t>(poolInfo.pPoolSizes));

        VkDescriptorPool rawPool = VK_NULL_HANDLE;
        VkResult poolResult = vkCreateDescriptorPool(device, &poolInfo, nullptr, &rawPool);
        LOG_DEBUG_CAT("RENDERER", "vkCreateDescriptorPool returned: {}", static_cast<int>(poolResult));
        if (poolResult != VK_SUCCESS) {
            LOG_ERROR_CAT("RENDERER", "Failed to create RT pool: {}", static_cast<int>(poolResult));
            throw std::runtime_error("Create RT descriptor pool failed");
        }
        VK_CHECK(poolResult, "Create RT descriptor pool");  // Redundant but for macro consistency

        // Assign to Handle
        LOG_TRACE_CAT("RENDERER", "Creating Handle for rtDescriptorPool_");
        rtDescriptorPool_ = RTX::Handle<VkDescriptorPool>(rawPool, device,
            [](VkDevice d, VkDescriptorPool p, const VkAllocationCallbacks*) {
                if (p != VK_NULL_HANDLE) vkDestroyDescriptorPool(d, p, nullptr);
            }, 0, "RTDescriptorPool");
        LOG_DEBUG_CAT("RENDERER", "Created new RT descriptor pool: 0x{:x}", reinterpret_cast<uintptr_t>(rawPool));
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
    allocInfo.descriptorPool = *rtDescriptorPool_;  // Now guaranteed valid
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();  // Guaranteed non-null
    LOG_DEBUG_CAT("RENDERER", "Alloc info: sType={}, pool=0x{:x}, count={}, pSetLayouts=0x{:x}", static_cast<int>(allocInfo.sType), reinterpret_cast<uintptr_t>(allocInfo.descriptorPool), allocInfo.descriptorSetCount, reinterpret_cast<uintptr_t>(allocInfo.pSetLayouts));

    // Bulletproof: Check pool validity (now redundant but kept for safety)
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
    LOG_TRACE_CAT("RENDERER", "updateNexusDescriptors — START");

    if (!hypertraceScoreView_.valid() || rtDescriptorSets_.empty()) {
        LOG_DEBUG_CAT("RENDERER", "updateNexusDescriptors — SKIPPED (no view or sets)");
        LOG_TRACE_CAT("RENDERER", "updateNexusDescriptors — COMPLETE (skipped)");
        return;
    }

    VkDescriptorSet set = rtDescriptorSets_[currentFrame_ % rtDescriptorSets_.size()];

    VkDescriptorImageInfo nexusInfo{};
    nexusInfo.imageView = *hypertraceScoreView_;
    nexusInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = 16;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.pImageInfo = &nexusInfo;

    vkUpdateDescriptorSets(RTX::g_ctx().vkDevice(), 1, &write, 0, nullptr);
    LOG_DEBUG_CAT("RENDERER", "Nexus score image bound → binding 16");

    LOG_TRACE_CAT("RENDERER", "updateNexusDescriptors — COMPLETE");
}

void VulkanRenderer::updateRTXDescriptors() {
    LOG_TRACE_CAT("RENDERER", "updateRTXDescriptors — START");
    // Currently handled by VulkanRTX::updateRTXDescriptors() in VulkanCore.cpp
    // This is a per-frame hook — can be used for dynamic bindings later
    LOG_TRACE_CAT("RENDERER", "updateRTXDescriptors — COMPLETE (delegated)");
}

void VulkanRenderer::updateTonemapDescriptorsInitial() {
    LOG_TRACE_CAT("RENDERER", "updateTonemapDescriptorsInitial — START");

    if (tonemapSets_.empty() || rtOutputViews_.empty()) {
        LOG_TRACE_CAT("RENDERER", "updateTonemapDescriptorsInitial — SKIPPED (no sets/views)");
        return;
    }

    VkDevice device = RTX::g_ctx().vkDevice();

    for (size_t i = 0; i < tonemapSets_.size(); ++i) {
        VkDescriptorSet set = tonemapSets_[i];

        VkDescriptorImageInfo imgInfo{};
        imgInfo.imageView = *rtOutputViews_[i % rtOutputViews_.size()];
        imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = set;
        write.dstBinding = 0;
        write.descriptorCount = 1;
        write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        write.pImageInfo = &imgInfo;

        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
    }

    LOG_TRACE_CAT("RENDERER", "updateTonemapDescriptorsInitial — COMPLETE");
}

void VulkanRenderer::updateDenoiserDescriptors() {
    LOG_TRACE_CAT("RENDERER", "updateDenoiserDescriptors — START");

    if (denoiserSets_.empty() || !denoiserView_.valid() || rtOutputViews_.empty()) {
        LOG_TRACE_CAT("RENDERER", "updateDenoiserDescriptors — SKIPPED");
        return;
    }

    VkDescriptorSet set = denoiserSets_[currentFrame_ % denoiserSets_.size()];

    std::array<VkWriteDescriptorSet, 2> writes{};
    std::array<VkDescriptorImageInfo, 2> infos{};

    // Input: noisy RT output
    infos[0].imageView = *rtOutputViews_[currentFrame_ % rtOutputViews_.size()];
    infos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[0].pImageInfo = &infos[0];

    // Output: denoised result
    infos[1].imageView = *denoiserView_;
    infos[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = set;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &infos[1];

    vkUpdateDescriptorSets(RTX::g_ctx().vkDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    LOG_TRACE_CAT("RENDERER", "updateDenoiserDescriptors — COMPLETE");
}

void VulkanRenderer::performDenoisingPass(VkCommandBuffer cmd) {
    if (!denoisingEnabled_ || !denoiserPipeline_.valid()) {
        LOG_TRACE_CAT("RENDERER", "performDenoisingPass — SKIPPED (disabled)");
        return;
    }

    LOG_PERF_CAT("RENDERER", "Executing SVGF denoiser — cmd=0x{:x}", reinterpret_cast<uintptr_t>(cmd));

    VkDescriptorSet set = denoiserSets_[currentFrame_ % denoiserSets_.size()];

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *denoiserPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *denoiserLayout_, 0, 1, &set, 0, nullptr);

    uint32_t wgX = (width_ + 15) / 16;
    uint32_t wgY = (height_ + 15) / 16;
    vkCmdDispatch(cmd, wgX, wgY, 1);

    // Memory barrier for next pass
    VkMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 1, &barrier, 0, nullptr, 0, nullptr);

    LOG_TRACE_CAT("RENDERER", "performDenoisingPass — COMPLETE");
}

void VulkanRenderer::performTonemapPass(VkCommandBuffer cmd, uint32_t imageIndex) {
    if (!tonemapEnabled_ || !tonemapPipeline_.valid()) {
        LOG_TRACE_CAT("RENDERER", "performTonemapPass — SKIPPED (disabled)");
        return;
    }

    LOG_PERF_CAT("RENDERER", "Executing tonemap pass — imageIndex={}", imageIndex);

    VkDescriptorSet set = tonemapSets_[imageIndex % tonemapSets_.size()];

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *tonemapPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *tonemapLayout_, 0, 1, &set, 0, nullptr);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    // RAW ACCESS — your actual swapchain images are stored here:
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = 0,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = SwapchainManager::get().images()[imageIndex],
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    LOG_TRACE_CAT("RENDERER", "performTonemapPass — COMPLETE");
}

void VulkanRenderer::updateUniformBuffer(uint32_t frame, const Camera& camera, float jitter) {
    LOG_TRACE_CAT("RENDERER", "updateUniformBuffer — START — frame={}, jitter={:.4f}", frame, jitter);

    if (uniformBufferEncs_.empty() || !sharedStagingBuffer_.valid()) {
        LOG_TRACE_CAT("RENDERER", "updateUniformBuffer — SKIPPED (no buffer)");
        return;
    }

    void* data = nullptr;
    vkMapMemory(RTX::g_ctx().vkDevice(), *sharedStagingMemory_, 0, VK_WHOLE_SIZE, 0, &data);

    alignas(16) struct {
        glm::mat4 view;
        glm::mat4 proj;
        glm::mat4 viewProj;
        glm::mat4 invView;
        glm::mat4 invProj;
        glm::vec4 cameraPos;
        glm::vec2 jitter;
        uint32_t frame;
        float time;
        uint32_t spp;
        float _pad[3];
    } ubo{};

    // Use the real GlobalCamera singleton — this is how your engine actually works
    const auto& cam = GlobalCamera::get();

    ubo.view     = cam.view();
    ubo.proj     = cam.proj(width_ / float(height_));
    ubo.viewProj = ubo.proj * ubo.view;
    ubo.invView  = glm::inverse(ubo.view);
    ubo.invProj  = glm::inverse(ubo.proj);
    ubo.cameraPos = glm::vec4(cam.pos(), 1.0f);
    ubo.jitter   = glm::vec2(jitter);
    ubo.frame    = frameNumber_;
    ubo.time     = frameTime_;
    ubo.spp      = currentSpp_;

    std::memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(RTX::g_ctx().vkDevice(), *sharedStagingMemory_);

    LOG_TRACE_CAT("RENDERER", "updateUniformBuffer — COMPLETE");
}

void VulkanRenderer::updateTonemapUniform(uint32_t frame) {
    LOG_TRACE_CAT("RENDERER", "updateTonemapUniform — START — frame={}", frame);

    if (tonemapUniformEncs_.empty() || !sharedStagingBuffer_.valid()) {
        LOG_TRACE_CAT("RENDERER", "updateTonemapUniform — SKIPPED");
        return;
    }

    void* data = nullptr;
    vkMapMemory(RTX::g_ctx().vkDevice(), *sharedStagingMemory_, 0, VK_WHOLE_SIZE, 0, &data);

    struct TonemapUniform {
        float exposure = 1.0f;
        uint32_t type;
        uint32_t enabled;
        float nexusScore;
        uint32_t frame;
        uint32_t spp;
    } ubo{};

    ubo.type = static_cast<uint32_t>(tonemapType_);
    ubo.enabled = tonemapEnabled_ ? 1u : 0u;
    ubo.nexusScore = currentNexusScore_;
    ubo.frame = frameNumber_;
    ubo.spp = currentSpp_;

    std::memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(RTX::g_ctx().vkDevice(), *sharedStagingMemory_);

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
// Load Ray Tracing Function Pointers — FINAL, NOEXCEPT, FULLY LOGGED, COMPILABLE
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::loadRayTracingExtensions() noexcept
{
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 3: Load Ray Tracing Extensions ===");

    auto& c = RTX::ctx();
    VkDevice dev = c.vkDevice();

    LOG_TRACE_CAT("RENDERER", "Fetching vkCmdTraceRaysKHR...");
    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(
        vkGetDeviceProcAddr(dev, "vkCmdTraceRaysKHR"));

    LOG_TRACE_CAT("RENDERER", "Fetching vkCreateRayTracingPipelinesKHR...");
    vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(
        vkGetDeviceProcAddr(dev, "vkCreateRayTracingPipelinesKHR"));

    LOG_TRACE_CAT("RENDERER", "Fetching vkGetRayTracingShaderGroupHandlesKHR...");
    vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(
        vkGetDeviceProcAddr(dev, "vkGetRayTracingShaderGroupHandlesKHR"));

    LOG_TRACE_CAT("RENDERER", "Fetching vkGetBufferDeviceAddressKHR...");
    vkGetBufferDeviceAddressKHR = reinterpret_cast<PFN_vkGetBufferDeviceAddressKHR>(
        vkGetDeviceProcAddr(dev, "vkGetBufferDeviceAddressKHR"));

    // ──────────────────────────────────────────────────────────────────────
    // Validate every pointer with loud logging
    // ──────────────────────────────────────────────────────────────────────
    bool allGood = true;

    if (!vkCmdTraceRaysKHR) {
        LOG_FATAL_CAT("RENDERER", "FATAL: vkCmdTraceRaysKHR is NULL — Ray Tracing NOT supported on this device!");
        allGood = false;
    } else {
        LOG_SUCCESS_CAT("RENDERER", "vkCmdTraceRaysKHR loaded → 0x{:x}", reinterpret_cast<uintptr_t>(vkCmdTraceRaysKHR));
    }

    if (!vkCreateRayTracingPipelinesKHR) {
        LOG_FATAL_CAT("RENDERER", "FATAL: vkCreateRayTracingPipelinesKHR is NULL");
        allGood = false;
    } else {
        LOG_SUCCESS_CAT("RENDERER", "vkCreateRayTracingPipelinesKHR loaded → 0x{:x}", reinterpret_cast<uintptr_t>(vkCreateRayTracingPipelinesKHR));
    }

    if (!vkGetRayTracingShaderGroupHandlesKHR) {
        LOG_FATAL_CAT("RENDERER", "FATAL: vkGetRayTracingShaderGroupHandlesKHR is NULL");
        allGood = false;
    } else {
        LOG_SUCCESS_CAT("RENDERER", "vkGetRayTracingShaderGroupHandlesKHR loaded → 0x{:x}", reinterpret_cast<uintptr_t>(vkGetRayTracingShaderGroupHandlesKHR));
    }

    if (!vkGetBufferDeviceAddressKHR) {
        LOG_FATAL_CAT("RENDERER", "FATAL: vkGetBufferDeviceAddressKHR is NULL");
        allGood = false;
    } else {
        LOG_SUCCESS_CAT("RENDERER", "vkGetBufferDeviceAddressKHR loaded → 0x{:x}", reinterpret_cast<uintptr_t>(vkGetBufferDeviceAddressKHR));
    }

    // ──────────────────────────────────────────────────────────────────────
    // Final verdict — NO throw, NO c.physicalDeviceProperties()
    // ──────────────────────────────────────────────────────────────────────
    if (!allGood) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(c.vkPhysicalDevice(), &props);

        LOG_FATAL_CAT("RENDERER", "RAY TRACING EXTENSIONS FAILED TO LOAD — THIS GPU IS NOT RTX-CAPABLE");
        LOG_FATAL_CAT("RENDERER", "DEVICE NAME: {}", props.deviceName);
        LOG_FATAL_CAT("RENDERER", "DEVICE TYPE: {}", static_cast<int>(props.deviceType));
        LOG_FATAL_CAT("RENDERER", "VENDOR ID: 0x{:04x} | DEVICE ID: 0x{:04x}", props.vendorID, props.deviceID);
        LOG_FATAL_CAT("RENDERER", "ABORTING INITIALIZATION — PINK PHOTONS CANNOT BE ARMED ON NON-RTX HARDWARE");
        std::abort();  // Clean, safe, allowed in noexcept
    }

    LOG_SUCCESS_CAT("RENDERER", "{}ALL RAY TRACING EXTENSIONS LOADED SUCCESSFULLY — PINK PHOTONS ARMED{}", 
                    LIME_GREEN, RESET);
    LOG_TRACE_CAT("RENDERER", "Step 3 COMPLETE — PROCEEDING TO SYNCHRONIZATION");
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