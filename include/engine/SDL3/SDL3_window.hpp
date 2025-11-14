// include/engine/SDL3/SDL3_window.hpp
// =============================================================================
// SDL3 Window RAII Wrapper — NOV 14 2025 — PINK PHOTONS ETERNAL
// =============================================================================

#pragma once

#include <memory>
#include <SDL3/SDL.h>
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

struct SDLWindowDeleter {
    void operator()(SDL_Window* w) const noexcept {
        if (w) {
            LOG_INFO_CAT("Dispose", "RAII: SDL_DestroyWindow @ {:p}", static_cast<void*>(w));
            SDL_DestroyWindow(w);
        }
    }
};

using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLWindowDeleter>;

namespace SDL3Window {

extern SDLWindowPtr g_sdl_window;

inline SDL_Window* get() noexcept {
    return g_sdl_window ? g_sdl_window.get() : nullptr;
}

[[nodiscard]] SDLWindowPtr create(const char* title, int width, int height, Uint32 flags = 0);

[[nodiscard]] std::vector<std::string> getVulkanExtensions(SDL_Window* window);

bool pollEvents(int& outW, int& outH, bool& quit, bool& toggleFS) noexcept;

void toggleFullscreen() noexcept;

inline void destroy() noexcept {
    if (g_sdl_window) {
        g_sdl_window.reset();
    }
}

} // namespace SDL3Window