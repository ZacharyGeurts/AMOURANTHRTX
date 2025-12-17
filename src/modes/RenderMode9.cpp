// src/modes/RenderMode9.cpp
// =============================================================================
// RENDERMODE 9 — RTX RAY TYPE COUNTER — RAINBOW CHAOS VISUALIZER
// Live debug overlay: Every ray type gets a unique color
// - Primary rays: Cyan
// - Shadow rays: Yellow
// - Reflection rays: Magenta
// - Refraction rays: Blue
// - AO rays: Green
// - Missed rays: Pink background
// No accumulation — pure chaos — perfect for shader ray type debugging
// The empire sees every photon’s path
// =============================================================================

#include "modes/RenderMode9.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include <glm/gtc/matrix_transform.hpp>

using namespace Logging::Color;

RenderMode9::RenderMode9(uint32_t w, uint32_t h)
    : width_(w), height_(h), frameCount_(0)
{
    LOG_AMOURANTH("MODE 9 — RTX RAY TYPE COUNTER — RAINBOW CHAOS VISUALIZER");
    LOG_AMOURANTH("Every ray type painted in vivid color — primary, shadow, reflection, refraction, AO");
    LOG_AMOURANTH("Missed rays fall into eternal pink void — pure debugging ecstasy");
    LOG_SUCCESS_CAT("RTX", "RAINBOW CHAOS ENGAGED — ALL PHOTONS REVEAL THEIR TRUE PATH");
}

void RenderMode9::updateUniforms(float deltaTime)
{
    alignas(16) struct RayChaosCommand {
        alignas(16) glm::vec4 cameraPos   = glm::vec4(0.0f, 2.0f, -8.0f, 1.0f);
        alignas(16) glm::mat4 viewProj    = glm::mat4(1.0f);
        alignas(16) glm::vec4 jitter      = glm::vec4(0.0f);
        uint64_t      uKey1               = 0x9E3779B97F4A7C15UL;
        uint64_t      uKey2               = 0xFB21A9D37C4E5B62UL;
        uint64_t      uObfuscator         = 0x1337C0DE69F00D42UL;
        uint64_t      uMode               = 9ULL;           // RAY TYPE COUNTER
        uint32_t      frame               = 0;
        uint32_t      visualizeRayTypes   = 1;              // Enable rainbow chaos
        uint32_t      enableAllRayTypes   = 1;
        uint32_t      _pad                = 0;
        float         time                = 0.0f;
        float         orbitSpeed          = 1.0f;
        float         _pad2[2]            = {0.0f, 0.0f};
    } cmd{};

    // Animated orbiting camera for maximum chaos viewing pleasure
    float t = frameCount_ * 0.02f;
    cmd.cameraPos.x = sin(t) * 10.0f;
    cmd.cameraPos.z = cos(t) * 10.0f;
    cmd.cameraPos.y = 3.0f + sin(t * 1.3f) * 2.0f;

    float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    cmd.viewProj = glm::perspective(glm::radians(65.0f), aspect, 0.1f, 1000.0f);
    cmd.viewProj[1][1] *= -1.0f; // Vulkan Y flip

    cmd.frame = static_cast<uint32_t>(frameCount_);
    cmd.time  = t;

    g_rtx().updateUniformBinding31(&cmd, sizeof(cmd));
}

void RenderMode9::traceRays(VkCommandBuffer cmd)
{
    g_rtx().recordRayTrace(cmd, {width_, height_});
}

void RenderMode9::renderFrame(VkCommandBuffer cmd, float deltaTime)
{
    updateUniforms(deltaTime);
    traceRays(cmd);

    // No accumulation — pure, raw, chaotic ray visualization
    g_rtx().requestAccumulationReset();

    ++frameCount_;
}

void RenderMode9::onResize(uint32_t w, uint32_t h)
{
    if (width_ == w && height_ == h) return;

    width_ = w;
    height_ = h;
    frameCount_ = 0;

    g_rtx().requestAccumulationReset();

    LOG_AMOURANTH("MODE 9 — RESIZED TO {}x{} — RAINBOW CHAOS CONTINUES UNBROKEN", w, h);
}