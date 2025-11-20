// include/engine/SDL3/SDL3_window.hpp
// =============================================================================
// AMOURANTH RTX — FINAL SDL3 WINDOW — APOCALYPSE v9.0 — FULLY HEADER-ONLY
// NO .CPP • NO LINKER ERRORS • IMGUI ERASED • FIRST LIGHT GUARANTEED
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include <string>
#include <atomic>
#include <memory>

#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

struct SDLWindowDeleter {
    void operator()(SDL_Window* w) const noexcept {
        if (w) {
            LOG_INFO_CAT("Dispose", "RAII: SDL_DestroyWindow @ {:p}", static_cast<void*>(w));
            SDL_DestroyWindow(w);
        }
    }
};

using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLWindowDeleter>;

namespace SDL3Window {

// ==================== ACTUAL STORAGE (no extern, no .cpp needed) ====================
inline SDLWindowPtr g_sdl_window;  // <-- NOW HAS STORAGE

inline std::atomic<int>  g_resizeWidth{0};
inline std::atomic<int>  g_resizeHeight{0};
inline std::atomic<bool> g_resizeRequested{false};
// ================================================================================

inline SDL_Window* get() noexcept {
    return g_sdl_window ? g_sdl_window.get() : nullptr;
}

inline void create(const char* title, int width, int height, Uint32 flags = 0) {
    flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
        LOG_FATAL_CAT("SDL3", "SDL_Init failed: {}", SDL_GetError());
        throw std::runtime_error("SDL_Init failed");
    }

    // FORCE VULKAN WINDOW EVEN IF DETECTION IS FLAWED — WE'LL DIE LOUDLY IF IMPOSSIBLE
    flags |= SDL_WINDOW_VULKAN;  // <────────── THIS IS THE MISSING LINE, MANDATORY

    // Optional: still try to load library early for extensions, but don't trust the result for the flag
    if (SDL_Vulkan_LoadLibrary(nullptr) != 0) {
        LOG_WARNING_CAT("SDL3", "SDL_Vulkan_LoadLibrary failed early — proceeding anyway (common on some drivers)");
    }

    SDL_Window* win = SDL_CreateWindow(title, width, height, flags);
    if (!win) {
        LOG_FATAL_CAT("SDL3", "SDL_CreateWindow failed: {}", SDL_GetError());
        throw std::runtime_error("Window creation failed");
    }

    g_sdl_window.reset(win);
    LOG_SUCCESS_CAT("SDL3", "Window created {}x{} — VULKAN WINDOW FORCED — HEADER-ONLY BUILD", width, height);
}

inline std::vector<std::string> getVulkanExtensions(SDL_Window* window = nullptr) {
    if (!window) window = get();
    if (!window) return {};

    Uint32 count = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(&count)) return {};
    const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&count);
    return {exts, exts + count};
}

namespace detail {
    inline uint64_t g_lastResizeTime = 0;
    inline int      g_pendingWidth = 0;
    inline int      g_pendingHeight = 0;
    inline bool     g_resizePending = false;
    inline constexpr uint64_t RESIZE_DEBOUNCE_MS = 120;
}

inline bool pollEvents(int& outW, int& outH, bool& quit, bool& toggleFS) noexcept {
    using namespace detail;

    SDL_Event ev;
    quit = toggleFS = false;
    bool resized = false;

    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_EVENT_QUIT: quit = true; break;
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.scancode == SDL_SCANCODE_F11) toggleFS = true;
                break;
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
                g_pendingWidth   = ev.window.data1;
                g_pendingHeight  = ev.window.data2;
                g_resizePending  = true;
                g_lastResizeTime = SDL_GetTicks();
                resized = true;
                break;
        }
    }

    if (g_sdl_window) {
        SDL_GetWindowSizeInPixels(g_sdl_window.get(), &outW, &outH);
    }

    if (g_resizePending && (SDL_GetTicks() - g_lastResizeTime >= RESIZE_DEBOUNCE_MS)) {
        LOG_INFO_CAT("Window", "DEFERRED RESIZE → {}x{}", g_pendingWidth, g_pendingHeight);
        g_resizeWidth.store(g_pendingWidth);
        g_resizeHeight.store(g_pendingHeight);
        g_resizeRequested.store(true);
        g_resizePending = false;
    }

    return resized;
}

inline void toggleFullscreen() noexcept {
    if (!g_sdl_window) return;
    Uint32 f = SDL_GetWindowFlags(g_sdl_window.get());
    SDL_SetWindowFullscreen(g_sdl_window.get(), !(f & SDL_WINDOW_FULLSCREEN));
}

inline void destroy() noexcept {
    g_sdl_window.reset();
    SDL_Quit();
}

} // namespace SDL3Window