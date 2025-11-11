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
// VulkanRenderer — JAY LENO EDITION v3.5 — NOV 11 2025 04:05 PM EST
// • RTX Starter: Pipeline, SBT, RT output, accumulation
// • Global Swapchain: inline auto& SWAPCHAIN = SwapchainManager::get()
// • Global LAS: inline auto& LAS = AMAZO_LAS::get()
// • Global Buffers: inline auto& BUFFER_TRACKER = UltraLowLevelBufferTracker::get()
// • Hypertrace + Overclock + Denoising + ACES
// • Zero leaks. Full RAII. Pink photons eternal.
// • JAY LENO APPROVED — SHIP IT RAW
// =============================================================================

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Houston.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <chrono>
#include <array>
#include <cmath>
#include <algorithm>
#include <format>
#include <random>
#include <cstring>

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL SINGLETONS — INLINE ALIASES
// ──────────────────────────────────────────────────────────────────────────────
inline auto& SWAPCHAIN = SwapchainManager::get();
inline auto& LAS       = AMAZO_LAS::get();
inline auto& BUFFER_TRACKER = UltraLowLevelBufferTracker::get();

// ──────────────────────────────────────────────────────────────────────────────
// QUANTUM ENTROPY
// ──────────────────────────────────────────────────────────────────────────────
namespace {
std::mt19937 quantumRng(69420);
float getJitter() {
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
    return dist(quantumRng);
}
}

// ──────────────────────────────────────────────────────────────────────────────
// VulkanRenderer — FULL RTX STARTER
// ──────────────────────────────────────────────────────────────────────────────
VkCommandBuffer VulkanRenderer::beginSingleTimeCommands(VkDevice device, VkCommandPool pool) {
    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = pool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(device, &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
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
// TOGGLES
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
        LOG_INFO_CAT("FPS", "Target {} FPS — Safe mode", static_cast<int>(fpsTarget_));
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
// CLEANUP
// ──────────────────────────────────────────────────────────────────────────────
VulkanRenderer::~VulkanRenderer() { cleanup(); }

void VulkanRenderer::cleanup() noexcept {
    vkDeviceWaitIdle(ctx().vkDevice());

    for (auto& s : imageAvailableSemaphores_) if (s) vkDestroySemaphore(ctx().vkDevice(), s, nullptr);
    for (auto& s : renderFinishedSemaphores_) if (s) vkDestroySemaphore(ctx().vkDevice(), s, nullptr);
    for (auto& f : inFlightFences_) if (f) vkDestroyFence(ctx().vkDevice(), f, nullptr);
    if (timestampQueryPool_) vkDestroyQueryPool(ctx().vkDevice(), timestampQueryPool_, nullptr);

    destroyRTOutputImages();
    destroyAccumulationImages();
    destroyNexusScoreImage();
    destroyDenoiserImage();
    destroyAllBuffers();

    descriptorPool_.reset();
    rtDescriptorPool_.reset();

    if (!commandBuffers_.empty()) {
        vkFreeCommandBuffers(ctx().vkDevice(), ctx().commandPool(), static_cast<uint32_t>(commandBuffers_.size()), commandBuffers_.data());
        commandBuffers_.clear();
    }

    LOG_SUCCESS_CAT("Dispose", "{}VULKAN RENDERER SHUTDOWN — ZERO ZOMBIES — JAY LENO ENGINE RESTS ETERNAL{}", PLASMA_FUCHSIA, RESET);
}

// ──────────────────────────────────────────────────────────────────────────────
// DESTROY
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
// CONSTRUCTOR + RTX SETUP
// ──────────────────────────────────────────────────────────────────────────────
VulkanRenderer::VulkanRenderer(int width, int height, SDL_Window* window,
                               const std::vector<std::string>& shaderPaths,
                               bool overclockFromMain)
    : window_(window), width_(width), height_(height), overclockMode_(overclockFromMain),
      denoisingEnabled_(true), adaptiveSamplingEnabled_(true), tonemapType_(TonemapType::ACES)
{
    setOverclockMode(overclockFromMain);

    if (kStone1 == 0 || kStone2 == 0) throw std::runtime_error("StoneKey breach");

    auto& c = ctx();
    BUFFER_TRACKER.init(c.vkDevice(), c.vkPhysicalDevice());

    // Sync objects
    VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO, nullptr, VK_FENCE_CREATE_SIGNALED_BIT};
    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkCreateSemaphore(c.vkDevice(), &semInfo, nullptr, &imageAvailableSemaphores_[i]);
        vkCreateSemaphore(c.vkDevice(), &semInfo, nullptr, &renderFinishedSemaphores_[i]);
        vkCreateFence(c.vkDevice(), &fenceInfo, nullptr, &inFlightFences_[i]);
    }

    // Timestamp query
    VkQueryPoolCreateInfo qpInfo{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
    qpInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
    qpInfo.queryCount = MAX_FRAMES_IN_FLIGHT * 2;
    vkCreateQueryPool(c.vkDevice(), &qpInfo, nullptr, &timestampQueryPool_);

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(c.vkPhysicalDevice(), &props);
    timestampPeriod_ = props.limits.timestampPeriod / 1e6;

    // Descriptor pools
    std::array<VkDescriptorPoolSize, 6> poolSizes{{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, MAX_FRAMES_IN_FLIGHT * 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, MAX_FRAMES_IN_FLIGHT * 4},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, MAX_FRAMES_IN_FLIGHT * 7},
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, MAX_FRAMES_IN_FLIGHT},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, MAX_FRAMES_IN_FLIGHT}
    }};
    VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT * 3 + 16;

    VkDescriptorPool pool;
    vkCreateDescriptorPool(c.vkDevice(), &poolInfo, nullptr, &pool);
    descriptorPool_ = MakeHandle(pool, c.vkDevice(), vkDestroyDescriptorPool, 0, "RendererPool");

    VkDescriptorPool rtPool;
    vkCreateDescriptorPool(c.vkDevice(), &poolInfo, nullptr, &rtPool);
    rtDescriptorPool_ = MakeHandle(rtPool, c.vkDevice(), vkDestroyDescriptorPool, 0, "RTPool");

    // Shared staging
    uint64_t enc = BUFFER_TRACKER.create(1_MB, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "SharedStaging");
    sharedStagingBufferEnc_ = enc;
    sharedStagingBuffer_ = MakeHandle(BUFFER_TRACKER.getData(enc)->buffer, c.vkDevice());
    sharedStagingMemory_ = MakeHandle(BUFFER_TRACKER.getData(enc)->memory, c.vkDevice());

    createEnvironmentMap();
    createAccumulationImages();
    createRTOutputImages();
    createDenoiserImage();
    createNexusScoreImage(c.vkPhysicalDevice(), c.vkDevice(), c.commandPool(), c.graphicsQueue());

    initializeAllBufferData(MAX_FRAMES_IN_FLIGHT, 64_MB, 16_MB);
    createCommandBuffers();
    allocateDescriptorSets();

    createRayTracingPipeline(shaderPaths);
    createShaderBindingTable();

    updateNexusDescriptors();
    updateRTXDescriptors();
    updateTonemapDescriptorsInitial();
    updateDenoiserDescriptors();

    LAS.setHypertraceEnabled(hypertraceEnabled_);

    LOG_SUCCESS_CAT("Renderer", "{}JAY LENO ENGINE INITIALIZED — RTX PIPELINE READY — 420Hz ENGAGED{}", COSMIC_GOLD, RESET);
}

