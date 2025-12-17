// src/modes/RenderMode6.cpp
// =============================================================================
// RENDERMODE 6 — RTX ALPHA TEST VISUALIZER — RED = ALPHA-CLIPPED PIXELS
// Live debug overlay: RED highlights fragments discarded by any-hit shader
// Pink background — no accumulation — perfect for material transparency tuning
// The empire sees through the veil — alpha testing revealed
// =============================================================================

#include "modes/RenderMode6.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include <glm/gtc/matrix_transform.hpp>

using namespace Logging::Color;

RenderMode6::RenderMode6(uint32_t w, uint32_t h)
    : width_(w), height_(h), frameCount_(0)
{
    LOG_AMOURANTH("MODE 6 — RTX ALPHA TEST VISUALIZER — RED = ALPHA-CLIPPED");
    LOG_AMOURANTH("Live overlay shows discarded transparent fragments in RED");
    LOG_AMOURANTH("Perfect for debugging any-hit shaders and alpha testing");
    LOG_SUCCESS_CAT("RTX", "ALPHA VISUALIZER ENGAGED — THE EMPIRE SEES THROUGH THE VEIL");
}

void RenderMode6::updateUniforms(float)
{
    alignas(16) struct AlphaVizCommand {
        alignas(16) glm::vec4 cameraPos   = glm::vec4(0.0f, 2.0f, -8.0f, 1.0f);
        alignas(16) glm::mat4 viewProj    = glm::mat4(1.0f);
        alignas(16) glm::vec4 jitter      = glm::vec4(0.0f);
        uint64_t      uKey1               = 0x9E3779B97F4A7C15UL;
        uint64_t      uKey2               = 0xFB21A9D37C4E5B62UL;
        uint64_t      uObfuscator         = 0x1337C0DE69F00D42UL;
        uint64_t      uMode               = 6ULL;           // ALPHA TEST VISUALIZER
        uint32_t      frame               = 0;
        uint32_t      visualizeAlphaClip  = 1;              // 1 = red overlay on clipped pixels
        uint32_t      enableAlphaTest     = 1;
        uint32_t      _pad                = 0;
        float         time                = 0.0f;
        float         alphaThreshold      = 0.5f;           // Adjust in shader if needed
        float         _pad2[2]            = {0.0f, 0.0f};
    } cmd{};

    // Smooth orbiting camera for better viewing
    float t = frameCount_ * 0.02f;
    cmd.cameraPos.x = sin(t) * 8.0f;
    cmd.cameraPos.z = cos(t) * 8.0f;
    cmd.cameraPos.y = 2.0f + sin(t * 0.7f) * 1.5f;

    float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    cmd.viewProj = glm::perspective(glm::radians(65.0f), aspect, 0.1f, 1000.0f);
    cmd.viewProj[1][1] *= -1.0f; // Vulkan Y flip

    cmd.frame = static_cast<uint32_t>(frameCount_);
    cmd.time  = t;

    g_rtx().updateUniformBinding31(&cmd, sizeof(cmd));
}

void RenderMode6::traceRays(VkCommandBuffer cmd)
{
    g_rtx().recordRayTrace(cmd, {width_, height_});
}

void RenderMode6::renderFrame(VkCommandBuffer cmd, float deltaTime)
{
    updateUniforms(deltaTime);
    traceRays(cmd);

    // No accumulation — instant response for live debugging
    g_rtx().requestAccumulationReset();

    ++frameCount_;
}

void RenderMode6::onResize(uint32_t w, uint32_t h)
{
    if (width_ == w && height_ == h) return;

    width_ = w;
    height_ = h;
    frameCount_ = 0;

    g_rtx().requestAccumulationReset();

    LOG_AMOURANTH("MODE 6 — RESIZED TO {}x{} — ALPHA VISUALIZER REMAINS ACTIVE", w, h);
}