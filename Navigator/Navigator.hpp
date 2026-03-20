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

// Global canvas
inline std::unique_ptr<RayCanvas> raycanvas;

// Splash on main renderer (no extra window)
static inline void showSacrificialSplash(SDL_Renderer* renderer, SDL_Window* window) noexcept {
    constexpr const char* TEX_PATH = "assets/textures/ammo.png";
    constexpr const char* ICON_PATH = "assets/textures/ammo.ico";
    constexpr const char* SOUND_PATH = "assets/audio/splash.wav";

    // Set window icon (silly but cute!)
    if (SDL_Surface* iconSurf = IMG_Load(ICON_PATH)) {
        SDL_SetWindowIcon(window, iconSurf);
        SDL_DestroySurface(iconSurf);
    }

    // Load splash texture
    SDL_Texture* tex = nullptr;
    if (SDL_Surface* surf = IMG_Load(TEX_PATH)) {
        tex = SDL_CreateTextureFromSurface(renderer, surf);
        SDL_DestroySurface(surf);
    }

    // Play splash sound
    SDL3System::get().playSound(SOUND_PATH, "play");

    // Render splash
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
        SDL_SetRenderDrawColor(renderer, 255, 20, 147, 255);
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

    // Vulkan init FIRST — claims surface
    if (!initRTX(window, Options::SDL3::DefaultWidth, Options::SDL3::DefaultHeight)) {
        LOG_FATAL_CAT("NAVIGATOR", "Vulkan init failed");
        SDL_DestroyWindow(window);
        SDL3System::get().shutdown();
        return 1;
    }

    // Renderer after Vulkan (try Vulkan backend first, fallback to software for splash)
    SDL_Renderer* renderer = SDL_CreateRenderer(window, "vulkan");
    if (!renderer) {
        LOG_WARNING_CAT("NAVIGATOR", "Vulkan renderer failed — falling back to software");
        renderer = SDL_CreateRenderer(window, "software");
        if (!renderer) {
            LOG_FATAL_CAT("NAVIGATOR", "Renderer creation failed: {}", SDL_GetError());
            cleanupRTX();
            SDL_DestroyWindow(window);
            SDL3System::get().shutdown();
            return 1;
        }
    }

    showSacrificialSplash(renderer, window);

    raycanvas = std::make_unique<RayCanvas>(
        Options::SDL3::DefaultWidth,
        Options::SDL3::DefaultHeight,
        window
    );

    LOG_SUCCESS_CAT("NAVIGATOR", "Engine ready — entering main loop");

    // Main loop — respects window close (X)
    while (true) {
        SDL_Event e;
        bool quitRequested = false;

        while (SDL_PollEvent(&e)) {
            SDL3System::get().pump(e);

            if (e.type == SDL_EVENT_QUIT) {
                LOG_INFO_CAT("NAVIGATOR", "Window close (X) requested — shutting down");
                Pipeline::should_quit = true;
                quitRequested = true;
                break;
            }
        }

        Pipeline::processInput(window,
                               raycanvas ? raycanvas->getWidth()  : Options::SDL3::DefaultWidth,
                               raycanvas ? raycanvas->getHeight() : Options::SDL3::DefaultHeight);

        raycanvas->maybeUpdateCanvas();

        if (quitRequested || raycanvas->isDestroyed() || Pipeline::shouldQuit()) {
            LOG_INFO_CAT("NAVIGATOR", "Exit signal received — shutting down");
            break;
        }
    }

    raycanvas.reset();
    Pipeline::shutdown();
    cleanupRTX();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL3System::get().shutdown();

    LOG_SUCCESS_CAT("NAVIGATOR", "Shutdown complete");
    return 0;
}