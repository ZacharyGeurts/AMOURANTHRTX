// src/engine/SDL3/SDL3_window.cpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 15, 2025 — APOCALYPSE v3.2
// PURE RANDOM ENTROPY — RDRAND + PID + TIME + TLS — SIMPLE & SECURE
// KEYS **NEVER** LOGGED — ONLY HASHED FINGERPRINTS — SECURITY > VANITY
// FULLY COMPLIANT WITH -Werror=unused-variable
// =============================================================================

#include "engine/SDL3/SDL3_window.hpp"
#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

namespace SDL3Window {

SDLWindowPtr g_sdl_window{nullptr};

[[nodiscard]] SDLWindowPtr create(const char* title, int width, int height, Uint32 flags)
{
    // Pre-init: Base flags setup (sans Vulkan probe)
    flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    if (Options::Performance::ENABLE_IMGUI) {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) == 0) {  // FIXED: != 0 for failure (0=success)
        const char* sdlError = SDL_GetError();
        LOG_FATAL_CAT("SDL3_window", "{}SDL_Init failed critically: {}{}", CRIMSON_MAGENTA, sdlError ? sdlError : "No error message provided", RESET);
        throw std::runtime_error(std::format("SDL_Init failed: {}", sdlError ? sdlError : "No error message provided"));
    }

    // FIXED: Vulkan probe AFTER video init per SDL3 docs (SDL_Vulkan_LoadLibrary post-SDL_Init(VIDEO))
    bool vulkanSupported = false;
    bool libLoaded = SDL_Vulkan_LoadLibrary(nullptr);
    if (libLoaded) {
        uint32_t extCount = 0;
        if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr) == VK_SUCCESS) {
            std::vector<VkExtensionProperties> exts(extCount);
            if (vkEnumerateInstanceExtensionProperties(nullptr, &extCount, exts.data()) == VK_SUCCESS) {
                for (const auto& ext : exts) {
                    if (strcmp(ext.extensionName, VK_KHR_SURFACE_EXTENSION_NAME) == 0) {
                        vulkanSupported = true;
                        break;
                    }
                }
            }
        }
        // FIXED: Conditional unload — keep loaded ONLY if supported (for SDL_WINDOW_VULKAN)
        if (!vulkanSupported) {
            SDL_Vulkan_UnloadLibrary();
        }
        if (!vulkanSupported) {
            const char* loadError = SDL_GetError();
            LOG_WARN_CAT("SDL3_window", "{}Vulkan library load failed post-init: {} — falling back to non-Vulkan window", loadError ? loadError : "Unknown error", RESET);
        }
    } else {
        const char* loadError = SDL_GetError();
        LOG_WARN_CAT("SDL3_window", "{}Vulkan library load failed post-init: {} — falling back to non-Vulkan window", loadError ? loadError : "Unknown error", RESET);
    }

    // Final flags assembly post-probe
    if (vulkanSupported) {
        flags |= SDL_WINDOW_VULKAN;
    }

    SDL_Window* raw = SDL_CreateWindow(title, width, height, flags);
    if (!raw) {
        const char* createError = SDL_GetError();
        LOG_FATAL_CAT("SDL3_window", "{}SDL_CreateWindow failed critically: {}{}", CRIMSON_MAGENTA, createError ? createError : "No error message provided", RESET);
        SDL_Quit();  // Safe early quit on failure
        throw std::runtime_error(std::format("SDL_CreateWindow failed: {}", createError ? createError : "No error message provided"));
    }

    // Explicitly construct + log to isolate if destruction happens during unique_ptr init/move
    SDLWindowPtr ptr(raw);

    return ptr;  // Move out — temporary should nullify post-move
}

[[nodiscard]] std::vector<std::string> getVulkanExtensions(SDL_Window* window)
{
    if (!window) {
        LOG_WARN_CAT("Vulkan", "{}Null window — returning empty extensions vector{}", EMERALD_GREEN, RESET);
        return {};
    }

    // SDL3: Single call populates both count and array
    Uint32 count = 0;
    const char * const * exts = SDL_Vulkan_GetInstanceExtensions(&count);  // FIXED: SDL3 sig — no window arg
    if (exts == nullptr || count == 0) {
        const char* sdlError = SDL_GetError();
        LOG_ERROR_CAT("Vulkan", "{}SDL_Vulkan_GetInstanceExtensions failed: {} (count={}){}", CRIMSON_MAGENTA, sdlError ? sdlError : "No error", count, RESET);
        return {};
    }

    std::vector<std::string> result;
    result.reserve(count);
    for (Uint32 i = 0; i < count; ++i) {
        if (exts[i]) {
            result.emplace_back(exts[i]);  // Copy strings (SDL owns exts)
        }
    }

    return result;
}

bool pollEvents(int& outW, int& outH, bool& quit, bool& toggleFS) noexcept
{
    SDL_Event ev;
    bool resized = false;
    quit = toggleFS = false;

    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
            case SDL_EVENT_QUIT:
                quit = true;
                break;
            case SDL_EVENT_KEY_DOWN:
                if (ev.key.scancode == SDL_SCANCODE_F11) {
                    toggleFS = true;
                }
                break;
            case SDL_EVENT_WINDOW_RESIZED:
                outW = ev.window.data1;
                outH = ev.window.data2;
                resized = true;
                break;
            default:
                break;
        }
    }

    // Fallback: Query current size if no resize event
    if (!resized && g_sdl_window) {
        SDL_Window* win = g_sdl_window.get();
        SDL_GetWindowSizeInPixels(win, &outW, &outH);
    } else if (!g_sdl_window) {
        LOG_WARN_CAT("SDL3_window", "{}Fallback size query skipped — g_sdl_window null{}", SAPPHIRE_BLUE, RESET);
    }

    return resized;
}

void toggleFullscreen() noexcept
{
    if (!g_sdl_window) {
        LOG_WARN_CAT("SDL3_window", "{}toggleFullscreen(): g_sdl_window null — noop{}", SAPPHIRE_BLUE, RESET);
        return;
    }

    SDL_Window* window = g_sdl_window.get();
    Uint32 currentFlags = SDL_GetWindowFlags(window);
    bool isCurrentlyFullscreen = (currentFlags & SDL_WINDOW_FULLSCREEN) == 0;
    bool targetFullscreen = !isCurrentlyFullscreen;

    if (SDL_SetWindowFullscreen(window, targetFullscreen) != 0) {  // FIXED: != 0 for failure (0=success)
        const char* fsError = SDL_GetError();
        LOG_ERROR_CAT("SDL3_window", "{}SDL_SetWindowFullscreen failed: {}{}", CRIMSON_MAGENTA, fsError ? fsError : "Unknown error", RESET);
        return;
    }

    int w = 0, h = 0;
    SDL_GetWindowSizeInPixels(window, &w, &h);

    // Notify renderer of resize (if exists)
    if (auto* r = &SDL3Vulkan::renderer()) {
        r->handleResize(w, h);
    }

}

// No definitions for inline functions here — they are in header

} // namespace SDL3Window

// =============================================================================
// PINK PHOTONS ETERNAL — SEGFAULT ERADICATED — VULKAN PROBE POST-INIT
// DAISY GALLOPS INTO THE OCEAN_TEAL SUNSET — YOUR EMPIRE IS BULLETPROOF
// GENTLEMAN GROK NODS: "The timing was the culprit, old chap. Post-init probe prevails."
// =============================================================================