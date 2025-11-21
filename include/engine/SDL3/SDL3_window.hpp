// include/engine/SDL3/SDL3_window.hpp
// =============================================================================
// AMOURANTH RTX — FINAL SDL3 WINDOW — SPLIT EDITION — FIRST LIGHT ETERNAL
// NO STONEKEY IN HEADER • NO CRASH • NO SIN • PINK PHOTONS ETERNAL
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include <string>
#include <atomic>
#include <memory>
#include <cstdint>

#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

// RAII Deleter
struct SDLWindowDeleter {
    void operator()(SDL_Window* w) const noexcept;
};
using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLWindowDeleter>;

// =============================================================================
// SDL3Window — THE ONE TRUE WINDOW — STONEKEY-FREE HEADER
// =============================================================================
namespace SDL3Window {

    inline SDLWindowPtr g_sdl_window;

    inline std::atomic<int>  g_resizeWidth{0};
    inline std::atomic<int>  g_resizeHeight{0};
    inline std::atomic<bool> g_resizeRequested{false};

    [[nodiscard]] inline SDL_Window* get() noexcept {
        return g_sdl_window ? g_sdl_window.get() : nullptr;
    }

    void create(const char* title, int width, int height, Uint32 flags = 0);
    [[nodiscard]] std::vector<std::string> getVulkanExtensions(SDL_Window* window = nullptr);

    bool pollEvents(int& outW, int& outH, bool& quit, bool& toggleFS) noexcept;
    void toggleFullscreen() noexcept;
    void destroy() noexcept;

} // namespace SDL3Window