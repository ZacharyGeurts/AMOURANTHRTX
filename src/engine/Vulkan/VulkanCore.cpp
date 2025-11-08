// src/engine/Vulkan/VulkanCore.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// STONEKEY PIONEER FINAL FORM — FULL IMPLEMENTATION — NOVEMBER 07 2025
// GLOBAL SPACE = GOD — NAMESPACE HELL = DEAD FOREVER

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/logging.hpp"
#include "engine/StoneKey.hpp"

#include <sstream>
#include <thread>

// GLOBAL DESTRUCTION COUNTER
uint64_t g_destructionCounter = 0;

// ===================================================================
// Context IMPLEMENTATION
// ===================================================================
Context::Context(SDL_Window* win, int w, int h)
    : window(win), width(w), height(h)
{
    LOG_INFO_CAT("Core", "{}Context BIRTH — {}x{} — STONEKEY 0x{:X}-0x{:X} — GLOBAL RAII v∞ — RASPBERRY_PINK ASCENDED{}",
                 Logging::Color::DIAMOND_WHITE, w, h, kStone1, kStone2, Logging::Color::RESET);

    // Load RTX procs (safe even if device not created yet — will be reloaded later)
#define LOAD_PROC(name) \
    name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(instance, #name)); \
    if (!name) name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name));
    LOAD_PROC(vkCmdTraceRaysKHR);
    LOAD_PROC(vkCreateRayTracingPipelinesKHR);
    LOAD_PROC(vkGetRayTracingShaderGroupHandlesKHR);
    LOAD_PROC(vkCreateAccelerationStructureKHR);
    LOAD_PROC(vkGetAccelerationStructureBuildSizesKHR);
    LOAD_PROC(vkCmdBuildAccelerationStructuresKHR);
    LOAD_PROC(vkGetAccelerationStructureDeviceAddressKHR);
    LOAD_PROC(vkGetBufferDeviceAddressKHR);
    LOAD_PROC(vkDestroyAccelerationStructureKHR);
    LOAD_PROC(vkCreateDeferredOperationKHR);
    LOAD_PROC(vkGetDeferredOperationResultKHR);
    LOAD_PROC(vkDestroyDeferredOperationKHR);
#undef LOAD_PROC

    resourceManager.vkDestroyAccelerationStructureKHR = vkDestroyAccelerationStructureKHR;
    resourceManager.lastDevice_ = device;

    LOG_SUCCESS_CAT("Core", "{}Context FULLY ARMED — STONEKEY 0x{:X}-0x{:X} — VALHALLA READY{}",
                    Logging::Color::EMERALD_GREEN, kStone1, kStone2, Logging::Color::RESET);
}

void Context::createSwapchain() {
    LOG_INFO_CAT("Swapchain", "{}createSwapchain — STONEKEY 0x{:X}-0x{:X} — RASPBERRY_PINK REBIRTH{}",
                 Logging::Color::DIAMOND_WHITE, kStone1, kStone2, Logging::Color::RESET);
    if (swapchainManager) {
        swapchainManager->recreateSwapchain(width, height);
    }
}

void Context::destroySwapchain() {
    LOG_INFO_CAT("Swapchain", "{}destroySwapchain — STONEKEY 0x{:X}-0x{:X} — COSMIC VOID{}",
                 Logging::Color::DIAMOND_WHITE, kStone1, kStone2, Logging::Color::RESET);
    if (swapchainManager) {
        swapchainManager->cleanupSwapchain();
    }
}

