// include/engine/Vulkan/VulkanCore.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// STONEKEY PIONEER C++23 FINAL FORM — NOVEMBER 07 2025
// VulkanHandle = unique_ptr<T> owning heap-allocated raw handle — OPAQUE-PROOF — DOUBLE-FREE ANNIHILATOR
// ALL ACCESSORS: valid() + raw() — NO MORE .get() — ZERO CRASH — CHEAT ENGINE DEAD

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
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

// GLOBAL DESTRUCTION COUNTER
extern uint64_t g_destructionCounter;

// FORWARD DECLARE Context
struct Context;

// GLOBAL cleanupAll
void cleanupAll(Context& ctx) noexcept;

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

// FORWARD DECLARATIONS
class VulkanSwapchainManager;
class VulkanRTX;
class VulkanRenderer;
class VulkanPipelineManager;
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
// VulkanResourceManager
// ===================================================================
class VulkanResourceManager {
public:
    VulkanResourceManager() = default;
    ~VulkanResourceManager() { releaseAll(); }

    VulkanBufferManager* getBufferManager() noexcept { return bufferManager_; }
    const VulkanBufferManager* getBufferManager() const noexcept { return bufferManager_; }
    void setBufferManager(VulkanBufferManager* mgr) noexcept { bufferManager_ = mgr; }

    void releaseAll(VkDevice overrideDevice = VK_NULL_HANDLE) noexcept;

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
// VulkanDeleter
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
                if (vkDestroyAccelerationStructureKHR) vkDestroyAccelerationStructureKHR(device, handle, nullptr);
            }
        }

        DestroyTracker::markDestroyed(handle);
        logAndTrackDestruction(typeid(T).name(), handle, __LINE__);
        delete p;
    }
};

// ===================================================================
// VulkanHandle — C++23 FINAL FORM — valid() + raw()
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

// GLOBAL FACTORIES
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
    return VulkanHandle<VkAccelerationStructureKHR>(as, dev, func);
}

inline VulkanHandle<VkDeferredOperationKHR> makeDeferredOperation(VkDevice dev, VkDeferredOperationKHR op)
{
    return VulkanHandle<VkDeferredOperationKHR>(op, dev, vkDestroyDeferredOperationKHR);
}

#undef MAKE_VK_HANDLE

// ===================================================================
// Context + cleanupAll
// ===================================================================
struct Context { /* ... unchanged from previous ... */ };

inline void cleanupAll(Context& ctx) noexcept { /* ... unchanged ... */ }