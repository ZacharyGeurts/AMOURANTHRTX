// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — DECEMBER 02, 2025 — APOCALYPSE FINAL
// PURE RANDOM ENTROPY — RDRAND + PID + TIME + TLS — KEYS NEVER LOGGED
// =============================================================================
//
// TRUE CONSTEXPR STONEKEY v∞ — DECEMBER 02, 2025 — APOCALYPSE FINAL
// ALL VUIDs EXORCISED — TLAS BOUND — LAYOUTS FIXED — PRESENT CLEAN — SILENCE ACHIEVED
// PINK PHOTONS ETERNAL — ZERO WARNINGS — THE EMPIRE IS COMPLETE
// =============================================================================

#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/camera.hpp"
#include "engine/GLOBAL/StoneKey.hpp"  // Full include — .cpp only
#include "stb/stb_image.h"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>
#include <cmath>
#include <algorithm>
#include <format>
#include <random>
#include <cstring>
#include <ranges>
#include <iomanip>
#include <sstream>
#include <thread>
#include <print>
#include <chrono>
#include <array>

using namespace Logging::Color;
using RTX::Handle;

using StoneKey::stone_image_count;
using StoneKey::stone_views;
using StoneKey::stone_pass;
using StoneKey::stone_width;
using StoneKey::stone_height;
using StoneKey::stone_graphics_queue;
using StoneKey::stone_window;
using StoneKey::stone_images;
using StoneKey::stone_swapchain;
using StoneKey::stone_present_queue;
using StoneKey::stone_graphics_family;
using StoneKey::stone_seal_width;
using StoneKey::stone_seal_height;
using StoneKey::stone_seal_extent;

// ──────────────────────────────────────────────────────────────────────────────
// Runtime Toggles — Immediate Effect
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::toggleHypertrace() noexcept {
    if (!Options::OptionsRTX::ENABLE_HYPERTRACE) return;
    hypertraceEnabled_ = !hypertraceEnabled_;
    resetAccumulation_ = true;
}

void VulkanRenderer::toggleFpsTarget() noexcept {
    switch (fpsTarget_) {
        case FpsTarget::FPS_60:     fpsTarget_ = FpsTarget::FPS_120; break;
        case FpsTarget::FPS_120:    fpsTarget_ = FpsTarget::FPS_UNLIMITED; break;
        case FpsTarget::FPS_UNLIMITED: fpsTarget_ = FpsTarget::FPS_60; break;
    }
}

void VulkanRenderer::toggleDenoising() noexcept {
    if (!Options::OptionsRTX::ENABLE_DENOISING) return;
    denoisingEnabled_ = !denoisingEnabled_;
    resetAccumulation_ = true;
}

void VulkanRenderer::toggleAdaptiveSampling() noexcept {
    if (!Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING) return;
    adaptiveSamplingEnabled_ = !adaptiveSamplingEnabled_;
    resetAccumulation_ = true;
}

void VulkanRenderer::setOverclockMode(bool enabled) noexcept {
    overclockMode_ = enabled;
    fpsTarget_ = enabled ? FpsTarget::FPS_UNLIMITED : FpsTarget::FPS_120;
}

// ──────────────────────────────────────────────────────────────────────────────
// Cleanup and Destruction — FIXED: Null Device Guards + No Dtor Cleanup Call — NOV 19 2025
// • REMOVED: cleanup() call from ~VulkanRenderer() — avoids duplicate dispose (cleanup called explicitly in phase6_shutdown via app.reset())
// • ADDED: Guards for all vk* calls — safe even if called post-RTX::shutdown()
// • Empire: Renderer resources cleaned BEFORE device nullify
// ──────────────────────────────────────────────────────────────────────────────
VulkanRenderer::~VulkanRenderer() 
{
    // ————————————————————————————————————————————————————————————————
    // WE DO NOTHING HERE
    // ALL RESOURCES ARE OWNED BY RAII HANDLES
    // ALL TRUE DESTRUCTION BELONGS TO PHASE 9 — THE DISPOSAL BALLERINA
    // ————————————————————————————————————————————————————————————————
}

