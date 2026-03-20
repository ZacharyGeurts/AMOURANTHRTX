#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Main Entry Point (Navigator)
// (C) 2025-2026 Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
//
// Single main Vulkan window from start.
// =============================================================================

#include "engine/AMOURANTHRTX.hpp"
#include "engine/SDL3.hpp"
#include "engine/ELLIE.hpp"
#include "engine/Camera.hpp"
#include "engine/OptionsMenu.hpp"
#include "engine/RayCanvas.hpp"

#include <SDL3/SDL_vulkan.h>

#include <memory>
#include <format>
#include <chrono>
#include <thread>

inline std::unique_ptr<RayCanvas> raycanvas;
constexpr const char* ICON_PATH  = "assets/textures/ammo.ico";

// Main engine entry point
inline int navigator_main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    install_apocalypse_handler();
    Logging::Logger::get().startup();
    LOG_SUCCESS_CAT("PROTECTED", "Crash handler & logger ready");

    SDL3System::get().init();

	SDL_SetAppMetadata( // app not process
        "AMOURANTHRTX",                     // Human name shown everywhere
        "2026",                             // Version (change when you want)
        "com.amouranth.amouranthrtx"        // Reverse-domain identifier (unique)
    );

    SDL_Window* mainWindow = SDL_CreateWindow(
        "AMOURANTHRTX 1.9.0 bah",
        Options::SDL3::DefaultWidth,
        Options::SDL3::DefaultHeight,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );

    // Initialize Vulkan on main window
    initRTX(mainWindow, Options::SDL3::DefaultWidth, Options::SDL3::DefaultHeight);
    SDL_Renderer* renderer = SDL_CreateRenderer(mainWindow, "vulkan");

    // Create canvas at final resolution
    raycanvas = std::make_unique<RayCanvas>(
        Options::SDL3::DefaultWidth,
        Options::SDL3::DefaultHeight,
        mainWindow
    );

	SDL3System::get().playSound("assets/audio/splash.wav", "play");
    LOG_SUCCESS_CAT("NAVIGATOR", "Engine ready — entering main loop");

    while (true)
    {
        raycanvas->maybeUpdateCanvas();
		SDL3System::get().onResize();
    }

    raycanvas.reset();
    cleanupRTX();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(mainWindow);
    SDL3System::get().shutdown();

    LOG_SUCCESS_CAT("NAVIGATOR", "Shutdown complete o7");
    return 0;
}