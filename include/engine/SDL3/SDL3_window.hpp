// include/engine/SDL3/SDL3_window.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// SDL3 Window RAII — GLOBAL VulkanRenderer — NOV 11 2025 11:18 AM EST
// • FULL GLOBAL VULKAN ACCESS — NO FORWARD DECLARES — INCLUDES VulkanRenderer.hpp
// • F11 toggle, resize, quit — routed via SDL3Vulkan interface
// • Pink dispose, zero leaks, Valhalla sealed — SHIP IT RAW
// =============================================================================

#pragma once

#include "engine/Vulkan/VulkanRenderer.hpp"  // FIXED: FULL INCLUDE FOR GLOBAL VULKAN ACCESS
#include <SDL3/SDL.h>
#include <memory>
#include <vector>
#include <unordered_set>

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL — NO NAMESPACE — IMMORTAL
// ──────────────────────────────────────────────────────────────────────────────
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

void toggleFullscreen(SDLWindowPtr& window) noexcept;  // FIXED: No renderer param — uses global SDL3Vulkan::getRenderer()

} // namespace SDL3Initializer

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================