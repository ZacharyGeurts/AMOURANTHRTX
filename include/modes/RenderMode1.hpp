// include/modes/RenderMode1.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — PURE MISS SHADER VOID — BINDING 31 ONLY
// No scene. No geometry. Only infinite pink photons.
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <cstdint>

class RenderMode1 {
public:
    RenderMode1(uint32_t width, uint32_t height);
    ~RenderMode1();

    // Called by VulkanRenderer every frame
    void renderFrame(VkCommandBuffer cmd, float deltaTime);

    // Called on window resize
    void onResize(uint32_t width, uint32_t height);

private:
    uint32_t width_  = 0;
    uint32_t height_ = 0;
    uint32_t frameCount_ = 0;

    // These functions are intentionally empty — we allocate ZERO resources
    void initResources()     { }
    void cleanupResources()  { }

    void updateUniforms(float deltaTime);
    void traceRays(VkCommandBuffer cmd);
    void accumulateAndToneMap(VkCommandBuffer cmd) { /* no-op */ }
};