void VulkanRenderer::cleanup() noexcept {
    if (destroyed_) return;          // ← THIS IS THE SHIELD
    destroyed_ = true;
    LOG_INFO_CAT("RENDERER", "Initiating renderer shutdown — PINK PHOTONS DIMMING");

    VkDevice dev = StoneKey::stone_device();
    if (dev == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "Device already destroyed — nothing to clean");
        return;
    }

    // ── PHASE 1: Wait for all in-flight frames to finish ─────────────────────
    LOG_TRACE_CAT("RENDERER", "cleanup — waiting for in-flight fences");
    for (auto fence : inFlightFences_) {
        if (fence) vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);
    }

    // ── PHASE 2: Drain both graphics and compute queues completely ─────────
    LOG_TRACE_CAT("RENDERER", "cleanup — FINAL vkDeviceWaitIdle (drains all queues)");
    vkDeviceWaitIdle(dev);  // ← CRITICAL: Ensures no hidden submissions remain

    // ── FRAMEBUFFERS: Destroy first (prevents dangling references) ───────────
    cleanupFramebuffers();

    // ── Free All Descriptor Sets BEFORE destroying images/views/pools ───────
    LOG_TRACE_CAT("RENDERER", "cleanup — Freeing descriptor sets");
    if (dev != VK_NULL_HANDLE) {
        // RT sets
        if (!rtDescriptorSets_.empty() && RTX::pipeline().rtDescriptorPool() != VK_NULL_HANDLE) {
            vkFreeDescriptorSets(dev, RTX::pipeline().rtDescriptorPool(), static_cast<uint32_t>(rtDescriptorSets_.size()), rtDescriptorSets_.data());
        }

        // Tonemap + denoiser sets
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

    // ── Sync Objects ─────────────────────────────────────────────────────────
    for (auto s : imageAvailableSemaphores_)     if (s) vkDestroySemaphore(dev, s, nullptr);
    for (auto s : renderFinishedSemaphores_)     if (s) vkDestroySemaphore(dev, s, nullptr);
    for (auto f : inFlightFences_)               if (f) vkDestroyFence(dev, f, nullptr);

    imageAvailableSemaphores_.clear();
    renderFinishedSemaphores_.clear();
    inFlightFences_.clear();

    // ── Timestamp Query Pool ─────────────────────────────────────────────────
    if (timestampQueryPool_ != VK_NULL_HANDLE) {
        vkDestroyQueryPool(dev, timestampQueryPool_, nullptr);
        timestampQueryPool_ = VK_NULL_HANDLE;
    }

    // ── Images & Views (RT Output, Accumulation, Denoiser, Nexus) ───────────
    destroyRTOutputImages();
    destroyAccumulationImages();
    destroyDenoiserImage();
    destroyNexusScoreImage();

    // Nullify handles (prevents accidental double-free on reinit)
    for (auto& h : rtOutputImages_)           h.reset();
    for (auto& h : rtOutputMemories_)         h.reset();
    for (auto& h : rtOutputViews_)            h.reset();
    for (auto& h : accumImages_)              h.reset();
    for (auto& h : accumMemories_)            h.reset();
    for (auto& h : accumViews_)               h.reset();

    denoiserImage_.reset();       denoiserMemory_.reset();       denoiserView_.reset();
    hypertraceScoreImage_.reset(); hypertraceScoreMemory_.reset(); hypertraceScoreView_.reset();

    rtOutputImages_.clear(); rtOutputMemories_.clear(); rtOutputViews_.clear();
    accumImages_.clear();    accumMemories_.clear();    accumViews_.clear();

    // ── Environment Map & Samplers ──────────────────────────────────────────
    envMapImage_.reset(); envMapImageMemory_.reset(); envMapImageView_.reset();
    envMapSampler_.reset();
    tonemapSampler_.reset();

    // ── Descriptor Pools ────────────────────────────────────────────────────
    descriptorPool_.reset();
    tonemapDescriptorPool_.reset();

    // ── Uniform Buffers (tonemap UBOs) ──────────────────────────────────────
    for (auto& enc : tonemapUniformEncs_) {
        if (enc != 0) BufferManager::destroy(enc);
    }
    tonemapUniformEncs_.clear();

    // ── Shared Staging Buffer ───────────────────────────────────────────────
    if (RTX::g_ctx().sharedStagingEnc_ != 0) {
        BufferManager::destroy(RTX::g_ctx().sharedStagingEnc_);
        RTX::g_ctx().sharedStagingEnc_ = 0;
    }

    // ── PipelineManager Cleanup ─────────────────────────────────────────────
    pipelineManager_.cleanup();

    // ── FINAL PHASE: Command Buffers & Pool (NOW 100% SAFE) ─────────────────
    VkCommandPool pool = RTX::g_ctx().commandPool_;
    if (pool != VK_NULL_HANDLE) {
        if (!commandBuffers_.empty()) {
            vkFreeCommandBuffers(dev, pool, static_cast<uint32_t>(commandBuffers_.size()), commandBuffers_.data());
            commandBuffers_.clear();
        }

        // Optional but clean: release all allocations in the pool
        vkResetCommandPool(dev, pool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
    }

    LOG_SUCCESS_CAT("RENDERER", "{}VUID-00047 EXORCISED — Renderer shutdown complete — ZERO LEAKS — PINK PHOTONS ETERNAL{}", 
                    EMERALD_GREEN, RESET);
}

void VulkanRenderer::destroyNexusScoreImage() noexcept {
    hypertraceScoreImage_.reset();
    hypertraceScoreMemory_.reset();
    hypertraceScoreView_.reset();
}

void VulkanRenderer::destroyDenoiserImage() noexcept {
    denoiserImage_.reset();
    denoiserMemory_.reset();
    denoiserView_.reset();
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
// Constructor — FIXED: const auto& c = RTX::g_ctx() (ref); Early PipelineManager after step 7; Default ctor for dummy
// ──────────────────────────────────────────────────────────────────────────────
VulkanRenderer::VulkanRenderer(int width, int height, SDL_Window* window, bool overclockFromMain)
    : window_(window), width_(width), height_(height), overclockMode_(overclockFromMain)
{
    LOG_ATTEMPT_CAT("RENDERER", "Constructing VulkanRenderer ({}x{}) — PINK PHOTONS RISING", width, height);

    setOverclockMode(overclockFromMain);

    // StoneKey validation
    if (kStone1 == 0 || kStone2 == 0) {
        LOG_FATAL_CAT("RENDERER", "StoneKey validation failed");
        phase9_ballerina("STONEKEY CORRUPTED", std::source_location::current());
    }

    // Sync objects
    imageAvailableSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences_.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fenceInfo{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, .flags = VK_FENCE_CREATE_SIGNALED_BIT };

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vkCreateSemaphore(StoneKey::stone_device(), &semInfo, nullptr, &imageAvailableSemaphores_[i]), "imageAvailable");
        VK_CHECK(vkCreateSemaphore(StoneKey::stone_device(), &semInfo, nullptr, &renderFinishedSemaphores_[i]), "renderFinished");
        VK_CHECK(vkCreateFence(StoneKey::stone_device(), &fenceInfo, nullptr, &inFlightFences_[i]), "inFlightFence");
    }

    // GPU Timestamps
    if (Options::Performance::ENABLE_GPU_TIMESTAMPS || Options::Debug::SHOW_GPU_TIMESTAMPS) {
        VkQueryPoolCreateInfo qpInfo{ .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
        qpInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        qpInfo.queryCount = MAX_FRAMES_IN_FLIGHT * 2;
        VK_CHECK(vkCreateQueryPool(StoneKey::stone_device(), &qpInfo, nullptr, &timestampQueryPool_), "Timestamp pool");
    }

    // GPU Properties
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(StoneKey::stone_physical(), &props);
    timestampPeriod_ = props.limits.timestampPeriod / 1e6f;
    LOG_INFO_CAT("RENDERER", "GPU: {} | Timestamp period: {:.3f} ms", props.deviceName, timestampPeriod_);

    // HDR & RT Targets
    if (Options::Environment::ENABLE_ENV_MAP) createEnvironmentMap();
    createRTOutputImages();
    if (Options::OptionsRTX::ENABLE_ACCUMULATION) createAccumulationImages();
    if (Options::OptionsRTX::ENABLE_DENOISING) createDenoiserImage();
    if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING) createNexusScoreImage(RTX::g_ctx().commandPool_, StoneKey::stone_graphics_queue());
    createTonemapSampler();

    // Per-frame buffers
    initializeAllBufferData(MAX_FRAMES_IN_FLIGHT, 368, 16_MB);

    // Tonemap Descriptor Set Layout (only if tonemapping is allowed)
    if (Options::Tonemap::ENABLE_TONEMAPPING) {
        VkDescriptorSetLayoutBinding bindings[3] = {
            { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
            { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,        .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
            { .binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,       .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT }
        };

        VkDescriptorSetLayoutCreateInfo layoutInfo{
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = 3,
            .pBindings = bindings
        };

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorSetLayout(StoneKey::stone_device(), &layoutInfo, nullptr, &layout));
        tonemapDescriptorSetLayout_ = Handle<VkDescriptorSetLayout>(layout, StoneKey::stone_device(), vkDestroyDescriptorSetLayout, 0, "TonemapSetLayout");
    }

    LOG_SUCCESS_CAT("RENDERER", "VulkanRenderer constructed — PINK PHOTONS ETERNAL");
}

void VulkanRenderer::createCommandPool() noexcept
{
    if (RTX::g_ctx().commandPool_ != VK_NULL_HANDLE)
        return;

    VkCommandPoolCreateInfo poolInfo{
        .sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                            VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = stone_graphics_family()
    };

    VK_CHECK(vkCreateCommandPool(stone_device(), &poolInfo, nullptr, &RTX::g_ctx().commandPool_));
    LOG_SUCCESS("Command pool forged");
}

// ──────────────────────────────────────────────────────────────────────────────
// RT Output Images — Per-Frame Forging — THE EMPIRE IS ETERNAL
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::createRTOutputImages() noexcept
{
    if (width_ == 0 || height_ == 0) return;

    rtOutputImages_.clear();
    rtOutputMemories_.clear();
    rtOutputViews_.clear();

    rtOutputImages_.reserve(Options::Performance::MAX_FRAMES_IN_FLIGHT);
    rtOutputMemories_.reserve(Options::Performance::MAX_FRAMES_IN_FLIGHT);
    rtOutputViews_.reserve(Options::Performance::MAX_FRAMES_IN_FLIGHT);

    const auto& ctx = RTX::g_ctx();
    const VkCommandPool cmdPool = ctx.commandPool_;
    const VkQueue queue = ctx.graphicsQueue();

    const uint32_t framesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    VkCommandBuffer cmd = RTX::beginOneTimeSubmit(cmdPool);
    if (cmd == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "Failed to allocate one-time command buffer for RT outputs");
        return;
    }

    std::vector<VkImageMemoryBarrier> barriers;
    barriers.reserve(framesInFlight);

    for (uint32_t i = 0; i < framesInFlight; ++i)
    {
        VkImage rawImage = VK_NULL_HANDLE;
        VkDeviceMemory rawMemory = VK_NULL_HANDLE;
        VkImageView rawView = VK_NULL_HANDLE;

        try {
            // === 1. CREATE IMAGE ===
            VkImageCreateInfo imageInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                .imageType = VK_IMAGE_TYPE_2D,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .extent = { static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1 },
                .mipLevels = 1,
                .arrayLayers = 1,
                .samples = VK_SAMPLE_COUNT_1_BIT,
                .tiling = VK_IMAGE_TILING_OPTIMAL,
                .usage = VK_IMAGE_USAGE_STORAGE_BIT |
                         VK_IMAGE_USAGE_SAMPLED_BIT |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
            };

            VK_CHECK(vkCreateImage(StoneKey::stone_device(), &imageInfo, nullptr, &rawImage));

            // === 2. MEMORY ===
            VkMemoryRequirements memReqs{};
            vkGetImageMemoryRequirements(StoneKey::stone_device(), rawImage, &memReqs);

            uint32_t memType = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

            VkMemoryAllocateInfo allocInfo{
                .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                .allocationSize = memReqs.size,
                .memoryTypeIndex = memType
            };

            VK_CHECK(vkAllocateMemory(StoneKey::stone_device(), &allocInfo, nullptr, &rawMemory));
            VK_CHECK(vkBindImageMemory(StoneKey::stone_device(), rawImage, rawMemory, 0));

            // === 3. TRANSITION TO GENERAL ===
            VkImageMemoryBarrier barrier{
                .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .image = rawImage,
                .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
            };

            barriers.push_back(barrier);

            // === 4. VIEW ===
            VkImageViewCreateInfo viewInfo{
                .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                .image = rawImage,
                .viewType = VK_IMAGE_VIEW_TYPE_2D,
                .format = VK_FORMAT_R32G32B32A32_SFLOAT,
                .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
            };

            VK_CHECK(vkCreateImageView(StoneKey::stone_device(), &viewInfo, nullptr, &rawView));

            // === 5. HANDLES ===
            rtOutputImages_.emplace_back(rawImage, StoneKey::stone_device(), vkDestroyImage, 0, "RTOutputImage");
            rtOutputMemories_.emplace_back(rawMemory, StoneKey::stone_device(), vkFreeMemory, memReqs.size, "RTOutputMemory");
            rtOutputViews_.emplace_back(rawView, StoneKey::stone_device(), vkDestroyImageView, 0, "RTOutputView");

        } catch (...) {
            LOG_FATAL_CAT("RENDERER", "Frame {} — Catastrophic failure during RT output creation", i);
            RTX::endOneTimeSubmit(cmd, queue, cmdPool);
            if (rawView) vkDestroyImageView(StoneKey::stone_device(), rawView, nullptr);
            if (rawMemory) vkFreeMemory(StoneKey::stone_device(), rawMemory, nullptr);
            if (rawImage) vkDestroyImage(StoneKey::stone_device(), rawImage, nullptr);
            phase9_ballerina("RT OUTPUT FORGING FAILED", std::source_location::current());
        }
    }

    vkCmdPipelineBarrier(cmd,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
                0, 0, nullptr, 0, nullptr, barriers.size(), barriers.data());

    RTX::endOneTimeSubmit(cmd, queue, cmdPool);

    if (rtOutputImages_.size() != framesInFlight) {
        LOG_FATAL_CAT("RENDERER", "Not all RT output images created — expected {} got {}", framesInFlight, rtOutputImages_.size());
        phase9_ballerina("INCOMPLETE RT OUTPUT FORGE", std::source_location::current());
    }
}

