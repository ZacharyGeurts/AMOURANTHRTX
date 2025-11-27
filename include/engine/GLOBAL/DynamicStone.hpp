// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — NOVEMBER 21, 2025
// FULLY COMPILING — PURE EMPIRE - Inspired by Ellie Fier
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

namespace RTX {

// This is the only thing 99.999% of developers will ever use
// Returns a valid VkDeviceAddress instantly — always works — never fails
[[nodiscard]] VkDeviceAddress StoneAllocate(
    VkDeviceSize size,
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                               VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                               VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    const char* name = nullptr
) noexcept;

// Optional: hint that you're done (does nothing in shipping, but makes juniors happy)
void StoneFree(VkDeviceAddress addr) noexcept;

// Debug stats — only used by you, never by users
uint64_t StoneTotalGiB() noexcept;
uint32_t StoneCount() noexcept;

} // namespace RTX