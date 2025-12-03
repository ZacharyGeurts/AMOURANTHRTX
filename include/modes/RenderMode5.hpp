// =============================================================================
// include/modes/RenderMode5.hpp
// =============================================================================
// RENDERMODE 5 — HYPERTRACE NEXUS VISUALIZER
// Pink void + live Nexus score heatmap overlay
// For tuning adaptive sampling thresholds. The empire sees variance.
// =============================================================================

#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>

class RenderMode5
{
public:
    RenderMode5(uint32_t width, uint32_t height);
    ~RenderMode5() = default;

    void renderFrame(VkCommandBuffer cmd, float deltaTime);
    void onResize(uint32_t width, uint32_t height);

private:
    void updateUniforms(float deltaTime);
    void traceRays(VkCommandBuffer cmd);

    uint32_t width_;
    uint32_t height_;
    uint64_t frameCount_ = 0;
};