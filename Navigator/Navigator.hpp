#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Main Entry Point (Navigator)
// (C) 2025-2026 Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
// =============================================================================

#include "engine/AMOURANTHRTX.hpp"
#include "engine/SDL3.hpp"
#include "engine/ELLIE.hpp"
#include "engine/Camera.hpp"
#include "engine/OptionsMenu.hpp"
#include "engine/RayCanvas.hpp"
#include "engine/Pipeline.hpp"

#include <SDL3/SDL_vulkan.h>
#include <SDL3_image/SDL_image.h>

#include <memory>
#include <format>
#include <chrono>

// Global canvas. Your TV Screen or Monitor
inline std::unique_ptr<RayCanvas> raycanvas;

static inline void showSplashPopup(SDL_Renderer* renderer, SDL_Window* window) noexcept {
    constexpr const char* TEX_PATH = "assets/textures/ammo.png";
    constexpr const char* ICON_PATH = "assets/textures/ammo.ico";
    constexpr const char* SOUND_PATH = "assets/audio/splash.wav";

    int MyAudioSlot = SDL3System::get().playSound(SOUND_PATH, "play");

	// window comes from navigator_main, below
    if (SDL_Surface* iconSurf = IMG_Load(ICON_PATH)) {
        SDL_SetWindowIcon(window, iconSurf);
        SDL_DestroySurface(iconSurf);
    }

    SDL_Texture* tex = nullptr;
    if (SDL_Surface* surf = IMG_Load(TEX_PATH)) {
        tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_DestroySurface(surf);
    }

    // Recommended to start with transparent
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    if (tex) {
        float tw = 0, th = 0;
        SDL_GetTextureSize(tex, &tw, &th);
        SDL_FRect dst{
            (static_cast<float>(Options::SDL3::DefaultWidth) - tw) * 0.5f,
            (static_cast<float>(Options::SDL3::DefaultHeight) - th) * 0.5f,
            tw, th
        };
        SDL_RenderTexture(renderer, tex, nullptr, &dst);
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 20, 147, 255); // fallback color
        SDL_RenderClear(renderer);
    }

    SDL_RenderPresent(renderer);

    // Wait for timeout or input
    double start = TotalTime::get().seconds();
    bool skip = false;

    while (TotalTime::get().seconds() - start < 3.0 && !skip) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            SDL3System::get().pump(e);

            if (e.type == SDL_EVENT_QUIT ||
                e.type == SDL_EVENT_KEY_DOWN ||
                e.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN ||
                e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
                skip = true;
                break;
            }
        }
        SDL_Delay(1);
    }

    if (tex) SDL_DestroyTexture(tex);

    SDL3System::get().playSound(SOUND_PATH, "stop", MyAudioSlot); // 3rd arg -1 stops all slots

    LOG_SUCCESS_CAT("SPLASH", "Splash completed (skipped={})", skip ? "yes" : "no");
}

// Main engine entry point
inline int navigator_main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {

    install_apocalypse_handler();
    Logging::Logger::get().startup();
    LOG_SUCCESS_CAT("NAVIGATOR", "Crash handler & logger ready");

    if (!SDL3System::get().init(nullptr)) {
        LOG_FATAL_CAT("NAVIGATOR", "SDL3 init failed");
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "AMOURANTHRTX",
        Options::SDL3::DefaultWidth,
        Options::SDL3::DefaultHeight,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    if (!window) {
        LOG_FATAL_CAT("NAVIGATOR", "Window creation failed: {}", SDL_GetError());
        SDL3System::get().shutdown();
        return 1;
    }

    if (!initRTX(window, Options::SDL3::DefaultWidth, Options::SDL3::DefaultHeight)) {
        LOG_FATAL_CAT("NAVIGATOR", "Vulkan init failed");
        SDL_DestroyWindow(window);
        SDL3System::get().shutdown();
        return 1;
    }

    // Renderer after Vulkan (try Vulkan backend first, fallback to software for splash)
    SDL_Renderer* renderer = SDL_CreateRenderer(window, "vulkan");
    if (!renderer) {
        LOG_FATAL_CAT("NAVIGATOR", "Renderer creation failed: {}", SDL_GetError());
        cleanupRTX();
        SDL_DestroyWindow(window);
        SDL3System::get().shutdown();
        return 1;
    }

    showSplashPopup(renderer, window);

    raycanvas = std::make_unique<RayCanvas>( // Your TV or Monitor
        Options::SDL3::DefaultWidth,
        Options::SDL3::DefaultHeight,
        window
    );

    LOG_SUCCESS_CAT("NAVIGATOR", "Engine ready — entering main loop");

    // Main loop
	bool isRunning = true;
    while (isRunning) { isRunning = raycanvas->maybeUpdateCanvas(isRunning); }

	// Shut down
    raycanvas.reset();
    Pipeline::shutdown();
    cleanupRTX();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL3System::get().shutdown();

    LOG_SUCCESS_CAT("NAVIGATOR", "Shutdown complete");
    return 0;
}