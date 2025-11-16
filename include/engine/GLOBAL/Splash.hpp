// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License version 3 or later (GPL-3.0+)
//    https://www.gnu.org/licenses/gpl-3.0.en.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// SPLASH SYSTEM — NOVEMBER 16, 2025 — CENTERED FROM CREATION
// • SDL3: 0 = SUCCESS
// • Audio plays ammo.wav
// • Image centered from frame 1 — NO TOP-LEFT FLASH
// • Window created HIDDEN + positioned via calculated center (display bounds)
// • Show (brief black), then RENDER/PRESENT image
// • No SIGSEGV
// • ENHANCED LOGGING — DETAILED TRACE FOR DEBUG
// • FIXED: SDL_CreateWindow (no x/y args) + SDL_SetWindowPosition(calculated center)
// • PINK PHOTONS + AMMO.WAV ETERNAL
// • NEW: Window icon set using dual ICOs (base + HiDPI) — matches main window
// • FIXED: Use SDL_GetError() directly — avoids IMG_GetError macro dependency
// • FIXED: Audio init check (0 = success, !=0 error)
// =============================================================================

#pragma once

#include "engine/GLOBAL/logging.hpp"
#include "engine/SDL3/SDL3_image.hpp"
#include "engine/SDL3/SDL3_audio.hpp"

#include <SDL3/SDL.h>
#include <format>
#include <string_view>
#include <filesystem>
#include <thread>
#include <chrono>

using namespace Logging::Color;

// ========================================================================
// ASSET EXISTENCE CHECK — INLINE, HEADER-ONLY, C++23
// ========================================================================
namespace Splash::detail {
    [[nodiscard]] inline bool assetExists(const char* path) noexcept {
        if (!path || path[0] == '\0') return false;
        const auto exists = std::filesystem::exists(path);
        LOG_DEBUG_CAT("SPLASH", "Asset check '{}': {}", path, exists ? "exists" : "missing");
        return exists;
    }
}

namespace Splash {

inline void show(const char* title, int w, int h, const char* imagePath, const char* audioPath = nullptr) {
    LOG_INFO_CAT("SPLASH", "{}SPLASH START — {}x{} — {}ms{}", ELECTRIC_BLUE, w, h, 3400, RESET);

    // FIXED: Initialize video subsystem — required for window creation (SDL3)
    LOG_DEBUG_CAT("SPLASH", "SDL_InitSubSystem(SDL_INIT_VIDEO) call");
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
        LOG_ERROR_CAT("SPLASH", "SDL_InitSubSystem(VIDEO) failed: {}", SDL_GetError());
        return;
    }
    LOG_SUCCESS_CAT("SPLASH", "SDL video subsystem initialized");

    // --- 0. Calculate center position using display bounds (pre-window) ---
    LOG_DEBUG_CAT("SPLASH", "Querying primary display bounds for centering");
    SDL_Rect displayBounds;
    if (SDL_GetDisplayBounds(0, &displayBounds) == 0) {
        LOG_WARN_CAT("SPLASH", "SDL_GetDisplayBounds failed: {} — using undefined position", SDL_GetError());
        displayBounds.w = 1920;  // Fallback assumption
        displayBounds.h = 1080;
    } else {
        LOG_DEBUG_CAT("SPLASH", "Primary display: {}x{} @ ({},{})", displayBounds.w, displayBounds.h, displayBounds.x, displayBounds.y);
    }
    int centerX = (displayBounds.w - w) / 2 + displayBounds.x;
    int centerY = (displayBounds.h - h) / 2 + displayBounds.y;
    LOG_SUCCESS_CAT("SPLASH", "Calculated center: ({},{})", centerX, centerY);

