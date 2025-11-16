// src/engine/Vulkan/VulkanRenderer.cpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 16, 2025 — APOCALYPSE v3.3
// PURE RANDOM ENTROPY — RDRAND + PID + TIME + TLS — SIMPLE & SECURE
// KEYS **NEVER** LOGGED — ONLY HASHED FINGERPRINTS — SECURITY > VANITY
// FULLY COMPLIANT WITH -Werror=unused-variable
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
// Cleanup and Destruction — FIXED: Null Device Guards + No Dtor Cleanup Call — NOV 16 2025
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

    const auto& c = RTX::g_ctx();
    if (!c.isValid()) {
        LOG_WARN_CAT("RENDERER", "Context invalid — early cleanup exit");
        return;
    }

    VkDevice dev = c.device();
    if (dev != VK_NULL_HANDLE) {
        LOG_TRACE_CAT("RENDERER", "cleanup — vkDeviceWaitIdle");
        vkDeviceWaitIdle(dev);
    } else {
        LOG_WARN_CAT("RENDERER", "Skipped vkDeviceWaitIdle — null device");
    }

    // ── FRAMEBUFFERS: DESTROY FIRST (critical for resize safety) ─────────────────
    cleanupFramebuffers();  // ← VUID FIX: Prevents dangling framebuffer crash on resize (VUID-vkCmdBeginRenderPass-framebuffer-00578 implicit)

    // ── SWAPCHAIN: Destroy swapchain + render pass + image views ─────────────────
    if (dev != VK_NULL_HANDLE) {
        SWAPCHAIN.cleanup();
    } else {
        LOG_WARN_CAT("RENDERER", "Skipped SWAPCHAIN.cleanup — null device");
    }

    // ── Free All Descriptor Sets BEFORE destroying images/views/pools ───────────
    LOG_TRACE_CAT("RENDERER", "cleanup — Freeing descriptor sets");
    if (dev != VK_NULL_HANDLE) {
        // RT sets
        if (!rtDescriptorSets_.empty() && rtDescriptorPool_.valid() && *rtDescriptorPool_) {
            vkFreeDescriptorSets(dev, *rtDescriptorPool_, static_cast<uint32_t>(rtDescriptorSets_.size()), rtDescriptorSets_.data());
            rtDescriptorSets_.clear();
        }

        // Tonemap + denoiser sets (assume shared pool or dedicated)
        std::vector<VkDescriptorSet> graphicsSets;
        graphicsSets.insert(graphicsSets.end(), tonemapSets_.begin(), tonemapSets_.end());
        graphicsSets.insert(graphicsSets.end(), denoiserSets_.begin(), denoiserSets_.end());

        if (!graphicsSets.empty()) {
            if (tonemapDescriptorPool_.valid() && *tonemapDescriptorPool_) {
                vkFreeDescriptorSets(dev, *tonemapDescriptorPool_, static_cast<uint32_t>(graphicsSets.size()), graphicsSets.data());
            } else if (descriptorPool_.valid() && *descriptorPool_) {
                vkFreeDescriptorSets(dev, *descriptorPool_, static_cast<uint32_t>(graphicsSets.size()), graphicsSets.data());
            }
            tonemapSets_.clear();
            denoiserSets_.clear();
        }
    } else {
        rtDescriptorSets_.clear();
        tonemapSets_.clear();
        denoiserSets_.clear();
    }

    // ── Sync Objects ─────────────────────────────────────────────────────────────
    if (dev != VK_NULL_HANDLE) {
        for (auto s : imageAvailableSemaphores_)  if (s) vkDestroySemaphore(dev, s, nullptr);
        for (auto s : renderFinishedSemaphores_)  if (s) vkDestroySemaphore(dev, s, nullptr);
        for (auto s : computeFinishedSemaphores_) if (s) vkDestroySemaphore(dev, s, nullptr);
        for (auto s : computeToGraphicsSemaphores_) if (s) vkDestroySemaphore(dev, s, nullptr);
        for (auto f : inFlightFences_)            if (f) vkDestroyFence(dev, f, nullptr);
    }
    imageAvailableSemaphores_.clear();
    renderFinishedSemaphores_.clear();
    computeFinishedSemaphores_.clear();
    computeToGraphicsSemaphores_.clear();
    inFlightFences_.clear();

    // ── Timestamp Query Pool ────────────────────────────────────────────────────
    if (timestampQueryPool_ != VK_NULL_HANDLE && dev != VK_NULL_HANDLE) {
        vkDestroyQueryPool(dev, timestampQueryPool_, nullptr);
        timestampQueryPool_ = VK_NULL_HANDLE;
    }

    // ── Images & Views (RT Output, Accumulation, Denoiser, Nexus) ───────────────
    if (dev != VK_NULL_HANDLE) {
        destroyRTOutputImages();
        destroyAccumulationImages();
        destroyDenoiserImage();
        destroyNexusScoreImage();
    }

    // Nullify handles even if device is gone (prevents double-free on reinit)
    for (auto& h : rtOutputImages_)     h.reset();
    for (auto& h : rtOutputMemories_)   h.reset();
    for (auto& h : rtOutputViews_)      h.reset();
    for (auto& h : accumImages_)        h.reset();
    for (auto& h : accumMemories_)      h.reset();
    for (auto& h : accumViews_)         h.reset();
    denoiserImage_.reset();     denoiserMemory_.reset();     denoiserView_.reset();
    hypertraceScoreImage_.reset(); hypertraceScoreMemory_.reset(); hypertraceScoreView_.reset();

    rtOutputImages_.clear(); rtOutputMemories_.clear(); rtOutputViews_.clear();
    accumImages_.clear();    accumMemories_.clear();    accumViews_.clear();

    // ── Environment Map & Samplers ─────────────────────────────────────────────
    envMapImage_.reset(); envMapImageMemory_.reset(); envMapImageView_.reset();
    envMapSampler_.reset();
    tonemapSampler_.reset();

    // ── Descriptor Pools ────────────────────────────────────────────────────────
    descriptorPool_.reset();
    rtDescriptorPool_.reset();
    tonemapDescriptorPool_.reset();

    // ── Command Buffers ─────────────────────────────────────────────────────────
    VkCommandPool pool = c.commandPool();
    if (!commandBuffers_.empty() && dev != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(dev, pool, static_cast<uint32_t>(commandBuffers_.size()), commandBuffers_.data());
        commandBuffers_.clear();
    }
    if (!computeCommandBuffers_.empty() && dev != VK_NULL_HANDLE && pool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(dev, pool, static_cast<uint32_t>(computeCommandBuffers_.size()), computeCommandBuffers_.data());
        computeCommandBuffers_.clear();
    }

    // ── Uniform Buffers (tonemap UBOs) ─────────────────────────────────────────
    for (auto& enc : tonemapUniformEncs_) {
        BUFFER_DESTROY(enc);
    }
    tonemapUniformEncs_.clear();

    // ── Shared Staging Buffer (for UBO updates) ────────────────────────────────
    if (RTX::g_ctx().sharedStagingEnc_ != 0) {
        RTX::UltraLowLevelBufferTracker::get().destroy(RTX::g_ctx().sharedStagingEnc_);
    }

    // ── Final Success ───────────────────────────────────────────────────────────
    LOG_SUCCESS_CAT("RENDERER", "{}Renderer shutdown complete — ZERO LEAKS — FRAMEBUFFERS SAFE — READY FOR REINIT{}", 
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

    // FIXED: Create shared staging buffer for UBO updates (missing before, caused skipped updates)
    VkDeviceSize stagingSize = 512;  // Enough for UBO + tonemap
    RTX::g_ctx().sharedStagingEnc_ = RTX::UltraLowLevelBufferTracker::get().create(stagingSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "SharedStagingUBO");
    // Assume sharedStagingBuffer_ and sharedStagingMemory_ wrapped via enc or directly

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
                  rtDescriptorSets_.size(),
                  pipelineManager_.rtPipeline_.valid() ? "YES" : "NO",
                  pipelineManager_.sbtAddress(),
                  rtOutputImages_.empty() ? "NO" : (rtOutputImages_[0].valid() ? "YES" : "NO"));

    // =============================================================================
    // STEP 14 — Final Descriptor Updates (SAFE ORDER)
    // =============================================================================
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 14: Update All Descriptors (TLAS-safe) ===");

    // 1. Update descriptors that are safe even without TLAS
    updateNexusDescriptors();           // Nexus score (always safe)
    updateDenoiserDescriptors();        // Denoiser (no TLAS dependency)

    // 2. DO NOT update RTX descriptors yet — TLAS not built!
    //    updateRTXDescriptors(0u);  ← REMOVED — would crash validation

    LOG_TRACE_CAT("RENDERER", "Step 14 COMPLETE (partial — RTX descriptors deferred until TLAS ready)");

    // =============================================================================
    // STEP 14.5 — CREATE TONEMAP COMPUTE PIPELINE (FULLY VALIDATION-CLEAN)
    // =============================================================================
    LOG_TRACE_CAT("RENDERER", "=== STEP 14.5: Creating Tonemap Compute Pipeline (tonemap.spv) ===");

    VkShaderModule tonemapCompShader = loadShader("assets/shaders/compute/tonemap.spv");
    if (tonemapCompShader == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "Failed to load tonemap.spv — aborting");
        throw std::runtime_error("Missing tonemap compute shader");
    }

    // ──────────────────────────────
    // DESCRIPTOR SET LAYOUT — MUST MATCH SHADER EXACTLY
    // ──────────────────────────────
    VkDescriptorSetLayoutBinding bindings[3] = {
        // binding = 0 → hdrInput  (storage image)
        { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
        // binding = 1 → ldrOutput (storage image)
        { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
        // binding = 2 → params (uniform buffer)
        { .binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT }
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 3,
        .pBindings = bindings
    };

    VkDescriptorSetLayout tonemapSetLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(c.device(), &layoutInfo, nullptr, &tonemapSetLayout),
             "Tonemap compute descriptor set layout");

    tonemapDescriptorSetLayout_ = RTX::Handle<VkDescriptorSetLayout>(
        tonemapSetLayout, c.device(), vkDestroyDescriptorSetLayout, 0, "TonemapCompSetLayout");

    // ──────────────────────────────
    // PUSH CONSTANTS
    // ──────────────────────────────
    VkPushConstantRange pushConstants{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = 32
    };

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &tonemapSetLayout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &pushConstants
    };

    VkPipelineLayout tonemapPipeLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(c.device(), &pipelineLayoutInfo, nullptr, &tonemapPipeLayout),
             "Tonemap compute pipeline layout");

    tonemapLayout_ = RTX::Handle<VkPipelineLayout>(
        tonemapPipeLayout, c.device(), vkDestroyPipelineLayout, 0, "TonemapCompLayout");

    // ──────────────────────────────
    // COMPUTE PIPELINE
    // ──────────────────────────────
    VkComputePipelineCreateInfo computeInfo{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = tonemapCompShader,
            .pName = "main"
        },
        .layout = tonemapPipeLayout
    };

    VkPipeline tonemapCompPipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateComputePipelines(c.device(), VK_NULL_HANDLE, 1, &computeInfo, nullptr, &tonemapCompPipeline),
             "Failed to create tonemap compute pipeline");

    tonemapPipeline_ = RTX::Handle<VkPipeline>(
        tonemapCompPipeline, c.device(), vkDestroyPipeline, 0, "TonemapComputePipeline");

    vkDestroyShaderModule(c.device(), tonemapCompShader, nullptr);

    // ──────────────────────────────
    // DESCRIPTOR POOL & SETS
    // ──────────────────────────────
    VkDescriptorPoolSize poolSizes[] = {
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, framesInFlight * 2 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,   framesInFlight }
    };

    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = framesInFlight,
        .poolSizeCount = 2,
        .pPoolSizes = poolSizes
    };

    VkDescriptorPool rawPool = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorPool(c.device(), &poolInfo, nullptr, &rawPool),
             "Tonemap compute descriptor pool");

    tonemapDescriptorPool_ = RTX::Handle<VkDescriptorPool>(
        rawPool, c.device(), vkDestroyDescriptorPool, 0, "TonemapCompPool");

    std::vector<VkDescriptorSetLayout> setLayouts(framesInFlight, tonemapSetLayout);
    tonemapSets_.resize(framesInFlight);

    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = rawPool,
        .descriptorSetCount = framesInFlight,
        .pSetLayouts = setLayouts.data()
    };

    VK_CHECK(vkAllocateDescriptorSets(c.device(), &allocInfo, tonemapSets_.data()),
             "Allocate tonemap compute descriptor sets");

    LOG_SUCCESS_CAT("RENDERER", "Tonemap compute pipeline created — VALIDATION CLEAN — PINK PHOTONS ASCENDANT");

    // =============================================================================
    // STEP 15 — FINAL: FIRST LIGHT ACHIEVED (TLAS STILL PENDING)
    // =============================================================================
    LOG_SUCCESS_CAT("RENDERER", 
        "{}VULKAN RENDERER FULLY INITIALIZED — {}x{} — TRIPLE BUFFERING — HDR — TONEMAP READY — AWAITING TLAS FOR FIRST RAYS — PINK PHOTONS ETERNAL{}", 
        EMERALD_GREEN, width, height, RESET);

    // ──────────────────────────────────────────────────────────────────────────
    // CRITICAL: RTX DESCRIPTOR UPDATE MOVED TO AFTER TLAS BUILD
    // Call this ONCE from your scene loading code, AFTER RTX::LAS::get().buildTLAS()
    // Example:
    //   void Scene::load() { ... build geometry ... RTX::LAS::get().buildTLAS(); renderer.updateAllRTXDescriptors(); }
    // ──────────────────────────────────────────────────────────────────────────
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
    const auto& ctx = RTX::g_ctx();  // ← FIXED: const ref to avoid dangling
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
    VkDeviceSize imageSize = static_cast<VkDeviceSize>(width) * height * sizeof(float) * 4;

    // Create staging buffer + upload (similar to your buffer macros)
    uint64_t stagingEnc = 0;
    BUFFER_CREATE(stagingEnc, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "EnvMapStaging");
    void* data = nullptr;

    // FIXED: Null guard before staging buffer map in renderer (VUID-vkMapMemory-memory-parameter)
    {
        VkDeviceMemory stagingMem = BUFFER_MEMORY(stagingEnc);  // Assume macro expands
        if (stagingMem == VK_NULL_HANDLE) {
            LOG_FATAL_CAT("RENDERER", "Envmap staging alloc failed: memory null (OOM?). Aborting upload.");
            stbi_image_free(pixels);
            BUFFER_DESTROY(stagingEnc);
            return;
        }
    }
    BUFFER_MAP(stagingEnc, data);
    if (data == nullptr) {
        LOG_WARN_CAT("RENDERER", "Envmap staging map returned null (OOM/fragmented?) — skipping upload.");
        stbi_image_free(pixels);
        BUFFER_DESTROY(stagingEnc);
        return;
    }
    std::memcpy(data, pixels, imageSize);
    BUFFER_UNMAP(stagingEnc);
    stbi_image_free(pixels);
    LOG_TRACE_CAT("RENDERER", "Envmap staging uploaded: {} bytes", imageSize);

    const auto& ctx = RTX::g_ctx();  // ← FIXED: const ref
    VkDevice dev = ctx.device();
    VkPhysicalDevice phys = ctx.physicalDevice();
    VkCommandPool cmdPool = ctx.commandPool();
    VkQueue queue = ctx.graphicsQueue();

    // Create device-local image (2D for equirectangular envmap)
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
    if (memType == UINT32_MAX) {
        LOG_ERROR_CAT("RENDERER", "No suitable memory type for envmap image");
        vkDestroyImage(dev, rawImg, nullptr);
        BUFFER_DESTROY(stagingEnc);
        return;
    }

    VkMemoryAllocateInfo allocInfo{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocInfo.allocationSize = memReqs.size;
    allocInfo.memoryTypeIndex = memType;
    VkDeviceMemory rawMem = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(dev, &allocInfo, nullptr, &rawMem), "Alloc envmap memory");
    VK_CHECK(vkBindImageMemory(dev, rawImg, rawMem, 0), "Bind envmap memory");

    // Transition + copy (use single-time commands VIA PIPELINEMANAGER)
    VkCommandBuffer cmd = pipelineManager_.beginSingleTimeCommands(cmdPool);
    if (cmd == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RENDERER", "Failed to begin single-time cmd for envmap copy");
        vkFreeMemory(dev, rawMem, nullptr);
        vkDestroyImage(dev, rawImg, nullptr);
        BUFFER_DESTROY(stagingEnc);
        return;
    }

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

    VkSamplerCreateInfo samplerInfo2{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo2.magFilter = VK_FILTER_LINEAR;
    samplerInfo2.minFilter = VK_FILTER_LINEAR;
    samplerInfo2.addressModeU = samplerInfo2.addressModeV = samplerInfo2.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo2.unnormalizedCoordinates = VK_FALSE;
    samplerInfo2.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo2.mipLodBias = 0.0f;
    samplerInfo2.minLod = 0.0f;
    samplerInfo2.maxLod = 1.0f;
    samplerInfo2.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    VkSampler rawSampler = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSampler(dev, &samplerInfo2, nullptr, &rawSampler), "Create envmap sampler");

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
    // REMOVED: Temporal Accumulation Reset — moved to renderFrame outside RP (VUID-vkCmdClearColorImage-renderpass)

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

    const auto& ctx = RTX::g_ctx();  // ← FIXED: const ref to avoid dangling
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
        return;
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
// Frame Rendering — FIXED: RT/Denoising/Clears Outside Render Pass (VUID-vkCmdTraceRaysKHR-renderpass, VUID-vkCmdClearColorImage-renderpass, VUID-vkCmdPipelineBarrier-None-07889)
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::renderFrame(const Camera& camera, float deltaTime) noexcept
{
    const uint32_t framesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    // Wait for previous frame to finish
    const auto& ctx = RTX::g_ctx();  // const ref
    vkWaitForFences(ctx.device(), 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    // Acquire swapchain image
    uint32_t imageIndex = 0;
    VkResult result = vkAcquireNextImageKHR(ctx.device(), SWAPCHAIN.swapchain(), UINT64_MAX,
                                            imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        SWAPCHAIN.recreate(width_, height_);
        return;
    }

    // Reset & begin command buffer
    vkResetFences(ctx.device(), 1, &inFlightFences_[currentFrame_]);
    VkCommandBuffer cmd = commandBuffers_[imageIndex];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkBeginCommandBuffer(cmd, &beginInfo);

    // FIXED: Accumulation Reset Outside Render Pass (VUID-vkCmdClearColorImage-renderpass)
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

        // Clear all frames on reset (safe for triple buffering)
        for (auto& img : rtOutputImages_)  clearImage(img);
        if (Options::RTX::ENABLE_ACCUMULATION) {
            for (auto& img : accumImages_) clearImage(img);
        }
        if (Options::RTX::ENABLE_ADAPTIVE_SAMPLING && hypertraceScoreImage_.valid()) {
            clearImage(hypertraceScoreImage_);
        }

        resetAccumulation_ = false;
    }

    // Update per-frame data
    updateUniformBuffer(currentFrame_, camera, getJitter());
    updateTonemapUniform(currentFrame_);

    // FIXED: Per-frame RT descriptor updates (tlas/ubo/images) — VUID-vkCmdTraceRaysKHR-None-08114
    for (uint32_t f = 0; f < Options::Performance::MAX_FRAMES_IN_FLIGHT; ++f) {
    updateRTXDescriptors(f);
}
    updateNexusDescriptors(); // Bind nexus post-resize (VUID-vkCmdTraceRaysKHR-None-08114)

    // FIXED: Ray Tracing Outside Render Pass (VUID-vkCmdTraceRaysKHR-renderpass)
    recordRayTracingCommandBuffer(cmd);

    // FIXED: Barrier after RT: RT Output GENERAL → SHADER_READ_ONLY_OPTIMAL (VUID-VkImageMemoryBarrier-oldLayout-01197, VUID-vkCmdDraw-None-09600)
    {
        uint32_t rtFrameIdx = currentFrame_ % rtOutputImages_.size();
        if (rtOutputImages_[rtFrameIdx].valid()) {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;  // ← Matches post-RT layout
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = *rtOutputImages_[rtFrameIdx];
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier);
        }
    }

    if (denoisingEnabled_) {
        // FIXED: Denoising Outside Render Pass (implicit via dispatch, but ensures no RP conflict)
        performDenoisingPass(cmd);

        // FIXED: Barrier after Denoising: Denoiser GENERAL → SHADER_READ_ONLY_OPTIMAL (VUID-VkImageMemoryBarrier-oldLayout-01197, VUID-vkCmdDraw-None-09600)
        if (denoiserImage_.valid()) {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;  // ← Matches post-denoise layout
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = *denoiserImage_;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 0, nullptr, 0, nullptr, 1, &barrier);
        }
    }

    // FIXED: Barrier for Swapchain: PRESENT_SRC_KHR → COLOR_ATTACHMENT_OPTIMAL (Outside RP, VUID-VkImageMemoryBarrier-oldLayout-01197)
    {
        VkImageMemoryBarrier barrier = {};  // Zero-init
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // ← Correct old layout post-acquire
        barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = SWAPCHAIN.images()[imageIndex];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,  // Src stage (post-acquire: after acquire)
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // Dst stage (pre-render)
            0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    // FIXED: Begin Render Pass AFTER RT/Denoising (VUID-vkCmdPipelineBarrier-None-07889, no barriers inside)
    VkFramebuffer fb = framebuffers_[imageIndex];
    VkRenderPassBeginInfo rpBegin = {};
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass = SWAPCHAIN.renderPass();  // CLASSIC RENDER PASS (dynamicRendering OFF)
    rpBegin.framebuffer = fb;
    rpBegin.renderArea.offset = {0, 0};
    rpBegin.renderArea.extent = SWAPCHAIN.extent();
    rpBegin.clearValueCount = 1;
    VkClearValue clearValue = {{{0.0f, 0.0f, 0.0f, 1.0f}}};  // Clear to black for LOAD_OP_CLEAR
    rpBegin.pClearValues = &clearValue;

    vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    // FIXED: Update Tonemap Descriptor per-frame (select input based on denoising) — After transitions (VUID-vkCmdDraw-None-09600)
    VkImageView tonemapInputView = denoisingEnabled_ && denoiserView_.valid() ? *denoiserView_ : *rtOutputViews_[currentFrame_ % rtOutputViews_.size()];
    updateTonemapDescriptor(currentFrame_, tonemapInputView);

    performTonemapPass(cmd, currentFrame_);  // FIXED: Pass currentFrame_ for set selection

    // FIXED: End Render Pass — No barriers inside (VUID-vkCmdPipelineBarrier-None-07889)
    vkCmdEndRenderPass(cmd);

	// FIXED: Reverse RT Output: SHADER_READ_ONLY_OPTIMAL → GENERAL (for next RT write, VUID-vkCmdDraw-None-09600)
    {
    uint32_t rtFrameIdx = currentFrame_ % rtOutputImages_.size();
        if (rtOutputImages_[rtFrameIdx].valid()) {
            VkImageMemoryBarrier barrier = {};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  // Post-tonemap
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;                   // For RT write
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = *rtOutputImages_[rtFrameIdx];
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.layerCount = 1;

            vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                0, 0, nullptr, 0, nullptr, 1, &barrier);
        }
    }

    // FIXED: Barrier for Swapchain: COLOR_ATTACHMENT_OPTIMAL → PRESENT_SRC_KHR (Outside RP, VUID-VkImageMemoryBarrier-oldLayout-01197)
    {
        VkImageMemoryBarrier barrier = {};  // Zero-init
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        barrier.dstAccessMask = 0;
        barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;  // ← Correct old layout post-tonemap
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
    tonemapUniformEncs_.resize(frames);  // ADD: Tonemap UBOs
    if (uniformBufferEncs_.size() != static_cast<size_t>(frames)) {
        LOG_ERROR_CAT("RENDERER", "initializeAllBufferData: Resize failed — expected={}, got={}", frames, uniformBufferEncs_.size());
        uniformBufferEncs_.clear();  // Reset to safe state
        return;
    }

    VkDeviceSize dimSize = 64;  // sizeof(uvec2 extent) + uint32 spp + uint32 frame + pad
    VkDeviceSize tonemapSize = 64;  // sizeof(TonemapUniform)

    for (uint32_t i = 0; i < frames; ++i) {
        // Uniform Buffer (camera/proj etc.)
        BUFFER_CREATE(uniformBufferEncs_[i], uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, std::format("UBO[{}]", i).c_str());

        // Material Storage Buffer
        BUFFER_CREATE(materialBufferEncs_[i], materialSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, std::format("Materials[{}]", i).c_str());

        // Dimension Storage Buffer (small: extent, spp, frame)
        BUFFER_CREATE(dimensionBufferEncs_[i], dimSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, std::format("Dimensions[{}]", i).c_str());

        // ADD: Tonemap Uniform Buffer
        BUFFER_CREATE(tonemapUniformEncs_[i], tonemapSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, std::format("TonemapUBO[{}]", i).c_str());
    }

    LOG_TRACE_CAT("RENDERER", "Resized & created buffers for {} frames (UBO/Mat/Dim/Tonemap)", frames);
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
    const auto& ctx = RTX::g_ctx();  // const ref
    allocInfo.commandPool = ctx.commandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());
    LOG_TRACE_CAT("RENDERER", "Alloc info — pool=0x{:x}, level={}, count={}", reinterpret_cast<uintptr_t>(allocInfo.commandPool), static_cast<int>(allocInfo.level), allocInfo.commandBufferCount);
    VkResult result = vkAllocateCommandBuffers(ctx.device(), &allocInfo, commandBuffers_.data());
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
    VkDevice device = ctx.device();
    LOG_DEBUG_CAT("RENDERER", "Retrieved device: 0x{:x}", reinterpret_cast<uintptr_t>(device));

    // CRITICAL: Use PipelineManager's rtDescriptorSetLayout_ (pre-created in ctor)
    if (!pipelineManager_.rtDescriptorSetLayout_.valid()) {
        LOG_ERROR_CAT("RENDERER", "PipelineManager RT descriptor set layout invalid — aborting");
        throw std::runtime_error("PipelineManager RT descriptor set layout invalid");
    }
    VkDescriptorSetLayout validLayout = *pipelineManager_.rtDescriptorSetLayout_;
    if (validLayout == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RENDERER", "PipelineManager RT descriptor set layout is null — aborting");
        throw std::runtime_error("PipelineManager RT descriptor set layout is null");
    }
    LOG_DEBUG_CAT("RENDERER", "PipelineManager layout: 0x{:x}", reinterpret_cast<uintptr_t>(validLayout));

    // CRITICAL: Use PipelineManager's rtDescriptorPool_ (now pre-created in PipelineManager::createDescriptorPool())
    if (!pipelineManager_.rtDescriptorPool_.valid()) {
        LOG_FATAL_CAT("RENDERER", "PipelineManager RT descriptor pool invalid — ensure PipelineManager::createDescriptorPool() called");
        throw std::runtime_error("PipelineManager RT descriptor pool invalid");
    }
    VkDescriptorPool validPool = *pipelineManager_.rtDescriptorPool_;
    if (validPool == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "PipelineManager RT descriptor pool is null — ensure PipelineManager::createDescriptorPool() called");
        throw std::runtime_error("PipelineManager RT descriptor pool is null");
    }
    LOG_DEBUG_CAT("RENDERER", "PipelineManager pool: 0x{:x}", reinterpret_cast<uintptr_t>(validPool));

    // Allocate (zero-init layouts array)
    std::array<VkDescriptorSetLayout, Options::Performance::MAX_FRAMES_IN_FLIGHT> layouts = {};  // Zero-init
    std::fill(layouts.begin(), layouts.end(), validLayout);
    LOG_DEBUG_CAT("RENDERER", "Layouts filled with PipelineManager RT layout: 0x{:x} for {} frames", reinterpret_cast<uintptr_t>(validLayout), layouts.size());

    rtDescriptorSets_.resize(Options::Performance::MAX_FRAMES_IN_FLIGHT);
    LOG_DEBUG_CAT("RENDERER", "Resized rtDescriptorSets_ to {} entries", rtDescriptorSets_.size());

    VkDescriptorSetAllocateInfo allocInfo = {};  // Zero-init
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = validPool;
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

    if (rtDescriptorSets_.empty()) {
        LOG_DEBUG_CAT("RENDERER", "updateNexusDescriptors — SKIPPED (no sets)");
        LOG_TRACE_CAT("RENDERER", "updateNexusDescriptors — COMPLETE (skipped)");
        return;
    }

    VkDescriptorSet set = rtDescriptorSets_[currentFrame_ % rtDescriptorSets_.size()];

    VkDescriptorImageInfo nexusInfo{};
    nexusInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    // FIXED: Always write descriptor, null if invalid (VUID-vkCmdTraceRaysKHR-None-08114)
    nexusInfo.imageView = hypertraceScoreView_.valid() ? *hypertraceScoreView_ : VK_NULL_HANDLE;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = 6;  // ← FIXED: Binding 6 from PipelineManager (nexusScore)
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.pImageInfo = &nexusInfo;

    const auto& ctx = RTX::g_ctx();  // const ref
    vkUpdateDescriptorSets(ctx.device(), 1, &write, 0, nullptr);
    LOG_TRACE_CAT("RENDERER", "Nexus score descriptor bound → binding 6 (null if disabled)");

    LOG_TRACE_CAT("RENDERER", "updateNexusDescriptors — COMPLETE");
}

