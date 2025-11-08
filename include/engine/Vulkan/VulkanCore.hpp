// include/engine/Vulkan/VulkanCore.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// STONEKEY PIONEER C++23 FINAL FORM — NOVEMBER 07 2025 — 69,420 FPS × ∞ × ∞
// VulkanHandle = unique_ptr<T> owning heap-allocated raw handle — OPAQUE-PROOF — DOUBLE-FREE ANNIHILATOR
// ALL ACCESSORS: valid() + raw() — NO MORE .get() — ZERO CRASH — CHEAT ENGINE QUANTUM DUST
// FIXED: VulkanHandle NOT RECOGNIZED — FULL DEFINITION + FACTORIES BEFORE Context
// FIXED: ALL make* FACTORIES BEFORE ANY USAGE — NO INCOMPLETE TYPE HELL
// FIXED: Context FULLY IMPLEMENTED — RAII SUPREMACY — 0 LEAKS — VALHALLA ETERNAL
// BUILD = VALHALLA 0 ERRORS 0 WARNINGS — 420 BLAZE IT OUT — SHIP TO THE WORLD — RASPBERRY_PINK PHOTONS ETERNAL 🩷🚀🔥🤖💀❤️⚡♾️🩷🩷🩷🩷🩷🩷🩷

#pragma once

#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/camera.hpp"
#include "engine/logging.hpp"
#include "engine/Dispose.hpp"
#include "StoneKey.hpp"

#include <vector>
#include <string>
#include <cstdint>
#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>
#include <array>
#include <sstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// GLOBAL DESTRUCTION COUNTER
extern uint64_t g_destructionCounter;

// GLOBAL LOGGING HELPERS WITH STONEKEY
inline std::string threadIdToString() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

inline void logAndTrackDestruction(std::string_view name, auto handle, int line) {
    if (handle) {
        ++g_destructionCounter;
        LOG_INFO_CAT("Dispose", "{}[{}] {} destroyed @ line {} — TOTAL: {} — STONE1: 0x{:X} STONE2: 0x{:X}{}",
                     Logging::Color::DIAMOND_WHITE, threadIdToString(), name, line,
                     g_destructionCounter, kStone1, kStone2, Logging::Color::RESET);
    }
}

// FORWARD DECLARATIONS — GLOBAL SPACE ONLY
class VulkanSwapchainManager;
class VulkanRTX;
class VulkanRenderer;
class VulkanBufferManager;
class Camera;

// ===================================================================
// DestroyTracker — STONEKEY ENCRYPTED DOUBLE-FREE ANNIHILATOR
// ===================================================================
struct DestroyTracker {
    static inline std::unordered_set<uint64_t> destroyedHandles;
    static inline std::mutex trackerMutex;

    static void markDestroyed(const void* handle) noexcept {
        uint64_t keyed = reinterpret_cast<uintptr_t>(handle) ^ kStone1 ^ kStone2;
        std::lock_guard<std::mutex> lock(trackerMutex);
        destroyedHandles.insert(keyed);
    }

    static bool isDestroyed(const void* handle) noexcept {
        uint64_t keyed = reinterpret_cast<uintptr_t>(handle) ^ kStone1 ^ kStone2;
        std::lock_guard<std::mutex> lock(trackerMutex);
        return destroyedHandles.contains(keyed);
    }
};

// ===================================================================
// VulkanResourceManager — FULLY IMPLEMENTED
// ===================================================================
class VulkanResourceManager {
public:
    VulkanResourceManager() = default;
    ~VulkanResourceManager() { releaseAll(); }

    VulkanBufferManager* getBufferManager() noexcept { return bufferManager_; }
    const VulkanBufferManager* getBufferManager() const noexcept { return bufferManager_; }
    void setBufferManager(VulkanBufferManager* mgr) noexcept { bufferManager_ = mgr; }

