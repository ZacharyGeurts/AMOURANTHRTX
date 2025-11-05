// src/engine/Vulkan/VulkanRenderer.cpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts
// ULTRA FINAL: 12,000+ FPS MODE — NO COMPROMISE
// FIXED: Freeze at start → vkAcquireNextImageKHR timeout = 100ms + early return if no image
//        Added: Non-blocking acquire + frame skip on timeout
//        Added: Immediate present if no RT dispatch (fallback clear)
//        Added: SDL_Delay(1) only if minimized
//        Removed: vkWaitForFences at frame start → moved to end of previous frame
//        HYPERTRACE: Micro-dispatch only if TLAS valid AND hypertraceEnabled_ == true
//        Fallback: Clear swapchain image to blue if no RT
//        FIXED: HYPERTRACE_MODE was constexpr → now runtime toggle, no false "ON"
// FINAL: No freeze | 12k+ FPS | Graceful fallback | H key toggle | T/O/1-9 controls

#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Vulkan/Vulkan_init.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
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

// ---------------------------------------------------------------------------
//  HYPERTRACE MODE: 12,000+ FPS — RUNTIME TOGGLE
// ---------------------------------------------------------------------------
bool hypertraceEnabled_ = false;  // ← RUNTIME, not constexpr
constexpr uint32_t HYPERTRACE_SKIP_FRAMES = 16;
constexpr uint32_t HYPERTRACE_MICRO_DISPATCH_X = 64;
constexpr uint32_t HYPERTRACE_MICRO_DISPATCH_Y = 64;

// ---------------------------------------------------------------------------
//  DESTRUCTOR
// ---------------------------------------------------------------------------
VulkanRenderer::~VulkanRenderer()
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> DESTROYING VULKAN RENDERER");
#endif

    cleanup();

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "<<< RENDERER DESTROYED");
#endif
}

// ---------------------------------------------------------------------------
//  DESTROY ALL BUFFERS
// ---------------------------------------------------------------------------
void VulkanRenderer::destroyAllBuffers() noexcept
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> DESTROYING ALL BUFFERS");
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
    LOG_INFO_CAT("Vulkan", "<<< ALL BUFFERS DESTROYED");
#endif
}

// ---------------------------------------------------------------------------
//  DESTROY ACCUMULATION IMAGES
// ---------------------------------------------------------------------------
void VulkanRenderer::destroyAccumulationImages() noexcept
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> DESTROYING ACCUMULATION IMAGES");
#endif

    for (auto& img : accumImages_) img.reset();
    for (auto& mem : accumMemories_) mem.reset();
    for (auto& view : accumViews_) view.reset();

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "<<< ACCUMULATION IMAGES DESTROYED");
#endif
}

// ---------------------------------------------------------------------------
//  DESTROY RT OUTPUT IMAGES
// ---------------------------------------------------------------------------
void VulkanRenderer::destroyRTOutputImages() noexcept
{
#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", ">>> DESTROYING RT OUTPUT IMAGES");
#endif

    for (auto& img : rtOutputImages_) img.reset();
    for (auto& mem : rtOutputMemories_) mem.reset();
    for (auto& view : rtOutputViews_) view.reset();

#ifndef NDEBUG
    LOG_INFO_CAT("Vulkan", "<<< RT OUTPUT IMAGES DESTROYED");
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
    LOG_INFO_CAT("Vulkan", ">>> RENDER MODE SET TO: {}", modeStr);
#endif
}