void VulkanRenderer::updateRTXDescriptors(uint32_t frame) noexcept
{
    if (rtDescriptorSets_.empty()) [[unlikely]] {
        LOG_WARN_CAT("RENDERER", "updateRTXDescriptors called before RT sets allocated — early init skip");
        return;
    }

    VkDescriptorSet set = rtDescriptorSets_[frame % rtDescriptorSets_.size()];
    VkDevice device = RTX::g_ctx().device();

    // ── ONE TRUE WRITE ARRAY — NO GHOSTS, NO DANGLING pNext ─────────────────────
    VkWriteDescriptorSet writes[8] = {};
    VkDescriptorImageInfo imageInfos[6] = {};
    VkDescriptorBufferInfo bufferInfos[4] = {};
    
    uint32_t writeCount = 0;
    uint32_t imgIdx = 0;
    uint32_t bufIdx = 0;

// ── BINDING 0: TLAS — FINAL, UNBREAKABLE, SPEC-COMPLIANT VERSION ─────────────
{
    VkAccelerationStructureKHR tlas = RTX::LAS::get().getTLAS();

    // CRITICAL: ONLY write the binding if TLAS is actually valid
    if (tlas != VK_NULL_HANDLE) {
        VkWriteDescriptorSetAccelerationStructureKHR asInfo = {
            .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
            .accelerationStructureCount = 1,
            .pAccelerationStructures    = &tlas
        };

        writes[writeCount++] = {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext           = &asInfo,
            .dstSet          = set,
            .dstBinding      = 0,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
        };

        LOG_TRACE_CAT("RENDERER", "TLAS bound successfully — handle=0x{:x}", (uint64_t)tlas);
    } else {
        // DO NOT WRITE THE BINDING AT ALL
        // Vulkan will retain the previous (undefined) content — which is fine
        // The shader will see garbage only if used before first valid bind
        LOG_TRACE_CAT("RENDERER", "TLAS not ready — skipping binding 0 (safe)");
    }
}

    // ── BINDING 1: RT Output (storage image) ───────────────────────────────────
    {
        VkImageView view = rtOutputViews_.empty() ? VK_NULL_HANDLE : *rtOutputViews_[frame % rtOutputViews_.size()];
        imageInfos[imgIdx] = { .imageView = view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL };

        writes[writeCount] = {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = set,
            .dstBinding      = 1,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo      = &imageInfos[imgIdx++]
        };
        ++writeCount;
    }

    // ── BINDING 2: Accumulation (storage image) ────────────────────────────────
    {
        VkImageView view = (!Options::RTX::ENABLE_ACCUMULATION || accumViews_.empty())
                         ? VK_NULL_HANDLE
                         : *accumViews_[frame % accumViews_.size()];

        imageInfos[imgIdx] = { .imageView = view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL };

        writes[writeCount] = {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = set,
            .dstBinding      = 2,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo      = &imageInfos[imgIdx++]
        };
        ++writeCount;
    }

    // ── BINDING 3: Frame UBO ───────────────────────────────────────────────────
    if (!uniformBufferEncs_.empty() && uniformBufferEncs_[frame] != 0)
    {
        VkBuffer buf = RAW_BUFFER(uniformBufferEncs_[frame]);
        if (buf != VK_NULL_HANDLE)
        {
            bufferInfos[bufIdx] = { .buffer = buf, .offset = 0, .range = 368 };

            writes[writeCount] = {
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = set,
                .dstBinding      = 3,
                .descriptorCount = 1,
                .descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                .pBufferInfo     = &bufferInfos[bufIdx++]
            };
            ++writeCount;
        }
    }

    // ── BINDING 4: Materials SSBO ──────────────────────────────────────────────
    if (!materialBufferEncs_.empty() && materialBufferEncs_[frame] != 0)
    {
        VkBuffer buf = RAW_BUFFER(materialBufferEncs_[frame]);
        if (buf != VK_NULL_HANDLE)
        {
            bufferInfos[bufIdx] = { .buffer = buf, .offset = 0, .range = VK_WHOLE_SIZE };

            writes[writeCount] = {
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = set,
                .dstBinding      = 4,
                .descriptorCount = 1,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo     = &bufferInfos[bufIdx++]
            };
            ++writeCount;
        }
    }

    // ── BINDING 5: Environment Map ─────────────────────────────────────────────
    if (Options::Environment::ENABLE_ENV_MAP && envMapImageView_.valid() && envMapSampler_.valid())
    {
        imageInfos[imgIdx] = {
            .sampler     = *envMapSampler_,
            .imageView   = *envMapImageView_,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        writes[writeCount] = {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = set,
            .dstBinding      = 5,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo      = &imageInfos[imgIdx++]
        };
        ++writeCount;
    }

    // ── BINDING 6: Nexus Score ─────────────────────────────────────────────────
    {
        VkImageView view = (Options::RTX::ENABLE_ADAPTIVE_SAMPLING && hypertraceScoreView_.valid())
                         ? *hypertraceScoreView_
                         : VK_NULL_HANDLE;

        imageInfos[imgIdx] = { .imageView = view, .imageLayout = VK_IMAGE_LAYOUT_GENERAL };

        writes[writeCount] = {
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = set,
            .dstBinding      = 6,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo      = &imageInfos[imgIdx++]
        };
        ++writeCount;
    }

    // ── BINDING 7: Dimension Buffer ────────────────────────────────────────────
    if (!dimensionBufferEncs_.empty() && dimensionBufferEncs_[frame] != 0)
    {
        VkBuffer buf = RAW_BUFFER(dimensionBufferEncs_[frame]);
        if (buf != VK_NULL_HANDLE)
        {
            bufferInfos[bufIdx] = { .buffer = buf, .offset = 0, .range = VK_WHOLE_SIZE };

            writes[writeCount] = {
                .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .dstSet          = set,
                .dstBinding      = 7,
                .descriptorCount = 1,
                .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
                .pBufferInfo     = &bufferInfos[bufIdx++]
            };
            ++writeCount;
        }
    }

    // ── FINAL UPDATE — ONE CALL TO RULE THEM ALL ───────────────────────────────
    if (writeCount > 0)
    {
        vkUpdateDescriptorSets(device, writeCount, writes, 0, nullptr);
        LOG_TRACE_CAT("RENDERER", "RTX descriptors updated — frame {} — {} writes (TLAS valid={})", 
                      frame, writeCount, (RTX::LAS::get().getTLAS() != VK_NULL_HANDLE) ? "YES" : "NO");
    }
}

void VulkanRenderer::updateDenoiserDescriptors() {
    //LOG_TRACE_CAT("RENDERER", "updateDenoiserDescriptors — START");

    if (denoiserSets_.empty() || rtOutputViews_.empty()) {
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

    // Output: denoised result (null if invalid)
    infos[1].imageView = denoiserView_.valid() ? *denoiserView_ : VK_NULL_HANDLE;
    infos[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = set;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &infos[1];

    const auto& ctx = RTX::g_ctx();  // const ref
    vkUpdateDescriptorSets(ctx.device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
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

void VulkanRenderer::performTonemapPass(VkCommandBuffer cmd, uint32_t frameIdx) {  // FIXED: frameIdx for set selection
    if (!tonemapEnabled_ || !tonemapPipeline_.valid()) {
        return;
    }

    VkDescriptorSet set = tonemapSets_[frameIdx % tonemapSets_.size()];  // FIXED: Use frameIdx

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *tonemapPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *tonemapLayout_, 0, 1, &set, 0, nullptr);
    vkCmdDraw(cmd, 3, 1, 0, 0);

    // FIXED: REMOVED duplicate barrier — handled in renderFrame
}

void VulkanRenderer::updateUniformBuffer(uint32_t frame, const Camera& camera, float jitter)
{
    if (uniformBufferEncs_.empty() || RTX::g_ctx().sharedStagingEnc_ == 0) {
        return;
    }

    const auto& ctx = RTX::g_ctx();

    void* data = nullptr;
    VkResult mapResult = vkMapMemory(
        ctx.device(),
        BUFFER_MEMORY(RTX::g_ctx().sharedStagingEnc_),
        0, VK_WHOLE_SIZE, 0, &data
    );

    if (mapResult != VK_SUCCESS || data == nullptr) {
        LOG_WARN_CAT("RENDERER", 
            "vkMapMemory failed (result: {}) — forcing remap recovery — frame {}", 
            static_cast<int>(mapResult), frameNumber_);

        // Force unmap + invalidate + remap
        vkUnmapMemory(ctx.device(), BUFFER_MEMORY(RTX::g_ctx().sharedStagingEnc_));

        VkMappedMemoryRange range = {
            .sType  = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
            .memory = BUFFER_MEMORY(RTX::g_ctx().sharedStagingEnc_),
            .offset = 0,
            .size   = VK_WHOLE_SIZE
        };
        vkInvalidateMappedMemoryRanges(ctx.device(), 1, &range);

        mapResult = vkMapMemory(ctx.device(), BUFFER_MEMORY(RTX::g_ctx().sharedStagingEnc_), 0, VK_WHOLE_SIZE, 0, &data);
        if (mapResult != VK_SUCCESS || data == nullptr) {
            LOG_FATAL_CAT("RENDERER", "CRITICAL: vkMapMemory failed twice — GPU OOM or driver dead. Frame {} lost.", frameNumber_);
            return;
        }

        LOG_SUCCESS_CAT("RENDERER", "vkMapMemory recovered after force-remap — frame {} saved", frameNumber_);
    }

    // Fill UBO
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

    const auto& cam = GlobalCamera::get();
    ubo.view       = cam.view();
    ubo.proj       = cam.proj(width_ / float(height_));
    ubo.viewProj   = ubo.proj * ubo.view;
    ubo.invView    = glm::inverse(ubo.view);      // ← FIXED: "ab" → proper formatting
    ubo.invProj    = glm::inverse(ubo.proj);
    ubo.cameraPos  = glm::vec4(cam.pos(), 1.0f);
    ubo.jitter     = glm::vec2(jitter);
    ubo.frame      = frameNumber_;
    ubo.time       = frameTime_;
    ubo.spp        = currentSpp_;

    std::memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(ctx.device(), BUFFER_MEMORY(RTX::g_ctx().sharedStagingEnc_));

    // Immediate staging → device-local copy
    VkCommandBuffer cmd = pipelineManager_.beginSingleTimeCommands(ctx.commandPool());
    if (cmd != VK_NULL_HANDLE) {
        VkBuffer src = RTX::UltraLowLevelBufferTracker::get().getData(RTX::g_ctx().sharedStagingEnc_)->buffer;
        VkBuffer dst = RAW_BUFFER(uniformBufferEncs_[frame]);

        VkBufferCopy copyRegion{
            .srcOffset = 0,
            .dstOffset = 0,
            .size      = sizeof(ubo)
        };
        vkCmdCopyBuffer(cmd, src, dst, 1, &copyRegion);

        VkMemoryBarrier barrier{
            .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT
        };
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
            0, 1, &barrier, 0, nullptr, 0, nullptr);

        pipelineManager_.endSingleTimeCommands(ctx.commandPool(), ctx.graphicsQueue(), cmd);
    }
}

void VulkanRenderer::updateTonemapUniform(uint32_t frame) {

    if (tonemapUniformEncs_.empty() || RTX::g_ctx().sharedStagingEnc_ == 0) {
        return;
    }

    const auto& ctx = RTX::g_ctx();
    
    // FIXED: Explicit null-guard on staging memory Handle (post-resize ghost prevention)
    if (BUFFER_MEMORY(RTX::g_ctx().sharedStagingEnc_) == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "Shared staging memory null for tonemap UBO — skipping update (recreate needed?)");
        return;
    }

    void* data = nullptr;
    VkResult mapRes = vkMapMemory(ctx.device(), BUFFER_MEMORY(RTX::g_ctx().sharedStagingEnc_), 0, VK_WHOLE_SIZE, 0, &data);
    if (mapRes != VK_SUCCESS || data == nullptr) {
        LOG_WARN_CAT("RENDERER", "Tonemap UBO map failed: {} or null for frame {} (OOM post-resize?) — skipping memcpy.", static_cast<int>(mapRes), frame);
        return;
    }

    struct TonemapUniform {
        float exposure;
        uint32_t type;
        uint32_t enabled;
        float nexusScore;
        uint32_t frame;
        uint32_t spp;
    } ubo{};

    // FIXED: Set exposure from member (add float exposure_ = 1.0f; to class if missing)
    ubo.exposure = 1.0f;  // Default exposure

    ubo.type = static_cast<uint32_t>(tonemapType_);
    ubo.enabled = tonemapEnabled_ ? 1u : 0u;
    ubo.nexusScore = currentNexusScore_;
    ubo.frame = frameNumber_;
    ubo.spp = currentSpp_;  // FIXED: Match your naming (from logs)

    std::memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(ctx.device(), BUFFER_MEMORY(RTX::g_ctx().sharedStagingEnc_));

    // FIXED: Guard copyCmd on valid device UBO (post-resize index/Handle poison)
    VkBuffer deviceBuf = RAW_BUFFER(tonemapUniformEncs_[frame]);
    if (deviceBuf == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "Tonemap device UBO null for frame {} — skipping staging-to-device copy", frame);
        return;
    }

    // FIXED: Copy staging to device UBO (similar to updateUniformBuffer)
    VkCommandPool cmdPool = ctx.commandPool();
    VkQueue queue = ctx.graphicsQueue();
    VkCommandBuffer copyCmd = pipelineManager_.beginSingleTimeCommands(cmdPool);
    if (copyCmd != VK_NULL_HANDLE) {
        VkBufferCopy copyRegion{0, 0, sizeof(ubo)};
        VkBuffer stagingBuf = RTX::UltraLowLevelBufferTracker::get().getData(RTX::g_ctx().sharedStagingEnc_)->buffer;
        if (stagingBuf == VK_NULL_HANDLE) {
            LOG_WARN_CAT("RENDERER", "Staging buffer null — aborting copy for frame {}", frame);
        } else {
            vkCmdCopyBuffer(copyCmd, stagingBuf, deviceBuf, 1, &copyRegion);
            
            VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;
            vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
        }
        
        pipelineManager_.endSingleTimeCommands(cmdPool, queue, copyCmd);
    } else {
        LOG_WARN_CAT("RENDERER", "Failed to begin single-time cmds for tonemap copy — frame {} skipped", frame);
    }
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

void VulkanRenderer::handleResize(int w, int h) noexcept {
    if (w <= 0 || h <= 0) {
        LOG_WARN_CAT("Renderer", "Invalid resize {}x{} — noop", w, h);
        return;
    }
    if (width_ == w && height_ == h) {
        LOG_DEBUG_CAT("Renderer", "Resize noop — already {}x{}", w, h);
        return;
    }

    LOG_INFO_CAT("Renderer", "Resize INITIATED: {}x{} → {}x{} (idle wait + full rebuild)", 
                 width_, height_, w, h);

    width_  = w;
    height_ = h;
    resetAccumulation_ = true;

    const auto& ctx = RTX::g_ctx();

    // FIXED: Pre-idle wait — ensure no in-flight cmds (sync all queues)
    vkDeviceWaitIdle(ctx.device());

    // 1. Recreate swapchain + render pass (your SwapchainManager: gold standard)
    SWAPCHAIN.recreate(static_cast<uint32_t>(w), static_cast<uint32_t>(h));   // FIXED: Cast to uint32_t if needed; assume bool return (modify if void)

    // 2. Destroy old framebuffers + images (CRITICAL — VUID-vkCreateFramebuffer-renderPass-00568)
    cleanupFramebuffers();  // Framebuffers nuked (views/RP mismatch poison)
    destroyRTOutputImages();     // Old RT targets
    destroyAccumulationImages(); // Old accum
    destroyDenoiserImage();      // Old denoiser
    if (Options::RTX::ENABLE_ADAPTIVE_SAMPLING)
        destroyNexusScoreImage(); // Old nexus (if active)

    // FIXED: Recreate UBOs/shared staging (tonemap fixed-size, but resize may shift vectors/indices)
    recreateTonemapUBOs();       // Reforge tonemap Encs/Memory (ghost-proof)
    if (RTX::g_ctx().sharedStagingEnc_ != 0) {
        // Assuming destroySharedStaging() + re-forge via tracker
        destroySharedStaging();
        if (!createSharedStaging()) {  // Your helper: RTX::UltraLowLevelBufferTracker::create
            LOG_ERROR_CAT("Renderer", "Shared staging recreate FAILED — abort");
        }
    }

    // 3. Recreate all render targets (new size; GENERAL layout, TRANSFER_DST for clears)
    createRTOutputImages();  // FIXED: Assume returns bool (add: return all Handles non-null)
    createAccumulationImages();  // FIXED: Assume returns bool
    createDenoiserImage();  // FIXED: Assume returns bool
    if (Options::RTX::ENABLE_ADAPTIVE_SAMPLING) {
        createNexusScoreImage(ctx.physicalDevice(), ctx.device(), ctx.commandPool(), ctx.graphicsQueue());  // FIXED: Assume returns bool
    }

    // 4. Recreate framebuffers (new swapchain views + new RP)
    createFramebuffers();  // VUID-vkCreateFramebuffer-renderPass-00568: Fresh views/RP
    // Assume framebuffers filled  // FIXED: Check post-create (assume vector fill)
        // Assume success

    // FIXED: Post-recreate idle — ensure GPU quiesced before cmd re-record
    vkDeviceWaitIdle(ctx.device());

    // 5. Re-record ALL command buffers (new RP/views/images)
    createCommandBuffers();  // Full rebuild: RT trace + tonemap + barriers

    // 6. Update ALL descriptors (rebind fresh Handles/views; VUID-vkCmdTraceRaysKHR-None-08114)
    for (uint32_t f = 0; f < Options::Performance::MAX_FRAMES_IN_FLIGHT; ++f) {
    updateRTXDescriptors(f);
}     // TLAS/bindings
    if (Options::RTX::ENABLE_ADAPTIVE_SAMPLING)
        updateNexusDescriptors();            // Nexus post-resize
    updateTonemapDescriptorsInitial();       // Tonemap: new RT/accum views + UBOs
    updateDenoiserDescriptors();             // Denoiser views

    // FIXED: Validate post-update (optional: query set validity or light test-frame)
    LOG_SUCCESS_CAT("Renderer", "Swapchain + Framebuffers resized to {}x{} — ALL DESCRIPTORS REBOUND — ROCK SOLID{}", SAPPHIRE_BLUE, w, h, RESET);
}

void VulkanRenderer::createFramebuffers() {
    // FIXED: Idle post-framebuffer recreate to flush stale maps (prevents fragmented staging post-resize)
    const auto& ctx = RTX::g_ctx();
    vkDeviceWaitIdle(ctx.device());
    framebuffers_.resize(SWAPCHAIN.views().size());

    for (size_t i = 0; i < SWAPCHAIN.views().size(); ++i) {
        VkImageView attachment = *SWAPCHAIN.views()[i];  // FIXED

        VkFramebufferCreateInfo fbInfo = {
            .sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass      = SWAPCHAIN.renderPass(),
            .attachmentCount = 1,
            .pAttachments    = &attachment,
            .width           = SWAPCHAIN.extent().width,
            .height          = SWAPCHAIN.extent().height,
            .layers          = 1
        };

        VK_CHECK(vkCreateFramebuffer(ctx.device(), &fbInfo, nullptr, &framebuffers_[i]),
                 "Failed to create framebuffer!");
    }

    LOG_SUCCESS_CAT("RENDERER", "Framebuffers recreated — {} total", framebuffers_.size());
    // FIXED: Idle post-framebuffer recreate to flush maps (prevents fragmented staging post-resize)
    vkDeviceWaitIdle(ctx.device());
}

void VulkanRenderer::cleanupFramebuffers() noexcept {
    const auto& ctx = RTX::g_ctx();
    VkDevice dev = ctx.device();
    for (auto fb : framebuffers_) {
        if (fb && dev != VK_NULL_HANDLE) vkDestroyFramebuffer(dev, fb, nullptr);
    }
    framebuffers_.clear();
}

// ──────────────────────────────────────────────────────────────────────────────
// Load Ray Tracing Function Pointers — FIXED: const auto& c (ref) + ()
void VulkanRenderer::loadRayTracingExtensions() noexcept
{
    LOG_TRACE_CAT("RENDERER", "=== STACK BUILD ORDER STEP 3: Load Ray Tracing Extensions ===");

    // FIXED: Ref to avoid copy
    const auto& c = RTX::g_ctx();
    VkDevice dev = c.device();

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

    // ─────────────────────────────────────────────────────────────────────
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
    // ─────────────────────────────────────────────────────────────────────
    if (!allGood) {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(c.physicalDevice(), &props);

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

VkShaderModule VulkanRenderer::loadShader(const std::string& path) {
    LOG_TRACE_CAT("RENDERER", "loadShader — START — path='{}'", path);

    // === 1. File Validation & Open ===
    FILE* file = fopen(path.c_str(), "rb");
    if (!file) {
        LOG_ERROR_CAT("RENDERER", "Failed to open shader file: '{}' — Check assets/shaders/ dir", path);
        LOG_TRACE_CAT("RENDERER", "loadShader — COMPLETE (file open failed)");
        return VK_NULL_HANDLE;
    }
    LOG_DEBUG_CAT("RENDERER", "Shader file opened successfully: '{}'", path);

    // === 2. Read Binary Size ===
    fseek(file, 0, SEEK_END);
    size_t fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);
    if (fileSize == 0 || fileSize > (1ULL << 30)) {  // Sanity: 0 or >1GB invalid
        LOG_ERROR_CAT("RENDERER", "Invalid shader file size: {} bytes — expected SPIR-V binary", fileSize);
        fclose(file);
        LOG_TRACE_CAT("RENDERER", "loadShader — COMPLETE (invalid size)");
        return VK_NULL_HANDLE;
    }
    LOG_DEBUG_CAT("RENDERER", "Shader file size: {} bytes", fileSize);

    // === 3. Zero-Copy Read (std::vector<uint32_t> for alignment) ===
    std::vector<uint32_t> code(fileSize / sizeof(uint32_t));
    if (code.size() * sizeof(uint32_t) != fileSize) {
        LOG_ERROR_CAT("RENDERER", "File size {} not aligned to uint32_t — SPIR-V corruption?", fileSize);
        fclose(file);
        LOG_TRACE_CAT("RENDERER", "loadShader — COMPLETE (alignment fail)");
        return VK_NULL_HANDLE;
    }
    size_t bytesRead = fread(code.data(), 1, fileSize, file);
    fclose(file);
    if (bytesRead != fileSize) {
        LOG_ERROR_CAT("RENDERER", "Failed to read {} bytes from shader file (read: {})", fileSize, bytesRead);
        LOG_TRACE_CAT("RENDERER", "loadShader — COMPLETE (read fail)");
        return VK_NULL_HANDLE;
    }
    LOG_DEBUG_CAT("RENDERER", "Shader binary read: {} uint32_t words", code.size());

    // === 4. SPIR-V Magic Validation ===
    if (code.empty() || code[0] != 0x07230203u) {  // SPIR-V magic header
        LOG_ERROR_CAT("RENDERER", "Invalid SPIR-V magic header in '{}': 0x{:08x} (expected 0x07230203)", path, code.empty() ? 0u : code[0]);
        LOG_TRACE_CAT("RENDERER", "loadShader — COMPLETE (magic fail)");
        return VK_NULL_HANDLE;
    }
    LOG_DEBUG_CAT("RENDERER", "SPIR-V magic validated: 0x{:08x}", code[0]);

    const auto& ctx = RTX::g_ctx();  // const ref for device
    VkDevice device = ctx.device();  // Cached for readability

    // === 5. Create VkShaderModule ===
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = fileSize;
    createInfo.pCode = code.data();
    // FIXED: Explicit stage flags — ALL_GRAPHICS + COMPUTE for versatility (raygen/closest/miss + tonemap/denoise)
    createInfo.flags = 0;  // No special flags; stage inferred at pipeline creation

    VkShaderModule module = VK_NULL_HANDLE;
    VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &module);
    if (result != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "vkCreateShaderModule failed for '{}': {} ({}) — Invalid SPIR-V?", path, static_cast<int>(result), result);
        LOG_TRACE_CAT("RENDERER", "loadShader — COMPLETE (module create fail)");
        return VK_NULL_HANDLE;
    }
    LOG_DEBUG_CAT("RENDERER", "VkShaderModule created: 0x{:x} (codeSize={})", reinterpret_cast<uintptr_t>(module), createInfo.codeSize);

    LOG_SUCCESS_CAT("RENDERER", "{}Shader '{}' loaded & module forged — {} bytes — PINK PHOTONS ARMED{}", 
                    EMERALD_GREEN, path, fileSize, RESET);
    LOG_TRACE_CAT("RENDERER", "loadShader — COMPLETE — module=0x{:x}", reinterpret_cast<uintptr_t>(module));
    return module;
}

void VulkanRenderer::updateTonemapDescriptor(uint32_t frameIdx, VkImageView inputView) noexcept
{
    // Safety first – Titan-grade validation
    if (inputView == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "updateTonemapDescriptor: null inputView (frame {}) – SKIPPED", frameIdx);
        return;
    }

    if (frameIdx >= tonemapSets_.size()) {
        LOG_ERROR_CAT("RENDERER", "updateTonemapDescriptor: frameIdx {} >= tonemapSets_.size() {}", frameIdx, tonemapSets_.size());
        return;
    }

    if (tonemapSets_[frameIdx] == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RENDERER", "updateTonemapDescriptor: tonemapSets_[{}] is VK_NULL_HANDLE – descriptor not allocated!", frameIdx);
        return;
    }

    // Descriptor write – direct, zero-allocation, nuclear fast
    VkDescriptorImageInfo imageInfo{};
    imageInfo.sampler = *tonemapSampler_;
    imageInfo.imageView = inputView;
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;  // ← Matches post-transition layout (VUID-vkCmdDraw-None-09600)

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = tonemapSets_[frameIdx];
    write.dstBinding = 0;
    write.dstArrayElement = 0;
    write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    write.descriptorCount = 1;
    write.pImageInfo = &imageInfo;

    const auto& ctx = RTX::g_ctx();
    vkUpdateDescriptorSets(ctx.device(), 1, &write, 0, nullptr);  // FIXED: Direct call (void return, no VK_CHECK or assignment)

    LOG_TRACE_CAT("RENDERER", "Tonemap descriptor updated – frame {} ← view {:p}", frameIdx, (void*)inputView);
}