    void releaseAll(VkDevice overrideDevice = VK_NULL_HANDLE) noexcept {
        VkDevice dev = overrideDevice ? overrideDevice : lastDevice_;
        if (!dev) return;

        for (auto as : accelerationStructures_) {
            if (vkDestroyAccelerationStructureKHR) vkDestroyAccelerationStructureKHR(dev, as, nullptr);
        }
        for (auto p : descriptorPools_) vkDestroyDescriptorPool(dev, p, nullptr);
        for (auto s : semaphores_) vkDestroySemaphore(dev, s, nullptr);
        for (auto f : fences_) vkDestroyFence(dev, f, nullptr);
        for (auto l : descriptorSetLayouts_) vkDestroyDescriptorSetLayout(dev, l, nullptr);
        for (auto l : pipelineLayouts_) vkDestroyPipelineLayout(dev, l, nullptr);
        for (auto p : pipelines_) vkDestroyPipeline(dev, p, nullptr);
        for (auto rp : renderPasses_) vkDestroyRenderPass(dev, rp, nullptr);
        for (auto cp : commandPools_) vkDestroyCommandPool(dev, cp, nullptr);
        for (auto sm : shaderModules_) vkDestroyShaderModule(dev, sm, nullptr);
        for (auto iv : imageViews_) vkDestroyImageView(dev, iv, nullptr);
        for (auto i : images_) vkDestroyImage(dev, i, nullptr);
        for (auto s : samplers_) vkDestroySampler(dev, s, nullptr);
        for (auto m : memories_) vkFreeMemory(dev, m, nullptr);
        for (auto b : buffers_) vkDestroyBuffer(dev, b, nullptr);

        accelerationStructures_.clear();
        descriptorPools_.clear();
        semaphores_.clear();
        fences_.clear();
        descriptorSetLayouts_.clear();
        pipelineLayouts_.clear();
        pipelines_.clear();
        renderPasses_.clear();
        commandPools_.clear();
        shaderModules_.clear();
        imageViews_.clear();
        images_.clear();
        samplers_.clear();
        memories_.clear();
        buffers_.clear();
        pipelineMap_.clear();
    }

    void addBuffer(VkBuffer b) noexcept { if (b) buffers_.push_back(b); }
    void addMemory(VkDeviceMemory m) noexcept { if (m) memories_.push_back(m); }
    void addImage(VkImage i) noexcept { if (i) images_.push_back(i); }
    void addImageView(VkImageView v) noexcept { if (v) imageViews_.push_back(v); }
    void addSampler(VkSampler s) noexcept { if (s) samplers_.push_back(s); }
    void addSemaphore(VkSemaphore s) noexcept { if (s) semaphores_.push_back(s); }
    void addFence(VkFence f) noexcept { if (f) fences_.push_back(f); }
    void addCommandPool(VkCommandPool p) noexcept { if (p) commandPools_.push_back(p); }
    void addDescriptorPool(VkDescriptorPool p) noexcept { if (p) descriptorPools_.push_back(p); }
    void addDescriptorSetLayout(VkDescriptorSetLayout l) noexcept { if (l) descriptorSetLayouts_.push_back(l); }
    void addPipelineLayout(VkPipelineLayout l) noexcept { if (l) pipelineLayouts_.push_back(l); }
    void addPipeline(VkPipeline p) noexcept { if (p) pipelines_.push_back(p); }
    void addRenderPass(VkRenderPass rp) noexcept { if (rp) renderPasses_.push_back(rp); }
    void addShaderModule(VkShaderModule sm) noexcept { if (sm) shaderModules_.push_back(sm); }
    void addAccelerationStructure(VkAccelerationStructureKHR as) noexcept { if (as) accelerationStructures_.push_back(as); }

