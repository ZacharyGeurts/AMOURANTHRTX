// include/engine/GLOBAL/Splash.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// FINAL AMOURANTH BANNER + AUDIO — 2025 — NO WARNINGS — NO BULLSHIT
// Uses your perfect SDL3Audio::AudioManager. Just works.
// =============================================================================

// include/engine/GLOBAL/Splash.hpp
// =============================================================================
// AMOURANTH RTX — FINAL SPLASH 2025 — PERFECTLY CENTERED — NO ERRORS — SDL3
// =============================================================================

// include/engine/GLOBAL/Splash.hpp
// =============================================================================
// AMOURANTH RTX — FINAL SPLASH 2025 — PERFECTLY CENTERED — NO ERRORS — SDL3
// =============================================================================

// include/engine/GLOBAL/Splash.hpp
// =============================================================================
// AMOURANTH RTX — FINAL SPLASH 2025 — PERFECTLY CENTERED — NO ERRORS — SDL3
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <filesystem>
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/SDL3.hpp"

using namespace Logging::Color;

namespace Splash {

inline void show(
    const char* imagePath = "assets/textures/ammo.png",
    const char* audioPath = "assets/audio/startup.wav")
{
    if (!g_sdl_window || !SDL_GetRenderer(g_sdl_window.get())) {
        LOG_ERROR_CAT("SPLASH", "{}No window/renderer — splash skipped{}", CRIMSON_MAGENTA, RESET);
        return;
    }

    SDL_Renderer* ren = SDL_GetRenderer(g_sdl_window.get());

    LOG_SUCCESS_CAT("SPLASH", "{}SPLASH RITUAL — MANIFESTING HER{}", DIAMOND_SPARKLE, RESET);

    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);

    if (imagePath && std::filesystem::exists(imagePath)) {
        if (SDL_Texture* tex = IMG_LoadTexture(ren, imagePath)) {
            float tex_w = 0.0f, tex_h = 0.0f;
            SDL_GetTextureSize(tex, &tex_w, &tex_h);

            int win_w = 0, win_h = 0;
            SDL_GetCurrentRenderOutputSize(ren, &win_w, &win_h);

            SDL_FRect dst{
                (win_w - tex_w) * 0.5f,
                (win_h - tex_h) * 0.5f,
                tex_w,
                tex_h
            };

            SDL_RenderTexture(ren, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);

            LOG_SUCCESS_CAT("SPLASH", "{}AMOURANTH MANIFESTED — PERFECTLY CENTERED{}", AURORA_PINK, RESET);
        } else {
            LOG_ERROR_CAT("SPLASH", "{}Failed to load image: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        }
    }

    SDL_RenderPresent(ren);

    static SDL3Audio::AudioManager s_audio;
    static bool s_ready = false;

    if (!s_ready) {
        if (s_audio.initMixer()) {
            if (audioPath && std::filesystem::exists(audioPath)) {
                [[maybe_unused]] bool ok = s_audio.loadSound(audioPath, "splash");
            }
            s_ready = true;
            LOG_SUCCESS_CAT("SPLASH", "{}AudioManager ready{}", PARTY_PINK, RESET);
        }
    }
    if (s_ready && audioPath && std::filesystem::exists(audioPath)) {
        s_audio.playSound("splash");
        LOG_SUCCESS_CAT("SPLASH", "{}PHOTONS SING — STARTUP SOUND{}", PURE_ENERGY, RESET);
    }

    LOG_SUCCESS_CAT("SPLASH", "{}SPLASH COMPLETE — EMPIRE RISES{}", VALHALLA_GOLD, RESET);
}

} // namespace Splash