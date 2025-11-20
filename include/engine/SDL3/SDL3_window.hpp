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
// SDL3_window.hpp — STONEKEY v∞ PROTECTED — APOCALYPSE FINAL 2025 AAAA
// NO INCLUDES. FORWARD DECLARE ONLY. PURE.
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include <string>
#include <atomic>

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

// Forward declaration only — StoneKey v∞ rule
namespace StoneKey::Raw { struct Cache; }

namespace SDL3Window {

[[nodiscard]] SDL_Window* get() noexcept;

void create(const char* title, int width, int height, Uint32 flags = 0);

[[nodiscard]] std::vector<std::string> getVulkanExtensions(SDL_Window* window = nullptr);

bool pollEvents(int& outW, int& outH, bool& quit, bool& toggleFS) noexcept;
void toggleFullscreen() noexcept;
void destroy() noexcept;

// Thread-safe resize signaling — used by renderer
extern std::atomic<int>  g_resizeWidth;
extern std::atomic<int>  g_resizeHeight;
extern std::atomic<bool> g_resizeRequested;

} // namespace SDL3Window