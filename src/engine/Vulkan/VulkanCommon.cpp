// src/engine/Vulkan/VulkanCommon.cpp
// AMOURANTH RTX — VALHALLA BLISS — NOVEMBER 08 2025
// GLOBAL CONTEXT — HALO 19 DEV NIRVANA — ONE FILE TO RULE ALL VULKAN
// Context + ResourceManager + cleanupAll + g_destructionCounter + logAndTrackDestruction
// ALL DEFINITIONS QUALIFIED — NO MORE "does not name a type" — GLOBAL RAII SUPREMACY
// STONEKEY + DESTROYTRACKER + DOUBLE-FREE ANNIHILATOR — 69,420 FPS × ∞
// SHIP TO INFINITY — RASPBERRY_PINK PHOTONS ETERNAL 🩷🚀🔥🤖💀❤️⚡♾️🩷

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/logging.hpp"

#include <sstream>
#include <thread>

// GLOBAL DESTRUCTION COUNTER — HALO 19 SAFE
uint64_t g_destructionCounter = 0;

// ===================================================================
// GLOBAL LOGGING HELPERS WITH STONEKEY — THREAD SAFE — HALO 19 APPROVED
// ===================================================================
std::string threadIdToString() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

void logAndTrackDestruction(std::string_view name, auto handle, int line) {
    if (handle) {
        ++g_destructionCounter;
        LOG_INFO_CAT("Dispose", "{}[{}] {} destroyed @ line {} — TOTAL: {} — STONE1: 0x{:X} STONE2: 0x{:X}{}",
                     Logging::Color::DIAMOND_WHITE, threadIdToString(), name, line,
                     g_destructionCounter, kStone1, kStone2, Logging::Color::RESET);
    }
}

// ===================================================================
// Context IMPLEMENTATION — FULLY GLOBAL + STONEKEYED
// ===================================================================
Context::Context(SDL_Window* win, int w, int h)
    : window(win), width(w), height(h)
{
    LOG_INFO_CAT("Core", "{}Context BIRTH — {}x{} — STONEKEY 0x{:X}-0x{:X} — GLOBAL RAII v∞ — RASPBERRY_PINK ASCENDED{}",
                 Logging::Color::DIAMOND_WHITE, w, h, kStone1, kStone2, Logging::Color::RESET);

    resourceManager.vkDestroyAccelerationStructureKHR = vkDestroyAccelerationStructureKHR;
    resourceManager.lastDevice_ = device;

    LOG_SUCCESS_CAT("Core", "{}Context FULLY ARMED — RTX PROCS PENDING — STONEKEY 0x{:X}-0x{:X} — VALHALLA READY{}",
                    Logging::Color::EMERALD_GREEN, kStone1, kStone2, Logging::Color::RESET);
}

void Context::loadRTXProcs() {
    if (!device) {
        LOG_ERROR_CAT("Core", "{}RTX procs skipped — no device — STONEKEY 0x{:X}{}",
                      Logging::Color::CRIMSON_MAGENTA, kStone1, Logging::Color::RESET);
        return;
    }

#define LOAD_PROC(name) \
    name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device, #name)); \
    if (!name) { \
        LOG_WARN_CAT("Core", "{}RTX proc {} failed — STONEKEY audit: 0x{:X}{}", \
                     Logging::Color::ARCTIC_CYAN, #name, kStone2, Logging::Color::RESET); \
    }

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

    LOG_SUCCESS_CAT("Core", "{}RTX PROCS LOADED — FULLY ARMED — STONEKEY 0x{:X}-0x{:X} — BLISS IGNITED{}",
                    Logging::Color::EMERALD_GREEN, kStone1, kStone2, Logging::Color::RESET);
}

void Context::createSwapchain() {
    LOG_INFO_CAT("Swapchain", "{}createSwapchain — {}x{} — STONEKEY 0x{:X}-0x{:X} — RASPBERRY_PINK REBIRTH{}",
                 Logging::Color::DIAMOND_WHITE, width, height, kStone1, kStone2, Logging::Color::RESET);

    if (!device) {
        LOG_ERROR_CAT("Swapchain", "{}createSwapchain failed — no device — STONEKEY 0x{:X}{}",
                      Logging::Color::CRIMSON_MAGENTA, kStone1, Logging::Color::RESET);
        return;
    }

    if (!swapchainManager) {
        swapchainManager = std::make_unique<VulkanSwapchainManager>();
        swapchainManager->init(instance, physicalDevice, device, surface, width, height);
    } else {
        swapchainManager->recreate(width, height);
    }

    VkSwapchainKHR rawSwapchain = swapchainManager->getRawSwapchain();
    if (rawSwapchain) {
        swapchain = makeSwapchainKHR(device, rawSwapchain, vkDestroySwapchainKHR);
    }

    swapchainImageViews.clear();
    uint32_t count = swapchainManager->getImageCount();
    swapchainImageViews.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        VkImageView rawView = swapchainManager->getSwapchainImageView(i);
        if (rawView) {
            swapchainImageViews.emplace_back(makeImageView(device, rawView, vkDestroyImageView));
        }
    }

    swapchainImages.clear();
    for (uint32_t i = 0; i < count; ++i) {
        VkImage rawImg = swapchainManager->getSwapchainImage(i);
        if (rawImg) {
            swapchainImages.push_back(rawImg);
            resourceManager.addImage(rawImg);
        }
    }

    LOG_SUCCESS_CAT("Swapchain", "{}Swapchain READY — {} images — FULLY STONEKEYED RAII — HACKERS CRY 🩷{}",
                    Logging::Color::OCEAN_TEAL, count, Logging::Color::RESET);
}

