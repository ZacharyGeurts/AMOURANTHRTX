// include/engine/SDL3/SDL3_vulkan.hpp
// AMOURANTH RTX Engine, October 2025
// RAII for Vulkan instance + surface via SDL3
// NO manual vkDestroy* — handled by Dispose

#ifndef SDL3_VULKAN_HPP
#define SDL3_VULKAN_HPP

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <string_view>

namespace SDL3Initializer {

struct VulkanInstanceDeleter {
    void operator()(VkInstance instance) const;
};

struct VulkanSurfaceDeleter {
    VulkanSurfaceDeleter() = default;
    explicit VulkanSurfaceDeleter(VkInstance instance) : m_instance(instance) {}
    void operator()(VkSurfaceKHR surface) const;
    VkInstance m_instance = VK_NULL_HANDLE;
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
    VkPhysicalDevice& physicalDevice
);

VkInstance getVkInstance(const VulkanInstancePtr& instance);
VkSurfaceKHR getVkSurface(const VulkanSurfacePtr& surface);
std::vector<std::string> getVulkanExtensions();

} // namespace SDL3Initializer

#endif // SDL3_VULKAN_HPP