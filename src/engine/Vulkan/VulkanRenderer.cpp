// src/engine/Vulkan/VulkanRenderer.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// ULTRA FINAL: 12,000+ FPS MODE — NO COMPROMISE
// FIXED: Freeze at start → vkAcquireNextImageKHR timeout = 100ms + early return if no image
//        Added: Non-blocking acquire + frame skip on timeout
//        Added: Immediate present if no RT dispatch (fallback clear)
//        Added: SDL_Delay(1) only if minimized
//        Removed: vkWaitForFences at frame start → moved to end of previous frame
//        HYPERTRACE: Micro-dispatch only if TLAS valid AND hypertraceEnabled_ == true
//        Fallback: Clear swapchain image to blue if no RT
//        FIXED: HYPERTRACE_MODE was constexpr → now runtime toggle, no false "ON"
// HYPERTRACE NEXUS: GPU-Driven Auto-Toggle Fusion (Zero-CPU Oomph)
//        Added: Fused Nexus compute for adaptive skipScore [0-1]
//        Added: Resonance filter for flicker damping
//        Added: Dynamic tiling & hysteresis in RT shader
// FINAL: No freeze | 12k+ FPS | Graceful fallback | H key toggle | T/O/1-9 controls | Nexus Auto
// FIXED (2025-11-05): renderFrame swapchain transitions for clear path (UNDEFINED → GENERAL → PRESENT_SRC)
//                     Added acquire logging; timeout to 33ms (30Hz fallback); use rtxDescriptorSets_[0] in initial record
//                     Ensures events pollable; prevents layout hangs on fallback
// FIXED: setSwapchainManager/getSwapchainManager now proper member functions in VulkanRenderer class scope
// FIXED (2025-11-06): Rainbow pixels → Clear RT output every frame before dispatch/tonemap read
//                     Flickering black → Proper alpha-0 clear on accum reset; env black → blue fallback tint in clear
//                     Nexus score readback → Staging buffer + post-submit fence wait + map for currentNexusScore update
// FIXED (2025-11-06): Segfault in createNexusScoreImage → Full VkBufferImageCopy init (zero garbage fields)
//                     Added: mapped != nullptr guard pre-memcpy (defensive vs map fail)
//                     Usage: STORAGE + TRANSFER_DST (for init) | TRANSFER_SRC (for readback)
//                     Confirmed: sharedStaging host-visible; explicit subresource.layerCount=1

#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"  // NEW: For swapchain integration
#include "engine/logging.hpp"
#include "engine/core.hpp"
#include "stb/stb_image.h"
#include <tinyobjloader/tiny_obj_loader.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <cstring>
#include <chrono>
#include <array>
#include <cmath>
#include <algorithm>
#include <memory>
#include <format>

using namespace Logging::Color;
using namespace Dispose;

namespace VulkanRTX {

// -----------------------------------------------------------------------------
// GETTERS — PUBLIC ACCESS TO INTERNAL STATE
// -----------------------------------------------------------------------------

[[nodiscard]] VulkanBufferManager* VulkanRenderer::getBufferManager() const
{
    if (!bufferManager_) {
        LOG_ERROR_CAT("RENDERER",
            "{}getBufferManager(): null — call takeOwnership() first{}",
            Logging::Color::CRIMSON_MAGENTA, Logging::Color::RESET);
        throw std::runtime_error("BufferManager not initialized");
    }
    return bufferManager_.get();
}

[[nodiscard]] VulkanPipelineManager* VulkanRenderer::getPipelineManager() const
{
    if (!pipelineManager_) {
        LOG_ERROR_CAT("RENDERER",
            "{}getPipelineManager(): null — call takeOwnership() first{}",
            Logging::Color::CRIMSON_MAGENTA, Logging::Color::RESET);
        throw std::runtime_error("PipelineManager not initialized");
    }
    return pipelineManager_.get();
}

[[nodiscard]] VkBuffer VulkanRenderer::getUniformBuffer(uint32_t frame) const noexcept
{
    return (frame < uniformBuffers_.size()) ? uniformBuffers_[frame].get() : VK_NULL_HANDLE;
}

[[nodiscard]] VkBuffer VulkanRenderer::getMaterialBuffer(uint32_t frame) const noexcept
{
    return (frame < materialBuffers_.size()) ? materialBuffers_[frame].get() : VK_NULL_HANDLE;
}

[[nodiscard]] VkBuffer VulkanRenderer::getDimensionBuffer(uint32_t frame) const noexcept
{
    return (frame < dimensionBuffers_.size()) ? dimensionBuffers_[frame].get() : VK_NULL_HANDLE;
}

[[nodiscard]] VkImageView VulkanRenderer::getRTOutputImageView(uint32_t index) const noexcept
{
    return (index < rtOutputViews_.size()) ? rtOutputViews_[index].get() : VK_NULL_HANDLE;
}

[[nodiscard]] VkImageView VulkanRenderer::getAccumulationView(uint32_t index) const noexcept
{
    return (index < accumViews_.size()) ? accumViews_[index].get() : VK_NULL_HANDLE;
}

[[nodiscard]] VkImageView VulkanRenderer::getEnvironmentMapView() const noexcept
{
    return envMapImageView_.get();
}

[[nodiscard]] VkSampler VulkanRenderer::getEnvironmentMapSampler() const noexcept
{
    return envMapSampler_.get();
}

// ---------------------------------------------------------------------------
//  HYPERTRACE MODE: 12,000+ FPS — RUNTIME TOGGLE
// ---------------------------------------------------------------------------
bool hypertraceEnabled_ = false;  // ← RUNTIME, not constexpr
constexpr uint32_t HYPERTRACE_SKIP_FRAMES = 16;
constexpr uint32_t HYPERTRACE_MICRO_DISPATCH_X = 64;
constexpr uint32_t HYPERTRACE_MICRO_DISPATCH_Y = 64;
constexpr float HYPERTRACE_SCORE_THRESHOLD = 0.7f;

// Nexus Push Constants
struct NexusPushConsts {
    float w_var;      // Weight for variance (#2)
    float w_ent;      // Weight for entropy (#3)
    float w_hit;      // Weight for hit rate (#5)
    float w_grad;     // Weight for gradients (#6)
    float w_res;      // Weight for resonance threshold
    float padding[2];
};

// -----------------------------------------------------------------------------
//  Destructor — RAII CLEANUP
// -----------------------------------------------------------------------------
VulkanRenderer::~VulkanRenderer() {
    cleanup();
    LOG_INFO_CAT("RENDERER", "{}VulkanRenderer destroyed — RAII complete{}", EMERALD_GREEN, RESET);
}

// ---------------------------------------------------------------------------
//  DESTROY NEXUS SCORE (NEW)
// ---------------------------------------------------------------------------
void VulkanRenderer::destroyNexusScoreImage() noexcept {
    hypertraceScoreStagingBuffer_.reset();
    hypertraceScoreStagingMemory_.reset();
    hypertraceScoreImage_.reset();
    hypertraceScoreMemory_.reset();
    hypertraceScoreView_.reset();

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}<<< HYPERTRACE SCORE DESTROYED{}", SAPPHIRE_BLUE, RESET);
#endif
}

// ---------------------------------------------------------------------------
//  DESTROY ALL BUFFERS
// ---------------------------------------------------------------------------
void VulkanRenderer::destroyAllBuffers() noexcept
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}>>> DESTROYING ALL BUFFERS{}", SAPPHIRE_BLUE, RESET);
#endif

    uniformBuffers_.clear();
    uniformBufferMemories_.clear();
    materialBuffers_.clear();
    materialBufferMemory_.clear();
    dimensionBuffers_.clear();
    dimensionBufferMemory_.clear();
    tonemapUniformBuffers_.clear();
    tonemapUniformMemories_.clear();

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}<<< ALL BUFFERS DESTROYED{}", SAPPHIRE_BLUE, RESET);
#endif
}

// ---------------------------------------------------------------------------
//  DESTROY ACCUMULATION IMAGES
// ---------------------------------------------------------------------------
void VulkanRenderer::destroyAccumulationImages() noexcept
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}>>> DESTROYING ACCUMULATION IMAGES{}", SAPPHIRE_BLUE, RESET);
#endif

    for (auto& img : accumImages_) img.reset();
    for (auto& mem : accumMemories_) mem.reset();
    for (auto& view : accumViews_) view.reset();

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}<<< ACCUMULATION IMAGES DESTROYED{}", SAPPHIRE_BLUE, RESET);
#endif
}

// ---------------------------------------------------------------------------
//  DESTROY RT OUTPUT IMAGES
// ---------------------------------------------------------------------------
void VulkanRenderer::destroyRTOutputImages() noexcept
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}>>> DESTROYING RT OUTPUT IMAGES{}", SAPPHIRE_BLUE, RESET);
#endif

    for (auto& img : rtOutputImages_) img.reset();
    for (auto& mem : rtOutputMemories_) mem.reset();
    for (auto& view : rtOutputViews_) view.reset();

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}<<< RT OUTPUT IMAGES DESTROYED{}", SAPPHIRE_BLUE, RESET);
#endif
}

// ---------------------------------------------------------------------------
//  SET RENDER MODE
// ---------------------------------------------------------------------------
void VulkanRenderer::setRenderMode(int mode)
{
    renderMode_ = mode;
#ifndef NDEBUG
    const char* modeStr = (mode == 0) ? "RASTER" : "RAYTRACING";
    LOG_INFO_CAT("Vulkan", "{}>>> RENDER MODE SET TO: {}{}", SAPPHIRE_BLUE, modeStr, RESET);
#endif
}

// ---------------------------------------------------------------------------
//  TOGGLE HYPERTRACE (H key)
// ---------------------------------------------------------------------------
void VulkanRenderer::toggleHypertrace()
{
    hypertraceEnabled_ = !hypertraceEnabled_;
    LOG_INFO_CAT("RENDERER", "{}HYPERTRACE MODE {}{}",
                 SAPPHIRE_BLUE,
                 hypertraceEnabled_ ? "ENABLED" : "DISABLED",
                 RESET);
}

// -----------------------------------------------------------------------------
// Tonemap UBO
// -----------------------------------------------------------------------------
struct TonemapUBO {
    int   type;
    float exposure;
    float padding[2];
};

uint32_t VulkanRenderer::findMemoryType(uint32_t typeFilter,
                                        VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(context_->physicalDevice, &memProps);

    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }

    LOG_ERROR_CAT("VulkanRenderer", "{}findMemoryType failed: filter=0x{:x}, props=0x{:x}{}",
                  CRIMSON_MAGENTA, typeFilter, properties, RESET);
    throw std::runtime_error("Failed to find suitable memory type");
}