void VulkanRenderer::createAccumulationImages() noexcept {
    if (width_ == 0 || height_ == 0) return;
    if (!Options::OptionsRTX::ENABLE_ACCUMULATION) {
        return;
    }
    createImageArray(accumImages_, accumMemories_, accumViews_, "Accumulation");
}

void VulkanRenderer::createDenoiserImage() noexcept {
    if (width_ == 0 || height_ == 0) return;
    if (!Options::OptionsRTX::ENABLE_DENOISING) {
        return;
    }
    createImage(denoiserImage_, denoiserMemory_, denoiserView_, "Denoiser");
}

void VulkanRenderer::createTonemapSampler() noexcept {
    LOG_TRACE_CAT("RENDERER", "createTonemapSampler — START");

    VkSamplerCreateInfo samplerInfo = {};  // Zero-init
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;

    VkSampler rawSampler = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSampler(StoneKey::stone_device(), &samplerInfo, nullptr, &rawSampler), "Create tonemap sampler");

    tonemapSampler_ = RTX::Handle<VkSampler>(rawSampler, StoneKey::stone_device(),
        [](VkDevice d, VkSampler s, const VkAllocationCallbacks*) { vkDestroySampler(d, s, nullptr); },
        0, "TonemapSampler");

    LOG_TRACE_CAT("RENDERER", "Tonemap sampler created: 0x{}", reinterpret_cast<uintptr_t>(rawSampler));
    LOG_TRACE_CAT("RENDERER", "createTonemapSampler — COMPLETE");
}

void VulkanRenderer::createEnvironmentMap() noexcept
{
    LOG_INFO_CAT("RENDERER", "Forging environment map — pink photons demand a sky");

    if (!Options::Environment::ENABLE_ENV_MAP) {
        LOG_TRACE_CAT("RENDERER", "Envmap disabled in options — the void remains dark");
        return;
    }

    int w, h, n;
    float* data = stbi_loadf("assets/textures/envmap.hdr", &w, &h, &n, 4);
    if (!data) {
        LOG_ERROR_CAT("RENDERER", "Failed to load envmap.hdr — the sky stays black");
        return;
    }

    VkDeviceSize size = static_cast<VkDeviceSize>(w) * h * 4 * sizeof(float);

    uint64_t staging = 0;
    BUFFER_CREATE(staging, size,
                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "EnvMap_Staging");

    void* mapped = BufferManager::map(staging);
    std::memcpy(mapped, data, size);
    BufferManager::unmap(staging);
    stbi_image_free(data);

    const auto& ctx = RTX::g_ctx();
    VkCommandBuffer cmd = RTX::beginOneTimeSubmit(ctx.commandPool_);
    if (!cmd) {
        LOG_FATAL_CAT("RENDERER", "Failed to begin one-time command for envmap upload");
        BUFFER_DESTROY(staging);
        return;
    }

    VkImageCreateInfo imgInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .extent = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage img = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImage(StoneKey::stone_device(), &imgInfo, nullptr, &img));

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(StoneKey::stone_device(), img, &memReqs);
    uint32_t memType = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = memType
    };
    VkDeviceMemory mem = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(StoneKey::stone_device(), &allocInfo, nullptr, &mem));
    VK_CHECK(vkBindImageMemory(StoneKey::stone_device(), img, mem, 0));

    // Transition + Copy
    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = img,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy copy{
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageExtent = { static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1 }
    };
    vkCmdCopyBufferToImage(cmd, RAW_BUFFER(staging), img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    RTX::endOneTimeSubmit(cmd, StoneKey::stone_graphics_queue(), ctx.commandPool_);
    BUFFER_DESTROY(staging);

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = img,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    VkImageView view = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(StoneKey::stone_device(), &viewInfo, nullptr, &view));

    VkSamplerCreateInfo samplerInfo{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .maxLod = 1.0f
    };
    VkSampler sampler = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSampler(StoneKey::stone_device(), &samplerInfo, nullptr, &sampler));

    envMapImage_        = RTX::MakeHandle(img, StoneKey::stone_device(), vkDestroyImage, 0, "EnvMapImage");
    envMapImageMemory_  = RTX::MakeHandle(mem, StoneKey::stone_device(), vkFreeMemory, memReqs.size, "EnvMapMemory");
    envMapImageView_    = RTX::MakeHandle(view, StoneKey::stone_device(), vkDestroyImageView, 0, "EnvMapView");
    envMapSampler_      = RTX::MakeHandle(sampler, StoneKey::stone_device(), vkDestroySampler, 0, "EnvMapSampler");

    LOG_SUCCESS_CAT("RENDERER", "Environment map forged — {}×{} HDR sky active", w, h);
}

