// src/engine/SDL3/SDL3_window.cpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#include "engine/SDL3/SDL3_window.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/Dispose.hpp"
#include "engine/logging.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_properties.h>

#include <stdexcept>
#include <vector>
#include <string>
#include <format>

using namespace std::literals;          // enables "…"s literals

namespace SDL3Initializer {

// ---------------------------------------------------------------------------
//  Deleter – also frees the stored Vulkan extensions vector
// ---------------------------------------------------------------------------
void SDLWindowDeleter::operator()(SDL_Window* w) const {
    if (w) {
        SDL_PropertiesID props = SDL_GetWindowProperties(w);
        if (props != 0) {
            void* data = SDL_GetPointerProperty(props, "vulkan_extensions", nullptr);
            delete static_cast<std::vector<std::string>*>(data);
        }
        Dispose::destroyWindow(w);
    }
}

// ---------------------------------------------------------------------------
//  createWindow
// ---------------------------------------------------------------------------
SDLWindowPtr createWindow(const char* title, int w, int h, Uint32 flags) {
    LOG_INFO_CAT("Window", "Creating SDL window: {} ({}x{}) flags=0x{:x}", title, w, h, flags);

    flags |= SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE;

    // Prefer Wayland on Linux, fall back to X11
    SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "wayland,x11");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        LOG_ERROR_CAT("Window", "SDL_Init failed: {}", SDL_GetError());
        throw std::runtime_error("SDL_Init failed: "s + SDL_GetError());
    }

    SDLWindowPtr window(SDL_CreateWindow(title, w, h, flags));
    if (!window) {
        LOG_ERROR_CAT("Window", "SDL_CreateWindow failed: {}", SDL_GetError());
        Dispose::quitSDL();
        throw std::runtime_error("SDL_CreateWindow failed: "s + SDL_GetError());
    }

    // ----- Vulkan instance extensions required by SDL -----
    uint32_t extCount = 0;
    const char* const* sdlExts = SDL_Vulkan_GetInstanceExtensions(&extCount);
    if (!sdlExts || extCount == 0) {
        LOG_ERROR_CAT("Window", "SDL_Vulkan_GetInstanceExtensions failed: {}", SDL_GetError());
        Dispose::quitSDL();
        throw std::runtime_error("SDL_Vulkan_GetInstanceExtensions failed: "s + SDL_GetError());
    }

    std::vector<std::string> extensions;
    extensions.reserve(extCount + 2);
    for (uint32_t i = 0; i < extCount; ++i)
        extensions.emplace_back(sdlExts[i]);

    // RTX + debug extensions we always want
    extensions.emplace_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

    LOG_DEBUG_CAT("Window", "SDL reports {} Vulkan instance extensions", extCount);

    // Store inside window properties for later retrieval
    SDL_PropertiesID props = SDL_GetWindowProperties(window.get());
    if (props == 0) {
        LOG_ERROR_CAT("Window", "SDL_GetWindowProperties returned 0");
        Dispose::quitSDL();
        throw std::runtime_error("Failed to obtain window properties");
    }
    SDL_SetPointerProperty(props, "vulkan_extensions",
                           new std::vector<std::string>(std::move(extensions)));

    const char* driver = SDL_GetCurrentVideoDriver();
    LOG_INFO_CAT("Window",
                 "Window created – driver: {} – flags: 0x{:x}",
                 driver ? driver : "none",
                 SDL_GetWindowFlags(window.get()));

    return window;
}

// ---------------------------------------------------------------------------
//  getWindowExtensions
// ---------------------------------------------------------------------------
std::vector<std::string> getWindowExtensions(const SDLWindowPtr& window) {
    if (!window) {
        LOG_ERROR_CAT("Window", "Null window passed to getWindowExtensions");
        throw std::runtime_error("Invalid window pointer");
    }

    SDL_PropertiesID props = SDL_GetWindowProperties(window.get());
    if (props == 0) {
        LOG_ERROR_CAT("Window", "Failed to get window properties in getWindowExtensions");
        throw std::runtime_error("Failed to get window properties");
    }

    void* ptr = SDL_GetPointerProperty(props, "vulkan_extensions", nullptr);
    if (!ptr) {
        LOG_ERROR_CAT("Window", "No vulkan_extensions property stored");
        throw std::runtime_error("Missing vulkan_extensions property");
    }
    return *static_cast<std::vector<std::string>*>(ptr);
}

// ---------------------------------------------------------------------------
//  getWindow
// ---------------------------------------------------------------------------
SDL_Window* getWindow(const SDLWindowPtr& window) {
    LOG_DEBUG_CAT("Window", "Returning raw SDL_Window*");
    return window.get();
}

