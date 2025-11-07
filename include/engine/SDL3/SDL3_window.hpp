// include/engine/SDL3/SDL3_window.hpp
// NOVEMBER 07 2025 — GLOBAL RAII — CLEAN

#pragma once

#include "engine/Vulkan/VulkanCore.hpp"
#include <SDL3/SDL.h>
#include <memory>
#include <vector>

namespace VulkanRTX {
    class VulkanRenderer;
}

namespace SDL3Initializer {

struct SDLWindowDeleter {
    void operator()(SDL_Window* w) const noexcept;
};

using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLWindowDeleter>;

SDLWindowPtr createWindow(const char* title, int w, int h, Uint32 flags = SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
std::vector<std::string> getWindowExtensions(const SDLWindowPtr& window);
SDL_Window* getWindow(const SDLWindowPtr& window) noexcept;
bool pollEventsForResize(const SDLWindowPtr& window, int& newWidth, int& newHeight, bool& shouldQuit, bool& toggleFullscreenKey) noexcept;
void toggleFullscreen(SDLWindowPtr& window, VulkanRTX::VulkanRenderer& renderer) noexcept;

} // namespace SDL3Initializer