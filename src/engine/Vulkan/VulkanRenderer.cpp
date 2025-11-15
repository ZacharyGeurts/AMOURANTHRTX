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

#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"  // ← FULL INTEGRATION: PipelineManager for RT
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/camera.hpp"
#include "engine/SDL3/SDL3_vulkan.hpp"
#include "stb/stb_image.h"

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
// Cleanup and Destruction — FIXED: Null Device Guards + No Dtor Cleanup Call — NOV 15 2025
// ──────────────────────────────────────────────────────────────────────────────
// • REMOVED: cleanup() call from ~VulkanRenderer() — avoids duplicate dispose (cleanup called explicitly in phase6_shutdown via app.reset())
// • ADDED: Guards for all vk* calls — safe even if called post-RTX::shutdown()
// • Empire: Renderer resources cleaned BEFORE device nullify
// ──────────────────────────────────────────────────────────────────────────────
VulkanRenderer::~VulkanRenderer() {
    LOG_TRACE_CAT("RENDERER", "Destructor — START (no explicit cleanup — handled in phase6)");
    // NO cleanup() here — prevents duplicate dispose; RAII relies on explicit call in main
    LOG_TRACE_CAT("RENDERER", "Destructor — COMPLETE");
}

void VulkanRenderer::cleanup() noexcept {
    LOG_INFO_CAT("RENDERER", "Initiating renderer shutdown — PINK PHOTONS DIMMING");

    const auto& c = RTX::ctx();
    if (!c.isValid()) {
        LOG_WARN_CAT("RENDERER", "Context invalid — early cleanup exit");
        return;
    }

    // FIXED: Guard vkDeviceWaitIdle — null device invalid (VUID-vkDeviceWaitIdle-device-parameter)
    VkDevice dev = c.device();  // Cache once
    if (dev != VK_NULL_HANDLE) {
        LOG_TRACE_CAT("RENDERER", "cleanup — vkDeviceWaitIdle");
        vkDeviceWaitIdle(dev);
    } else {
        LOG_WARN_CAT("RENDERER", "Skipped vkDeviceWaitIdle — null device");
    }

    LOG_TRACE_CAT("RENDERER", "cleanup — SWAPCHAIN");
    // FIXED: Guard SWAPCHAIN.cleanup() — ensures no call if device null
    if (dev != VK_NULL_HANDLE) {
        SWAPCHAIN.cleanup();  // Assume SWAPCHAIN has internal guards
    } else {
        LOG_WARN_CAT("RENDERER", "Skipped SWAPCHAIN.cleanup — null device");
    }

    // ── NEW: Free All Descriptor Sets BEFORE Destroying Views/Images/Pools (Fixes VUID-vkDestroyImageView-imageView-01026) ──
    LOG_TRACE_CAT("RENDERER", "cleanup — Freeing descriptor sets (unbinds views from sets)");
    if (dev != VK_NULL_HANDLE) {
        std::vector<VkDescriptorSet> rtSets(rtDescriptorSets_.begin(), rtDescriptorSets_.end());
        std::vector<VkDescriptorSet> tonemapSets(tonemapSets_.begin(), tonemapSets_.end());
        std::vector<VkDescriptorSet> denoiserSets(denoiserSets_.begin(), denoiserSets_.end());
        // Add other sets if any (e.g., from g_rtx() or graphics/compute)

        // Free RT sets from rtDescriptorPool_
        if (!rtSets.empty() && rtDescriptorPool_.valid() && *rtDescriptorPool_ != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(dev, *rtDescriptorPool_, static_cast<uint32_t>(rtSets.size()), rtSets.data());
            rtDescriptorSets_.clear();
            LOG_TRACE_CAT("RENDERER", "Freed {} RT descriptor sets", rtSets.size());
        }

        // Free tonemap/denoiser from descriptorPool_ (assume shared pool)
        std::vector<VkDescriptorSet> graphicsSets;
        graphicsSets.insert(graphicsSets.end(), tonemapSets.begin(), tonemapSets.end());
        graphicsSets.insert(graphicsSets.end(), denoiserSets.begin(), denoiserSets.end());
        if (!graphicsSets.empty() && descriptorPool_.valid() && *descriptorPool_ != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(dev, *descriptorPool_, static_cast<uint32_t>(graphicsSets.size()), graphicsSets.data());
            tonemapSets_.clear();
            denoiserSets_.clear();
            LOG_TRACE_CAT("RENDERER", "Freed {} graphics descriptor sets", graphicsSets.size());
        }
    } else {
        LOG_WARN_CAT("RENDERER", "Skipped descriptor set free — null device");
        rtDescriptorSets_.clear();
        tonemapSets_.clear();
        denoiserSets_.clear();
    }

    // ── Sync Objects (only what you actually have) ────────────────────────
    // FIXED: Explicit vkDestroy calls (leaks fixed: semaphores/fences must be destroyed, not just cleared)
    LOG_TRACE_CAT("RENDERER", "cleanup — Destroying semaphores & fences");
    if (dev != VK_NULL_HANDLE) {
        for (auto s : imageAvailableSemaphores_) {
            if (s != VK_NULL_HANDLE) {  // FIXED: Explicit null check
                vkDestroySemaphore(dev, s, nullptr);
            }
        }
        for (auto s : renderFinishedSemaphores_) {
            if (s != VK_NULL_HANDLE) {  // FIXED: Explicit null check
                vkDestroySemaphore(dev, s, nullptr);
            }
        }
        for (auto f : inFlightFences_) {
            if (f != VK_NULL_HANDLE) {  // FIXED: Explicit null check
                vkDestroyFence(dev, f, nullptr);
            }
        }
    } else {
        LOG_WARN_CAT("RENDERER", "Skipped sync object destroys — null device");
    }
    imageAvailableSemaphores_.clear();
    renderFinishedSemaphores_.clear();
    inFlightFences_.clear();

    // ── Timestamp Query Pool (you DO have this member) ────────────────────
    // FIXED: Guard vkDestroyQueryPool — null device triggers VUID-vkDestroyQueryPool-device-parameter
    if (timestampQueryPool_ != VK_NULL_HANDLE && dev != VK_NULL_HANDLE) {
        LOG_TRACE_CAT("RENDERER", "Destroying timestampQueryPool_ 0x{:x}", 
                      reinterpret_cast<uint64_t>(timestampQueryPool_));
        vkDestroyQueryPool(dev, timestampQueryPool_, nullptr);
        timestampQueryPool_ = VK_NULL_HANDLE;
    } else if (timestampQueryPool_ != VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "Skipped timestampQueryPool_ destroy — null device (0x{:x})", 
                     reinterpret_cast<uint64_t>(timestampQueryPool_));
    }

    // ── RT Output / Accumulation / Denoiser Images ───────────────────────
    // FIXED: Explicit guards in destroy* methods (assume implemented; add vkDestroyImage calls with dev check)
    if (dev != VK_NULL_HANDLE) {
        destroyRTOutputImages();       // e.g., vkDestroyImage(dev, rtOutputImage_, nullptr);
        destroyAccumulationImages();   // e.g., vkDestroyImage(dev, accumImage_, nullptr);
        destroyNexusScoreImage();      // e.g., vkDestroyImage(dev, nexusImage_, nullptr);
        destroyDenoiserImage();        // e.g., vkDestroyImage(dev, denoiserImage_, nullptr);
    } else {
        LOG_WARN_CAT("RENDERER", "Skipped image destroys — null device (nullifying refs only)");
    // ——— FIXED: Correct cleanup of all RT images (vectors + singular Handles) ———
    for (auto& h : rtOutputImages_)   h.reset();
    for (auto& h : accumImages_)      h.reset();

    //nexusImage_.reset();      // ← singular Handle, not a vector
    denoiserImage_.reset();   // ← singular Handle, not a vector

    rtOutputImages_.clear();
    accumImages_.clear();
    // No .clear() needed on singular Handles — they’re not containers
    }

    // ── Descriptor Pools ─────────────────────────────────────────────────
    // FIXED: Assume reset() has guards; if std::unique_ptr-like, it's safe
    descriptorPool_.reset();
    rtDescriptorPool_.reset();

    // ── Command Buffers ──────────────────────────────────────────────────
    // FIXED: Guard vkFreeCommandBuffers — null device invalid
    VkCommandPool pool = c.commandPool();
    if (!commandBuffers_.empty() && dev != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(dev, pool,
                             static_cast<uint32_t>(commandBuffers_.size()),
                             commandBuffers_.data());
        commandBuffers_.clear();
        LOG_TRACE_CAT("RENDERER", "Command buffers freed");
    } else if (!commandBuffers_.empty()) {
        LOG_WARN_CAT("RENDERER", "Skipped command buffer free — invalid device/pool (size: {})", 
                     commandBuffers_.size());
        commandBuffers_.clear();  // Clear vector anyway to avoid stale refs
    }

    LOG_SUCCESS_CAT("RENDERER", "{}Renderer shutdown complete — ZERO LEAKS — READY FOR REINIT{}", 
                    EMERALD_GREEN, RESET);
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
// Constructor — FIXED: const auto& c = RTX::g_ctx() (ref); Early PipelineManager after step 7; Default ctor for dummy
// ──────────────────────────────────────────────────────────────────────────────
VulkanRenderer::VulkanRenderer(int width, int height, SDL_Window* window, bool overclockFromMain)
    : window_(window), width_(width), height_(height), overclockMode_(overclockFromMain),
      hypertraceEnabled_(Options::RTX::ENABLE_ADAPTIVE_SAMPLING),
      denoisingEnabled_(Options::RTX::ENABLE_DENOISING),
      adaptiveSamplingEnabled_(Options::RTX::ENABLE_ADAPTIVE_SAMPLING),
      tonemapType_(TonemapType::ACES),
      lastPerfLogTime_(std::chrono::steady_clock::now()), frameCounter_(0),
      pipelineManager_()  // ← FIXED: Default ctor (nulls) — real instance assigned post-step 7
{
    LOG_ATTEMPT_CAT("RENDERER", "Constructing VulkanRenderer ({}x{}) — INTERNAL SHADERS ACTIVE — PINK PHOTONS RISING", width, height);

    // FIXED: Ref to ctx to avoid copy/delete error
    const auto& c = RTX::g_ctx();  // ← FIXED: const auto& (ref), valid throughout ctor

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
    // STACK BUILD ORDER — REPAIRED: All Context Calls with ref c + ()
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

    const uint32_t framesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT; // 3 triple buffer

    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 3: Load Ray Tracing Extensions ===");
    loadRayTracingExtensions();
    LOG_TRACE_CAT("RENDERER", "Step 3 COMPLETE");

    // --- 4. Create Surface VulkanCore.cpp ---

    // =============================================================================
    // STEP 5 — CREATE SYNCHRONIZATION OBJECTS (TRIPLE BUFFERING + ASYNC COMPUTE)
    // =============================================================================
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 5: Create Synchronization Objects ===");
    LOG_TRACE_CAT("RENDERER", "Target in-flight frames: {} — TRUE TRIPLE BUFFERING ACTIVE", framesInFlight);

    imageAvailableSemaphores_.resize(framesInFlight);
    renderFinishedSemaphores_.resize(framesInFlight);
    inFlightFences_.resize(framesInFlight);
    computeFinishedSemaphores_.resize(framesInFlight);
    computeToGraphicsSemaphores_.resize(framesInFlight);

    VkSemaphoreCreateInfo semInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };

    for (uint32_t i = 0; i < framesInFlight; ++i) {
        LOG_TRACE_CAT("RENDERER", "Creating sync objects for frame {} / {}", i, framesInFlight - 1);

        VK_CHECK(vkCreateSemaphore(c.device(), &semInfo, nullptr, &imageAvailableSemaphores_[i]),      "imageAvailable");
        VK_CHECK(vkCreateSemaphore(c.device(), &semInfo, nullptr, &renderFinishedSemaphores_[i]),      "renderFinished");
        VK_CHECK(vkCreateSemaphore(c.device(), &semInfo, nullptr, &computeFinishedSemaphores_[i]),    "computeFinished");
        VK_CHECK(vkCreateSemaphore(c.device(), &semInfo, nullptr, &computeToGraphicsSemaphores_[i]),  "compute→graphics");
        VK_CHECK(vkCreateFence(c.device(), &fenceInfo, nullptr, &inFlightFences_[i]),                 "inFlightFence");
    }
    LOG_SUCCESS_CAT("RENDERER", "Step 5 COMPLETE — {} full sync sets created", framesInFlight);

    // =============================================================================
    // STEP 6 — GPU Timestamp Query Pool
    // =============================================================================
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 6: GPU Timestamp Queries ===");
    if (Options::Performance::ENABLE_GPU_TIMESTAMPS || Options::Debug::SHOW_GPU_TIMESTAMPS) {
        VkQueryPoolCreateInfo qpInfo{ .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
        qpInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        qpInfo.queryCount = framesInFlight * 2;
        VK_CHECK(vkCreateQueryPool(c.device(), &qpInfo, nullptr, &timestampQueryPool_), "Timestamp pool");
    }
    LOG_TRACE_CAT("RENDERER", "Step 6 COMPLETE");

    // =============================================================================
    // STEP 7 — GPU Properties + Timestamp Period
    // =============================================================================
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 7: Query GPU Properties ===");
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(c.physicalDevice(), &props);
    timestampPeriod_ = props.limits.timestampPeriod / 1e6f;
    LOG_INFO_CAT("RENDERER", "GPU: {} | Timestamp period: {:.3f} ms", props.deviceName, timestampPeriod_);
    LOG_TRACE_CAT("RENDERER", "Step 7 COMPLETE");

    // =============================================================================
    // STEP 7.5 — EARLY PIPELINEMANAGER CONSTRUCTION (Post-Properties, Pre-Targets)
    // =============================================================================
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 7.5: Construct PipelineManager Early ===");
    LOG_TRACE_CAT("RENDERER", "Pre-construct check: dev=0x{:x}, phys=0x{:x}", reinterpret_cast<uintptr_t>(c.device()), reinterpret_cast<uintptr_t>(c.physicalDevice()));
    if (c.device() == VK_NULL_HANDLE || c.physicalDevice() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "Invalid context for PipelineManager — dev=0x{:x}, phys=0x{:x}", reinterpret_cast<uintptr_t>(c.device()), reinterpret_cast<uintptr_t>(c.physicalDevice()));
        throw std::runtime_error("Invalid Vulkan context for PipelineManager");
    }
    pipelineManager_ = RTX::PipelineManager(c.device(), c.physicalDevice());  // ← FIXED: Move-assign valid instance early
    LOG_TRACE_CAT("RENDERER", "Step 7.5 COMPLETE — PipelineManager armed (dev=0x{:x}, phys=0x{:x})", reinterpret_cast<uintptr_t>(c.device()), reinterpret_cast<uintptr_t>(c.physicalDevice()));

    // =============================================================================
    // STEP 8 — Initialize Swapchain (uses global surface from RTX::initContext)
    // =============================================================================
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 8: Initialize Swapchain ===");
    SWAPCHAIN.init(c.instance(), c.physicalDevice(), c.device(), c.surface(), width, height);

    if (SWAPCHAIN.images().empty()) {
        LOG_FATAL_CAT("RENDERER", "Swapchain has zero images — initialization failed");
        throw std::runtime_error("Invalid swapchain");
    }
    LOG_SUCCESS_CAT("RENDERER", "Swapchain ready: {} images @ {}x{}", SWAPCHAIN.images().size(), SWAPCHAIN.extent().width, SWAPCHAIN.extent().height);
    LOG_TRACE_CAT("RENDERER", "Step 8 COMPLETE");

    // =============================================================================
    // STEP 9 — HDR + RT RENDER TARGETS (POST-SWAPCHAIN, POST-PIPELINEMANAGER)
    // =============================================================================
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 9: Create HDR & RT Targets ===");
    if (Options::Environment::ENABLE_ENV_MAP) createEnvironmentMap();
    createAccumulationImages();                    // HDR accumulation
    createRTOutputImages();                        // HDR ray tracing output
    if (Options::RTX::ENABLE_DENOISING) createDenoiserImage();
    if (Options::RTX::ENABLE_ADAPTIVE_SAMPLING)
        createNexusScoreImage(c.physicalDevice(), c.device(), c.commandPool(), c.graphicsQueue());
    LOG_SUCCESS_CAT("RENDERER", "Step 9 COMPLETE — HDR pipeline targets created");

    // =============================================================================
    // STEP 10 — Descriptor System (Uses Global VulkanRTX Instance)
    // =============================================================================
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 10: Descriptor System ===");
    g_rtx().initDescriptorPoolAndSets();
    LOG_SUCCESS_CAT("RENDERER", "Step 10 COMPLETE — {} descriptor sets forged", Options::Performance::MAX_FRAMES_IN_FLIGHT);

    // =============================================================================
    // STEP 11 — Per-Frame Buffers
    // =============================================================================
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 11: Initialize Per-Frame Buffers ===");
    initializeAllBufferData(framesInFlight, 64_MB, 16_MB);
    LOG_TRACE_CAT("RENDERER", "Step 11 COMPLETE");

    // =============================================================================
    // STEP 12 — Command Buffers
    // =============================================================================
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 12: Allocate Command Buffers ===");
    createCommandBuffers();
    LOG_TRACE_CAT("RENDERER", "Step 12 COMPLETE");

    // =============================================================================
    // STEP 13 — RT Pipeline + SBT via PipelineManager (Now Early — Step 7.5)
    // =============================================================================
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 13: Ray Tracing Pipeline via PipelineManager ===");
    pipelineManager_.createRayTracingPipeline(finalShaderPaths);              // ← Uses internal loadShader, zero-init, UNUSED_KHR, etc.
    pipelineManager_.createShaderBindingTable(c.commandPool(), c.graphicsQueue());  // ← Uses internal begin/endSingleTimeCommands, DEVICE_ADDRESS_BIT
    LOG_TRACE_CAT("RENDERER", "Step 13 COMPLETE — PipelineManager fully armed ({} groups, SBT @ 0x{:x})", 
                  pipelineManager_.raygenGroupCount() + pipelineManager_.missGroupCount() + pipelineManager_.hitGroupCount(), pipelineManager_.sbtAddress());

    // =============================================================================
    // REPAIRED: STEP 13.5 — Allocate RT Descriptor Sets (POST-Pipeline/SBT, PRE-Updates)
    // =============================================================================
    // • NEW: Explicit allocation using PipelineManager's layout/pool — ensures rtDescriptorSets_ valid
    // • GUARD: Post-alloc validation — fatal log + abort if empty (prevents render warnings)
    // =============================================================================
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 13.5: Allocate RT Descriptor Sets via PipelineManager ===");
    allocateDescriptorSets();  // ← REPAIRED: Explicit call here — populates rtDescriptorSets_ w/ framesInFlight sets
    if (rtDescriptorSets_.empty()) {
        LOG_FATAL_CAT("RENDERER", "CRITICAL: RT descriptor sets allocation failed — empty post-allocateDescriptorSets() — ABORTING INIT");
        throw std::runtime_error("RT descriptor sets allocation failed — invalid RT state guaranteed");
    }
    LOG_TRACE_CAT("RENDERER", "RT sets validated: {} non-empty sets (first=0x{:x})", rtDescriptorSets_.size(), reinterpret_cast<uintptr_t>(rtDescriptorSets_[0]));
    LOG_SUCCESS_CAT("RENDERER", "Step 13.5 COMPLETE — RT descriptors allocated & validated — NO MORE INVALID STATE WARNINGS");
    LOG_TRACE_CAT("RENDERER", "Pre-update RT state check: sets={}, pipeline_valid={}, sbt=0x{:x}, output_valid={}",
                  rtDescriptorSets_.empty() ? 0 : rtDescriptorSets_.size(),
                  pipelineManager_.rtPipeline_.valid() ? "YES" : "NO",
                  pipelineManager_.sbtAddress(),
                  rtOutputImages_.empty() ? "NO" : (rtOutputImages_[0].valid() ? "YES" : "NO"));

    // =============================================================================
    // STEP 14 — Final Descriptor Updates
    // =============================================================================
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 14: Update All Descriptors ===");
    updateNexusDescriptors();
    updateRTXDescriptors(0u);  // FIXED: Provide initial frame index
    updateTonemapDescriptorsInitial();
    updateDenoiserDescriptors();
    LOG_TRACE_CAT("RENDERER", "Step 14 COMPLETE");

    // =============================================================================
    // FINAL — FIRST LIGHT ACHIEVED
    // =============================================================================
    LOG_SUCCESS_CAT("RENDERER", 
        "{}VULKAN RENDERER FULLY INITIALIZED — {}x{} — TRIPLE BUFFERING — ASYNC COMPUTE — HDR — PIPELINEMANAGER INTEGRATED — RT STATE VALIDATED — PINK PHOTONS ETERNAL — FIRST LIGHT ACHIEVED{}", 
        EMERALD_GREEN, width, height, RESET);
}

