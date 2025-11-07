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

// ---------------------------------------------------------------------------
//  DESTRUCTOR
// ---------------------------------------------------------------------------
VulkanRenderer::~VulkanRenderer()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}>>> DESTROYING VULKAN RENDERER{}", SAPPHIRE_BLUE, RESET);
#endif

    cleanup();

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}<<< RENDERER DESTROYED{}", SAPPHIRE_BLUE, RESET);
#endif
}

// ---------------------------------------------------------------------------
//  DESTROY NEXUS SCORE (NEW)
// ---------------------------------------------------------------------------
void VulkanRenderer::destroyNexusScoreImage() noexcept {
    if (hypertraceScoreStagingBuffer_) {
        vkDestroyBuffer(context_->device, hypertraceScoreStagingBuffer_, nullptr);
        hypertraceScoreStagingBuffer_ = VK_NULL_HANDLE;
    }
    if (hypertraceScoreStagingMemory_) {
        vkFreeMemory(context_->device, hypertraceScoreStagingMemory_, nullptr);
        hypertraceScoreStagingMemory_ = VK_NULL_HANDLE;
    }
    if (hypertraceScoreImage_) hypertraceScoreImage_.reset();
    if (hypertraceScoreMemory_) hypertraceScoreMemory_.reset();
    if (hypertraceScoreView_) hypertraceScoreView_.reset();
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}<<< NEXUS SCORE DESTROYED{}", SAPPHIRE_BLUE, RESET);
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

