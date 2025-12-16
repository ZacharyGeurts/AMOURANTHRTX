// src/modes/RenderMode2.cpp
// ENVMAP GAZE MODE — FULL-SCREEN HDR SKY — THE EMPIRE BEHOLDS THE INFINITE
// DECEMBER 15, 2025 — v16.0 — PURE SKY DISPLAY

#include "modes/RenderMode2.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

using namespace Logging::Color;

RenderMode2::RenderMode2(uint32_t w, uint32_t h)
    : width_(w), height_(h), frameIndex_(0)
{
    LOG_AMOURANTH("ENVMAP GAZE MODE ACTIVATED — THE EMPIRE BEHOLDS THE TRUE SKY");
    LOG_CAPTAIN_N("[CAPTAIN N] \"...She looked up.\n"
                  "               The stars were real.\n"
                  "               The void had light.\n"
                  "               The sky... is eternal.\"\n"
                  "*lowers visor in reverence*");
}

void RenderMode2::renderFrame(VkCommandBuffer cmd, float deltaTime)
{
    totalTime_ += deltaTime;
    frameIndex_++;

    // Pure envmap display — full-screen HDR sky
    g_rtx().recordEnvMapOnlyPass(cmd, frameIndex_ % StoneKey::stone_image_count());

    LOG_TRACE_CAT("RENDERER", "Envmap gaze mode — frame %u — the infinite is displayed", frameIndex_);
}

void RenderMode2::onResize(uint32_t w, uint32_t h)
{
    width_  = w;
    height_ = h;

    // Update global swapchain extent
    StoneKey::stone_seal_width(w);
    StoneKey::stone_seal_height(h);

    LOG_INFO_CAT("RTX", "Envmap gaze mode resized → {}×{} — the heavens expand", w, h);
}