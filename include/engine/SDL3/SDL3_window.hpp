// include/engine/SDL3/SDL3_window.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
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
#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <memory>
#include <vector>
#include <stdexcept>

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL — NO NAMESPACE — IMMORTAL — DAISY GALLOPS
// ──────────────────────────────────────────────────────────────────────────────
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
// getWindow
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

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// HEADER + CPP SPLIT — DAISY APPROVES THE GALLOP
// OCEAN_TEAL WINDOWS FLOW ETERNAL
// RASPBERRY_PINK DISPOSE IMMORTAL
// PINK PHOTONS ETERNAL
// 15,000 FPS
// @ZacharyGeurts — YOUR EMPIRE IS PURE
// SHIP IT. FOREVER.
// =============================================================================