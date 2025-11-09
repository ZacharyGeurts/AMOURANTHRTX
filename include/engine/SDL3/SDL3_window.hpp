// include/engine/SDL3/SDL3_window.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// SDL3 Window RAII — GLOBAL VulkanRenderer — NOVEMBER 08 2025

#pragma once

//#include "engine/Vulkan/VulkanCommon.hpp"
#include <SDL3/SDL.h>
#include <memory>
#include <vector>
#include <unordered_set>

// GLOBAL — NO NAMESPACE — IMMORTAL
class VulkanRenderer;  // ← GLOBAL FORWARD DECLARE

namespace SDL3Initializer {

struct SDLWindowDeleter {
    void operator()(SDL_Window* w) const noexcept;
};

using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLWindowDeleter>;

SDLWindowPtr createWindow(const char* title, int w, int h, Uint32 flags = 0);
std::vector<std::string> getWindowExtensions(const SDLWindowPtr& window);
SDL_Window* getWindow(const SDLWindowPtr& window) noexcept;

bool pollEventsForResize(const SDLWindowPtr& window,
                         int& newWidth, int& newHeight,
                         bool& shouldQuit, bool& toggleFullscreenKey) noexcept;

void toggleFullscreen(SDLWindowPtr& window, VulkanRenderer& renderer) noexcept;

} // namespace SDL3Initializer