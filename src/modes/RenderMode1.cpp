// src/modes/RenderMode1.cpp
// PURE PINK DREAM — actually pretty edition

#include "modes/RenderMode1.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
using namespace Logging::Color;

RenderMode1::RenderMode1(uint32_t w, uint32_t h)
    : width_(w), height_(h), frameCount_(0)
{
    LOG_SUCCESS_CAT("RTX", "Pink Dream Mode engaged — prepare for beauty");
}

void RenderMode1::updateUniforms(float deltaTime)
{
    struct DreamUBO {
        float   time;           // total time
        uint32_t frame;
        float   resolution[2];
        float   _pad[2];
    } ubo{};

    ubo.time        = frameCount_ * 0.016f;           // ~60 fps assumption
    ubo.frame       = frameCount_;
    ubo.resolution[0] = float(width_);
    ubo.resolution[1] = float(height_);

    g_rtx().updateUniformBinding31(&ubo, sizeof(ubo));
}

void RenderMode1::traceRays(VkCommandBuffer cmd)
{
    g_rtx().recordRayTrace(cmd, {width_, height_});
}

void RenderMode1::renderFrame(VkCommandBuffer cmd, float deltaTime)
{
    updateUniforms(deltaTime);
    traceRays(cmd);
    ++frameCount_;
}

void RenderMode1::onResize(uint32_t w, uint32_t h)
{
    width_ = w; height_ = h;
    LOG_INFO_CAT("RTX", "Pink Dream resized → {}×{}", w, h);
}