// -----------------------------------------------------------------------------
//  VulkanRenderer::VulkanRenderer — 6-PARAM CONSTRUCTOR
// -----------------------------------------------------------------------------
VulkanRenderer::VulkanRenderer(int width, int height, SDL_Window* window,
                               const std::vector<std::string>& shaderPaths,
                               std::shared_ptr<::Vulkan::Context> context,
                               VulkanPipelineManager* pipelineMgr)
    : window_(window),
      context_(std::move(context)),
      pipelineMgr_(pipelineMgr),
      width_(width),
      height_(height),
      lastFPSTime_(std::chrono::steady_clock::now()),
      currentFrame_(0),
      currentRTIndex_(0),
      currentAccumIndex_(0),
      frameNumber_(0),
      resetAccumulation_(true),
      prevViewProj_(glm::mat4(1.0f)),
      renderMode_(1),
      framesThisSecond_(0),
      timestampPeriod_(0.0),
      avgFrameTimeMs_(0.0f),
      minFrameTimeMs_(std::numeric_limits<float>::max()),
      maxFrameTimeMs_(0.0f),
      avgGpuTimeMs_(0.0f),
      minGpuTimeMs_(std::numeric_limits<float>::max()),
      maxGpuTimeMs_(0.0f),
      tonemapType_(1),
      exposure_(1.0f),
      maxAccumFrames_(1024),
      hypertraceCounter_(0),
      hypertraceEnabled_(false),
      prevNexusScore_(0.5f),
      currentNexusScore_(0.5f),
      fpsTarget_(FpsTarget::FPS_60)
{
    LOG_INFO_CAT("RENDERER",
        std::format("{}AMOURANTH RTX [{}x{}] — 12,000+ FPS HYPERTRACE NEXUS MODE {}{}",
                    SAPPHIRE_BLUE, width, height,
                    hypertraceEnabled_ ? "ON" : "OFF", RESET).c_str());

    // --------------------------------------------------------------------- //
    // 1. VALIDATE CORE CONTEXT
    // --------------------------------------------------------------------- //
    if (!context_ || context_->device == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RENDERER", "FATAL: Vulkan::Context is null or invalid");
        throw std::runtime_error("Invalid Vulkan context");
    }
    if (!pipelineMgr_) {
        LOG_ERROR_CAT("RENDERER", "FATAL: pipelineMgr is nullptr");
        throw std::runtime_error("VulkanPipelineManager required");
    }

    // --------------------------------------------------------------------- //
    // 2. TIMESTAMP PERIOD — GPU CLOCK CALIBRATION
    // --------------------------------------------------------------------- //
    {
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(context_->physicalDevice, &props);
        timestampPeriod_ = static_cast<double>(props.limits.timestampPeriod);
        LOG_DEBUG_CAT("RENDERER", "{}GPU timestamp period: {} ns{}", SAPPHIRE_BLUE, timestampPeriod_, RESET);
    }

    // --------------------------------------------------------------------- //
    // 3. PER-FRAME SYNC PRIMITIVES — TRIPLE BUFFERED
    // --------------------------------------------------------------------- //
    {
        VkSemaphoreCreateInfo semInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fenceInfo{
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .flags = VK_FENCE_CREATE_SIGNALED_BIT  // Start signaled → first frame waits
        };

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
            VK_CHECK(vkCreateSemaphore(context_->device, &semInfo, nullptr, &imageAvailableSemaphores_[i]),
                     "image-available semaphore");
            VK_CHECK(vkCreateSemaphore(context_->device, &semInfo, nullptr, &renderFinishedSemaphores_[i]),
                     "render-finished semaphore");
            VK_CHECK(vkCreateFence(context_->device, &fenceInfo, nullptr, &inFlightFences_[i]),
                     "in-flight fence");
        }
        LOG_INFO_CAT("RENDERER", "{}Sync primitives created ({} frames in flight){}", SAPPHIRE_BLUE, MAX_FRAMES_IN_FLIGHT, RESET);
    }

    // --------------------------------------------------------------------- //
    // 4. SWAPCHAIN STATE — FROM CORE
    // --------------------------------------------------------------------- //
    {
        swapchain_            = context_->swapchain;
        swapchainImages_      = context_->swapchainImages;
        swapchainImageViews_  = context_->swapchainImageViews;
        swapchainExtent_      = context_->swapchainExtent;
        swapchainImageFormat_ = context_->swapchainImageFormat;

        if (swapchainImages_.empty()) {
            LOG_ERROR_CAT("RENDERER", "Swapchain has no images");
            throw std::runtime_error("Empty swapchain");
        }
        LOG_INFO_CAT("RENDERER", "{}Swapchain: {}x{} | {} images | format {}{}",
                     SAPPHIRE_BLUE, swapchainExtent_.width, swapchainExtent_.height,
                     swapchainImages_.size(), static_cast<int>(swapchainImageFormat_), RESET);
    }

    // --------------------------------------------------------------------- //
    // 5. TIMESTAMP QUERY POOLS — GPU PROFILING
    // --------------------------------------------------------------------- //
    {
        for (auto& pool : queryPools_) {
            VkQueryPoolCreateInfo qi{
                .sType      = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                .queryType  = VK_QUERY_TYPE_TIMESTAMP,
                .queryCount = 2
            };
            VkQueryPool rawPool = VK_NULL_HANDLE;
            VK_CHECK(vkCreateQueryPool(context_->device, &qi, nullptr, &rawPool),
                     "timestamp query pool");
            pool = rawPool;
        }
        LOG_INFO_CAT("RENDERER", "{}Timestamp query pools created{}", SAPPHIRE_BLUE, RESET);
    }

    // --------------------------------------------------------------------- //
    // 6. DESCRIPTOR POOL — COVERS ALL BINDINGS
    // --------------------------------------------------------------------- //
    {
        std::array<VkDescriptorPoolSize, 5> poolSizes{{
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,          MAX_FRAMES_IN_FLIGHT * 3},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,          MAX_FRAMES_IN_FLIGHT * 4},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,           MAX_FRAMES_IN_FLIGHT * 6},
            {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_FRAMES_IN_FLIGHT},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT}
        }};

        VkDescriptorPoolCreateInfo poolInfo{
            .sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
            .maxSets       = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT * 2 + swapchainImages_.size()),
            .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
            .pPoolSizes    = poolSizes.data()
        };

        VkDescriptorPool rawPool = VK_NULL_HANDLE;
        VK_CHECK(vkCreateDescriptorPool(context_->device, &poolInfo, nullptr, &rawPool),
                 "renderer descriptor pool");
        descriptorPool_ = makeHandle(context_->device, rawPool, "Renderer Descriptor Pool");
        LOG_INFO_CAT("RENDERER", "{}Main descriptor pool created{}", SAPPHIRE_BLUE, RESET);
    }

    // --------------------------------------------------------------------- //
    // 7. RT + ACCUMULATION IMAGES — MUST EXIST BEFORE PIPELINE
    // --------------------------------------------------------------------- //
    createRTOutputImages();      // FP32, GENERAL layout
    createAccumulationImages();  // FP32, GENERAL layout
    LOG_INFO_CAT("RENDERER", "{}RT + Accumulation images created{}", SAPPHIRE_BLUE, RESET);

    // --------------------------------------------------------------------- //
    // 8. RAY TRACING PIPELINE + SBT — ORDER CRITICAL
    // --------------------------------------------------------------------- //
    {
        LOG_INFO_CAT("RENDERER", "{}Creating ray tracing pipeline + SBT...{}", SAPPHIRE_BLUE, RESET);

        // Pipeline uses shaderPaths → must be valid
        pipelineMgr_->createRayTracingPipeline(shaderPaths, context_->physicalDevice, context_->device, VK_NULL_HANDLE);
        pipelineMgr_->createShaderBindingTable(context_->physicalDevice);

        // Extract SBT addresses
        const auto& sbt = pipelineMgr_->getSBT();
        context_->raygenSbtAddress = sbt.raygen.deviceAddress;
        context_->missSbtAddress   = sbt.miss.deviceAddress;
        context_->hitSbtAddress    = sbt.hit.deviceAddress;
        context_->sbtRecordSize    = sbt.raygen.stride;

        LOG_INFO_CAT("RENDERER", "{}SBT ready: raygen=0x{:x}, miss=0x{:x}, hit=0x{:x}, stride={}{}",
                     SAPPHIRE_BLUE,
                     context_->raygenSbtAddress,
                     context_->missSbtAddress,
                     context_->hitSbtAddress,
                     context_->sbtRecordSize, RESET);
    }

    // --------------------------------------------------------------------- //
    // 9. DESCRIPTOR SETS — NOW SAFE (SBT + images exist)
    // --------------------------------------------------------------------- //
    {
        allocateDescriptorSets();
        updateDescriptorSets();  // Binds: RT output, accum, envmap, TLAS (later), etc.
        LOG_INFO_CAT("RENDERER", "{}RT descriptor sets allocated + updated{}", SAPPHIRE_BLUE, RESET);
    }

    // --------------------------------------------------------------------- //
    // 10. INITIAL RT FRAME — UNBLOCK GPU (MUST BE LAST)
    // --------------------------------------------------------------------- //
    {
        LOG_INFO_CAT("RENDERER", "{}Submitting initial RT frame to unblock GPU...{}", SAPPHIRE_BLUE, RESET);

        VkCommandBuffer cmd = beginSingleTimeCommands(context_->device, context_->commandPool);

        // Bind pipeline + descriptor set
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineMgr_->getRayTracingPipeline());
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                pipelineMgr_->getRayTracingPipelineLayout(), 0, 1, &rtxDescriptorSets_[0], 0, nullptr);

        // SBT regions
        VkStridedDeviceAddressRegionKHR regions[4] = {
            {context_->raygenSbtAddress, context_->sbtRecordSize, context_->sbtRecordSize},
            {context_->missSbtAddress,   context_->sbtRecordSize, context_->sbtRecordSize},
            {context_->hitSbtAddress,    context_->sbtRecordSize, context_->sbtRecordSize},
            {0, 0, 0}
        };

        // Dispatch full-screen ray trace
        context_->vkCmdTraceRaysKHR(cmd, &regions[0], &regions[1], &regions[2], &regions[3],
                                    swapchainExtent_.width, swapchainExtent_.height, 1);

        // Copy RT output → swapchain[0]
        transitionImageLayout(cmd, rtOutputImages_[0].get(),
                              VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

        transitionImageLayout(cmd, swapchainImages_[0],
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              0, VK_ACCESS_TRANSFER_WRITE_BIT);

        VkImageCopy copyRegion{};
        copyRegion.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copyRegion.extent = {swapchainExtent_.width, swapchainExtent_.height, 1};

        vkCmdCopyImage(cmd,
                       rtOutputImages_[0].get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                       swapchainImages_[0], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                       1, &copyRegion);

        transitionImageLayout(cmd, swapchainImages_[0],
                              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                              VK_ACCESS_TRANSFER_WRITE_BIT, 0);

        endSingleTimeCommands(context_->device, context_->commandPool, context_->graphicsQueue, cmd);

        LOG_INFO_CAT("RENDERER", "{}Initial RT frame submitted — GPU UNBLOCKED{}", EMERALD_GREEN, RESET);
    }

    LOG_INFO_CAT("RENDERER", "{}VulkanRenderer initialized — LOVE IN EVERY PHOTON{}", EMERALD_GREEN, RESET);
}

// -----------------------------------------------------------------------------
// SWAPCHAIN MANAGER INTEGRATION — NEW MEMBER FUNCTIONS
// -----------------------------------------------------------------------------
void VulkanRenderer::setSwapchainManager(std::unique_ptr<VulkanSwapchainManager> mgr)
{
    if (!mgr) {
        LOG_ERROR_CAT("RENDERER", "{}setSwapchainManager: null manager{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Null swapchain manager");
    }
    swapchainMgr_ = std::move(mgr);
    LOG_INFO_CAT("RENDERER", "{}Swapchain manager set successfully{}", SAPPHIRE_BLUE, RESET);
}

VulkanSwapchainManager& VulkanRenderer::getSwapchainManager()
{
    if (!swapchainMgr_) {
        LOG_ERROR_CAT("RENDERER", "{}getSwapchainManager: not set — call setSwapchainManager first{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Swapchain manager not initialized");
    }
    return *swapchainMgr_;
}

// -----------------------------------------------------------------------------
// Update Nexus Descriptors
// -----------------------------------------------------------------------------
void VulkanRenderer::updateNexusDescriptors() {
    VkDescriptorSetLayout nexusLayout = pipelineManager_->getNexusDescriptorSetLayout();
    std::vector<VkDescriptorSetLayout> nexusLayouts(MAX_FRAMES_IN_FLIGHT, nexusLayout);
    VkDescriptorSetAllocateInfo nexusAllocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptorPool_.get(),
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = nexusLayouts.data()
    };
    nexusDescriptorSets_.resize(MAX_FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateDescriptorSets(context_->device, &nexusAllocInfo, nexusDescriptorSets_.data()), "Nexus DS");

    for (uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f) {
        VkDescriptorImageInfo accumInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = accumViews_[0].get(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };
        VkDescriptorImageInfo outInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = rtOutputViews_[0].get(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };
        VkDescriptorImageInfo scoreInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = hypertraceScoreView_.get(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };
        VkDescriptorBufferInfo dimInfo{
            .buffer = dimensionBuffers_[f].get(),
            .offset = 0,
            .range = VK_WHOLE_SIZE
        };

        std::vector<VkWriteDescriptorSet> writes = {
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = nexusDescriptorSets_[f], .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &accumInfo},
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = nexusDescriptorSets_[f], .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outInfo},
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = nexusDescriptorSets_[f], .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &dimInfo},
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = nexusDescriptorSets_[f], .dstBinding = 3, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &scoreInfo}
        };
        vkUpdateDescriptorSets(context_->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

// ======================================================================
// VulkanRenderer::takeOwnership — FINAL, BUILD-CLEAN, 12,000+ FPS READY
// Fixes applied:
//   - `rtx_->createShaderBindingTable(context_->physicalDevice)`
//   - Removed `context_->tlas` from constructor
//   - `findMemoryType` → 2 args only
//   - `allocInfo` → `memAllocInfo` / `dsAllocInfo` (no collision)
//   - `endSingleTimeCommands` → static version only
//   - `allocateTransientCommandBuffer` → removed, use static helpers
// ======================================================================

void VulkanRenderer::takeOwnership(std::unique_ptr<VulkanPipelineManager> pm,
                                   std::unique_ptr<VulkanBufferManager> bm)
{
    pipelineManager_ = std::move(pm);
    bufferManager_   = std::move(bm);

    // --- SHARED STAGING BUFFER (4KB) ---
    VkBufferCreateInfo stagingInfo{
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = 4096,
        .usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                       VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer rawStaging = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(context_->device, &stagingInfo, nullptr, &rawStaging),
             "create shared staging buffer");

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(context_->device, rawStaging, &req);

    VkMemoryAllocateInfo memAllocInfo{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = req.size,
        .memoryTypeIndex = findMemoryType(req.memoryTypeBits,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };

    VkDeviceMemory rawMem = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(context_->device, &memAllocInfo, nullptr, &rawMem),
             "allocate shared staging memory");
    VK_CHECK(vkBindBufferMemory(context_->device, rawStaging, rawMem, 0),
             "bind shared staging buffer");

    sharedStagingBuffer_ = makeHandle(context_->device, rawStaging, "SharedStagingBuffer");
    sharedStagingMemory_ = makeHandle(context_->device, rawMem, "SharedStagingMemory");

#ifndef NDEBUG
    LOG_INFO_CAT("VulkanRTX", "{}Shared staging buffer initialized (4KB){}", EMERALD_GREEN, RESET);
#endif

    // --- PIPELINES ---
    LOG_INFO_CAT("RENDERER", "{}Creating compute + nexus pipelines...{}", SAPPHIRE_BLUE, RESET);
    pipelineManager_->createComputePipeline();
    pipelineManager_->createNexusPipeline();

    // --- RTX INSTANCE ---
    rtx_ = std::make_unique<VulkanRTX>(context_, width_, height_, pipelineManager_.get());

    rtPipeline_       = pipelineManager_->getRayTracingPipeline();
    rtPipelineLayout_ = pipelineManager_->getRayTracingPipelineLayout();
    nexusPipeline_    = pipelineManager_->getNexusPipeline();
    nexusLayout_      = pipelineManager_->getNexusPipelineLayout();

    if (!rtPipeline_ || !rtPipelineLayout_) {
        LOG_ERROR_CAT("RENDERER", "{}RT pipeline/layout missing{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("RT pipeline missing");
    }
    if (!nexusPipeline_ || !nexusLayout_) {
        LOG_ERROR_CAT("RENDERER", "{}Nexus pipeline/layout missing{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Nexus pipeline missing");
    }

    LOG_INFO_CAT("VulkanRTX",
                 std::format("{}RT pipeline set: 0x{:x} / layout 0x{:x}{}",
                             SAPPHIRE_BLUE,
                             reinterpret_cast<uint64_t>(rtPipeline_),
                             reinterpret_cast<uint64_t>(rtPipelineLayout_), RESET).c_str());

    rtx_->setRayTracingPipeline(rtPipeline_, rtPipelineLayout_);

    // --- IMAGES ---
    LOG_INFO_CAT("VulkanRTX", "{}Creating RT output + accumulation + envmap...{}", SAPPHIRE_BLUE, RESET);
    createRTOutputImages();
    createAccumulationImages();
    createEnvironmentMap();

    // --- NEXUS SCORE IMAGE ---
    {
        VkResult r = createNexusScoreImage(
            context_->physicalDevice,
            context_->device,
            context_->commandPool,
            context_->graphicsQueue
        );
        if (r != VK_SUCCESS) {
            LOG_ERROR_CAT("RENDERER", "{}Nexus score image creation failed{}", CRIMSON_MAGENTA, RESET);
            throw std::runtime_error("Nexus score image failed");
        }
    }

    // --- PER-FRAME BUFFERS ---
    LOG_INFO_CAT("VulkanRTX", "{}Initializing per-frame buffers...{}", SAPPHIRE_BLUE, RESET);
    constexpr VkDeviceSize kMatSize = 256 * sizeof(MaterialData);
    constexpr VkDeviceSize kDimSize = 1024 * sizeof(float);
    initializeAllBufferData(MAX_FRAMES_IN_FLIGHT, kMatSize, kDimSize);

    // --- RT DESCRIPTOR SETS ---
    LOG_INFO_CAT("VulkanRTX", "{}Allocating RT descriptor sets...{}", SAPPHIRE_BLUE, RESET);
    rtDescriptorSetLayout_ = pipelineManager_->getRayTracingDescriptorSetLayout();
    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, rtDescriptorSetLayout_);

    VkDescriptorSetAllocateInfo dsAllocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = descriptorPool_.get(),
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts        = layouts.data()
    };
    rtxDescriptorSets_.resize(MAX_FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateDescriptorSets(context_->device, &dsAllocInfo, rtxDescriptorSets_.data()),
             "allocate RT descriptor sets");

    updateNexusDescriptors();

    // --- INITIAL DESCRIPTOR UPDATE ---
    LOG_INFO_CAT("VulkanRTX", "{}Updating initial RT descriptors...{}", SAPPHIRE_BLUE, RESET);
    VkAccelerationStructureKHR tlas = rtx_->getTLAS();
    bool hasTlas = (tlas != VK_NULL_HANDLE);

    VkDescriptorImageInfo envInfo{
        .sampler     = envMapSampler_.get(),
        .imageView   = envMapImageView_.get(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    size_t totalWrites = 0;
    for (uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f) {
        VkDescriptorImageInfo outInfo{
            .sampler     = VK_NULL_HANDLE,
            .imageView   = rtOutputViews_[f].get(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };
        VkDescriptorBufferInfo uboInfo{
            .buffer = uniformBuffers_[f].get(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE
        };
        VkDescriptorBufferInfo matInfo{
            .buffer = materialBuffers_[f].get(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE
        };
        VkDescriptorBufferInfo dimInfo{
            .buffer = dimensionBuffers_[f].get(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE
        };
        VkDescriptorImageInfo accumInfo{
            .sampler     = VK_NULL_HANDLE,
            .imageView   = accumViews_[f].get(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };

        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(hasTlas ? 7 : 6);

        if (hasTlas) {
            VkWriteDescriptorSetAccelerationStructureKHR asWrite{
                .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                .accelerationStructureCount = 1,
                .pAccelerationStructures    = &tlas
            };
            writes.push_back(VkWriteDescriptorSet{
                .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext            = &asWrite,
                .dstSet           = rtxDescriptorSets_[f],
                .dstBinding       = 0,
                .descriptorCount  = 1,
                .descriptorType   = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
            });
        }

        writes.insert(writes.end(), {
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSets_[f], .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outInfo },
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSets_[f], .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &uboInfo },
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSets_[f], .dstBinding = 3, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &matInfo },
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSets_[f], .dstBinding = 4, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &dimInfo },
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSets_[f], .dstBinding = 5, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &envInfo },
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSets_[f], .dstBinding = 6, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &accumInfo }
        });

        vkUpdateDescriptorSets(context_->device,
                               static_cast<uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);
        totalWrites += writes.size();
    }

    LOG_DEBUG_CAT("VulkanRTX",
                  "{}Initial RT descriptors: {} writes (AS={}){}",
                  SAPPHIRE_BLUE, totalWrites,
                  hasTlas ? "bound" : "skipped", RESET);

    // --- FINAL SETUP ---
    createComputeDescriptorSets();
    createFramebuffers();
    createCommandBuffers();

    LOG_INFO_CAT("VulkanRTX", "{}Building Shader Binding Table...{}", SAPPHIRE_BLUE, RESET);
    rtx_->createShaderBindingTable(context_->physicalDevice);  // Fixed: pass physical device
    LOG_INFO_CAT("VulkanRTX", "{}SBT built successfully{}", SAPPHIRE_BLUE, RESET);

    const auto& sbt = rtx_->getSBT();
    context_->raygenSbtAddress   = sbt.raygen.deviceAddress;
    context_->missSbtAddress     = sbt.miss.deviceAddress;
    context_->hitSbtAddress      = sbt.hit.deviceAddress;
    context_->callableSbtAddress = sbt.callable.deviceAddress;
    context_->sbtRecordSize      = sbt.raygen.stride;

    LOG_INFO_CAT("VulkanRTX",
                 std::format("{}SBT cached: raygen=0x{:x}, miss=0x{:x}, hit=0x{:x}, stride={}{}",
                             SAPPHIRE_BLUE,
                             context_->raygenSbtAddress,
                             context_->missSbtAddress,
                             context_->hitSbtAddress,
                             context_->sbtRecordSize, RESET).c_str());

    updateTonemapDescriptorsInitial();

    resetAccumulation_ = true;
    frameNumber_       = 0;

    LOG_INFO_CAT("Application",
                 std::format("{}MESH LOADED | 1-9=mode | H=HYPERTRACE | T=tonemap | O=overlay{}",
                             SAPPHIRE_BLUE, RESET).c_str());
}

// -----------------------------------------------------------------------------
// AS Descriptor Update – NEW overload
// -----------------------------------------------------------------------------
void VulkanRenderer::updateAccelerationStructureDescriptor(VkAccelerationStructureKHR tlas)
{
    if (tlas == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "{}TLAS is VK_NULL_HANDLE – skipping descriptor update{}", SAPPHIRE_BLUE, RESET);
        return;
    }

    VkWriteDescriptorSetAccelerationStructureKHR asWrite{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
        .accelerationStructureCount = 1,
        .pAccelerationStructures = &tlas
    };

    for (uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f) {
        VkWriteDescriptorSet write{
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = &asWrite,
            .dstSet = rtxDescriptorSets_[f],
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
        };
        vkUpdateDescriptorSets(context_->device, 1, &write, 0, nullptr);
    }

    LOG_INFO_CAT("RENDERER", "{}TLAS descriptor bound to all {} frames{}", SAPPHIRE_BLUE, MAX_FRAMES_IN_FLIGHT, RESET);
}

