// include/modes/RenderMode9.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — RENDER MODE 9 — SACRED WHITE VOID
// =============================================================================
// Pure solid white clear fallback mode — zero overhead, guaranteed visible.
// WHITE PHOTONS ETERNAL — THE EMPIRE RESTS IN SACRED LIGHT
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

class RenderMode9 {
public:
    RenderMode9(uint32_t width, uint32_t height);

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