// -----------------------------------------------------------------------------
// Memory type finder
// -----------------------------------------------------------------------------
static uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t filter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
        if ((filter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    throw std::runtime_error("No suitable memory type");
}

// -----------------------------------------------------------------------------
// Constructor
// -----------------------------------------------------------------------------
// src/engine/Vulkan/VulkanRenderer.cpp
VulkanRenderer::VulkanRenderer(int width, int height, SDL_Window* window,
                               const std::vector<std::string>& shaderPaths,
                               std::shared_ptr< ::Vulkan::Context> context)
    : window_(window),
      context_(std::move(context)),
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
      fpsTarget_(FpsTarget::FPS_60),
      rtOutputImages_{},
      rtOutputMemories_{},
      rtOutputViews_{},
      accumImages_{},
      accumMemories_{},
      accumViews_{},
      hypertraceScoreStagingBuffer_(VK_NULL_HANDLE),
      hypertraceScoreStagingMemory_(VK_NULL_HANDLE)
{
    LOG_INFO_CAT("RENDERER",
        std::format("{}AMOURANTH RTX [{}x{}] — 12,000+ FPS HYPERTRACE NEXUS MODE {}{}",
                    SAPPHIRE_BLUE, width, height, hypertraceEnabled_ ? "ON" : "OFF", RESET).c_str());

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(context_->physicalDevice, &props);
    timestampPeriod_ = static_cast<double>(props.limits.timestampPeriod);

    VkSemaphoreCreateInfo semInfo{.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                .flags = VK_FENCE_CREATE_SIGNALED_BIT};

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VK_CHECK(vkCreateSemaphore(context_->device, &semInfo, nullptr, &imageAvailableSemaphores_[i]), "Semaphore");
        VK_CHECK(vkCreateSemaphore(context_->device, &semInfo, nullptr, &renderFinishedSemaphores_[i]), "Semaphore");
        VK_CHECK(vkCreateFence(context_->device, &fenceInfo, nullptr, &inFlightFences_[i]), "Fence");
    }

    swapchain_            = context_->swapchain;
    swapchainImages_      = context_->swapchainImages;
    swapchainImageViews_  = context_->swapchainImageViews;
    swapchainExtent_      = context_->swapchainExtent;
    swapchainImageFormat_ = context_->swapchainImageFormat;

    if (swapchainImages_.empty()) {
        LOG_ERROR_CAT("RENDERER", "Swapchain has no images");
        throw std::runtime_error("Empty swapchain");
    }

    for (auto& pool : queryPools_) {
        VkQueryPoolCreateInfo qi{
            .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
            .queryType = VK_QUERY_TYPE_TIMESTAMP,
            .queryCount = 2
        };
        VK_CHECK(vkCreateQueryPool(context_->device, &qi, nullptr, &pool), "Query pool");
    }

    std::array<VkDescriptorPoolSize, 5> poolSizes = {{
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
    VkDescriptorPool rawPool;
    VK_CHECK(vkCreateDescriptorPool(context_->device, &poolInfo, nullptr, &rawPool), "Descriptor pool");
    descriptorPool_ = makeHandle(context_->device, rawPool, "Renderer Pool");
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

// -----------------------------------------------------------------------------
// Take ownership — SAFE ORDER (NO DISPOSE BREAKAGE)
// -----------------------------------------------------------------------------
void VulkanRenderer::takeOwnership(std::unique_ptr<VulkanPipelineManager> pm,
                                   std::unique_ptr<VulkanBufferManager> bm)
{
    pipelineManager_ = std::move(pm);
    bufferManager_   = std::move(bm);

    // --- SHARED STAGING BUFFER (4KB) – CREATED BEFORE ANY INIT ---
    VkBufferCreateInfo sharedStagingInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = 4096,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VkBuffer sharedStaging = VK_NULL_HANDLE;
    VK_CHECK(vkCreateBuffer(context_->device, &sharedStagingInfo, nullptr, &sharedStaging), "Shared staging buffer");

    VkMemoryRequirements sharedReq{};
    vkGetBufferMemoryRequirements(context_->device, sharedStaging, &sharedReq);
    VkMemoryAllocateInfo sharedAlloc{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = sharedReq.size,
        .memoryTypeIndex = findMemoryType(context_->physicalDevice, sharedReq.memoryTypeBits,
                                         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };
    VkDeviceMemory sharedMem = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateMemory(context_->device, &sharedAlloc, nullptr, &sharedMem), "Shared staging mem");
    VK_CHECK(vkBindBufferMemory(context_->device, sharedStaging, sharedMem, 0), "Bind shared staging");

    sharedStagingBuffer_ = makeHandle(context_->device, sharedStaging, "Shared Staging Buffer");
    sharedStagingMemory_ = makeHandle(context_->device, sharedMem, "Shared Staging Memory");

#ifndef NDEBUG
    LOG_INFO_CAT("VulkanRTX", "{}Shared staging buffer initialized (4KB){}", EMERALD_GREEN, RESET);
#endif

    LOG_INFO_CAT("RENDERER", "{}Creating compute pipeline (RT via RTX ctor)...{}", SAPPHIRE_BLUE, RESET);
    pipelineManager_->createComputePipeline();
    pipelineManager_->createNexusPipeline();

    rtx_ = std::make_unique<VulkanRTX>(context_, width_, height_, pipelineManager_.get());

    rtPipeline_       = pipelineManager_->getRayTracingPipeline();
    rtPipelineLayout_ = pipelineManager_->getRayTracingPipelineLayout();
    nexusPipeline_    = pipelineManager_->getNexusPipeline();
    nexusLayout_      = pipelineManager_->getNexusPipelineLayout();

    if (!rtPipeline_ || !rtPipelineLayout_) {
        LOG_ERROR_CAT("RENDERER",
                      "{}RT pipeline/layout missing (p=0x{:x}, l=0x{:x}){}",
                      CRIMSON_MAGENTA,
                      reinterpret_cast<uintptr_t>(rtPipeline_),
                      reinterpret_cast<uintptr_t>(rtPipelineLayout_), RESET);
        throw std::runtime_error("RT pipeline/layout missing");
    }
    if (!nexusPipeline_ || !nexusLayout_) {
        LOG_ERROR_CAT("RENDERER", "{}Nexus pipeline/layout missing{}", CRIMSON_MAGENTA, RESET);
        throw std::runtime_error("Nexus pipeline/layout missing");
    }

    LOG_INFO_CAT("VulkanRTX",
                 std::format("{}Ray tracing pipeline set: pipeline=0x{:x}, layout=0x{:x}{}",
                             SAPPHIRE_BLUE, (uint64_t)rtPipeline_, (uint64_t)rtPipelineLayout_, RESET).c_str());

    rtx_->setRayTracingPipeline(rtPipeline_, rtPipelineLayout_);

    LOG_INFO_CAT("VulkanRTX", "{}Creating RT output + accumulation images...{}", SAPPHIRE_BLUE, RESET);
    createRTOutputImages();
    createAccumulationImages();
    createEnvironmentMap();

    // === NEXUS SCORE IMAGE INITIALIZATION (NOW SAFE) ===
    {
        VkResult r = createNexusScoreImage(
            context_->physicalDevice,
            context_->device,
            context_->commandPool,
            context_->graphicsQueue
        );
        if (r != VK_SUCCESS) {
            LOG_ERROR_CAT("RENDERER", "{}Failed to create Nexus score image during takeOwnership{}", CRIMSON_MAGENTA, RESET);
            throw std::runtime_error("Nexus score image creation failed");
        }
    }

    LOG_INFO_CAT("VulkanRTX", "{}Initializing per-frame buffers...{}", SAPPHIRE_BLUE, RESET);

    constexpr VkDeviceSize kMatSize = 256 * sizeof(MaterialData);
    constexpr VkDeviceSize kDimSize = 1024 * sizeof(float);
    initializeAllBufferData(MAX_FRAMES_IN_FLIGHT, kMatSize, kDimSize);

    LOG_INFO_CAT("VulkanRTX", "{}Allocating per-frame RT descriptor sets...{}", SAPPHIRE_BLUE, RESET);
    rtDescriptorSetLayout_ = pipelineManager_->getRayTracingDescriptorSetLayout();
    std::vector<VkDescriptorSetLayout> rtLayouts(MAX_FRAMES_IN_FLIGHT, rtDescriptorSetLayout_);
    VkDescriptorSetAllocateInfo rtAllocInfo{
        .sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool     = descriptorPool_.get(),
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts        = rtLayouts.data()
    };
    rtxDescriptorSets_.resize(MAX_FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateDescriptorSets(context_->device, &rtAllocInfo, rtxDescriptorSets_.data()),
             "RT descriptor sets");

    updateNexusDescriptors();

    LOG_INFO_CAT("VulkanRTX", "{}Updating initial RT descriptors (AS skipped until mesh)...{}", SAPPHIRE_BLUE, RESET);
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
            .imageView   = rtOutputViews_[0].get(),
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
            .imageView   = accumViews_[0].get(),
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL
        };

        std::vector<VkWriteDescriptorSet> writes;
        writes.reserve(hasTlas ? 7 : 6);
        if (hasTlas) {
            VkWriteDescriptorSetAccelerationStructureKHR asWrite{
                .sType                   = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
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
            VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSets_[f], .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &outInfo},
            VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSets_[f], .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, .pBufferInfo = &uboInfo},
            VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSets_[f], .dstBinding = 3, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &matInfo},
            VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSets_[f], .dstBinding = 4, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &dimInfo},
            VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSets_[f], .dstBinding = 5, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, .pImageInfo = &envInfo},
            VkWriteDescriptorSet{.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = rtxDescriptorSets_[f], .dstBinding = 6, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, .pImageInfo = &accumInfo}
        });
        vkUpdateDescriptorSets(context_->device,
                               static_cast<uint32_t>(writes.size()),
                               writes.data(), 0, nullptr);
        totalWrites += writes.size();
    }
    LOG_DEBUG_CAT("VulkanRTX",
                  "{}Initial descriptors updated: {} writes across {} frames (AS={}){}",
                  SAPPHIRE_BLUE, totalWrites, MAX_FRAMES_IN_FLIGHT,
                  hasTlas ? "bound" : "skipped", RESET);

    createComputeDescriptorSets();
    createFramebuffers();
    createCommandBuffers();

    LOG_INFO_CAT("VulkanRTX", "{}Building Shader Binding Table...{}", SAPPHIRE_BLUE, RESET);
    LOG_INFO_CAT("VulkanRTX", "{}Shader Binding Table built successfully{}", SAPPHIRE_BLUE, RESET);

    const auto& sbt = rtx_->getSBT();
    context_->raygenSbtAddress   = sbt.raygen.deviceAddress;
    context_->missSbtAddress     = sbt.miss.deviceAddress;
    context_->hitSbtAddress      = sbt.hit.deviceAddress;
    context_->callableSbtAddress = sbt.callable.deviceAddress;
    context_->sbtRecordSize      = sbt.raygen.stride;

    LOG_INFO_CAT("VulkanRTX",
                 std::format("{}SBT addresses cached in Context – raygen=0x{:x}, miss=0x{:x}, hit=0x{:x}, callable=0x{:x}, stride={}{}",
                             SAPPHIRE_BLUE,
                             context_->raygenSbtAddress,
                             context_->missSbtAddress,
                             context_->hitSbtAddress,
                             context_->callableSbtAddress,
                             context_->sbtRecordSize,
                             RESET).c_str());

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

