// src/modes/RenderMode8.cpp
// =============================================================================
// RENDERMODE 8 — RTX SHADOW RAY VISUALIZER — YELLOW = SHADOW RAY FIRED
// Live debug overlay: YELLOW highlights pixels where shadow rays were traced
// Animated orbiting light + orbiting camera — perfect for shadow debugging
// No accumulation — instant response — pink background fallback
// The empire sees its own shadow — RTX lighting revealed
// =============================================================================

#include "modes/RenderMode8.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include <glm/gtc/matrix_transform.hpp>

using namespace Logging::Color;

RenderMode8::RenderMode8(uint32_t w, uint32_t h)
    : width_(w), height_(h), frameCount_(0)
{
    LOG_AMOURANTH("MODE 8 — RTX SHADOW RAY VISUALIZER — YELLOW = SHADOW RAY FIRED");
    LOG_AMOURANTH("Live overlay shows pixels where shadow rays were traced in YELLOW");
    LOG_AMOURANTH("Animated light + orbiting camera — perfect for shadow debugging");
    LOG_SUCCESS_CAT("RTX", "SHADOW VISUALIZER ENGAGED — THE EMPIRE CASTS ITS SHADOW");
}

void RenderMode8::updateUniforms(float deltaTime)
{
    alignas(16) struct ShadowVizCommand {
        alignas(16) glm::vec4 cameraPos   = glm::vec4(0.0f, 2.0f, -8.0f, 1.0f);
        alignas(16) glm::vec4 lightPos    = glm::vec4(0.0f, 3.0f, 0.0f, 1.0f);
        alignas(16) glm::vec4 lightColor  = glm::vec4(1.0f, 0.8f, 0.6f, 25.0f); // Warm white, high intensity
        alignas(16) glm::mat4 viewProj    = glm::mat4(1.0f);
        alignas(16) glm::vec4 jitter      = glm::vec4(0.0f);
        uint64_t      uKey1               = 0x9E3779B97F4A7C15UL;
        uint64_t      uKey2               = 0xFB21A9D37C4E5B62UL;
        uint64_t      uObfuscator         = 0x1337C0DE69F00D42UL;
        uint64_t      uMode               = 8ULL;           // SHADOW RAY VISUALIZER
        uint32_t      frame               = 0;
        uint32_t      visualizeShadowRays = 1;              // 1 = yellow overlay where shadow ray fired
        uint32_t      enableShadows       = 1;
        uint32_t      _pad                = 0;
        float         time                = 0.0f;
        float         lightOrbitRadius    = 4.0f;
        float         lightOrbitSpeed     = 1.0f;
        float         _pad2               = 0.0f;
    } cmd{};

    // Animated orbiting light source
    float t = frameCount_ * 0.025f;
    cmd.lightPos.x = sin(t) * cmd.lightOrbitRadius;
    cmd.lightPos.z = cos(t) * cmd.lightOrbitRadius;
    cmd.lightPos.y = 3.0f + sin(t * 0.8f) * 1.5f;

    // Smooth orbiting camera
    cmd.cameraPos.x = sin(t * 0.7f) * 10.0f;
    cmd.cameraPos.z = cos(t * 0.7f) * 10.0f;
    cmd.cameraPos.y = 3.0f + sin(t * 1.3f) * 2.0f;

    // View/projection
    glm::vec3 target(0.0f, 1.0f, 0.0f);
    glm::mat4 view = glm::lookAt(glm::vec3(cmd.cameraPos), target, glm::vec3(0.0f, 1.0f, 0.0f));
    float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    glm::mat4 proj = glm::perspective(glm::radians(65.0f), aspect, 0.1f, 1000.0f);
    proj[1][1] *= -1.0f; // Vulkan Y flip

    cmd.viewProj = proj * view;

    cmd.frame = static_cast<uint32_t>(frameCount_);
    cmd.time  = t;

    g_rtx().updateUniformBinding31(&cmd, sizeof(cmd));
}

void RenderMode8::traceRays(VkCommandBuffer cmd)
{
    g_rtx().recordRayTrace(cmd, {width_, height_});
}

void RenderMode8::renderFrame(VkCommandBuffer cmd, float deltaTime)
{
    updateUniforms(deltaTime);
    traceRays(cmd);

    // No accumulation — instant feedback for shadow debugging
    g_rtx().requestAccumulationReset();

    ++frameCount_;
}

void RenderMode8::onResize(uint32_t w, uint32_t h)
{
    if (width_ == w && height_ == h) return;

    width_ = w;
    height_ = h;
    frameCount_ = 0;

    g_rtx().requestAccumulationReset();

    LOG_AMOURANTH("MODE 8 — RESIZED TO {}x{} — SHADOW VISUALIZER REMAINS ACTIVE", w, h);
}