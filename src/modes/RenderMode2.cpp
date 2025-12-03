// =============================================================================
// src/modes/RenderMode2.cpp
// =============================================================================
// RENDERMODE 2 — PURE RAYGEN + MISS — BINDING 31 — PINK PHOTONS ETERNAL
// No scene. No accumulation. Only the sacred pink void.
// First light achieved. The photons obey.
// =============================================================================

#include "modes/RenderMode2.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"   // ← g_rtx()

using namespace Logging::Color;

RenderMode2::RenderMode2(uint32_t width, uint32_t height)
    : width_(width), height_(height), frameCount_(0)
{
    LOG_INFO_CAT("RTX", "MODE 2 — PURE RAYGEN + MISS — BINDING 31 IGNITED — {}x{}", width, height);
    LOG_SUCCESS_CAT("RTX", "NO TLAS. NO GEOMETRY. ONLY PHOTONS. ONLY TRUTH. ONLY PINK.");
}

void RenderMode2::updateUniforms(float)
{
    alignas(16) struct PinkRaygenCommand {
        alignas(16) glm::vec4 cameraPos;
        alignas(16) glm::mat4 viewProj;
        alignas(16) glm::vec4 jitter;
        uint64_t     uKey1           = 0x9E37AF18C64D8A17UL;
        uint64_t     uKey2           = 0xE4F8B29D71A3C56CUL;
        uint64_t     uObfuscator     = 0x9E37AF18C64D8A17UL ^ 0xE4F8B29D71A3C56CUL ^ 0x1337C0DE69F00D42UL;
        uint64_t     uPinkRaygenMode = 2ULL;
        uint32_t     frame           = 0;
        uint32_t     spp             = 0;
        float        time            = 0.0f;
        uint32_t     _pad[1]         = {0};
    } cmd{};

    // NOW WE CAN SAFELY USE MEMBER VARIABLES
    float aspect = static_cast<float>(width_) / static_cast<float>(height_);
    cmd.viewProj = glm::perspective(glm::radians(60.0f), aspect, 0.1f, 1000.0f);
    cmd.cameraPos = glm::vec4(0.0f, 0.0f, -5.0f, 1.0f);
    cmd.jitter = glm::vec4(0.0f);
    cmd.frame = static_cast<uint32_t>(frameCount_);
    cmd.spp   = static_cast<uint32_t>(frameCount_ + 1);
    cmd.time  = static_cast<float>(frameCount_) * 0.016f;

    g_rtx().updateUniformBinding31(&cmd, sizeof(cmd));
}

void RenderMode2::traceRays(VkCommandBuffer cmd)
{
    // No TLAS needed — we use dummy TLAS or null
    // recordRayTrace will handle null TLAS gracefully (fallback to miss shader)
    g_rtx().recordRayTrace(cmd, {width_, height_});
}

void RenderMode2::renderFrame(VkCommandBuffer cmd, float deltaTime)
{
    updateUniforms(deltaTime);
    traceRays(cmd);
    ++frameCount_;
}

void RenderMode2::onResize(uint32_t width, uint32_t height)
{
    if (width == width_ && height == height_) return;

    width_  = width;
    height_ = height;
    frameCount_ = 0;

    LOG_INFO_CAT("RTX", "MODE 2 — RESIZED TO {}x{} — PINK RAYGEN VOID REMAINS ETERNAL", width, height);
}