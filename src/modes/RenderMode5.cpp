// src/modes/RenderMode5.cpp
// =============================================================================
// RENDERMODE 5 — RTX LIGHTING SHOWCASE — DYNAMIC GLOBAL ILLUMINATION
// Animated emissive orb + temporal accumulation + live Nexus variance heatmap
// Pink photons bounce eternally — the empire bathes in RTX glory
// =============================================================================

#include "modes/RenderMode5.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

using namespace Logging::Color;

RenderMode5::RenderMode5(uint32_t width, uint32_t height)
    : width_(width), height_(height), frameCount_(0)
{
    LOG_AMOURANTH("MODE 5 — RTX LIGHTING SHOWCASE — DYNAMIC GI + TEMPORAL ACCUMULATION");
    LOG_AMOURANTH("Animated emissive orb illuminates the scene — pink photons bounce with full RTX glory");
    LOG_AMOURANTH("Red overlay = high variance (Nexus adaptive sampling active)");
    LOG_SUCCESS_CAT("RTX", "THE EMPIRE GLOWS — GLOBAL ILLUMINATION IGNITED — PINK PHOTONS ETERNAL");
}

void RenderMode5::updateUniforms(float deltaTime)
{
    alignas(16) struct ShowcaseCommand {
        alignas(16) glm::vec4 lightPos     = glm::vec4(0.0f, 2.0f, 0.0f, 1.0f);
        alignas(16) glm::vec4 lightColor   = glm::vec4(1.0f, 0.3f, 0.7f, 20.0f);  // Hot pink, high intensity
        alignas(16) glm::vec4 cameraPos    = glm::vec4(0.0f, 1.0f, -5.0f, 1.0f);
        alignas(16) glm::mat4 viewProj     = glm::mat4(1.0f);
        alignas(16) glm::vec4 jitter       = glm::vec4(0.0f);
        uint64_t      uKey1                = 0x9E3779B97F4A7C15UL;
        uint64_t      uKey2                = 0xFB21A9D37C4E5B62UL;
        uint64_t      uObfuscator          = 0x1337C0DE69F00D42UL;
        uint64_t      uMode                = 5ULL;           // RTX LIGHTING SHOWCASE
        uint32_t      frame                = 0;
        uint32_t      visualizeNexus       = 1;              // Show variance heatmap
        uint32_t      enableGI             = 1;
        uint32_t      enableShadows        = 1;
        float         time                 = 0.0f;
        float         lightRadius          = 0.5f;
        float         lightPulse           = 1.0f;
        float         _pad                 = 0.0f;
    } cmd{};

    // Animated orbiting light
    float t = frameCount_ * 0.02f;
    cmd.lightPos.x = sin(t) * 3.0f;
    cmd.lightPos.z = cos(t) * 3.0f;
    cmd.lightPos.y = 2.0f + sin(t * 1.7f) * 1.0f;

    // Pulsing intensity
    cmd.lightColor.w = 15.0f + sin(t * 3.0f) * 5.0f;

    float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    cmd.viewProj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 1000.0f);

    cmd.frame = static_cast<uint32_t>(frameCount_);
    cmd.time = static_cast<float>(frameCount_) * deltaTime;

    g_rtx().updateUniformBinding31(&cmd, sizeof(cmd));
}

void RenderMode5::traceRays(VkCommandBuffer cmd)
{
    g_rtx().recordRayTrace(cmd, {width_, height_});
}

void RenderMode5::renderFrame(VkCommandBuffer cmd, float deltaTime)
{
    updateUniforms(deltaTime);
    traceRays(cmd);

    // Keep accumulation for smooth GI, but reset occasionally for fresh look
    if (frameCount_ % 600 == 0) {
        g_rtx().requestAccumulationReset();
    }

    ++frameCount_;
}

void RenderMode5::onResize(uint32_t width, uint32_t height)
{
    if (width == width_ && height == height_) return;

    width_  = width;
    height_ = height;

    g_rtx().requestAccumulationReset();

    LOG_AMOURANTH("MODE 5 — RESIZED TO {}x{} — RTX LIGHTING CONTINUES UNBROKEN", width, height);
}