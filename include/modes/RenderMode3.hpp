// modes/RenderMode3.hpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// RenderMode3: Random Color Clear — Per-Frame Stochastic Hues
// • No uniforms or ray tracing — Direct output clear with random RGB each frame
// • Uses Mersenne Twister RNG: uniform [0,1) for R/G/B, A=1.0
// • Chaotic, vibrant mode for testing randomness/resizing
// • VALHALLA v80 TURBO — NOVEMBER 15, 2025 — CHAOS PHOTONS ETERNAL
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/camera.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <chrono>
#include <random>

using namespace RTX;

namespace Engine {

class RenderMode3 {
public:
    RenderMode3(VulkanRTX& rtx, uint32_t width, uint32_t height);
    ~RenderMode3();

    void initResources();
    void renderFrame(VkCommandBuffer cmd, float deltaTime);
    void onResize(uint32_t width, uint32_t height);

private:
    VulkanRTX& rtx_;
    uint32_t width_, height_;

    // Resources
    RTX::Handle<VkImage> outputImage_;
    RTX::Handle<VkImageView> outputView_;

    // RNG
    std::random_device rd_;
    std::mt19937 gen_;
    std::uniform_real_distribution<float> dist_{0.0f, 1.0f};

    void clearRandom(VkCommandBuffer cmd);
};

}  // namespace Engine