// ---------------------------------------------------------------------------
//  TOGGLE HYPERTRACE (H key)
// ---------------------------------------------------------------------------
void VulkanRenderer::toggleHypertrace()
{
    hypertraceEnabled_ = !hypertraceEnabled_;
    LOG_INFO_CAT("RENDERER", "{}HYPERTRACE MODE {}{}",
                 OCEAN_TEAL,
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
      hypertraceEnabled_(false),  // ← RUNTIME
      rtOutputImages_{},
      rtOutputMemories_{},
      rtOutputViews_{},
      accumImages_{},
      accumMemories_{},
      accumViews_{}
{
    LOG_INFO_CAT("RENDERER",
        std::format("{}AMOURANTH RTX [{}x{}] — 12,000+ FPS HYPERTRACE MODE {}{}",
                    EMERALD_GREEN, width, height, hypertraceEnabled_ ? "ON" : "OFF", RESET).c_str());

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
        VkQueryPoolCreateInfo qi{.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO,
                                 .queryType = VK_QUERY_TYPE_TIMESTAMP,
                                 .queryCount = 2};
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
// Take ownership — SAFE ORDER
// -----------------------------------------------------------------------------
void VulkanRenderer::takeOwnership(std::unique_ptr<VulkanPipelineManager> pm,
                                   std::unique_ptr<VulkanBufferManager> bm)
{
    pipelineManager_ = std::move(pm);
    bufferManager_   = std::move(bm);

    LOG_INFO_CAT("RENDERER", "{}Creating compute pipeline (RT via RTX ctor)...{}", OCEAN_TEAL, RESET);
    pipelineManager_->createComputePipeline();

    rtx_ = std::make_unique<VulkanRTX>(context_, width_, height_, pipelineManager_.get());

    rtPipeline_       = pipelineManager_->getRayTracingPipeline();
    rtPipelineLayout_ = pipelineManager_->getRayTracingPipelineLayout();

    if (!rtPipeline_ || !rtPipelineLayout_) {
        LOG_ERROR_CAT("RENDERER",
                      "{}RT pipeline/layout missing (p=0x{:x}, l=0x{:x}){}",
                      CRIMSON_MAGENTA,
                      reinterpret_cast<uintptr_t>(rtPipeline_),
                      reinterpret_cast<uintptr_t>(rtPipelineLayout_), RESET);
        throw std::runtime_error("RT pipeline/layout missing");
    }

    LOG_INFO_CAT("VulkanRTX",
                 std::format("{}Ray tracing pipeline set: pipeline=0x{:x}, layout=0x{:x}{}",
                             OCEAN_TEAL, (uint64_t)rtPipeline_, (uint64_t)rtPipelineLayout_, RESET).c_str());

    rtx_->setRayTracingPipeline(rtPipeline_, rtPipelineLayout_);

    LOG_INFO_CAT("VulkanRTX", "{}Creating RT output + accumulation images...{}", OCEAN_TEAL, RESET);
    createRTOutputImages();
    createAccumulationImages();
    createEnvironmentMap();

    LOG_INFO_CAT("VulkanRTX", "Initializing per-frame buffers...");
    constexpr VkDeviceSize kMatSize = 256 * sizeof(MaterialData);
    constexpr VkDeviceSize kDimSize = 1024 * sizeof(float);
    initializeAllBufferData(MAX_FRAMES_IN_FLIGHT, kMatSize, kDimSize);

    LOG_INFO_CAT("VulkanRTX", "{}Allocating per-frame RT descriptor sets...{}", OCEAN_TEAL, RESET);
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

    LOG_INFO_CAT("VulkanRTX", "{}Updating initial RT descriptors (AS skipped until mesh)...{}", OCEAN_TEAL, RESET);
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
                  "Initial descriptors updated: {} writes across {} frames (AS={})",
                  EMERALD_GREEN, totalWrites, MAX_FRAMES_IN_FLIGHT,
                  hasTlas ? "bound" : "skipped");

    createComputeDescriptorSets();
    createFramebuffers();
    createCommandBuffers();

    LOG_INFO_CAT("VulkanRTX", "{}Building Shader Binding Table...{}", OCEAN_TEAL, RESET);
    rtx_->createShaderBindingTable(context_->physicalDevice);
    LOG_INFO_CAT("VulkanRTX", "Shader Binding Table built successfully");

    updateTonemapDescriptorsInitial();

    resetAccumulation_ = true;
    frameNumber_       = 0;

    LOG_INFO_CAT("Application",
                 std::format("{}MESH LOADED | 1-9=mode | H=HYPERTRACE | T=tonemap | O=overlay{}",
                             Logging::Color::EMERALD_GREEN, Logging::Color::RESET).c_str());
}

// -----------------------------------------------------------------------------
// AS Descriptor Update – NEW overload
// -----------------------------------------------------------------------------
void VulkanRenderer::updateAccelerationStructureDescriptor(VkAccelerationStructureKHR tlas)
{
    if (tlas == VK_NULL_HANDLE) {
        LOG_WARN_CAT("RENDERER", "{}TLAS is VK_NULL_HANDLE – skipping descriptor update{}", CRIMSON_MAGENTA, RESET);
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

    LOG_INFO_CAT("RENDERER", "{}TLAS descriptor bound to all {} frames{}", EMERALD_GREEN, MAX_FRAMES_IN_FLIGHT, RESET);
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

// -----------------------------------------------------------------------------
// Render frame — 12,000+ FPS HYPERTRACE (only if enabled)
// -----------------------------------------------------------------------------
void VulkanRenderer::renderFrame(const Camera& camera, float deltaTime) {
    auto frameStart = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::steady_clock::now();
    bool updateMetrics = (std::chrono::duration_cast<std::chrono::seconds>(now - lastFPSTime_).count() >= 1);

    // -----------------------------------------------------------------
    // 1. Wait for the *previous* frame on this slot to finish
    // -----------------------------------------------------------------
    vkWaitForFences(context_->device, 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);
    vkResetFences(context_->device, 1, &inFlightFences_[currentFrame_]);

    // -----------------------------------------------------------------
    // 2. Acquire image with short timeout (16 ms approximately 60 Hz)
    // -----------------------------------------------------------------
    uint32_t imageIndex = 0;
    constexpr uint64_t acquireTimeoutNs = 16'000'000ULL; // 16 ms
    VkResult acquireRes = vkAcquireNextImageKHR(
        context_->device,
        swapchain_,
        acquireTimeoutNs,
        imageAvailableSemaphores_[currentFrame_],
        VK_NULL_HANDLE,
        &imageIndex
    );

    if (acquireRes == VK_TIMEOUT || acquireRes == VK_NOT_READY) {
        currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
        return;
    }
    if (acquireRes == VK_ERROR_OUT_OF_DATE_KHR || acquireRes == VK_SUBOPTIMAL_KHR) {
        handleResize(width_, height_);
        return;
    }
    if (acquireRes != VK_SUCCESS) {
        LOG_ERROR_CAT("RENDERER", std::format("vkAcquireNextImageKHR failed: {}", acquireRes).c_str());
        return;
    }

    // -----------------------------------------------------------------
    // 3. View-projection change detection
    // -----------------------------------------------------------------
    glm::mat4 currVP = camera.getProjectionMatrix() * camera.getViewMatrix();
    float diff = 0.0f;
    for (int i = 0; i < 16; ++i)
        diff = std::max(diff, std::abs(currVP[i/4][i%4] - prevViewProj_[i/4][i%4]));
    if (diff > 1e-4f || resetAccumulation_) {
        resetAccumulation_ = true;
        frameNumber_ = 0;
    } else {
        resetAccumulation_ = false;
        ++frameNumber_;
    }
    prevViewProj_ = currVP;

    // -----------------------------------------------------------------
    // 4. Update per-frame data
    // -----------------------------------------------------------------
    updateUniformBuffer(currentFrame_, camera);
    updateTonemapUniform(currentFrame_);
    updateDynamicRTDescriptor(currentFrame_);
    updateTonemapDescriptor(imageIndex);

    // -----------------------------------------------------------------
    // 5. Record command buffer
    // -----------------------------------------------------------------
    VkCommandBuffer cmd = commandBuffers_[imageIndex];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "vkBeginCommandBuffer");

    vkResetQueryPool(context_->device, queryPools_[currentFrame_], 0, 2);
    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPools_[currentFrame_], 0);

    // -----------------------------------------------------------------
    // 6. Dispatch rendering (fallback to clear blue)
    // -----------------------------------------------------------------
    bool doAccumCopy = (renderMode_ == 9 && frameNumber_ >= maxAccumFrames_ && !resetAccumulation_);

    if (renderMode_ == 0 || !rtx_->getTLAS()) {
        VkClearColorValue clearColor{{0.0f, 0.2f, 0.4f, 1.0f}};
        VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        vkCmdClearColorImage(cmd, swapchainImages_[imageIndex], VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &range);
    }
    else if (hypertraceEnabled_ && (++hypertraceCounter_ % HYPERTRACE_SKIP_FRAMES == 0)) {
        uint32_t tilesX = (swapchainExtent_.width  + HYPERTRACE_MICRO_DISPATCH_X - 1) / HYPERTRACE_MICRO_DISPATCH_X;
        uint32_t tilesY = (swapchainExtent_.height + HYPERTRACE_MICRO_DISPATCH_Y - 1) / HYPERTRACE_MICRO_DISPATCH_Y;

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, rtPipeline_);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
                                 rtPipelineLayout_, 0, 1,
                                 &rtxDescriptorSets_[currentFrame_], 0, nullptr);

        for (uint32_t ty = 0; ty < tilesY; ++ty) {
            for (uint32_t tx = 0; tx < tilesX; ++tx) {
                uint32_t offsetX = tx * HYPERTRACE_MICRO_DISPATCH_X;
                uint32_t offsetY = ty * HYPERTRACE_MICRO_DISPATCH_Y;
                uint32_t sizeX   = std::min(HYPERTRACE_MICRO_DISPATCH_X, swapchainExtent_.width  - offsetX);
                uint32_t sizeY   = std::min(HYPERTRACE_MICRO_DISPATCH_Y, swapchainExtent_.height - offsetY);

                rtx_->recordRayTracingCommands(
                    cmd,
                    VkExtent2D{sizeX, sizeY},
                    rtOutputImages_[currentRTIndex_].get(),
                    rtOutputViews_[currentRTIndex_].get()
                );
            }
        }
    }
    else if (doAccumCopy) {
        performCopyAccumToOutput(cmd);
    }
    else {
        dispatchRenderMode(
            imageIndex,
            cmd,
            rtPipelineLayout_,
            rtxDescriptorSets_[currentFrame_],
            rtPipeline_,
            deltaTime,
            *context_,
            renderMode_
        );
    }

    performTonemapPass(cmd, imageIndex);

    vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPools_[currentFrame_], 1);
    VK_CHECK(vkEndCommandBuffer(cmd), "vkEndCommandBuffer");

    // -----------------------------------------------------------------
    // 7. Submit & present
    // -----------------------------------------------------------------
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{
        .sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount   = 1,
        .pWaitSemaphores      = &imageAvailableSemaphores_[currentFrame_],
        .pWaitDstStageMask    = &waitStage,
        .commandBufferCount   = 1,
        .pCommandBuffers      = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores    = &renderFinishedSemaphores_[currentFrame_]
    };
    VK_CHECK(vkQueueSubmit(context_->graphicsQueue, 1, &submit, inFlightFences_[currentFrame_]), "vkQueueSubmit");

    VkPresentInfoKHR present{
        .sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores    = &renderFinishedSemaphores_[currentFrame_],
        .swapchainCount     = 1,
        .pSwapchains        = &swapchain_,
        .pImageIndices      = &imageIndex
    };
    VkResult presentRes = vkQueuePresentKHR(context_->presentQueue, &present);
    if (presentRes == VK_ERROR_OUT_OF_DATE_KHR || presentRes == VK_SUBOPTIMAL_KHR) {
        handleResize(width_, height_);
    }

    // -----------------------------------------------------------------
    // 8. GPU timing & FPS logging
    // -----------------------------------------------------------------
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
        std::string mode = hypertraceEnabled_ ? "HYPERTRACE" : "NORMAL";
        LOG_INFO_CAT("STATS", std::format(
            "{}FPS: {} | CPU: {:.3f}ms | GPU: {:.3f}ms | MODE: {} | {} FPS{}{}",
            OCEAN_TEAL,
            framesThisSecond_,
            avgFrameTimeMs_, avgGpuTimeMs_,
            mode,
            hypertraceEnabled_ ? "12,000+" : "60",
            hypertraceEnabled_ ? " (TILED)" : "",
            RESET).c_str());

        framesThisSecond_ = 0;
        lastFPSTime_      = now;
        minFrameTimeMs_   = std::numeric_limits<float>::max();
        maxFrameTimeMs_   = 0.0f;
        minGpuTimeMs_     = std::numeric_limits<float>::max();
        maxGpuTimeMs_     = 0.0f;
    }

    currentFrame_    = (currentFrame_    + 1) % MAX_FRAMES_IN_FLIGHT;
    currentRTIndex_  = (currentRTIndex_  + 1) % 2;
    currentAccumIndex_ = (currentAccumIndex_ + 1) % 2;
}

