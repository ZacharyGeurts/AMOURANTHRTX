// modes/RenderMode4.hpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// RenderMode4: Camera-Tinted Clear — Position-Based XYZ Hue
// • No ray tracing — Direct output clear tinted by camera position
// • Normalize cam.pos (XYZ) to [0,1] for RGB, A=1.0 — Moves with camera
// • Interactive mode: Ties color to lazyCam movement
// • VALHALLA v80 TURBO — NOVEMBER 15, 2025 — CAMERA PHOTONS ETERNAL
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/camera.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <chrono>

using namespace RTX;

namespace Engine {

class RenderMode4 {
public:
    RenderMode4(VulkanRTX& rtx, uint32_t width, uint32_t height);
    ~RenderMode4();

    void initResources();
    void renderFrame(VkCommandBuffer cmd, float deltaTime);
    void onResize(uint32_t width, uint32_t height);

private:
    VulkanRTX& rtx_;
    uint32_t width_, height_;

    // Resources
    RTX::Handle<VkImage> outputImage_;
    RTX::Handle<VkImageView> outputView_;

    void clearCameraTinted(VkCommandBuffer cmd);
    glm::vec3 normalizePosition(const glm::vec3& pos);
};

}  // namespace Engine