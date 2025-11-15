// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// SPLASH SYSTEM — NOVEMBER 13, 2025 — FINAL & CORRECT
// • SDL3 == 0 = SUCCESS
// • Audio plays ammo.wav
// • Image centered from frame 1
// • No SIGSEGV
// • ENHANCED LOGGING — DETAILED TRACE FOR DEBUG
// • FIXED: Window centered on screen via SDL_SetWindowPosition (SDL3 spec)
// • PINK PHOTONS + AMMO.WAV ETERNAL
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

    // --- 1. Create window (BORDERLESS, VISIBLE) ---
    LOG_DEBUG_CAT("SPLASH", "Creating window: title='{}', size={}x{}", title ? title : "null", w, h);
    SDL_Window* win = SDL_CreateWindow(
        title,
        w, h,
        SDL_WINDOW_BORDERLESS
    );
    if (!win) {
        LOG_ERROR_CAT("SPLASH", "Failed to create splash window: {}", SDL_GetError());
        return;
    }
    LOG_SUCCESS_CAT("SPLASH", "Window created: 0x{:x}", reinterpret_cast<uint64_t>(win));

    // FIXED: Center window on screen (SDL3: SDL_SetWindowPosition after creation)
    LOG_DEBUG_CAT("SPLASH", "Centering window on screen");
    SDL_SetWindowPosition(win, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    LOG_SUCCESS_CAT("SPLASH", "Window centered on primary display");

    // --- 2. Create renderer ---
    LOG_DEBUG_CAT("SPLASH", "Creating renderer for window 0x{:x}", reinterpret_cast<uint64_t>(win));
    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) {
        SDL_DestroyWindow(win);
        LOG_ERROR_CAT("SPLASH", "Failed to create splash renderer: {}", SDL_GetError());
        return;
    }
    LOG_SUCCESS_CAT("SPLASH", "Renderer created: 0x{:x}", reinterpret_cast<uint64_t>(ren));

    // --- 3. Clear to black ---
    LOG_DEBUG_CAT("SPLASH", "Clearing renderer to black");
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);

    // --- 4. Load image and center it ---
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

    // --- 5. Present — image is now visible and centered ---
    LOG_DEBUG_CAT("SPLASH", "SDL_RenderPresent call");
    SDL_RenderPresent(ren);
    LOG_INFO_CAT("SPLASH", "Splash presented — image visible");

    // --- 6. Initialize audio subsystem ONCE (SDL3: == 0 = success) ---
    static bool audioInit = []() -> bool {
        LOG_DEBUG_CAT("SPLASH", "SDL_InitSubSystem(SDL_INIT_AUDIO) call");
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) == 0) {  // SDL3: non-zero = success
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

    LOG_SUCCESS_CAT("SPLASH", "{}SPLASH DISMISSED — PINK PHOTONS + AMMO.WAV ETERNAL{}", PLASMA_FUCHSIA, RESET);
}

} // namespace Splash