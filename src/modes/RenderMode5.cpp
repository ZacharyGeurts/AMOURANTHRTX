// =============================================================================
// src/modes/RenderMode5.cpp
// =============================================================================
// RENDERMODE 5 — HYPERTRACE NEXUS VISUALIZER — BINDING 31
// Pink void + live Nexus score overlay (red = high variance)
// The empire tunes itself.
// =============================================================================

#include "modes/RenderMode5.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"

using namespace Logging::Color;

RenderMode5::RenderMode5(uint32_t width, uint32_t height)
    : width_(width), height_(height), frameCount_(0)
{
    LOG_INFO_CAT("RTX", "MODE 5 — HYPERTRACE NEXUS VISUALIZER — BINDING 31 IGNITED — {}x{}", width, height);
    LOG_SUCCESS_CAT("RTX", "PINK VOID + LIVE NEXUS SCORE HEATMAP. VARIANCE IS VISIBLE. THE EMPIRE TUNES.");
}

void RenderMode5::updateUniforms(float)
{
    alignas(16) struct NexusCommand {
        alignas(16) glm::vec4 cameraPos   = glm::vec4(0.0f, 0.0f, -5.0f, 1.0f);
        alignas(16) glm::mat4 viewProj    = glm::mat4(1.0f);
        alignas(16) glm::vec4 jitter      = glm::vec4(0.0f);
        uint64_t      uKey1               = 0x9E37AF18C64D8A17UL;
        uint64_t      uKey2               = 0xE4F8B29D71A3C56CUL;
        uint64_t      uObfuscator         = 0x9E37AF18C64D8A17UL ^ 0xE4F8B29D71A3C56CUL ^ 0x1337C0DE69F00D42UL;
        uint64_t      uMode               = 5ULL;           // NEXUS VISUALIZER MODE
        uint32_t      frame               = 0;
        uint32_t      visualizeNexus      = 1;              // 1 = show score heatmap
        float         time                = 0.0f;
        uint32_t      _pad[1]             = {0};
    } cmd{};

    float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    cmd.viewProj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 1000.0f);
    cmd.frame    = static_cast<uint32_t>(frameCount_);
    cmd.time     = static_cast<float>(frameCount_) * 0.016f;

    g_rtx().updateUniformBinding31(&cmd, sizeof(cmd));
}

void RenderMode5::traceRays(VkCommandBuffer cmd)
{
    // Uses dummy TLAS → pure miss shader path
    // But uMode = 5 → shader switches to Nexus score visualization
    g_rtx().recordRayTrace(cmd, {width_, height_});
}

void RenderMode5::renderFrame(VkCommandBuffer cmd, float deltaTime)
{
    updateUniforms(deltaTime);
    traceRays(cmd);

    // Force pink + reset every frame so we see live Nexus score changes
    g_rtx().requestAccumulationReset();

    ++frameCount_;
}

void RenderMode5::onResize(uint32_t width, uint32_t height)
{
    if (width == width_ && height == height_) return;

    width_  = width;
    height_ = height;
    frameCount_ = 0;

    g_rtx().requestAccumulationReset();

    LOG_INFO_CAT("RTX", "MODE 5 — RESIZED TO {}x{} — NEXUS HEATMAP REMAINS LIVE", width, height);
}