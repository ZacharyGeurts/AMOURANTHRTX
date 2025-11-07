// src/engine/SDL3/SDL3_window.cpp
// AMOURANTH RTX Engine – NOVEMBER 07 2025 — GLOBAL RAII
// destroyWindow + quitSDL GLOBAL — NO PREFIX — ALL IMMORTAL
// SDL3 single call — key event fixed — 69,420 FPS ETERNAL

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/SDL3/SDL3_window.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/logging.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_properties.h>

#include <stdexcept>
#include <vector>
#include <string>

using namespace std::literals;

namespace SDL3Initializer {

// ---------------------------------------------------------------------------
// Deleter — GLOBAL RAII
// ---------------------------------------------------------------------------
void SDLWindowDeleter::operator()(SDL_Window* w) const noexcept {
    destroyWindow(w);
}

// ---------------------------------------------------------------------------
// createWindow — GLOBAL RAII — SDL3 single call
// ---------------------------------------------------------------------------
SDLWindowPtr createWindow(const char* title, int w, int h, Uint32 flags) {
    LOG_INFO_CAT("Window", "Creating SDL window: {} ({}x{}) flags=0x{:x}", title, w, h, flags);

    flags |= SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        LOG_ERROR_CAT("Window", "SDL_Init failed: {}", SDL_GetError());
        throw std::runtime_error("SDL_Init failed: "s + SDL_GetError());
    }

    SDLWindowPtr window(SDL_CreateWindow(title, w, h, flags));
    if (!window) {
        LOG_ERROR_CAT("Window", "SDL_CreateWindow failed: {}", SDL_GetError());
        quitSDL();
        throw std::runtime_error("SDL_CreateWindow failed: "s + SDL_GetError());
    }

    uint32_t extCount = 0;
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);
    if (!sdlExts || extCount == 0) {
        LOG_ERROR_CAT("Window", "SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
        quitSDL();
        throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed");
    }

    std::vector<std::string> extensions;
    extensions.reserve(extCount + 2);
    for (uint32_t i = 0; i < extCount; ++i) {
        extensions.emplace_back(sdlExts[i]);
    }

    extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    LOG_DEBUG_CAT("Window", "SDL reports {} Vulkan instance extensions", extCount);

    SDL_PropertiesID props = SDL_GetWindowProperties(window.get());
    if (props == 0) {
        LOG_ERROR_CAT("Window", "SDL_GetWindowProperties returned 0");
        quitSDL();
        throw std::runtime_error("Failed to obtain window properties");
    }
    SDL_SetPointerProperty(props, "vulkan_extensions",
                           new std::vector<std::string>(std::move(extensions)));

    const char* driver = SDL_GetCurrentVideoDriver();
    LOG_INFO_CAT("Window", "Window created – driver: {} – flags: 0x{:x}",
                 driver ? driver : "none", SDL_GetWindowFlags(window.get()));

    return window;
}

// ---------------------------------------------------------------------------
// getWindowExtensions — leak-free
// ---------------------------------------------------------------------------
std::vector<std::string> getWindowExtensions(const SDLWindowPtr& window) {
    if (!window) {
        LOG_ERROR_CAT("Window", "Null window passed to getWindowExtensions");
        throw std::runtime_error("Invalid window pointer");
    }

    SDL_PropertiesID props = SDL_GetWindowProperties(window.get());
    if (props == 0) {
        LOG_ERROR_CAT("Window", "Failed to get window properties");
        throw std::runtime_error("Failed to get window properties");
    }

    void* ptr = SDL_GetPointerProperty(props, "vulkan_extensions", nullptr);
    if (!ptr) {
        LOG_ERROR_CAT("Window", "No vulkan_extensions property stored");
        throw std::runtime_error("Missing vulkan_extensions property");
    }
    auto* vec = static_cast<std::vector<std::string>*>(ptr);
    std::vector<std::string> result = std::move(*vec);
    delete vec;
    return result;
}

SDL_Window* getWindow(const SDLWindowPtr& window) noexcept { return window.get(); }

// ---------------------------------------------------------------------------
// pollEventsForResize — SDL3 key event FIXED
// ---------------------------------------------------------------------------
bool pollEventsForResize(const SDLWindowPtr& window,
                         int& newWidth, int& newHeight,
                         bool& shouldQuit, bool& toggleFullscreenKey) noexcept
{
    auto* win = window.get();
    if (!win) return false;

    SDL_Event ev;
    bool changed = false;

    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_EVENT_QUIT:
                shouldQuit = true;
                return false;

            case SDL_EVENT_KEY_DOWN:
                if (ev.key.key == SDLK_F11 && !ev.key.repeat) {
                    toggleFullscreenKey = true;
                    LOG_INFO_CAT("Window", "F11 → toggle fullscreen");
                }
                break;

            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_MAXIMIZED:
            case SDL_EVENT_WINDOW_RESTORED:
            case SDL_EVENT_WINDOW_MINIMIZED:
                changed = true;
                break;
        }
    }

    if (changed) {
        SDL_GetWindowSizeInPixels(win, &newWidth, &newHeight);
        auto flags = SDL_GetWindowFlags(win);
        if (flags & SDL_WINDOW_MINIMIZED) {
            newWidth = newHeight = 0;
        } else if (newWidth <= 0 || newHeight <= 0) {
            newWidth = newHeight = 1;
        }
        return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// toggleFullscreen
// ---------------------------------------------------------------------------
void toggleFullscreen(SDLWindowPtr& window, VulkanRTX::VulkanRenderer& renderer) noexcept {
    auto* win = window.get();
    if (!win) return;

    auto flags = SDL_GetWindowFlags(win);
    bool fs = (flags & SDL_WINDOW_FULLSCREEN) != 0;

    if (SDL_SetWindowFullscreen(win, !fs) != 0) {
        LOG_ERROR_CAT("Window", "Fullscreen toggle failed: {}", SDL_GetError());
        return;
    }

    SDL_SyncWindow(win);

    int w, h;
    SDL_GetWindowSizeInPixels(win, &w, &h);
    if (w <= 0 || h <= 0) w = h = 1;

    renderer.handleResize(w, h);
    LOG_INFO_CAT("Window", "Fullscreen toggled → resize {}×{}", w, h);
}

} // namespace SDL3Initializer