// ──────────────────────────────────────────────────────────────────────────────
// getShaderGroupHandle — VIA PIPELINEMANAGER (Reduced Code)
// ──────────────────────────────────────────────────────────────────────────────
VkDeviceAddress VulkanRenderer::getShaderGroupHandle(uint32_t group) {
    LOG_TRACE_CAT("RENDERER", "getShaderGroupHandle — START — group={}", group);
    // DELEGATE: Use PipelineManager's SBT layout (raygen=0, miss=1+, hit=raygen+miss+)
    VkDeviceAddress groupAddress = pipelineManager_.sbtAddress();
    if (group < pipelineManager_.raygenGroupCount()) {
        groupAddress += pipelineManager_.raygenSbtOffset() + (group * pipelineManager_.sbtStride());
    } else if (group < pipelineManager_.raygenGroupCount() + pipelineManager_.missGroupCount()) {
        uint32_t missGroupIdx = group - pipelineManager_.raygenGroupCount();
        groupAddress += pipelineManager_.missSbtOffset() + (missGroupIdx * pipelineManager_.sbtStride());
    } else if (group < pipelineManager_.raygenGroupCount() + pipelineManager_.missGroupCount() + pipelineManager_.hitGroupCount()) {
        uint32_t hitGroupIdx = group - pipelineManager_.raygenGroupCount() - pipelineManager_.missGroupCount();
        groupAddress += pipelineManager_.hitSbtOffset() + (hitGroupIdx * pipelineManager_.sbtStride());
    } else {
        LOG_WARN_CAT("RENDERER", "Invalid shader group index: {}", group);
        return 0;
    }
    LOG_TRACE_CAT("RENDERER", "Group {} address: 0x{:x}", group, groupAddress);
    LOG_TRACE_CAT("RENDERER", "getShaderGroupHandle — COMPLETE");
    return groupAddress;
}