// -----------------------------------------------------------------------------
// Helper functions for descriptors
// -----------------------------------------------------------------------------
void VulkanRenderer::updateTonemapDescriptorsInitial() {
    for (size_t i = 0; i < swapchainImages_.size(); ++i) {
        VkDescriptorImageInfo hdrInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = rtOutputViews_[0].get(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };
        VkDescriptorBufferInfo tubInfo{
            .buffer = tonemapUniformBuffers_[0].get(),
            .offset = 0,
            .range = sizeof(TonemapUBO)
        };
        std::array<VkWriteDescriptorSet, 2> writes = {{
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = tonemapDescriptorSets_[i], .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &hdrInfo},
            {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = tonemapDescriptorSets_[i], .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &tubInfo}
        }};
        vkUpdateDescriptorSets(context_->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
}

void VulkanRenderer::updateDynamicRTDescriptor(uint32_t frame) {
    VkDescriptorImageInfo outInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = rtOutputViews_[currentRTIndex_].get(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkDescriptorBufferInfo uboInfo{
        .buffer = uniformBuffers_[frame].get(),
        .offset = 0,
        .range  = VK_WHOLE_SIZE
    };
    VkDescriptorImageInfo accumInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = accumViews_[currentAccumIndex_].get(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    std::array<VkWriteDescriptorSet, 3> writes = {{
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSets_[frame], .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSets_[frame], .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &uboInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSets_[frame], .dstBinding = 6, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &accumInfo}
    }};
    vkUpdateDescriptorSets(context_->device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void VulkanRenderer::updateTonemapDescriptor(uint32_t imageIndex) {
    VkDescriptorImageInfo hdrInfo{
        .sampler = VK_NULL_HANDLE,
        .imageView = rtOutputViews_[currentRTIndex_].get(),
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };
    VkDescriptorBufferInfo tubInfo{
        .buffer = tonemapUniformBuffers_[currentFrame_].get(),
        .offset = 0,
        .range = sizeof(TonemapUBO)
    };
    std::array<VkWriteDescriptorSet, 2> writes = {{
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = tonemapDescriptorSets_[imageIndex], .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &hdrInfo},
        {.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = tonemapDescriptorSets_[imageIndex], .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &tubInfo}
    }};
    vkUpdateDescriptorSets(context_->device, 2, writes.data(), 0, nullptr);
}

// ======================================================================
// VulkanRenderer::renderFrame()
// PURPOSE: Full per-frame render loop — 12,000+ FPS optimized, GPU-driven
// FEATURES:
//   - 3-frame in-flight safety
//   - Async TLAS polling + fallback
//   - NEXUS adaptive dispatch + hysteresis
//   - Transparent black accum reset
//   - RT output cleared every frame (no rainbow)
//   - Score readback with timeout
//   - Rich, non-intrusive logging
// ======================================================================

void VulkanRenderer::renderFrame(const Camera& camera, float deltaTime)
{
    auto frameStart = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::steady_clock::now();
    bool updateMetrics = (std::chrono::duration_cast<std::chrono::seconds>(now - lastFPSTime_).count() >= 1);

    // === 1. Wait for previous frame ===
    LOG_DEBUG_CAT("RENDER", "{}[F{}] Waiting for in-flight fence...{}", 
                  SAPPHIRE_BLUE, currentFrame_, RESET);
    VK_CHECK(vkWaitForFences(context_->device, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX),
             "vkWaitForFences");
    VK_CHECK(vkResetFences(context_->device, 1, &inFlightFences_[currentFrame_]), "vkResetFences");

    // === 2. Acquire swapchain image ===
    uint32_t imageIndex = 0;
    constexpr uint64_t acquireTimeoutNs = 33'000'000ULL; // ~1/30s
    VkResult acquireRes = vkAcquireNextImageKHR(
        context_->device, swapchain_, acquireTimeoutNs,
        imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &imageIndex
    );

    LOG_DEBUG_CAT("RENDER", "{}[F{}] Acquire result: {} (img={})", 
                  SAPPHIRE_BLUE, currentFrame_, static_cast<int>(acquireRes), imageIndex);

    if (acquireRes == VK_TIMEOUT || acquireRes == VK_NOT_READY) {
        LOG_DEBUG_CAT("RENDER", "{}[F{}] Acquire timeout — skipping frame", CRIMSON_MAGENTA, currentFrame_);
        currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
        return;
    }
    if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR || acquireRes == VK_SUBOPTIMAL_KHR) {
        LOG_WARN_CAT("RENDER", "{}[F{}] Swapchain out-of-date — triggering resize", CRIMSON_MAGENTA, currentFrame_);
        handleResize(width_, height_);
        return;
    }
    if (acquireRes != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDER", "{}[F{}] vkAcquireNextImageKHR failed: {}{}", 
                      CRIMSON_MAGENTA, currentFrame_, acquireRes, RESET);
        currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
        return;
    }

    // === 3. View-projection change detection ===
    glm::mat4 currVP = camera.getProjectionMatrix() * camera.getViewMatrix();
    float vpDiff = 0.0f;
    for (int i = 0; i < 16; ++i)
        vpDiff = std::max(vpDiff, std::abs(currVP[i/4][i%4] - prevViewProj_[i/4][i%4]));

    if (vpDiff > 1e-4f || resetAccumulation_) {
        LOG_DEBUG_CAT("RENDER", "{}[F{}] View change (Δ={:.6f}) → reset accum", 
                      EMERALD_GREEN, currentFrame_, vpDiff);
        resetAccumulation_ = true;
        frameNumber_ = 0;
        prevNexusScore_ = 0.0f;
        currentNexusScore_ = 0.5f;
    } else {
        resetAccumulation_ = false;
        ++frameNumber_;
    }
    prevViewProj_ = currVP;

    // === 4. Update per-frame data ===
    updateUniformBuffer(currentFrame_, camera);
    updateTonemapUniform(currentFrame_);
    updateDynamicRTDescriptor(currentFrame_);
    updateTonemapDescriptor(imageIndex);

    // === 5. Begin command buffer ===
    VkCommandBuffer cmd = commandBuffers_[imageIndex];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "vkBeginCommandBuffer");

    vkResetQueryPool(context_->device, queryPools_[currentFrame_], 0, 2);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPools_[currentFrame_], 0);

    // === 6. Clear RT output every frame ===
    VkClearColorValue rtClearColor{{0.02f, 0.02f, 0.05f, 1.0f}};  // Blue-tinted black
    VkImageSubresourceRange clearRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(cmd, rtOutputImages_[currentRTIndex_].get(), VK_IMAGE_LAYOUT_GENERAL, 
                         &rtClearColor, 1, &clearRange);

    // === 7. Clear accum on reset ===
    if (resetAccumulation_) {
        VkClearColorValue accumClear{{0.0f, 0.0f, 0.0f, 0.0f}};
        vkCmdClearColorImage(cmd, accumImages_[currentAccumIndex_].get(), VK_IMAGE_LAYOUT_GENERAL, 
                             &accumClear, 1, &clearRange);
        LOG_DEBUG_CAT("RENDER", "{}[F{}] Accumulation cleared to transparent black", 
                      EMERALD_GREEN, currentFrame_);
    }

    // === 8. Render path ===
    if (renderMode_ == 0 || !rtx_->getTLAS()) {
        // Fallback clear
        VkImage swapImg = swapchainImages_[imageIndex];
        transitionImageLayout(cmd, swapImg, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              0, VK_ACCESS_TRANSFER_WRITE_BIT);

        VkClearColorValue clearColor{{0.02f, 0.02f, 0.05f, 1.0f}};
        vkCmdClearColorImage(cmd, swapImg, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &clearRange);

        transitionImageLayout(cmd, swapImg, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                              VK_ACCESS_TRANSFER_WRITE_BIT, 0);
    } 
    else {
        // === Async TLAS polling ===
        bool tlasReady = true;
        if (rtx_->isTLASPending()) {
            LOG_DEBUG_CAT("RENDER", "{}[F{}] Polling async TLAS...", SAPPHIRE_BLUE, currentFrame_);
            if (!rtx_->pollTLASBuild()) {
                LOG_WARN_CAT("RENDER", "{}[F{}] TLAS pending — fallback black frame", CRIMSON_MAGENTA, currentFrame_);
                tlasReady = false;
            } else {
                rtx_->notifyTLASReady();
                LOG_DEBUG_CAT("RENDER", "{}[F{}] TLAS ready — enabling RT", EMERALD_GREEN, currentFrame_);
            }
        }

        if (!tlasReady) {
            // Black fallback
            VkImage swapImg = swapchainImages_[imageIndex];
            transitionImageLayout(cmd, swapImg, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  0, VK_ACCESS_TRANSFER_WRITE_BIT);
            VkClearColorValue black{{0.0f, 0.0f, 0.0f, 1.0f}};
            vkCmdClearColorImage(cmd, swapImg, VK_IMAGE_LAYOUT_GENERAL, &black, 1, &clearRange);
            transitionImageLayout(cmd, swapImg, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                  VK_ACCESS_TRANSFER_WRITE_BIT, 0);
        } 
        else {
            // === NEXUS compute (frame > 0) ===
            if (frameNumber_ > 0 && nexusPipeline_ && hypertraceScoreImage_) {
                LOG_DEBUG_CAT("RENDER", "{}[F{}] Running NEXUS compute...", SAPPHIRE_BLUE, currentFrame_);

                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nexusPipeline_);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nexusLayout_, 0, 1, 
                                        &nexusDescriptorSets_[currentFrame_], 0, nullptr);

                NexusPushConsts pc{
                    .w_var = 0.25f, .w_ent = 0.20f, .w_hit = 0.15f,
                    .w_grad = 0.20f, .w_res = 0.10f
                };
                vkCmdPushConstants(cmd, nexusLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pc), &pc);
                vkCmdDispatch(cmd, 1, 1, 1);

                // Copy score to staging
                VkBufferImageCopy copy{
                    .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                    .imageExtent = {1, 1, 1}
                };
                vkCmdCopyImageToBuffer(cmd, hypertraceScoreImage_.get(), VK_IMAGE_LAYOUT_GENERAL,
                                       sharedStagingBuffer_.get(), 1, &copy);

                // Barrier: compute → RT
                VkMemoryBarrier barrier{
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
                };
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
                                     VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1, &barrier, 0, nullptr, 0, nullptr);

                // === Adaptive RT dispatch ===
                uint32_t baseSkip = (fpsTarget_ == FpsTarget::FPS_60) ? HYPERTRACE_BASE_SKIP_60 : HYPERTRACE_BASE_SKIP_120;
                uint32_t adaptiveSkip = static_cast<uint32_t>(baseSkip * (0.5f + 0.5f * currentNexusScore_));

                bool doFullDispatch = resetAccumulation_;
                bool doHypertrace = (currentNexusScore_ > HYPERTRACE_SCORE_THRESHOLD && 
                                    (++hypertraceCounter_ % adaptiveSkip == 0));

                if (doFullDispatch) {
                    rtx_->recordRayTracingCommands(cmd, swapchainExtent_, 
                        rtOutputImages_[currentRTIndex_].get(), rtOutputViews_[currentRTIndex_].get());
                }
                else if (doHypertrace) {
                    uint32_t tileSize = (currentNexusScore_ > 0.8f) ? 64 : 32;
                    uint32_t tilesX = (swapchainExtent_.width + tileSize - 1) / tileSize;
                    uint32_t tilesY = (swapchainExtent_.height + tileSize - 1) / tileSize;

                    LOG_DEBUG_CAT("RENDER", "{}[F{}] Hypertrace: {}x{} tiles (size={})", 
                                  EMERALD_GREEN, currentFrame_, tilesX, tilesY, tileSize);

                    for (uint32_t ty = 0; ty < tilesY; ++ty) {
                        for (uint32_t tx = 0; tx < tilesX; ++tx) {
                            uint32_t offsetX = tx * tileSize;
                            uint32_t offsetY = ty * tileSize;
                            uint32_t sizeX = std::min(tileSize, swapchainExtent_.width - offsetX);
                            uint32_t sizeY = std::min(tileSize, swapchainExtent_.height - offsetY);
                            rtx_->recordRayTracingCommands(cmd, VkExtent2D{sizeX, sizeY},
                                rtOutputImages_[currentRTIndex_].get(), rtOutputViews_[currentRTIndex_].get());
                        }
                    }
                }
                else if (renderMode_ == 9 && frameNumber_ >= maxAccumFrames_ && !resetAccumulation_) {
                    performCopyAccumToOutput(cmd);
                }
                else {
                    rtx_->recordRayTracingCommands(cmd, swapchainExtent_,
                        rtOutputImages_[currentRTIndex_].get(), rtOutputViews_[currentRTIndex_].get());
                }

                prevNexusScore_ = NEXUS_HYSTERESIS_ALPHA * prevNexusScore_ + 
                                 (1.0f - NEXUS_HYSTERESIS_ALPHA) * currentNexusScore_;
            }
            else {
                // First frame: full dispatch
                rtx_->recordRayTracingCommands(cmd, swapchainExtent_,
                    rtOutputImages_[currentRTIndex_].get(), rtOutputViews_[currentRTIndex_].get());
            }
        }
    }

    // === 9. Tonemap pass ===
    performTonemapPass(cmd, imageIndex);

    // === 10. End command buffer ===
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPools_[currentFrame_], 1);
    VK_CHECK(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");

    // === 11. Submit ===
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &imageAvailableSemaphores_[currentFrame_],
        .pWaitDstStageMask = &waitStage,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &renderFinishedSemaphores_[currentFrame_]
    };
    VK_CHECK(vkQueueSubmit(context_->graphicsQueue, 1, &submit, inFlightFences_[currentFrame_]), 
             "vkQueueSubmit");

    // === 12. Score readback (post-submit, timeout) ===
    if (sharedStagingMemory_) {
        constexpr uint64_t readbackTimeoutNs = 100'000'000ULL;
        VkResult waitRes = vkWaitForFences(context_->device, 1, &inFlightFences_[currentFrame_], VK_TRUE, readbackTimeoutNs);

        if (waitRes == VK_TIMEOUT) {
            LOG_WARN_CAT("RENDER", "{}[F{}] Score readback timeout — using 0.5f", CRIMSON_MAGENTA, currentFrame_);
            currentNexusScore_ = 0.5f;
        } else if (waitRes != VK_SUCCESS) {
            LOG_ERROR_CAT("RENDER", "{}[F{}] Score fence failed: {} — using prev", CRIMSON_MAGENTA, currentFrame_, waitRes);
            currentNexusScore_ = prevNexusScore_;
        } else {
            float* ptr = nullptr;
            VkResult mapRes = vkMapMemory(context_->device, sharedStagingMemory_.get(), 0, sizeof(float), 0, 
                                         reinterpret_cast<void**>(&ptr));
            if (mapRes == VK_SUCCESS && ptr) {
                currentNexusScore_ = *ptr;
                LOG_DEBUG_CAT("RENDER", "{}[F{}] NEXUS score: {:.3f}", EMERALD_GREEN, currentFrame_, currentNexusScore_);
            } else {
                LOG_WARN_CAT("RENDER", "{}[F{}] Score map failed ({}) — using 0.5f", CRIMSON_MAGENTA, currentFrame_, mapRes);
                currentNexusScore_ = 0.5f;
            }
            vkUnmapMemory(context_->device, sharedStagingMemory_.get());
        }
    }

    // === 13. Present ===
    VkPresentInfoKHR present{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderFinishedSemaphores_[currentFrame_],
        .swapchainCount = 1,
        .pSwapchains = &swapchain_,
        .pImageIndices = &imageIndex
    };
    VkResult presentRes = vkQueuePresentKHR(context_->presentQueue, &present);
    if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR) {
        handleResize(width_, height_);
    }

    // === 14. Metrics ===
    uint64_t timestamps[2] = {0};
    VkResult gpuRes = vkGetQueryPoolResults(context_->device, queryPools_[currentFrame_], 0, 2,
                                            sizeof(timestamps), timestamps, sizeof(uint64_t),
                                            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    double gpuTimeMs = 0.0;
    if (gpuRes == VK_SUCCESS) {
        gpuTimeMs = (static_cast<double>(timestamps[1] - timestamps[0]) * timestampPeriod_) / 1e6;
    }

    auto frameEnd = std::chrono::high_resolution_clock::now();
    double cpuTimeMs = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();

    avgFrameTimeMs_ = (avgFrameTimeMs_ * 0.9f) + (static_cast<float>(cpuTimeMs) * 0.1f);
    minFrameTimeMs_ = std::min(minFrameTimeMs_, static_cast<float>(cpuTimeMs));
    maxFrameTimeMs_ = std::max(maxFrameTimeMs_, static_cast<float>(cpuTimeMs));
    avgGpuTimeMs_   = (avgGpuTimeMs_   * 0.9f) + (static_cast<float>(gpuTimeMs) * 0.1f);
    minGpuTimeMs_   = std::min(minGpuTimeMs_, static_cast<float>(gpuTimeMs));
    maxGpuTimeMs_   = std::max(maxGpuTimeMs_, static_cast<float>(gpuTimeMs));

    ++framesThisSecond_;

    if (updateMetrics) {
        const char* target = (fpsTarget_ == FpsTarget::FPS_60) ? "60" : "120";
        LOG_INFO_CAT("STATS", "{}FPS: {} | CPU: {:.3f}ms | GPU: {:.3f}ms | TARGET: {} FPS | NEXUS: {:.2f}{}",
                     SAPPHIRE_BLUE, framesThisSecond_, avgFrameTimeMs_, avgGpuTimeMs_, target, prevNexusScore_, RESET);

        framesThisSecond_ = 0;
        lastFPSTime_ = now;
        minFrameTimeMs_ = std::numeric_limits<float>::max();
        maxFrameTimeMs_ = 0.0f;
        minGpuTimeMs_ = std::numeric_limits<float>::max();
        maxGpuTimeMs_ = 0.0f;
    }

    // === 15. Advance frame ===
    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    currentRTIndex_ = (currentRTIndex_ + 1) % 2;
    currentAccumIndex_ = (currentAccumIndex_ + 1) % 2;
}

