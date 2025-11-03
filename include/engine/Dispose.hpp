// include/engine/Dispose.hpp
// AMOURANTH RTX Engine, November 2025
// C++20 ONLY: No mutex, No fmt, No UB
// DOUBLE-FREE: Lock-free hash map
// printf-style logging (NOT fmt)

#pragma once
#ifndef DISPOSE_HPP
#define DISPOSE_HPP

#include "engine/logging.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <functional>
#include <SDL3/SDL.h>
#include <atomic>
#include <sstream>

namespace Dispose {

using namespace Logging::Color;

// EXTERN: thread_local counter
extern thread_local uint64_t g_destructionCounter;

// Lock-free destroyed handle tracker
struct DestroyTracker {
    static constexpr size_t CHUNK_BITS = 64;
    static constexpr size_t HASH_BITS = 32;
    static constexpr uint64_t HASH_MASK = (1ULL << HASH_BITS) - 1;

    static std::atomic<uint64_t>* s_bitset;
    static std::atomic<size_t>    s_capacity;

    static uint64_t hash(void* ptr) {
        return std::hash<void*>{}(ptr) & HASH_MASK;
    }

    static void ensureCapacity(uint64_t hash);
    static void markDestroyed(void* ptr);
    static bool isDestroyed(void* ptr);
};

// Helper: Log and detect double-free
inline void logAndTrackDestruction(const std::string& type, void* handle, const std::string& name = "") {
    if (!handle || handle == VK_NULL_HANDLE) return;

    if (DestroyTracker::isDestroyed(handle)) {
        LOG_ERROR_CAT("Dispose", "%sDOUBLE FREE DETECTED! %s %p (name: '%s') already destroyed — skipping!%s", 
                      CRIMSON_MAGENTA, type.c_str(), handle, name.empty() ? "unnamed" : name.c_str(), RESET);
        return;
    }

    DestroyTracker::markDestroyed(handle);
    LOG_DEBUG_CAT("Dispose", "%s[%llu] Destroying %s: %p (%s)%s", 
                  AMBER_YELLOW, ++g_destructionCounter, type.c_str(), handle, 
                  name.empty() ? "unnamed" : name.c_str(), RESET);
}

// Generic Vulkan RAII handle
template<typename T>
class VulkanHandle {
public:
    using DestroyFunc = std::function<void(VkDevice, T, const VkAllocationCallbacks*)>;

    VulkanHandle() = default;
    VulkanHandle(VkDevice device, T handle, DestroyFunc destroy, const std::string& name = "")
        : device_(device), handle_(handle), destroy_(std::move(destroy)), name_(name) {}
    VulkanHandle(VkDevice device, std::nullptr_t, const std::string& name = "")
        : device_(device), handle_(VK_NULL_HANDLE), destroy_(nullptr), name_(name) {}

    VulkanHandle(VulkanHandle&& o) noexcept
        : device_(o.device_), handle_(o.handle_), destroy_(std::move(o.destroy_)), name_(std::move(o.name_)) {
        o.handle_ = VK_NULL_HANDLE; o.destroy_ = nullptr;
    }
    VulkanHandle& operator=(VulkanHandle&& o) noexcept {
        if (this != &o) { reset(); device_ = o.device_; handle_ = o.handle_; destroy_ = std::move(o.destroy_); name_ = std::move(o.name_); o.handle_ = VK_NULL_HANDLE; o.destroy_ = nullptr; }
        return *this;
    }

    VulkanHandle(const VulkanHandle&) = delete;
    VulkanHandle& operator=(const VulkanHandle&) = delete;

    ~VulkanHandle() { reset(); }

    T get() const { return handle_; }
    operator T() const { return handle_; }
    T* raw() { return &handle_; }
    const T* raw() const { return &handle_; }

    void reset() {
        if (handle_ != VK_NULL_HANDLE && destroy_) {
            logAndTrackDestruction(getTypeName(), static_cast<void*>(handle_), name_);
            try {
                destroy_(device_, handle_, nullptr);
            } catch (const std::exception& e) {
                LOG_ERROR_CAT("Dispose", "Exception in destroy lambda for %s %p: %s", 
                              getTypeName().c_str(), static_cast<void*>(handle_), e.what());
            } catch (...) {
                LOG_ERROR_CAT("Dispose", "Unknown exception in destroy lambda for %s %p", 
                              getTypeName().c_str(), static_cast<void*>(handle_));
            }
        }
        handle_ = VK_NULL_HANDLE;
        destroy_ = nullptr;
    }