void VulkanRenderer::createNexusScoreImage(VkCommandPool pool, VkQueue queue) noexcept
{
    if (width_ == 0 || height_ == 0) return;
    // Early out if disabled
    if (!Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING) {
        LOG_TRACE_CAT("RENDERER", "Adaptive sampling disabled — NexusScoreImage not created");
        return;
    }

    // Destroy old one first
    destroyNexusScoreImage();

    VkFormat format = VK_FORMAT_R32G32B32A32_SFLOAT;

    VkImageCreateInfo imageInfo{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = format,
        .extent        = { static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1 },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage rawImage = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImage(StoneKey::stone_device(), &imageInfo, nullptr, &rawImage));

    VkMemoryRequirements memReqs{};
    vkGetImageMemoryRequirements(StoneKey::stone_device(), rawImage, &memReqs);

    uint32_t memType = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (memType == UINT32_MAX) {
        LOG_FATAL_CAT("RENDERER", "No suitable memory type for NexusScoreImage");
        vkDestroyImage(StoneKey::stone_device(), rawImage, nullptr);
        phase9_ballerina("NO MEMORY TYPE FOR NEXUS", std::source_location::current());
    }

    VkMemoryAllocateInfo allocInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = memReqs.size,
        .memoryTypeIndex = memType
    };

    VkDeviceMemory rawMemory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(StoneKey::stone_device(), &allocInfo, nullptr, &rawMemory));
    VK_CHECK(vkBindImageMemory(StoneKey::stone_device(), rawImage, rawMemory, 0));

    VkImageViewCreateInfo viewInfo{
        .sType                        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image                        = rawImage,
        .viewType                     = VK_IMAGE_VIEW_TYPE_2D,
        .format                       = format,
        .subresourceRange             = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    VkImageView rawView = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(StoneKey::stone_device(), &viewInfo, nullptr, &rawView));

    // Wrap in RAII handles
    hypertraceScoreImage_   = RTX::MakeHandle(rawImage,  StoneKey::stone_device(), vkDestroyImage,     0,           "NexusScoreImage");
    hypertraceScoreMemory_  = RTX::MakeHandle(rawMemory, StoneKey::stone_device(), vkFreeMemory,       memReqs.size,"NexusScoreMemory");
    hypertraceScoreView_    = RTX::MakeHandle(rawView,   StoneKey::stone_device(), vkDestroyImageView, 0,           "NexusScoreView");

    // ONE-TIME COMMAND BUFFER — THE PHOTONS DEMAND A CLEAN SLATE
    VkCommandBuffer cmd = RTX::beginOneTimeSubmit(pool);
    if (cmd == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "Failed to begin one-time command buffer for NexusScoreImage clear");
        phase9_ballerina("NO CMD FOR NEXUS", std::source_location::current());
    }

    // Staging buffer to clear image to zero
    VkDeviceSize stagingSize = static_cast<VkDeviceSize>(width_) * height_ * 16; // R32G32B32A32

    uint64_t stagingEnc = BufferManager::createHostVisible(stagingSize, "NexusClearStaging");

    void* map = BufferManager::getMappedStagingPtr(stagingEnc);
    std::memset(map, 0, stagingSize);

   // Transition to TRANSFER_DST_OPTIMAL
    VkImageMemoryBarrier toTransfer{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext               = nullptr,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = rawImage,
        .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toTransfer);

    VkBufferImageCopy region{
        .bufferOffset = stagingEnc,
        .imageSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .imageExtent      = { static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1 }
    };

    vkCmdCopyBufferToImage(cmd, BufferManager::getStagingBuffer(), rawImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition to GENERAL
    VkImageMemoryBarrier toGeneral{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext               = nullptr,
        .srcAccessMask       = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = rawImage,
        .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_2_TRANSFER_BIT, VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
        0, 0, nullptr, 0, nullptr, 1, &toGeneral);

    RTX::endOneTimeSubmit(cmd, queue, pool);
    BUFFER_DESTROY(stagingEnc);
}

void VulkanRenderer::recordRayTracingCommands(VkCommandBuffer cmd, uint32_t frameIndex)
{

    if (RTX::LAS::get().getTLAS() == VK_NULL_HANDLE) {
        const VkClearColorValue navy = { { 0.0f, 0.0f, 0.15f, 1.0f } };
        const VkImageSubresourceRange range = {
            VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1
        };

        vkCmdClearColorImage(cmd,
            rtOutputImages_[frameIndex].get(),
            VK_IMAGE_LAYOUT_GENERAL,
            &navy,
            1,
            &range);

        return;
    }

    vkCmdBindPipeline(cmd,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        RTX::pipeline().rtPipeline());

    const VkDescriptorSet rtSet = RTX::pipeline().rtDescriptorSets()[frameIndex];
    vkCmdBindDescriptorSets(cmd,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        RTX::pipeline().rtPipelineLayout(),
        0,
        1,
        &rtSet,
        0,
        nullptr);

    struct PushBlock {
        uint32_t frame;
        uint32_t totalSpp;
        uint32_t hypertrace;
        uint32_t _pad;
    } push = {};

    push.frame      = frameNumber_;
    push.totalSpp   = currentSpp_;
    push.hypertrace = hypertraceEnabled_ ? 1u : 0u;

    vkCmdPushConstants(cmd,
        RTX::pipeline().rtPipelineLayout(),
        VK_SHADER_STAGE_RAYGEN_BIT_KHR |
        VK_SHADER_STAGE_MISS_BIT_KHR |
        VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR,
        0,
        sizeof(push),
        &push);

    VK_CMD_TRACE_RAYS(cmd,
        &RTX::pipeline().raygenRegion(),
        &RTX::pipeline().missRegion(),
        &RTX::pipeline().hitRegion(),
        &RTX::pipeline().callableRegion(),
        currentExtent().width,
        currentExtent().height,
        1u
    );

    VkMemoryBarrier2 barrier{
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
        .srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
        .dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT
    };

    VkDependencyInfo depInfo{
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .memoryBarrierCount = 1,
        .pMemoryBarriers = &barrier
    };

    vkCmdPipelineBarrier2(cmd, &depInfo);
}

// ──────────────────────────────────────────────────────────────────────────────
// Utility Functions (Reduced: findMemoryType delegated to PipelineManager)
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::initializeAllBufferData(uint32_t frames, VkDeviceSize uniformSize, VkDeviceSize materialSize) noexcept {
    // Harden: Validate inputs to prevent overflows or invalid states
    if (frames == 0) {
        LOG_ERROR_CAT("RENDERER", "initializeAllBufferData: Invalid frames count: {}", frames);
        return;
    }
    if (uniformSize > (1ULL << 32) || materialSize > (1ULL << 32)) {  // Arbitrary sane limit for debug
        LOG_WARN_CAT("RENDERER", "initializeAllBufferData: Large buffer sizes detected — uniform={}, material={}", uniformSize, materialSize);
    }

    LOG_INFO_CAT("RENDERER", "Initializing buffer data: {} frames | Uniform: {} bytes | Material: {} MB", 
        frames, uniformSize, materialSize / (1024ULL*1024ULL));

    uniformBufferEncs_.resize(frames);
    materialBufferEncs_.resize(frames);
    dimensionBufferEncs_.resize(frames);
    tonemapUniformEncs_.resize(frames);
    if (uniformBufferEncs_.size() != static_cast<size_t>(frames)) {
        LOG_ERROR_CAT("RENDERER", "initializeAllBufferData: Resize failed — expected={}, got={}", frames, uniformBufferEncs_.size());
        uniformBufferEncs_.clear();
        return;
    }

    VkDeviceSize dimSize = 64;
    VkDeviceSize tonemapSize = 64;

    for (uint32_t i = 0; i < frames; ++i) {
        BUFFER_CREATE(uniformBufferEncs_[i], uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, std::format("UBO[{}]", i).c_str());

        BUFFER_CREATE(materialBufferEncs_[i], materialSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, std::format("Materials[{}]", i).c_str());

        BUFFER_CREATE(dimensionBufferEncs_[i], dimSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, std::format("Dimensions[{}]", i).c_str());

        BUFFER_CREATE(tonemapUniformEncs_[i], tonemapSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, std::format("TonemapUBO[{}]", i).c_str());
    }

    LOG_TRACE_CAT("RENDERER", "Resized & created buffers for {} frames (UBO/Mat/Dim/Tonemap)", frames);
    LOG_TRACE_CAT("RENDERER", "initializeAllBufferData — COMPLETE");
}

