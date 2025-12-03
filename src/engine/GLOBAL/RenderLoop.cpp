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
// ZERO TEARING — ONE TRUE RESIZE — TLAS SAFE — PINK PHOTONS ASCEND
// EMPIRE-APPROVED, BATTLE-TESTED, FLAWLESS — FIRST LIGHT ACHIEVED
// =============================================================================

#include "engine/GLOBAL/RenderLoop.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/camera.hpp"

#include <thread>

using StoneKey::stone_window;
using StoneKey::stone_width;
using StoneKey::stone_height;
using StoneKey::stone_device;

using namespace Logging::Color;
using namespace RTX;

// =============================================================================
// RenderLoop — THE ONE TRUE HEART OF THE EMPIRE
// =============================================================================
RenderLoop::RenderLoop(VulkanRenderer& renderer, SDL_Window* window)
    : renderer_(renderer), window_(window), lastFrameTime_(Clock::now())
{
    LOG_AMOURANTH("[RENDERLOOP] RenderLoop forged — {} frames in flight — THE EMPIRE'S HEART BEATS", MAX_FRAMES_IN_FLIGHT);
}

// Destructor is defaulted in header — DO NOT REDEFINE
// ~RenderLoop() = default;  ← already in .hpp

void RenderLoop::run()
{
    LOG_AMOURANTH("[RENDERLOOP] THE ONE TRUE LOOP HAS AWAKENED — FIRST LIGHT ETERNAL — PHOTONS RISE");

    while (running_)
    {
        const auto now = Clock::now();
        const float deltaTime = std::chrono::duration<float>(now - lastFrameTime_).count();
        lastFrameTime_ = now;

        // ── INPUT & WINDOW EVENTS ─────────────────────────────────────────────
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
                case SDL_EVENT_QUIT:
                    LOG_AMOURANTH("[RENDERLOOP] QUIT REQUESTED — GOODBYE, WARRIOR");
                    running_ = false;
                    break;

                case SDL_EVENT_WINDOW_RESIZED:
                case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                {
                    const int w = event.window.data1;
                    const int h = event.window.data2;
                    if (w > 0 && h > 0)
                    {
                        LOG_MAIN("SDL resize event → {}×{} — scheduling safe rebuild", w, h);
                        requestResize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
                    }
                    break;
                }

                case SDL_EVENT_WINDOW_HDR_STATE_CHANGED:
                case SDL_EVENT_DISPLAY_ADDED:
                case SDL_EVENT_DISPLAY_REMOVED:
                    SwapchainManager::handleDisplayHotplug(&event);
                    break;

                default:
                    break;
            }
        }

        // ── RESIZE ORCHESTRATION — ONLY ONE TRUE PATH — NO DUPLICATES ───────
        handlePendingResize();

        // ── FRAME PACING & TLAS SAFETY — CRITICAL ORDER — DO NOT DISTURB ───
        beginFrame();

        // ── RENDER ONE FRAME — ONLY IF VALID SIZE (uses public accessors) ───
        if (!renderer_.minimized() && renderer_.width() > 0 && renderer_.height() > 0)
        {
            renderer_.renderFrame(Camera::get(), deltaTime);
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    // ── FINAL SHUTDOWN — PHOTONS RETURN TO THE VOID IN GRACE ─────────────
    vkDeviceWaitIdle(stone_device());
    LOG_AMOURANTH("[RENDERLOOP] Loop terminated — photons rest in eternal grace — empire sleeps in pink light");
}

void RenderLoop::beginFrame()
{
    const uint32_t slot = currentFrame_;

    vkWaitForFences(stone_device(), 1, renderer_.inFlightFencePtr(slot), VK_TRUE, UINT64_MAX);
    RTX::las().beginFrame();
    vkResetFences(stone_device(), 1, renderer_.inFlightFencePtr(slot));
}

void RenderLoop::handlePendingResize()
{
    if (!resizeRequested_.exchange(false, std::memory_order_acq_rel))
        return;

    const uint32_t w = pendingWidth_.load(std::memory_order_relaxed);
    const uint32_t h = pendingHeight_.load(std::memory_order_relaxed);

    if (w == 0 || h == 0)
        return;

    LOG_AMOURANTH("RESIZE EXECUTED: {}×{} — FULL EMPIRE REBIRTH COMMENCING", w, h);

    vkDeviceWaitIdle(stone_device());
    RTX::las().waitForAllFences();

    renderer_.onWindowResize(w, h);  // ← ONLY THIS PATH IS USED — NO DUPLICATES

    LOG_AMOURANTH("RESIZE COMPLETE — SWAPCHAIN REBORN — ACCUMULATION PURGED — RTX ETERNAL");
}

void RenderLoop::requestResize(uint32_t width, uint32_t height) noexcept
{
    pendingWidth_.store(width, std::memory_order_relaxed);
    pendingHeight_.store(height, std::memory_order_relaxed);
    resizeRequested_.store(true, std::memory_order_release);
}

void RenderLoop::toggleMaximize() noexcept
{
    if (!stone_window()) {
        LOG_WARN_CAT("APP", "toggleMaximize() called with null window — ritual denied");
        return;
    }

    const Uint32 flags = SDL_GetWindowFlags(stone_window());
    const bool isMaximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
    const bool isMinimized = (flags & SDL_WINDOW_MINIMIZED) != 0;

    if (isMinimized) {
        SDL_RestoreWindow(stone_window());
        LOG_INFO_CAT("APP", "Window restored from minimized state");
    }

    if (isMaximized) {
        SDL_RestoreWindow(stone_window());
        LOG_AMOURANTH("WINDOW RESTORED — PHOTONS RETURN TO ORIGINAL FORM");
    } else {
        SDL_MaximizeWindow(stone_window());
        LOG_AMOURANTH("WINDOW MAXIMIZED — PHOTONS SPREAD TO THE EDGES OF THE VOID");
    }

    int w, h;
    SDL_GetWindowSizeInPixels(stone_window(), &w, &h);
    requestResize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));

    LOG_SUCCESS_CAT("APP", "Toggle maximize complete → {}×{}", w, h);
}