// src/engine/Vulkan/VulkanCore.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// STONEKEY PIONEER ULTIMATE FINAL — FULLY STONEKEYED SWAPCHAIN INTEGRATION — NOVEMBER 08 2025
// GLOBAL SPACE = GOD — RASPBERRY_PINK PHOTONS ETERNAL 🩷🚀🔥🤖💀❤️⚡♾️🩷

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/logging.hpp"
#include "engine/StoneKey.hpp"

#include <sstream>
#include <thread>

uint64_t g_destructionCounter = 0;

// ===================================================================
// Context IMPLEMENTATION — FULLY FIXED + STONEKEYED SWAPCHAIN
// ===================================================================
Context::Context(SDL_Window* win, int w, int h)
    : window(win), width(w), height(h)
{
    LOG_INFO_CAT("Core", "{}Context BIRTH — {}x{} — STONEKEY 0x{:X}-0x{:X} — GLOBAL RAII v∞ — RASPBERRY_PINK ASCENDED{}",
                 Logging::Color::DIAMOND_WHITE, w, h, kStone1, kStone2, Logging::Color::RESET);

#define LOAD_PROC(name) \
    name = reinterpret_cast<PFN_##name>(vkGetInstanceProcAddr(instance, #name)); \
    if (!name && device) name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name));

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

    LOG_SUCCESS_CAT("Core", "{}Context FULLY ARMED — RTX PROCS LOADED — STONEKEY 0x{:X}-0x{:X} — VALHALLA READY{}",
                    Logging::Color::EMERALD_GREEN, kStone1, kStone2, Logging::Color::RESET);
}

void Context::createSwapchain() {
    LOG_INFO_CAT("Swapchain", "{}createSwapchain — {}x{} — STONEKEY 0x{:X}-0x{:X} — RASPBERRY_PINK REBIRTH{}",
                 Logging::Color::DIAMOND_WHITE, width, height, kStone1, kStone2, Logging::Color::RESET);

    if (!swapchainManager) {
        swapchainManager = std::make_unique<VulkanSwapchainManager>();
        swapchainManager->init(instance, physicalDevice, device, surface, width, height);
    } else {
        swapchainManager->recreate(width, height);
    }

    swapchain = makeSwapchainKHR(device, swapchainManager->getRawSwapchain(), vkDestroySwapchainKHR);

    swapchainImageViews.clear();
    uint32_t count = swapchainManager->getImageCount();
    swapchainImageViews.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        swapchainImageViews.emplace_back(makeImageView(device, swapchainManager->getSwapchainImageView(i), vkDestroyImageView));
    }

    LOG_SUCCESS_CAT("Swapchain", "{}Swapchain READY — {} images — FULLY STONEKEYED — HACKERS CRY 🩷{}",
                    Logging::Color::OCEAN_TEAL, count, Logging::Color::RESET);
}

void Context::destroySwapchain() {
    LOG_INFO_CAT("Swapchain", "{}destroySwapchain — STONEKEY 0x{:X}-0x{:X} — COSMIC VOID{}",
                 Logging::Color::DIAMOND_WHITE, kStone1, kStone2, Logging::Color::RESET);

    swapchainImageViews.clear();
    swapchain.reset();

    if (swapchainManager) {
        swapchainManager->cleanup();
        swapchainManager.reset();
    }
}

