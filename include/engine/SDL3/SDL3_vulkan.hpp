// AMOURANTH RTX Engine, October 2025 - Vulkan instance and surface initialization with SDL3.
// Dependencies: SDL3, Vulkan 1.3+, C++20 standard library.
// Zachary Geurts 2025

#ifndef SDL3_VULKAN_HPP
#define SDL3_VULKAN_HPP

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <string_view>

namespace SDL3Initializer {

struct VulkanInstanceDeleter {
    void operator()(VkInstance instance);
};

struct VulkanSurfaceDeleter {
    VulkanSurfaceDeleter() = default;
    explicit VulkanSurfaceDeleter(VkInstance instance) : m_instance(instance) {}
    void operator()(VkSurfaceKHR surface);
    VkInstance m_instance = nullptr;
};

using VulkanInstancePtr = std::unique_ptr<std::remove_pointer_t<VkInstance>, VulkanInstanceDeleter>;
using VulkanSurfacePtr = std::unique_ptr<std::remove_pointer_t<VkSurfaceKHR>, VulkanSurfaceDeleter>;

void initVulkan(
    SDL_Window* window, 
    VulkanInstancePtr& instance,
    VulkanSurfacePtr& surface,
    bool enableValidation, 
    bool preferNvidia, 
    bool rt, 
    std::string_view title,
    VkPhysicalDevice& physicalDevice);

VkInstance getVkInstance(const VulkanInstancePtr& instance);

VkSurfaceKHR getVkSurface(const VulkanSurfacePtr& surface);

std::vector<std::string> getVulkanExtensions();

} // namespace SDL3Initializer

#endif // SDL3_VULKAN_HPP