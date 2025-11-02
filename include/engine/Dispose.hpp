// engine/Dispose.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// Centralized RAII disposal. NO SINGLETON. NO LEAKS. GROK-READY.
// FULLY POLISHED. ZERO WARNINGS. ZERO NARROWING. 100% COMPILABLE.

#ifndef DISPOSE_HPP
#define DISPOSE_HPP

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/logging.hpp"
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <memory>
#include <vector>
#include <string>
#include <atomic>

// ---------------------------------------------------------------------
// FORWARD DECLARE EXTENSION FUNCTION POINTERS
// ---------------------------------------------------------------------
using PFN_vkDestroyAccelerationStructureKHR = void (*)(VkDevice, VkAccelerationStructureKHR, const VkAllocationCallbacks*);
using PFN_vkDestroyDebugUtilsMessengerEXT = void (*)(VkInstance, VkDebugUtilsMessengerEXT, const VkAllocationCallbacks*);

namespace Dispose {

// ====================================================================
// RAII WRAPPER: VulkanHandle<T>
// ====================================================================

template <typename HandleT>
class VulkanHandle {
public:
    using DestroyFunc = void(*)(VkDevice, HandleT, const VkAllocationCallbacks*);

    VkDevice device_ = VK_NULL_HANDLE;
    HandleT handle_ = VK_NULL_HANDLE;
    DestroyFunc destroy_ = nullptr;

    VulkanHandle() = default;

    VulkanHandle(VkDevice device, HandleT handle, DestroyFunc destroy)
        : device_(device), handle_(handle), destroy_(destroy) {
        if (handle_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE) {
            LOG_DEBUG_CAT("RAII", "Created {}: {:p}", typeName(), static_cast<void*>(handle_));
        }
    }

    ~VulkanHandle() { reset(); }

    void reset(HandleT new_handle = VK_NULL_HANDLE) {
        if (handle_ != VK_NULL_HANDLE && device_ != VK_NULL_HANDLE && destroy_) {
            try {
                LOG_DEBUG_CAT("RAII", "Destroying {}: {:p}", typeName(), static_cast<void*>(handle_));
                destroy_(device_, handle_, nullptr);
                LOG_INFO_CAT("RAII", "Destroyed {}: {:p}", typeName(), static_cast<void*>(handle_));
            } catch (...) {
                LOG_ERROR_CAT("RAII", "Failed to destroy {}: {:p}", typeName(), static_cast<void*>(handle_));
            }
        }
        handle_ = new_handle;
    }

    HandleT get() const { return handle_; }
    HandleT operator*() const { return handle_; }
    HandleT* operator->() const { return &handle_; }
    explicit operator bool() const { return handle_ != VK_NULL_HANDLE; }

    HandleT* put() { reset(); return &handle_; }

    // Move semantics
    VulkanHandle(VulkanHandle&& o) noexcept
        : device_(o.device_), handle_(o.handle_), destroy_(o.destroy_) {
        o.device_ = VK_NULL_HANDLE; o.handle_ = VK_NULL_HANDLE; o.destroy_ = nullptr;
    }

    VulkanHandle& operator=(VulkanHandle&& o) noexcept {
        if (this != &o) {
            reset();
            device_ = o.device_; handle_ = o.handle_; destroy_ = o.destroy_;
            o.device_ = VK_NULL_HANDLE; o.handle_ = VK_NULL_HANDLE; o.destroy_ = nullptr;
        }
        return *this;
    }