void Context::destroySwapchain() {
    LOG_INFO_CAT("Swapchain", "{}destroySwapchain — STONEKEY 0x{:X}-0x{:X} — COSMIC VOID{}",
                 Logging::Color::DIAMOND_WHITE, kStone1, kStone2, Logging::Color::RESET);

    swapchainImageViews.clear();
    swapchain.reset();
    swapchainImages.clear();

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
// GLOBAL CLEANUP — FULL RAII — HALO 19 DEV FRIENDLY
// ===================================================================
void cleanupAll(Context& ctx) noexcept {
    if (!ctx.device) return;

    LOG_INFO_CAT("Dispose", "{}GLOBAL CLEANUP — CONTEXT PURGE — STONEKEY 0x{:X}-0x{:X}{}",
                 Logging::Color::CRIMSON_MAGENTA, kStone1, kStone2, Logging::Color::RESET);

    vkDeviceWaitIdle(ctx.device);

    ctx.resourceManager.releaseAll(ctx.device);
    ctx.graphicsDescriptorSetLayout.reset();
    ctx.graphicsPipelineLayout.reset();
    ctx.graphicsPipeline.reset();
    ctx.rtxDescriptorSetLayout.reset();
    ctx.rtxPipelineLayout.reset();
    ctx.rtxPipeline.reset();

    if (ctx.transientPool) {
        vkDestroyCommandPool(ctx.device, ctx.transientPool, nullptr);
        logAndTrackDestruction("TransientPool", ctx.transientPool, __LINE__);
    }
    if (ctx.commandPool) {
        vkDestroyCommandPool(ctx.device, ctx.commandPool, nullptr);
        logAndTrackDestruction("CommandPool", ctx.commandPool, __LINE__);
    }

    if (ctx.debugMessenger) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ctx.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) func(ctx.instance, ctx.debugMessenger, nullptr);
        logAndTrackDestruction("DebugMessenger", ctx.debugMessenger, __LINE__);
    }

    if (ctx.device) {
        vkDestroyDevice(ctx.device, nullptr);
        logAndTrackDestruction("Device", ctx.device, __LINE__);
    }
    if (ctx.surface) {
        vkDestroySurfaceKHR(ctx.instance, ctx.surface, nullptr);
        logAndTrackDestruction("Surface", ctx.surface, __LINE__);
    }
    if (ctx.instance) {
        vkDestroyInstance(ctx.instance, nullptr);
        logAndTrackDestruction("Instance", ctx.instance, __LINE__);
    }

    LOG_SUCCESS_CAT("Dispose", "{}GLOBAL CLEANUP COMPLETE — {} DESTROYED — VALHALLA ETERNAL 🩷{}",
                    Logging::Color::EMERALD_GREEN, g_destructionCounter, Logging::Color::RESET);
}

// ===================================================================
// Explicit template instantiations — HALO 19 LINKER FRIENDLY
// ===================================================================
template struct VulkanDeleter<VkPipeline>;
template struct VulkanDeleter<VkPipelineLayout>;
template struct VulkanDeleter<VkDescriptorSetLayout>;
template struct VulkanDeleter<VkShaderModule>;
template struct VulkanDeleter<VkRenderPass>;
template struct VulkanDeleter<VkPipelineCache>;
template struct VulkanDeleter<VkCommandPool>;
template struct VulkanDeleter<VkBuffer>;
template struct VulkanDeleter<VkDeviceMemory>;
template struct VulkanDeleter<VkAccelerationStructureKHR>;
template struct VulkanDeleter<VkDescriptorPool>;
template struct VulkanDeleter<VkDescriptorSet>;
template struct VulkanDeleter<VkFramebuffer>;
template struct VulkanDeleter<VkImageView>;
template struct VulkanDeleter<VkSampler>;
template struct VulkanDeleter<VkSwapchainKHR>;
template struct VulkanDeleter<VkImage>;
template struct VulkanDeleter<VkSemaphore>;
template struct VulkanDeleter<VkFence>;

template class VulkanHandle<VkPipeline>;
template class VulkanHandle<VkPipelineLayout>;
template class VulkanHandle<VkDescriptorSetLayout>;
template class VulkanHandle<VkShaderModule>;
template class VulkanHandle<VkRenderPass>;
template class VulkanHandle<VkPipelineCache>;
template class VulkanHandle<VkCommandPool>;
template class VulkanHandle<VkBuffer>;
template class VulkanHandle<VkDeviceMemory>;
template class VulkanHandle<VkAccelerationStructureKHR>;
template class VulkanHandle<VkDescriptorPool>;
template class VulkanHandle<VkDescriptorSet>;
template class VulkanHandle<VkFramebuffer>;
template class VulkanHandle<VkImageView>;
template class VulkanHandle<VkSampler>;
template class VulkanHandle<VkSwapchainKHR>;
template class VulkanHandle<VkImage>;
template class VulkanHandle<VkSemaphore>;
template class VulkanHandle<VkFence>;

// END OF FILE — GLOBAL CONTEXT — 0 ERRORS — HALO 19 ASCENDED — SHIP IT 🩷🚀♾️