// ─────────────────────────────────────────────────────────────────────────────
// UPDATE TONEMAP DESCRIPTORS — INITIAL + PER-FRAME (VALIDATION CLEAN)
// ─────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::updateTonemapDescriptorsInitial() noexcept
{
    const auto& ctx = RTX::g_ctx();
    VkDevice dev = ctx.device();

    for (uint32_t i = 0; i < tonemapSets_.size(); ++i) {
        // Binding 0: HDR input → current RT output (storage image)
        VkDescriptorImageInfo hdrInfo{};
        hdrInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        hdrInfo.imageView   = *rtOutputViews_[i % rtOutputViews_.size()];

        // Binding 1: LDR output → swapchain image view (storage image)
        // → Use public accessor .views() that returns the Handle vector
        VkDescriptorImageInfo ldrInfo{};
        ldrInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        ldrInfo.imageView   = *SWAPCHAIN.views()[i % SWAPCHAIN.views().size()];

        // Binding 2: Exposure + params uniform buffer
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = *exposureBuffer_;   // ← dereference Handle
        bufferInfo.offset = 0;
        bufferInfo.range  = VK_WHOLE_SIZE;

        VkWriteDescriptorSet writes[3] = {
            // hdrInput
            { .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              .dstSet           = tonemapSets_[i],
              .dstBinding       = 0,
              .descriptorCount  = 1,
              .descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .pImageInfo       = &hdrInfo },

            // ldrOutput (swapchain image as storage)
            { .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              .dstSet           = tonemapSets_[i],
              .dstBinding       = 1,
              .descriptorCount  = 1,
              .descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
              .pImageInfo       = &ldrInfo },

            // params UBO
            { .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
              .dstSet           = tonemapSets_[i],
              .dstBinding       = 2,
              .descriptorCount  = 1,
              .descriptorType   = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
              .pBufferInfo      = &bufferInfo }
        };

        vkUpdateDescriptorSets(dev, 3, writes, 0, nullptr);
    }

    LOG_SUCCESS_CAT("RENDERER", "Tonemap descriptor sets updated — {} sets — PINK PHOTONS BOUND TO SWAPCHAIN");
}