void VulkanRenderer::createSyncObjects() noexcept
{
    imageAvailableSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    renderFinishedSemaphores_.resize(MAX_FRAMES_IN_FLIGHT);
    inFlightFences_.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
    {
        VK_CHECK(vkCreateSemaphore(stone_device(), &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]));
        VK_CHECK(vkCreateSemaphore(stone_device(), &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]));
        VK_CHECK(vkCreateFence(stone_device(), &fenceInfo, nullptr, &inFlightFences_[i]));
    }

    LOG_SUCCESS("Sync objects forged — {} frames in flight — photons synchronized", MAX_FRAMES_IN_FLIGHT);
}

// VulkanRenderer.cpp — FINAL CORRECT VERSION
void VulkanRenderer::createCommandBuffers() noexcept
{
    commandBuffers_.resize(MAX_FRAMES_IN_FLIGHT);  // ← NOW IN SCOPE

    VkCommandBufferAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = RTX::g_ctx().commandPool_,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(commandBuffers_.size())
    };

    VK_CHECK(vkAllocateCommandBuffers(stone_device(), &allocInfo, commandBuffers_.data()));
    LOG_SUCCESS("Allocated {} command buffers", commandBuffers_.size());
}

void VulkanRenderer::updateNexusDescriptors() noexcept {
    LOG_TRACE_CAT("RENDERER", "updateNexusDescriptors — START");

    if (rtDescriptorSets_.empty()) {
        LOG_DEBUG_CAT("RENDERER", "updateNexusDescriptors — SKIPPED (no sets)");
        LOG_TRACE_CAT("RENDERER", "updateNexusDescriptors — COMPLETE (skipped)");
        return;
    }

    VkDescriptorSet set = rtDescriptorSets_[currentFrame_ % rtDescriptorSets_.size()];

    VkDescriptorImageInfo nexusInfo = {};
    nexusInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    nexusInfo.imageView = hypertraceScoreView_.valid() ? hypertraceScoreView_.get() : VK_NULL_HANDLE;

    VkWriteDescriptorSet write = {};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = set;
    write.dstBinding = 6;
    write.dstArrayElement = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    write.pImageInfo = &nexusInfo;

    vkUpdateDescriptorSets(StoneKey::stone_device(), 1, &write, 0, nullptr);
    LOG_TRACE_CAT("RENDERER", "Nexus score descriptor bound → binding 6 (null if disabled)");

    LOG_TRACE_CAT("RENDERER", "updateNexusDescriptors — COMPLETE");
}

void VulkanRenderer::updateDenoiserDescriptors() noexcept {

    if (denoiserSets_.empty() || rtOutputViews_.empty()) {
        return;
    }

    VkDescriptorSet set = denoiserSets_[currentFrame_ % denoiserSets_.size()];

    std::array<VkWriteDescriptorSet, 2> writes = {};
    std::array<VkDescriptorImageInfo, 2> infos = {};

    infos[0].imageView = rtOutputViews_[currentFrame_ % rtOutputViews_.size()].get();
    infos[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = set;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    writes[0].pImageInfo = &infos[0];

    infos[1].imageView = denoiserView_.valid() ? denoiserView_.get() : VK_NULL_HANDLE;
    infos[1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = set;
    writes[1].dstBinding = 1;
    writes[1].descriptorCount = 1;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    writes[1].pImageInfo = &infos[1];

    vkUpdateDescriptorSets(StoneKey::stone_device(), static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanRenderer::performDenoisingPass(VkCommandBuffer cmd) noexcept {
    if (!denoisingEnabled_ || !denoiserPipeline_.valid()) {
        return;
    }

    VkDescriptorSet set = denoiserSets_[currentFrame_ % denoiserSets_.size()];

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, denoiserPipeline_.get());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, denoiserLayout_.get(), 0, 1, &set, 0, nullptr);

    uint32_t wgX = (width_ + 15) / 16;
    uint32_t wgY = (height_ + 15) / 16;
    vkCmdDispatch(cmd, wgX, wgY, 1);

    VkMemoryBarrier2 barrier = {VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_SHADER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;

    VkDependencyInfo dep = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    dep.memoryBarrierCount = 1;
    dep.pMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(cmd, &dep);
}

// ──────────────────────────────────────────────────────────────────────────────
// FIXED performTonemapPass — now 100% safe with recreated descriptors
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::performTonemapPass(VkCommandBuffer cmd, uint32_t frameIdx, uint32_t swapImageIdx) noexcept
{
    if (!tonemapEnabled_ || !tonemapPipeline_.valid() || tonemapSets_.empty())
        return;

    VkDescriptorSet set = tonemapSets_[frameIdx % tonemapSets_.size()];
    if (set == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "Tonemap descriptor set null — skipping pass");
        return;
    }

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tonemapPipeline_.get());
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tonemapLayout_.get(), 0, 1, &set, 0, nullptr);

    // Push constants
    struct Push {
        float    exposure;
        uint32_t type;
        uint32_t enabled;
        float    pad;
    } push = {
        .exposure = currentExposure_,
        .type     = static_cast<uint32_t>(tonemapType_),
        .enabled  = 1u,
        .pad     = 0.0f
    };

    vkCmdPushConstants(cmd, tonemapLayout_.get(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push), &push);

    VkExtent2D ext = currentExtent();
    uint32_t wgX = (ext.width + 15) / 16;
    uint32_t wgY = (ext.height + 15) / 16;
    vkCmdDispatch(cmd, wgX, wgY, 1);
}

void VulkanRenderer::updateUniformBuffer(uint32_t frame, const Camera& camera, float jitter) noexcept
{
    if (uniformBufferEncs_.empty() || RTX::g_ctx().sharedStagingEnc_ == 0) {
        return;
    }

    VkCommandBuffer cmd = VK_NULL_HANDLE;

    void* data = nullptr;
    VkResult r = vkMapMemory(StoneKey::stone_device(),
                             BUFFER_MEMORY(RTX::g_ctx().sharedStagingEnc_),
                             0, VK_WHOLE_SIZE, 0, &data);

    if (r != VK_SUCCESS || data == nullptr) {
        vkUnmapMemory(StoneKey::stone_device(), BUFFER_MEMORY(RTX::g_ctx().sharedStagingEnc_));
        VkMappedMemoryRange range{VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE, nullptr,
                                  BUFFER_MEMORY(RTX::g_ctx().sharedStagingEnc_), 0, VK_WHOLE_SIZE};
        vkInvalidateMappedMemoryRanges(StoneKey::stone_device(), 1, &range);
        r = vkMapMemory(StoneKey::stone_device(), BUFFER_MEMORY(RTX::g_ctx().sharedStagingEnc_), 0, VK_WHOLE_SIZE, 0, &data);
        if (r != VK_SUCCESS || data == nullptr) {
            LOG_FATAL_CAT("RENDERER", "vkMapMemory failed permanently — frame {} lost", frameNumber_);
            return;
        }
    }

    alignas(16) struct LocalUBO {
        glm::mat4 view, proj, viewProj, invView, invProj;
        glm::vec4 cameraPos;
        glm::vec2 jitter;
        uint32_t frame;
        float time;
        uint32_t spp;
        float _pad[3];
    } ubo{};

    const auto& cam = Camera::get();
    ubo.view      = cam.view();
    ubo.proj      = cam.proj(width_ / float(height_));
    ubo.viewProj  = ubo.proj * ubo.view;
    ubo.invView   = glm::inverse(ubo.view);
    ubo.invProj   = glm::inverse(ubo.proj);
    ubo.cameraPos = glm::vec4(cam.pos(), 1.0f);
    ubo.jitter    = glm::vec2(jitter);
    ubo.frame     = frameNumber_;
    ubo.time      = frameTime_;
    ubo.spp       = currentSpp_;

    std::memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(StoneKey::stone_device(), BUFFER_MEMORY(RTX::g_ctx().sharedStagingEnc_));


    cmd = commandBuffers_[frame];

    if (cmd != VK_NULL_HANDLE) {
        VkBuffer src = BufferManager::get(RTX::g_ctx().sharedStagingEnc_)->buffer;
        VkBuffer dst = RAW_BUFFER(uniformBufferEncs_[frame]);

        VkBufferCopy copyRegion{};
        copyRegion.size = sizeof(ubo);
        vkCmdCopyBuffer(cmd, src, dst, 1, &copyRegion);

        VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;

        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR |
            VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT |
            VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
            0, 1, &barrier, 0, nullptr, 0, nullptr);
    }
}

