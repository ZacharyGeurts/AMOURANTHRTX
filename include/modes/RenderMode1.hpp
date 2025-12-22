// include/modes/RenderMode1.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — RENDER MODE 1 — SACRED PINK VOID
// =============================================================================
// Pure solid pink clear fallback mode — zero overhead, guaranteed visible.
// Fully compatible with StoneKey, SwapchainManager, and resize system.
// No dependencies on ray tracing or VulkanRenderer internals.
// PINK PHOTONS ETERNAL — THE EMPIRE RESTS IN SACRED LIGHT
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

class RenderMode1 {
public:
    RenderMode1(uint32_t width, uint32_t height);

    // Renders directly into the current swapchain image
    void renderFrame(VkCommandBuffer cmd,
                     uint32_t frameIndex,
                     uint32_t imageIndex,
                     float deltaTime) noexcept;

    void onResize(uint32_t newWidth, uint32_t newHeight) noexcept;

private:
    uint32_t width_;
    uint32_t height_;
};