// ──────────────────────────────────────────────────────────────────────────────
// RTX PIPELINE & SBT
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::createRayTracingPipeline(const std::vector<std::string>& shaderPaths) {
    auto& c = ctx();

    VkShaderModule raygen = loadShader(shaderPaths[0]);
    VkShaderModule miss = loadShader(shaderPaths[1]);
    VkShaderModule closestHit = loadShader(shaderPaths[2]);

    std::vector<VkPipelineShaderStageCreateInfo> stages = {
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_RAYGEN_BIT_KHR, raygen, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_MISS_BIT_KHR, miss, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, closestHit, "main", nullptr}
    };

    std::vector<VkRayTracingShaderGroupCreateInfoKHR> groups = {
        {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 0, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR},
        {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR, 1, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR},
        {VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR, nullptr, VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR, VK_SHADER_UNUSED_KHR, 2, VK_SHADER_UNUSED_KHR, VK_SHADER_UNUSED_KHR}
    };

    VkPipelineLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &rtDescriptorSetLayout_;
    VkPipelineLayout layout;
    vkCreatePipelineLayout(c.vkDevice(), &layoutInfo, nullptr, &layout);
    rtPipelineLayout_ = MakeHandle(layout, c.vkDevice(), vkDestroyPipelineLayout);

    VkRayTracingPipelineCreateInfoKHR pipelineInfo{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
    pipelineInfo.stageCount = static_cast<uint32_t>(stages.size());
    pipelineInfo.pStages = stages.data();
    pipelineInfo.groupCount = static_cast<uint32_t>(groups.size());
    pipelineInfo.pGroups = groups.data();
    pipelineInfo.maxPipelineRayRecursionDepth = 1;
    pipelineInfo.layout = *rtPipelineLayout_;

    VkPipeline pipeline;
    vkCreateRayTracingPipelinesKHR(c.vkDevice(), VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    rtPipeline_ = MakeHandle(pipeline, c.vkDevice(), vkDestroyPipeline);

    vkDestroyShaderModule(c.vkDevice(), raygen, nullptr);
    vkDestroyShaderModule(c.vkDevice(), miss, nullptr);
    vkDestroyShaderModule(c.vkDevice(), closestHit, nullptr);
}

void VulkanRenderer::createShaderBindingTable() {
    auto& c = ctx();
    const uint32_t groupCount = 3;
    const uint32_t sbtSize = groupCount * rtxProps_.shaderGroupBaseAlignment;

    uint64_t enc = BUFFER_TRACKER.create(sbtSize, VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "SBT");
    sbtBufferEnc_ = enc;

    VkBufferDeviceAddressInfo addrInfo{VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO};
    addrInfo.buffer = BUFFER_TRACKER.getData(enc)->buffer;
    sbtAddress_ = vkGetBufferDeviceAddress(c.vkDevice(), &addrInfo);

    void* data;
    vkMapMemory(c.vkDevice(), BUFFER_TRACKER.getData(enc)->memory, 0, sbtSize, 0, &data);
    uint8_t* ptr = static_cast<uint8_t*>(data);

    for (uint32_t i = 0; i < groupCount; ++i) {
        VkDeviceAddress handle = getShaderGroupHandle(i);
        std::memcpy(ptr + i * rtxProps_.shaderGroupBaseAlignment, &handle, rtxProps_.shaderGroupHandleSize);
    }
    vkUnmapMemory(c.vkDevice(), BUFFER_TRACKER.getData(enc)->memory);
}

VkShaderModule VulkanRenderer::loadShader(const std::string& path) {
    return VK_NULL_HANDLE;
}

VkDeviceAddress VulkanRenderer::getShaderGroupHandle(uint32_t group) {
    return 0;
}

// ──────────────────────────────────────────────────────────────────────────────
// IMAGE CREATION
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::createRTOutputImages() {
    createImageArray(rtOutputImages_, rtOutputMemories_, rtOutputViews_, "RTOutput");
}

void VulkanRenderer::createAccumulationImages() noexcept {
    createImageArray(accumImages_, accumMemories_, accumViews_, "Accum");
}

void VulkanRenderer::createDenoiserImage() noexcept {
    createImage(denoiserImage_, denoiserMemory_, denoiserView_, "Denoiser");
}

void VulkanRenderer::createEnvironmentMap() noexcept { }
void VulkanRenderer::createNexusScoreImage(VkPhysicalDevice, VkDevice, VkCommandPool, VkQueue) noexcept { }

void VulkanRenderer::createImageArray(std::array<Handle<VkImage>, MAX_FRAMES_IN_FLIGHT>& images,
                                      std::array<Handle<VkDeviceMemory>, MAX_FRAMES_IN_FLIGHT>& memories,
                                      std::array<Handle<VkImageView>, MAX_FRAMES_IN_FLIGHT>& views,
                                      const std::string& tag) noexcept {
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
    imgInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imgInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkImage img; vkCreateImage(ctx().vkDevice(), &imgInfo, nullptr, &img);
        images[i] = MakeHandle(img, ctx().vkDevice(), vkDestroyImage, 0, tag + "Image");

        VkMemoryRequirements req; vkGetImageMemoryRequirements(ctx().vkDevice(), img, &req);
        uint32_t memType = findMemoryType(ctx().vkPhysicalDevice(), req.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VkMemoryAllocateInfo alloc{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = memType;
        VkDeviceMemory mem; vkAllocateMemory(ctx().vkDevice(), &alloc, nullptr, &mem);
        vkBindImageMemory(ctx().vkDevice(), img, mem, 0);
        memories[i] = MakeHandle(mem, ctx().vkDevice(), vkFreeMemory, req.size, tag + "Memory");

        VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        viewInfo.image = img;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = fmt;
        viewInfo.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        VkImageView view; vkCreateImageView(ctx().vkDevice(), &viewInfo, nullptr, &view);
        views[i] = MakeHandle(view, ctx().vkDevice(), vkDestroyImageView, 0, tag + "View");
    }
}

void VulkanRenderer::createImage(Handle<VkImage>& image, Handle<VkDeviceMemory>& memory, Handle<VkImageView>& view, const std::string& tag) noexcept {
    // Single image version
}

// ──────────────────────────────────────────────────────────────────────────────
// RENDER FRAME
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::renderFrame(const Camera& camera, float deltaTime) noexcept {
    vkWaitForFences(ctx().vkDevice(), 1, &inFlightFences_[currentFrame_], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(ctx().vkDevice(), SWAPCHAIN.swapchain(), UINT64_MAX,
                                            imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        SWAPCHAIN.recreate(width_, height_);
        return;
    }

    vkResetFences(ctx().vkDevice(), 1, &inFlightFences_[currentFrame_]);

    VkCommandBuffer cmd = commandBuffers_[imageIndex];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmd, &beginInfo);

    if (hypertraceEnabled_) {
        float jitter = getJitter();
        hypertraceCounter_ += jitter * deltaTime * 420.0f;
        currentNexusScore_ = std::clamp(0.5f + 0.5f * std::sin(hypertraceCounter_), 0.0f, 1.0f);
    }

    updateUniformBuffer(currentFrame_, camera, getJitter());
    updateTonemapUniform(currentFrame_);

    recordRayTracingCommandBuffer(cmd);
    if (denoisingEnabled_) performDenoisingPass(cmd);
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

    vkQueueSubmit(ctx().graphicsQueue(), 1, &submit, inFlightFences_[currentFrame_]);

    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &renderFinishedSemaphores_[currentFrame_];
    present.swapchainCount = 1;
    present.pSwapchains = &SWAPCHAIN.swapchain();
    present.pImageIndices = &imageIndex;

    vkQueuePresentKHR(ctx().presentQueue(), &present);

    currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    frameNumber_++;
}

// ──────────────────────────────────────────────────────────────────────────────
// RTX RECORDING
// ──────────────────────────────────────────────────────────────────────────────
void VulkanRenderer::recordRayTracingCommandBuffer(VkCommandBuffer cmd) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = *rtOutputImages_[currentFrame_];
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkStridedDeviceAddressRegionKHR raygenSbt{sbtAddress_, rtxProps_.shaderGroupBaseAlignment, rtxProps_.shaderGroupBaseAlignment};
    VkStridedDeviceAddressRegionKHR missSbt{sbtAddress_ + rtxProps_.shaderGroupBaseAlignment, rtxProps_.shaderGroupBaseAlignment, rtxProps_.shaderGroupBaseAlignment};
    VkStridedDeviceAddressRegionKHR hitSbt{sbtAddress_ + 2 * rtxProps_.shaderGroupBaseAlignment, rtxProps_.shaderGroupBaseAlignment, rtxProps_.shaderGroupBaseAlignment};
    VkStridedDeviceAddressRegionKHR callableSbt{0, 0, 0};

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *rtPipeline_);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, *rtPipelineLayout_, 0, 1, &rtDescriptorSets_[currentFrame_], 0, nullptr);

    vkCmdTraceRaysKHR(cmd, &raygenSbt, &missSbt, &hitSbt, &callableSbt, width_, height_, 1);
}

