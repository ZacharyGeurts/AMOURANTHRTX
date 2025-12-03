// engine/GLOBAL/RenderLoop.cpp
// =============================================================================
// RENDERLOOP — FINAL ETERNAL PRODUCTION VERSION — DECEMBER 03 2025
// ZERO TEARING — PERFECT RESIZE — TLAS SAFE — PINK PHOTONS ASCEND
// =============================================================================

#include "engine/GLOBAL/RenderLoop.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/camera.hpp"

#include <source_location> 

using namespace RTX;

RenderLoop::RenderLoop(VulkanRenderer& renderer, SDL_Window* window)
    : renderer_(renderer), window_(window), lastFrameTime_(Clock::now())
{
    LOG_SUCCESS_CAT("RENDERLOOP", "RenderLoop forged — {} frames in flight — THE EMPIRE'S HEART BEATS", MAX_FRAMES_IN_FLIGHT);
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
                running_ = false;

            if (event.type == SDL_EVENT_WINDOW_RESIZED)
            {
                int w = event.window.data1;
                int h = event.window.data2;
                if (w > 0 && h > 0)
                    requestResize(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
            }
        }

        // ── RESIZE ORCHESTRATION ───────────────────────────────
        handlePendingResize();

        // ── FRAME PACING & TLAS SAFETY ─────────────────────────
        beginFrame();

        // ── RENDER ONE FRAME ───────────────────────────────────
        if (!renderer_.minimized())
            renderer_.renderFrame(Camera::get(), deltaTime);

        currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    vkDeviceWaitIdle(stone_device());
    LOG_AMOURANTH("[RENDERLOOP] Loop terminated — photons rest in eternal grace");
}

void RenderLoop::beginFrame()
{
    // 1. Wait for previous frame on this slot
    vkWaitForFences(stone_device(), 1, renderer_.inFlightFencePtr(currentFrame_), VK_TRUE, UINT64_MAX);

    // 2. CRITICAL: Retire old TLAS slot — prevents tearing forever
    RTX::las().beginFrame();

    // 3. Reset fence for next use
    vkResetFences(stone_device(), 1, renderer_.inFlightFencePtr(currentFrame_));
}

void RenderLoop::handlePendingResize()
{
    if (!resizeRequested_.exchange(false, std::memory_order_acquire))
        return;

    uint32_t w = pendingWidth_.load(std::memory_order_relaxed);
    uint32_t h = pendingHeight_.load(std::memory_order_relaxed);

    if (w == 0 || h == 0)
        return;

    LOG_SUCCESS_CAT("RESIZE", "RenderLoop executing full resize: {}x{}", w, h);

    // THE FINAL SAFE SEQUENCE
    vkDeviceWaitIdle(stone_device());
    RTX::las().waitForAllFences();  // TLAS builds cannot touch old resources

    renderer_.onWindowResize(w, h);

    LOG_SUCCESS_CAT("RESIZE", "Resize complete — swapchain reborn — RTX ETERNAL");
}

void RenderLoop::requestResize(uint32_t width, uint32_t height) noexcept
{
    pendingWidth_.store(width, std::memory_order_relaxed);
    pendingHeight_.store(height, std::memory_order_relaxed);
    resizeRequested_.store(true, std::memory_order_release);
}