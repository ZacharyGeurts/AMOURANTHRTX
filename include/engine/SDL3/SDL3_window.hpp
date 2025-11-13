// include/engine/SDL3/SDL3_window.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// SDL3 Window RAII — SPLIT INTO HEADER + CPP — NOV 13 2025
// • FULL GLOBAL VULKAN ACCESS — VulkanRenderer.hpp INCLUDED
// • F11 toggle, resize, quit — routed via SDL3Vulkan::getRenderer()
// • RESPECTS Options::Performance::ENABLE_IMGUI → resizable flag
// • RASPBERRY_PINK DISPOSE — ZERO LEAKS — VALHALLA SEALED
// • SDL3 GLOBAL POINTER: g_sdl3_window — SAFE, RAII
// • CENTERED BY DEFAULT — SDL_SetWindowPosition
// • PINK PHOTONS ETERNAL — 15,000 FPS — SHIP IT RAW
// =============================================================================

#pragma once

#include "engine/Vulkan/VulkanRenderer.hpp"      // FULL INCLUDE — GLOBAL ACCESS
#include "engine/SDL3/SDL3_vulkan.hpp"           // SDL3Vulkan::getRenderer()
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <memory>
#include <vector>
#include <stdexcept>

using namespace Logging::Color;

namespace SDL3Initializer {

struct SDLWindowDeleter {
    void operator()(SDL_Window* w) const noexcept;
};

using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLWindowDeleter>;
extern SDLWindowPtr g_sdl3_window;  // ← SDL3 GLOBAL POINTER

[[nodiscard]] SDLWindowPtr createWindow(const char* title, int w, int h, Uint32 flags = 0);

[[nodiscard]] std::vector<std::string> getWindowExtensions(const SDLWindowPtr& window);

[[nodiscard]] inline SDL_Window* getWindow(const SDLWindowPtr& window) noexcept { 
    return window.get(); 
}

[[nodiscard]] inline SDL_Window* getSDL3Window() noexcept {
    if (!g_sdl3_window) {
        LOG_WARN_CAT("SDL3", "{}g_sdl3_window is null — returning nullptr{}", AMBER_YELLOW, RESET);
        return nullptr;
    }
    LOG_TRACE_CAT("SDL3", "g_sdl3_window accessed @ {:p}", static_cast<void*>(g_sdl3_window.get()));
    return g_sdl3_window.get();
}

inline void setSDL3Window(SDLWindowPtr&& ptr) noexcept {
    if (g_sdl3_window) {
        LOG_WARN_CAT("SDL3", "{}g_sdl3_window already assigned — overwriting @ {:p}{}", 
                     CRIMSON_MAGENTA, static_cast<void*>(g_sdl3_window.get()), RESET);
    } else {
        LOG_INFO_CAT("SDL3", "{}g_sdl3_window being assigned for the first time{}", PLASMA_FUCHSIA, RESET);
    }
    g_sdl3_window = std::move(ptr);
    LOG_SUCCESS_CAT("SDL3", "{}g_sdl3_window assigned @ {:p}{}", 
                    EMERALD_GREEN, static_cast<void*>(g_sdl3_window.get()), RESET);
}

bool pollEventsForResize(const SDLWindowPtr& window,
                         int& newWidth, int& newHeight,
                         bool& shouldQuit, bool& toggleFullscreenKey) noexcept;

void toggleFullscreen(SDLWindowPtr& window) noexcept;

} // namespace SDL3Initializer