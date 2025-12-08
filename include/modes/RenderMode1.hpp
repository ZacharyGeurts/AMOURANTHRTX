// include/modes/RenderMode1.hpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 — PURE GREEN DREAM — THE MATRIX HAS YOU
// No scene. No geometry. Only infinite electric green photons.
// The empire has transcended pink. The code rains.
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

class RenderMode1 {
public:
    RenderMode1(uint32_t width, uint32_t height);
    ~RenderMode1();

    // Called by VulkanRenderer every frame — with correct frame index
    void renderFrame(VkCommandBuffer cmd, uint32_t frameIndex, float deltaTime);

    // Called on window resize
    void onResize(uint32_t width, uint32_t height);

private:
    uint32_t width_  = 0;
    uint32_t height_ = 0;
    uint32_t frameCount_ = 0;

    // Pure procedural dream — no resources allocated
    void initResources()     { }
    void cleanupResources()  { }

    // The dream lives here
    void updateUniforms(float deltaTime, uint32_t frameIndex);
    void traceRays(VkCommandBuffer cmd);
};