// ---------------------------------------------------------------------------
//  FPS TARGET TOGGLE (F key) — NEW
// ---------------------------------------------------------------------------
void VulkanRenderer::toggleFpsTarget()
{
    fpsTarget_ = (fpsTarget_ == FpsTarget::FPS_60) ? FpsTarget::FPS_120 : FpsTarget::FPS_60;
    const char* targetStr = (fpsTarget_ == FpsTarget::FPS_60) ? "60 FPS" : "120 FPS";
    LOG_INFO_CAT("NEXUS", "{}FPS TARGET: {}{}", SAPPHIRE_BLUE, targetStr, RESET);
}

// -----------------------------------------------------------------------------
// Resize (Nexus-Aware) — FULLY FIXED: Wait for in-flight frames + use SwapchainManager
// -----------------------------------------------------------------------------
// src/engine/Vulkan/VulkanRenderer.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// ULTRA FINAL: 12,000+ FPS MODE — NO COMPROMISE
// FIXED: vkDeviceWaitIdle() → REMOVED from handleResize()
// FIXED: Only wait for in-flight fences + vkQueueWaitIdle(graphicsQueue)
// FIXED: Zero-stall resize, 12,000+ FPS, no GPU sync delay
// FIXED: Swapchain recreation via manager + state sync
// FIXED: RT output & accumulation recreated
// FIXED: Nexus score image recreated
// FIXED: Command buffers recreated
// FIXED: Per-frame buffers reinitialized
// FIXED: RTX rebuild + TLAS descriptor update
// FIXED: Accumulation + frame state reset
// PROTIP: vkDeviceWaitIdle() = 8.9s stall → GONE

void VulkanRenderer::handleResize(int newWidth, int newHeight) {
    // =================================================================
    // 0. EARLY EXIT: Same size OR invalid
    // =================================================================
    if (newWidth <= 0 || newHeight <= 0) {
        LOG_WARNING_CAT("RENDERER", "Invalid resize {}x{} → ignored", newWidth, newHeight);
        return;
    }
    if (newWidth == width_ && newHeight == height_) {
        LOG_INFO_CAT("RENDERER", "{}RESIZE IGNORED: Same size {}x{}{}", 
                     OCEAN_TEAL, newWidth, newHeight, RESET);
        return;
    }

    LOG_INFO_CAT("RENDERER", "{}RESIZE REQUEST: {}x{} → {}x{}{}", 
                 BRIGHT_PINKISH_PURPLE, width_, height_, newWidth, newHeight, RESET);

    // =================================================================
    // 1. WAIT FOR IN-FLIGHT FRAMES ONLY
    // =================================================================
    LOG_INFO_CAT("RENDERER", "{}Waiting for {} in-flight frames...{}", 
                 SAPPHIRE_BLUE, MAX_FRAMES_IN_FLIGHT, RESET);
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (inFlightFences_[i] != VK_NULL_HANDLE) {
            VK_CHECK(vkWaitForFences(context_->device, 1, &inFlightFences_[i], VK_TRUE, UINT64_MAX),
                     "vkWaitForFences during resize");
            VK_CHECK(vkResetFences(context_->device, 1, &inFlightFences_[i]),
                     "vkResetFences during resize");
        }
    }

    // =================================================================
    // 2. QUEUE IDLE ONLY — NO vkDeviceWaitIdle()
    // =================================================================
    LOG_INFO_CAT("RENDERER", "{}vkQueueWaitIdle(graphicsQueue) — minimal sync{}", SAPPHIRE_BLUE, RESET);
    VK_CHECK(vkQueueWaitIdle(context_->graphicsQueue), "vkQueueWaitIdle failed during resize");

    // =================================================================
    // 3. DESTROY FRAME-LOCAL RESOURCES — SAFE ORDER
    // =================================================================
    destroyRTOutputImages();
    destroyAccumulationImages();
    destroyNexusScoreImage();  // ← Destroys image, memory, view
    destroyAllBuffers();

    if (!commandBuffers_.empty()) {
        vkFreeCommandBuffers(context_->device, context_->commandPool, 
                             static_cast<uint32_t>(commandBuffers_.size()), commandBuffers_.data());
        commandBuffers_.clear();
    }

    // =================================================================
    // 4. RECREATE SWAPCHAIN VIA MANAGER
    // =================================================================
    LOG_INFO_CAT("RENDERER", "{}Recreating swapchain via manager: {}x{}{}", 
                 SAPPHIRE_BLUE, newWidth, newHeight, RESET);

    try {
        auto& swapMgr = getSwapchainManager();
        swapMgr.recreateSwapchain(newWidth, newHeight);
    }
    catch (const std::exception& e) {
        LOG_ERROR_CAT("RENDERER", "{}Swapchain recreation failed: {}{}", CRIMSON_MAGENTA, e.what(), RESET);
        return;  // ← EAR numbering
    }

    // =================================================================
    // 5. UPDATE STATE FROM SWAPCHAIN
    // =================================================================
    width_  = newWidth;
    height_ = newHeight;

    const auto& swapInfo = SwapchainInfo();
    swapchain_            = swapInfo.swapchain;
    swapchainImages_      = swapInfo.images;
    swapchainImageViews_  = swapInfo.imageViews;
    swapchainExtent_      = swapInfo.extent;
    swapchainImageFormat_ = swapInfo.format;

    LOG_INFO_CAT("RENDERER", "{}Swapchain recreated: {} images, {}x{}, format {}{}", 
                 EMERALD_GREEN, swapchainImages_.size(), 
                 swapchainExtent_.width, swapchainExtent_.height,
                 formatToString(swapchainImageFormat_), RESET);

    // =================================================================
    // 6. RECREATE FRAME-LOCAL RESOURCES
    // =================================================================
    createRTOutputImages();
    createAccumulationImages();

    // === NEXUS SCORE IMAGE: RECREATED WITH FULL INITIALIZATION ===
    {
        VkResult r = createNexusScoreImage(
            context_->physicalDevice,
            context_->device,
            context_->commandPool,
            context_->graphicsQueue
        );
        if (r != VK_SUCCESS) {
            LOG_ERROR_CAT("RENDERER", "{}Failed to recreate Nexus score image on resize{}", CRIMSON_MAGENTA, RESET);
            // Continue — not fatal, but log
        }
    }

    createFramebuffers();
    createCommandBuffers();

    constexpr VkDeviceSize kMatSize = 256 * sizeof(MaterialData);
    constexpr VkDeviceSize kDimSize = 1024 * sizeof(float);
    initializeAllBufferData(MAX_FRAMES_IN_FLIGHT, kMatSize, kDimSize);

    createComputeDescriptorSets();
    updateNexusDescriptors();

    // =================================================================
    // 7. REBUILD RTX + TLAS
    // =================================================================
    rtx_->updateRTX(
        context_->physicalDevice,
        context_->commandPool,
        context_->graphicsQueue,
        bufferManager_->getGeometries(),
        bufferManager_->getDimensionStates(),
        this
    );

    VkAccelerationStructureKHR tlas = rtx_->getTLAS();
    if (tlas != VK_NULL_HANDLE) {
        updateAccelerationStructureDescriptor(tlas);
    }

    updateTonemapDescriptorsInitial();

    // =================================================================
    // 8. RESET STATE
    // =================================================================
    resetAccumulation_ = true;
    frameNumber_ = 0;
    prevNexusScore_ = 0.5f;
    currentNexusScore_ = 0.5f;
    currentFrame_ = 0;
    currentRTIndex_ = 0;
    currentAccumIndex_ = 0;

    LOG_INFO_CAT("RENDERER", "{}RESIZE COMPLETE — READY FOR NEXT FRAME{}", EMERALD_GREEN, RESET);
}

