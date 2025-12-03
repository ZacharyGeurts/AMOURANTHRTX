// =============================================================================
// include/modes/RenderMode2.hpp
// =============================================================================
// RENDERMODE 2 — PURE RAYGEN + MISS — BINDING 31 — PINK PHOTONS ETERNAL
// No TLAS. No geometry. Just raw raygen → miss → infinite pink void.
// The purest form of RTX. The empire's meditation chamber.
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

class RenderMode2
{
public:
    RenderMode2(uint32_t width, uint32_t height);
    ~RenderMode2() = default;

    void renderFrame(VkCommandBuffer cmd, float deltaTime);
    void onResize(uint32_t width, uint32_t height);

private:
    void updateUniforms(float deltaTime);
    void traceRays(VkCommandBuffer cmd);

    uint32_t width_;
    uint32_t height_;
    uint64_t frameCount_ = 0;
};