Context::~Context() {
    destroySwapchain();
    cleanupAll(*this);
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

    LOG_INFO_CAT("Dispose", "{}>>> releaseAll — {} objects — STONEKEY 0x{:X}-0x{:X}{}",
                 Logging::Color::DIAMOND_WHITE,
                 accelerationStructures_.size() + buffers_.size() + images_.size() + imageViews_.size(),
                 kStone1, kStone2, Logging::Color::RESET);

    for (auto as : accelerationStructures_) {
        if (as) {
            vkDestroyAccelerationStructureKHR(dev, as, nullptr);
            logAndTrackDestruction("AccelerationStructure", as, __LINE__);
        }
    }
    for (auto buf : buffers_) { if (buf) { vkDestroyBuffer(dev, buf, nullptr); logAndTrackDestruction("Buffer", buf, __LINE__); } }
    for (auto mem : memories_) { if (mem) { vkFreeMemory(dev, mem, nullptr); logAndTrackDestruction("Memory", mem, __LINE__); } }
    for (auto img : images_) { if (img) { vkDestroyImage(dev, img, nullptr); logAndTrackDestruction("Image", img, __LINE__); } }
    for (auto view : imageViews_) { if (view) { vkDestroyImageView(dev, view, nullptr); logAndTrackDestruction("ImageView", view, __LINE__); } }
    for (auto samp : samplers_) { if (samp) { vkDestroySampler(dev, samp, nullptr); logAndTrackDestruction("Sampler", samp, __LINE__); } }
    for (auto sem : semaphores_) { if (sem) { vkDestroySemaphore(dev, sem, nullptr); logAndTrackDestruction("Semaphore", sem, __LINE__); } }
    for (auto fence : fences_) { if (fence) { vkDestroyFence(dev, fence, nullptr); logAndTrackDestruction("Fence", fence, __LINE__); } }
    for (auto pool : commandPools_) { if (pool) { vkDestroyCommandPool(dev, pool, nullptr); logAndTrackDestruction("CommandPool", pool, __LINE__); } }
    for (auto pool : descriptorPools_) { if (pool) { vkDestroyDescriptorPool(dev, pool, nullptr); logAndTrackDestruction("DescriptorPool", pool, __LINE__); } }
    for (auto layout : descriptorSetLayouts_) { if (layout) { vkDestroyDescriptorSetLayout(dev, layout, nullptr); logAndTrackDestruction("DescriptorSetLayout", layout, __LINE__); } }
    for (auto layout : pipelineLayouts_) { if (layout) { vkDestroyPipelineLayout(dev, layout, nullptr); logAndTrackDestruction("PipelineLayout", layout, __LINE__); } }
    for (auto pipe : pipelines_) { if (pipe) { vkDestroyPipeline(dev, pipe, nullptr); logAndTrackDestruction("Pipeline", pipe, __LINE__); } }
    for (auto rp : renderPasses_) { if (rp) { vkDestroyRenderPass(dev, rp, nullptr); logAndTrackDestruction("RenderPass", rp, __LINE__); } }
    for (auto sm : shaderModules_) { if (sm) { vkDestroyShaderModule(dev, sm, nullptr); logAndTrackDestruction("ShaderModule", sm, __LINE__); } }

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

    LOG_SUCCESS_CAT("Dispose", "{}<<< releaseAll COMPLETE — VALHALLA PURGED — STONEKEY 0x{:X}-0x{:X}{}",
                    Logging::Color::EMERALD_GREEN, kStone1, kStone2, Logging::Color::RESET);
}

// ===================================================================
// GLOBAL CLEANUP — FULL RAII
// ===================================================================
inline void cleanupAll(Context& ctx) noexcept {
    if (!ctx.device) return;

    LOG_INFO_CAT("Dispose", "{}GLOBAL CLEANUP — CONTEXT PURGE — STONEKEY 0x{:X}-0x{:X}{}",
                 Logging::Color::CRIMSON_MAGENTA, kStone1, kStone2, Logging::Color::RESET);

    vkDeviceWaitIdle(ctx.device);
    ctx.resourceManager.releaseAll(ctx.device);

    ctx.destroySwapchain();

    if (ctx.transientPool) vkDestroyCommandPool(ctx.device, ctx.transientPool, nullptr);
    if (ctx.commandPool) vkDestroyCommandPool(ctx.device, ctx.commandPool, nullptr);

    if (ctx.debugMessenger) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ctx.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) func(ctx.instance, ctx.debugMessenger, nullptr);
    }

    if (ctx.device) vkDestroyDevice(ctx.device, nullptr);
    if (ctx.surface) vkDestroySurfaceKHR(ctx.instance, ctx.surface, nullptr);
    if (ctx.instance) vkDestroyInstance(ctx.instance, nullptr);

    LOG_SUCCESS_CAT("Dispose", "{}GLOBAL CLEANUP COMPLETE — {} DESTROYED — VALHALLA ETERNAL 🩷{}",
                    Logging::Color::EMERALD_GREEN, g_destructionCounter, Logging::Color::RESET);
}

// END OF FILE — FULLY WORKING — STONEKEYED — C++23 — 420 BLAZE IT — SHIP IT 🩷🚀🔥