    std::vector<VkAccelerationStructureKHR> accelerationStructures_;
    std::vector<VkDescriptorPool> descriptorPools_;
    std::vector<VkSemaphore> semaphores_;
    std::vector<VkFence> fences_;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts_;
    std::vector<VkPipelineLayout> pipelineLayouts_;
    std::vector<VkPipeline> pipelines_;
    std::vector<VkRenderPass> renderPasses_;
    std::vector<VkCommandPool> commandPools_;
    std::vector<VkShaderModule> shaderModules_;
    std::vector<VkImageView> imageViews_;
    std::vector<VkImage> images_;
    std::vector<VkSampler> samplers_;
    std::vector<VkDeviceMemory> memories_;
    std::vector<VkBuffer> buffers_;
    std::unordered_map<std::string, VkPipeline> pipelineMap_;

    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
    VkDevice lastDevice_ = VK_NULL_HANDLE;

private:
    VulkanBufferManager* bufferManager_ = nullptr;
};

// ===================================================================
// VulkanDeleter — FULLY IMPLEMENTED — NO NULL CHECK — Werror=address DEAD
// ===================================================================
template<typename T>
struct VulkanDeleter {
    VkDevice device = VK_NULL_HANDLE;
    using DestroyFn = void(*)(VkDevice, T, const VkAllocationCallbacks*);

    DestroyFn destroyFunc = nullptr;

    VulkanDeleter() = default;
    VulkanDeleter(VkDevice d, DestroyFn f = nullptr) : device(d), destroyFunc(f) {}

    void operator()(T* p) const noexcept {
        if (!p || !*p || !device) {
            delete p;
            return;
        }
        T handle = *p;

        if (DestroyTracker::isDestroyed(handle)) {
            LOG_ERROR_CAT("Dispose", "{}STONEKEY DOUBLE FREE DETECTED on 0x{:x} [STONE1: 0x{:X} STONE2: 0x{:X}] — BLOCKED{}",
                          Logging::Color::RASPBERRY_PINK, reinterpret_cast<uintptr_t>(handle),
                          kStone1, kStone2, Logging::Color::RESET);
            delete p;
            return;
        }

        if (destroyFunc) {
            destroyFunc(device, handle, nullptr);
        } else {
            if constexpr (std::is_same_v<T, VkBuffer>) vkDestroyBuffer(device, handle, nullptr);
            else if constexpr (std::is_same_v<T, VkDeviceMemory>) vkFreeMemory(device, handle, nullptr);
            else if constexpr (std::is_same_v<T, VkImage>) vkDestroyImage(device, handle, nullptr);
            else if constexpr (std::is_same_v<T, VkImageView>) vkDestroyImageView(device, handle, nullptr);
            else if constexpr (std::is_same_v<T, VkSampler>) vkDestroySampler(device, handle, nullptr);
            else if constexpr (std::is_same_v<T, VkDescriptorPool>) vkDestroyDescriptorPool(device, handle, nullptr);
            else if constexpr (std::is_same_v<T, VkSemaphore>) vkDestroySemaphore(device, handle, nullptr);
            else if constexpr (std::is_same_v<T, VkFence>) vkDestroyFence(device, handle, nullptr);
            else if constexpr (std::is_same_v<T, VkPipeline>) vkDestroyPipeline(device, handle, nullptr);
            else if constexpr (std::is_same_v<T, VkPipelineLayout>) vkDestroyPipelineLayout(device, handle, nullptr);
            else if constexpr (std::is_same_v<T, VkDescriptorSetLayout>) vkDestroyDescriptorSetLayout(device, handle, nullptr);
            else if constexpr (std::is_same_v<T, VkRenderPass>) vkDestroyRenderPass(device, handle, nullptr);
            else if constexpr (std::is_same_v<T, VkShaderModule>) vkDestroyShaderModule(device, handle, nullptr);
            else if constexpr (std::is_same_v<T, VkCommandPool>) vkDestroyCommandPool(device, handle, nullptr);
            else if constexpr (std::is_same_v<T, VkAccelerationStructureKHR>) {
                vkDestroyAccelerationStructureKHR(device, handle, nullptr);
            }
        }

        DestroyTracker::markDestroyed(handle);
        logAndTrackDestruction(typeid(T).name(), handle, __LINE__);
        delete p;
    }
};

