// modes/RenderMode2.hpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// RenderMode2: Animated Color Clear — Time-Based Sine Wave Gradient
// • No uniforms or ray tracing — Direct output clear with oscillating RGB
// • Computes hue cycle: R=sin(t), G=sin(t+2π/3), B=sin(t+4π/3)
// • Simple, low-overhead mode for testing/resizing
// • VALHALLA v80 TURBO — NOVEMBER 15, 2025 — RAINBOW PHOTONS ETERNAL
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/camera.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <chrono>
#include <cmath>

using namespace RTX;

namespace Engine {

class RenderMode2 {
public:
    RenderMode2(VulkanRTX& rtx, uint32_t width, uint32_t height);
    ~RenderMode2();

    void initResources();
    void renderFrame(VkCommandBuffer cmd, float deltaTime);
    void onResize(uint32_t width, uint32_t height);

private:
    VulkanRTX& rtx_;
    uint32_t width_, height_;
    std::chrono::steady_clock::time_point startTime_;

    // Resources
    RTX::Handle<VkImage> outputImage_;
    RTX::Handle<VkImageView> outputView_;

    void clearAnimated(VkCommandBuffer cmd);
    static constexpr float PI = 3.14159265359f;
};

}  // namespace Engine