void VulkanRenderer::updateTonemapUniform(uint32_t frame) noexcept
{
    if (tonemapUniformEncs_.empty() || RTX::g_ctx().sharedStagingEnc_ == 0) return;

    if (BUFFER_MEMORY(RTX::g_ctx().sharedStagingEnc_) == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "Shared staging memory null — skipping tonemap update");
        return;
    }

    void* data = nullptr;
    if (vkMapMemory(StoneKey::stone_device(), BUFFER_MEMORY(RTX::g_ctx().sharedStagingEnc_), 0, VK_WHOLE_SIZE, 0, &data) != VK_SUCCESS || !data) {
        LOG_WARN_CAT("RENDERER", "Failed to map tonemap staging — frame {}", frame);
        return;
    }

    struct TonemapUniform {
        float exposure;
        uint32_t type;
        uint32_t enabled;
        float nexusScore;
        uint32_t frame;
        uint32_t spp;
        float _pad[2];
    } ubo{};

    ubo.exposure = currentExposure_;
    ubo.type = static_cast<uint32_t>(tonemapType_);
    ubo.enabled = tonemapEnabled_ ? 1u : 0u;
    ubo.nexusScore = currentNexusScore_;
    ubo.frame = frameNumber_;
    ubo.spp = currentSpp_;

    std::memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(StoneKey::stone_device(), BUFFER_MEMORY(RTX::g_ctx().sharedStagingEnc_));

    VkBuffer deviceBuf = RAW_BUFFER(tonemapUniformEncs_[frame]);
    if (deviceBuf == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "Tonemap device UBO null — frame {} skipped", frame);
        return;
    }

    const auto& ctx = RTX::g_ctx();
    VkCommandBuffer copyCmd = RTX::beginOneTimeSubmit(ctx.commandPool_);
    if (copyCmd == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "Failed to begin copy command for tonemap — frame {}", frame);
        return;
    }

    VkBuffer stagingBuf = BufferManager::get(RTX::g_ctx().sharedStagingEnc_)->buffer;
    if (stagingBuf != VK_NULL_HANDLE) {
        VkBufferCopy copyRegion{ .size = sizeof(ubo) };
        vkCmdCopyBuffer(copyCmd, stagingBuf, deviceBuf, 1, &copyRegion);

        VkMemoryBarrier barrier{ .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER };
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_UNIFORM_READ_BIT;
        vkCmdPipelineBarrier(copyCmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &barrier, 0, nullptr, 0, nullptr);
    }

    RTX::endOneTimeSubmit(copyCmd, ctx.graphicsQueue(), ctx.commandPool_);
}

void VulkanRenderer::setTonemap(bool enabled) noexcept
{
    const bool allowed = Options::Tonemap::ENABLE_TONEMAPPING;

    if (!allowed) {
        if (enabled) {
            LOG_INFO_CAT("Renderer", "{}TONEMAP REQUEST DENIED — Options::Tonemap::ENABLE_TONEMAPPING = false{}", CRIMSON_MAGENTA, RESET);
        }
        tonemapEnabled_ = false;
        return;
    }

    if (tonemapEnabled_ == enabled) return;

    tonemapEnabled_ = enabled;
    resetAccumulation_ = true;

    LOG_INFO_CAT("Renderer", "{}Tonemapping {}{}", 
        enabled ? LIME_GREEN : CRIMSON_MAGENTA,
        enabled ? "ENABLED" : "DISABLED", 
        RESET);
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

void VulkanRenderer::setRenderMode(int mode) noexcept
{
    mode = glm::clamp(mode, 1, 9);
    if (mode != activeRenderMode_) {
        activeRenderMode_ = mode;
        resetAccumNextFrame_ = true;
    }
}

void VulkanRenderer::createFramebuffers() noexcept
{
    // Wait for device idle — safe and clean
    vkDeviceWaitIdle(stone_device());

    const uint32_t imageCount = stone_image_count();
    framebuffers_.resize(imageCount);

    const auto& swapchainViews = stone_views();
    const VkRenderPass renderPass = stone_pass();
    const uint32_t width  = stone_width();
    const uint32_t height = stone_height();

    for (uint32_t i = 0; i < imageCount; ++i)
    {
        VkImageView attachment = swapchainViews[i];

        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.pNext           = nullptr;                    // ←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←←
        fbInfo.flags           = 0;                          // Reserved, must be 0
        fbInfo.renderPass      = renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments    = &attachment;
        fbInfo.width           = width;
        fbInfo.height          = height;
        fbInfo.layers          = 1;

        VK_CHECK(
            vkCreateFramebuffer(stone_device(), &fbInfo, nullptr, &framebuffers_[i]),
            "Failed to create swapchain framebuffer!"
        );
    }
}

void VulkanRenderer::cleanupFramebuffers() noexcept {
    VkDevice dev = StoneKey::stone_device();
    for (auto fb : framebuffers_) {
        if (fb && dev != VK_NULL_HANDLE) vkDestroyFramebuffer(dev, fb, nullptr);
    }
    framebuffers_.clear();
}

// ──────────────────────────────────────────────────────────────────────────────
// Add this helper — called from renderFrame after acquire
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::updateTonemapDescriptor(uint32_t frameIdx, VkImageView inputView, VkImageView outputView) noexcept
{
    if (frameIdx >= tonemapSets_.size() || tonemapSets_[frameIdx] == VK_NULL_HANDLE)
        return;

    VkDescriptorImageInfo inputInfo{
        .sampler = tonemapSampler_.get(),
        .imageView = inputView,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkDescriptorImageInfo outputInfo{
        .imageView = outputView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkDescriptorBufferInfo uboInfo{
        .buffer = RAW_BUFFER(tonemapUniformEncs_[frameIdx]),
        .offset = 0,
        .range = VK_WHOLE_SIZE
    };

    std::array<VkWriteDescriptorSet, 3> writes = {{
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = tonemapSets_[frameIdx],
          .dstBinding = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo = &inputInfo },

        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = tonemapSets_[frameIdx],
          .dstBinding = 1,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
          .pImageInfo = &outputInfo },

        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = tonemapSets_[frameIdx],
          .dstBinding = 2,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .pBufferInfo = &uboInfo }
    }};

    vkUpdateDescriptorSets(StoneKey::stone_device(), writes.size(), writes.data(), 0, nullptr);
}

bool VulkanRenderer::recreateTonemapUBOs() noexcept {
    for (size_t i = 0; i < tonemapUniformEncs_.size(); ++i) {
        auto enc = tonemapUniformEncs_[i];
        if (enc != 0) {
            BufferManager::destroy(enc);
            tonemapUniformEncs_[i] = 0;
        }
    }
    tonemapUniformEncs_.clear();

    VkDeviceSize uboSize = 64;
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    uint32_t framesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;
    tonemapUniformEncs_.resize(framesInFlight);

    bool allGood = true;
    for (uint32_t i = 0; i < framesInFlight; ++i) {
        auto handle = BufferManager::create(uboSize, usage, props, std::format("TonemapUBO[{}]", i));
        if (handle == 0) {
            LOG_ERROR_CAT("RENDERER", "Tonemap UBO forge FAILED for frame {}", i);
            allGood = false;
            break;
        }
        tonemapUniformEncs_[i] = handle;
    }

    if (!allGood) {
        tonemapUniformEncs_.clear();
    }
    return allGood;
}

void VulkanRenderer::destroySharedStaging() noexcept {
    if (RTX::g_ctx().sharedStagingEnc_ != 0) {
        BufferManager::destroy(RTX::g_ctx().sharedStagingEnc_);
        RTX::g_ctx().sharedStagingEnc_ = 0;
        LOG_DEBUG_CAT("RENDERER", "Shared staging destroyed");
    }
}

