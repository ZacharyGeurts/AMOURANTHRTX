// include/engine/SDL3/SDL3_window.hpp
// AMOURANTH RTX Engine, October 2025 - SDL3 window creation and management.
// Dependencies: SDL3, Vulkan 1.3+, C++20 standard library, logging.hpp, Vulkan_init.hpp.
// Supported platforms: Linux, Windows.
// Zachary Geurts 2025

#ifndef SDL3_WINDOW_HPP
#define SDL3_WINDOW_HPP

#include <SDL3/SDL.h>
#include <memory>
#include <set>
#include <string>

#include "engine/SDL3/SDL3_init.hpp"

namespace SDL3Initializer {

struct SDLWindowDeleter {
    void operator()(SDL_Window* w) const;
};

using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLWindowDeleter>;

SDLWindowPtr createWindow(
    const char* title, 
    int w, 
    int h, 
    Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
);

SDL_Window* getWindow(const SDLWindowPtr& window);

} // namespace SDL3Initializer

#endif // SDL3_WINDOW_HPP