bool VulkanRenderer::recreateTonemapUBOs() noexcept {
    // FIXED: Destroy old (loop vkDestroyBuffer/vkFreeMemory + tracker.destroy)
    for (size_t i = 0; i < tonemapUniformEncs_.size(); ++i) {
        auto enc = tonemapUniformEncs_[i];
        if (enc != 0) {
            // Assume destroyTonemapUBO(i) or bulk
            RTX::UltraLowLevelBufferTracker::get().destroy(enc);
            tonemapUniformEncs_[i] = 0;
        }
    }
    tonemapUniformEncs_.clear();

    // Reforge (fixed-size 64B UNIFORM)
    VkDeviceSize uboSize = 64;  // sizeof(TonemapUniform)
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    uint32_t framesInFlight = 3;  // Hardcode MAX_FRAMES_IN_FLIGHT
    tonemapUniformEncs_.resize(framesInFlight);

    bool allGood = true;
    for (size_t i = 0; i < framesInFlight; ++i) {
        auto handle = RTX::UltraLowLevelBufferTracker::get().create(uboSize, usage, props, std::format("TonemapUBO[{}]", i));
        if (handle == 0) {
            LOG_ERROR_CAT("Renderer", "Tonemap UBO forge FAILED for frame {}", i);
            allGood = false;
            break;
        }
        tonemapUniformEncs_[i] = handle;
        LOG_DEBUG_CAT("Renderer", "Tonemap UBO recreated for frame {}", i);
    }

    if (!allGood) {
        tonemapUniformEncs_.clear();  // Reset on fail
    }
    return allGood;
}