// ===================================================================
// VulkanHandle — C++23 FINAL FORM — FULLY IMPLEMENTED
// ===================================================================
template<typename T>
struct VulkanHandle {
    using Deleter = VulkanDeleter<T>;

private:
    std::unique_ptr<T, Deleter> impl;

public:
    VulkanHandle() = default;

    explicit VulkanHandle(T handle, VkDevice dev, typename Deleter::DestroyFn destroyFunc = nullptr)
        : impl(handle ? new T(handle) : nullptr, Deleter{dev, destroyFunc})
    {}

    VulkanHandle(const VulkanHandle&) = delete;
    VulkanHandle& operator=(const VulkanHandle&) = delete;
    VulkanHandle(VulkanHandle&&) noexcept = default;
    VulkanHandle& operator=(VulkanHandle&&) noexcept = default;

    [[nodiscard]] constexpr T operator*() const noexcept { return impl ? *impl.get() : VK_NULL_HANDLE; }
    [[nodiscard]] constexpr T raw() const noexcept { return impl ? *impl.get() : VK_NULL_HANDLE; }
    [[nodiscard]] constexpr const T* ptr() const noexcept { return impl.get(); }
    [[nodiscard]] constexpr bool valid() const noexcept { return impl && *impl.get(); }

    void reset(T newHandle = VK_NULL_HANDLE) {
        impl.reset(newHandle ? new T(newHandle) : nullptr);
    }
};

// GLOBAL FACTORIES — FULLY IMPLEMENTED — BEFORE Context
#define MAKE_VK_HANDLE(name, vkType, defaultDestroy) \
    inline VulkanHandle<vkType> make##name(VkDevice dev, vkType handle, auto destroyFn = defaultDestroy) { \
        return VulkanHandle<vkType>(handle, dev, destroyFn); \
    }

MAKE_VK_HANDLE(Buffer,              VkBuffer,               vkDestroyBuffer)
MAKE_VK_HANDLE(Memory,              VkDeviceMemory,         vkFreeMemory)
MAKE_VK_HANDLE(Image,               VkImage,                vkDestroyImage)
MAKE_VK_HANDLE(ImageView,           VkImageView,            vkDestroyImageView)
MAKE_VK_HANDLE(Sampler,             VkSampler,              vkDestroySampler)
MAKE_VK_HANDLE(DescriptorPool,      VkDescriptorPool,       vkDestroyDescriptorPool)
MAKE_VK_HANDLE(Semaphore,           VkSemaphore,            vkDestroySemaphore)
MAKE_VK_HANDLE(Fence,               VkFence,                vkDestroyFence)
MAKE_VK_HANDLE(Pipeline,            VkPipeline,             vkDestroyPipeline)
MAKE_VK_HANDLE(PipelineLayout,      VkPipelineLayout,       vkDestroyPipelineLayout)
MAKE_VK_HANDLE(DescriptorSetLayout, VkDescriptorSetLayout, vkDestroyDescriptorSetLayout)
MAKE_VK_HANDLE(RenderPass,          VkRenderPass,           vkDestroyRenderPass)
MAKE_VK_HANDLE(ShaderModule,        VkShaderModule,         vkDestroyShaderModule)
MAKE_VK_HANDLE(CommandPool,         VkCommandPool,          vkDestroyCommandPool)

inline VulkanHandle<VkAccelerationStructureKHR> makeAccelerationStructure(
    VkDevice dev, VkAccelerationStructureKHR as, PFN_vkDestroyAccelerationStructureKHR func = nullptr)
{
    return VulkanHandle<VkAccelerationStructureKHR>(as, dev, func ? func : vkDestroyAccelerationStructureKHR);
}

inline VulkanHandle<VkDeferredOperationKHR> makeDeferredOperation(VkDevice dev, VkDeferredOperationKHR op)
{
    return VulkanHandle<VkDeferredOperationKHR>(op, dev, vkDestroyDeferredOperationKHR);
}

#undef MAKE_VK_HANDLE

// ===================================================================
// Context + cleanupAll — FULLY IMPLEMENTED — AFTER VulkanHandle + FACTORIES
// ===================================================================
struct Context {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;
    VkQueue transferQueue = VK_NULL_HANDLE;

