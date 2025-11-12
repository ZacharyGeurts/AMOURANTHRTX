// include/engine/SDL3/SDL3_vulkan.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// SDL3 + Vulkan RAII – SPLIT INTO HEADER + CPP – C++23 – v4.6 – NOV 13 2025
// • ONLY uses engine/GLOBAL/logging.hpp in header
// • Declarations | VK_CHECK, no throw
// • C++23: constexpr, span, [[likely]], zero-overhead
// • RESPECTS Options::Performance::ENABLE_GPU_TIMESTAMPS → enables validation
// • PINK PHOTONS ETERNAL — 15,000+ FPS
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"   // ONLY THIS
#include "engine/GLOBAL/LAS.hpp"
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <array>
#include <span>
#include <set>

using namespace Logging::Color;

// -----------------------------------------------------------------------------
// C++23: constexpr RTX extensions
// -----------------------------------------------------------------------------
inline constexpr std::array<const char*, 6> RTX_EXTENSIONS = {
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_RAY_QUERY_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
};

// -----------------------------------------------------------------------------
// Global renderer forward declare
// -----------------------------------------------------------------------------
class VulkanRenderer;
extern std::unique_ptr<VulkanRenderer> g_vulkanRenderer;

// -----------------------------------------------------------------------------
// SDL3Vulkan interface
// -----------------------------------------------------------------------------
namespace SDL3Vulkan {

[[nodiscard]] VulkanRenderer& getRenderer() noexcept;

void initRenderer(int w, int h) noexcept;
void shutdownRenderer() noexcept;

} // namespace SDL3Vulkan

// -----------------------------------------------------------------------------
// RAII deleters
// -----------------------------------------------------------------------------
struct VulkanInstanceDeleter {
    void operator()(VkInstance i) const noexcept;
};

struct VulkanSurfaceDeleter {
    VkInstance inst = VK_NULL_HANDLE;
    constexpr explicit VulkanSurfaceDeleter(VkInstance i = VK_NULL_HANDLE) noexcept : inst(i) {}
    void operator()(VkSurfaceKHR s) const noexcept;
};

using VulkanInstancePtr = std::unique_ptr<VkInstance_T, VulkanInstanceDeleter>;
using VulkanSurfacePtr  = std::unique_ptr<VkSurfaceKHR_T, VulkanSurfaceDeleter>;

// -----------------------------------------------------------------------------
// Vulkan core init / shutdown — NO THROW, USE VK_CHECK
// -----------------------------------------------------------------------------
void initVulkan(
    SDL_Window* window,
    VulkanInstancePtr& instance,
    VulkanSurfacePtr& surface,
    VkDevice& device,
    bool enableValidation,
    bool preferNvidia,
    bool rt,
    std::string_view title,
    VkPhysicalDevice& physicalDevice) noexcept;

void shutdownVulkan() noexcept;

// -----------------------------------------------------------------------------
// Utils
// -----------------------------------------------------------------------------
[[nodiscard]] inline VkInstance getVkInstance(const VulkanInstancePtr& i) noexcept {
    return i ? i.get() : RTX::g_ctx().instance_;
}
[[nodiscard]] inline VkSurfaceKHR getVkSurface(const VulkanSurfacePtr& s) noexcept {
    return s ? s.get() : RTX::g_ctx().surface_;
}
[[nodiscard]] inline constexpr auto getVulkanExtensions() noexcept -> std::span<const char* const> {
    return RTX_EXTENSIONS;
}

// =============================================================================
// END — C++23 SPEED | VK_CHECK | NO THROW | PINK PHOTONS ETERNAL
// =============================================================================