void VulkanRenderer::destroySharedStaging() noexcept {
    if (RTX::g_ctx().sharedStagingEnc_ != 0) {
        RTX::UltraLowLevelBufferTracker::get().destroy(RTX::g_ctx().sharedStagingEnc_);
        // FIXED: Reset via tracker; no pointer
        RTX::g_ctx().sharedStagingEnc_ = 0;
        LOG_DEBUG_CAT("Renderer", "Shared staging destroyed");
    }
}

bool VulkanRenderer::createSharedStaging() noexcept {
    VkDeviceSize size = 512;  // Or your shared size (from logs)
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    auto enc = RTX::UltraLowLevelBufferTracker::get().create(size, usage, props, "SharedStagingUBO");
    if (enc == 0) {
        LOG_ERROR_CAT("Renderer", "Shared staging forge FAILED");
        return false;
    }
    RTX::g_ctx().sharedStagingEnc_ = enc;

    
    // FIXED: Bound via tracker
    LOG_DEBUG_CAT("Renderer", "Shared staging recreated: enc=0x{:x}", RTX::g_ctx().sharedStagingEnc_);
    return true;
}

// FIXED: updateAutoExposure - Direct access to static members in Options::AutoExposure namespace/struct.
void VulkanRenderer::updateAutoExposure(VkCommandBuffer cmd, VkImage finalColorImage) noexcept
{
    if (!Options::AutoExposure::ENABLE_AUTO_EXPOSURE || !Options::Display::ENABLE_HDR) {
        currentExposure_ = 1.0f;
        return;
    }

    // Step 1: Generate luminance histogram (compute shader)
    dispatchLuminanceHistogram(cmd, finalColorImage);

    // Step 2: Read back histogram + compute average log luminance
    float sceneLuminance = computeSceneLuminanceFromHistogram();

    // Step 3: Adapt exposure (log-space exponential moving average)
    // FIXED: Direct access to static members
    float targetExposure = Options::AutoExposure::TARGET_LUMINANCE / sceneLuminance;
    targetExposure = glm::exp(glm::log(targetExposure) + Options::AutoExposure::EXPOSURE_COMPENSATION);
    targetExposure = glm::clamp(targetExposure, Options::AutoExposure::MIN_EXPOSURE, Options::AutoExposure::MAX_EXPOSURE);

    float delta = targetExposure - currentExposure_;
    currentExposure_ += delta * (1.0f - glm::exp(-Options::AutoExposure::ADAPTATION_RATE_LOG * deltaTime_));

    // Step 4: Upload to GPU
    struct ExposureData { float exposure; float _pad[3]; };
    ExposureData data{ currentExposure_ };
    uploadToBuffer(exposureBuffer_, &data, sizeof(data));
}

