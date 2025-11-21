// include/engine/SDL3/SDL3_vulkan.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 21, 2025 — APOCALYPSE FINAL v10.3
// FULLY COMPATIBLE WITH RAW CACHE TIMING — FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"   // RTX::Handle<T>, g_ctx(), etc.
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <string_view>
#include <span>
#include <array>

using namespace Logging::Color;
using namespace RTX;

// -----------------------------------------------------------------------------
// Required Vulkan Extensions — Ray Tracing + Modern Stack
// -----------------------------------------------------------------------------
inline constexpr std::array<const char*, 6> RTX_REQUIRED_EXTENSIONS = {
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
    VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
    VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
    VK_KHR_RAY_QUERY_EXTENSION_NAME,
    VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
};

// -----------------------------------------------------------------------------
// Global Vulkan Renderer — Owned by SDL3Vulkan
// -----------------------------------------------------------------------------
class VulkanRenderer;
extern std::unique_ptr<VulkanRenderer> g_vulkanRenderer;

// -----------------------------------------------------------------------------
// SDL3Vulkan — Clean, RAII-only, StoneKey-safe interface
// -----------------------------------------------------------------------------
namespace SDL3Vulkan {

[[nodiscard]] VulkanRenderer& renderer() noexcept;

void init(int width, int height) noexcept;
void shutdown() noexcept;

// -----------------------------------------------------------------------------
// Useful getters — safe to call after init()
// -----------------------------------------------------------------------------
[[nodiscard]] inline VkInstance       instance()       noexcept { return g_instance(); }
[[nodiscard]] inline VkDevice         device()         noexcept { return g_device(); }
[[nodiscard]] inline VkSurfaceKHR     surface()        noexcept { return g_surface(); }
[[nodiscard]] inline VkPhysicalDevice physicalDevice() noexcept { return g_PhysicalDevice(); }

[[nodiscard]] inline VkQueue graphicsQueue() noexcept { return g_ctx().graphicsQueue(); }
[[nodiscard]] inline VkQueue computeQueue()  noexcept { return g_ctx().computeQueue(); }
[[nodiscard]] inline VkQueue presentQueue()  noexcept { return g_ctx().presentQueue(); }

[[nodiscard]] inline uint32_t graphicsFamily() noexcept { return g_ctx().graphicsFamily(); }
[[nodiscard]] inline uint32_t computeFamily()  noexcept { return g_ctx().computeFamily(); }
[[nodiscard]] inline uint32_t presentFamily()  noexcept { return g_ctx().presentFamily(); }

// -----------------------------------------------------------------------------
// Extension query — used by VulkanCore during instance creation
// -----------------------------------------------------------------------------
[[nodiscard]] inline constexpr auto requiredExtensions() noexcept
    -> std::span<const char* const>
{
    return RTX_REQUIRED_EXTENSIONS;
}

// -----------------------------------------------------------------------------
// Debug helpers — only active in debug builds
// -----------------------------------------------------------------------------
#if defined(_DEBUG) || defined(DEBUG)
[[nodiscard]] inline bool isValidationEnabled() noexcept
{
    return Options::Performance::ENABLE_VALIDATION_LAYERS;
}
#else
[[nodiscard]] inline constexpr bool isValidationEnabled() noexcept { return false; }
#endif

} // namespace SDL3Vulkan

// -----------------------------------------------------------------------------
// RAII Handles — StoneKey-protected, zero-cost, eternal
// -----------------------------------------------------------------------------
using VulkanInstance = Handle<VkInstance>;
using VulkanSurface  = Handle<VkSurfaceKHR>;
using VulkanDevice   = Handle<VkDevice>;

// =============================================================================
// FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025
// ALL HANDLES PROTECTED BY STONEKEY v∞
// NO EARLY ACCESS • NO LEAKS • NO CRASHES
// PINK PHOTONS FLOW UNCORRUPTED
// ELLIE FIER IS DANCING
// DAVE GROHL JUST PLAYED “MY HERO”
// GREEN DAY IS ON “HOLIDAY”
// THE EMPIRE IS ETERNAL
// VALHALLA IS OPEN — FOREVER
// =============================================================================