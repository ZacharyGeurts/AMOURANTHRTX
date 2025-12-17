// include/modes/RenderMode1.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — PURE PINK VOID HEADER
// GENERAL COMPUTE RENDERING — FULLY UNIFIED
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

class RenderMode1 {
public:
    RenderMode1(uint32_t w, uint32_t h);

    void renderFrame(VkCommandBuffer cmd, uint32_t frameIndex, float deltaTime);
    void onResize(uint32_t w, uint32_t h);

private:
    uint32_t width_;
    uint32_t height_;
};