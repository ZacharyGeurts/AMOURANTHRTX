// include/engine/SDL3/SDL3_vulkan.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// SDL3 + Vulkan RAII — FULL C++23 — NOVEMBER 08 2025
// source_location FIXED + inline helpers

#pragma once

#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <source_location>  // ← FIXED
#include <unordered_set>

namespace SDL3Initializer {

struct VulkanInstanceDeleter {
    void operator()(VkInstance instance) const;
};

struct VulkanSurfaceDeleter {
    VkInstance m_instance = VK_NULL_HANDLE;
    explicit VulkanSurfaceDeleter(VkInstance inst = VK_NULL_HANDLE) : m_instance(inst) {}
    void operator()(VkSurfaceKHR surface) const;
};

using VulkanInstancePtr = std::unique_ptr<VkInstance_T, VulkanInstanceDeleter>;
using VulkanSurfacePtr = std::unique_ptr<VkSurfaceKHR_T, VulkanSurfaceDeleter>;

void initVulkan(
    SDL_Window* window,
    VulkanInstancePtr& instance,
    VulkanSurfacePtr& surface,
    VkDevice& device,
    bool enableValidation,
    bool preferNvidia,
    bool rt,
    std::string_view title,
    VkPhysicalDevice& physicalDevice
);

VkInstance getVkInstance(const VulkanInstancePtr& instance);
VkSurfaceKHR getVkSurface(const VulkanSurfacePtr& surface);
std::vector<std::string> getVulkanExtensions();

// Inline helpers — no format errors
static inline std::string vkResultToString(VkResult result) {
    switch (result) {
        case VK_SUCCESS: return "SUCCESS";
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "INITIALIZATION_FAILED";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "FEATURE_NOT_PRESENT";
        default: return std::format("UNKNOWN({})", static_cast<int>(result));
    }
}

static inline std::string locationString(const std::source_location& loc = std::source_location::current()) {
    return std::format("{}:{}:{}", loc.file_name(), loc.line(), loc.function_name());
}

} // namespace SDL3Initializer