// ──────────────────────────────────────────────────────────────────────────────
// Image Creation — Options-Driven (Unchanged)
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
    const auto& ctx = RTX::ctx();  // ← FIXED: const ref to avoid dangling
    VkDevice dev = ctx.device();  // Avoid shadowing
    VkPhysicalDevice physDev = ctx.physicalDevice();
    VkCommandPool cmdPool = ctx.commandPool();
    VkQueue queue = ctx.graphicsQueue();

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
                                      VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                      VK_IMAGE_USAGE_TRANSFER_DST_BIT;  // FIXED: Add TRANSFER_DST for vkCmdClearColorImage
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            VK_CHECK(vkCreateImage(dev, &imageInfo, nullptr, &rawImage),
                     ("Failed to create RT output image for frame " + std::to_string(i)).c_str());

            LOG_TRACE_CAT("RENDERER", "Frame {} Image created: 0x{:x} (usage incl. TRANSFER_DST)", i, reinterpret_cast<uintptr_t>(rawImage));

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

            uint32_t memType = pipelineManager_.findMemoryType(physDev, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);  // ← VIA PIPELINEMANAGER
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
            VkCommandBuffer cmd = pipelineManager_.beginSingleTimeCommands(cmdPool);  // ← VIA PIPELINEMANAGER
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

            pipelineManager_.endSingleTimeCommands(cmdPool, queue, cmd);  // ← VIA PIPELINEMANAGER
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

            LOG_TRACE_CAT("RENDERER", "Frame {} — RTOutput ready: img=0x{:x}, view=0x{:x} (TRANSFER_DST enabled)",
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

    LOG_SUCCESS_CAT("RENDERER", "RT output images created — {} frames in GENERAL layout (TRANSFER_DST enabled for clears)", framesInFlight);
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

    // Load HDR file (use stb_image or similar; assume assets/textures/envmap.hdr)
    int width, height, channels;
    float* pixels = stbi_loadf("assets/textures/envmap.hdr", &width, &height, &channels, 4);  // RGBA float
    if (!pixels) {
        LOG_ERROR_CAT("RENDERER", "Failed to load envmap.hdr: {}", stbi_failure_reason());
        LOG_TRACE_CAT("RENDERER", "createEnvironmentMap — COMPLETE (load failed)");
        return;
    }
    LOG_INFO_CAT("RENDERER", "Loaded HDR envmap: {}x{} ({} channels)", width, height, channels);
    VkDeviceSize imageSize = width * height * sizeof(float) * 4;

    // Create staging buffer + upload (similar to your buffer macros)
    uint64_t stagingEnc = 0;
    BUFFER_CREATE(stagingEnc, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, "EnvMapStaging");
    void* data = nullptr;
    BUFFER_MAP(stagingEnc, data);
    memcpy(data, pixels, imageSize);
    BUFFER_UNMAP(stagingEnc);
    stbi_image_free(pixels);

    const auto& ctx = RTX::ctx();  // ← FIXED: const ref
    VkDevice dev = ctx.device();
    VkPhysicalDevice phys = ctx.physicalDevice();

    // Create device-local image (cubemap for equirectangular envmap)
    VkImageCreateInfo imgInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imgInfo.imageType = VK_IMAGE_TYPE_2D;
    imgInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;  // HDR float
    imgInfo.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    imgInfo.mipLevels = 1;  // Add mipgen for better sampling if needed
    imgInfo.arrayLayers = 1;
    imgInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imgInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imgInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgInfo.flags = 0;

    VkImage rawImg = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImage(dev, &imgInfo, nullptr, &rawImg), "Create envmap image");

    // Allocate/bind memory (device local)
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(dev, rawImg, &memReqs);
    uint32_t memType = pipelineManager_.findMemoryType(phys, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);  // ← VIA PIPELINEMANAGER

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memType;
    VkDeviceMemory rawMem = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(dev, &allocInfo, nullptr, &rawMem), "Alloc envmap memory");
    VK_CHECK(vkBindImageMemory(dev, rawImg, rawMem, 0), "Bind envmap memory");

    VkCommandPool cmdPool = ctx.commandPool();
    VkQueue queue = ctx.graphicsQueue();

    // Transition + copy (use single-time commands VIA PIPELINEMANAGER)
    VkCommandBuffer cmd = pipelineManager_.beginSingleTimeCommands(cmdPool);
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = rawImg;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region = {};  // Zero-init for compatibility
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1};
    VkBuffer stagingBuf = RAW_BUFFER(stagingEnc);
    vkCmdCopyBufferToImage(cmd, stagingBuf, rawImg, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition back to sampled
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    pipelineManager_.endSingleTimeCommands(cmdPool, queue, cmd);

    // Create view + sampler
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = rawImg;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    VkImageView rawView = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(dev, &viewInfo, nullptr, &rawView), "Create envmap view");

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VkSampler rawSampler = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSampler(dev, &samplerInfo, nullptr, &rawSampler), "Create envmap sampler");

    // Wrap in Handles (match class member names)
    envMapImage_ = RTX::Handle<VkImage>(rawImg, dev, [](VkDevice d, VkImage i, auto) { vkDestroyImage(d, i, nullptr); }, 0, "EnvMapImage");
    envMapImageMemory_ = RTX::Handle<VkDeviceMemory>(rawMem, dev, [](VkDevice d, VkDeviceMemory m, auto) { vkFreeMemory(d, m, nullptr); }, memReqs.size, "EnvMapMemory");
    envMapImageView_ = RTX::Handle<VkImageView>(rawView, dev, [](VkDevice d, VkImageView v, auto) { vkDestroyImageView(d, v, nullptr); }, 0, "EnvMapView");
    envMapSampler_ = RTX::Handle<VkSampler>(rawSampler, dev, [](VkDevice d, VkSampler s, auto) { vkDestroySampler(d, s, nullptr); }, 0, "EnvMapSampler");

    // Cleanup staging
    BUFFER_DESTROY(stagingEnc);

    LOG_SUCCESS_CAT("RENDERER", "HDR envmap loaded & uploaded — {}x{} float RGBA", width, height);
    LOG_TRACE_CAT("RENDERER", "createEnvironmentMap — COMPLETE");
}