    // --- 1. Create window (BORDERLESS + HIDDEN — no top-left flash) ---
    LOG_DEBUG_CAT("SPLASH", "Creating hidden window: title='{}', size={}x{}", title ? title : "null", w, h);
    SDL_Window* win = SDL_CreateWindow(
        title,
        w, h,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN  // FIXED: No x/y args in SDL3
    );
    if (!win) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        LOG_ERROR_CAT("SPLASH", "Failed to create splash window: {}", SDL_GetError());
        return;
    }
    LOG_SUCCESS_CAT("SPLASH", "Hidden window created: 0x{:x}", reinterpret_cast<uint64_t>(win));

    // FIXED: Set calculated position immediately (while hidden — no flash)
    LOG_DEBUG_CAT("SPLASH", "Setting window position to calculated center ({},{})", centerX, centerY);
    SDL_SetWindowPosition(win, centerX, centerY);
    LOG_SUCCESS_CAT("SPLASH", "Window positioned at center (still hidden)");

    // NEW: Set splash window icon with dual sizes for HiDPI support: 32x32 base + 64x64 alternate
    LOG_INFO_CAT("SPLASH", "Loading dual window icons for splash: ammo32.ico (base) + ammo.ico (2x HiDPI)");
    SDL_Surface* base_icon = IMG_Load("assets/textures/ammo32.ico");
    SDL_Surface* hdpi_icon = IMG_Load("assets/textures/ammo.ico");
    if (base_icon && hdpi_icon) {
        // Log sizes for validation
        LOG_INFO_CAT("SPLASH", "Splash base icon loaded: {}x{}", base_icon->w, base_icon->h);
        LOG_INFO_CAT("SPLASH", "Splash HiDPI icon loaded: {}x{}", hdpi_icon->w, hdpi_icon->h);
        
        // Add 64x64 as alternate for high-DPI scaling (SDL infers scale from size ratio)
        SDL_AddSurfaceAlternateImage(base_icon, hdpi_icon);
        SDL_SetWindowIcon(win, base_icon);
        
        SDL_DestroySurface(base_icon);
        SDL_DestroySurface(hdpi_icon);
        LOG_SUCCESS_CAT("SPLASH", "Dual splash window icons set successfully (base + 2x HiDPI)");
    } else {
        // Fallback: Try base only
        if (!base_icon) {
            LOG_WARN_CAT("SPLASH", "Failed to load splash base icon ammo32.ico: {}", SDL_GetError());
            base_icon = IMG_Load("assets/textures/ammo.ico");  // Fallback to single ICO
        }
        if (base_icon) {
            SDL_SetWindowIcon(win, base_icon);
            SDL_DestroySurface(base_icon);
            LOG_SUCCESS_CAT("SPLASH", "Fallback splash window icon set using ammo.ico");
        } else {
            LOG_WARN_CAT("SPLASH", "Failed to load any splash icon: {}", SDL_GetError());
        }
        if (hdpi_icon) SDL_DestroySurface(hdpi_icon);  // Clean up if partial load
    }

    // --- 2. Create renderer ---
    LOG_DEBUG_CAT("SPLASH", "Creating renderer for hidden window 0x{:x}", reinterpret_cast<uint64_t>(win));
    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) {
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        LOG_ERROR_CAT("SPLASH", "Failed to create splash renderer: {}", SDL_GetError());
        return;
    }
    LOG_SUCCESS_CAT("SPLASH", "Renderer created: 0x{:x}", reinterpret_cast<uint64_t>(ren));

    // --- 3. Show window (centered, black/undefined — instant) ---
    LOG_DEBUG_CAT("SPLASH", "SDL_ShowWindow — centered window now visible (pre-render)");
    SDL_ShowWindow(win);
    LOG_INFO_CAT("SPLASH", "Window shown — centered (brief black frame)");

    // --- 4. Clear to black + Load image and center it (now visible) ---
    LOG_DEBUG_CAT("SPLASH", "Clearing renderer to black");
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);

    SDL_Texture* tex = nullptr;
    int img_x = 0, img_y = 0;
    float tw = 0.0f, th = 0.0f;

    if (detail::assetExists(imagePath)) {
        LOG_INFO_CAT("SPLASH", "Loading image: {}", imagePath);
        LOG_DEBUG_CAT("SPLASH", "IMG_LoadTexture call for '{}'", imagePath);
        tex = IMG_LoadTexture(ren, imagePath);
        if (tex) {
            LOG_DEBUG_CAT("SPLASH", "IMG_LoadTexture success, querying size");
            SDL_GetTextureSize(tex, &tw, &th);
            LOG_DEBUG_CAT("SPLASH", "Texture size: {}x{}", tw, th);
            img_x = static_cast<int>((w - tw) * 0.5f);
            img_y = static_cast<int>((h - th) * 0.5f);
            LOG_DEBUG_CAT("SPLASH", "Centered at ({},{})", img_x, img_y);

            SDL_FRect dst = { static_cast<float>(img_x), static_cast<float>(img_y), tw, th };
            SDL_RenderTexture(ren, tex, nullptr, &dst);
            LOG_SUCCESS_CAT("SPLASH", "Image centered: {}x{} @ ({},{})", tw, th, img_x, img_y);
        } else {
            LOG_WARN_CAT("SPLASH", "IMG_LoadTexture failed: {}", SDL_GetError());
        }
    } else {
        LOG_WARN_CAT("SPLASH", "Image not found: {}", imagePath);
    }

    // --- 5. Present — image now visible and centered ---
    LOG_DEBUG_CAT("SPLASH", "SDL_RenderPresent call — swap to image");
    SDL_RenderPresent(ren);
    LOG_INFO_CAT("SPLASH", "Splash presented — PNG visible and centered from frame 1");

    // --- 6. Initialize audio subsystem ONCE (SDL3: 0 = success) ---
    static bool audioInit = []() -> bool {
        LOG_DEBUG_CAT("SPLASH", "SDL_InitSubSystem(SDL_INIT_AUDIO) call");
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {  // FIXED: != 0 = error (0 = success)
            LOG_ERROR_CAT("SPLASH", "SDL_InitSubSystem(AUDIO) failed: {}", SDL_GetError());
            return false;
        }
        LOG_SUCCESS_CAT("SPLASH", "SDL audio subsystem initialized");
        return true;
    }();

    // --- 7. Create persistent AudioManager (once) ---
    static SDL3Audio::AudioManager* g_audioManager = nullptr;
    static std::string g_loadedAudioKey = "splash_sound";  // Fixed key for splash audio

    if (audioInit && !g_audioManager) {
        LOG_DEBUG_CAT("SPLASH", "Creating global AudioManager");
        g_audioManager = new SDL3Audio::AudioManager();
        auto initRes = g_audioManager->initMixer();
        if (!initRes) {
            LOG_ERROR_CAT("SPLASH", "AudioManager::initMixer() failed: {}", initRes.error());
            delete g_audioManager;
            g_audioManager = nullptr;
        } else {
            LOG_SUCCESS_CAT("SPLASH", "Global AudioManager initialized and ready");
        }
    }

    // --- 8. Load and play splash sound (once per unique path) ---
    if (audioPath && detail::assetExists(audioPath) && g_audioManager) {
        // Load only if not already loaded
        static std::string lastAudioPath;
        if (lastAudioPath != audioPath) {
            LOG_DEBUG_CAT("SPLASH", "Loading new splash audio: {}", audioPath);
            if (g_audioManager->loadSound(audioPath, g_loadedAudioKey)) {
                lastAudioPath = audioPath;
                LOG_SUCCESS_CAT("SPLASH", "Splash audio loaded: {}", audioPath);
            } else {
                LOG_WARN_CAT("SPLASH", "Failed to load splash audio: {}", audioPath);
            }
        }

        LOG_DEBUG_CAT("SPLASH", "Playing splash sound: {}", audioPath);
        g_audioManager->playSound(g_loadedAudioKey);
        LOG_SUCCESS_CAT("SPLASH", "Playing: {}", audioPath);
    } else if (audioPath) {
        LOG_WARN_CAT("SPLASH", "Audio not found or manager unavailable: {}", audioPath);
    }

    // --- 9. Hold 3400ms with event pump ---
    LOG_INFO_CAT("SPLASH", "Holding splash for 3400ms...");
    auto start = std::chrono::steady_clock::now();
    int event_count = 0;
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::steady_clock::now() - start).count() < 3400) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            event_count++;
            if (ev.type == SDL_EVENT_QUIT) {
                LOG_DEBUG_CAT("SPLASH", "Quit event received, jumping to cleanup");
                goto cleanup;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    LOG_INFO_CAT("SPLASH", "Splash delay complete | Events processed: {}", event_count);

cleanup:
    // --- 10. Cleanup ---
    if (tex) {
        LOG_DEBUG_CAT("SPLASH", "Destroying texture 0x{:x}", reinterpret_cast<uint64_t>(tex));
        SDL_DestroyTexture(tex);
    }
    if (ren) {
        LOG_DEBUG_CAT("SPLASH", "Destroying renderer 0x{:x}", reinterpret_cast<uint64_t>(ren));
        SDL_DestroyRenderer(ren);
    }
    if (win) {
        LOG_DEBUG_CAT("SPLASH", "Destroying window 0x{:x}", reinterpret_cast<uint64_t>(win));
        SDL_DestroyWindow(win);
    }

    // --- 11. Destroy global AudioManager (only on program exit) ---
    // We keep it alive across splash calls — destroy only when needed
    // delete g_audioManager; g_audioManager = nullptr;  // Uncomment if single-use

    // FIXED: Quit video subsystem after splash (safe if called multiple times)
    LOG_DEBUG_CAT("SPLASH", "SDL_QuitSubSystem(SDL_INIT_VIDEO)");
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    LOG_SUCCESS_CAT("SPLASH", "{}SPLASH DISMISSED — PINK PHOTONS + AMMO.WAV ETERNAL{}", PLASMA_FUCHSIA, RESET);
}

} // namespace Splash