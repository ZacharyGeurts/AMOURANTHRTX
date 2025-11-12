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

namespace SDL3Initializer {

struct SDLWindowDeleter {
    void operator()(SDL_Window* w) const noexcept;
};

using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLWindowDeleter>;

// ──────────────────────────────────────────────────────────────────────────────
// createWindow — RAII + Vulkan-ready
// ──────────────────────────────────────────────────────────────────────────────
[[nodiscard]] SDLWindowPtr createWindow(const char* title, int w, int h, Uint32 flags = 0);

// ──────────────────────────────────────────────────────────────────────────────
// getWindowExtensions — leak-free
// ──────────────────────────────────────────────────────────────────────────────
[[nodiscard]] std::vector<std::string> getWindowExtensions(const SDLWindowPtr& window);

// ──────────────────────────────────────────────────────────────────────────────
// getWindow — direct access
// ──────────────────────────────────────────────────────────────────────────────
[[nodiscard]] inline SDL_Window* getWindow(const SDLWindowPtr& window) noexcept { 
    return window.get(); 
}

// ──────────────────────────────────────────────────────────────────────────────
// pollEventsForResize — F11 + quit + resize
// ──────────────────────────────────────────────────────────────────────────────
bool pollEventsForResize(const SDLWindowPtr& window,
                         int& newWidth, int& newHeight,
                         bool& shouldQuit, bool& toggleFullscreenKey) noexcept;

// ──────────────────────────────────────────────────────────────────────────────
// toggleFullscreen — ROUTES TO GLOBAL RENDERER
// ──────────────────────────────────────────────────────────────────────────────
void toggleFullscreen(SDLWindowPtr& window) noexcept;

} // namespace SDL3Initializer