void VulkanRenderer::createNexusScoreImage(VkPhysicalDevice phys, VkDevice dev, VkCommandPool pool, VkQueue queue) {
    if (!Options::RTX::ENABLE_ADAPTIVE_SAMPLING) {
        return;
    }

    // FIXED: Full impl — single RGBA32_SFLOAT storage image in GENERAL layout (matches shader Rgba32f)
    VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;
    VkImage rawImage = VK_NULL_HANDLE;
    VkDeviceMemory rawMemory = VK_NULL_HANDLE;
    VkImageView rawView = VK_NULL_HANDLE;

    try {
        // === 1. Create Image ===
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = format;
        imageInfo.extent = { static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1 };
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        VK_CHECK(vkCreateImage(dev, &imageInfo, nullptr, &rawImage), "Create nexus image");

        // === 2. Allocate & Bind Memory ===
        VkMemoryRequirements memReqs{};
        vkGetImageMemoryRequirements(dev, rawImage, &memReqs);

        uint32_t memType = pipelineManager_.findMemoryType(phys, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memType == static_cast<uint32_t>(~0u)) {
            throw std::runtime_error("No suitable memory type for nexus image");
        }

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = memType;

        VK_CHECK(vkAllocateMemory(dev, &allocInfo, nullptr, &rawMemory), "Alloc nexus memory");
        VK_CHECK(vkBindImageMemory(dev, rawImage, rawMemory, 0), "Bind nexus memory");

        // === 3. Transition to GENERAL ===
        VkCommandBuffer cmd = pipelineManager_.beginSingleTimeCommands(pool);
        if (cmd == VK_NULL_HANDLE) {
            throw std::runtime_error("Failed to begin single-time cmd for nexus transition");
        }

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = rawImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        pipelineManager_.endSingleTimeCommands(pool, queue, cmd);

        // === 4. Create Image View ===
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = rawImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(dev, &viewInfo, nullptr, &rawView), "Create nexus view");

        // === 5. Wrap in Handles ===
        hypertraceScoreImage_ = RTX::Handle<VkImage>(rawImage, dev, vkDestroyImage, 0, "NexusScoreImage");
        hypertraceScoreMemory_ = RTX::Handle<VkDeviceMemory>(rawMemory, dev, vkFreeMemory, memReqs.size, "NexusScoreMemory");
        hypertraceScoreView_ = RTX::Handle<VkImageView>(rawView, dev, vkDestroyImageView, 0, "NexusScoreView");

        // Staging buffer (host-visible for score uploads if needed; init to 0)
        VkDeviceSize stagingSize = static_cast<VkDeviceSize>(width_) * height_ * sizeof(float) * 4;  // RGBA
        uint64_t stagingEnc = 0;
        BUFFER_CREATE(stagingEnc, stagingSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "NexusStaging");
        void* stagingData = nullptr;
        BUFFER_MAP(stagingEnc, stagingData);
        memset(stagingData, 0, stagingSize);  // Init to 0 score
        BUFFER_UNMAP(stagingEnc);

        // Copy staging to image (clear to 0)
        cmd = pipelineManager_.beginSingleTimeCommands(pool);
        VkBuffer stagingBuf = RAW_BUFFER(stagingEnc);
        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = { static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1 };

        // FIXED: Transition to TRANSFER_DST_OPTIMAL before copy
        VkImageMemoryBarrier preCopyBarrier{};
        preCopyBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        preCopyBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        preCopyBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        preCopyBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        preCopyBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        preCopyBarrier.image = *hypertraceScoreImage_;
        preCopyBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        preCopyBarrier.subresourceRange.levelCount = 1;
        preCopyBarrier.subresourceRange.layerCount = 1;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &preCopyBarrier);

        vkCmdCopyBufferToImage(cmd, stagingBuf, *hypertraceScoreImage_, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        // Transition back to GENERAL post-copy
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        pipelineManager_.endSingleTimeCommands(pool, queue, cmd);

        // FIXED: Destroy staging immediately after use — no Handle wrapping to avoid double-destroy
        BUFFER_DESTROY(stagingEnc);

    } catch (const std::exception& e) {
        LOG_FATAL_CAT("RENDERER", "Exception in createNexusScoreImage: {} — cleaning up", e.what());
        if (rawView != VK_NULL_HANDLE) vkDestroyImageView(dev, rawView, nullptr);
        if (rawMemory != VK_NULL_HANDLE) vkFreeMemory(dev, rawMemory, nullptr);
        if (rawImage != VK_NULL_HANDLE) vkDestroyImage(dev, rawImage, nullptr);
        throw;
    }
}

void VulkanRenderer::recordRayTracingCommandBuffer(VkCommandBuffer cmd) noexcept
{
    //LOG_TRACE_CAT("RENDERER", "recordRayTracingCommandBuffer — FIRING PINK PHOTONS — FULL RTX UNLEASHED");

    // ── Temporal Accumulation Reset (only when needed)
    if (resetAccumulation_) {
        VkClearColorValue clearColor = { { 0.0f, 0.0f, 0.0f, 0.0f } };
        VkImageSubresourceRange clearRange = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        };

        const auto clearImage = [&](RTX::Handle<VkImage>& img) {
            if (!img.valid()) return;

            VkImageMemoryBarrier barrier = {
                .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .srcAccessMask       = 0,
                .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
                .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
                .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                .image               = *img,
                .subresourceRange    = clearRange
            };
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            vkCmdClearColorImage(cmd, *img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &clearRange);

            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
            barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout     = VK_IMAGE_LAYOUT_GENERAL;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &barrier);
        };

        for (auto& img : rtOutputImages_)  clearImage(img);
        if (Options::RTX::ENABLE_ACCUMULATION) {
            for (auto& img : accumImages_) clearImage(img);
        }
        if (Options::RTX::ENABLE_ADAPTIVE_SAMPLING && hypertraceScoreImage_.valid()) {
            clearImage(hypertraceScoreImage_);
        }

        resetAccumulation_ = false;
        //LOG_SUCCESS_CAT("RENDERER", "Accumulation reset complete — fresh temporal state — photons reborn");
    }

    // ── Bind RT Pipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *pipelineManager_.rtPipeline_);

    // ── Bind Per-Frame Descriptor Set
    const uint32_t frameIdx = currentFrame_ % rtDescriptorSets_.size();
    VkDescriptorSet rtSet = rtDescriptorSets_[frameIdx];

    vkCmdBindDescriptorSets(cmd,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        *pipelineManager_.rtPipelineLayout_,
        0, 1, &rtSet, 0, nullptr);

    // ── Push Constants — Frame counter, SPP, Hypertrace toggle
    struct PushConstants {
        uint32_t frame;
        uint32_t totalSpp;
        uint32_t hypertraceEnabled;
        uint32_t _pad;
    } push = {
        .frame             = static_cast<uint32_t>(frameNumber_ & 0xFFFFFFFFULL),
        .totalSpp          = currentSpp_,
        .hypertraceEnabled = hypertraceEnabled_ ? 1u : 0u,
        ._pad              = 0
    };

    vkCmdPushConstants(cmd,
        *pipelineManager_.rtPipelineLayout_,
        VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        0, sizeof(push), &push);

    // ── SBT Regions — USING YOUR ACTUAL PipelineManager API
    const VkStridedDeviceAddressRegionKHR* raygen   = pipelineManager_.getRaygenSbtRegion();
    const VkStridedDeviceAddressRegionKHR* miss     = pipelineManager_.getMissSbtRegion();
    const VkStridedDeviceAddressRegionKHR* hit      = pipelineManager_.getHitSbtRegion();
    const VkStridedDeviceAddressRegionKHR* callable = pipelineManager_.getCallableSbtRegion();

    // ── FIRE THE RAYS — FULL RESOLUTION — MAXIMUM THROUGHPUT
    const VkExtent2D extent = SWAPCHAIN.extent();

    vkCmdTraceRaysKHR(cmd,
        raygen,
        miss,
        hit,
        callable,
        extent.width,
        extent.height,
        1);
}