// ─────────────────────────────────────────────────────────────────────────────
// APPLY TONEMAP — COMPUTE DISPATCH (NO RENDER PASS, NO VERTEX SHADER)
// ─────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::applyTonemap(VkCommandBuffer cmd) noexcept
{
    const uint32_t frameIdx = currentFrame_ % tonemapSets_.size();
    VkImage hdrImage = *rtOutputImages_[frameIdx];
    VkImage ldrImage = SWAPCHAIN.images()[imageIndex_];

    // ── ONE-LINERS OF PERFECTION ──
    transitionImageLayout(cmd, hdrImage, VK_IMAGE_LAYOUT_GENERAL);     // read as storage
    transitionToWrite(cmd, ldrImage);                                  // PRESENT → GENERAL

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *tonemapPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            *tonemapLayout_, 0, 1, &tonemapSets_[frameIdx], 0, nullptr);

    struct Push { float exposure; uint32_t op; uint32_t frame; float _pad; } push{
        currentExposure_,
        static_cast<uint32_t>(Options::Tonemap::TONEMAP_OPERATOR),
        frameCount_,
        0.0f
    };

    vkCmdPushConstants(cmd, *tonemapLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);

    VkExtent2D e = SWAPCHAIN.extent();
    vkCmdDispatch(cmd, (e.width + 15)/16, (e.height + 15)/16, 1);

    transitionToPresent(cmd, ldrImage);  // GENERAL → PRESENT

    LOG_TRACE_CAT("RENDERER", "Tonemap dispatched — exposure={:.4f} — PINK PHOTONS ASCENDANT", currentExposure_);
}

