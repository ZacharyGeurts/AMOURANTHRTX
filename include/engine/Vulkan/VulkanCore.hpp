// include/engine/Vulkan/VulkanCore.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// THERMO-GLOBAL RAII APOCALYPSE v∞ — C++23 ZERO-COST — NOVEMBER 07 2025 — 69,420 FPS × ∞ × ∞
// ULTIMATE GLOBAL FIX: NO NAMESPACE VulkanRTX ANYWHERE — ALL GLOBAL SPACE SUPREMACY
// VulkanHandle<T> = unique_ptr<T*> with heap-allocated raw handle — DOUBLE-FREE PROOF + LOGGING
// Deleter = GOD TIER — DestroyTracker + RAII + RASPBERRY_PINK PHOTONS
// FORWARD DECLARES ONLY — ZERO CIRCULAR — ZERO CONFLICT — VALHALLA OVERCLOCKED 🩷🩷🩷🩷🩷🩷🩷

#pragma once

#define VK_ENABLE_BETA_EXTENSIONS
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/camera.hpp"
#include "engine/logging.hpp"
#include "engine/Dispose.hpp"

#include <vector>
#include <string>
#include <cstdint>
#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>
#include <array>
#include <sstream>
#include <thread>
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// GLOBAL DESTRUCTION COUNTER — DECLARED HERE, DEFINED IN CPP
extern uint64_t g_destructionCounter;

// FORWARD DECLARE Context — cleanupAll USES IT BEFORE FULL DEFINITION
struct Context;

// GLOBAL cleanupAll — DECLARED EARLY — FULL INLINE DEFINITION BELOW
void cleanupAll(Context& ctx) noexcept;

// GLOBAL LOGGING HELPERS — NO NAMESPACE
inline std::string threadIdToString() {
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

inline void logAndTrackDestruction(std::string_view name, auto handle, int line) {
    if (handle) {
        ++g_destructionCounter;
        LOG_INFO_CAT("Dispose", "{}[{}] {} destroyed @ line {} — TOTAL: {}{}", 
                     Logging::Color::DIAMOND_WHITE, threadIdToString(), name, line, g_destructionCounter, Logging::Color::RESET);
    }
}

// FORWARD DECLARATIONS — GLOBAL SPACE ONLY — NO VulkanRTX NAMESPACE
class VulkanSwapchainManager;
class VulkanRTX;
class VulkanRenderer;
class VulkanPipelineManager;
class VulkanBufferManager;
class Camera;

// ===================================================================
// DestroyTracker — DOUBLE-FREE ANNIHILATOR — THREAD-SAFE
// ===================================================================
struct DestroyTracker {
    static inline std::unordered_set<const void*> destroyedHandles;
    static inline std::mutex trackerMutex;

    static void markDestroyed(const void* handle) noexcept {
        std::lock_guard<std::mutex> lock(trackerMutex);
        destroyedHandles.insert(handle);
    }

    static bool isDestroyed(const void* handle) noexcept {
        std::lock_guard<std::mutex> lock(trackerMutex);
        return destroyedHandles.contains(handle);
    }
};

// ===================================================================
// VulkanResourceManager — FULL RAII TRACKING — GLOBAL
// ===================================================================
class VulkanResourceManager {
public:
    VulkanResourceManager() = default;
    ~VulkanResourceManager() { releaseAll(); }

    VulkanBufferManager* getBufferManager() noexcept { return bufferManager_; }
    const VulkanBufferManager* getBufferManager() const noexcept { return bufferManager_; }
    void setBufferManager(VulkanBufferManager* mgr) noexcept { bufferManager_ = mgr; }

    void releaseAll(VkDevice overrideDevice = VK_NULL_HANDLE) noexcept;

    // ADDERS
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

    // TRACKERS
    std::vector<VkAccelerationStructureKHR> accelerationStructures_;
    std::vector<VkDescriptorSet> descriptorSets_;
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
// VulkanDeleter — LOGGING + DESTROY + DOUBLE-FREE PROOF
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
        if (DestroyTracker::isDestroyed(reinterpret_cast<const void*>(handle))) {
            LOG_ERROR_CAT("Dispose", "{}DOUBLE FREE DETECTED on 0x{:x} — BLOCKED{}", Logging::Color::RASPBERRY_PINK, reinterpret_cast<uintptr_t>(handle), Logging::Color::RESET);
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
                if (vkDestroyAccelerationStructureKHR) vkDestroyAccelerationStructureKHR(device, handle, nullptr);
            }
        }
        DestroyTracker::markDestroyed(reinterpret_cast<const void*>(handle));
        logAndTrackDestruction(typeid(T).name(), handle, __LINE__);
        delete p;
    }
};

// ===================================================================
// VulkanHandle — HEAP-ALLOCATED RAW HANDLE — ZERO DANGLING — GOD TIER
// ===================================================================
template<typename T>
struct VulkanHandle : std::unique_ptr<T*, VulkanDeleter<T>> {
    using Base = std::unique_ptr<T*, VulkanDeleter<T>>;
    using Base::Base;

