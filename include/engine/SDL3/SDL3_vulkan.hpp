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
// SDL3 + Vulkan RAII – FINAL CLEAN VERSION – NOV 14 2025
// • VulkanRenderer OWNS ALL RT SHADERS — this file is now DUMB AND HAPPY
// • NO shader paths here — EVER AGAIN
// • Only does: SDL ↔ Vulkan bridge + renderer init/shutdown
// • PINK PHOTONS ETERNAL — 15,000+ FPS — FIRST LIGHT ACHIEVED
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <SDL3/SDL_vulkan.h>
#include <vulkan/vulkan.h>

#include <memory>
#include <string_view>
#include <span>
#include <array>

using namespace Logging::Color;

// -----------------------------------------------------------------------------
// RTX Required Instance + Device Extensions (ONLY extensions, NOT shaders!)
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
// Global renderer
// -----------------------------------------------------------------------------
class VulkanRenderer;
extern std::unique_ptr<VulkanRenderer> g_vulkanRenderer;

// -----------------------------------------------------------------------------
// SDL3Vulkan — Minimal, clean, happy interface
// -----------------------------------------------------------------------------
namespace SDL3Vulkan {

[[nodiscard]] VulkanRenderer& getRenderer() noexcept;

void initRenderer(int w, int h) noexcept;
void shutdownRenderer() noexcept;

} // namespace SDL3Vulkan

// -----------------------------------------------------------------------------
// RAII Deleters
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
// Core Vulkan init/shutdown
// -----------------------------------------------------------------------------
void initVulkan(
    SDL_Window* window,
    VulkanInstancePtr& instance,
    VulkanSurfacePtr& surface,
    VkDevice& device,
    bool enableValidation,
    bool preferNvidia,
    bool requireRT,
    std::string_view title,
    VkPhysicalDevice& physicalDevice
) noexcept;

void shutdownVulkan() noexcept;

// -----------------------------------------------------------------------------
// Utils
// -----------------------------------------------------------------------------
[[nodiscard]] inline constexpr auto getRTXExtensions() noexcept -> std::span<const char* const> {
    return RTX_EXTENSIONS;
}

// =============================================================================
// END — DUMB AND HAPPY — VulkanRenderer owns the photons now
// =============================================================================