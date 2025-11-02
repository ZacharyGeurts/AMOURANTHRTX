// include/engine/Dispose.hpp
// AMOURANTH RTX Engine, November 2025
// RAII + cleanup utilities. Supports lambda destroyers.
// cleanupAll() in Dispose.cpp.
// Dependencies: VulkanCore.hpp (Context), logging.hpp, SDL3

#pragma once
#ifndef DISPOSE_HPP
#define DISPOSE_HPP

#include "engine/logging.hpp"
#include "engine/Vulkan/VulkanCore.hpp"  // For Vulkan::Context
#include <vulkan/vulkan.h>
#include <string>
#include <vector>
#include <functional>
#include <SDL3/SDL.h>

namespace Dispose {

using namespace Logging::Color;

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
            LOG_DEBUG_CAT("Dispose", "{}Destroying {}: {:p}{}", AMBER_YELLOW, name_.empty() ? "resource" : name_, static_cast<void*>(handle_), RESET);
            destroy_(device_, handle_, nullptr);
        }
        handle_ = VK_NULL_HANDLE;
        destroy_ = nullptr;
    }

    // Compile-time destroy for core Vulkan objects
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
    VkDevice device_ = VK_NULL_HANDLE;
    T handle_ = VK_NULL_HANDLE;
    DestroyFunc destroy_;
    std::string name_;
};

// Factory for core Vulkan objects
template<typename T>
[[nodiscard]] inline VulkanHandle<T> makeHandle(VkDevice device, T handle, const std::string& name = "") {
    return VulkanHandle<T>(device, handle, VulkanHandle<T>::getDestroyFunc(device), name);
}

// Factory for KHR extension objects (uses loaded function pointer)
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

// SDL cleanup
inline void quitSDL() noexcept { SDL_Quit(); }
inline void destroyWindow(SDL_Window* w) { if (w) SDL_DestroyWindow(w); }
struct SDLWindowDeleter { void operator()(SDL_Window* w) const { destroyWindow(w); } };

// Global cleanup
void cleanupAll(Vulkan::Context& ctx) noexcept;

} // namespace Dispose

#endif // DISPOSE_HPP