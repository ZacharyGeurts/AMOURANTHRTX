// modes/RenderMode1.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — RENDER MODE 1 — PURE PINK VOID HEADER
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

class RenderMode1
{
public:
    RenderMode1(uint32_t width, uint32_t height);
    ~RenderMode1();

    void renderFrame(VkCommandBuffer cmd, uint32_t frameIndex, float deltaTime);
    void onResize(uint32_t newWidth, uint32_t newHeight);

private:
    uint32_t width_;
    uint32_t height_;
};