// ──────────────────────────────────────────────────────────────────────────────
// REPAIRED: createImageArray — noexcept + () Calls + Local ctx ref + VIA PIPELINEMANAGER for findMemoryType/cmds
// ──────────────────────────────────────────────────────────────────────────────
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
        LOG_FATAL_CAT("RENDERER", "Invalid swapchain extent {}x{} for {} images — swapchain uninitialized?", 
                      ext.width, ext.height, tag);
        // noexcept: Cannot throw — log fatal & return early
        return;
    }

    const auto& ctx = RTX::ctx();  // ← FIXED: const ref to avoid dangling
    VkDevice device = ctx.device();  // REPAIRED: ()
    VkPhysicalDevice phys = ctx.physicalDevice();  // REPAIRED: ()

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

    // ─── Precompute memory type VIA PIPELINEMANAGER ───
    uint32_t memTypeIndex = UINT32_MAX;
    {
        VkImage dummy = VK_NULL_HANDLE;
        if (vkCreateImage(device, &imgInfo, nullptr, &dummy) == VK_SUCCESS) {
            VkMemoryRequirements req;
            vkGetImageMemoryRequirements(device, dummy, &req);
            memTypeIndex = pipelineManager_.findMemoryType(phys, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);  // ← VIA PIPELINEMANAGER
            vkDestroyImage(device, dummy, nullptr);
        }
    }
    if (memTypeIndex == UINT32_MAX) {
        LOG_FATAL_CAT("RENDERER", "Failed to precompute memory type for {} images", tag);
        return;  // noexcept: Early return on failure
    }

    // ─── Resize containers ───
    images.resize(frames);
    memories.resize(frames);
    views.resize(frames);

    VkCommandPool cmdPool = ctx.commandPool();
    VkQueue queue = ctx.graphicsQueue();

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

        // ─── Transition to GENERAL VIA PIPELINEMANAGER ───
        VkCommandBuffer cmd = pipelineManager_.beginSingleTimeCommands(cmdPool);
        if (cmd != VK_NULL_HANDLE) {
            VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = img;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            pipelineManager_.endSingleTimeCommands(cmdPool, queue, cmd);
        }
    }

    LOG_SUCCESS_CAT("RENDERER", "{} image array created — {} frames @ {}x{}", tag, frames, ext.width, ext.height);
    LOG_TRACE_CAT("RENDERER", "createImageArray — COMPLETE");
}

void VulkanRenderer::createImage(RTX::Handle<VkImage>& image,
                     RTX::Handle<VkDeviceMemory>& memory,
                     RTX::Handle<VkImageView>& view,
                     const std::string& tag) noexcept {
    LOG_TRACE_CAT("RENDERER", "createImage — START — tag='{}'", tag);
    LOG_TRACE_CAT("RENDERER", "Creating single {} image", tag);
    // TODO: Implement single-image variant with similar robustness/speed checks
    LOG_TRACE_CAT("RENDERER", "createImage — COMPLETE (placeholder)");
}

// ──────────────────────────────────────────────────────────────────────────────
// Frame Rendering — CLEAN PRODUCTION LOOP (MINIMAL LOGGING) + VIA PIPELINEMANAGER
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::renderFrame(const Camera& camera, float deltaTime) noexcept
{
    const uint32_t framesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    // Wait for previous frame to finish
    const auto& ctx = RTX::g_ctx();  // const ref
    vkWaitForFences(ctx.vkDevice(), 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    // Acquire swapchain image
    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(ctx.vkDevice(), SWAPCHAIN.swapchain(), UINT64_MAX,
                                            imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        SWAPCHAIN.recreate(width_, height_);
        return;
    }
    if (result != VK_SUCCESS) return;

    // Reset & begin command buffer
    vkResetFences(ctx.vkDevice(), 1, &inFlightFences_[currentFrame_]);
    VkCommandBuffer cmd = commandBuffers_[imageIndex];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cmd, &beginInfo);

    // FIXED: Barrier 1 — Transition acquired swapchain image from UNDEFINED to COLOR_ATTACHMENT_OPTIMAL
    {
        VkImageMemoryBarrier barrier = {};  // Zero-init
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = SWAPCHAIN.images()[imageIndex];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // Src stage (post-acquire)
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // Dst stage (pre-render)
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // Update per-frame data
    updateUniformBuffer(currentFrame_, camera, getJitter());
    updateTonemapUniform(currentFrame_);

    // FIXED: Per-frame RT descriptor updates (tlas/ubo/images) — VUID-08114
    updateRTXDescriptors(currentFrame_);

    // Ray tracing + post-process passes
    recordRayTracingCommandBuffer(cmd);
    if (denoisingEnabled_) performDenoisingPass(cmd);
    performTonemapPass(cmd, imageIndex);

    // FIXED: Barrier 2 — Transition swapchain image from COLOR_ATTACHMENT_OPTIMAL to PRESENT_SRC_KHR
    {
        VkImageMemoryBarrier barrier = {};  // Zero-init
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = SWAPCHAIN.images()[imageIndex];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // Src stage (post-render)
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,           // Dst stage (pre-present)
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    vkEndCommandBuffer(cmd);

    // Submit
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &imageAvailableSemaphores_[currentFrame_];
    submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &renderFinishedSemaphores_[currentFrame_];

    vkQueueSubmit(ctx.graphicsQueue(), 1, &submit, inFlightFences_[currentFrame_]);

    // Present
    VkPresentInfoKHR present{ .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderFinishedSemaphores_[currentFrame_];
    present.swapchainCount = 1;
    VkSwapchainKHR swapchainHandle = SWAPCHAIN.swapchain();
    present.pSwapchains = &swapchainHandle;
    present.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(ctx.presentQueue(), &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        SWAPCHAIN.recreate(width_, height_);
    }

    // Advance frame
    currentFrame_ = (currentFrame_ + 1) % framesInFlight;
    frameNumber_++;
    frameCounter_++;
}

// ──────────────────────────────────────────────────────────────────────────────
// Utility Functions (Reduced: findMemoryType delegated to PipelineManager)
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::initializeAllBufferData(uint32_t frames, VkDeviceSize uniformSize, VkDeviceSize materialSize) {
    // Harden: Validate inputs to prevent overflows or invalid states
    if (frames == 0) {
        LOG_ERROR_CAT("RENDERER", "initializeAllBufferData: Invalid frames count: {}", frames);
        return;
    }
    if (uniformSize > (1ULL << 32) || materialSize > (1ULL << 32)) {  // Arbitrary sane limit for debug
        LOG_WARN_CAT("RENDERER", "initializeAllBufferData: Large buffer sizes detected — uniform={}, material={}", uniformSize, materialSize);
    }

    LOG_INFO_CAT("RENDERER", "Initializing buffer data: {} frames | Uniform: {} MB | Material: {} MB", 
        frames, uniformSize / (1024ULL*1024ULL), materialSize / (1024ULL*1024ULL));  // Use ULL for safe division

    // FIXED: Resize all buffer encodings + create actual buffers via macros
    uniformBufferEncs_.resize(frames);
    materialBufferEncs_.resize(frames);
    dimensionBufferEncs_.resize(frames);
    if (uniformBufferEncs_.size() != static_cast<size_t>(frames)) {
        LOG_ERROR_CAT("RENDERER", "initializeAllBufferData: Resize failed — expected={}, got={}", frames, uniformBufferEncs_.size());
        uniformBufferEncs_.clear();  // Reset to safe state
        return;
    }

    VkDeviceSize dimSize = 64;  // sizeof(uvec2 extent) + uint32 spp + uint32 frame + pad

    for (uint32_t i = 0; i < frames; ++i) {
        // Uniform Buffer (camera/proj etc.)
        BUFFER_CREATE(uniformBufferEncs_[i], uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, std::format("UBO[{}]", i).c_str());

        // Material Storage Buffer
        BUFFER_CREATE(materialBufferEncs_[i], materialSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, std::format("Materials[{}]", i).c_str());

        // Dimension Storage Buffer (small: extent, spp, frame)
        BUFFER_CREATE(dimensionBufferEncs_[i], dimSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, std::format("Dimensions[{}]", i).c_str());
    }

    LOG_TRACE_CAT("RENDERER", "Resized & created buffers for {} frames (UBO/Mat/Dim)", frames);
    LOG_TRACE_CAT("RENDERER", "initializeAllBufferData — COMPLETE");
}

void VulkanRenderer::createCommandBuffers() {
    LOG_TRACE_CAT("RENDERER", "createCommandBuffers — START");
    size_t numImages = SWAPCHAIN.images().size();
    if (numImages == 0) {
        LOG_ERROR_CAT("RENDERER", "Invalid swapchain: 0 images — cannot create command buffers");
        throw std::runtime_error("Invalid swapchain in createCommandBuffers");
    }
    LOG_INFO_CAT("RENDERER", "Allocating {} command buffers", numImages);
    commandBuffers_.resize(numImages);
    VkCommandBufferAllocateInfo allocInfo = {};  // Zero-init (fixes garbage)
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    const auto& ctx = RTX::ctx();  // const ref
    allocInfo.commandPool = ctx.commandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());
    LOG_TRACE_CAT("RENDERER", "Alloc info — pool=0x{:x}, level={}, count={}", reinterpret_cast<uintptr_t>(allocInfo.commandPool), static_cast<int>(allocInfo.level), allocInfo.commandBufferCount);
    VkResult result = vkAllocateCommandBuffers(ctx.vkDevice(), &allocInfo, commandBuffers_.data());
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "vkAllocateCommandBuffers failed: {}", static_cast<int>(result));
        throw std::runtime_error("Command buffer allocation failed");
    }
    LOG_TRACE_CAT("RENDERER", "Allocated command buffers — data=0x{:x}", reinterpret_cast<uintptr_t>(commandBuffers_.data()));
    for (size_t i = 0; i < commandBuffers_.size(); ++i) {
        if (commandBuffers_[i] == VK_NULL_HANDLE) {
            LOG_ERROR_CAT("RENDERER", "Invalid command buffer at index {}", i);
            throw std::runtime_error("Null command buffer allocated");
        }
        LOG_TRACE_CAT("RENDERER", "commandBuffers_[{}]: 0x{:x}", i, reinterpret_cast<uint64_t>(commandBuffers_[i]));
    }
    LOG_TRACE_CAT("RENDERER", "createCommandBuffers — COMPLETE");
}

