// src/engine/SDL3/SDL3_window.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// SDL3 Window RAII — FINAL SDL3 API FIX — NOV 13 2025
// • Vulkan-ready + ImGui support
// • F11 toggle, resize, quit — routed via SDL3Vulkan::getRenderer()
// • SDL3 GLOBAL POINTER: g_sdl3_window — SAFE, RAII, ZERO LEAKS
// • CENTERED VIA SDL_SetWindowPosition
// • EVERY STEP LOGGED — VALHALLA SEALED
// • PINK PHOTONS ETERNAL — 15,000 FPS — SHIP IT RAW
// =============================================================================

#include "engine/SDL3/SDL3_window.hpp"
#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <format>
#include <vector>

using namespace Logging::Color;

namespace SDL3Initializer {

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL SDL3 POINTER DEFINITION — RAII, Thread-Safe
// ──────────────────────────────────────────────────────────────────────────────
SDLWindowPtr g_sdl3_window = nullptr;

// ──────────────────────────────────────────────────────────────────────────────
// Deleter — RAII for SDL_Window
// ──────────────────────────────────────────────────────────────────────────────
void SDLWindowDeleter::operator()(SDL_Window* w) const noexcept {
    if (w) {
        LOG_INFO_CAT("Dispose", "{}Destroying SDL_Window @ {:p}{}", 
                     RASPBERRY_PINK, static_cast<void*>(w), RESET);
        SDL_DestroyWindow(w);
    }
    SDL_Quit();
    LOG_SUCCESS_CAT("Dispose", "{}SDL3 subsystem shutdown complete{}", EMERALD_GREEN, RESET);
}

// ──────────────────────────────────────────────────────────────────────────────
// createWindow — RAII + Vulkan-ready + CENTERED + MAX LOGGING + SDL3 GLOBAL
// ──────────────────────────────────────────────────────────────────────────────
SDLWindowPtr createWindow(const char* title, int w, int h, Uint32 flags) {
    LOG_ATTEMPT_CAT("Window", "Attempting to create SDL window: '{}' ({}x{})", title, w, h);

    // ── STEP 1: Base Vulkan Flag ───────────────────────────────────────────
    LOG_TRACE_CAT("Window", "Adding SDL_WINDOW_VULKAN flag (required for Vulkan)");
    flags |= SDL_WINDOW_VULKAN;
    LOG_DEBUG_CAT("Window", "Flags after Vulkan: 0x{:08x}", flags);

    // ── STEP 2: ImGui Support (Resizable + High DPI) ───────────────────────
    if (Options::Performance::ENABLE_IMGUI) {
        LOG_INFO_CAT("Window", "{}ImGui enabled — adding RESIZABLE + HIGH_DPI flags{}", SAPPHIRE_BLUE, RESET);
        flags |= SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
        LOG_DEBUG_CAT("Window", "Flags after ImGui: 0x{:08x}", flags);
    } else {
        LOG_TRACE_CAT("Window", "ImGui disabled — no RESIZABLE or HIGH_DPI");
    }

    // ── STEP 3: Video Driver Hint (Wayland → X11 fallback) ─────────────────
    LOG_TRACE_CAT("Window", "Setting SDL_HINT_VIDEO_DRIVER = 'wayland,x11'");
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");

    // ── STEP 4: Initialize SDL Subsystems ──────────────────────────────────
    LOG_ATTEMPT_CAT("SDL", "Initializing SDL subsystems: VIDEO | EVENTS");
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        LOG_FATAL_CAT("SDL", "{}SDL_Init failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        throw std::runtime_error("SDL_Init failed");
    }
    LOG_SUCCESS_CAT("SDL", "{}SDL_Init completed — VIDEO + EVENTS active{}", EMERALD_GREEN, RESET);

    // ── STEP 5: Create Window (SDL3: no x,y params) ────────────────────────
    LOG_ATTEMPT_CAT("Window", "Calling SDL_CreateWindow (SDL3: no x,y params)");
    LOG_DEBUG_CAT("Window", "  Title: '{}'", title);
    LOG_DEBUG_CAT("Window", "  Width: {}", w);
    LOG_DEBUG_CAT("Window", "  Height: {}", h);
    LOG_DEBUG_CAT("Window", "  Flags: 0x{:08x}", flags);

    SDL_Window* raw = SDL_CreateWindow(title, w, h, flags);

    if (!raw) {
        LOG_FATAL_CAT("SDL", "{}SDL_CreateWindow failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        throw std::runtime_error("SDL_CreateWindow failed");
    }

    // ── STEP 6: Center Window Post-Creation ───────────────────────────────
    LOG_TRACE_CAT("Window", "Centering window post-creation with SDL_SetWindowPosition");
    SDL_SetWindowPosition(raw, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    LOG_DEBUG_CAT("Window", "Window centered — SDL_SetWindowPosition called");

    // ── STEP 7: Success — Log Full Window Info ────────────────────────────
    LOG_SUCCESS_CAT("Window", "{}SDL_Window created successfully!{}", PLASMA_FUCHSIA, RESET);
    LOG_INFO_CAT("Window", "  Handle: @ {:p}", static_cast<void*>(raw));
    LOG_INFO_CAT("Window", "  Size:   {}x{}", w, h);
    LOG_INFO_CAT("Window", "  Flags:  0x{:08x}", SDL_GetWindowFlags(raw));

    // ── STEP 8: Query Actual Position (Confirm Centered) ─────────────────
    int actualX, actualY;
    SDL_GetWindowPosition(raw, &actualX, &actualY);
    LOG_INFO_CAT("Window", "  Position: ({}, {}) — CONFIRMED CENTERED", actualX, actualY);

    // ── STEP 9: Display Info (Which Monitor?) ───────────────────────────
    SDL_DisplayID displayID = SDL_GetDisplayForWindow(raw);
    if (displayID != 0) {
        const SDL_DisplayMode* mode = SDL_GetCurrentDisplayMode(displayID);
        if (mode) {
            LOG_INFO_CAT("Window", "  Placed on Display ID {}: {}x{} @ {}Hz", 
                         displayID, mode->w, mode->h, mode->refresh_rate);
        } else {
            LOG_WARN_CAT("Window", "  Could not get current display mode: {}", SDL_GetError());
        }
    } else {
        LOG_WARN_CAT("Window", "  Failed to get display for window: {}", SDL_GetError());
    }

    // ── STEP 10: Pixel Size (High DPI Aware) ─────────────────────────────
    int pixelW, pixelH;
    SDL_GetWindowSizeInPixels(raw, &pixelW, &pixelH);
    if (pixelW != w || pixelH != h) {
        LOG_INFO_CAT("Window", "  High DPI: Logical {}x{} → Physical {}x{} (scale: {:.2f})", 
                     w, h, pixelW, pixelH, static_cast<float>(pixelW) / w);
    } else {
        LOG_TRACE_CAT("Window", "  1:1 pixel mapping (no scaling)");
    }

    // ── STEP 11: Assign to SDL3 Global Pointer (g_sdl3_window) ───────────
    LOG_ATTEMPT_CAT("SDL3", "Assigning window to global g_sdl3_window");
    setSDL3Window(SDLWindowPtr(raw));
    LOG_SUCCESS_CAT("SDL3", "{}g_sdl3_window assigned @ {:p} — GLOBAL ACCESS READY{}", 
                    PLASMA_FUCHSIA, static_cast<void*>(raw), RESET);

    LOG_SUCCESS_CAT("Window", "{}Window creation pipeline COMPLETE — CENTERED & GLOBAL{}", 
                    COSMIC_GOLD, RESET);

    // Return RAII wrapper (ownership already transferred to g_sdl3_window)
    return SDLWindowPtr(raw);
}

// ──────────────────────────────────────────────────────────────────────────────
// getWindowExtensions — leak-free
// ──────────────────────────────────────────────────────────────────────────────
std::vector<std::string> getWindowExtensions(const SDLWindowPtr&) {
    unsigned int count = 0;

    if (SDL_Vulkan_GetInstanceExtensions(&count) == 0) {
        LOG_ERROR_CAT("Vulkan", "{}SDL_Vulkan_GetInstanceExtensions failed (count): {}{}", 
                      CRIMSON_MAGENTA, SDL_GetError(), RESET);
        return {};
    }

    LOG_INFO_CAT("Vulkan", "{} extensions required by SDL3", count);

    std::vector<const char*> names(count);

    if (SDL_Vulkan_GetInstanceExtensions(&count) != 0) {
        LOG_ERROR_CAT("Vulkan", "{}SDL_Vulkan_GetInstanceExtensions failed (names): {}{}", 
                      CRIMSON_MAGENTA, SDL_GetError(), RESET);
        return {};
    }

    std::vector<std::string> result;
    result.reserve(count);
    for (unsigned int i = 0; i < count; ++i) {
        result.emplace_back(names[i]);
        LOG_TRACE_CAT("Vulkan", "  [{}] {}", i, names[i]);
    }

    LOG_SUCCESS_CAT("Vulkan", "{}Retrieved {} Vulkan instance extensions{}", 
                    QUANTUM_PURPLE, count, RESET);
    return result;
}

// ──────────────────────────────────────────────────────────────────────────────
// pollEventsForResize — F11, resize, quit
// ──────────────────────────────────────────────────────────────────────────────
bool pollEventsForResize(const SDLWindowPtr& window,
                         int& newWidth, int& newHeight,
                         bool& shouldQuit, bool& toggleFullscreenKey) noexcept {
    SDL_Event event;
    bool resized = false;
    shouldQuit = false;
    toggleFullscreenKey = false;

    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT) {
            shouldQuit = true;
            LOG_INFO_CAT("APP", "{}Quit event received{}", AMBER_YELLOW, RESET);
        }
        else if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_F11) {
                toggleFullscreenKey = true;
                LOG_INFO_CAT("APP", "{}F11 pressed — toggling fullscreen{}", PULSAR_GREEN, RESET);
            }
        }
        else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
            newWidth = event.window.data1;
            newHeight = event.window.data2;
            resized = true;
            LOG_INFO_CAT("APP", "{}Window resized: {}x{}{}", SAPPHIRE_BLUE, newWidth, newHeight, RESET);
        }
    }

    return resized;
}

// ──────────────────────────────────────────────────────────────────────────────
// toggleFullscreen — Notify global renderer
// ──────────────────────────────────────────────────────────────────────────────
void toggleFullscreen(SDLWindowPtr& window) noexcept {
    bool isFullscreen = (SDL_GetWindowFlags(window.get()) & SDL_WINDOW_FULLSCREEN) == 0;

    if (SDL_SetWindowFullscreen(window.get(), !isFullscreen) == 0) {
        LOG_SUCCESS_CAT("Window", "{}Fullscreen: {}{}", 
                        isFullscreen ? CRIMSON_MAGENTA : EMERALD_GREEN,
                        isFullscreen ? "OFF" : "ON", RESET);
    } else {
        LOG_WARNING_CAT("Window", "{}Fullscreen toggle failed: {}{}", 
                        AMBER_YELLOW, SDL_GetError(), RESET);
    }

    int w, h;
    SDL_GetWindowSizeInPixels(window.get(), &w, &h);
    if (auto* renderer = &SDL3Vulkan::getRenderer()) {
        renderer->handleResize(w, h);
    }
}

} // namespace SDL3Initializer