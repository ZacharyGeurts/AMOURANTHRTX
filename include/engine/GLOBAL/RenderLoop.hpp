// engine/GLOBAL/RenderLoop.hpp
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// AMOURANTH RTX — RENDERLOOP v∞ — FIRST LIGHT ETERNAL — DECEMBER 03 2025
// THE ONE TRUE LOOP — SEPARATE, CLEAN, ETERNAL
// =============================================================================

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include "engine/GLOBAL/OptionsMenu.hpp"

struct VulkanRenderer;
struct SDL_Window;

namespace RTX {

class RenderLoop {
public:
    RenderLoop(VulkanRenderer& renderer, SDL_Window* window);
    ~RenderLoop() = default;

    void run();
    void stop() noexcept { running_ = false; }

    // Thread-safe resize request from anywhere
    void requestResize(uint32_t width, uint32_t height) noexcept;
	void toggleMaximize() noexcept;

private:
    void beginFrame();
    void handlePendingResize();

    VulkanRenderer& renderer_;
    SDL_Window*     window_;

    std::atomic<bool> running_{true};
    std::atomic<bool> resizeRequested_{false};
    std::atomic<uint32_t> pendingWidth_{0};
    std::atomic<uint32_t> pendingHeight_{0};

    uint32_t currentFrame_ = 0;
    const uint32_t MAX_FRAMES_IN_FLIGHT = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    using Clock = std::chrono::steady_clock;
    Clock::time_point lastFrameTime_;
};

} // namespace RTX