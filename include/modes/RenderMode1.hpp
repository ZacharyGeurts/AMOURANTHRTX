// modes/RenderMode1.hpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// RenderMode1: Basic Path Tracing — Accumulation + Tonemapping
// • Uniforms: ViewProj + Time + Frame
// • Ray Trace → Accum Buffer (RGBA16F) → Tonemap to Output (R8G8B8A8)
// • Hot Pink Clear (Placeholder/Debug)
// • VALHALLA v80 TURBO — NOVEMBER 15, 2025 — PINK PHOTONS ETERNAL
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/camera.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <chrono>

namespace Engine {

class RenderMode1 {
public:
    RenderMode1(VulkanRTX& rtx, uint32_t width, uint32_t height);
    ~RenderMode1();

    void initResources();
    void renderFrame(VkCommandBuffer cmd, float deltaTime);
    void onResize(uint32_t width, uint32_t height);

private:
    VulkanRTX& rtx_;
    uint32_t width_, height_;
    uint32_t frameCount_ = 0;
    float accumWeight_ = 1.0f;
    std::chrono::steady_clock::time_point lastFrame_;

    // Resources
    uint64_t uniformBuf_ = 0;
    uint64_t accumulationBuf_ = 0;
    VkDeviceSize accumSize_ = 0;
    RTX::Handle<VkImage> accumImage_;
    RTX::Handle<VkImageView> accumView_;
    RTX::Handle<VkImage> outputImage_;
    RTX::Handle<VkImageView> outputView_;

    void updateUniforms(float deltaTime);
    void traceRays(VkCommandBuffer cmd);
    void accumulateAndToneMap(VkCommandBuffer cmd);
};

}  // namespace Engine