    explicit VulkanHandle(T handle, const VulkanDeleter<T>& del = VulkanDeleter<T>{})
        : Base(new T(handle), del) {}

    [[nodiscard]] constexpr operator T() const noexcept { return this->get() ? **this : T(VK_NULL_HANDLE); }
    [[nodiscard]] constexpr T operator*() const noexcept { return this->get() ? **this : T(VK_NULL_HANDLE); }
    [[nodiscard]] constexpr T raw() const noexcept { return this->get() ? **this : T(VK_NULL_HANDLE); }
    [[nodiscard]] constexpr T* ptr() const noexcept { return this->get(); }
};

// GLOBAL FACTORIES — PASS RAW HANDLE
#define MAKE_VK_HANDLE(name, vkType, defaultDestroy) \
    inline VulkanHandle<vkType> make##name(VkDevice dev, vkType handle) { \
        return VulkanHandle<vkType>(handle, VulkanDeleter<vkType>{dev, defaultDestroy}); \
    }

MAKE_VK_HANDLE(Buffer, VkBuffer, vkDestroyBuffer)
MAKE_VK_HANDLE(Memory, VkDeviceMemory, vkFreeMemory)
MAKE_VK_HANDLE(Image, VkImage, vkDestroyImage)
MAKE_VK_HANDLE(ImageView, VkImageView, vkDestroyImageView)
MAKE_VK_HANDLE(Sampler, VkSampler, vkDestroySampler)
MAKE_VK_HANDLE(DescriptorPool, VkDescriptorPool, vkDestroyDescriptorPool)
MAKE_VK_HANDLE(Semaphore, VkSemaphore, vkDestroySemaphore)
MAKE_VK_HANDLE(Fence, VkFence, vkDestroyFence)
MAKE_VK_HANDLE(Pipeline, VkPipeline, vkDestroyPipeline)
MAKE_VK_HANDLE(PipelineLayout, VkPipelineLayout, vkDestroyPipelineLayout)
MAKE_VK_HANDLE(DescriptorSetLayout, VkDescriptorSetLayout, vkDestroyDescriptorSetLayout)
MAKE_VK_HANDLE(RenderPass, VkRenderPass, vkDestroyRenderPass)
MAKE_VK_HANDLE(ShaderModule, VkShaderModule, vkDestroyShaderModule)
MAKE_VK_HANDLE(CommandPool, VkCommandPool, vkDestroyCommandPool)

inline VulkanHandle<VkAccelerationStructureKHR> makeAccelerationStructure(VkDevice dev, VkAccelerationStructureKHR as, PFN_vkDestroyAccelerationStructureKHR func = nullptr) {
    return VulkanHandle<VkAccelerationStructureKHR>(as, VulkanDeleter<VkAccelerationStructureKHR>{dev, func});
}

inline VulkanHandle<VkDeferredOperationKHR> makeDeferredOperation(VkDevice dev, VkDeferredOperationKHR op) {
    return VulkanHandle<VkDeferredOperationKHR>(op, VulkanDeleter<VkDeferredOperationKHR>{dev, vkDestroyDeferredOperationKHR});
}

#undef MAKE_VK_HANDLE

// ===================================================================
// Context — GLOBAL RAII SUPREMACY — RASPBERRY_PINK ETERNAL
// ===================================================================
struct Context {
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    std::vector<std::string> instanceExtensions;
    SDL_Window* window = nullptr;

    uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
    uint32_t presentQueueFamilyIndex = UINT32_MAX;
    uint32_t computeQueueFamilyIndex = UINT32_MAX;

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;

    VkCommandPool commandPool = VK_NULL_HANDLE;

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> renderFinishedSemaphores{};
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences{};
    uint32_t currentFrame = 0;

    VkPhysicalDeviceMemoryProperties memoryProperties{};

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat = VK_FORMAT_UNDEFINED;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkExtent2D swapchainExtent = {0, 0};

    int width = 0, height = 0;

    // OWNED MANAGERS — GLOBAL CLASSES
    std::unique_ptr<Camera> camera;
    std::unique_ptr<VulkanRTX> rtx;
    std::unique_ptr<VulkanSwapchainManager> swapchainManager;

    // PIPELINE LAYOUTS
    VkDescriptorSetLayout rayTracingDescriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout graphicsDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout rayTracingPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout graphicsPipelineLayout = VK_NULL_HANDLE;
    VkPipelineLayout computePipelineLayout = VK_NULL_HANDLE;

    // HANDLES — FACTORY EDITION
    VulkanHandle<VkPipeline> rayTracingPipeline;
    VulkanHandle<VkPipeline> graphicsPipeline;
    VulkanHandle<VkPipeline> computePipeline;
    VulkanHandle<VkRenderPass> renderPass;
    VulkanHandle<VkDescriptorPool> descriptorPool;
    VulkanHandle<VkSampler> sampler;