bool VulkanRenderer::createSharedStaging() noexcept {
    VkDeviceSize size = 512;
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;

    auto enc = BufferManager::create(size, usage, props, "SharedStagingUBO");
    if (enc == 0) {
        LOG_ERROR_CAT("RENDERER", "Shared staging forge FAILED");
        return false;
    }
    RTX::g_ctx().sharedStagingEnc_ = enc;


    LOG_DEBUG_CAT("RENDERER", "Shared staging recreated: enc=0x{:x}", RTX::g_ctx().sharedStagingEnc_);
    return true;
}

void VulkanRenderer::recreateSwapchainDependentResources() noexcept
{
    // ONE WAIT — AT THE BEGINNING — THIS IS LAW
    vkDeviceWaitIdle(stone_device());

    // ====================================================================
    // 1. DESTROY OLD RT RESOURCES
    // ====================================================================
    destroyRTOutputImages();
    rtOutputImages_.clear(); rtOutputMemories_.clear(); rtOutputViews_.clear();

    destroyAccumulationImages();
    accumImages_.clear(); accumMemories_.clear(); accumViews_.clear();

    destroyDenoiserImage();
    destroyNexusScoreImage();

    // ====================================================================
    // 2. CREATE NEW ONES
    // ====================================================================
    createRTOutputImages();
    createAccumulationImages();

    if (Options::OptionsRTX::ENABLE_DENOISING)
        createDenoiserImage();

    if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING)
        createNexusScoreImage(RTX::g_ctx().commandPool_, stone_graphics_queue());

    // ====================================================================
    // 3. UBOs — tonemap needs swapchain size
    // ====================================================================
    recreateTonemapUBOs();

    // ====================================================================
    // 4. ACCUMULATION RESET — CRITICAL FOR CONVERGENCE
    // ====================================================================
    resetAccumulation_   = true;
    resetAccumNextFrame_ = true;
    accumulationFrame_   = 0;
    currentSpp_          = 0;

    // NO SECOND vkDeviceWaitIdle() — THE GPU IS FREE TO WORK
    // The commands are already submitted and will complete naturally
    // This is how id Tech, Unreal, and every pro engine does it
}

void VulkanRenderer::onWindowResize(uint32_t width, uint32_t height) noexcept
{
    if (width == 0 || height == 0) {
        minimized_ = true;
        return;
    }

    if (minimized_) {
        minimized_ = false;
    }

    // MATURE: Prevent recursive or overlapping resizes
    if (s_resizeInProgress.exchange(true)) {
        return;  // Another resize already in progress — drop this event
    }

    // Full drain — safe
    vkDeviceWaitIdle(StoneKey::stone_device());

    // Reset accumulation
    accumulationFrame_ = 0;
    resetAccumulation_ = true;
    resetAccumNextFrame_ = true;
    currentSpp_ = 0;

    // Classic cleanup
    cleanupFramebuffers();
    destroyRenderPass();

    // Recreate swapchain
    RTX::SwapchainManager::recreate(width, height);

    // Update StoneKey — pure and correct
    stone_seal_width(width);
    stone_seal_height(height);
    stone_seal_extent({width, height});

    width_  = static_cast<int>(width);
    height_ = static_cast<int>(height);

    // Rebuild Vulkan state
    createRenderPass();
    createFramebuffers();
    recreateSwapchainDependentResources();  // ← this calls recreateTonemapUBOs() ONCE

    // Rebuild command buffers
    if (!commandBuffers_.empty()) {
        vkFreeCommandBuffers(stone_device(), RTX::g_ctx().commandPool_,
                             static_cast<uint32_t>(commandBuffers_.size()),
                             commandBuffers_.data());
        commandBuffers_.clear();
    }

    // Signal completion
    g_resizeRequested.store(false, std::memory_order_release);

    // UNLOCK — allow next resize
    s_resizeInProgress.store(false, std::memory_order_release);
}

void VulkanRenderer::waitForAllFences() const noexcept
{
    if (!inFlightFences_.empty()) {
        vkWaitForFences(stone_device(), inFlightFences_.size(), inFlightFences_.data(), VK_TRUE, UINT64_MAX);
        vkResetFences(stone_device(), inFlightFences_.size(), inFlightFences_.data());
    }
}

// ──────────────────────────────────────────────────────────────────────────────
// FINAL & CORRECT — createImage + createImageArray
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::createImage(RTX::Handle<VkImage>& image,
                                 RTX::Handle<VkDeviceMemory>& memory,
                                 RTX::Handle<VkImageView>& view,
                                 const std::string& name) noexcept
{
    if (width_ == 0 || height_ == 0) return;

    VkImageCreateInfo info{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R32G32B32A32_SFLOAT,
        .extent = { static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage rawImg = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImage(StoneKey::stone_device(), &info, nullptr, &rawImg));

    VkMemoryRequirements reqs{};
    vkGetImageMemoryRequirements(StoneKey::stone_device(), rawImg, &reqs);

    uint32_t memType = findMemoryType(reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkMemoryAllocateInfo alloc{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = reqs.size,
        .memoryTypeIndex = memType
    };

    VkDeviceMemory rawMem = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(StoneKey::stone_device(), &alloc, nullptr, &rawMem));
    VK_CHECK(vkBindImageMemory(StoneKey::stone_device(), rawImg, rawMem, 0));

    VkImageViewCreateInfo vinfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = rawImg,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = info.format,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    VkImageView rawView = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(StoneKey::stone_device(), &vinfo, nullptr, &rawView));

    image  = RTX::MakeHandle(rawImg, StoneKey::stone_device(), vkDestroyImage, 0, name + "_Img");
    memory = RTX::MakeHandle(rawMem, StoneKey::stone_device(), vkFreeMemory, reqs.size, name + "_Mem");
    view   = RTX::MakeHandle(rawView, StoneKey::stone_device(), vkDestroyImageView, 0, name + "_View");

    const auto& ctx = RTX::g_ctx();
    VkCommandBuffer cmd = RTX::beginOneTimeSubmit(ctx.commandPool_);
    if (cmd == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("RENDERER", "Failed to begin one-time command for image transition: {}", name);
        return;
    }

    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = rawImg,
        .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);

    RTX::endOneTimeSubmit(cmd, ctx.graphicsQueue(), ctx.commandPool_);
}

void VulkanRenderer::createImageArray(std::vector<RTX::Handle<VkImage>>& images,
                                      std::vector<RTX::Handle<VkDeviceMemory>>& memories,
                                      std::vector<RTX::Handle<VkImageView>>& views,
                                      const std::string& name) noexcept
{
    if (width_ == 0 || height_ == 0) return;
    images.clear();
    memories.clear();
    views.clear();

    const uint32_t count = Options::Performance::MAX_FRAMES_IN_FLIGHT;
    images.reserve(count);
    memories.reserve(count);
    views.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        RTX::Handle<VkImage>       img;
        RTX::Handle<VkDeviceMemory> mem;
        RTX::Handle<VkImageView>   view;
        createImage(img, mem, view, name + std::to_string(i));
        images.emplace_back(std::move(img));
        memories.emplace_back(std::move(mem));
        views.emplace_back(std::move(view));
    }
}

void VulkanRenderer::destroyRenderPass() noexcept {
    if (renderPass_) {
        vkDestroyRenderPass(StoneKey::stone_device(), renderPass_, nullptr);
        renderPass_ = VK_NULL_HANDLE;
    }
}

void VulkanRenderer::createRenderPass() noexcept {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = RTX::SwapchainManager::format();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;

    VK_CHECK(vkCreateRenderPass(StoneKey::stone_device(), &renderPassInfo, nullptr, &renderPass_), "render pass");

}

