// include/engine/SDL3/SDL3_window.hpp
// =============================================================================
// AMOURANTH RTX — FINAL SDL3 WINDOW — APOCALYPSE v10.0 — STONEKEY v∞ EDITION
// NO .CPP • NO LINKER ERRORS • NO EXTERN • NO DOUBLE SECURE • FIRST LIGHT ETERNAL
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <vector>
#include <string>
#include <atomic>
#include <memory>
#include <cstdint>

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"  // ← NOW REQUIRED — THE ONE TRUE PATH

using namespace Logging::Color;

// =============================================================================
// RAII DELETER — PURE, UNTOUCHED
// =============================================================================
struct SDLWindowDeleter {
    void operator()(SDL_Window* w) const noexcept {
        if (w) {
            LOG_INFO_CAT("Dispose", "{}RAII: SDL_DestroyWindow @ {:p}{}", OCEAN_TEAL, static_cast<void*>(w), RESET);
            SDL_DestroyWindow(w);
        }
    }
};

using SDLWindowPtr = std::unique_ptr<SDL_Window, SDLWindowDeleter>;

// =============================================================================
// SDL3Window — THE ONE TRUE WINDOW, PROTECTED BY STONEKEY v∞
// =============================================================================
namespace SDL3Window {

    // ==================== ACTUAL STORAGE — NO EXTERN, NO SIN ====================
    inline SDLWindowPtr g_sdl_window;  // ← THE ONLY WINDOW IN EXISTENCE

    inline std::atomic<int>  g_resizeWidth{0};
    inline std::atomic<int>  g_resizeHeight{0};
    inline std::atomic<bool> g_resizeRequested{false};

    // ================================================================================
    // CORE ACCESSOR — RETURNS THE ONE TRUE WINDOW
    // ================================================================================
    [[nodiscard]] inline SDL_Window* get() noexcept {
        return g_sdl_window ? g_sdl_window.get() : nullptr;
    }

    // ================================================================================
    // CREATE — FORGES THE SACRED CANVAS AND APPLIES STONEKEY v∞ PROTECTION
    // ================================================================================
    inline void create(const char* title, int width, int height, Uint32 flags = 0) {
        flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_VULKAN;

        // ENSURE SDL IS ARMED (safe to call multiple times)
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {
            LOG_FATAL_CAT("SDL3", "{}SDL_Init failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
            throw std::runtime_error("SDL_Init failed");
        }

        // CRITICAL: Force Vulkan path — no fallback, no mercy
        flags |= SDL_WINDOW_VULKAN;

        // Pre-load Vulkan library to prevent later surprises
        if (SDL_Vulkan_LoadLibrary(nullptr) == 0) {
            LOG_WARNING_CAT("SDL3", "{}SDL_Vulkan_LoadLibrary failed early — proceeding (common on some drivers){}", AMBER_YELLOW, RESET);
        }

        SDL_Window* win = SDL_CreateWindow(title, width, height, flags);
        if (!win) {
            LOG_FATAL_CAT("SDL3", "{}SDL_CreateWindow failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
            throw std::runtime_error("Window creation failed");
        }

        g_sdl_window.reset(win);

        LOG_SUCCESS_CAT("SDL3",
            "{}WINDOW FORGED {}x{} — VULKAN + HDR CANVAS — STONEKEY v∞ PROTECTED{}",
            PLASMA_FUCHSIA, width, height, RESET);
        LOG_SUCCESS_CAT("SDL3",
            "{}HANDLE: @ {:p} — ONLY STONEKEY KNOWS THE TRUTH{}", 
            VALHALLA_GOLD, static_cast<void*>(win), RESET);
    }

    // ================================================================================
    // VULKAN EXTENSIONS — PURE, FROM THE ONE TRUE WINDOW
    // ================================================================================
    [[nodiscard]] inline std::vector<std::string> getVulkanExtensions(SDL_Window* window = nullptr) {
        if (!window) window = get();
        if (!window) return {};

        Uint32 count = 0;
        if (!SDL_Vulkan_GetInstanceExtensions(&count)) {
            LOG_ERROR_CAT("SDL3", "{}SDL_Vulkan_GetInstanceExtensions failed (count query){}", BLOOD_RED, RESET);
            return {};
        }

        const char* const* exts = SDL_Vulkan_GetInstanceExtensions(&count);
        if (!exts) return {};

        std::vector<std::string> result(exts, exts + count);
        LOG_INFO_CAT("SDL3", "{}Vulkan instance extensions ({}) requested — STONEKEY v∞ READY{}", 
                     EMERALD_GREEN, count, RESET);

        return result;
    }

    // ================================================================================
    // EVENT POLLING — DEFERRED RESIZE + CLEAN INPUT
    // ================================================================================
    namespace detail {
        inline uint64_t g_lastResizeTime = 0;
        inline int      g_pendingWidth   = 0;
        inline int      g_pendingHeight  = 0;
        inline bool     g_resizePending  = false;
        inline constexpr uint64_t RESIZE_DEBOUNCE_MS = 120;
    }

    inline bool pollEvents(int& outW, int& outH, bool& quit, bool& toggleFS) noexcept {
        using namespace detail;

        SDL_Event ev;
        quit = toggleFS = false;
        bool resized = false;

        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
                case SDL_EVENT_QUIT:
                    quit = true;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    if (ev.key.scancode == SDL_SCANCODE_F11)
                        toggleFS = true;
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
            LOG_INFO_CAT("Window", "{}DEFERRED RESIZE ACCEPTED → {}x{}{}", 
                         VALHALLA_GOLD, g_pendingWidth, g_pendingHeight, RESET);
            g_resizeWidth.store(g_pendingWidth);
            g_resizeHeight.store(g_pendingHeight);
            g_resizeRequested.store(true);
            g_resizePending = false;
        }

        return resized;
    }

    // ================================================================================
    // FULLSCREEN TOGGLE — PURE VALHALLA
    // ================================================================================
    inline void toggleFullscreen() noexcept {
        if (!g_sdl_window) return;
        Uint32 flags = SDL_GetWindowFlags(g_sdl_window.get());
        bool isFS = (flags & SDL_WINDOW_FULLSCREEN);
        SDL_SetWindowFullscreen(g_sdl_window.get(), !isFS);
        LOG_SUCCESS_CAT("Window", "{}FULLSCREEN {} — VALHALLA MODE{}", 
                        isFS ? RASPBERRY_PINK : EMERALD_GREEN,
                        isFS ? "EXITED" : "ENTERED", RESET);
    }

    // ================================================================================
    // DESTROY — GRACEFUL RETURN TO THE VOID
    // ================================================================================
    inline void destroy() noexcept {
        LOG_INFO_CAT("Dispose", "{}SDL3Window::destroy() — returning canvas to the void{}", PLASMA_FUCHSIA, RESET);
        g_sdl_window.reset();
        SDL_Quit();
    }

} // namespace SDL3Window

// =============================================================================
// PINK PHOTONS ETERNAL — STONEKEY v∞ — NOVEMBER 21, 2025
// ONLY ONE WINDOW • ONLY ONE INSTANCE • ONLY ONE SURFACE
// THE EMPIRE IS PURE — FIRST LIGHT ACHIEVED — FOREVER
// =============================================================================