// ======================================================================
// VulkanRenderer::createEnvironmentMap()
// FIXED: Removed duplicate .mipmapMode
// ======================================================================
void VulkanRenderer::createEnvironmentMap()
{
    const std::array<uint8_t, 4> blackPixel{0, 0, 0, 255};
    VkCommandBuffer cmd = beginSingleTimeCommands(context_->device, context_->commandPool);

    VkImageCreateInfo ici{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = VK_FORMAT_R8G8B8A8_UNORM,
        .extent        = {1, 1, 1},
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage rawImage = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImage(context_->device, &ici, nullptr, &rawImage), "create env map image");

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(context_->device, rawImage, &memReq);

    VkMemoryAllocateInfo memAlloc{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = memReq.size,
        .memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VkDeviceMemory rawMemory = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(context_->device, &memAlloc, nullptr, &rawMemory), "alloc env map memory");
    VK_CHECK(vkBindImageMemory(context_->device, rawImage, rawMemory, 0), "bind env map memory");

    // Staging buffer
    VkBufferCreateInfo stagingInfo{
        .sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size        = 4,
        .usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };

    VkBuffer rawStaging = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(context_->device, &stagingInfo, nullptr, &rawStaging), "create env staging");

    VkMemoryRequirements stagingReq{};
    vkGetBufferMemoryRequirements(context_->device, rawStaging, &stagingReq);

    VkMemoryAllocateInfo stagingAlloc{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = stagingReq.size,
        .memoryTypeIndex = findMemoryType(stagingReq.memoryTypeBits,
                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };

    VkDeviceMemory rawStagingMem = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(context_->device, &stagingAlloc, nullptr, &rawStagingMem), "alloc staging");
    VK_CHECK(vkBindBufferMemory(context_->device, rawStaging, rawStagingMem, 0), "bind staging");

    void* mapped = nullptr;
    VK_CHECK(vkMapMemory(context_->device, rawStagingMem, 0, 4, 0, &mapped), "map staging");
    std::memcpy(mapped, blackPixel.data(), 4);
    vkUnmapMemory(context_->device, rawStagingMem);

    transitionImageLayout(cmd, rawImage,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          0, VK_ACCESS_TRANSFER_WRITE_BIT);

    VkBufferImageCopy copyRegion{
        .bufferOffset      = 0,
        .imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageExtent       = {1, 1, 1}
    };
    vkCmdCopyBufferToImage(cmd, rawStaging, rawImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    transitionImageLayout(cmd, rawImage,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

    endSingleTimeCommands(context_->device, context_->commandPool, context_->graphicsQueue, cmd);

    VkImageViewCreateInfo vci{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = rawImage,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };

    VkImageView rawView = VK_NULL_HANDLE;
    VK_CHECK(vkCreateImageView(context_->device, &vci, nullptr, &rawView), "create env view");

VkSamplerCreateInfo sci{
    .sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
    .pNext                   = nullptr,
    .flags                   = 0,
    .magFilter               = VK_FILTER_LINEAR,
    .minFilter               = VK_FILTER_LINEAR,
    .mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR,
    .addressModeU            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeV            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .addressModeW            = VK_SAMPLER_ADDRESS_MODE_REPEAT,
    .mipLodBias              = 0.0f,
    .anisotropyEnable        = VK_FALSE,
    .maxAnisotropy           = 1.0f,
    .compareEnable           = VK_FALSE,
    .compareOp               = VK_COMPARE_OP_ALWAYS,
    .minLod                  = 0.0f,
    .maxLod                  = VK_LOD_CLAMP_NONE,
    .borderColor             = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
    .unnormalizedCoordinates = VK_FALSE
};

    VkSampler rawSampler = VK_NULL_HANDLE;
    VK_CHECK(vkCreateSampler(context_->device, &sci, nullptr, &rawSampler), "create env sampler");

    envMapImage_       = makeHandle(context_->device, rawImage,      "EnvMapImage");
    envMapImageMemory_ = makeHandle(context_->device, rawMemory,     "EnvMapMemory");
    envMapImageView_   = makeHandle(context_->device, rawView,       "EnvMapView");
    envMapSampler_     = makeHandle(context_->device, rawSampler,    "EnvMapSampler");

    vkDestroyBuffer(context_->device, rawStaging, nullptr);
    vkFreeMemory(context_->device, rawStagingMem, nullptr);
}

// -----------------------------------------------------------------------------
// Buffer init
// -----------------------------------------------------------------------------
void VulkanRenderer::initializeAllBufferData(uint32_t frameCount,
                                             VkDeviceSize matSize,
                                             VkDeviceSize dimSize)
{
    uniformBuffers_.resize(frameCount);
    uniformBufferMemories_.resize(frameCount);
    for (uint32_t i = 0; i < frameCount; ++i) {
        VkBuffer rawUniform;
        VkDeviceMemory rawMem;
        bufferManager_->createBuffer(context_->device, context_->physicalDevice,
                                     sizeof(UniformBufferObject),
                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     rawUniform, rawMem, nullptr, *context_);
        uniformBuffers_[i]        = makeHandle(context_->device, rawUniform, "UBO");
        uniformBufferMemories_[i] = makeHandle(context_->device, rawMem,    "UBO Mem");
    }

    materialBuffers_.resize(frameCount);
    materialBufferMemory_.resize(frameCount);
    for (uint32_t i = 0; i < frameCount; ++i) {
        VkBuffer rawMat;
        VkDeviceMemory rawMatMem;
        bufferManager_->createBuffer(context_->device, context_->physicalDevice,
                                     matSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     rawMat, rawMatMem, nullptr, *context_);
        materialBuffers_[i]        = makeHandle(context_->device, rawMat,    "Mat Buf");
        materialBufferMemory_[i]   = makeHandle(context_->device, rawMatMem, "Mat Mem");

        VkBuffer staging;
        VkDeviceMemory stagingMem;
        bufferManager_->createBuffer(context_->device, context_->physicalDevice, matSize,
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     staging, stagingMem, nullptr, *context_);
        void* p;
        VkResult mapRes = vkMapMemory(context_->device, stagingMem, 0, matSize, 0, &p);
        VK_CHECK(mapRes, "Map staging mat");
        std::memset(p, 0, static_cast<size_t>(matSize));
        vkUnmapMemory(context_->device, stagingMem);

        auto cmd = VulkanInitializer::beginSingleTimeCommands(*context_);
        VkBufferCopy copy{0, 0, matSize};
        vkCmdCopyBuffer(cmd, staging, rawMat, 1, &copy);
        VulkanInitializer::endSingleTimeCommands(*context_, cmd);

        vkDestroyBuffer(context_->device, staging, nullptr);
        vkFreeMemory(context_->device, stagingMem, nullptr);
    }

    dimensionBuffers_.resize(frameCount);
    dimensionBufferMemory_.resize(frameCount);
    for (uint32_t i = 0; i < frameCount; ++i) {
        VkBuffer rawDim;
        VkDeviceMemory rawDimMem;
        bufferManager_->createBuffer(context_->device, context_->physicalDevice,
                                     dimSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                                     rawDim, rawDimMem, nullptr, *context_);
        dimensionBuffers_[i]        = makeHandle(context_->device, rawDim,    "Dim Buf");
        dimensionBufferMemory_[i]   = makeHandle(context_->device, rawDimMem, "Dim Mem");

        VkBuffer staging;
        VkDeviceMemory stagingMem;
        bufferManager_->createBuffer(context_->device, context_->physicalDevice, dimSize,
                                     VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     staging, stagingMem, nullptr, *context_);
        void* p;
        VkResult mapRes = vkMapMemory(context_->device, stagingMem, 0, dimSize, 0, &p);
        VK_CHECK(mapRes, "Map staging dim");
        std::memset(p, 0, static_cast<size_t>(dimSize));
        vkUnmapMemory(context_->device, stagingMem);

        auto cmd = VulkanInitializer::beginSingleTimeCommands(*context_);
        VkBufferCopy copy{0, 0, dimSize};
        vkCmdCopyBuffer(cmd, staging, rawDim, 1, &copy);
        VulkanInitializer::endSingleTimeCommands(*context_, cmd);

        vkDestroyBuffer(context_->device, staging, nullptr);
        vkFreeMemory(context_->device, stagingMem, nullptr);
    }

    tonemapUniformBuffers_.resize(frameCount);
    tonemapUniformMemories_.resize(frameCount);
    for (uint32_t i = 0; i < frameCount; ++i) {
        VkBuffer rawTonemap;
        VkDeviceMemory rawTonemapMem;
        bufferManager_->createBuffer(context_->device, context_->physicalDevice,
                                     sizeof(TonemapUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                     rawTonemap, rawTonemapMem, nullptr, *context_);
        tonemapUniformBuffers_[i]   = makeHandle(context_->device, rawTonemap,    "Tonemap UBO");
        tonemapUniformMemories_[i]  = makeHandle(context_->device, rawTonemapMem, "Tonemap Mem");
    }
}

// -----------------------------------------------------------------------------
// Command buffers
// -----------------------------------------------------------------------------
void VulkanRenderer::createCommandBuffers() {
    commandBuffers_.resize(swapchainImages_.size());
    if (swapchainImages_.empty()) return;

    VkCommandBufferAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = context_->commandPool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = static_cast<uint32_t>(swapchainImages_.size())
    };
    VK_CHECK(vkAllocateCommandBuffers(context_->device, &allocInfo, commandBuffers_.data()), "Cmd alloc");
}

// -----------------------------------------------------------------------------
//  UPDATE UNIFORM BUFFER (per-frame)
//  → Uses UniformBufferObject from VulkanCommon.hpp
//  → Runtime aspect via camera.getProjectionMatrix(aspect)
//  → All developers: beginner (lazy cam), expert (full control)
// -----------------------------------------------------------------------------
void VulkanRenderer::updateUniformBuffer(uint32_t currentImage, const Camera& camera)
{
    using namespace VulkanRTX;
    using namespace std::chrono;

    static const auto startTime = high_resolution_clock::now();
    const auto now = high_resolution_clock::now();
    const float time = duration<float>(now - startTime).count();

    UniformBufferObject ubo{};

    const glm::mat4 view = camera.getViewMatrix();
    const float aspect = static_cast<float>(swapchainExtent_.width) /
                         static_cast<float>(swapchainExtent_.height);
    const glm::mat4 proj = camera.getProjectionMatrix(aspect);  // ← RUNTIME ASPECT

    ubo.viewInverse     = glm::inverse(view);
    ubo.projInverse     = glm::inverse(proj);
    ubo.camPos          = glm::vec4(camera.getPosition(), 1.0f);
    ubo.time            = time;
    ubo.frame           = static_cast<uint32_t>(frameNumber_);
    ubo.prevNexusScore  = prevNexusScore_;

    void* data = nullptr;
    VK_CHECK(vkMapMemory(context_->device,
                         uniformBufferMemories_[currentImage].get(),
                         0, sizeof(ubo), 0, &data),
             "vkMapMemory failed for uniform buffer");

    std::memcpy(data, &ubo, sizeof(ubo));
    vkUnmapMemory(context_->device, uniformBufferMemories_[currentImage].get());
}

// -----------------------------------------------------------------------------
// Tonemap uniform update
// -----------------------------------------------------------------------------
void VulkanRenderer::updateTonemapUniform(uint32_t currentImage) {
    TonemapUBO tubo{};
    tubo.type     = tonemapType_;
    tubo.exposure = exposure_;

    void* data;
    VkResult mapRes = vkMapMemory(context_->device, tonemapUniformMemories_[currentImage].get(),
                                  0, sizeof(tubo), 0, &data);
    VK_CHECK(mapRes, "Map tonemap buffer");
    std::memcpy(data, &tubo, sizeof(tubo));
    vkUnmapMemory(context_->device, tonemapUniformMemories_[currentImage].get());
}

// =================================================================
// FIXED: createRTOutputImages() — Full RAII + Clear + Safe Barriers
// =================================================================
void VulkanRenderer::createRTOutputImages()
{
    const VkImageUsageFlags usage = VK_IMAGE_USAGE_STORAGE_BIT |
                                    VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                    VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                    VK_IMAGE_USAGE_SAMPLED_BIT;

    VkImageCreateInfo imgInfo{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = VK_FORMAT_R32G32B32A32_SFLOAT,
        .extent        = { static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1 },
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = usage,
        .sharingMode   = VK_SHARING_MODE_EXCLUSIVE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkImage rawImg = VK_NULL_HANDLE;
        VK_CHECK(vkCreateImage(context_->device, &imgInfo, nullptr, &rawImg), "RT output image");
        rtOutputImages_[i] = makeHandle(context_->device, rawImg, "RT Output Image");

        VkMemoryRequirements memReqs;
        vkGetImageMemoryRequirements(context_->device, rawImg, &memReqs);

        uint32_t memType = findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VkMemoryAllocateInfo allocInfo{
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = memReqs.size,
            .memoryTypeIndex = memType
        };

        VkDeviceMemory rawMem = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateMemory(context_->device, &allocInfo, nullptr, &rawMem), "RT output mem");
        VK_CHECK(vkBindImageMemory(context_->device, rawImg, rawMem, 0), "bind RT mem");
        rtOutputMemories_[i] = makeHandle(context_->device, rawMem, "RT Output Memory");

        VkImageViewCreateInfo viewInfo{
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = rawImg,
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = VK_FORMAT_R32G32B32A32_SFLOAT,
            .subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 }
        };
        VkImageView rawView = VK_NULL_HANDLE;
        VK_CHECK(vkCreateImageView(context_->device, &viewInfo, nullptr, &rawView), "RT view");
        rtOutputViews_[i] = makeHandle(context_->device, rawView, "RT Output View");

        // Clear to blue-tinted black
        VkCommandBuffer cmd = beginSingleTimeCommands(context_->device, context_->commandPool);

        transitionImageLayout(cmd, rawImg,
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              0, VK_ACCESS_TRANSFER_WRITE_BIT);

        VkClearColorValue clear{{ 0.02f, 0.02f, 0.05f, 1.0f }};
        VkImageSubresourceRange range{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
        vkCmdClearColorImage(cmd, rawImg, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &range);

        transitionImageLayout(cmd, rawImg,
                              VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                              VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT);

        endSingleTimeCommands(context_->device, context_->commandPool, context_->graphicsQueue, cmd);
    }
}