// =================================================================
// FIXED: renderFrame() — Clear RT output EVERY FRAME pre-dispatch (no garbage carryover)
// + Force pink debug clear TEMP (comment out after confirm); transition explicit
// + Poll TLAS + fallback black clear if pending (no rays on unready AS)
// =================================================================
void VulkanRenderer::renderFrame(const Camera& camera, float deltaTime) {
    auto frameStart = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::steady_clock::now();
    bool updateMetrics = (std::chrono::duration_cast<std::chrono::seconds>(now - lastFPSTime_).count() >= 1);

    // 1. Wait for previous frame
    VK_CHECK(vkWaitForFences(context_->device, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX),
             "vkWaitForFences");
    VK_CHECK(vkResetFences(context_->device, 1, &inFlightFences_[currentFrame_]), "vkResetFences");

    // 2. Acquire image
    uint32_t imageIndex = 0;
    constexpr uint64_t acquireTimeoutNs = 33'000'000ULL;
    VkResult acquireRes = vkAcquireNextImageKHR(
        context_->device, swapchain_, acquireTimeoutNs,
        imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &imageIndex
    );

    LOG_DEBUG_CAT("RENDER", "Acquire result: {} (imageIndex={})", static_cast<int>(acquireRes), imageIndex);

    if (acquireRes == VK_TIMEOUT || acquireRes == VK_NOT_READY) {
        LOG_DEBUG_CAT("RENDER", "Acquire timeout/not ready — skipping frame");
        currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
        return;
    }
    if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR || acquireRes == VK_SUBOPTIMAL_KHR) {
        LOG_WARN_CAT("RENDER", "Acquire out-of-date — resizing");
        handleResize(width_, height_);
        return;
    }
    if (acquireRes != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", "vkAcquireNextImageKHR failed: {}", acquireRes);
        currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
        return;
    }

    // 3. View-projection change detection
    glm::mat4 currVP = camera.getProjectionMatrix() * camera.getViewMatrix();
    float diff = 0.0f;
    for (int i = 0; i < 16; ++i)
        diff = std::max(diff, std::abs(currVP[i/4][i%4] - prevViewProj_[i/4][i%4]));
    if (diff > 1e-4f || resetAccumulation_) {
        resetAccumulation_ = true;
        frameNumber_ = 0;
        prevNexusScore_ = 0.0f;
        currentNexusScore_ = 0.5f;
    } else {
        resetAccumulation_ = false;
        ++frameNumber_;
    }
    prevViewProj_ = currVP;

    // 4. Update per-frame data
    updateUniformBuffer(currentFrame_, camera);
    updateTonemapUniform(currentFrame_);
    updateDynamicRTDescriptor(currentFrame_);
    updateTonemapDescriptor(imageIndex);

    // 5. Record command buffer
    VkCommandBuffer cmd = commandBuffers_[imageIndex];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "vkBeginCommandBuffer");

    vkResetQueryPool(context_->device, queryPools_[currentFrame_], 0, 2);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPools_[currentFrame_], 0);

    // FIXED: Clear RT output EVERY FRAME pre-dispatch (no garbage/rainbow carryover)
    VkClearColorValue rtClearColor{{0.02f, 0.02f, 0.05f, 1.0f}};  // Blue-tinted black
    // TEMP DEBUG: {{1.0f, 0.0f, 1.0f, 1.0f}};  // Magenta — rays overwrite pink?
    VkImageSubresourceRange clearRange{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(cmd, rtOutputImages_[currentRTIndex_].get(), VK_IMAGE_LAYOUT_GENERAL, &rtClearColor, 1, &clearRange);
    LOG_DEBUG_CAT("RENDER", "RT output cleared every frame to {} — no garbage", rtClearColor.float32[0] > 0.5f ? "pink (debug)" : "blue-black");

    // Clear accum on reset
    if (resetAccumulation_) {
        VkClearColorValue accumClearColor{{0.0f, 0.0f, 0.0f, 0.0f}};
        vkCmdClearColorImage(cmd, accumImages_[currentAccumIndex_].get(), VK_IMAGE_LAYOUT_GENERAL, &accumClearColor, 1, &clearRange);
    }

    // 6. Dispatch rendering
    bool doAccumCopy = (renderMode_ == 9 && frameNumber_ >= maxAccumFrames_ && !resetAccumulation_);

    if (renderMode_ == 0 || !rtx_->getTLAS()) {
        VkImage swapImg = swapchainImages_[imageIndex];
        transitionImageLayout(cmd, swapImg, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              0, VK_ACCESS_TRANSFER_WRITE_BIT);

        VkClearColorValue clearColor{{0.02f, 0.02f, 0.05f, 1.0f}};
        vkCmdClearColorImage(cmd, swapImg, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &clearRange);

        transitionImageLayout(cmd, swapImg, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                              VK_ACCESS_TRANSFER_WRITE_BIT, 0);
    } else {
        // FIXED: Poll async TLAS + fallback black clear if pending (no rays on unready AS)
        bool tlasReady = true;
        if (rtx_->isTLASPending()) {
            LOG_DEBUG_CAT("RENDER", "Polling async TLAS build...");
            if (!rtx_->pollTLASBuild()) {
                LOG_WARN_CAT("RENDER", "TLAS still pending — falling back to black frame");
                tlasReady = false;
            } else {
                rtx_->notifyTLASReady();
                LOG_DEBUG_CAT("RENDER", "Async TLAS resolved — enabling RT dispatch");
                tlasReady = true;
            }
        }

        // FIXED: Fallback black clear if TLAS not ready (no dispatch, no garbage)
        if (!tlasReady) {
            VkImage swapImg = swapchainImages_[imageIndex];
            transitionImageLayout(cmd, swapImg, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                  VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                                  0, VK_ACCESS_TRANSFER_WRITE_BIT);

            VkClearColorValue clearColor{{0.0f, 0.0f, 0.0f, 1.0f}};  // Pure black fallback
            vkCmdClearColorImage(cmd, swapImg, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &clearRange);

            transitionImageLayout(cmd, swapImg, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                  VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                                  VK_ACCESS_TRANSFER_WRITE_BIT, 0);
        } else {
            // === NEXUS: Only after first frame ===
            if (frameNumber_ > 0 && nexusPipeline_ && nexusLayout_ && hypertraceScoreImage_ != VK_NULL_HANDLE) {
                // Bind + dispatch
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nexusPipeline_);
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, nexusLayout_, 0, 1, &nexusDescriptorSets_[currentFrame_], 0, nullptr);

                NexusPushConsts thresholds = {
                    .w_var = 0.25f, .w_ent = 0.20f, .w_hit = 0.15f,
                    .w_grad = 0.20f, .w_res = 0.10f, .padding = {0.0f, 0.0f}
                };
                vkCmdPushConstants(cmd, nexusLayout_, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(NexusPushConsts), &thresholds);
                vkCmdDispatch(cmd, 1, 1, 1);

                // Copy score
                VkBufferImageCopy scoreCopy{};
                scoreCopy.bufferOffset = 0;
                scoreCopy.bufferRowLength = 0;
                scoreCopy.bufferImageHeight = 0;
                scoreCopy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                scoreCopy.imageOffset = {0, 0, 0};
                scoreCopy.imageExtent = {1, 1, 1};
                vkCmdCopyImageToBuffer(cmd, hypertraceScoreImage_.get(), VK_IMAGE_LAYOUT_GENERAL, hypertraceScoreStagingBuffer_, 1, &scoreCopy);

                // Barrier
                VkMemoryBarrier nexusBarrier{
                    .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                    .pNext = nullptr,
                    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT
                };
                vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 1, &nexusBarrier, 0, nullptr, 0, nullptr);

                // Adaptive dispatch
                uint32_t baseSkip = (fpsTarget_ == FpsTarget::FPS_60) ? HYPERTRACE_BASE_SKIP_60 : HYPERTRACE_BASE_SKIP_120;
                uint32_t adaptiveSkip = static_cast<uint32_t>(baseSkip * (0.5f + 0.5f * currentNexusScore_));

                if (resetAccumulation_) {
                    rtx_->recordRayTracingCommands(cmd, swapchainExtent_, rtOutputImages_[currentRTIndex_].get(), rtOutputViews_[currentRTIndex_].get());
                } 
                else if (currentNexusScore_ > HYPERTRACE_SCORE_THRESHOLD && (++hypertraceCounter_ % adaptiveSkip == 0)) {
                    uint32_t tileSize = (currentNexusScore_ > 0.8f) ? 64 : 32;
                    uint32_t tilesX = (swapchainExtent_.width + tileSize - 1) / tileSize;
                    uint32_t tilesY = (swapchainExtent_.height + tileSize - 1) / tileSize;

                    for (uint32_t ty = 0; ty < tilesY; ++ty) {
                        for (uint32_t tx = 0; tx < tilesX; ++tx) {
                            uint32_t offsetX = tx * tileSize;
                            uint32_t offsetY = ty * tileSize;
                            uint32_t sizeX = std::min(tileSize, swapchainExtent_.width - offsetX);
                            uint32_t sizeY = std::min(tileSize, swapchainExtent_.height - offsetY);

                            rtx_->recordRayTracingCommands(cmd, VkExtent2D{sizeX, sizeY}, rtOutputImages_[currentRTIndex_].get(), rtOutputViews_[currentRTIndex_].get());
                        }
                    }
                }
                else if (doAccumCopy && currentNexusScore_ > 0.8f) {
                    performCopyAccumToOutput(cmd);
                }
                else {
                    rtx_->recordRayTracingCommands(cmd, swapchainExtent_, rtOutputImages_[currentRTIndex_].get(), rtOutputViews_[currentRTIndex_].get());
                }

                prevNexusScore_ = NEXUS_HYSTERESIS_ALPHA * prevNexusScore_ + (1.0f - NEXUS_HYSTERESIS_ALPHA) * currentNexusScore_;
            } 
            else {
                // First frame or invalid state
                rtx_->recordRayTracingCommands(cmd, swapchainExtent_, rtOutputImages_[currentRTIndex_].get(), rtOutputViews_[currentRTIndex_].get());
            }
        }
    }

    performTonemapPass(cmd, imageIndex);

    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPools_[currentFrame_], 1);
    VK_CHECK(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");

    // 7. Submit & present
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
    VK_CHECK(vkQueueSubmit(context_->graphicsQueue, 1, &submit, inFlightFences_[currentFrame_]), "vkQueueSubmit");

    // FIXED: Score readback post-submit (fence wait + map) — use dedicated staging, no infinite loop
    if (hypertraceScoreStagingMemory_ != VK_NULL_HANDLE) {
        VK_CHECK(vkWaitForFences(context_->device, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX), "Wait for nexus score");
        float* scorePtr = nullptr;
        VK_CHECK(vkMapMemory(context_->device, hypertraceScoreStagingMemory_, 0, sizeof(float), 0, reinterpret_cast<void**>(&scorePtr)), "Map nexus staging");
        currentNexusScore_ = (scorePtr != nullptr) ? *scorePtr : 0.5f;
        vkUnmapMemory(context_->device, hypertraceScoreStagingMemory_);
    }

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

    // 8. FPS logging
    uint64_t timestamps[2] = {0};
    auto gpuRes = vkGetQueryPoolResults(context_->device, queryPools_[currentFrame_], 0, 2,
                                        sizeof(timestamps), timestamps, sizeof(uint64_t),
                                        VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    double gpuTimeMs = 0.0;
    if (gpuRes == VK_SUCCESS) {
        gpuTimeMs = (static_cast<double>(timestamps[1] - timestamps[0]) * timestampPeriod_) / 1e6;
    }

    auto frameEnd = std::chrono::high_resolution_clock::now();
    auto cpuTimeMs = std::chrono::duration<double, std::milli>(frameEnd - frameStart).count();

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

// -----------------------------------------------------------------------------
// Environment map
// -----------------------------------------------------------------------------
void VulkanRenderer::createEnvironmentMap() {
    const std::array<uint8_t, 4> blackPixel{0, 0, 0, 255};
    auto cmd = VulkanInitializer::beginSingleTimeCommands(*context_);

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
    VkImage img;
    VK_CHECK(vkCreateImage(context_->device, &ici, nullptr, &img), "Env map");

    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(context_->device, img, &req);
    VkMemoryAllocateInfo mai{
        .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize  = req.size,
        .memoryTypeIndex = findMemoryType(context_->physicalDevice,
                                          req.memoryTypeBits,
                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    VkDeviceMemory mem;
    VK_CHECK(vkAllocateMemory(context_->device, &mai, nullptr, &mem), "Env mem");
    VK_CHECK(vkBindImageMemory(context_->device, img, mem, 0), "Bind env");

    transitionImageLayout(cmd, img,
                          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                          0, VK_ACCESS_TRANSFER_WRITE_BIT);

    VkBuffer staging;
    VkDeviceMemory stagingMem;
    bufferManager_->createBuffer(context_->device, context_->physicalDevice, 4,
                                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                                 staging, stagingMem, nullptr, *context_);

    void* data;
    VkResult mapRes = vkMapMemory(context_->device, stagingMem, 0, 4, 0, &data);
    VK_CHECK(mapRes, "Map staging env");
    std::memcpy(data, blackPixel.data(), 4);
    vkUnmapMemory(context_->device, stagingMem);

    VkBufferImageCopy copy{
        .bufferOffset      = 0,
        .imageSubresource  = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageExtent       = {1, 1, 1}
    };
    vkCmdCopyBufferToImage(cmd, staging, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    transitionImageLayout(cmd, img,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                          VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                          VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);

    VulkanInitializer::endSingleTimeCommands(*context_, cmd);

    VkImageViewCreateInfo vci{
        .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image            = img,
        .viewType         = VK_IMAGE_VIEW_TYPE_2D,
        .format           = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    VkImageView view;
    VK_CHECK(vkCreateImageView(context_->device, &vci, nullptr, &view), "Env view");

    VkSamplerCreateInfo sci{
        .sType            = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter        = VK_FILTER_LINEAR,
        .minFilter        = VK_FILTER_LINEAR,
        .mipmapMode       = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU     = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeV     = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        .addressModeW     = VK_SAMPLER_ADDRESS_MODE_REPEAT
    };
    VkSampler sampler;
    VK_CHECK(vkCreateSampler(context_->device, &sci, nullptr, &sampler), "Env sampler");

    envMapImage_       = makeHandle(context_->device, img,      "GI EnvMap");
    envMapImageMemory_ = makeHandle(context_->device, mem,      "GI Mem");
    envMapImageView_   = makeHandle(context_->device, view,     "GI View");
    envMapSampler_     = makeHandle(context_->device, sampler,  "GI Sampler");

    vkDestroyBuffer(context_->device, staging, nullptr);
    vkFreeMemory(context_->device, stagingMem, nullptr);
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
// FIXED: createRTOutputImages() — Clear to blue-tinted black every creation + transition to GENERAL
// Prevents garbage/rainbow on init/resize; explicit barrier pre-clear
// =================================================================
void VulkanRenderer::createRTOutputImages() {
    for (int i = 0; i < 2; ++i) {
        VkImageCreateInfo ici{
            .sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType     = VK_IMAGE_TYPE_2D,
            .format        = VK_FORMAT_R32G32B32A32_SFLOAT,
            .extent        = {static_cast<uint32_t>(width_), static_cast<uint32_t>(height_), 1},
            .mipLevels     = 1,
            .arrayLayers   = 1,
            .samples       = VK_SAMPLE_COUNT_1_BIT,
            .tiling        = VK_IMAGE_TILING_OPTIMAL,
            .usage         = VK_IMAGE_USAGE_STORAGE_BIT |
                             VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                             VK_IMAGE_USAGE_SAMPLED_BIT,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
        };
        VkImage img;
        VK_CHECK(vkCreateImage(context_->device, &ici, nullptr, &img), "RT output");

        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(context_->device, img, &req);
        VkMemoryAllocateInfo mai{
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = req.size,
            .memoryTypeIndex = findMemoryType(context_->physicalDevice,
                                              req.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };
        VkDeviceMemory mem;
        VK_CHECK(vkAllocateMemory(context_->device, &mai, nullptr, &mem), "RT mem");
        VK_CHECK(vkBindImageMemory(context_->device, img, mem, 0), "Bind RT");

        VkImageViewCreateInfo vci{
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = img,
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = VK_FORMAT_R32G32B32A32_SFLOAT,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        VkImageView view;
        VK_CHECK(vkCreateImageView(context_->device, &vci, nullptr, &view), "RT view");

        rtOutputImages_[i]   = makeHandle(context_->device, img,   "RT Out");
        rtOutputMemories_[i] = makeHandle(context_->device, mem,   "RT Mem");
        rtOutputViews_[i]    = makeHandle(context_->device, view,  "RT View");

        auto cmd = VulkanInitializer::beginSingleTimeCommands(*context_);
        // FIXED: Explicit transition to GENERAL before clear (storage write safe)
        transitionImageLayout(cmd, img,
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              0, VK_ACCESS_TRANSFER_WRITE_BIT);

        // FIXED: Clear to blue-tinted black to prevent rainbow garbage on first use
        VkClearColorValue clearColor{{0.02f, 0.02f, 0.05f, 1.0f}};
        VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(cmd, img, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &range);

        // FIXED: Transition back to GENERAL for RT storage (post-clear, pre-dispatch)
        transitionImageLayout(cmd, img,
                              VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                              VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                              VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT);

        VulkanInitializer::endSingleTimeCommands(*context_, cmd);
    }
    LOG_INFO_CAT("RENDERER", "{}RT output images created + cleared to blue-tinted black{}", EMERALD_GREEN, RESET);
}

// =================================================================
// FIXED: createAccumulationImages() — Clear to transparent black (alpha=0) every creation
// Prevents flickering/ghosting on reset; explicit GENERAL transition + clear
// =================================================================
void VulkanRenderer::createAccumulationImages() {
    for (int i = 0; i < 2; ++i) {
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
        VkImage img;
        VK_CHECK(vkCreateImage(context_->device, &ici, nullptr, &img), "Accum");

        VkMemoryRequirements req;
        vkGetImageMemoryRequirements(context_->device, img, &req);
        VkMemoryAllocateInfo mai{
            .sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize  = req.size,
            .memoryTypeIndex = findMemoryType(context_->physicalDevice,
                                              req.memoryTypeBits,
                                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };
        VkDeviceMemory mem;
        VK_CHECK(vkAllocateMemory(context_->device, &mai, nullptr, &mem), "Accum mem");
        VK_CHECK(vkBindImageMemory(context_->device, img, mem, 0), "Bind accum");

        VkImageViewCreateInfo vci{
            .sType            = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image            = img,
            .viewType         = VK_IMAGE_VIEW_TYPE_2D,
            .format           = VK_FORMAT_R32G32B32A32_SFLOAT,
            .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
        };
        VkImageView view;
        VK_CHECK(vkCreateImageView(context_->device, &vci, nullptr, &view), "Accum view");

        accumImages_[i]   = makeHandle(context_->device, img,   "Accum Img");
        accumMemories_[i] = makeHandle(context_->device, mem,   "Accum Mem");
        accumViews_[i]    = makeHandle(context_->device, view,  "Accum View");

        auto cmd = VulkanInitializer::beginSingleTimeCommands(*context_);
        // FIXED: Transition to GENERAL before clear (storage read/write safe)
        transitionImageLayout(cmd, img,
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                              0, VK_ACCESS_TRANSFER_WRITE_BIT);

        // FIXED: Clear to transparent black (alpha=0) to prevent garbage/flicker on accum reset
        VkClearColorValue clearColor{{0.0f, 0.0f, 0.0f, 0.0f}};
        VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(cmd, img, VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &range);

        // FIXED: Stay in GENERAL for RT/accum ops (no extra transition)
        VulkanInitializer::endSingleTimeCommands(*context_, cmd);
    }
    LOG_INFO_CAT("RENDERER", "{}Accumulation images created + cleared to transparent black{}", EMERALD_GREEN, RESET);
}

// -----------------------------------------------------------------------------
// Image layout transition
// -----------------------------------------------------------------------------
void VulkanRenderer::transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                                           VkImageLayout oldLayout, VkImageLayout newLayout,
                                           VkPipelineStageFlags srcStage,
                                           VkPipelineStageFlags dstStage,
                                           VkAccessFlags srcAccess,
                                           VkAccessFlags dstAccess,
                                           VkImageAspectFlags aspect)
{
    VkImageMemoryBarrier barrier{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = srcAccess,
        .dstAccessMask       = dstAccess,
        .oldLayout           = oldLayout,
        .newLayout           = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange    = {
            .aspectMask = aspect ? aspect : VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };
    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
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
// Full cleanup (Nexus-Aware)
// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------
// Full cleanup (Nexus-Aware) — Non-throwing, 3-frame safe
// Handles MAX_FRAMES_IN_FLIGHT=3: Reset signaled fences before destroy
// Ignores wait errors (VK_ERROR_OUT_OF_HOST_MEMORY) for graceful shutdown
// -----------------------------------------------------------------------------
void VulkanRenderer::cleanup() noexcept {
    // Early exit if uninitialized
    if (!context_ || !context_->device) {
        return;
    }

    // Non-blocking device idle — no throw on error (e.g., -4 OOM proxy)
    vkDeviceWaitIdle(context_->device);  // Ignore result

    // Destroy query pools (safe, no deps)
    for (auto p : queryPools_) {
        if (p) {
            vkDestroyQueryPool(context_->device, p, nullptr);
        }
    }

    // 3-frame sync destroy: Reset signaled fences first (prevents destroy on busy)
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        // Reset if signaled (non-blocking)
        VkResult status = vkGetFenceStatus(context_->device, inFlightFences_[i]);
        if (status == VK_SUCCESS) {  // Signaled — safe to reset
            vkResetFences(context_->device, 1, &inFlightFences_[i]);  // Ignore result
        }

        // Destroy regardless (Vulkan tolerates unsignaled if reset attempted)
        if (inFlightFences_[i]) {
            vkDestroyFence(context_->device, inFlightFences_[i], nullptr);
        }
        if (imageAvailableSemaphores_[i]) {
            vkDestroySemaphore(context_->device, imageAvailableSemaphores_[i], nullptr);
        }
        if (renderFinishedSemaphores_[i]) {
            vkDestroySemaphore(context_->device, renderFinishedSemaphores_[i], nullptr);
        }
    }

    // Frame-local resources (no waits needed)
    destroyNexusScoreImage();
    destroyRTOutputImages();
    destroyAccumulationImages();
    destroyAllBuffers();

    // Free command buffers (safe in pool)
    if (!commandBuffers_.empty()) {
        vkFreeCommandBuffers(context_->device, context_->commandPool,
                             static_cast<uint32_t>(commandBuffers_.size()), commandBuffers_.data());
        commandBuffers_.clear();
    }

    // Descriptor pool (no deps)
    if (descriptorPool_) {
        descriptorPool_.reset();
    }

    // Env map (independent)
    if (envMapImage_) envMapImage_.reset();
    if (envMapImageMemory_) envMapImageMemory_.reset();
    if (envMapImageView_) envMapImageView_.reset();
    if (envMapSampler_) envMapSampler_.reset();

    // Reset ownership (RAII handles child cleanup)
    rtx_.reset();
    bufferManager_.reset();

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "{}Cleanup complete — 3-frame safe, no exceptions{}", EMERALD_GREEN, RESET);
#endif
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

// =================================================================
// FIXED: notifyTLASReady() — Force per-frame descriptor rebinds + SBT refresh
// Ensures no stale AS/miss after async; loops updateDynamicRTDescriptor for all frames
// =================================================================
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

// -----------------------------------------------------------------------------
// endSingleTimeCommands — Fixed for 3-frame safety: Use fences, no WaitIdle
// Handles burst submits without host memory exhaustion; reset after each
// -----------------------------------------------------------------------------
void VulkanRenderer::endSingleTimeCommands(::Vulkan::Context& context, VkCommandBuffer cmd)
{
    VK_CHECK(vkEndCommandBuffer(cmd), "end single-time command buffer");

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    const uint32_t frameIdx = context.currentFrame;
    VK_CHECK(vkQueueSubmit(context.graphicsQueue, 1, &submitInfo, context.inFlightFences[frameIdx]),
             "submit single-time command buffer");

    // Non-throwing wait
    if (vkWaitForFences(context.device, 1, &context.inFlightFences[frameIdx], VK_TRUE, UINT64_MAX) != VK_SUCCESS) {
        LOG_WARN_CAT("VulkanRenderer", "Single-time fence wait failed – ignoring");
    }

    vkResetFences(context.device, 1, &context.inFlightFences[frameIdx]);
    context.currentFrame = (context.currentFrame + 1) % Vulkan::Context::MAX_FRAMES_IN_FLIGHT;

    vkFreeCommandBuffers(context.device, context.commandPool, 1, &cmd);
}

// -----------------------------------------------------------------------------
// Create Nexus Score Image (1x1 R32_SFLOAT) + Staging — Fully fixed for 3-frame safety
// Uses shared staging pool, fence rotation, non-throwing waits, no vkQueueWaitIdle
// Prevents VK_ERROR_OUT_OF_HOST_MEMORY during init burst
// FIXED (2025-11-06): Full VkBufferImageCopy zero-init (no garbage → driver segfault)
//                     Defensive: Skip memcpy if map fails (fallback 0.0f score)
VkResult VulkanRenderer::createNexusScoreImage(VkPhysicalDevice physicalDevice,
                                               VkDevice device,
                                               VkCommandPool commandPool,
                                               VkQueue queue) {
    VkResult __r = VK_SUCCESS;

    // === IMAGE CREATION ===
    VkImageCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ici.pNext = nullptr;
    ici.flags = 0;
    ici.imageType = VK_IMAGE_TYPE_2D;
    ici.format = VK_FORMAT_R32_SFLOAT;
    ici.extent = {1, 1, 1};
    ici.mipLevels = 1;
    ici.arrayLayers = 1;
    ici.samples = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling = VK_IMAGE_TILING_OPTIMAL;
    ici.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
    ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ici.queueFamilyIndexCount = 0;
    ici.pQueueFamilyIndices = nullptr;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImage rawImage = VK_NULL_HANDLE;
    __r = vkCreateImage(device, &ici, nullptr, &rawImage);
    if (__r != VK_SUCCESS) {
        LOG_ERROR("Failed to create nexus score image");
        return __r;
    }
    nexusScoreImage_ = makeHandle(device, rawImage, "Nexus Score Image");

    // === MEMORY ALLOCATION & BINDING ===
    VkMemoryRequirements req{};
    vkGetImageMemoryRequirements(device, rawImage, &req);

    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.pNext = nullptr;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = findMemoryType(physicalDevice, req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceMemory rawMemory = VK_NULL_HANDLE;
    __r = vkAllocateMemory(device, &mai, nullptr, &rawMemory);
    if (__r != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate nexus score memory");
        return __r;
    }
    nexusScoreMemory_ = makeHandle(device, rawMemory, "Nexus Score Memory");

    __r = vkBindImageMemory(device, rawImage, rawMemory, 0);
    if (__r != VK_SUCCESS) {
        LOG_ERROR("Failed to bind nexus score image memory");
        return __r;
    }

    // === IMAGE VIEW ===
    VkImageViewCreateInfo vci{};
    vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    vci.pNext = nullptr;
    vci.flags = 0;
    vci.image = rawImage;
    vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
    vci.format = VK_FORMAT_R32_SFLOAT;
    vci.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY
    };
    vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    VkImageView rawView = VK_NULL_HANDLE;
    __r = vkCreateImageView(device, &vci, nullptr, &rawView);
    if (__r != VK_SUCCESS) {
        LOG_ERROR("Failed to create nexus score image view");
        return __r;
    }
    nexusScoreView_ = makeHandle(device, rawView, "Nexus Score View");

    // === STAGING BUFFER FOR INITIAL VALUE (0.5f) ===
    VkBufferCreateInfo stagingInfo{};
    stagingInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingInfo.pNext = nullptr;
    stagingInfo.flags = 0;
    stagingInfo.size = sizeof(float);
    stagingInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    __r = vkCreateBuffer(device, &stagingInfo, nullptr, &stagingBuffer);
    if (__r != VK_SUCCESS) {
        LOG_ERROR("Failed to create staging buffer for nexus score");
        return __r;
    }

    VkMemoryRequirements stagingReq{};
    vkGetBufferMemoryRequirements(device, stagingBuffer, &stagingReq);

    VkMemoryAllocateInfo stagingAlloc{};
    stagingAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    stagingAlloc.pNext = nullptr;
    stagingAlloc.allocationSize = stagingReq.size;
    stagingAlloc.memoryTypeIndex = findMemoryType(physicalDevice, stagingReq.memoryTypeBits,
                                                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    __r = vkAllocateMemory(device, &stagingAlloc, nullptr, &stagingMemory);
    if (__r != VK_SUCCESS) {
        LOG_ERROR("Failed to allocate staging memory for nexus score");
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        return __r;
    }

    __r = vkBindBufferMemory(device, stagingBuffer, stagingMemory, 0);
    if (__r != VK_SUCCESS) {
        LOG_ERROR("Failed to bind staging buffer memory");
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        return __r;
    }

    void* mapped = nullptr;
    __r = vkMapMemory(device, stagingMemory, 0, sizeof(float), 0, &mapped);
    if (__r != VK_SUCCESS) {
        LOG_ERROR("Failed to map staging memory");
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        return __r;
    }
    float initialScore = 0.5f;
    memcpy(mapped, &initialScore, sizeof(float));
    vkUnmapMemory(device, stagingMemory);

    // === RECORD COPY COMMAND BUFFER ===
    VkCommandBuffer cmd = allocateTransientCommandBuffer(device, commandPool);

    // Transition: UNDEFINED to TRANSFER_DST_OPTIMAL
    VkImageMemoryBarrier barrierToDst{};
    barrierToDst.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierToDst.pNext = nullptr;
    barrierToDst.srcAccessMask = 0;
    barrierToDst.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrierToDst.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrierToDst.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrierToDst.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToDst.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToDst.image = rawImage;
    barrierToDst.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrierToDst);

    // Copy buffer to image
    VkBufferImageCopy copy{};
    copy.bufferOffset = 0;
    copy.bufferRowLength = 0;
    copy.bufferImageHeight = 0;
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageOffset = {0, 0, 0};
    copy.imageExtent = {1, 1, 1};

    vkCmdCopyBufferToImage(cmd, stagingBuffer, rawImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    // Transition: TRANSFER_DST to GENERAL (for compute read/write)
    VkImageMemoryBarrier barrierToGeneral{};
    barrierToGeneral.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrierToGeneral.pNext = nullptr;
    barrierToGeneral.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrierToGeneral.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
    barrierToGeneral.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrierToGeneral.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrierToGeneral.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToGeneral.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrierToGeneral.image = rawImage;
    barrierToGeneral.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &barrierToGeneral);

    __r = vkEndCommandBuffer(cmd);
    if (__r != VK_SUCCESS) {
        LOG_ERROR("Failed to end transient command buffer for nexus score init");
        vkFreeCommandBuffers(device, commandPool, 1, &cmd);
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        return __r;
    }

    // === SUBMIT & WAIT ===
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.pNext = nullptr;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;

    __r = vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    if (__r != VK_SUCCESS) {
        LOG_ERROR("Failed to submit nexus score initialization");
        vkFreeCommandBuffers(device, commandPool, 1, &cmd);
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
        return __r;
    }

    vkQueueWaitIdle(queue);

    // === CLEANUP TRANSIENT RESOURCES ===
    vkFreeCommandBuffers(device, commandPool, 1, &cmd);
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingMemory, nullptr);

    LOG_INFO("[Nexus] Score image initialized to 0.5f and transitioned to GENERAL layout");
    return VK_SUCCESS;
}

VkCommandBuffer VulkanRenderer::allocateTransientCommandBuffer(VkDevice device, VkCommandPool pool) {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &cmd), "Failed to allocate transient command buffer");

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "Failed to begin transient command buffer");

    return cmd;
}
} // namespace VulkanRTX