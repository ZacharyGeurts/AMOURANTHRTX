// include/modes/RenderMode8.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — RENDER MODE 8 — SACRED ORANGE VOID
// =============================================================================
// Pure solid orange clear fallback mode — zero overhead, guaranteed visible.
// ORANGE PHOTONS ETERNAL — THE EMPIRE RESTS IN SACRED LIGHT
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

class RenderMode8 {
public:
    RenderMode8(uint32_t width, uint32_t height);

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