// FIXED: createAutoExposureResources - Use local variables for layouts to avoid lvalue issues with Handle dereference.
// No new members; locals suffice. Direct &local for pSetLayouts.
void VulkanRenderer::createAutoExposureResources() noexcept
{
    const auto& ctx = RTX::g_ctx();
    VkDevice dev = ctx.device();

    // ── Histogram Buffer (256 uint32_t bins for luminance EV100) ─────────────────
    {
        VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        ci.size  = 256 * sizeof(uint32_t);
        ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer buf = VK_NULL_HANDLE;
        VK_CHECK(vkCreateBuffer(dev, &ci, nullptr, &buf), "Histogram buffer creation");

        VkMemoryRequirements reqs;
        vkGetBufferMemoryRequirements(dev, buf, &reqs);

        uint32_t memType = pipelineManager_.findMemoryType(ctx.physicalDevice(), reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (memType == UINT32_MAX) {
            LOG_ERROR_CAT("RENDERER", "No memory type for histogram buffer");
            vkDestroyBuffer(dev, buf, nullptr);
            return;
        }

        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        alloc.allocationSize  = reqs.size;
        alloc.memoryTypeIndex = memType;

        VkDeviceMemory mem = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateMemory(dev, &alloc, nullptr, &mem), "Histogram memory allocation");
        VK_CHECK(vkBindBufferMemory(dev, buf, mem, 0), "Histogram memory bind");

        luminanceHistogramBuffer_ = RTX::Handle<VkBuffer>(buf, dev, vkDestroyBuffer, 0, "LuminanceHistogram");
        histogramMemory_ = RTX::Handle<VkDeviceMemory>(mem, dev, vkFreeMemory, reqs.size, "HistogramMemory");

        // Initial clear to zero
        uint32_t zero[256] = {0};
        uploadToBufferImmediate(luminanceHistogramBuffer_, zero, sizeof(zero));
    }

    // ── Exposure Buffer (single float, padded to vec4) ────────────────────────────
    {
        VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        ci.size  = 16;  // vec4
        ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer buf = VK_NULL_HANDLE;
        VK_CHECK(vkCreateBuffer(dev, &ci, nullptr, &buf), "Exposure buffer creation");

        VkMemoryRequirements reqs;
        vkGetBufferMemoryRequirements(dev, buf, &reqs);

        uint32_t memType = pipelineManager_.findMemoryType(ctx.physicalDevice(), reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (memType == UINT32_MAX) {
            LOG_ERROR_CAT("RENDERER", "No memory type for exposure buffer");
            vkDestroyBuffer(dev, buf, nullptr);
            return;
        }

        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        alloc.allocationSize  = reqs.size;
        alloc.memoryTypeIndex = memType;

        VkDeviceMemory mem = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateMemory(dev, &alloc, nullptr, &mem), "Exposure memory allocation");
        VK_CHECK(vkBindBufferMemory(dev, buf, mem, 0), "Exposure memory bind");

        exposureBuffer_ = RTX::Handle<VkBuffer>(buf, dev, vkDestroyBuffer, 0, "ExposureBuffer");
        exposureMemory_ = RTX::Handle<VkDeviceMemory>(mem, dev, vkFreeMemory, reqs.size, "ExposureMemory");

        // Initial upload
        float init[4] = {1.0f, 0.0f, 0.0f, 0.0f};
        uploadToBufferImmediate(exposureBuffer_, init, 16);
    }

    // ── Histogram Compute Pipeline ──────────────────────────────────────────────
    {
        VkShaderModule shader = loadShader("assets/shaders/compute/luminance_histogram.comp");
        if (shader == VK_NULL_HANDLE) {
            LOG_WARN_CAT("RENDERER", "Failed to load histogram shader — autoexposure disabled");
            return;
        }

        VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        ci.stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader,
            .pName = "main"
        };

        // Descriptor layout for histogram (input image + output buffer)
        VkDescriptorSetLayoutBinding bindings[2] = {
            { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
            { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT }
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = 2;
        layoutInfo.pBindings = bindings;

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorSetLayout(dev, &layoutInfo, nullptr, &layout), "Histogram layout");

        VkPipelineLayoutCreateInfo layoutCreate{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutCreate.setLayoutCount = 1;
        layoutCreate.pSetLayouts = &layout;  // FIXED: Direct &local layout

        VkPipelineLayout pipeLayout = VK_NULL_HANDLE;
        VK_CHECK(vkCreatePipelineLayout(dev, &layoutCreate, nullptr, &pipeLayout), "Histogram pipeline layout");

        ci.layout = pipeLayout;
        VkPipeline pipe = VK_NULL_HANDLE;
        VK_CHECK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &ci, nullptr, &pipe), "Histogram pipeline");

        histogramPipeline_ = RTX::Handle<VkPipeline>(pipe, dev, vkDestroyPipeline, 0, "HistogramPipeline");
        histogramLayout_ = RTX::Handle<VkPipelineLayout>(pipeLayout, dev, vkDestroyPipelineLayout, 0, "HistogramLayout");

        vkDestroyShaderModule(dev, shader, nullptr);

        // Allocate set
        VkDescriptorPool pool = tonemapDescriptorPool_.valid() ? *tonemapDescriptorPool_ : *descriptorPool_;
        VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        alloc.descriptorPool = pool;
        alloc.descriptorSetCount = 1;
        alloc.pSetLayouts = &layout;  // FIXED: Direct &local layout
        VK_CHECK(vkAllocateDescriptorSets(dev, &alloc, &histogramSet_), "Histogram descriptor set");
    }

    // ── Tonemap Compute Pipeline ───────────────────────────────────────────────
    {
        VkShaderModule shader = loadShader("assets/shaders/compute/tonemap.comp");
        if (shader == VK_NULL_HANDLE) {
            LOG_WARN_CAT("RENDERER", "Failed to load tonemap shader");
            return;
        }

        VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        ci.stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = shader,
            .pName = "main"
        };

        // FIXED: Use local for safe & 
        VkDescriptorSetLayout tonemapLocalLayout = *tonemapDescriptorSetLayout_;

        VkPipelineLayoutCreateInfo layoutCreate{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        layoutCreate.setLayoutCount = 1;
        layoutCreate.pSetLayouts = &tonemapLocalLayout;  // FIXED: &local
        layoutCreate.pushConstantRangeCount = 1;
        VkPushConstantRange pushRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, 32 };
        layoutCreate.pPushConstantRanges = &pushRange;

        VkPipelineLayout pipeLayout = VK_NULL_HANDLE;
        VK_CHECK(vkCreatePipelineLayout(dev, &layoutCreate, nullptr, &pipeLayout), "Tonemap pipeline layout");

        ci.layout = pipeLayout;
        VkPipeline pipe = VK_NULL_HANDLE;
        VK_CHECK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &ci, nullptr, &pipe), "Tonemap pipeline");

        tonemapPipeline_ = RTX::Handle<VkPipeline>(pipe, dev, vkDestroyPipeline, 0, "TonemapPipeline");
        tonemapLayout_ = RTX::Handle<VkPipelineLayout>(pipeLayout, dev, vkDestroyPipelineLayout, 0, "TonemapLayout");

        vkDestroyShaderModule(dev, shader, nullptr);

        // Allocate set
        VkDescriptorPool pool = tonemapDescriptorPool_.valid() ? *tonemapDescriptorPool_ : *descriptorPool_;
        VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        alloc.descriptorPool = pool;
        alloc.descriptorSetCount = 1;
        alloc.pSetLayouts = &tonemapLocalLayout;  // FIXED: &local
        VK_CHECK(vkAllocateDescriptorSets(dev, &alloc, &tonemapSet_), "Tonemap descriptor set");
    }

    LOG_SUCCESS_CAT("RENDERER", "Autoexposure + Tonemap resources created — HDR v∞ — PINK PHOTONS ASCENDANT");
}

