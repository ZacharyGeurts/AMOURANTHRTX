// modes/RenderMode5.hpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// RenderMode5: Plasma Field — Classic 90s Demo-Scene Plasma
// • Pure CPU-side plasma using sin(time + x*y) math
// • Fills output image with vkCmdClearColorImage + animated palette
// • No ray tracing, no storage buffers — pure nostalgia
// • VALHALLA v80 TURBO — PLASMA PHOTONS ETERNAL
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/camera.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <chrono>

using namespace RTX;

namespace Engine {

class RenderMode5 {
public:
    RenderMode5(VulkanRTX& rtx, uint32_t width, uint32_t height);
    ~RenderMode5();

    void initResources();
    void renderFrame(VkCommandBuffer cmd, float deltaTime);
    void onResize(uint32_t width, uint32_t height);

private:
    VulkanRTX& rtx_;
    uint32_t width_, height_;
    std::chrono::steady_clock::time_point startTime_;

    RTX::Handle<VkImage> outputImage_;
    RTX::Handle<VkImageView> outputView_;

    void clearPlasma(VkCommandBuffer cmd);
    glm::vec3 plasmaColor(float t, float x, float y);
};

}  // namespace Engine