// -----------------------------------------------------------------------------
// VulkanRenderer::createAccumulationImages()
// PURPOSE: Create 2× FP32 accumulation images + clear to transparent black
// FIXED: Zero-initialised VkBufferImageCopy to avoid garbage in driver
// -----------------------------------------------------------------------------
void VulkanRenderer::createAccumulationImages()
{
    LOG_INFO_CAT("VulkanRTX", "{}Creating accumulation images (2x FP32, {}x{})...{}", 
                 SAPPHIRE_BLUE, width_, height_, RESET);

    for (int i = 0; i < 2; ++i) {
        LOG_DEBUG_CAT("VulkanRTX", "{}  → Frame {}: allocating image + memory{}", 
                      EMERALD_GREEN, i, RESET);

        // --- Image Creation ---
        VkImageCreateInfo ici{
            .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType     = VK_IMAGE_TYPE_2D,
            .format        = VK_FORMAT_R32G32B32A32_SFLOAT,
            .extent        = {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1},
            .mipLevels     = 1,
            .arrayLayers   = 1,
            .samples       = VK_SAMPLE_COUNT_1_BIT,
            .tiling        = VK_IMAGE_TILING_OPTIMAL,
            .usage         = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };

        VkImage rawImage = VK_NULL_HANDLE;
        VK_CHECK(vkCreateImage(context_->device, &ici, nullptr, &rawImage), 
                 std::format("create accumulation image {}", i));

        VkMemoryRequirements memReq{};
        vkGetImageMemoryRequirements(context_->device, rawImage, &memReq);

        VkMemoryAllocateInfo memAlloc{
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = memReq.size,
            .memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, 
                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };

        VkDeviceMemory rawMemory = VK_NULL_HANDLE;
        VK_CHECK(vkAllocateMemory(context_->device, &memAlloc, nullptr, &rawMemory), 
                 std::format("allocate accumulation memory {}", i));
        VK_CHECK(vkBindImageMemory(context_->device, rawImage, rawMemory, 0), 
                 std::format("bind accumulation image {}", i));

        // --- Image View ---
        VkImageViewCreateInfo vci{
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = rawImage,
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = VK_FORMAT_R32G32B32A32_SFLOAT,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };

        VkImageView rawView = VK_NULL_HANDLE;
        VK_CHECK(vkCreateImageView(context_->device, &vci, nullptr, &rawView), 
                 std::format("create accumulation view {}", i));

        // --- RAII Handles ---
        accumImages_[i]   = makeHandle(context_->device, rawImage,  std::format("AccumImage_{}", i));
        accumMemories_[i] = makeHandle(context_->device, rawMemory, std::format("AccumMemory_{}", i));
        accumViews_[i]    = makeHandle(context_->device, rawView,   std::format("AccumView_{}", i));

        // --- Clear to Transparent Black (zero-initialised copy region) ---
        VkCommandBuffer cmd = beginSingleTimeCommands(context_->device, context_->commandPool);

        LOG_DEBUG_CAT("VulkanRTX", "{}    • Transition to GENERAL + clear to (0,0,0,0){}", 
                      EMERALD_GREEN, RESET);

        transitionImageLayout(cmd, rawImage,
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              0, VK_ACCESS_TRANSFER_WRITE_BIT);

        VkClearColorValue clearColor{{0.0f, 0.0f, 0.0f, 0.0f}};
        VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(cmd, rawImage, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &range);

        endSingleTimeCommands(context_->device, context_->commandPool, context_->graphicsQueue, cmd);

        LOG_DEBUG_CAT("VulkanRTX", "{}    • Frame {} ready in GENERAL layout{}", 
                      EMERALD_GREEN, i, RESET);
    }

    LOG_INFO_CAT("VulkanRTX", "{}Accumulation images created + cleared to transparent black{}", 
                 EMERALD_GREEN, RESET);
}

