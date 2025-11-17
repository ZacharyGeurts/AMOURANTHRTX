// include/engine/Vulkan/Compositor.hpp
// AMOURANTH RTX — CROSS-PLATFORM HDR COMPOSITOR v4.1 — NOVEMBER 16 2025
// Custom Vulkan-based 10-bit HDR on Linux (Wayland/X11 via Mesa) + Windows (Native WSI)
// Self-contained: Nested surface, tone-map, passthrough — no external deps
// FORCE OVERRIDE: Fallback to coerced 10-bit if detection fails — PINK PHOTONS DEFY GRAVITY
// PINK PHOTONS ETERNAL — 10-BIT EVERYWHERE

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
    // Returns true if 10-bit/HDR pipeline activated (platform-agnostic)
    [[nodiscard]] bool try_enable_hdr() noexcept;

    // Force HDR activation without surface query (for fallback override)
    void force_hdr(VkFormat fmt, VkColorSpaceKHR cs) noexcept;

    // Query current HDR status at any time
    [[nodiscard]] bool is_hdr_active() noexcept;

    // Per-frame: Inject HDR metadata before present (call in render loop)
    void inject_hdr_metadata(VkDevice device, VkSwapchainKHR swapchain, float maxCLL = 1000.0f, float maxFALL = 400.0f) noexcept;

} // namespace HDRCompositor