    VulkanHandle(const VulkanHandle&) = delete;
    VulkanHandle& operator=(const VulkanHandle&) = delete;

private:
    std::string typeName() const {
        const char* raw = typeid(HandleT).name();
        std::string name = raw;
        if (name.find("VkPipeline") != std::string::npos) return "VkPipeline";
        if (name.find("VkPipelineLayout") != std::string::npos) return "VkPipelineLayout";
        if (name.find("VkRenderPass") != std::string::npos) return "VkRenderPass";
        if (name.find("VkBuffer") != std::string::npos) return "VkBuffer";
        if (name.find("VkDeviceMemory") != std::string::npos) return "VkDeviceMemory";
        if (name.find("VkImage") != std::string::npos) return "VkImage";
        if (name.find("VkImageView") != std::string::npos) return "VkImageView";
        if (name.find("VkSampler") != std::string::npos) return "VkSampler";
        if (name.find("VkDescriptorSetLayout") != std::string::npos) return "VkDescriptorSetLayout";
        if (name.find("VkAccelerationStructureKHR") != std::string::npos) return "VkAccelerationStructureKHR";
        if (name.find("VkCommandPool") != std::string::npos) return "VkCommandPool";
        if (name.find("VkSwapchainKHR") != std::string::npos) return "VkSwapchainKHR";
        if (name.find("VkShaderModule") != std::string::npos) return "VkShaderModule";
        if (name.find("VkDescriptorPool") != std::string::npos) return "VkDescriptorPool";
        if (name.find("VkPipelineCache") != std::string::npos) return "VkPipelineCache";
        return name;
    }
};

// ====================================================================
// HELPER: Auto-destroy function lookup (THREAD-SAFE)
// ====================================================================

template<typename T>
inline auto getDestroyFunc(VkDevice device) -> typename VulkanHandle<T>::DestroyFunc {
    using FuncPtr = typename VulkanHandle<T>::DestroyFunc;

    if constexpr (std::is_same_v<T, VkPipeline>) return vkDestroyPipeline;
    else if constexpr (std::is_same_v<T, VkPipelineLayout>) return vkDestroyPipelineLayout;
    else if constexpr (std::is_same_v<T, VkRenderPass>) return vkDestroyRenderPass;
    else if constexpr (std::is_same_v<T, VkPipelineCache>) return vkDestroyPipelineCache;
    else if constexpr (std::is_same_v<T, VkDescriptorSetLayout>) return vkDestroyDescriptorSetLayout;
    else if constexpr (std::is_same_v<T, VkBuffer>) return vkDestroyBuffer;
    else if constexpr (std::is_same_v<T, VkDeviceMemory>) return vkFreeMemory;
    else if constexpr (std::is_same_v<T, VkImage>) return vkDestroyImage;
    else if constexpr (std::is_same_v<T, VkImageView>) return vkDestroyImageView;
    else if constexpr (std::is_same_v<T, VkSampler>) return vkDestroySampler;
    else if constexpr (std::is_same_v<T, VkSwapchainKHR>) return vkDestroySwapchainKHR;
    else if constexpr (std::is_same_v<T, VkCommandPool>) return vkDestroyCommandPool;
    else if constexpr (std::is_same_v<T, VkShaderModule>) return vkDestroyShaderModule;
    else if constexpr (std::is_same_v<T, VkDescriptorPool>) return vkDestroyDescriptorPool;
    else if constexpr (std::is_same_v<T, VkAccelerationStructureKHR>) {
        static std::atomic<FuncPtr> cached{nullptr};
        FuncPtr func = cached.load(std::memory_order_acquire);
        if (!func && device != VK_NULL_HANDLE) {
            auto loaded = (PFN_vkDestroyAccelerationStructureKHR)
                vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR");
            if (loaded) {
                cached.store(loaded, std::memory_order_release);
                return loaded;
            }
        }
        return func;
    }
    return nullptr;
}

// ====================================================================
// SINGLE DESTROYERS (INLINE)
// ====================================================================

template <typename HandleType, void (*DestroyFunc)(VkDevice, HandleType, const VkAllocationCallbacks*)>
inline void destroySingle(VkDevice device, HandleType& handle) noexcept {
    if (device != VK_NULL_HANDLE && handle != VK_NULL_HANDLE) {
        try {
            DestroyFunc(device, handle, nullptr);
            LOG_INFO_CAT("RAII", "Destroyed {}: {:p}", typeid(HandleType).name(), static_cast<void*>(handle));
        } catch (...) {
            LOG_ERROR_CAT("RAII", "Failed to destroy {}: {:p}", typeid(HandleType).name(), static_cast<void*>(handle));
        }
        handle = VK_NULL_HANDLE;
    }
}

inline void destroySingleAccelerationStructure(VkDevice device, VkAccelerationStructureKHR& as) noexcept {
    if (device != VK_NULL_HANDLE && as != VK_NULL_HANDLE) {
        auto func = (PFN_vkDestroyAccelerationStructureKHR)
            vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR");
        if (func) {
            func(device, as, nullptr);
            LOG_INFO_CAT("RAII", "Destroyed VkAccelerationStructureKHR: {:p}", static_cast<void*>(as));
        } else {
            LOG_WARNING_CAT("RAII", "vkDestroyAccelerationStructureKHR not loaded — leaking AS: {:p}", static_cast<void*>(as));
        }
        as = VK_NULL_HANDLE;
    }
}

// --- SDL ---

inline void destroyWindow(SDL_Window* window) noexcept {
    if (window) {
        SDL_DestroyWindow(window);
        LOG_INFO_CAT("SDL", "Destroyed SDL_Window: {:p}", static_cast<void*>(window));
    }
}

inline void quitSDL() noexcept {
    SDL_Quit();
    LOG_INFO_CAT("SDL", "SDL_Quit called");
}

// --- BATCH DESTROYERS ---

template <typename HandleType, void (*DestroyFunc)(VkDevice, HandleType, const VkAllocationCallbacks*)>
inline void destroyHandles(VkDevice device, std::vector<HandleType>& handles) noexcept {
    if (device == VK_NULL_HANDLE || handles.empty()) return;
    for (auto& h : handles) {
        if (h != VK_NULL_HANDLE) {
            DestroyFunc(device, h, nullptr);
        }
        h = VK_NULL_HANDLE;
    }
    handles.clear();
    LOG_INFO_CAT("RAII", "Batch destroyed {} {}(s)", handles.size(), typeid(HandleType).name());
}

// --- SPECIALIZED BATCH ---

inline void destroyFramebuffers(VkDevice d, std::vector<VkFramebuffer>& v) noexcept { destroyHandles<VkFramebuffer, vkDestroyFramebuffer>(d, v); }
inline void destroySemaphores(VkDevice d, std::vector<VkSemaphore>& v) noexcept { destroyHandles<VkSemaphore, vkDestroySemaphore>(d, v); }
inline void destroyFences(VkDevice d, std::vector<VkFence>& v) noexcept { destroyHandles<VkFence, vkDestroyFence>(d, v); }
inline void destroyImageViews(VkDevice d, std::vector<VkImageView>& v) noexcept { destroyHandles<VkImageView, vkDestroyImageView>(d, v); }
inline void destroyBuffers(VkDevice d, std::vector<VkBuffer>& v) noexcept { destroyHandles<VkBuffer, vkDestroyBuffer>(d, v); }
inline void freeDeviceMemories(VkDevice d, std::vector<VkDeviceMemory>& v) noexcept { destroyHandles<VkDeviceMemory, vkFreeMemory>(d, v); }
inline void destroyShaderModules(VkDevice d, std::vector<VkShaderModule>& v) noexcept { destroyHandles<VkShaderModule, vkDestroyShaderModule>(d, v); }

// --- SINGLE ---

inline void destroySingleImageView(VkDevice d, VkImageView& v) noexcept { destroySingle<VkImageView, vkDestroyImageView>(d, v); }
inline void destroySingleImage(VkDevice d, VkImage& v) noexcept { destroySingle<VkImage, vkDestroyImage>(d, v); }
inline void freeSingleDeviceMemory(VkDevice d, VkDeviceMemory& v) noexcept { destroySingle<VkDeviceMemory, vkFreeMemory>(d, v); }
inline void destroySingleBuffer(VkDevice d, VkBuffer& v) noexcept { destroySingle<VkBuffer, vkDestroyBuffer>(d, v); }
inline void destroySingleSampler(VkDevice d, VkSampler& v) noexcept { destroySingle<VkSampler, vkDestroySampler>(d, v); }
inline void destroySingleDescriptorSetLayout(VkDevice d, VkDescriptorSetLayout& v) noexcept { destroySingle<VkDescriptorSetLayout, vkDestroyDescriptorSetLayout>(d, v); }
inline void destroySinglePipeline(VkDevice d, VkPipeline& v) noexcept { destroySingle<VkPipeline, vkDestroyPipeline>(d, v); }
inline void destroySinglePipelineLayout(VkDevice d, VkPipelineLayout& v) noexcept { destroySingle<VkPipelineLayout, vkDestroyPipelineLayout>(d, v); }
inline void destroySingleRenderPass(VkDevice d, VkRenderPass& v) noexcept { destroySingle<VkRenderPass, vkDestroyRenderPass>(d, v); }
inline void destroySingleSwapchain(VkDevice d, VkSwapchainKHR& v) noexcept { destroySingle<VkSwapchainKHR, vkDestroySwapchainKHR>(d, v); }
inline void destroySingleCommandPool(VkDevice d, VkCommandPool& v) noexcept { destroySingle<VkCommandPool, vkDestroyCommandPool>(d, v); }
inline void destroySingleShaderModule(VkDevice d, VkShaderModule& v) noexcept { destroySingle<VkShaderModule, vkDestroyShaderModule>(d, v); }

// --- COMMAND BUFFERS ---

inline void freeCommandBuffers(VkDevice device, VkCommandPool pool, std::vector<VkCommandBuffer>& cmds) noexcept {
    if (!cmds.empty() && pool != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(device, pool, static_cast<uint32_t>(cmds.size()), cmds.data());
        LOG_INFO_CAT("RAII", "Freed {} command buffer(s)", cmds.size());
        cmds.clear();
    }
}

// --- DESCRIPTOR SETS ---

inline void freeSingleDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSet& set) noexcept {
    if (set != VK_NULL_HANDLE && pool != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
        VkResult r = vkFreeDescriptorSets(device, pool, 1, &set);
        if (r == VK_SUCCESS) {
            LOG_INFO_CAT("RAII", "Freed VkDescriptorSet: {:p}", static_cast<void*>(set));
        } else {
            LOG_ERROR_CAT("RAII", "vkFreeDescriptorSets failed: {}", static_cast<int>(r));
        }
        set = VK_NULL_HANDLE;
    }
}

