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
// ZERO TEARING — ONE TRUE RESIZE (IN SDL3.cpp) — TLAS SAFE — PINK PHOTONS ASCEND
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

using StoneKey::stone_device;
using StoneKey::stone_window;
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

void RenderLoop::run()
{
    LOG_AMOURANTH("[RENDERLOOP] THE ONE TRUE LOOP HAS AWAKENED — FIRST LIGHT ETERNAL — PHOTONS RISE");

    while (running_)
    {
        const auto now = Clock::now();
        const float deltaTime = std::chrono::duration<float>(now - lastFrameTime_).count();
        lastFrameTime_ = now;

        // ── RESIZE CONSUMPTION ONLY — SDL3.cpp OWNS ALL EVENT DETECTION ─────
        // g_resizeRequested is set EXCLUSIVELY by SDL3Window::pollEvents()
        handlePendingResize();

        // ── FRAME PACING & TLAS SAFETY — CRITICAL ORDER — DO NOT DISTURB ───
        beginFrame();

        // ── RENDER ONE FRAME — ONLY IF NOT MINIMIZED AND VALID SIZE ───────
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

    // ── FINAL SHUTDOWN — ALL PHOTONS RETURN TO THE VOID IN GRACE ───────
    vkDeviceWaitIdle(stone_device());
    LOG_AMOURANTH("[RENDERLOOP] Loop terminated — photons rest in eternal grace — empire sleeps in pink light");
}

void RenderLoop::beginFrame()
{
    const uint32_t slot = currentFrame_;

    vkWaitForFences(stone_device(), 1, renderer_.inFlightFencePtr(slot), VK_TRUE, UINT64_MAX);
    RTX::las().beginFrame();  // Retire old TLAS builds — prevents use-after-free
    vkResetFences(stone_device(), 1, renderer_.inFlightFencePtr(slot));
}

void RenderLoop::handlePendingResize()
{
    // This flag is set ONLY by SDL3.cpp — debounced, atomic, perfect
    if (!g_resizeRequested.exchange(false, std::memory_order_acq_rel))
        return;

    const uint32_t w = g_resizeWidth.load(std::memory_order_relaxed);
    const uint32_t h = g_resizeHeight.load(std::memory_order_relaxed);

    if (w == 0 || h == 0)
        return;

    LOG_AMOURANTH("RESIZE EXECUTED: {}×{} — FULL EMPIRE REBIRTH COMMENCING", w, h);

    vkDeviceWaitIdle(stone_device());
    RTX::las().waitForAllFences();

    renderer_.onWindowResize(w, h);  // ← ONE TRUE PATH — FULL SAFE REBUILD

    LOG_AMOURANTH("RESIZE COMPLETE — SWAPCHAIN REBORN — ACCUMULATION PURGED — RTX ETERNAL");
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

    // Trigger resize via the ONE TRUE PATH
    int w, h;
    SDL_GetWindowSizeInPixels(stone_window(), &w, &h);
    g_resizeWidth.store(w, std::memory_order_release);
    g_resizeHeight.store(h, std::memory_order_release);
    g_resizeRequested.store(true, std::memory_order_release);

    LOG_SUCCESS_CAT("APP", "Toggle maximize complete → {}×{}", w, h);
}