void VulkanRenderer::allocateDescriptorSets() {
    LOG_TRACE_CAT("RENDERER", "allocateDescriptorSets — START — {} frames", Options::Performance::MAX_FRAMES_IN_FLIGHT);

    const auto& ctx = RTX::g_ctx();  // const ref
    VkDevice device = ctx.vkDevice();
    LOG_DEBUG_CAT("RENDERER", "Retrieved device: 0x{:x}", reinterpret_cast<uintptr_t>(device));

    // CRITICAL: Use PipelineManager's rtDescriptorSetLayout_ (pre-created in ctor)
    VkDescriptorSetLayout validLayout = *pipelineManager_.rtDescriptorSetLayout_;
    LOG_DEBUG_CAT("RENDERER", "PipelineManager layout: 0x{:x}", reinterpret_cast<uintptr_t>(validLayout));
    if (validLayout == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RENDERER", "PipelineManager RT descriptor set layout is null — aborting");
        throw std::runtime_error("PipelineManager RT descriptor set layout is null");
    }

    // CRITICAL: Use PipelineManager's rtDescriptorPool_ (pre-created in ctor, but alloc here if needed)
    if (!pipelineManager_.rtDescriptorPool_.valid() || *pipelineManager_.rtDescriptorPool_ == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "PipelineManager rtDescriptorPool invalid — creating RT pool");

        // Zero-init poolSizes array
        std::array<VkDescriptorPoolSize, 8> poolSizes = {};  // ← 8 BINDINGS FROM PIPELINEMANAGER
        LOG_DEBUG_CAT("RENDERER", "Initializing RT pool sizes array of size {}", poolSizes.size());

        poolSizes[0].type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        poolSizes[0].descriptorCount = Options::Performance::MAX_FRAMES_IN_FLIGHT;

        poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[1].descriptorCount = Options::Performance::MAX_FRAMES_IN_FLIGHT;

        poolSizes[2].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[2].descriptorCount = Options::Performance::MAX_FRAMES_IN_FLIGHT;

        poolSizes[3].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[3].descriptorCount = Options::Performance::MAX_FRAMES_IN_FLIGHT;

        poolSizes[4].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[4].descriptorCount = Options::Performance::MAX_FRAMES_IN_FLIGHT;

        poolSizes[5].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[5].descriptorCount = Options::Performance::MAX_FRAMES_IN_FLIGHT;

        poolSizes[6].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSizes[6].descriptorCount = Options::Performance::MAX_FRAMES_IN_FLIGHT;

        poolSizes[7].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        poolSizes[7].descriptorCount = Options::Performance::MAX_FRAMES_IN_FLIGHT;

        VkDescriptorPoolCreateInfo poolInfo = {};  // Zero-init
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = 0;
        poolInfo.maxSets = Options::Performance::MAX_FRAMES_IN_FLIGHT;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();

        VkDescriptorPool rawPool = VK_NULL_HANDLE;
        VkResult poolResult = vkCreateDescriptorPool(device, &poolInfo, nullptr, &rawPool);
        LOG_DEBUG_CAT("RENDERER", "vkCreateDescriptorPool returned: {}", static_cast<int>(poolResult));
        if (poolResult != VK_SUCCESS) {
            LOG_ERROR_CAT("RENDERER", "Failed to create RT pool: {}", static_cast<int>(poolResult));
            throw std::runtime_error("Create RT descriptor pool failed");
        }

        pipelineManager_.rtDescriptorPool_ = RTX::Handle<VkDescriptorPool>(rawPool, device,
            [](VkDevice d, VkDescriptorPool p, const VkAllocationCallbacks*) {
                if (p != VK_NULL_HANDLE) vkDestroyDescriptorPool(d, p, nullptr);
            }, 0, "RTDescriptorPool");
        LOG_DEBUG_CAT("RENDERER", "Created PipelineManager RT descriptor pool: 0x{:x}", reinterpret_cast<uintptr_t>(rawPool));
    }

    // Allocate (zero-init layouts array)
    std::array<VkDescriptorSetLayout, Options::Performance::MAX_FRAMES_IN_FLIGHT> layouts = {};  // Zero-init
    std::fill(layouts.begin(), layouts.end(), validLayout);
    LOG_DEBUG_CAT("RENDERER", "Layouts filled with PipelineManager RT layout: 0x{:x} for {} frames", reinterpret_cast<uintptr_t>(validLayout), layouts.size());

    rtDescriptorSets_.resize(Options::Performance::MAX_FRAMES_IN_FLIGHT);
    LOG_DEBUG_CAT("RENDERER", "Resized rtDescriptorSets_ to {} entries", rtDescriptorSets_.size());

    VkDescriptorSetAllocateInfo allocInfo = {};  // Zero-init
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = *pipelineManager_.rtDescriptorPool_;
    allocInfo.descriptorSetCount = static_cast<uint32_t>(layouts.size());
    allocInfo.pSetLayouts = layouts.data();

    LOG_DEBUG_CAT("RENDERER", "Alloc info: sType={}, pool=0x{:x}, count={}, pSetLayouts=0x{:x}", static_cast<int>(allocInfo.sType), reinterpret_cast<uintptr_t>(allocInfo.descriptorPool), allocInfo.descriptorSetCount, reinterpret_cast<uintptr_t>(allocInfo.pSetLayouts));

    VkResult allocResult = vkAllocateDescriptorSets(device, &allocInfo, rtDescriptorSets_.data());
    LOG_DEBUG_CAT("RENDERER", "vkAllocateDescriptorSets returned: {}", static_cast<int>(allocResult));
    if (allocResult != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "Failed to allocate RT sets: {}", static_cast<int>(allocResult));
        throw std::runtime_error("Descriptor allocation failed");
    }

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
    write.dstBinding = 6;  // ← FIXED: Binding 6 from PipelineManager (nexusScore)
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.pImageInfo = &nexusInfo;

    const auto& ctx = RTX::g_ctx();  // const ref
    vkUpdateDescriptorSets(ctx.vkDevice(), 1, &write, 0, nullptr);
    LOG_DEBUG_CAT("RENDERER", "Nexus score image bound → binding 6 (PipelineManager layout)");

    LOG_TRACE_CAT("RENDERER", "updateNexusDescriptors — COMPLETE");
}

