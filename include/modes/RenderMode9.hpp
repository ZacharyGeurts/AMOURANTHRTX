// modes/RenderMode9.hpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// RenderMode9: THE ASCENSION — Full-Screen Post-Process Shader Test
// • Actually uses the RTX pipeline for once!
// • Binds a tiny fullscreen compute shader that outputs:
//       color = sin(time + uv * 10) * hot pink + camera position tint
// • First mode that goes beyond vkCmdClearColorImage
// • This is the bridge to real RTX — the ninth gate
// • VALHALLA v80 TURBO — ASCENSION PHOTONS ETERNAL
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/camera.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <chrono>

namespace Engine {

class RenderMode9 {
public:
    RenderMode9(VulkanRTX& rtx, uint32_t width, uint32_t height);
    ~RenderMode9();

    void initResources();
    void renderFrame(VkCommandBuffer cmd, float deltaTime);
    void onResize(uint32_t width, uint32_t height);

private:
    VulkanRTX& rtx_;
    uint32_t width_, height_;
    std::chrono::steady_clock::time_point startTime_;

    // Output only — we write directly into the final image via compute
    RTX::Handle<VkImage> outputImage_;
    RTX::Handle<VkImageView> outputView_;

    // Uniform buffer: time + frame + padding
    uint64_t uniformBuf_ = 0;

    void updateUniforms(float deltaTime);
    void dispatchAscension(VkCommandBuffer cmd);
};

}  // namespace Engine