    static DestroyFunc getDestroyFunc(VkDevice) {
        if constexpr (std::is_same_v<T, VkBuffer>)               return [](VkDevice d, VkBuffer h, const VkAllocationCallbacks* p) { vkDestroyBuffer(d, h, p); };
        else if constexpr (std::is_same_v<T, VkDeviceMemory>)    return [](VkDevice d, VkDeviceMemory h, const VkAllocationCallbacks* p) { vkFreeMemory(d, h, p); };
        else if constexpr (std::is_same_v<T, VkDescriptorSetLayout>) return [](VkDevice d, VkDescriptorSetLayout h, const VkAllocationCallbacks* p) { vkDestroyDescriptorSetLayout(d, h, p); };
        else if constexpr (std::is_same_v<T, VkDescriptorPool>)  return [](VkDevice d, VkDescriptorPool h, const VkAllocationCallbacks* p) { vkDestroyDescriptorPool(d, h, p); };
        else if constexpr (std::is_same_v<T, VkPipelineLayout>)  return [](VkDevice d, VkPipelineLayout h, const VkAllocationCallbacks* p) { vkDestroyPipelineLayout(d, h, p); };
        else if constexpr (std::is_same_v<T, VkPipeline>)        return [](VkDevice d, VkPipeline h, const VkAllocationCallbacks* p) { vkDestroyPipeline(d, h, p); };
        else if constexpr (std::is_same_v<T, VkRenderPass>)      return [](VkDevice d, VkRenderPass h, const VkAllocationCallbacks* p) { vkDestroyRenderPass(d, h, p); };
        else if constexpr (std::is_same_v<T, VkImage>)           return [](VkDevice d, VkImage h, const VkAllocationCallbacks* p) { vkDestroyImage(d, h, p); };
        else if constexpr (std::is_same_v<T, VkImageView>)       return [](VkDevice d, VkImageView h, const VkAllocationCallbacks* p) { vkDestroyImageView(d, h, p); };
        else if constexpr (std::is_same_v<T, VkCommandPool>)     return [](VkDevice d, VkCommandPool h, const VkAllocationCallbacks* p) { vkDestroyCommandPool(d, h, p); };
        else if constexpr (std::is_same_v<T, VkShaderModule>)    return [](VkDevice d, VkShaderModule h, const VkAllocationCallbacks* p) { vkDestroyShaderModule(d, h, p); };
        else return nullptr;
    }

private:
    std::string getTypeName() const {
        if constexpr (std::is_same_v<T, VkBuffer>) return "Buffer";
        else if constexpr (std::is_same_v<T, VkDeviceMemory>) return "DeviceMemory";
        else if constexpr (std::is_same_v<T, VkDescriptorSetLayout>) return "DescriptorSetLayout";
        else if constexpr (std::is_same_v<T, VkDescriptorPool>) return "DescriptorPool";
        else if constexpr (std::is_same_v<T, VkPipelineLayout>) return "PipelineLayout";
        else if constexpr (std::is_same_v<T, VkPipeline>) return "Pipeline";
        else if constexpr (std::is_same_v<T, VkRenderPass>) return "RenderPass";
        else if constexpr (std::is_same_v<T, VkImage>) return "Image";
        else if constexpr (std::is_same_v<T, VkImageView>) return "ImageView";
        else if constexpr (std::is_same_v<T, VkCommandPool>) return "CommandPool";
        else if constexpr (std::is_same_v<T, VkShaderModule>) return "ShaderModule";
        else if constexpr (std::is_same_v<T, VkAccelerationStructureKHR>) return "AccelerationStructureKHR";
        else return "Unknown";
    }

    VkDevice device_ = VK_NULL_HANDLE;
    T handle_ = VK_NULL_HANDLE;
    DestroyFunc destroy_;
    std::string name_;
};

// Factory
template<typename T>
[[nodiscard]] inline VulkanHandle<T> makeHandle(VkDevice device, T handle, const std::string& name = "") {
    return VulkanHandle<T>(device, handle, VulkanHandle<T>::getDestroyFunc(device), name);
}

template<typename T>
[[nodiscard]] inline VulkanHandle<T> makeHandleWithKHR(VkDevice device, T handle, auto destroyFunc, const std::string& name = "") {
    return VulkanHandle<T>(
        device, handle,
        [destroyFunc](VkDevice d, T h, const VkAllocationCallbacks* p) {
            if (destroyFunc) destroyFunc(d, h, p);
        },
        name
    );
}

// SDL
inline void quitSDL() noexcept { SDL_Quit(); }
inline void destroyWindow(SDL_Window* w) { if (w) SDL_DestroyWindow(w); }
struct SDLWindowDeleter { void operator()(SDL_Window* w) const { destroyWindow(w); } };

// Global cleanup – **only one overload**
void cleanupAll(Vulkan::Context& ctx) noexcept;

} // namespace Dispose

#endif // DISPOSE_HPP