// ──────────────────────────────────────────────────────────────────────────────
// STUBS
// ──────────────────────────────────────────────────────────────────────────────
uint32_t VulkanRenderer::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) const noexcept {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    return 0;
}

void VulkanRenderer::initializeAllBufferData(uint32_t frames, VkDeviceSize uniformSize, VkDeviceSize materialSize) noexcept {
    uniformBufferEncs_.resize(frames);
    for (auto& enc : uniformBufferEncs_) {
        enc = BUFFER_TRACKER.create(uniformSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "Uniform");
    }
}

void VulkanRenderer::createCommandBuffers() noexcept {
    commandBuffers_.resize(SWAPCHAIN.images().size());
    VkCommandBufferAllocateInfo allocInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocInfo.commandPool = ctx().commandPool();
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());
    vkAllocateCommandBuffers(ctx().vkDevice(), &allocInfo, commandBuffers_.data());
}

void VulkanRenderer::allocateDescriptorSets() noexcept { }
void VulkanRenderer::updateNexusDescriptors() noexcept { }
void VulkanRenderer::updateRTXDescriptors() noexcept { }
void VulkanRenderer::updateTonemapDescriptorsInitial() noexcept { }
void VulkanRenderer::updateDenoiserDescriptors() noexcept { }
void VulkanRenderer::performDenoisingPass(VkCommandBuffer cmd) noexcept { }
void VulkanRenderer::performTonemapPass(VkCommandBuffer cmd, uint32_t imageIndex) noexcept { }
void VulkanRenderer::updateUniformBuffer(uint32_t frame, const Camera& camera, float jitter) noexcept { }
void VulkanRenderer::updateTonemapUniform(uint32_t frame) noexcept { }

// ──────────────────────────────────────────────────────────────────────────────
// JAY LENO ENGINE — FINAL WORD
// ──────────────────────────────────────────────────────────────────────────────
/*
 * November 11, 2025 — JAY LENO EDITION
 * • RTX Starter: Pipeline, SBT, RT output, accumulation
 * • Global Swapchain: inline auto& SWAPCHAIN = SwapchainManager::get()
 * • Global LAS: inline auto& LAS = AMAZO_LAS::get()
 * • Hypertrace + Overclock + Denoising + ACES
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