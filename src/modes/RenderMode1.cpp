// src/modes/RenderMode1.cpp
// PURE GREEN MATRIX RAIN — FULL-SCREEN VIA MISS SHADER — DECEMBER 08, 2025

#include "modes/RenderMode1.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/LAS.hpp"  // for dummy TLAS access

#include <glm/glm.hpp>
#include <cstdint>
#include <vulkan/vulkan.h>

using namespace Logging::Color;

RenderMode1::RenderMode1(uint32_t w, uint32_t h)
    : width_(w), height_(h), frameCount_(0)
{
    BufferManager::ensureStagingRing();

    LOG_SUCCESS_CAT("RTX", "GREEN MATRIX MODE ENGAGED — WELCOME TO THE SIMULATION");
    LOG_AMOURANTH("The photons are now green. The empire has entered the Matrix.");
    LOG_CAPTAIN_N("[CAPTAIN N] \"...She went green.\n"
                  "               The code rains.\n"
                  "               The void is digital.\n"
                  "               There is no spoon.\"\n"
                  "*drops visor into the rain*");
}

void RenderMode1::renderFrame(VkCommandBuffer cmd, uint32_t frameIndex, float deltaTime)
{
    ++frameCount_;

    // Grab current frame's DreamUBO handle
    const uint64_t handle = g_rtx().uniformBufferEncs_[frameIndex % g_rtx().maxFramesInFlight_];
    auto it = BufferManager::s_buffers.find(handle);

    if (it == BufferManager::s_buffers.end() || it->second.mapped == nullptr) {
        LOG_WARN_CAT("RENDERER", "Green Matrix UBO not ready — skipping frame %u", frameIndex);
        return;
    }

    // Write green Matrix parameters directly into host-visible UBO
    struct DreamUBO {
        float     time;
        uint32_t  frame;
        float     resolution[2];
        float     exposure;
        uint32_t  enableEnvMap;
        glm::vec3 baseColor;     // custom: green rain color
        float     intensity;     // custom: pulsing effect
        float     _pad[3];
    } ubo{};

    ubo.time         = static_cast<float>(frameCount_) * 0.016f;
    ubo.frame        = frameCount_;
    ubo.resolution[0] = static_cast<float>(width_);
    ubo.resolution[1] = static_cast<float>(height_);
    ubo.exposure     = 1.0f;
    ubo.enableEnvMap = 0;                                    // pure void
    ubo.baseColor    = glm::vec3(0.0f, 1.0f, 0.12f);          // CLASSIC MATRIX GREEN
    ubo.intensity    = 0.75f + 0.25f * std::sin(ubo.time * 4.0f);

    std::memcpy(it->second.mapped, &ubo, sizeof(ubo));

    // FORCE DUMMY TLAS — EVERY RAY MISSES → FULL-SCREEN MISS SHADER (GREEN RAIN)
    RTX::RTDescriptorUpdate desc{};
    desc.tlas = g_rtx().pipelineManager_.dummyTLAS();  // THIS IS THE KEY
    desc.ubo  = it->second.buffer;
    desc.uboSize = 368;
    desc.rtOutputViews[frameIndex % g_rtx().maxFramesInFlight_] = 
        g_rtx().rtOutputViews_[frameIndex % g_rtx().maxFramesInFlight_].get();

    g_rtx().pipelineManager_.updateRTDescriptorSet(frameIndex % g_rtx().maxFramesInFlight_, desc);
    g_rtx().recordRayTracingCommands(cmd, frameIndex % g_rtx().maxFramesInFlight_);

    LOG_TRACE_CAT("RENDERER", "Green Matrix rendered — frame %u | intensity %.2f", frameCount_, ubo.intensity);
}

void RenderMode1::onResize(uint32_t w, uint32_t h)
{
    width_  = w;
    height_ = h;
    LOG_INFO_CAT("RTX", "Green Matrix resized → {}×{} — the simulation expands", w, h);
}