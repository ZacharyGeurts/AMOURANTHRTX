// src/engine/SDL3/SDL3_window.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
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
// createWindow — RAII + Vulkan-ready
// ──────────────────────────────────────────────────────────────────────────────
SDLWindowPtr createWindow(const char* title, int w, int h, Uint32 flags) {
    LOG_ATTEMPT_CAT("Window", "Creating SDL window: '{}' ({}x{})", title, w, h);

    flags |= SDL_WINDOW_VULKAN;

    if (Options::Performance::ENABLE_IMGUI) {
        flags |= SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
        LOG_INFO_CAT("Window", "{}ImGui enabled — RESIZABLE + HIGH_DPI flags{}", 
                     SAPPHIRE_BLUE, RESET);
    }

    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        LOG_ERROR_CAT("SDL", "{}SDL_Init failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        throw std::runtime_error("SDL_Init failed");
    }

    SDL_Window* raw = SDL_CreateWindow(title, w, h, flags);
    if (!raw) {
        LOG_ERROR_CAT("SDL", "{}SDL_CreateWindow failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        throw std::runtime_error("SDL_CreateWindow failed");
    }

    LOG_SUCCESS_CAT("Window", "{}SDL_Window created: {}x{} @ {:p}{}", 
                    EMERALD_GREEN, w, h, static_cast<void*>(raw), RESET);

    return SDLWindowPtr(raw);
}

// ──────────────────────────────────────────────────────────────────────────────
// getWindowExtensions — Vulkan instance extensions (SDL3 API)
// ──────────────────────────────────────────────────────────────────────────────
std::vector<std::string> getWindowExtensions(const SDLWindowPtr&) {
    unsigned int count = 0;

    if (SDL_Vulkan_GetInstanceExtensions(&count) != 0) {
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
    bool isFullscreen = (SDL_GetWindowFlags(window.get()) & SDL_WINDOW_FULLSCREEN) != 0;

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