// include/engine/Vulkan/Compositor.hpp
// AMOURANTH RTX — INVISIBLE HDR COMPOSITOR v2.0 — NOVEMBER 16 2025
// Fully automatic 10-bit HDR activation on Mesa / NVIDIA / Intel
// Uses existing SDL Vulkan surface — zero extra dependencies
// PINK PHOTONS ETERNAL

#pragma once

#include <vulkan/vulkan_core.h>
#include <cstdint>

// Forward declarations of your engine globals
namespace RTX {
    struct Context;
    Context& g_ctx() noexcept;
}

// Public namespace — just drop this header in and you're done
namespace HDRCompositor {

    // Call once after Vulkan instance + surface are created, before first swapchain
    // Returns true if HDR was successfully activated
    [[nodiscard]] bool try_enable_invisible_hdr() noexcept;

    // Query current HDR status at any time
    [[nodiscard]] bool is_hdr_active() noexcept;

    // Internal — do NOT touch
    namespace detail {
        extern VkFormat        g_hdr_format;
        extern VkColorSpaceKHR g_hdr_color_space;
    }

} // namespace HDRCompositor