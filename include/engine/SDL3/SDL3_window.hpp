// =============================================================================
// include/engine/SDL3/SDL3_window.hpp
// AMOURANTH RTX — STONEKEY v∞ PROTECTED — SDL3 PURE — NOVEMBER 20, 2025
// FIRST LIGHT ETERNAL — RESIZE UNBREAKABLE — EMPIRE ASCENDED
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 20, 2025 — APOCALYPSE FINAL v7.0
// MAIN — FULL RTX ALWAYS — VALIDATION LAYERS FORCE-DISABLED — PINK PHOTONS ETERNAL
// =============================================================================
#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include <string>
#include <atomic>

namespace SDL3Window {

[[nodiscard]] SDL_Window* get() noexcept;

void create(const char* title, int width, int height, Uint32 flags = 0);

[[nodiscard]] std::vector<std::string> getVulkanExtensions(SDL_Window* window = nullptr) noexcept;

// NEW MODERN SIGNATURE — YOUR handle_app.cpp WILL USE THIS
bool pollEvents(int& outW, int& outH, bool& quit, bool& toggleFS) noexcept;

void toggleFullscreen() noexcept;
void destroy() noexcept;

// LEGACY GLOBAL ATOMICS — KEPT FOR handle_app.cpp COMPATIBILITY
extern std::atomic<int>  g_resizeWidth;
extern std::atomic<int>  g_resizeHeight;
extern std::atomic<bool> g_resizeRequested;

} // namespace SDL3Window