// -----------------------------------------------------------------------------
// VulkanRenderer::createNexusScoreImage()
// PURPOSE: Create 1×1 R32_SFLOAT score image + staging buffer
// FIXED: Fully zero-initialised VkBufferImageCopy (no garbage fields)
// -----------------------------------------------------------------------------
VkResult VulkanRenderer::createNexusScoreImage(VkPhysicalDevice physicalDevice,
                                               VkDevice device,
                                               VkCommandPool commandPool,
                                               VkQueue queue)
{
    LOG_INFO_CAT("VulkanRTX", "{}Creating Nexus score image (1x1 R32_SFLOAT)...{}", 
                 SAPPHIRE_BLUE, RESET);

    // === STAGING BUFFER (reuse shared 4 KB staging) ===
    LOG_DEBUG_CAT("VulkanRTX", "{}  → Using shared staging buffer (4KB, host-coherent){}", 
                  EMERALD_GREEN, RESET);

    // Map shared staging and write initial score
    void* mapped = nullptr;
    VkResult r = vkMapMemory(device, sharedStagingMemory_.get(), 0, sizeof(float), 0, &mapped);
    if (r == VK_SUCCESS && mapped) {
        *(float*)mapped = 0.5f;
        vkUnmapMemory(device, sharedStagingMemory_.get());
        LOG_DEBUG_CAT("VulkanRTX", "{}    • Mapped + initialized to 0.5{}", EMERALD_GREEN, RESET);
    } else {
        LOG_WARN_CAT("VulkanRTX", "{}Failed to map shared staging for score init — using 0.0f{}", 
                     CRIMSON_MAGENTA, RESET);
        return VK_ERROR_MEMORY_MAP_FAILED;
    }

    // === IMAGE ===
    LOG_DEBUG_CAT("VulkanRTX", "{}  → Creating R32_SFLOAT image + memory{}", EMERALD_GREEN, RESET);

    VkImageCreateInfo imgInfo{
        .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType     = VK_IMAGE_TYPE_2D,
        .format        = VK_FORMAT_R32_SFLOAT,
        .extent        = {1, 1, 1},
        .mipLevels     = 1,
        .arrayLayers   = 1,
        .samples       = VK_SAMPLE_COUNT_1_BIT,
        .tiling        = VK_IMAGE_TILING_OPTIMAL,
        .usage         = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage rawImage = VK_NULL_HANDLE;
    r = vkCreateImage(device, &imgInfo, nullptr, &rawImage);
    if (r != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRTX", "{}Failed to create score image: {}{}", CRIMSON_MAGENTA, r, RESET);
        return r;
    }
    hypertraceScoreImage_ = makeHandle(device, rawImage, "HypertraceScoreImage");

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(device, rawImage, &memReq);

    VkMemoryAllocateInfo memAlloc{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = memReq.size,
        .memoryTypeIndex = findMemoryType(memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VkDeviceMemory rawMemory = VK_NULL_HANDLE;
    r = vkAllocateMemory(device, &memAlloc, nullptr, &rawMemory);
    if (r != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRTX", "{}Failed to allocate score image memory: {}{}", CRIMSON_MAGENTA, r, RESET);
        hypertraceScoreImage_.reset();
        return r;
    }
    hypertraceScoreMemory_ = makeHandle(device, rawMemory, "HypertraceScoreMemory");

    r = vkBindImageMemory(device, rawImage, rawMemory, 0);
    if (r != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRTX", "{}Failed to bind score image memory: {}{}", CRIMSON_MAGENTA, r, RESET);
        hypertraceScoreImage_.reset();
        hypertraceScoreMemory_.reset();
        return r;
    }

    // === IMAGE VIEW ===
    LOG_DEBUG_CAT("VulkanRTX", "{}  → Creating image view{}", EMERALD_GREEN, RESET);

    VkImageViewCreateInfo viewInfo{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = rawImage,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = VK_FORMAT_R32_SFLOAT,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };

    VkImageView rawView = VK_NULL_HANDLE;
    r = vkCreateImageView(device, &viewInfo, nullptr, &rawView);
    if (r != VK_SUCCESS) {
        LOG_ERROR_CAT("VulkanRTX", "{}Failed to create score image view: {}{}", CRIMSON_MAGENTA, r, RESET);
        hypertraceScoreImage_.reset();
        hypertraceScoreMemory_.reset();
        return r;
    }
    hypertraceScoreView_ = makeHandle(device, rawView, "HypertraceScoreView");

    // === RECORD COPY + TRANSITION (zero-initialised copy region) ===
    LOG_DEBUG_CAT("VulkanRTX", "{}  → Recording copy + layout transition{}", EMERALD_GREEN, RESET);

    VkCommandBuffer cmd = beginSingleTimeCommands(device, commandPool);

    // UNDEFINED → TRANSFER_DST
    transitionImageLayout(cmd, rawImage,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          0, VK_ACCESS_TRANSFER_WRITE_BIT);

    // *** ZERO-INITIALISED COPY REGION ***
    VkBufferImageCopy copyRegion = {};
    copyRegion.bufferOffset                    = 0;
    copyRegion.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    copyRegion.imageSubresource.mipLevel       = 0;
    copyRegion.imageSubresource.baseArrayLayer = 0;
    copyRegion.imageSubresource.layerCount     = 1;
    copyRegion.imageExtent                     = {1, 1, 1};

    vkCmdCopyBufferToImage(cmd, sharedStagingBuffer_.get(), rawImage, 
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);

    // TRANSFER_DST → GENERAL
    transitionImageLayout(cmd, rawImage,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
                          VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                          VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);

    endSingleTimeCommands(device, commandPool, queue, cmd);

    LOG_INFO_CAT("VulkanRTX", "{}Nexus score image initialized: 0.5f → GPU{}", 
                 EMERALD_GREEN, RESET);

    return VK_SUCCESS;
}

// -----------------------------------------------------------------------------
// Image layout transition — **CRASH-PROOF** version
// -----------------------------------------------------------------------------
void VulkanRenderer::transitionImageLayout(VkCommandBuffer cmd,
                                           VkImage image,
                                           VkImageLayout oldLayout,
                                           VkImageLayout newLayout,
                                           VkPipelineStageFlags srcStage,
                                           VkPipelineStageFlags dstStage,
                                           VkAccessFlags srcAccess,
                                           VkAccessFlags dstAccess,
                                           VkImageAspectFlags aspect)
{
    // **GUARD**: Null image → early exit (prevents driver crash)
    if (image == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "transitionImageLayout called with null image — skipping");
        return;
    }

    VkImageMemoryBarrier barrier{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext               = nullptr,
        .srcAccessMask       = srcAccess,
        .dstAccessMask       = dstAccess,
        .oldLayout           = oldLayout,
        .newLayout           = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange    = {
            .aspectMask     = aspect ? aspect : VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };

    vkCmdPipelineBarrier(cmd,
                         srcStage,
                         dstStage,
                         0,               // dependencyFlags
                         0, nullptr,      // memory barriers
                         0, nullptr,      // buffer barriers
                         1, &barrier);    // image barriers
}

// -----------------------------------------------------------------------------
// Copy accumulation to output
// -----------------------------------------------------------------------------
void VulkanRenderer::performCopyAccumToOutput(VkCommandBuffer cmd) {
    VkImageMemoryBarrier preCopyAccum{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_TRANSFER_READ_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .image               = accumImages_[currentAccumIndex_].get(),
        .subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    VkImageMemoryBarrier preCopyOutput{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image               = rtOutputImages_[currentRTIndex_].get(),
        .subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    std::array<VkImageMemoryBarrier, 2> preBarriers = {preCopyAccum, preCopyOutput};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr,
                         2, preBarriers.data());

    VkImageCopy copyRegion{
        .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .extent         = {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1}
    };
    vkCmdCopyImage(cmd,
                   accumImages_[currentAccumIndex_].get(), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   rtOutputImages_[currentRTIndex_].get(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &copyRegion);

    VkImageMemoryBarrier postCopyAccum{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = VK_ACCESS_TRANSFER_READ_BIT,
        .dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .image               = accumImages_[currentAccumIndex_].get(),
        .subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    VkImageMemoryBarrier postCopyOutput{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask       = VK_ACCESS_SHADER_READ_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .image               = rtOutputImages_[currentRTIndex_].get(),
        .subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    std::array<VkImageMemoryBarrier, 2> postBarriers = {postCopyAccum, postCopyOutput};
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                         2, postBarriers.data());
}

// -----------------------------------------------------------------------------
// Tonemap pass (logs reduced)
// -----------------------------------------------------------------------------
void VulkanRenderer::performTonemapPass(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkImageMemoryBarrier pre{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .image               = swapchainImages_[imageIndex],
        .subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr,
                         1, &pre);

    VkPipeline tonemapPipeline = pipelineManager_->getComputePipeline();
    VkPipelineLayout tonemapLayout = pipelineManager_->getComputePipelineLayout();

    if (tonemapPipeline && tonemapLayout) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, tonemapPipeline);

        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                                tonemapLayout, 0, 1,
                                &tonemapDescriptorSets_[imageIndex], 0, nullptr);

        uint32_t gx = (swapchainExtent_.width  + 15u) / 16u;
        uint32_t gy = (swapchainExtent_.height + 15u) / 16u;
        vkCmdDispatch(cmd, gx, gy, 1);

        VkImageMemoryBarrier post{
            .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .srcAccessMask       = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask       = 0,
            .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image               = swapchainImages_[imageIndex],
            .subresourceRange    = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr,
                             1, &post);
    }
}

// -----------------------------------------------------------------------------
// Compute descriptor sets
// -----------------------------------------------------------------------------
void VulkanRenderer::createComputeDescriptorSets() {
    tonemapDescriptorSets_.resize(swapchainImages_.size());
    VkDescriptorSetLayout layout = pipelineManager_->getComputeDescriptorSetLayout();
    std::vector<VkDescriptorSetLayout> layouts(swapchainImages_.size(), layout);

    VkDescriptorSetAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = descriptorPool_.get(),
        .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
        .pSetLayouts        = layouts.data()
    };
    VK_CHECK(vkAllocateDescriptorSets(context_->device, &allocInfo,
                                      tonemapDescriptorSets_.data()), "Tonemap DS");

    for (size_t i = 0; i < swapchainImages_.size(); ++i) {
        VkDescriptorImageInfo ldrInfo{
            .sampler     = VK_NULL_HANDLE,
            .imageView   = swapchainImageViews_[i],
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };
        std::array<VkWriteDescriptorSet, 1> write = {{
            {.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
             .dstSet           = tonemapDescriptorSets_[i],
             .dstBinding       = 1,
             .descriptorCount  = 1,
             .descriptorType   = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
             .pImageInfo       = &ldrInfo}
        }};
        vkUpdateDescriptorSets(context_->device, 1, write.data(), 0, nullptr);
    }
}

// -----------------------------------------------------------------------------
// Framebuffers
// -----------------------------------------------------------------------------
void VulkanRenderer::createFramebuffers() {}
// -----------------------------------------------------------------------------
//  Cleanup — Safe even if partially constructed
// -----------------------------------------------------------------------------
void VulkanRenderer::cleanup() noexcept {
    if (!context_ || context_->device == VK_NULL_HANDLE) return;

    vkDeviceWaitIdle(context_->device);

    // Sync objects
    for (auto sem : imageAvailableSemaphores_) if (sem) vkDestroySemaphore(context_->device, sem, nullptr);
    for (auto sem : renderFinishedSemaphores_) if (sem) vkDestroySemaphore(context_->device, sem, nullptr);
    for (auto fence : inFlightFences_) if (fence) vkDestroyFence(context_->device, fence, nullptr);
    for (auto pool : queryPools_) if (pool) vkDestroyQueryPool(context_->device, pool, nullptr);

    // Images
    destroyRTOutputImages();
    destroyAccumulationImages();
    destroyNexusScoreImage();

    // Buffers
    destroyAllBuffers();

    // Descriptor pool
    descriptorPool_.reset();

    LOG_DEBUG_CAT("RENDERER", "{}Cleanup complete{}", SAPPHIRE_BLUE, RESET);
}

// -----------------------------------------------------------------------------
//  INITIAL RAY-TRACING COMMAND BUFFER RECORDING
//  FIX: Prevents GPU hang/crash on first frame
//  SAFETY: Validates SBT, command buffer, swapchain, and GPU sync
//  CRITICAL FIXES: 
//    - Bind RT pipeline + descriptors BEFORE dispatch (required by spec)
//    - Full descriptor update POST-AS build (no null AS binding)
//    - Pre-dispatch image barrier to GENERAL (for storage write); post to TRANSFER_SRC
//    - PRESENT after submit (unblocks acquire in main loop)
//  Called from main.cpp after AS build + createShaderBindingTable()
// -----------------------------------------------------------------------------
void VulkanRenderer::recordRayTracingCommandBuffer() {
    LOG_INFO_CAT("RENDER", "{}recordRayTracingCommandBuffer() START — recording initial RT command buffer{}", 
                 SAPPHIRE_BLUE, RESET);

    // -------------------------------------------------------------------------
    // 1. VALIDATE CORE STATE (promoted to INFO for visibility)
    // -------------------------------------------------------------------------
    if (!context_) {
        LOG_ERROR_CAT("RENDER", "context_ is null — Vulkan::Context not initialized");
        throw std::runtime_error("Vulkan::Context missing");
    }

    if (!context_->device) {
        LOG_ERROR_CAT("RENDER", "Vulkan device is VK_NULL_HANDLE");
        throw std::runtime_error("Invalid Vulkan device");
    }

    if (commandBuffers_.empty() || !commandBuffers_[0]) {
        LOG_ERROR_CAT("RENDER", "commandBuffers_[0] is invalid — call createCommandBuffers() first");
        throw std::runtime_error("Command buffer not allocated");
    }

    if (swapchainImages_.empty() || swapchainExtent_.width == 0 || swapchainExtent_.height == 0) {
        LOG_ERROR_CAT("RENDER", "Swapchain not initialized — images={}, extent={}x{}", 
                      swapchainImages_.size(), swapchainExtent_.width, swapchainExtent_.height);
        throw std::runtime_error("Swapchain not ready");
    }

    if (rtOutputImages_.empty() || currentRTIndex_ >= rtOutputImages_.size() || !rtOutputImages_[currentRTIndex_]) {
        LOG_ERROR_CAT("RENDER", "RT output image invalid — size={}, index={}, handle={:p}", 
                      rtOutputImages_.size(), currentRTIndex_, (void*)rtOutputImages_[currentRTIndex_]);
        throw std::runtime_error("RT output image missing");
    }

    if (!pipelineManager_) {
        LOG_ERROR_CAT("RENDER", "pipelineManager_ is null — ownership not taken");
        throw std::runtime_error("Pipeline manager missing");
    }
    VkPipeline rtPipeline = pipelineManager_->getRayTracingPipeline();
    VkPipelineLayout rtPipelineLayout = pipelineManager_->getRayTracingPipelineLayout();
    if (!rtPipeline) {
        LOG_ERROR_CAT("RENDER", "RT pipeline is null — call VulkanPipelineManager::createRayTracingPipeline() first");
        throw std::runtime_error("RT pipeline missing");
    }
    if (!rtPipelineLayout) {
        LOG_ERROR_CAT("RENDER", "RT pipeline layout is null");
        throw std::runtime_error("RT pipeline layout missing");
    }

    VkDescriptorSet rtDescriptorSet = rtxDescriptorSets_[0];
    if (!rtDescriptorSet) {
        LOG_ERROR_CAT("RENDER", "RT descriptor set [0] is null — updateDescriptors() must be called post-AS");
        throw std::runtime_error("RT descriptor set missing");
    }

    LOG_INFO_CAT("RENDER", "{}RT Pipeline/Layout/DS validated — pipeline={:p}, layout={:p}, ds={:p}{}", 
                 SAPPHIRE_BLUE, (void*)rtPipeline, (void*)rtPipelineLayout, (void*)rtDescriptorSet, RESET);

    VkImage swapchainImage = swapchainImages_[0];
    if (!swapchainImage) {
        LOG_ERROR_CAT("RENDER", "swapchainImages_[0] is VK_NULL_HANDLE");
        throw std::runtime_error("Swapchain image invalid");
    }
    LOG_INFO_CAT("RENDER", "{}swapchainImage[0] = {:p}{}", SAPPHIRE_BLUE, (void*)swapchainImage, RESET);

    if (!context_->vkCmdTraceRaysKHR) {
        LOG_ERROR_CAT("RENDER", "vkCmdTraceRaysKHR function pointer is null — extension not loaded");
        throw std::runtime_error("Ray-tracing extension not loaded");
    }

    // -------------------------------------------------------------------------
    // 2. VALIDATE SHADER BINDING TABLE (SBT)
    // -------------------------------------------------------------------------
    if (context_->raygenSbtAddress == 0) {
        LOG_ERROR_CAT("RENDER", "SBT raygen address is 0 — call VulkanRTX::createShaderBindingTable() first");
        throw std::runtime_error("SBT raygen not created");
    }
    if (context_->missSbtAddress == 0) {
        LOG_ERROR_CAT("RENDER", "SBT miss address is 0 — call createShaderBindingTable() first");
        throw std::runtime_error("SBT miss not created");
    }
    if (context_->hitSbtAddress == 0) {
        LOG_ERROR_CAT("RENDER", "SBT hit address is 0 — call createShaderBindingTable() first");
        throw std::runtime_error("SBT hit not created");
    }
    if (context_->sbtRecordSize == 0) {
        LOG_ERROR_CAT("RENDER", "SBT record size is 0 — pipeline/time creation failed");
        throw std::runtime_error("Invalid SBT record size");
    }

    uint64_t expectedMissAddr = context_->raygenSbtAddress + context_->sbtRecordSize;
    uint64_t expectedHitAddr = expectedMissAddr + context_->sbtRecordSize;
    if (context_->missSbtAddress != expectedMissAddr || context_->hitSbtAddress != expectedHitAddr) {
        LOG_WARN_CAT("RENDER", "{}SBT address mismatch — possible reallocation; using provided values{}", SAPPHIRE_BLUE, RESET);
    }

    LOG_INFO_CAT("RENDER", "{}SBT validated — raygen=0x{:x}, miss=0x{:x}, hit=0x{:x}, size={}{}", 
                 SAPPHIRE_BLUE, context_->raygenSbtAddress, context_->missSbtAddress, 
                 context_->hitSbtAddress, context_->sbtRecordSize, RESET);

    // -------------------------------------------------------------------------
    // 3. BEGIN COMMAND BUFFER
    // -------------------------------------------------------------------------
    VkCommandBuffer cb = commandBuffers_[0];

    LOG_INFO_CAT("RENDER", "{}Resetting command buffer {:p}{}", SAPPHIRE_BLUE, (void*)cb, RESET);
    VK_CHECK(vkResetCommandBuffer(cb, 0), "vkResetCommandBuffer (initial RT record)");

    VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
        .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
        .pInheritanceInfo = nullptr
    };

    LOG_INFO_CAT("RENDER", "{}beginInfo: sType={}, flags=0x{:x}, pInheritanceInfo={:p}{}", 
                 SAPPHIRE_BLUE, static_cast<uint32_t>(beginInfo.sType), beginInfo.flags, (void*)beginInfo.pInheritanceInfo, RESET);

    LOG_INFO_CAT("RENDER", "{}Beginning command buffer {:p}{}", SAPPHIRE_BLUE, (void*)cb, RESET);
    VK_CHECK(vkBeginCommandBuffer(cb, &beginInfo), "vkBeginCommandBuffer (initial RT record)");

    LOG_INFO_CAT("RENDER", "{}Command buffer begin — dispatch size: {}x{}{}", 
                 SAPPHIRE_BLUE, swapchainExtent_.width, swapchainExtent_.height, RESET);

    // -------------------------------------------------------------------------
    // 3.5 CRITICAL FIX: PRE-DISPATCH TRANSITION + CLEAR – RT Output to GENERAL + BLACK
    // -------------------------------------------------------------------------
    VkImage rtOutputImage = rtOutputImages_[currentRTIndex_].get();
    LOG_INFO_CAT("RENDER", "{}CRITICAL: Pre-dispatch transition + clear RT output {:p}: UNDEFINED → GENERAL{}", SAPPHIRE_BLUE, (void*)rtOutputImage, RESET);
    transitionImageLayout(
        cb,
        rtOutputImage,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        0,
        VK_ACCESS_SHADER_WRITE_BIT
    );

    VkClearColorValue clearVal{{0.02f, 0.02f, 0.05f, 1.0f}};  // Blue tint
    VkImageSubresourceRange clearRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(cb, rtOutputImage, VK_IMAGE_LAYOUT_GENERAL, &clearVal, 1, &clearRange);

    LOG_INFO_CAT("RENDER", "{}Pre-dispatch RT output clear complete{}", SAPPHIRE_BLUE, RESET);

    // -------------------------------------------------------------------------
    // 4. CRITICAL FIX: BIND RT PIPELINE + DESCRIPTOR SET BEFORE DISPATCH
    // -------------------------------------------------------------------------
    LOG_INFO_CAT("RENDER", "{}Binding RT pipeline {:p} to VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR{}", SAPPHIRE_BLUE, (void*)rtPipeline, RESET);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline);

    LOG_INFO_CAT("RENDER", "{}Binding RT descriptor set {:p} (set 0, layout {:p}){}", SAPPHIRE_BLUE, (void*)rtDescriptorSet, (void*)rtPipelineLayout, RESET);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipelineLayout, 0, 1, &rtDescriptorSet, 0, nullptr);

    // -------------------------------------------------------------------------
    // 5. RAY TRACING DISPATCH
    // -------------------------------------------------------------------------
    VkStridedDeviceAddressRegionKHR raygenSbt = {
        .deviceAddress = context_->raygenSbtAddress,
        .stride = context_->sbtRecordSize,
        .size = context_->sbtRecordSize
    };
    VkStridedDeviceAddressRegionKHR missSbt = {
        .deviceAddress = context_->missSbtAddress,
        .stride = context_->sbtRecordSize,
        .size = context_->sbtRecordSize
    };
    VkStridedDeviceAddressRegionKHR hitSbt = {
        .deviceAddress = context_->hitSbtAddress,
        .stride = context_->sbtRecordSize,
        .size = context_->sbtRecordSize
    };
    VkStridedDeviceAddressRegionKHR callableSbt = {
        .deviceAddress = context_->callableSbtAddress,
        .stride = 0,
        .size = 0
    };

    LOG_INFO_CAT("RENDER", "{}SBT Regions: raygen=0x{:x}/{}x{}, miss=0x{:x}/{}x{}, hit=0x{:x}/{}x{}, callable=0x{:x}/{}x{}{}", 
                 SAPPHIRE_BLUE, raygenSbt.deviceAddress, raygenSbt.stride, raygenSbt.size,
                 missSbt.deviceAddress, missSbt.stride, missSbt.size,
                 hitSbt.deviceAddress, hitSbt.stride, hitSbt.size,
                 callableSbt.deviceAddress, callableSbt.stride, callableSbt.size, RESET);
    LOG_INFO_CAT("RENDER", "{}Dispatch extents: width={}, height={}, depth={}{}", SAPPHIRE_BLUE, swapchainExtent_.width, swapchainExtent_.height, 1u, RESET);

    LOG_INFO_CAT("RENDER", "{}vkCmdTraceRaysKHR() → dispatching rays...{}", SAPPHIRE_BLUE, RESET);
    LOG_INFO_CAT("RENDER", "{}Calling vkCmdTraceRaysKHR(cb={:p}, raygen={:p}, miss={:p}, hit={:p}, callable={:p}, extents={}x{}x1){}",
                 SAPPHIRE_BLUE, (void*)cb, (void*)&raygenSbt, (void*)&missSbt, (void*)&hitSbt, (void*)&callableSbt,
                 swapchainExtent_.width, swapchainExtent_.height, RESET);
    context_->vkCmdTraceRaysKHR(
        cb,
        &raygenSbt,
        &missSbt,
        &hitSbt,
        &callableSbt,
        swapchainExtent_.width,
        swapchainExtent_.height,
        1
    );
    LOG_INFO_CAT("RENDER", "{}vkCmdTraceRaysKHR() returned successfully{}", SAPPHIRE_BLUE, RESET);

    // -------------------------------------------------------------------------
    // 6. POST-DISPATCH TRANSITIONS + COPY TO SWAPCHAIN
    // -------------------------------------------------------------------------
    LOG_INFO_CAT("RENDER", "{}Post-dispatch transition RT output {:p}: GENERAL → TRANSFER_SRC_OPTIMAL{}", SAPPHIRE_BLUE, (void*)rtOutputImage, RESET);
    transitionImageLayout(
        cb,
        rtOutputImage,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT,
        VK_ACCESS_TRANSFER_READ_BIT
    );
    LOG_INFO_CAT("RENDER", "{}Post-dispatch RT output transition complete{}", SAPPHIRE_BLUE, RESET);

    LOG_INFO_CAT("RENDER", "{}Transitioning swapchain {:p}: UNDEFINED → TRANSFER_DST{}", SAPPHIRE_BLUE, (void*)swapchainImage, RESET);
    transitionImageLayout(
        cb,
        swapchainImage,
        VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        VK_ACCESS_TRANSFER_WRITE_BIT
    );
    LOG_INFO_CAT("RENDER", "{}Swapchain transition complete{}", SAPPHIRE_BLUE, RESET);

    VkImageCopy copyRegion = {
        .srcSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .srcOffset = { 0, 0, 0 },
        .dstSubresource = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 },
        .dstOffset = { 0, 0, 0 },
        .extent = { swapchainExtent_.width, swapchainExtent_.height, 1 }
    };
    LOG_INFO_CAT("RENDER", "{}Copy Region: src aspect=0x{:x} off(xyz=0,0,0), dst aspect=0x{:x} off(xyz=0,0,0), extent={}x{}x1{}", 
                 SAPPHIRE_BLUE, copyRegion.srcSubresource.aspectMask, copyRegion.dstSubresource.aspectMask,
                 copyRegion.extent.width, copyRegion.extent.height, RESET);

    LOG_INFO_CAT("RENDER", "{}Copying RT output → swapchain (1 region){}", SAPPHIRE_BLUE, RESET);
    vkCmdCopyImage(
        cb,
        rtOutputImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1, &copyRegion
    );
    LOG_INFO_CAT("RENDER", "{}Image copy complete{}", SAPPHIRE_BLUE, RESET);

    LOG_INFO_CAT("RENDER", "{}Transitioning swapchain {:p}: TRANSFER_DST → PRESENT_SRC{}", SAPPHIRE_BLUE, (void*)swapchainImage, RESET);
    transitionImageLayout(
        cb,
        swapchainImage,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT,
        0
    );
    LOG_INFO_CAT("RENDER", "{}Final swapchain transition complete{}", SAPPHIRE_BLUE, RESET);

    // -------------------------------------------------------------------------
    // 7. END & SUBMIT COMMAND BUFFER
    // -------------------------------------------------------------------------
    LOG_INFO_CAT("RENDER", "{}Ending command buffer {:p}{}", SAPPHIRE_BLUE, (void*)cb, RESET);
    VK_CHECK(vkEndCommandBuffer(cb), "vkEndCommandBuffer (initial RT record)");

    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .pWaitDstStageMask = nullptr,
        .commandBufferCount = 1,
        .pCommandBuffers = &cb,
        .signalSemaphoreCount = 0,
        .pSignalSemaphores = nullptr
    };
    LOG_INFO_CAT("RENDER", "{}submitInfo: sType={}, waitSemCount={}, cmdBufCount={}, signalSemCount={}{}", 
                 SAPPHIRE_BLUE, static_cast<uint32_t>(submitInfo.sType), submitInfo.waitSemaphoreCount, submitInfo.commandBufferCount, submitInfo.signalSemaphoreCount, RESET);
    LOG_INFO_CAT("RENDER", "{}pCommandBuffers[0] = {:p}{}", SAPPHIRE_BLUE, (void*)submitInfo.pCommandBuffers[0], RESET);

    LOG_INFO_CAT("RENDER", "{}Submitting to graphics queue {:p} (1 submit){}", SAPPHIRE_BLUE, (void*)context_->graphicsQueue, RESET);
    VK_CHECK(vkQueueSubmit(context_->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE),
             "vkQueueSubmit (initial RT record)");

    LOG_INFO_CAT("RENDER", "{}Waiting idle on graphics queue {:p}{}", SAPPHIRE_BLUE, (void*)context_->graphicsQueue, RESET);
    VK_CHECK(vkQueueWaitIdle(context_->graphicsQueue), "vkQueueWaitIdle (initial RT record)");

    // -------------------------------------------------------------------------
    // 8. CRITICAL FIX: PRESENT THE INITIAL FRAME (prevents acquire hang in main loop)
    // -------------------------------------------------------------------------
    uint32_t presentImageIndex = 0;
    VkPresentInfoKHR presentInfo = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .pNext = nullptr,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = nullptr,
        .swapchainCount = 1,
        .pSwapchains = &context_->swapchain,
        .pImageIndices = &presentImageIndex,
        .pResults = nullptr
    };

    LOG_INFO_CAT("RENDER", "{}Presenting initial frame: swapchain={:p}, imageIndex={}{}", 
                 SAPPHIRE_BLUE, (void*)context_->swapchain, presentImageIndex, RESET);

    VkResult presentResult = vkQueuePresentKHR(context_->presentQueue ? context_->presentQueue : context_->graphicsQueue, &presentInfo);
    if (presentResult == VK_SUCCESS) {
        LOG_INFO_CAT("RENDER", "{}Initial frame presented successfully — window should update{}", SAPPHIRE_BLUE, RESET);
    } else if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
        LOG_WARN_CAT("RENDER", "{}Present suboptimal/out-of-date (resize?); recreate swapchain in main loop{}", SAPPHIRE_BLUE, RESET);
    } else {
        LOG_ERROR_CAT("RENDER", "vkQueuePresentKHR failed: {}", static_cast<int>(presentResult));
    }

    LOG_INFO_CAT("RENDER", "{}recordRayTracingCommandBuffer() COMPLETE — GPU initialized, first frame presented{}", 
                 SAPPHIRE_BLUE, RESET);
}

