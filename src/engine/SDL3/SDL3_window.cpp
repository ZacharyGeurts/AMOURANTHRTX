// =============================================================================
// src/engine/SDL3/SDL3_window.cpp
// AMOURANTH RTX — STONEKEY v∞ ACTIVE — SDL3 PURE — PINK PHOTONS ETERNAL
// NOVEMBER 20, 2025 — APOCALYPSE FINAL — RESIZE PERFECTED
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

#include "engine/SDL3/SDL3_window.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"

#include <SDL3/SDL.h>

using namespace Logging::Color;

// =============================================================================
// STONEKEY v∞ RAII — RAW WINDOW NEVER LEAKS
// =============================================================================

struct SDLWindowDeleter {
    void operator()(SDL_Window* w) const noexcept {
        if (w) SDL_DestroyWindow(w);
    }
};
using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLWindowDeleter>;
thread_local SDLWindowPtr g_stonekey_window;

// =============================================================================
// LEGACY GLOBAL ATOMICS — DEFINED HERE SO LINKER IS HAPPY
// =============================================================================

namespace SDL3Window {
std::atomic<int>  g_resizeWidth{0};
std::atomic<int>  g_resizeHeight{0};
std::atomic<bool> g_resizeRequested{false};
}

// =============================================================================
// INTERNAL DEBOUNCE STATE
// =============================================================================

namespace {
    uint64_t g_last_resize_time = 0;
    constexpr uint64_t RESIZE_DEBOUNCE_MS = 100;
}

// =============================================================================
// PUBLIC INTERFACE
// =============================================================================

namespace SDL3Window {

SDL_Window* get() noexcept { return g_stonekey_window.get(); }

void create(const char* title, int width, int height, Uint32 flags)
{
    flags |= SDL_WINDOW_VULKAN | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_RESIZABLE;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        std::abort();
    }

    SDL_Window* raw = SDL_CreateWindow(title, width, height, flags);
    if (!raw) std::abort();

    g_stonekey_window.reset(raw);
}

std::vector<std::string> getVulkanExtensions(SDL_Window* window) noexcept
{
    if (!window) window = get();
    if (!window) return {};

    unsigned int count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(&count)) return {};

    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&count);
    return {exts, exts + count};
}

// =============================================================================
// FINAL pollEvents — FULL FEATURE, NO LOGGING, PERFECT COMPATIBILITY
// =============================================================================

bool pollEvents(int& outW, int& outH, bool& quit, bool& toggleFS) noexcept
{
    SDL_Event ev;
    quit = toggleFS = false;
    bool hadEvent = false;

    while (SDL_PollEvent(&ev)) {
        hadEvent = true;

        switch (ev.type) {
            case SDL_EVENT_QUIT:
                quit = true;
                break;

            case SDL_EVENT_KEY_DOWN:
                if (ev.key.scancode == SDL_SCANCODE_F11)
                    toggleFS = true;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED: {
                int w = ev.window.data1;
                int h = ev.window.data2;

                if (w <= 0 || h <= 0) break;  // Minimized

                // Legacy signaling for handle_app.cpp
                g_resizeWidth.store(w);
                g_resizeHeight.store(h);
                g_resizeRequested.store(true);

                // Internal debounce
                g_last_resize_time = SDL_GetTicks();
                break;
            }
        }
    }

    // Always report current size
    if (get()) {
        SDL_GetWindowSizeInPixels(get(), &outW, &outH);
    } else {
        outW = outH = 0;
    }

    // Debounced swapchain recreation
    if (g_resizeRequested.load() &&
        (SDL_GetTicks() - g_last_resize_time >= RESIZE_DEBOUNCE_MS)) {

        int w = g_resizeWidth.load();
        int h = g_resizeHeight.load();

        SwapchainManager::get().recreate(static_cast<uint32_t>(w), static_cast<uint32_t>(h));
        SwapchainManager::get().updateWindowTitle(get(), 0.0f);

        g_resizeRequested.store(false);
    }

    return hadEvent;
}

void toggleFullscreen() noexcept
{
    if (!get()) return;

    Uint32 flags = SDL_GetWindowFlags(get());
    bool isFS = (flags & SDL_WINDOW_FULLSCREEN);
    SDL_SetWindowFullscreen(get(), !isFS);

    // Force immediate resize handling
    g_last_resize_time = SDL_GetTicks() - RESIZE_DEBOUNCE_MS - 1;
    g_resizeRequested.store(true);
}

void destroy() noexcept
{
    g_stonekey_window.reset();
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
}

} // namespace SDL3Window