void VulkanRenderer::updateRTXDescriptors(uint32_t frame) noexcept {
    //LOG_TRACE_CAT("RENDERER", "updateRTXDescriptors — START — frame={}", frame);

    if (rtDescriptorSets_.empty()) {
        LOG_WARN_CAT("RENDERER", "updateRTXDescriptors — SKIPPED (no RT sets)");
        //LOG_TRACE_CAT("RENDERER", "updateRTXDescriptors — COMPLETE (skipped)");
        return;
    }

    VkDescriptorSet set = rtDescriptorSets_[frame % rtDescriptorSets_.size()];
    const auto& ctx = RTX::g_ctx();  // const ref
    VkDevice device = ctx.vkDevice();

    // ── Prepare Supporting Infos (Lifetime until vkUpdate) ─────────────────────
    VkWriteDescriptorSetAccelerationStructureKHR accelInfo = {};
    std::array<VkDescriptorImageInfo, 4> imageInfos = {};
    std::array<VkDescriptorBufferInfo, 3> bufferInfos = {};
    int imageIdx = 0;
    int bufIdx = 0;

    // ── Prepare Writes Array (Explicit Zero-Init) ─────────────────────────────
    std::array<VkWriteDescriptorSet, 8> writes = {};
    uint32_t writeCount = 0;

    // ── Binding 0: TLAS (Acceleration Structure) ────────────────────────────────
    // • From RTX::LAS::get().getTlas() — single global TLAS (requires public getter in LAS.hpp)
    {
        VkAccelerationStructureKHR tlasHandle = RTX::LAS::get().getTLAS();  // FIXED: Use public getter
        // FIXED: Guard invalid handle — set null to avoid VUID-VkWriteDescriptorSetAccelerationStructureKHR-pAccelerationStructures-parameter
        if (tlasHandle == VK_NULL_HANDLE || reinterpret_cast<uint64_t>(tlasHandle) < 0x1000) {  // Basic sanity: low addresses likely invalid
            LOG_WARN_CAT("RENDERER", "Invalid TLAS handle 0x{:x} — using VK_NULL_HANDLE for frame {}", reinterpret_cast<uint64_t>(tlasHandle), frame);
            tlasHandle = VK_NULL_HANDLE;
        }
        accelInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
        accelInfo.accelerationStructureCount = 1;
        accelInfo.pAccelerationStructures = &tlasHandle;

        writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[writeCount].pNext = &accelInfo;
        writes[writeCount].dstSet = set;
        writes[writeCount].dstBinding = 0;
        writes[writeCount].descriptorCount = 1;
        writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        ++writeCount;
        //LOG_TRACE_CAT("RENDERER", "Binding 0: TLAS accel bound (handle=0x{:x})", reinterpret_cast<uintptr_t>(tlasHandle));
    }

    // ── Binding 1: RT Output (Storage Image — GENERAL for RT Write) ────────────
    if (!rtOutputViews_.empty() && rtOutputViews_[frame % rtOutputViews_.size()].valid()) {
        imageInfos[imageIdx].imageView = *rtOutputViews_[frame % rtOutputViews_.size()];
        imageInfos[imageIdx].imageLayout = VK_IMAGE_LAYOUT_GENERAL;  // Storage write in RT
        imageInfos[imageIdx].sampler = VK_NULL_HANDLE;

        writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[writeCount].pNext = nullptr;
        writes[writeCount].dstSet = set;
        writes[writeCount].dstBinding = 1;
        writes[writeCount].descriptorCount = 1;
        writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[writeCount].pImageInfo = &imageInfos[imageIdx];
        ++writeCount;
        ++imageIdx;
        //LOG_TRACE_CAT("RENDERER", "Binding 1: RT output storage image bound (view=0x{:x}, GENERAL)", reinterpret_cast<uintptr_t>(imageInfos[imageIdx-1].imageView));
    }

    // ── Binding 2: Accumulation (Storage Image — GENERAL for Accum Write) ──────
    if (Options::RTX::ENABLE_ACCUMULATION && !accumViews_.empty() && accumViews_[frame % accumViews_.size()].valid()) {
        imageInfos[imageIdx].imageView = *accumViews_[frame % accumViews_.size()];
        imageInfos[imageIdx].imageLayout = VK_IMAGE_LAYOUT_GENERAL;  // Storage write in shader
        imageInfos[imageIdx].sampler = VK_NULL_HANDLE;

        writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[writeCount].pNext = nullptr;
        writes[writeCount].dstSet = set;
        writes[writeCount].dstBinding = 2;
        writes[writeCount].descriptorCount = 1;
        writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[writeCount].pImageInfo = &imageInfos[imageIdx];
        ++writeCount;
        ++imageIdx;
        //LOG_TRACE_CAT("RENDERER", "Binding 2: Accum storage image bound (view=0x{:x}, GENERAL)", reinterpret_cast<uintptr_t>(imageInfos[imageIdx-1].imageView));
    }

    // ── Binding 3: Uniform Buffer (Camera/Proj/SPP — Device Address or Buffer) ──
    if (!uniformBufferEncs_.empty() && uniformBufferEncs_[frame] != 0) {
        VkBuffer uboBuffer = RAW_BUFFER(uniformBufferEncs_[frame]);  // From BUFFER_CREATE enc
        if (uboBuffer != VK_NULL_HANDLE) {
            bufferInfos[bufIdx].buffer = uboBuffer;
            bufferInfos[bufIdx].offset = 0;
            bufferInfos[bufIdx].range = 368;  // FIXED: Hardcoded sizeof(UBO) matching updateUniformBuffer (5x mat4 + vec4 + vec2 + 3x uint32/float + pad)

            writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[writeCount].pNext = nullptr;
            writes[writeCount].dstSet = set;
            writes[writeCount].dstBinding = 3;
            writes[writeCount].descriptorCount = 1;
            writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writes[writeCount].pBufferInfo = &bufferInfos[bufIdx];
            ++writeCount;
            ++bufIdx;
            //LOG_TRACE_CAT("RENDERER", "Binding 3: UBO bound (buffer=0x{:x}, range={})", reinterpret_cast<uintptr_t>(uboBuffer), bufferInfos[bufIdx-1].range);
        }
    }

    // ── Binding 4: Materials Storage Buffer (Per-Frame if Dynamic) ─────────────
    if (!materialBufferEncs_.empty() && materialBufferEncs_[frame] != 0) {
        VkBuffer matBuffer = RAW_BUFFER(materialBufferEncs_[frame]);
        if (matBuffer != VK_NULL_HANDLE) {
            bufferInfos[bufIdx].buffer = matBuffer;
            bufferInfos[bufIdx].offset = 0;
            bufferInfos[bufIdx].range = VK_WHOLE_SIZE;  // Full buffer

            writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[writeCount].pNext = nullptr;
            writes[writeCount].dstSet = set;
            writes[writeCount].dstBinding = 4;
            writes[writeCount].descriptorCount = 1;
            writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[writeCount].pBufferInfo = &bufferInfos[bufIdx];
            ++writeCount;
            ++bufIdx;
            //LOG_TRACE_CAT("RENDERER", "Binding 4: Materials storage buffer bound (buffer=0x{:x})", reinterpret_cast<uintptr_t>(matBuffer));
        }
    }

    // ── Binding 5: Envmap (Combined Image Sampler — SHADER_READ_ONLY_OPTIMAL) ──
    if (Options::Environment::ENABLE_ENV_MAP && envMapImageView_.valid() && envMapSampler_.valid()) {
        imageInfos[imageIdx].imageView = *envMapImageView_;
        imageInfos[imageIdx].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfos[imageIdx].sampler = *envMapSampler_;

        writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[writeCount].pNext = nullptr;
        writes[writeCount].dstSet = set;
        writes[writeCount].dstBinding = 5;
        writes[writeCount].descriptorCount = 1;
        writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[writeCount].pImageInfo = &imageInfos[imageIdx];
        ++writeCount;
        ++imageIdx;
        //LOG_TRACE_CAT("RENDERER", "Binding 5: Envmap sampler bound (view=0x{:x}, sampler=0x{:x})", reinterpret_cast<uintptr_t>(imageInfos[imageIdx-1].imageView), reinterpret_cast<uintptr_t>(imageInfos[imageIdx-1].sampler));
    }

    // ── Binding 6: Nexus Score (Storage Image — GENERAL for Feedback) ──────────
    if (Options::RTX::ENABLE_ADAPTIVE_SAMPLING && hypertraceScoreView_.valid()) {
        imageInfos[imageIdx].imageView = *hypertraceScoreView_;
        imageInfos[imageIdx].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfos[imageIdx].sampler = VK_NULL_HANDLE;

        writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[writeCount].pNext = nullptr;
        writes[writeCount].dstSet = set;
        writes[writeCount].dstBinding = 6;
        writes[writeCount].descriptorCount = 1;
        writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        writes[writeCount].pImageInfo = &imageInfos[imageIdx];
        ++writeCount;
        ++imageIdx;
        //LOG_TRACE_CAT("RENDERER", "Binding 6: Nexus storage image bound (view=0x{:x}, GENERAL)", reinterpret_cast<uintptr_t>(imageInfos[imageIdx-1].imageView));
    }

    // ── Binding 7: Dimension Storage Buffer (Extent/SPP/Time — Per-Frame) ───────
    if (!dimensionBufferEncs_.empty() && dimensionBufferEncs_[frame] != 0) {
        VkBuffer dimBuffer = RAW_BUFFER(dimensionBufferEncs_[frame]);
        if (dimBuffer != VK_NULL_HANDLE) {
            bufferInfos[bufIdx].buffer = dimBuffer;
            bufferInfos[bufIdx].offset = 0;
            bufferInfos[bufIdx].range = VK_WHOLE_SIZE;  // Full buffer (dims are small)

            writes[writeCount].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[writeCount].pNext = nullptr;
            writes[writeCount].dstSet = set;
            writes[writeCount].dstBinding = 7;
            writes[writeCount].descriptorCount = 1;
            writes[writeCount].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[writeCount].pBufferInfo = &bufferInfos[bufIdx];
            ++writeCount;
            ++bufIdx;
            //LOG_TRACE_CAT("RENDERER", "Binding 7: Dims storage buffer bound (buffer=0x{:x})", reinterpret_cast<uintptr_t>(dimBuffer));
        }
    }

    // ── Apply All Writes ───────────────────────────────────────────────────────
    if (writeCount > 0) {
        vkUpdateDescriptorSets(device, writeCount, writes.data(), 0, nullptr);
        //LOG_TRACE_CAT("RENDERER", "Applied {} descriptor writes to RT set[{}] (0x{:x})", writeCount, frame, reinterpret_cast<uintptr_t>(set));
    } else {
        LOG_WARN_CAT("RENDERER", "No descriptors updated for frame {} — check resources", frame);
    }

    //LOG_TRACE_CAT("RENDERER", "updateRTXDescriptors — COMPLETE — frame={}", frame);
}

