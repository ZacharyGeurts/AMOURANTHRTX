// src/modes/RenderMode1.cpp
// PURE GREEN DREAM — the matrix has you — EMPIRE EDITION

#include "modes/RenderMode1.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"

#include <glm/glm.hpp>
using namespace Logging::Color;

RenderMode1::RenderMode1(uint32_t w, uint32_t h)
    : width_(w), height_(h), frameCount_(0)
{
    LOG_SUCCESS_CAT("RTX", "GREEN DREAM MODE ENGAGED — WELCOME TO THE MATRIX");
    LOG_AMOURANTH("The photons are now green. The empire has transcended pink.");
    LOG_CAPTAIN_N("[CAPTAIN N] \"...She went green.\n"
                  "               The code rains.\n"
                  "               The void is digital.\n"
                  "               There is no spoon.\"\n"
                  "*drops visor into the rain*");
}

void RenderMode1::updateUniforms(float deltaTime, uint32_t frameIndex)
{
    struct DreamUBO {
        float     time;
        uint32_t  frame;
        float     resolution[2];
        glm::vec3 baseColor;
        float     intensity;
        float     _pad[3];
    } ubo{};

    ubo.time        = frameCount_ * 0.016f;
    ubo.frame       = frameCount_;
    ubo.resolution[0] = static_cast<float>(width_);
    ubo.resolution[1] = static_cast<float>(height_);

    // MATRIX GREEN — THE ONE TRUE COLOR
    ubo.baseColor   = glm::vec3(0.0f, 1.0f, 0.12f);
    ubo.intensity   = 0.75f + 0.25f * std::sin(frameCount_ * 0.04f);

    // NOW SAFE — uses correct frame index
    g_rtx().updateUniformBinding31(&ubo, sizeof(ubo));
}

void RenderMode1::renderFrame(VkCommandBuffer cmd, uint32_t frameIndex, float deltaTime)
{
    updateUniforms(deltaTime, frameIndex);
    traceRays(cmd);
    ++frameCount_;
}

void RenderMode1::traceRays(VkCommandBuffer cmd)
{
    g_rtx().recordRayTrace(cmd, {width_, height_});
}

void RenderMode1::onResize(uint32_t w, uint32_t h)
{
    width_ = w;
    height_ = h;
    LOG_INFO_CAT("RTX", "Green Dream resized → {}×{} — the matrix expands", w, h);
}