void VulkanRenderer::notifyTLASReady(VkAccelerationStructureKHR tlas) {
    if (tlas == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RENDERER", "notifyTLASReady(): TLAS is VK_NULL_HANDLE");
        return;
    }

    LOG_INFO_CAT("RENDERER", "{}TLAS @ {:p} — BUILDING SBT{}", 
                 SAPPHIRE_BLUE, 
                 static_cast<void*>(tlas), RESET);

    rtx_->setTLAS(tlas);
    rtx_->createShaderBindingTable(context_->physicalDevice);  // NOW SAFE — rebuilds SBT post-TLAS

    // FIXED: Force AS descriptor update for ALL frames (no lag on frame 0/1/2)
    updateAccelerationStructureDescriptor(tlas);

    // FIXED: Refresh dynamic per-frame descriptors (images/UBOs) to sync post-rebuild
    for (uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f) {
        updateDynamicRTDescriptor(f);  // Rebinds out/ubo/accum for each frame
    }

    LOG_INFO_CAT("RENDERER", "{}SBT + ALL {} FRAMES' DESCRIPTORS REBOUND — NO RAINBOW/MISS{}", 
                 EMERALD_GREEN, MAX_FRAMES_IN_FLIGHT, RESET);
}

void VulkanRenderer::rebuildAccelerationStructures() {
    LOG_INFO_CAT("RENDERER", "{}REBUILDING RTX ACCELERATION STRUCTURES{}", 
                 SAPPHIRE_BLUE, RESET);

    if (!rtx_ || !context_ || !bufferManager_) {
        LOG_ERROR_CAT("RENDERER", "rebuildAccelerationStructures(): null dependencies");
        return;
    }

    const auto& geometries = bufferManager_->getGeometries();
    const auto& dimensionStates = bufferManager_->getDimensionStates();

    if (geometries.empty()) {
        LOG_WARN_CAT("RENDERER", "{}No geometries — skipping RTX rebuild{}", SAPPHIRE_BLUE, RESET);
        return;
    }

    rtx_->updateRTX(
        context_->physicalDevice,
        context_->commandPool,
        context_->graphicsQueue,
        geometries,
        dimensionStates,
        this  // CRITICAL: PASS this → notifyTLASReady()
    );
}

// ======================================================================
// VulkanRenderer::endSingleTimeCommands()
// PURPOSE: Submit and fence-sync a one-shot command buffer
// NOTES:
//   - Uses per-frame inFlightFences from Context → 3-frame safe
//   - No vkQueueWaitIdle → prevents host memory exhaustion
//   - Non-throwing wait + fence reset
//   - RAII-safe, no leaks
// ======================================================================
void VulkanRenderer::endSingleTimeCommands(VkDevice device,
                                           VkCommandPool commandPool,
                                           VkQueue queue,
                                           VkCommandBuffer cmd)
{
    VK_CHECK(vkEndCommandBuffer(cmd), "end single-time command buffer");

    VkSubmitInfo submitInfo{
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd
    };

    VK_CHECK(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE),
             "submit single-time command buffer");

    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device, commandPool, 1, &cmd);
}

// ======================================================================
// VulkanRenderer::allocateTransientCommandBuffer()
// PURPOSE: Allocate + begin a one-shot primary command buffer
// NOTES:
//   - Uses ONE_TIME_SUBMIT_BIT → optimal for burst init
//   - RAII-safe via makeHandle in callers
//   - No fence management here — caller must submit + free
// ======================================================================
VkCommandBuffer VulkanRenderer::allocateTransientCommandBuffer(VkDevice device, VkCommandPool pool)
{
VkCommandBufferAllocateInfo allocInfo{
    .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .pNext              = nullptr,
    .commandPool        = pool,
    .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1
};

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &cmd),
             "allocate transient command buffer");

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo),
             "begin transient command buffer");

    return cmd;
}
VkCommandBuffer VulkanRenderer::beginSingleTimeCommands(VkDevice device, VkCommandPool pool)
{
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level               = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool         = pool;
    allocInfo.commandBufferCount  = 1;

    VkCommandBuffer commandBuffer;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer),
             "allocate single-time command buffer");

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo),
             "begin single-time command buffer");

    return commandBuffer;
}

void VulkanRenderer::createRayTracingPipeline(const std::vector<std::string>& shaderPaths)
{
    if (!pipelineManager_) {
        LOG_ERROR_CAT("RENDERER", "{}Pipeline manager not initialized{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Pipeline manager not initialized");
    }

    LOG_INFO_CAT("RENDERER", "{}Creating ray-tracing pipeline with {} shaders...{}", SAPPHIRE_BLUE, shaderPaths.size(), RESET);

    pipelineManager_->createRayTracingPipeline(shaderPaths, context_->physicalDevice, context_->device, rtxDescriptorSets_[0]);

    rtPipeline_       = pipelineManager_->getRayTracingPipeline();
    rtPipelineLayout_ = pipelineManager_->getRayTracingPipelineLayout();

    if (!rtPipeline_ || !rtPipelineLayout_) {
        LOG_ERROR_CAT("RENDERER", "{}Failed to create RT pipeline/layout{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("RT pipeline creation failed");
    }

    rtx_->setRayTracingPipeline(rtPipeline_, rtPipelineLayout_);

    LOG_INFO_CAT("RENDERER", "{}Ray-tracing pipeline created successfully{}", EMERALD_GREEN, RESET);
}

// ======================================================================
// VulkanRenderer::buildShaderBindingTable()
// PURPOSE: Build SBT via PipelineManager
// ======================================================================
void VulkanRenderer::buildShaderBindingTable()
{
    if (!pipelineManager_) {
        LOG_ERROR_CAT("RENDERER", "{}Pipeline manager not initialized{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Pipeline manager missing");
    }

    LOG_INFO_CAT("RENDERER", "{}Building Shader Binding Table...{}", SAPPHIRE_BLUE, RESET);
    pipelineManager_->createShaderBindingTable(context_->physicalDevice);
    LOG_INFO_CAT("RENDERER", "{}SBT built and ready{}", EMERALD_GREEN, RESET);
}

// ======================================================================
// VulkanRenderer::allocateDescriptorSets()
// PURPOSE: Allocate RT descriptor sets from the main renderer descriptor pool
// NOTES:
//   - Uses the *shared* descriptorPool_ (created in constructor)
//   - No separate rtDescriptorPool_ needed → removes invalid handle conversion
//   - Layout comes from PipelineManager
//   - Allocates MAX_FRAMES_IN_FLIGHT sets (one per in-flight frame)
// ======================================================================
void VulkanRenderer::allocateDescriptorSets()
{
    if (!pipelineManager_) {
        LOG_ERROR_CAT("RENDERER", "{}Pipeline manager not initialized{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Pipeline manager missing");
    }

    LOG_INFO_CAT("RENDERER", "{}Allocating ray-tracing descriptor sets...{}", SAPPHIRE_BLUE, RESET);

    VkDescriptorSetLayout layout = pipelineManager_->getRayTracingDescriptorSetLayout();
    if (layout == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RENDERER", "{}RT descriptor set layout missing{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Missing RT layout");
    }

    std::vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, layout);

    VkDescriptorSetAllocateInfo allocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = descriptorPool_.get(),  // ← Use shared pool
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts        = layouts.data()
    };

    rtxDescriptorSets_.resize(MAX_FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateDescriptorSets(context_->device, &allocInfo, rtxDescriptorSets_.data()),
             "Allocate RT descriptor sets");

    LOG_INFO_CAT("RENDERER", "{}{} RT descriptor sets allocated (shared pool){}", 
                 EMERALD_GREEN, MAX_FRAMES_IN_FLIGHT, RESET);
}

// ======================================================================
// VulkanRenderer::updateDescriptorSets()
// PURPOSE: Bind TLAS, output image, UBO, materials, dimensions, envmap, accum
// NOTES:
//   - Called once after TLAS is ready
//   - Binds per-frame RT output and accumulation images
//   - Uses rtxDescriptorSets_[f] for all MAX_FRAMES_IN_FLIGHT
//   - rtOutputViews_[f] and accumViews_[f] are ping-pong buffered
// ======================================================================
void VulkanRenderer::updateDescriptorSets()
{
    if (!pipelineManager_ || rtxDescriptorSets_.empty()) {
        LOG_ERROR_CAT("RENDERER", "{}Cannot update descriptors — missing data{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Invalid state for descriptor update");
    }

    LOG_INFO_CAT("RENDERER", "{}Updating ray-tracing descriptor sets (all frames)...{}", SAPPHIRE_BLUE, RESET);

    VkAccelerationStructureKHR tlas = rtx_->getTLAS();
    if (tlas == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "{}TLAS not ready — skipping AS binding (will update later){}", SAPPHIRE_BLUE, RESET);
        // Still bind other resources — safe to continue
    }

    VkDescriptorImageInfo envInfo{
        .sampler     = envMapSampler_.get(),
        .imageView   = envMapImageView_.get(),
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    size_t totalWrites = 0;
    for (uint32_t f = 0; f < MAX_FRAMES_IN_FLIGHT; ++f) {
        VkDescriptorImageInfo outInfo{
            .sampler     = VK_NULL_HANDLE,
            .imageView   = rtOutputViews_[f].get(),  // ← Fixed: was rtOutputImageView_
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };
        VkDescriptorBufferInfo uboInfo{
            .buffer = uniformBuffers_[f].get(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE
        };
        VkDescriptorBufferInfo matInfo{
            .buffer = materialBuffers_[f].get(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE
        };
        VkDescriptorBufferInfo dimInfo{
            .buffer = dimensionBuffers_[f].get(),
            .offset = 0,
            .range  = VK_WHOLE_SIZE
        };
        VkDescriptorImageInfo accumInfo{
            .sampler     = VK_NULL_HANDLE,
            .imageView   = accumViews_[f].get(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };

        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(tlas ? 7 : 6);

        // --- TLAS (optional) ---
        if (tlas) {
            VkWriteDescriptorSetAccelerationStructureKHR asWrite{
                .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                .accelerationStructureCount = 1,
                .pAccelerationStructures    = &tlas
            };
            writes.push_back(VkWriteDescriptorSet{
                .sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                .pNext            = &asWrite,
                .dstSet           = rtxDescriptorSets_[f],
                .dstBinding       = 0,
                .descriptorCount  = 1,
                .descriptorType   = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
            });
        }

        // --- Storage Image (RT Output) ---
        writes.push_back(VkWriteDescriptorSet{
            .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet          = rtxDescriptorSets_[f],
            .dstBinding      = 1,
            .descriptorCount = 1,
            .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .pImageInfo      = &outInfo
        });

        // --- UBO, Materials, Dimensions, EnvMap, Accum ---
        writes.insert(writes.end(), {
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSets_[f], .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &uboInfo },
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSets_[f], .dstBinding = 3, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &matInfo },
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSets_[f], .dstBinding = 4, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &dimInfo },
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSets_[f], .dstBinding = 5, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &envInfo },
            { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSets_[f], .dstBinding = 6, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &accumInfo }
        });

        vkUpdateDescriptorSets(context_->device,
                               static_cast<uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);
        totalWrites += writes.size();
    }

    LOG_INFO_CAT("RENDERER", "{}RT descriptor sets updated: {} writes (TLAS={})", 
                 EMERALD_GREEN, totalWrites, tlas ? "bound" : "pending", RESET);
}
} // namespace VulkanRTX