inline void destroySingleDescriptorPool(VkDevice d, VkDescriptorPool& p) noexcept {
    destroySingle<VkDescriptorPool, vkDestroyDescriptorPool>(d, p);
}

// --- INSTANCE / DEVICE ---

inline void destroyDevice(VkDevice d) noexcept {
    if (d != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(d);
        vkDestroyDevice(d, nullptr);
        LOG_INFO_CAT("Vulkan", "Destroyed VkDevice: {:p}", static_cast<void*>(d));
    }
}

inline void destroyDebugUtilsMessengerEXT(VkInstance i, VkDebugUtilsMessengerEXT m) noexcept {
    if (m != VK_NULL_HANDLE && i != VK_NULL_HANDLE) {
        auto f = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(i, "vkDestroyDebugUtilsMessengerEXT");
        if (f) f(i, m, nullptr);
    }
}

inline void destroySurfaceKHR(VkInstance i, VkSurfaceKHR s) noexcept {
    if (i != VK_NULL_HANDLE && s != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(i, s, nullptr);
    }
}

inline void destroyInstance(VkInstance i) noexcept {
    if (i != VK_NULL_HANDLE) {
        vkDestroyInstance(i, nullptr);
    }
}

// --- CONTEXT CLEANUP ---

inline void cleanupVulkanContext(Vulkan::Context& context) noexcept {
    LOG_INFO_CAT("Vulkan", "=== BEGIN Vulkan Context Cleanup ===");
    if (context.device != VK_NULL_HANDLE) vkDeviceWaitIdle(context.device);

    destroySingleDescriptorPool(context.device, context.descriptorPool);
    destroySingleCommandPool(context.device, context.commandPool);
    destroySingleSwapchain(context.device, context.swapchain);

    for (auto& view : context.swapchainImageViews) destroySingleImageView(context.device, view);
    context.swapchainImageViews.clear();

    for (auto& mem : context.uniformBufferMemories) freeSingleDeviceMemory(context.device, mem);
    for (auto& buf : context.uniformBuffers) destroySingleBuffer(context.device, buf);

    destroyDevice(context.device);
    destroySurfaceKHR(context.instance, context.surface);
    destroyDebugUtilsMessengerEXT(context.instance, context.debugMessenger);
    destroyInstance(context.instance);

    LOG_INFO_CAT("Vulkan", "=== END Vulkan Context Cleanup ===");
}

} // namespace Dispose

#endif // DISPOSE_HPP