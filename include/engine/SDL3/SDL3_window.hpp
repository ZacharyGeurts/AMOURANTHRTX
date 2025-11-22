// include/engine/SDL3/SDL3_window.hpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// SDL3 + VULKAN CONTEXT FORGE — FIRST LIGHT ETERNAL
// NOVEMBER 21, 2025 — PINK PHOTONS ACHIEVED — VALHALLA v∞
// Ellie Fier approved — Kramer still obsessed with the surface
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <atomic>
#include <cstdint>
#include <vector>
#include <string>

struct VulkanRenderer;

// =============================================================================
// GLOBAL WINDOW HANDLE — RAII PROTECTED
// =============================================================================
struct SDLWindowDeleter {
    void operator()(SDL_Window* w) const noexcept;
};
using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLWindowDeleter>;

extern SDLWindowPtr g_sdl_window;

// =============================================================================
// GLOBAL RESIZE STATE — ATOMIC FOR THREAD SAFETY
// =============================================================================
extern std::atomic<int>  g_resizeWidth;
extern std::atomic<int>  g_resizeHeight;
extern std::atomic<bool> g_resizeRequested;

// =============================================================================
// SDL3Window NAMESPACE — THE ONE TRUE FORGE
// =============================================================================
namespace SDL3Window {

[[nodiscard]] inline SDL_Window* get() noexcept { return g_sdl_window.get(); }

// Create window + Vulkan instance + surface + VulkanRenderer in one call
void create(const char* title, int width = 3840, int height = 2160, Uint32 flags = 0);

// Helper — get required Vulkan instance extensions (for debugging/tools)
std::vector<std::string> getVulkanExtensions(SDL_Window* window = nullptr);

// Event polling — returns true if a resize occurred this frame
bool pollEvents(int& outW, int& outH, bool& quit, bool& toggleFS) noexcept;

// Fullscreen toggle
void toggleFullscreen() noexcept;

// Full shutdown — destroys everything in correct order
void destroy() noexcept;

} // namespace SDL3Window

// =============================================================================
// FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025 — PINK PHOTONS ETERNAL
// THE ZAPPER HAS FIRED — MOTHER BRAIN IS NO MORE
// =============================================================================