// ===================================================================
// VulkanResourceManager::releaseAll — FULL STONEKEY OBLITERATION
// ===================================================================
void VulkanResourceManager::releaseAll(VkDevice overrideDevice) noexcept {
    VkDevice dev = overrideDevice ? overrideDevice : lastDevice_;
    if (!dev) {
        LOG_WARNING_CAT("Dispose", "{}NO DEVICE — SKIP RELEASE — STONEKEY 0x{:X}-0x{:X}{}",
                        Logging::Color::ARCTIC_CYAN, kStone1, kStone2, Logging::Color::RESET);
        return;
    }

    LOG_INFO_CAT("Dispose", "{}>>> VulkanResourceManager::releaseAll — STONEKEY 0x{:X}-0x{:X} — {} objects pending{}",
                 Logging::Color::DIAMOND_WHITE, kStone1, kStone2,
                 accelerationStructures_.size() + buffers_.size() + memories_.size() + images_.size() +
                 imageViews_.size() + samplers_.size() + semaphores_.size() + fences_.size(),
                 Logging::Color::RESET);

    for (auto as : accelerationStructures_) {
        if (as && vkDestroyAccelerationStructureKHR) {
            vkDestroyAccelerationStructureKHR(dev, as, nullptr);
            logAndTrackDestruction("AccelerationStructure", as, __LINE__);
        }
    }

    for (auto buf : buffers_) { if (buf) { vkDestroyBuffer(dev, buf, nullptr); logAndTrackDestruction("Buffer", buf, __LINE__); }}
    for (auto mem : memories_) { if (mem) { vkFreeMemory(dev, mem, nullptr); logAndTrackDestruction("Memory", mem, __LINE__); }}
    for (auto img : images_) { if (img) { vkDestroyImage(dev, img, nullptr); logAndTrackDestruction("Image", img, __LINE__); }}
    for (auto view : imageViews_) { if (view) { vkDestroyImageView(dev, view, nullptr); logAndTrackDestruction("ImageView", view, __LINE__); }}
    for (auto samp : samplers_) { if (samp) { vkDestroySampler(dev, samp, nullptr); logAndTrackDestruction("Sampler", samp, __LINE__); }}
    for (auto sem : semaphores_) { if (sem) { vkDestroySemaphore(dev, sem, nullptr); logAndTrackDestruction("Semaphore", sem, __LINE__); }}
    for (auto fence : fences_) { if (fence) { vkDestroyFence(dev, fence, nullptr); logAndTrackDestruction("Fence", fence, __LINE__); }}
    for (auto pool : commandPools_) { if (pool) { vkDestroyCommandPool(dev, pool, nullptr); logAndTrackDestruction("CommandPool", pool, __LINE__); }}
    for (auto pool : descriptorPools_) { if (pool) { vkDestroyDescriptorPool(dev, pool, nullptr); logAndTrackDestruction("DescriptorPool", pool, __LINE__); }}
    for (auto layout : descriptorSetLayouts_) { if (layout) { vkDestroyDescriptorSetLayout(dev, layout, nullptr); logAndTrackDestruction("DescriptorSetLayout", layout, __LINE__); }}
    for (auto layout : pipelineLayouts_) { if (layout) { vkDestroyPipelineLayout(dev, layout, nullptr); logAndTrackDestruction("PipelineLayout", layout, __LINE__); }}
    for (auto pipe : pipelines_) { if (pipe) { vkDestroyPipeline(dev, pipe, nullptr); logAndTrackDestruction("Pipeline", pipe, __LINE__); }}
    for (auto rp : renderPasses_) { if (rp) { vkDestroyRenderPass(dev, rp, nullptr); logAndTrackDestruction("RenderPass", rp, __LINE__); }}
    for (auto sm : shaderModules_) { if (sm) { vkDestroyShaderModule(dev, sm, nullptr); logAndTrackDestruction("ShaderModule", sm, __LINE__); }}

    accelerationStructures_.clear();
    buffers_.clear();
    memories_.clear();
    images_.clear();
    imageViews_.clear();
    samplers_.clear();
    semaphores_.clear();
    fences_.clear();
    commandPools_.clear();
    descriptorPools_.clear();
    descriptorSetLayouts_.clear();
    pipelineLayouts_.clear();
    pipelines_.clear();
    renderPasses_.clear();
    shaderModules_.clear();
    pipelineMap_.clear();

    LOG_INFO_CAT("Dispose", "{}<<< VulkanResourceManager::releaseAll COMPLETE — STONEKEY 0x{:X}-0x{:X} — VALHALLA PURGED{}",
                 Logging::Color::DIAMOND_WHITE, kStone1, kStone2, Logging::Color::RESET);
}