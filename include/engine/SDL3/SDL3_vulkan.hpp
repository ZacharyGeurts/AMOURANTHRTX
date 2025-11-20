// =============================================================================
// SDL3_vulkan.hpp — STONEKEY v∞ PROTECTED — APOCALYPSE FINAL 2025 AAAA
// VULKAN HEADERS REQUIRED — THIS IS ALLOWED
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>        // ← REQUIRED — DO NOT REMOVE
#include <span>
#include <array>

namespace StoneKey::Raw { struct Cache; }

class VulkanRenderer;

namespace SDL3Vulkan {

[[nodiscard]] VulkanRenderer& renderer() noexcept;

void init(int width, int height) noexcept;
void shutdown() noexcept;

[[nodiscard]] inline constexpr auto requiredExtensions() noexcept
    -> std::span<const char* const>
{
    constexpr std::array exts{
        VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
        VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
        VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
        VK_KHR_RAY_QUERY_EXTENSION_NAME,
        VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
        VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
    };
    return exts;
}

} // namespace SDL3Vulkan