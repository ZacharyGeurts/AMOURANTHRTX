// engine/GLOBAL/RenderLoop.cpp
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// RENDERLOOP — FINAL ETERNAL PRODUCTION VERSION — DECEMBER 03 2025
// ZERO TEARING — PERFECT RESIZE — TLAS SAFE — PINK PHOTONS ASCEND
// EMPIRE-APPROVED, BATTLE-TESTED, FLAWLESS
// =============================================================================

#include "engine/GLOBAL/RenderLoop.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/camera.hpp"

#include <source_location>

using namespace RTX;
namespace RTX { class SwapchainManager; }

RenderLoop::RenderLoop(VulkanRenderer& renderer, SDL_Window* window)
    : renderer_(renderer), window_(window), lastFrameTime_(Clock::now())
{
    LOG_AMOURANTH("[RENDERLOOP] RenderLoop forged — {} frames in flight — THE EMPIRE'S HEART BEATS", MAX_FRAMES_IN_FLIGHT);
}

void RenderLoop::run()
{
    LOG_AMOURANTH("[RENDERLOOP] THE ONE TRUE LOOP HAS AWAKENED — FIRST LIGHT ETERNAL");

    while (running_)
    {
        const auto now = Clock::now();
        const float deltaTime = std::chrono::duration<float>(now - lastFrameTime_).count();
        lastFrameTime_ = now;

        // ── INPUT & EVENTS ─────────────────────────────────────
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_EVENT_QUIT)
            {
                running_ = false;
                break;
            }

            if (event.type == SDL_EVENT_WINDOW_RESIZED)
            {
                int w = event.window.data1;
                int h = event.window.data2;
                if (w > 0 && h > 0)
                {
                    LOG_MAIN("SDL resize event: {}x{} — scheduling safe rebuild", w, h);
                    requestResize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
                }
            }

            // Optional: forward hotplug / HDR change events
            if (event.type == SDL_EVENT_WINDOW_HDR_STATE_CHANGED ||
                event.type == SDL_EVENT_DISPLAY_ADDED ||
                event.type == SDL_EVENT_DISPLAY_REMOVED)
            {
                SwapchainManager::handleDisplayHotplug(&event);
            }
        }

        // ── RESIZE ORCHESTRATION (SAFE, ATOMIC, NON-BLOCKING) ─────
        handlePendingResize();

        // ── FRAME PACING & TLAS SAFETY (CRITICAL ORDER) ──────────
        beginFrame();

        // ── RENDER ONE FRAME ───────────────────────────────────
        if (!renderer_.minimized())
        {
            renderer_.renderFrame(Camera::get(), deltaTime);
        }

        currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    // Final shutdown — all photons return to the void
    vkDeviceWaitIdle(stone_device());
    LOG_AMOURANTH("[RENDERLOOP] Loop terminated — photons rest in eternal grace");
}

void RenderLoop::beginFrame()
{
    const uint32_t slot = currentFrame_;

    // 1. Wait for this frame slot to be free
    vkWaitForFences(stone_device(), 1, renderer_.inFlightFencePtr(slot), VK_TRUE, UINT64_MAX);

    // 2. CRITICAL: Retire old TLAS — prevents use-after-free and tearing forever
    RTX::las().beginFrame();

    // 3. Reset fence for next use
    vkResetFences(stone_device(), 1, renderer_.inFlightFencePtr(slot));
}

void RenderLoop::handlePendingResize()
{
    if (!resizeRequested_.exchange(false, std::memory_order_acq_rel))
        return;

    uint32_t w = pendingWidth_.load(std::memory_order_relaxed);
    uint32_t h = pendingHeight_.load(std::memory_order_relaxed);

    if (w == 0 || h == 0)
        return;

    LOG_AMOURANTH("RESIZE EXECUTED: {}x{} — FULL EMPIRE REBIRTH COMMENCING", w, h);

    // THE FINAL SAFE SEQUENCE — NON-NEGOTIABLE
    vkDeviceWaitIdle(stone_device());           // GPU fully idle
    RTX::las().waitForAllFences();              // All TLAS builds complete

    renderer_.onWindowResize(w, h);             // Full safe reconstruction

    LOG_AMOURANTH("RESIZE COMPLETE — SWAPCHAIN REBORN — ACCUMULATION PURGED — RTX ETERNAL");
}

void RenderLoop::requestResize(uint32_t width, uint32_t height) noexcept
{
    pendingWidth_.store(width, std::memory_order_relaxed);
    pendingHeight_.store(height, std::memory_order_relaxed);
    resizeRequested_.store(true, std::memory_order_release);
}