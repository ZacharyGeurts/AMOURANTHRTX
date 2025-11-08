// src/engine/SDL3/SDL3_window.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// FINAL SDL3 Window — GLOBAL VulkanRenderer — NOVEMBER 08 2025
// ZERO conflicts — F11 toggle — resize — quit — pink dispose

#include "engine/SDL3/SDL3_window.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"  // ← GLOBAL INCLUDE
#include "engine/logging.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <stdexcept>
#include <format>

using namespace Logging::Color;

namespace SDL3Initializer {

// ---------------------------------------------------------------------------
// Deleter — RASPBERRY_PINK ETERNAL
// ---------------------------------------------------------------------------
void SDLWindowDeleter::operator()(SDL_Window* w) const noexcept {
    if (w) {
        LOG_INFO_CAT("Dispose", "{}Destroying SDL_Window @ {:p} — RASPBERRY_PINK IMMORTAL 🩷{}", 
                     RASPBERRY_PINK, static_cast<void*>(w), RESET);
        SDL_DestroyWindow(w);
    }
    SDL_Quit();
}

// ---------------------------------------------------------------------------
// createWindow — GLOBAL RAII
// ---------------------------------------------------------------------------
SDLWindowPtr createWindow(const char* title, int w, int h, Uint32 flags) {
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

// ---------------------------------------------------------------------------
// getWindowExtensions — leak-free
// ---------------------------------------------------------------------------
std::vector<std::string> getWindowExtensions(const SDLWindowPtr& window) {
    SDL_PropertiesID props = SDL_GetWindowProperties(window.get());
    void* ptr = SDL_GetPointerProperty(props, "vulkan_extensions", nullptr);
    if (!ptr) throw std::runtime_error("Missing vulkan_extensions");
    auto* vec = static_cast<std::vector<std::string>*>(ptr);
    std::vector<std::string> result = std::move(*vec);
    delete vec;
    return result;
}

SDL_Window* getWindow(const SDLWindowPtr& window) noexcept { return window.get(); }

// ---------------------------------------------------------------------------
// pollEventsForResize — F11 + quit + resize
// ---------------------------------------------------------------------------
bool pollEventsForResize(const SDLWindowPtr& window,
                         int& newWidth, int& newHeight,
                         bool& shouldQuit, bool& toggleFullscreenKey) noexcept
{
    SDL_Event ev;
    bool resized = false;

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

// ---------------------------------------------------------------------------
// toggleFullscreen — GLOBAL VulkanRenderer
// ---------------------------------------------------------------------------
void toggleFullscreen(SDLWindowPtr& window, VulkanRenderer& renderer) noexcept {
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

    renderer.handleResize(w, h);  // ← NOW VALID — GLOBAL CLASS

    LOG_SUCCESS_CAT("Window", "Fullscreen {} → {}×{}", isFs ? "OFF" : "ON", w, h);
}

} // namespace SDL3Initializer