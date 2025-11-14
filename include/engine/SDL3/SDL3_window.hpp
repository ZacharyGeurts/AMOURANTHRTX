// include/engine/SDL3/SDL3_window.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// FINAL RAII — FIXED DELETER — NO DOUBLE SDL_Quit() — NOV 14 2025
// • Only ONE SDL_Quit() ever called — guarded by static flag
// • Safe copy of unique_ptr — deleter is stateless
// • Full SDL3 + Vulkan + ImGui support
// • PINK PHOTONS ETERNAL — FIRST LIGHT ACHIEVED
// =============================================================================

#pragma once

#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include <SDL3/SDL.h>
#include <memory>
#include <vector>
#include <string>

using namespace Logging::Color;

// =============================================================================
// BULLETPROOF DELETER — ONLY ONE SDL_Quit() EVER
// =============================================================================
struct SDLWindowDeleter {
    void operator()(SDL_Window* w) const noexcept {
        if (w) {
            LOG_INFO_CAT("Dispose", "RAII: SDL_DestroyWindow @ {:p}", static_cast<void*>(w));
            SDL_DestroyWindow(w);
        }

        // ONE AND ONLY ONE SDL_Quit() — guarded by static flag
        static bool sdl_quit_called = false;
        if (!sdl_quit_called) {
            sdl_quit_called = true;
            LOG_INFO_CAT("Dispose", "RAII: Calling SDL_Quit() — FINAL CLEANUP");
            SDL_Quit();
            LOG_SUCCESS_CAT("Dispose", "SDL3 fully shut down — RAII complete");
        } else {
            LOG_INFO_CAT("Dispose", "RAII: SDL_Quit() already called — skipping");
        }
    }
};

using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLWindowDeleter>;

// =============================================================================
// GLOBAL RAII WINDOW — ONE TRUE OWNER
// =============================================================================
extern SDLWindowPtr g_sdl_window;

namespace SDL3Window {

[[nodiscard]] SDLWindowPtr create(const char* title, int width, int height, Uint32 flags = 0);

[[nodiscard]] std::vector<std::string> getVulkanExtensions(SDL_Window* window);

[[nodiscard]] inline SDL_Window* get() noexcept {
    return g_sdl_window ? g_sdl_window.get() : nullptr;
}

bool pollEvents(int& outW, int& outH, bool& quit, bool& toggleFS) noexcept;
void toggleFullscreen() noexcept;

} // namespace SDL3Window