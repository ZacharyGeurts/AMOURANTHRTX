// =============================================================================
// src/modes/RenderMode8.cpp
// =============================================================================
// RENDERMODE 8 — SHADOW RAY VISUALIZER — YELLOW = SHADOW RAY FIRED
// LIVE DEBUG OVERLAY — NO ACCUMULATION — PINK BACKGROUND
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
    LOG_SUCCESS_CAT("RTX", "MODE 8 — SHADOW RAY VISUALIZER — YELLOW = SHADOW RAY FIRED");
}

void RenderMode8::updateUniforms(float)
{
    alignas(16) struct Cmd {
        alignas(16) glm::vec4 cameraPos;
        alignas(16) glm::mat4 viewProj;
        uint64_t uKey1;
        uint64_t uKey2;
        uint64_t uObfuscator;
        uint64_t uMode;
        uint32_t frame;
        uint32_t visualize;
    } cmd{};

    cmd.cameraPos   = glm::vec4(0.0f, 0.0f, -5.0f, 1.0f);
    cmd.uKey1       = 0x9E37AF18C64D8A17UL;
    cmd.uKey2       = 0xE4F8B29D71A3C56CUL;
    cmd.uObfuscator = 0x9E37AF18C64D8A17UL ^ 0xE4F8B29D71A3C56CUL ^ 0x1337C0DE69F00D42UL;
    cmd.uMode       = 8ULL;
    cmd.frame       = static_cast<uint32_t>(frameCount_);
    cmd.visualize   = 1;

    float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    cmd.viewProj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 1000.0f);

    g_rtx().updateUniformBinding31(&cmd, sizeof(cmd));
}

void RenderMode8::traceRays(VkCommandBuffer cmd)
{
    g_rtx().recordRayTrace(cmd, {width_, height_});
}

void RenderMode8::renderFrame(VkCommandBuffer cmd, float dt)
{
    updateUniforms(dt);
    traceRays(cmd);
    g_rtx().requestAccumulationReset();
    ++frameCount_;
}

void RenderMode8::onResize(uint32_t w, uint32_t h)
{
    width_ = w;
    height_ = h;
    frameCount_ = 0;
    g_rtx().requestAccumulationReset();
}