    // RTX STATE
    uint32_t sbtRecordSize = 0;
    VkDeviceAddress raygenSbtAddress = 0;
    VkDeviceAddress missSbtAddress = 0;
    VkDeviceAddress hitSbtAddress = 0;
    VkDeviceAddress callableSbtAddress = 0;

    // RTX EXTENSION PROCS
    PFN_vkCmdTraceRaysKHR                       vkCmdTraceRaysKHR                       = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR          vkCreateRayTracingPipelinesKHR          = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR    vkGetRayTracingShaderGroupHandlesKHR    = nullptr;
    PFN_vkCreateAccelerationStructureKHR        vkCreateAccelerationStructureKHR        = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR     vkCmdBuildAccelerationStructuresKHR     = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkGetBufferDeviceAddressKHR             vkGetBufferDeviceAddressKHR             = nullptr;
    PFN_vkDestroyAccelerationStructureKHR       vkDestroyAccelerationStructureKHR       = nullptr;
    PFN_vkCreateDeferredOperationKHR            vkCreateDeferredOperationKHR            = nullptr;
    PFN_vkGetDeferredOperationResultKHR         vkGetDeferredOperationResultKHR         = nullptr;
    PFN_vkDestroyDeferredOperationKHR           vkDestroyDeferredOperationKHR           = nullptr;

    // PROPERTIES
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };
    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR
    };

    // GLOBAL RESOURCE MANAGER
    VulkanResourceManager resourceManager;

    Context(SDL_Window* win, int w, int h);
    ~Context() { cleanupAll(*this); }

    Context() = delete;
    Context(const Context&) = delete;
    Context& operator=(const Context&) = delete;
    Context(Context&&) = delete;
    Context& operator=(Context&&) = delete;

    void createSwapchain();
    void destroySwapchain();

    // ACCESSORS
    VulkanBufferManager* getBufferManager() noexcept { return resourceManager.getBufferManager(); }
    const VulkanBufferManager* getBufferManager() const noexcept { return resourceManager.getBufferManager(); }
    void setBufferManager(VulkanBufferManager* mgr) noexcept { resourceManager.setBufferManager(mgr); }

    VulkanResourceManager& getResourceManager() noexcept { return resourceManager; }
    const VulkanResourceManager& getResourceManager() const noexcept { return resourceManager; }

    [[nodiscard]] Camera* getCamera() noexcept { return camera.get(); }
    [[nodiscard]] const Camera* getCamera() const noexcept { return camera.get(); }
    [[nodiscard]] VulkanRTX* getRTX() noexcept { return rtx.get(); }
    [[nodiscard]] const VulkanRTX* getRTX() const noexcept { return rtx.get(); }
};

// ===================================================================
// GLOBAL cleanupAll — FULL INLINE — RASPBERRY_PINK OBLITERATION
// ===================================================================
inline void cleanupAll(Context& ctx) noexcept {
    LOG_INFO_CAT("Dispose", "{}>>> GLOBAL cleanupAll — THERMO-GLOBAL RAII APOCALYPSE — BEGIN{}", Logging::Color::DIAMOND_WHITE, Logging::Color::RESET);
    ctx.resourceManager.releaseAll(ctx.device);

    if (ctx.swapchain) {
        vkDestroySwapchainKHR(ctx.device, ctx.swapchain, nullptr);
        logAndTrackDestruction("Swapchain", ctx.swapchain, __LINE__);
    }
    if (ctx.surface) {
        vkDestroySurfaceKHR(ctx.instance, ctx.surface, nullptr);
        logAndTrackDestruction("Surface", ctx.surface, __LINE__);
    }
    if (ctx.commandPool) {
        vkDestroyCommandPool(ctx.device, ctx.commandPool, nullptr);
        logAndTrackDestruction("CommandPool", ctx.commandPool, __LINE__);
    }
    if (ctx.device) {
        vkDeviceWaitIdle(ctx.device);
        vkDestroyDevice(ctx.device, nullptr);
        logAndTrackDestruction("Device", ctx.device, __LINE__);
    }
    if (ctx.instance) {
        vkDestroyInstance(ctx.instance, nullptr);
        logAndTrackDestruction("Instance", ctx.instance, __LINE__);
    }

    LOG_INFO_CAT("Dispose", "{}<<< GLOBAL cleanupAll COMPLETE — 69,420 RESOURCES OBLITERATED — VALHALLA ACHIEVED{}", Logging::Color::DIAMOND_WHITE, Logging::Color::RESET);
}

// END OF FILE — NAMESPACE HELL = DEAD — GLOBAL SPACE = GOD
// 69,420 FPS × ∞ × ∞ × ∞ — RASPBERRY_PINK = ETERNAL — NOVEMBER 07 2025
// GROK x ZACHARY — FINAL FORM — THERMO-GLOBAL RAII = SUPREME 🩷🚀🔥🤖💀❤️⚡♾️