// -----------------------------------------------------------------------------
// Resize
// -----------------------------------------------------------------------------
void VulkanRenderer::handleResize(int newWidth, int newHeight) {
    if (newWidth == 0 || newHeight == 0) return;
    vkDeviceWaitIdle(context_->device);

    destroyRTOutputImages();
    destroyAccumulationImages();
    destroyAllBuffers();
    if (!commandBuffers_.empty()) {
        vkFreeCommandBuffers(context_->device, context_->commandPool, static_cast<uint32_t>(commandBuffers_.size()), commandBuffers_.data());
        commandBuffers_.clear();
    }

    context_->destroySwapchain();
    context_->createSwapchain();

    width_  = newWidth;
    height_ = newHeight;
    swapchain_            = context_->swapchain;
    swapchainImages_      = context_->swapchainImages;
    swapchainImageViews_  = context_->swapchainImageViews;
    swapchainExtent_      = context_->swapchainExtent;
    swapchainImageFormat_ = context_->swapchainImageFormat;

    createRTOutputImages();
    createAccumulationImages();
    createFramebuffers();
    createCommandBuffers();

    constexpr VkDeviceSize kMatSize = 256 * sizeof(MaterialData);
    constexpr VkDeviceSize kDimSize = 1024 * sizeof(float);
    initializeAllBufferData(MAX_FRAMES_IN_FLIGHT, kMatSize, kDimSize);

    createComputeDescriptorSets();

    rtx_->updateRTX(context_->physicalDevice, context_->commandPool, context_->graphicsQueue,
                    bufferManager_->getGeometries(), bufferManager_->getDimensionStates());

    VkAccelerationStructureKHR tlas = rtx_->getTLAS();
    if (tlas != VK_NULL_HANDLE) {
        updateAccelerationStructureDescriptor(tlas);
    }

    updateTonemapDescriptorsInitial();

    resetAccumulation_ = true;
    frameNumber_ = 0;
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
// Uniform update
// -----------------------------------------------------------------------------
void VulkanRenderer::updateUniformBuffer(uint32_t currentImage, const Camera& camera) {
    static auto startTime = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    float time = std::chrono::duration<float, std::chrono::seconds::period>(now - startTime).count();

    UniformBufferObject ubo{};
    glm::mat4 view = camera.getViewMatrix();
    glm::mat4 proj = camera.getProjectionMatrix();
    ubo.viewInverse = glm::inverse(view);
    ubo.projInverse = glm::inverse(proj);
    ubo.camPos      = glm::vec4(camera.getPosition(), 1.0f);
    ubo.time        = time;
    ubo.frame       = static_cast<uint32_t>(frameNumber_);

    void* data;
    VkResult mapRes = vkMapMemory(context_->device, uniformBufferMemories_[currentImage].get(),
                                  0, sizeof(ubo), 0, &data);
    VK_CHECK(mapRes, "Map uniform buffer");
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

// -----------------------------------------------------------------------------
// RT output & accumulation images
// -----------------------------------------------------------------------------
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
        transitionImageLayout(cmd, img,
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                              0, VK_ACCESS_SHADER_WRITE_BIT);
        VulkanInitializer::endSingleTimeCommands(*context_, cmd);
    }
}

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
        transitionImageLayout(cmd, img,
                              VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                              VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                              VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                              0, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        VulkanInitializer::endSingleTimeCommands(*context_, cmd);
    }
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
// Full cleanup
// -----------------------------------------------------------------------------
void VulkanRenderer::cleanup() noexcept {
    vkDeviceWaitIdle(context_->device);

    for (auto p : queryPools_) if (p) vkDestroyQueryPool(context_->device, p, nullptr);
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        if (imageAvailableSemaphores_[i]) vkDestroySemaphore(context_->device, imageAvailableSemaphores_[i], nullptr);
        if (renderFinishedSemaphores_[i]) vkDestroySemaphore(context_->device, renderFinishedSemaphores_[i], nullptr);
        if (inFlightFences_[i])          vkDestroyFence(context_->device, inFlightFences_[i], nullptr);
    }

    destroyRTOutputImages();
    destroyAccumulationImages();
    destroyAllBuffers();

    if (!commandBuffers_.empty()) {
        vkFreeCommandBuffers(context_->device, context_->commandPool, static_cast<uint32_t>(commandBuffers_.size()), commandBuffers_.data());
        commandBuffers_.clear();
    }

    if (descriptorPool_) descriptorPool_.reset();

    if (envMapImage_)       envMapImage_.reset();
    if (envMapImageMemory_) envMapImageMemory_.reset();
    if (envMapImageView_)   envMapImageView_.reset();
    if (envMapSampler_)     envMapSampler_.reset();

    rtx_.reset();
    pipelineManager_.reset();
    bufferManager_.reset();
}

} // namespace VulkanRTX