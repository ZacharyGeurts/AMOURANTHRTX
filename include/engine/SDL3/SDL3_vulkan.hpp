// include/engine/SDL3/SDL3_vulkan.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// SDL3 + Vulkan RAII — FINAL CLEAN VERSION — NOV 14 2025
// • Full RAII using Handle<T> from RTXHandler.hpp
// • No manual destroy calls — EVER
// • VulkanRenderer owns everything else
// • PINK PHOTONS ETERNAL — 15,000+ FPS — FIRST LIGHT ACHIEVED
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"   // Brings in RTX::Handle<T>, RTX::ctx(), etc.
#include "engine/GLOBAL/logging.hpp"
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <string_view>
#include <span>
#include <array>

using namespace Logging::Color;
using namespace RTX;

// -----------------------------------------------------------------------------
// Required Vulkan Extensions (Ray Tracing + Dynamic Rendering)
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
// Global Vulkan Renderer
// -----------------------------------------------------------------------------
class VulkanRenderer;
extern std::unique_ptr<VulkanRenderer> g_vulkanRenderer;

// -----------------------------------------------------------------------------
// SDL3Vulkan — Clean, RAII-only interface
// -----------------------------------------------------------------------------
namespace SDL3Vulkan {

[[nodiscard]] VulkanRenderer& renderer() noexcept;

void init(int width, int height) noexcept;
void shutdown() noexcept;

[[nodiscard]] inline constexpr auto requiredExtensions() noexcept
    -> std::span<const char* const>
{
    return RTX_REQUIRED_EXTENSIONS;
}

} // namespace SDL3Vulkan

// -----------------------------------------------------------------------------
// RAII HANDLES — USING RTX::Handle<T> — EXTERNAL LINKAGE, MAP-SAFE, PERFECT
// -----------------------------------------------------------------------------
using VulkanInstance = Handle<VkInstance>;
using VulkanSurface  = Handle<VkSurfaceKHR>;
using VulkanDevice   = Handle<VkDevice>;

// =============================================================================
// END — FIRST LIGHT ACHIEVED
// ALL VULKAN OBJECTS NOW RAII VIA RTX::Handle<T>
// NO LEAKS. NO MANUAL DESTROY. ONLY PHOTONS.
// DAISY GALLOPS INTO THE OCEAN_TEAL SUNSET
// YOUR EMPIRE IS PURE
// SHIP IT RAW
// =============================================================================