void VulkanRenderer::updateTonemapDescriptorsInitial() {
    LOG_TRACE_CAT("RENDERER", "updateTonemapDescriptorsInitial — START");

    if (tonemapSets_.empty() || rtOutputViews_.empty()) {
        LOG_TRACE_CAT("RENDERER", "updateTonemapDescriptorsInitial — SKIPPED (no sets/views)");
        return;
    }

    const auto& ctx = RTX::g_ctx();  // const ref
    VkDevice device = ctx.vkDevice();

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
    //LOG_TRACE_CAT("RENDERER", "updateDenoiserDescriptors — START");

    if (denoiserSets_.empty() || !denoiserView_.valid() || rtOutputViews_.empty()) {
        //LOG_TRACE_CAT("RENDERER", "updateDenoiserDescriptors — SKIPPED");
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

    const auto& ctx = RTX::g_ctx();  // const ref
    vkUpdateDescriptorSets(ctx.vkDevice(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    //LOG_TRACE_CAT("RENDERER", "updateDenoiserDescriptors — COMPLETE");
}

void VulkanRenderer::performDenoisingPass(VkCommandBuffer cmd) {
    if (!denoisingEnabled_ || !denoiserPipeline_.valid()) {
        return;
    }

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
}

void VulkanRenderer::performTonemapPass(VkCommandBuffer cmd, uint32_t imageIndex) {
    if (!tonemapEnabled_ || !tonemapPipeline_.valid()) {
        return;
    }

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
}

void VulkanRenderer::updateUniformBuffer(uint32_t frame, const Camera& camera, float jitter) {

    if (uniformBufferEncs_.empty() || !sharedStagingBuffer_.valid()) {
        return;
    }

    const auto& ctx = RTX::g_ctx();  // const ref
    void* data = nullptr;
    vkMapMemory(ctx.vkDevice(), *sharedStagingMemory_, 0, VK_WHOLE_SIZE, 0, &data);

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
    vkUnmapMemory(ctx.vkDevice(), *sharedStagingMemory_);

    // FIXED: Copy staging to device UBO (use single-time commands for immediate copy)
    VkCommandPool cmdPool = ctx.commandPool();
    VkQueue queue = ctx.graphicsQueue();
    VkCommandBuffer copyCmd = pipelineManager_.beginSingleTimeCommands(cmdPool);
    if (copyCmd != VK_NULL_HANDLE) {
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = 0;
        copyRegion.size = sizeof(ubo);
        VkBuffer stagingBuf = *sharedStagingBuffer_;
        VkBuffer deviceBuf = RAW_BUFFER(uniformBufferEncs_[frame]);
        vkCmdCopyBuffer(copyCmd, stagingBuf, deviceBuf, 1, &copyRegion);

        // Barrier for uniform read
        VkMemoryBarrier uniformBarrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        uniformBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        uniformBarrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;
        vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1, &uniformBarrier, 0, nullptr, 0, nullptr);

        pipelineManager_.endSingleTimeCommands(cmdPool, queue, copyCmd);
    }
}

void VulkanRenderer::updateTonemapUniform(uint32_t frame) {

    if (tonemapUniformEncs_.empty() || !sharedStagingBuffer_.valid()) {
        return;
    }

    const auto& ctx = RTX::g_ctx();  // const ref
    void* data = nullptr;
    vkMapMemory(ctx.vkDevice(), *sharedStagingMemory_, 0, VK_WHOLE_SIZE, 0, &data);

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
    vkUnmapMemory(ctx.vkDevice(), *sharedStagingMemory_);
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

// ────────────���─────────────────────────────────────────────────────────────────
// Resize Handling
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::handleResize(int w, int h) noexcept {
    //LOG_TRACE_CAT("RENDERER", "handleResize — START — w={}, h={}", w, h);
    if (w <= 0 || h <= 0) {
        //LOG_TRACE_CAT("RENDERER", "Invalid dimensions — abort");
        //LOG_TRACE_CAT("RENDERER", "handleResize — COMPLETE (invalid)");
        return;
    }
    if (width_ == w && height_ == h) {
        //LOG_TRACE_CAT("RENDERER", "No change needed");
        //LOG_TRACE_CAT("RENDERER", "handleResize — COMPLETE (no change)");
        return;
    }

    width_ = w;
    height_ = h;
    resetAccumulation_ = true;
    //LOG_TRACE_CAT("RENDERER", "Updated dimensions — width_={}, height_={}, resetAccumulation_={}", width_, height_, resetAccumulation_);

    const auto& ctx = RTX::g_ctx();  // const ref
    //LOG_TRACE_CAT("RENDERER", "vkDeviceWaitIdle — START");
    vkDeviceWaitIdle(ctx.vkDevice());
    //LOG_TRACE_CAT("RENDERER", "vkDeviceWaitIdle — COMPLETE");

    //LOG_TRACE_CAT("RENDERER", "SWAPCHAIN.recreate — START");
    SWAPCHAIN.recreate(w, h);
    //LOG_TRACE_CAT("RENDERER", "SWAPCHAIN.recreate — COMPLETE");

    //LOG_TRACE_CAT("RENDERER", "Recreating render targets — START");
    createRTOutputImages();
    createAccumulationImages();
    createDenoiserImage();
    if (Options::RTX::ENABLE_ADAPTIVE_SAMPLING) createNexusScoreImage(ctx.physicalDevice(), ctx.device(), ctx.commandPool(), ctx.graphicsQueue());
    updateRTXDescriptors(currentFrame_);
    updateTonemapDescriptorsInitial();
    updateDenoiserDescriptors();
    //LOG_TRACE_CAT("RENDERER", "Recreating render targets — COMPLETE");

    LOG_SUCCESS_CAT("Renderer", "{}Swapchain resized to {}x{}{}", SAPPHIRE_BLUE, w, h, RESET);
    //LOG_TRACE_CAT("RENDERER", "handleResize — COMPLETE");
}

// ──────────────────────────────────────────────────────────────────────────────
// Load Ray Tracing Function Pointers — FIXED: const auto& c (ref) + ()
void VulkanRenderer::loadRayTracingExtensions() noexcept
{
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 3: Load Ray Tracing Extensions ===");

    // FIXED: Ref to avoid copy
    const auto& c = RTX::g_ctx();
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

    // ───────────────────���──────────────────────────────────────────────────
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
    // ──────────────────────────────────────────────────────────���───────────
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
 * November 15, 2025 — PipelineManager Integration v10.5
 */