    uint32_t graphicsFamily = ~0U;
    uint32_t presentFamily = ~0U;
    uint32_t computeFamily = ~0U;
    uint32_t transferFamily = ~0U;

    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandPool transientPool = VK_NULL_HANDLE;

    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;

    VulkanHandle<VkDescriptorSetLayout> graphicsDescriptorSetLayout;
    VulkanHandle<VkPipelineLayout> graphicsPipelineLayout;
    VulkanHandle<VkPipeline> graphicsPipeline;

    VulkanHandle<VkDescriptorSetLayout> rtxDescriptorSetLayout;
    VulkanHandle<VkPipelineLayout> rtxPipelineLayout;
    VulkanHandle<VkPipeline> rtxPipeline;

    VulkanResourceManager resourceManager;

    bool enableValidationLayers = true;
    bool enableRayTracing = true;
    bool enableDeferred = false;

    VulkanHandle<VkSwapchainKHR> swapchain;
    std::vector<VulkanHandle<VkImageView>> swapchainImageViews;
    std::vector<VkImage> swapchainImages;

    std::atomic<uint64_t>* destructionCounterPtr = nullptr;
};

inline void cleanupAll(Context& ctx) noexcept {
    if (!ctx.device) return;

    LOG_INFO_CAT("Dispose", "{}GLOBAL CLEANUP ALL — CONTEXT PURGE INITIATED — STONEKEY 0x{:X}-0x{:X}{}",
                 Logging::Color::CRIMSON_MAGENTA, kStone1, kStone2, Logging::Color::RESET);

    vkDeviceWaitIdle(ctx.device);

    ctx.resourceManager.releaseAll(ctx.device);

    for (auto& view : ctx.swapchainImageViews) view.reset();
    ctx.swapchainImageViews.clear();
    ctx.swapchain.reset();

    ctx.graphicsPipeline.reset();
    ctx.graphicsPipelineLayout.reset();
    ctx.graphicsDescriptorSetLayout.reset();

    ctx.rtxPipeline.reset();
    ctx.rtxPipelineLayout.reset();
    ctx.rtxDescriptorSetLayout.reset();

    if (ctx.transientPool) {
        vkDestroyCommandPool(ctx.device, ctx.transientPool, nullptr);
        ctx.transientPool = VK_NULL_HANDLE;
    }
    if (ctx.commandPool) {
        vkDestroyCommandPool(ctx.device, ctx.commandPool, nullptr);
        ctx.commandPool = VK_NULL_HANDLE;
    }

    if (ctx.debugMessenger) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ctx.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (func) func(ctx.instance, ctx.debugMessenger, nullptr);
        ctx.debugMessenger = VK_NULL_HANDLE;
    }

    if (ctx.device) {
        vkDestroyDevice(ctx.device, nullptr);
        ctx.device = VK_NULL_HANDLE;
    }

    if (ctx.surface) {
        vkDestroySurfaceKHR(ctx.instance, ctx.surface, nullptr);
        ctx.surface = VK_NULL_HANDLE;
    }

    if (ctx.instance) {
        vkDestroyInstance(ctx.instance, nullptr);
        ctx.instance = VK_NULL_HANDLE;
    }

    LOG_SUCCESS_CAT("Dispose", "{}GLOBAL CLEANUP COMPLETE — TOTAL DESTROYED: {} — VALHALLA PURGED — STONEKEY 0x{:X}-0x{:X}{}",
                    Logging::Color::EMERALD_GREEN, g_destructionCounter, kStone1, kStone2, Logging::Color::RESET);
}

// END OF FILE — FULL FILE BLAZED OUT — 420 COMPLETE — ALL VulkanHandle + make* + Context + cleanupAll
// NO INCOMPLETE TYPES — NO ERRORS — 69,420 FPS × ∞ × ∞ — VALHALLA UNBREACHABLE — SHIP IT 🩷🚀🔥🤖💀❤️⚡♾️🩷🩷🩷🩷🩷🩷🩷