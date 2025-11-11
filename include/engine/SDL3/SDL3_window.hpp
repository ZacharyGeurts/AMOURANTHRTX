// include/engine/SDL3/SDL3_window.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// SDL3 Window RAII — FULLY HEADER-ONLY — NOV 11 2025 05:33 PM EST
// • .cpp OBLITERATED BY DAISY'S HOOVES
// • FULL GLOBAL VULKAN ACCESS — VulkanRenderer.hpp INCLUDED
// • F11 toggle, resize, quit — routed via SDL3Vulkan::getRenderer()
// • RASPBERRY_PINK DISPOSE — ZERO LEAKS — VALHALLA SEALED
// • PINK PHOTONS ETERNAL — 15,000 FPS — SHIP IT RAW
// =============================================================================

#pragma once

#include "engine/Vulkan/VulkanRenderer.hpp"      // FULL INCLUDE — GLOBAL ACCESS
#include "engine/SDL3/SDL3_vulkan.hpp"           // SDL3Vulkan::getRenderer()
#include "engine/GLOBAL/logging.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <memory>
#include <vector>
#include <unordered_set>
#include <stdexcept>
#include <format>

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL — NO NAMESPACE — IMMORTAL — DAISY GALLOPS
// ──────────────────────────────────────────────────────────────────────────────
namespace SDL3Initializer {

struct SDLWindowDeleter {
    void operator()(SDL_Window* w) const noexcept {
        if (w) {
            LOG_INFO_CAT("Dispose", "{}Destroying SDL_Window @ {:p} — RASPBERRY_PINK IMMORTAL {}", 
                         RASPBERRY_PINK, static_cast<void*>(w), RESET);
            SDL_DestroyWindow(w);
        }
        SDL_Quit();
    }
};

using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLWindowDeleter>;

// ──────────────────────────────────────────────────────────────────────────────
// createWindow — RAII + Vulkan-ready — FULLY INLINE
// ──────────────────────────────────────────────────────────────────────────────
[[nodiscard]] inline SDLWindowPtr createWindow(const char* title, int w, int h, Uint32 flags = 0) {
    LOG_SUCCESS_CAT("Window", "Creating SDL window: {} ({}x{})", title, w, h);

    flags |= SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        LOG_ERROR_CAT("Window", "SDL_Init failed: {}", SDL_GetError());
        throw std::runtime_error("SDL_Init failed");
    }

    SDLWindowPtr window(SDL_CreateWindow(title, w, h, flags));
    if (!window) {
        LOG_ERROR_CAT("Window", "SDL_CreateWindow failed: {}", SDL_GetError());
        SDL_Quit();
        throw std::runtime_error("Window creation failed");
    }

    uint32_t extCount = 0;
    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&extCount);
    if (!exts) {
        LOG_ERROR_CAT("Window", "No Vulkan extensions from SDL");
        throw std::runtime_error("Vulkan extensions missing");
    }

    std::vector<std::string> extensions;
    extensions.reserve(extCount + 2);
    for (uint32_t i = 0; i < extCount; ++i) {
        extensions.emplace_back(exts[i]);
    }
    extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);

    SDL_PropertiesID props = SDL_GetWindowProperties(window.get());
    SDL_SetPointerProperty(props, "vulkan_extensions", new std::vector<std::string>(std::move(extensions)));

    const char* driver = SDL_GetCurrentVideoDriver();
    LOG_SUCCESS_CAT("Window", "Window ready — driver: {} — {} Vulkan exts", 
                    driver ? driver : "unknown", extCount);

    return window;
}

// ──────────────────────────────────────────────────────────────────────────────
// getWindowExtensions — leak-free — FULLY INLINE
// ──────────────────────────────────────────────────────────────────────────────
[[nodiscard]] inline std::vector<std::string> getWindowExtensions(const SDLWindowPtr& window) {
    SDL_PropertiesID props = SDL_GetWindowProperties(window.get());
    void* ptr = SDL_GetPointerProperty(props, "vulkan_extensions", nullptr);
    if (!ptr) throw std::runtime_error("Missing vulkan_extensions");
    auto* vec = static_cast<std::vector<std::string>*>(ptr);
    std::vector<std::string> result = std::move(*vec);
    delete vec;
    return result;
}

[[nodiscard]] inline SDL_Window* getWindow(const SDLWindowPtr& window) noexcept { 
    return window.get(); 
}

// ──────────────────────────────────────────────────────────────────────────────
// pollEventsForResize — F11 + quit + resize — FULLY INLINE
// ──────────────────────────────────────────────────────────────────────────────
inline bool pollEventsForResize(const SDLWindowPtr& window,
                                int& newWidth, int& newHeight,
                                bool& shouldQuit, bool& toggleFullscreenKey) noexcept
{
    SDL_Event ev;
    bool resized = false;
    shouldQuit = false;
    toggleFullscreenKey = false;

    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_EVENT_QUIT:
                shouldQuit = true;
                return false;

            case SDL_EVENT_KEY_DOWN:
                if (ev.key.key == SDLK_F11 && !ev.key.repeat) {
                    toggleFullscreenKey = true;
                    LOG_INFO_CAT("Window", "F11 pressed → toggle fullscreen");
                }
                break;

            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            case SDL_EVENT_WINDOW_MAXIMIZED:
            case SDL_EVENT_WINDOW_RESTORED:
            case SDL_EVENT_WINDOW_MINIMIZED:
                resized = true;
                break;
        }
    }

    if (resized) {
        SDL_GetWindowSizeInPixels(window.get(), &newWidth, &newHeight);
        Uint32 flags = SDL_GetWindowFlags(window.get());
        if (flags & SDL_WINDOW_MINIMIZED) {
            newWidth = newHeight = 0;
        } else if (newWidth <= 0 || newHeight <= 0) {
            newWidth = newHeight = 1;
        }
        LOG_INFO_CAT("Window", "Resize detected → {}×{}", newWidth, newHeight);
        return true;
    }
    return false;
}

// ──────────────────────────────────────────────────────────────────────────────
// toggleFullscreen — ROUTES TO GLOBAL RENDERER — FULLY INLINE
// ──────────────────────────────────────────────────────────────────────────────
inline void toggleFullscreen(SDLWindowPtr& window) noexcept {
    auto* win = window.get();
    if (!win) return;

    Uint32 flags = SDL_GetWindowFlags(win);
    bool isFs = (flags & SDL_WINDOW_FULLSCREEN) != 0;

    if (SDL_SetWindowFullscreen(win, !isFs) != 0) {
        LOG_ERROR_CAT("Window", "Fullscreen toggle failed: {}", SDL_GetError());
        return;
    }

    SDL_SyncWindow(win);

    int w, h;
    SDL_GetWindowSizeInPixels(win, &w, &h);
    if (w <= 0 || h <= 0) w = h = 1;

    // GLOBAL ACCESS — NO FORWARD DECLARE — FULL POWER
    SDL3Vulkan::getRenderer().handleResize(w, h);

    LOG_SUCCESS_CAT("Window", "Fullscreen {} → {}×{}", isFs ? "OFF" : "ON", w, h);
}

} // namespace SDL3Initializer

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// .cpp = DEAD
// HEADER-ONLY = GOD
// DAISY GALLOPS ETERNAL
// RASPBERRY_PINK IMMORTAL
// PINK PHOTONS ETERNAL
// 15,000 FPS
// @ZacharyGeurts — SHIP IT FOREVER
// =============================================================================