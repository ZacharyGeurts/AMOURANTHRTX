// src/modes/RenderMode1.cpp
// =============================================================================
// RenderMode1 — PURE PINK VOID — MISS SHADER ONLY — BINDING 31 ACTIVE
// No scene. No geometry. Only infinite pink photons.
// =============================================================================

#include "modes/RenderMode1.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"   // ← needed for g_rtx()

using namespace Logging::Color;

RenderMode1::RenderMode1(uint32_t width, uint32_t height)
    : width_(width), height_(height), frameCount_(0)
{
    LOG_INFO_CAT("RTX", "MODE 1 — PURE MISS SHADER VOID — BINDING 31 ENGAGED — {}x{}", width, height);
    LOG_SUCCESS_CAT("RTX", "NO GEOMETRY. NO ACCUMULATION. ONLY PINK PHOTONS. ONLY TRUTH.");
}

RenderMode1::~RenderMode1() = default;

void RenderMode1::updateUniforms(float)
{
    struct PinkVoidCommand {
        uint64_t uKey1       = 0x9E37AF18C64D8A17UL;
        uint64_t uKey2       = 0xE4F8B29D71A3C56CUL;
        uint64_t uObfuscator = 0x9E37AF18C64D8A17UL ^ 0xE4F8B29D71A3C56CUL ^ 0x1337C0DE69F00D42UL;
        uint64_t uPinkVoid   = 1ULL;
        uint32_t frame       = 0;          // ← will be filled below
        uint32_t _pad[3]     = {0};
    } cmd{};

    cmd.frame = frameCount_;               // ← now valid: member variable
    g_rtx().updateUniformBinding31(&cmd, sizeof(cmd));  // ← g_rtx() is the global accessor
}

void RenderMode1::traceRays(VkCommandBuffer cmd)
{
    // Uses the global renderer — swapchain image is already bound as storage image
    g_rtx().recordRayTrace(cmd, {width_, height_});
}

void RenderMode1::renderFrame(VkCommandBuffer cmd, float deltaTime)
{
    updateUniforms(deltaTime);
    traceRays(cmd);
    ++frameCount_;
}

void RenderMode1::onResize(uint32_t width, uint32_t height)
{
    if (width == width_ && height == height_) return;
    width_ = width;
    height_ = height;
    frameCount_ = 0;
    LOG_INFO_CAT("RTX", "MODE 1 — RESIZED TO {}x{} — PINK VOID REMAINS ETERNAL", width, height);
}