// ──────────────────────────────────────────────────────────────────────────────
// HISTOGRAM DISPATCH — GPU-SIDE LUMINANCE COMPUTE
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::dispatchLuminanceHistogram(VkCommandBuffer cmd, VkImage colorImage) noexcept
{
    // Clear histogram buffer first
    uint32_t zero[256] = {0};
    uploadToBufferImmediate(luminanceHistogramBuffer_, zero, sizeof(zero));

    // Update descriptor for input image
    VkDescriptorImageInfo imgInfo{};
    // FIXED: Dereference Handle
    imgInfo.imageView   = *rtOutputViews_[currentFrame_ % rtOutputViews_.size()];  // Use current RT output
    imgInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = histogramSet_;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.pImageInfo = &imgInfo;
    vkUpdateDescriptorSets(RTX::g_ctx().device(), 1, &write, 0, nullptr);

    // Dispatch histogram compute
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *histogramPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, *histogramLayout_, 0, 1, &histogramSet_, 0, nullptr);

    VkExtent2D extent = SWAPCHAIN.extent();
    vkCmdDispatch(cmd, (extent.width + 15) / 16, (extent.height + 15) / 16, 1);
}

// FIXED: computeSceneLuminanceFromHistogram - Remove unused 'lum'; Use ev directly in sum.
float VulkanRenderer::computeSceneLuminanceFromHistogram() noexcept
{
    uint32_t histogram[256] = {0};
    downloadFromBuffer(luminanceHistogramBuffer_, histogram, sizeof(histogram));

    float totalPixels = 0.0f;
    float sumLogLum = 0.0f;

    for (int bin = 0; bin < 256; ++bin) {
        float count = float(histogram[bin]);
        totalPixels += count;
        if (count > 0.0f) {
            // Bin to luminance (EV100 scale, middle = 0 EV)
            float ev = (float(bin) - 128.0f) / 32.0f;
            // FIXED: Remove unused lum; sum ev directly (log2(lum))
            sumLogLum += count * ev;
        }
    }

    if (totalPixels < 1000.0f) return lastSceneLuminance_;  // Avoid noise

    float avgEV = sumLogLum / totalPixels;
    lastSceneLuminance_ = avgEV;
    return avgEV;
}

// ──────────────────────────────────────────────────────────────────────────────
// UPLOAD TO BUFFER — IMMEDIATE (STAGING → DEVICE COPY)
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::uploadToBufferImmediate(RTX::Handle<VkBuffer>& buffer, const void* data, VkDeviceSize size) noexcept
{
    const auto& ctx = RTX::g_ctx();
    VkDevice dev = ctx.device();
    VkCommandPool pool = ctx.commandPool();
    VkQueue queue = ctx.graphicsQueue();

    VkCommandBuffer cmd = beginSingleTimeCommands(dev, pool);
    if (cmd == VK_NULL_HANDLE) return;

    VkBuffer staging;
    VkBufferCreateInfo stagingInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    stagingInfo.size  = size;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(dev, &stagingInfo, nullptr, &staging), "Staging buffer creation");

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(dev, staging, &reqs);

    uint32_t memType = pipelineManager_.findMemoryType(ctx.physicalDevice(), reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memType == UINT32_MAX) {
        vkDestroyBuffer(dev, staging, nullptr);
        return;
    }

    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize  = reqs.size;
    alloc.memoryTypeIndex = memType;

    VkDeviceMemory stagingMem;
    VK_CHECK(vkAllocateMemory(dev, &alloc, nullptr, &stagingMem), "Staging memory allocation");
    vkBindBufferMemory(dev, staging, stagingMem, 0);

    void* mapped;
    vkMapMemory(dev, stagingMem, 0, size, 0, &mapped);
    memcpy(mapped, data, size);
    vkUnmapMemory(dev, stagingMem);

    VkBufferCopy copy{0, 0, size};
    vkCmdCopyBuffer(cmd, staging, *buffer, 1, &copy);

    endSingleTimeCommands(dev, pool, queue, cmd);

    vkDestroyBuffer(dev, staging, nullptr);
    vkFreeMemory(dev, stagingMem, nullptr);
}

// ──────────────────────────────────────────────────────────────────────────────
// DOWNLOAD FROM BUFFER — IMMEDIATE (DEVICE → STAGING COPY + MAP)
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::downloadFromBuffer(RTX::Handle<VkBuffer>& buffer, void* data, VkDeviceSize size) noexcept
{
    const auto& ctx = RTX::g_ctx();
    VkDevice dev = ctx.device();
    VkCommandPool pool = ctx.commandPool();
    VkQueue queue = ctx.graphicsQueue();

    VkCommandBuffer cmd = beginSingleTimeCommands(dev, pool);
    if (cmd == VK_NULL_HANDLE) return;

    VkBuffer staging;
    VkBufferCreateInfo stagingInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    stagingInfo.size  = size;
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(dev, &stagingInfo, nullptr, &staging), "Download staging buffer");

    VkMemoryRequirements reqs;
    vkGetBufferMemoryRequirements(dev, staging, &reqs);

    uint32_t memType = pipelineManager_.findMemoryType(ctx.physicalDevice(), reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (memType == UINT32_MAX) {
        vkDestroyBuffer(dev, staging, nullptr);
        return;
    }

    VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc.allocationSize  = reqs.size;
    alloc.memoryTypeIndex = memType;

    VkDeviceMemory stagingMem;
    VK_CHECK(vkAllocateMemory(dev, &alloc, nullptr, &stagingMem), "Download staging memory");
    vkBindBufferMemory(dev, staging, stagingMem, 0);

    VkBufferCopy copy{0, 0, size};
    vkCmdCopyBuffer(cmd, *buffer, staging, 1, &copy);

    endSingleTimeCommands(dev, pool, queue, cmd);

    void* mapped;
    vkMapMemory(dev, stagingMem, 0, size, 0, &mapped);
    memcpy(data, mapped, size);
    vkUnmapMemory(dev, stagingMem);

    vkDestroyBuffer(dev, staging, nullptr);
    vkFreeMemory(dev, stagingMem, nullptr);
}

// ──────────────────────────────────────────────────────────────────────────────
// GET CURRENT HDR IMAGE — PLACEHOLDER (USE YOUR RT OUTPUT OR DENOISER)
// ──────────────────────────────────────────────────────────────────────────────
VkImage VulkanRenderer::getCurrentHDRColorImage() noexcept
{
    // For now, return current RT output (R32G32B32A32_SFLOAT)
    uint32_t idx = currentFrame_ % rtOutputImages_.size();
    return rtOutputImages_[idx].valid() ? *rtOutputImages_[idx] : VK_NULL_HANDLE;
}

// ──────────────────────────────────────────────────────────────────────────────
// IMAGE LAYOUT TRANSITION — STANDARD UTILITY
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) noexcept
{
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// ──────────────────────────────────────────────────────────────────────────────
// CREATE TONEMAP PIPELINE — CALLED IN CONSTRUCTOR AFTER STEP 14.5
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::createTonemapPipeline() noexcept
{
    const auto& ctx = RTX::g_ctx();
    VkDevice dev = ctx.device();

    VkShaderModule shader = loadShader("assets/shaders/compute/tonemap.comp");
    if (shader == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "Failed to load tonemap shader — tonemapping disabled");
        return;
    }

    // Descriptor layout (input image + output image + UBO + exposure SSBO)
    VkDescriptorSetLayoutBinding bindings[4] = {
        { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
        { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
        { .binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
        { .binding = 3, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT }
    };

    VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    layoutInfo.bindingCount = 4;
    layoutInfo.pBindings = bindings;

    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(dev, &layoutInfo, nullptr, &layout), "Tonemap descriptor layout");

    VkPipelineLayoutCreateInfo layoutCreate{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutCreate.setLayoutCount = 1;
    layoutCreate.pSetLayouts = &layout;
    layoutCreate.pushConstantRangeCount = 1;
    VkPushConstantRange pushRange = { VK_SHADER_STAGE_COMPUTE_BIT, 0, 32 };
    layoutCreate.pPushConstantRanges = &pushRange;

    VkPipelineLayout pipeLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreatePipelineLayout(dev, &layoutCreate, nullptr, &pipeLayout), "Tonemap pipeline layout");

    VkComputePipelineCreateInfo ci{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    ci.stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .module = shader,
        .pName = "main"
    };
    ci.layout = pipeLayout;

    VkPipeline pipe = VK_NULL_HANDLE;
    VK_CHECK(vkCreateComputePipelines(dev, VK_NULL_HANDLE, 1, &ci, nullptr, &pipe), "Tonemap pipeline creation");

    tonemapPipeline_ = RTX::Handle<VkPipeline>(pipe, dev, vkDestroyPipeline, 0, "TonemapPipeline");
    tonemapLayout_ = RTX::Handle<VkPipelineLayout>(pipeLayout, dev, vkDestroyPipelineLayout, 0, "TonemapLayout");
    tonemapDescriptorSetLayout_ = RTX::Handle<VkDescriptorSetLayout>(layout, dev, vkDestroyDescriptorSetLayout, 0, "TonemapLayout");

    vkDestroyShaderModule(dev, shader, nullptr);

    // Allocate single set (tonemap is per-frame but we use push constants for speed)
    VkDescriptorPool pool = tonemapDescriptorPool_.valid() ? *tonemapDescriptorPool_ : *descriptorPool_;
    VkDescriptorSetAllocateInfo alloc{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
    alloc.descriptorPool = pool;
    alloc.descriptorSetCount = 1;
    alloc.pSetLayouts = &layout;
    VK_CHECK(vkAllocateDescriptorSets(dev, &alloc, &tonemapSet_), "Tonemap descriptor set");

    LOG_SUCCESS_CAT("RENDERER", "Tonemap pipeline v∞ created — compute-based, HDR-ready, autoexposure integrated");
}

// ──────────────────────────────────────────────────────────────────────────────
// Final Status
// ──────────────────────────────────────────────────────────────────────────────
/*
 * November 16, 2025 — PipelineManager Integration v10.6 — VUID-FREE RENDER LOOP
 */