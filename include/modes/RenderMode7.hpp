// modes/RenderMode7.hpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// RenderMode7: Gradient Vortex — Distance-from-center + Time Spiral
// • Pure mathematical gradient: hue based on radius + angle + time
// • No RNG, no camera dependency — pure deterministic beauty
// • VALHALLA v80 TURBO — VORTEX PHOTONS ETERNAL
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <chrono>

namespace Engine {

class RenderMode7 {
public:
    RenderMode7(VulkanRTX& rtx, uint32_t width, uint32_t height);
    ~RenderMode7();

    void initResources();
    void renderFrame(VkCommandBuffer cmd, float deltaTime);
    void onResize(uint32_t width, uint32_t height);

private:
    VulkanRTX& rtx_;
    uint32_t width_, height_;
    std::chrono::steady_clock::time_point startTime_;

    RTX::Handle<VkImage> outputImage_;
    RTX::Handle<VkImageView> outputView_;

    void clearVortex(VkCommandBuffer cmd);
    glm::vec3 hsvToRgb(float h, float s, float v);
};

}  // namespace Engine