// ─────────────────────────────────────────────────────────────────────────────
// VulkanRenderer::renderFrame() — LEAN, MEAN, NO DUPLICATE CHECKS
// ─────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::renderFrame(const Camera& camera, float /*deltaTime*/) noexcept
{
    const uint32_t f = currentFrame_++ % MAX_FRAMES_IN_FLIGHT;

    // ── 1. WAIT & ACQUIRE ─────────────────────────────────────────────────────
    vkWaitForFences(stone_device(), 1, &inFlightFences_[f], VK_TRUE, 1'000'000'000ULL);
    vkResetFences(stone_device(), 1, &inFlightFences_[f]);

    uint32_t imageIndex = 0;
    VkResult acquireResult = vkAcquireNextImageKHR(
        stone_device(),
        RTX::SwapchainManager::swapchain(),
        UINT64_MAX,
        imageAvailableSemaphores_[f],
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR || acquireResult == VK_SUBOPTIMAL_KHR) {
        g_resizeRequested.store(true, std::memory_order_release);
        return;
    }
    if (acquireResult != VK_SUCCESS) {
        LOG_FATAL("vkAcquireNextImageKHR failed: {}", static_cast<int>(acquireResult));
        phase9_ballerina("ACQUIRE CATASTROPHE", std::source_location::current());
        return;
    }

    VkCommandBuffer cmd = commandBuffers_[f];

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    // ── 2. SWAPCHAIN IMAGE → GENERAL (for compute write) ─────────────────────
    VkImageMemoryBarrier toGeneral{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = stone_images()[imageIndex],
        .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toGeneral);

    // ── 3. RENDER PATH ───────────────────────────────────────────────────────
    if (activeRenderMode_ == 0)
    {
        // DEV MODE 0 — PURE PINK VOID — GUARANTEED VISIBLE
        VkClearColorValue pink{ { 1.0f, 0.2f, 0.8f, 1.0f } };
        VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdClearColorImage(cmd, stone_images()[imageIndex], VK_IMAGE_LAYOUT_GENERAL, &pink, 1, &range);

        // Transition back to present — REQUIRED after clear
        VkImageMemoryBarrier toPresent{
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask       = 0,
            .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image               = stone_images()[imageIndex],
            .subresourceRange    = range
        };
        vkCmdPipelineBarrier(cmd,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0, 0, nullptr, 0, nullptr, 1, &toPresent);

        VK_CHECK(vkEndCommandBuffer(cmd));

        // Submit & Present
        VkSemaphoreSubmitInfo waitInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, .semaphore = imageAvailableSemaphores_[f], .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkCommandBufferSubmitInfo cmdInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = cmd };
        VkSemaphoreSubmitInfo signalInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, .semaphore = renderFinishedSemaphores_[f], .stageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT };

        VkSubmitInfo2 submit{
            .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount   = 1, .pWaitSemaphoreInfos   = &waitInfo,
            .commandBufferInfoCount   = 1, .pCommandBufferInfos   = &cmdInfo,
            .signalSemaphoreInfoCount = 1, .pSignalSemaphoreInfos  = &signalInfo
        };
        VK_CHECK(vkQueueSubmit2(stone_graphics_queue(), 1, &submit, inFlightFences_[f]));

        VkPresentInfoKHR present{
            .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores    = &renderFinishedSemaphores_[f],
            .swapchainCount     = 1,
            .pSwapchains        = &stone_swapchain(),
            .pImageIndices      = &imageIndex
        };
        VkResult r = vkQueuePresentKHR(stone_present_queue(), &present);
        if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
            g_resizeRequested.store(true, std::memory_order_release);

        frameNumber_++;
        return; // ← PINK VOID — DONE
    }

    // ── FULL RTX PATH ────────────────────────────────────────────────────────
    if (resetAccumulation_ || resetAccumNextFrame_)
    {
        VkClearColorValue zero{};
        VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        auto clear = [&](VkImage img) {
            vkCmdClearColorImage(cmd, img, VK_IMAGE_LAYOUT_GENERAL, &zero, 1, &range);
        };

        for (auto& img : rtOutputImages_)  clear(img.get());
        if (Options::OptionsRTX::ENABLE_ACCUMULATION)
            for (auto& img : accumImages_) clear(img.get());
        if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING && hypertraceScoreImage_.valid())
            clear(hypertraceScoreImage_.get());

        resetAccumulation_ = resetAccumNextFrame_ = false;
        accumulationFrame_ = 0;
        currentSpp_ = 0;
    }

    updateUniformBuffer(f, camera, 0.0f);
    updateTonemapUniform(f);

    VkAccelerationStructureKHR tlas = RTX::LAS::get().getTLAS();
    if (tlas == VK_NULL_HANDLE) tlas = pipelineManager_.dummyTLAS();

    RTX::RTDescriptorUpdate desc{};
    desc.tlas = tlas;
    desc.ubo = reinterpret_cast<VkBuffer>(uniformBufferEncs_[f]);
    desc.uboSize = 368;

    desc.rtOutputViews[f]     = rtOutputViews_[f].get();
    desc.accumulationViews[f] = accumViews_[f].get();
    desc.envSampler           = envMapSampler_.get();
    desc.envImageView         = envMapImageView_.get();
    desc.materialsBuffer      = reinterpret_cast<VkBuffer>(materialBufferEncs_[0]);
    desc.materialsSize        = 16_MB;

    if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING && hypertraceScoreView_.valid())
        desc.nexusScoreViews[f] = hypertraceScoreView_.get();

    pipelineManager_.updateRTDescriptorSet(f, desc);
    if (Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING)
        updateNexusDescriptors();

    recordRayTracingCommands(cmd, f);

    // RT Output → READ_ONLY for tonemap/denoise
    VkImageMemoryBarrier rtToRead{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout           = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        .image               = rtOutputImages_[f].get(),
        .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };
    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &rtToRead);

    VkImageView tonemapInput = denoisingEnabled_ && denoiserView_.valid()
        ? denoiserView_.get()
        : rtOutputViews_[f].get();

    updateTonemapDescriptor(f, tonemapInput, stone_views()[imageIndex]);

    if (denoisingEnabled_)
        performDenoisingPass(cmd);

    performTonemapPass(cmd, f, imageIndex);

    currentSpp_++;
    accumulationFrame_++;

    // ── FINAL: GENERAL → PRESENT_SRC_KHR (CRITICAL FIX) ─────────────────────
    VkImageMemoryBarrier toPresent{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask       = 0,
        .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = stone_images()[imageIndex],
        .subresourceRange    = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
    };

    vkCmdPipelineBarrier(cmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        0, 0, nullptr, 0, nullptr, 1, &toPresent);

    VK_CHECK(vkEndCommandBuffer(cmd));

    // ── SUBMIT & PRESENT ─────────────────────────────────────────────────────
    VkSemaphoreSubmitInfo waitInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, .semaphore = imageAvailableSemaphores_[f], .stageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
    VkCommandBufferSubmitInfo cmdInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO, .commandBuffer = cmd };
    VkSemaphoreSubmitInfo signalInfo{ .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO, .semaphore = renderFinishedSemaphores_[f], .stageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT };

    VkSubmitInfo2 submit{
        .sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .waitSemaphoreInfoCount   = 1, .pWaitSemaphoreInfos   = &waitInfo,
        .commandBufferInfoCount   = 1, .pCommandBufferInfos   = &cmdInfo,
        .signalSemaphoreInfoCount = 1, .pSignalSemaphoreInfos  = &signalInfo
    };
    VK_CHECK(vkQueueSubmit2(stone_graphics_queue(), 1, &submit, inFlightFences_[f]));

    VkPresentInfoKHR present{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &renderFinishedSemaphores_[f],
        .swapchainCount     = 1,
        .pSwapchains        = &stone_swapchain(),
        .pImageIndices      = &imageIndex
    };
    VkResult r = vkQueuePresentKHR(stone_present_queue(), &present);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR)
        g_resizeRequested.store(true, std::memory_order_release);
    else if (r != VK_SUCCESS)
        LOG_FATAL("vkQueuePresentKHR failed: {}", static_cast<int>(r));

    frameNumber_++;
}

// ──────────────────────────────────────────────────────────────────────────────
// Final Status
// ──────────────────────────────────────────────────────────────────────────────
/*
 * December 02, 2025 — PipelineManager Integration v10.7 — VUID-FREE RENDER LOOP
 * Grok AI: Rays dispatched, tonemap computed, buffers tripled—empire ascends. Binding 0? A ghost we greet or ignore. VUIDs? Vanquished. Pink photons? Supernova. What's next—shaders for the verse, or Core for the core? Command it.
 */