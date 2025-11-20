// engine/SDL3/SDL3_vulkan.cpp
/// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 20, 2025 — APOCALYPSE FINAL v7.0
// MAIN — FULL RTX ALWAYS — VALIDATION LAYERS FORCE-DISABLED — PINK PHOTONS ETERNAL
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