// ---------------------------------------------------------------------------
//  pollEventsForResize
// ---------------------------------------------------------------------------
//  Returns true when a size change (resize / restore / minimize) was detected.
//  Caller must immediately call renderer.handleResize(newWidth, newHeight).
// ---------------------------------------------------------------------------
bool pollEventsForResize(const SDLWindowPtr& window,
                         int& newWidth, int& newHeight,
                         bool& shouldQuit, bool& toggleFullscreenKey)
{
    SDL_Window* win = window.get();
    LOG_DEBUG_CAT("Window", "Polling events for resize/minimize/quit");

    SDL_Event ev;
    bool resizeDetected   = false;
    bool minimizeDetected = false;
    bool restoreDetected  = false;

    shouldQuit           = false;
    toggleFullscreenKey = false;

    SDL_PumpEvents();

    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {

            case SDL_EVENT_QUIT:
                LOG_INFO_CAT("Window", "Quit event");
                shouldQuit = true;
                return false;

            case SDL_EVENT_KEY_DOWN:
                // SDL-3: ev.key.key contains the virtual key code
                if (ev.key.key == SDLK_F11 && !ev.key.repeat) {
                    toggleFullscreenKey = true;
                    LOG_INFO_CAT("Window", "F11 → toggle fullscreen");
                }
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                resizeDetected = true;
                LOG_DEBUG_CAT("Window", "Resize event {}x{}", ev.window.data1, ev.window.data2);
                break;

            case SDL_EVENT_WINDOW_MINIMIZED:
                minimizeDetected = true;
                LOG_DEBUG_CAT("Window", "Window minimized");
                break;

            case SDL_EVENT_WINDOW_MAXIMIZED:
            case SDL_EVENT_WINDOW_RESTORED:
                restoreDetected = true;
                LOG_DEBUG_CAT("Window", "Window maximized or restored");
                break;

            default:
                break;
        }
    }

    // --------------------------------------------------------------
    //  Determine final drawable size
    // --------------------------------------------------------------
    if (resizeDetected || restoreDetected || minimizeDetected) {
        if (!SDL_GetWindowSizeInPixels(win, &newWidth, &newHeight)) {
            LOG_ERROR_CAT("Window", "SDL_GetWindowSizeInPixels failed: {}", SDL_GetError());
            newWidth = newHeight = 0;
            return false;
        }

        // Report 0×0 when minimized – swap-chain will pause
        if (SDL_GetWindowFlags(win) & SDL_WINDOW_MINIMIZED) {
            newWidth = newHeight = 0;
            LOG_DEBUG_CAT("Window", "Minimized → reporting 0×0");
        } else if (newWidth <= 0 || newHeight <= 0) {
            // Guard against drivers that return 0 after a restore
            newWidth = newHeight = 1;
            LOG_WARNING_CAT("Window", "Drawable size 0×0 → clamped to 1×1");
        }

        LOG_DEBUG_CAT("Window", "Final size after event handling: {}×{}", newWidth, newHeight);
        return true;               // caller must call renderer.handleResize()
    }

    return false;
}

// ---------------------------------------------------------------------------
//  toggleFullscreen
// ---------------------------------------------------------------------------
void toggleFullscreen(SDLWindowPtr& window, VulkanRTX::VulkanRenderer& renderer)
{
    SDL_Window* win = window.get();
    if (!win) {
        LOG_ERROR_CAT("Window", "toggleFullscreen called with null window");
        return;
    }

    Uint32 flags = SDL_GetWindowFlags(win);
    bool currentlyFS = (flags & SDL_WINDOW_FULLSCREEN) != 0;
    LOG_INFO_CAT("Window", "Toggle fullscreen (current={})", currentlyFS ? "yes" : "no");

    // Borderless fullscreen – avoids exclusive-mode bugs on Windows
    bool success = SDL_SetWindowFullscreen(win, currentlyFS ? false : true) == 0;
    if (!success) {
        LOG_ERROR_CAT("Window", "SDL_SetWindowFullscreen failed: {}", SDL_GetError());
        return;
    }

    SDL_SyncWindow(win);   // make the change immediate

    int w = 0, h = 0;
    if (!SDL_GetWindowSizeInPixels(win, &w, &h)) {
        LOG_ERROR_CAT("Window", "Failed to query size after fullscreen toggle: {}", SDL_GetError());
        return;
    }

    if (w <= 0 || h <= 0) {
        LOG_WARNING_CAT("Window", "Post-fullscreen size {}×{} → clamp to 1×1", w, h);
        w = h = 1;
    }

    // Immediate resize – no queuing
    renderer.handleResize(w, h);
    LOG_INFO_CAT("Window", "Fullscreen toggled → immediate resize {}×{}", w, h);
}

} // namespace SDL3Initializer