// =============================================================================
// include/modes/RenderMode3.hpp
// =============================================================================
// RENDERMODE 3 — FULL RTX PATH — TLAS + ACCUMULATION + PINK ON FRAME 0
// The real engine. The truth. The empire in motion.
// First light achieved. The photons now have a world.
// =============================================================================

#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>

class RenderMode3
{
public:
    RenderMode3(uint32_t width, uint32_t height);
    ~RenderMode3() = default;

    void renderFrame(VkCommandBuffer cmd, float deltaTime);
    void onResize(uint32_t width, uint32_t height);

private:
    void updateUniforms(float deltaTime);
    void traceRays(VkCommandBuffer cmd);

    uint32_t width_;
    uint32_t height_;
    uint64_t frameCount_ = 0;
};