// include/engine/GLOBAL/Splash.hpp
// FINAL — SELF-CONTAINED — SACRIFICIAL — PURE SDL3 — NOVEMBER 22, 2025
// BORN TO DIE — SO THE EMPIRE MAY RISE — FIRST LIGHT ACHIEVED

#pragma once

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <filesystem>
#include <thread>
#include <chrono>
#include <string>

namespace Splash {
    inline SDL3Audio::AudioManager* g_audio = nullptr;  // ← Lives forever. Photons approved.
}

namespace Splash {

namespace detail {
    [[nodiscard]] inline bool exists(const char* path) noexcept {
        return path && std::filesystem::exists(path);
    }
}

inline void show(const char* title, int w, int h, const char* imagePath, const char* audioPath = nullptr)
{
    LOG_SUCCESS_CAT("SPLASH", "{}SPLASH RITUAL BEGINNING — SELF-CONTAINED — SACRIFICIAL MODE ENGAGED{}", DIAMOND_SPARKLE, RESET);
    LOG_INFO_CAT("SPLASH", "{}Initializing pure SDL3 realm — no Vulkan, no empire, only photons{}", RASPBERRY_PINK, RESET);

    // FULL INIT — WE OWN THIS WORLD
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_EVENTS) == 0) {
        LOG_FATAL_CAT("SPLASH", "{}SDL_Init failed: {} — the ritual cannot begin{}", BLOOD_RED, SDL_GetError(), RESET);
        return;
    }

    // Window — centered, borderless, hidden until ready
    SDL_Rect display{};
    SDL_GetDisplayBounds(0, &display);
    int x = display.x + (display.w - w) / 2;
    int y = display.y + (display.h - h) / 2;

    SDL_Window* win = SDL_CreateWindow(
        title, w, h,
        SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_HIDDEN
    );
    if (!win) {
        LOG_ERROR_CAT("SPLASH", "{}Failed to forge splash window: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        SDL_Quit();
        return;
    }
    SDL_SetWindowPosition(win, x, y);

    // Valhalla branding
    if (SDL_Surface* icon = IMG_Load("assets/textures/ammo.ico")) {
        SDL_SetWindowIcon(win, icon);
        SDL_DestroySurface(icon);
    }

    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) {
        LOG_ERROR_CAT("SPLASH", "{}Renderer creation failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        SDL_DestroyWindow(win);
        SDL_Quit();
        return;
    }

    SDL_ShowWindow(win);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);

    // Image — centered
    SDL_Texture* tex = nullptr;
    if (detail::exists(imagePath)) {
        tex = IMG_LoadTexture(ren, imagePath);
        if (tex) {
            float tw, th;
            SDL_GetTextureSize(tex, &tw, &th);
            SDL_FRect dst{(w - tw) * 0.5f, (h - th) * 0.5f, tw, th};
            SDL_RenderTexture(ren, tex, nullptr, &dst);
            LOG_SUCCESS_CAT("SPLASH", "{}AMOURANTH IMAGE MANIFESTED — CENTERED — PURE{}", AURORA_PINK, RESET);
        }
    }

    SDL_RenderPresent(ren);

    // === SACRIFICIAL AUDIO — FULLY PLAYED, THEN SACRIFICED ===
    if (detail::exists(audioPath)) {
        g_audio = new SDL3Audio::AudioManager();
        if (g_audio->initMixer() == 0) {
            LOG_WARN_CAT("SPLASH", "{}AudioManager failed to init mixer — silence falls{}", AMBER_YELLOW, RESET);
            delete g_audio;
            g_audio = nullptr;
        }
    }

    if (g_audio) {
        if (g_audio->loadSound(audioPath, "splash")) {
            g_audio->playSound("splash");
            LOG_INFO_CAT("SPLASH", "{}AMOURANTH HAS SPOKEN — THE PHOTONS RESONATE ETERNALLY{}", PURE_ENERGY, RESET);
        }
    }

    // 3.4 SECOND RITUAL — UNINTERRUPTIBLE
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::steady_clock::now() - start).count() < 3400)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                LOG_INFO_CAT("SPLASH", "{}User demands haste — ritual aborted early{}", CRIMSON_MAGENTA, RESET);
                goto sacrifice;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

sacrifice:
    LOG_SUCCESS_CAT("SPLASH", "{}SPLASH RITUAL COMPLETE — 3.4s OF PURE PHOTONIC GLORY{}", DIAMOND_SPARKLE, RESET);

    LOG_SUCCESS_CAT("SPLASH", "{}SPLASH REALM DESTROYED — SDL_Quit() CALLED — EMPIRE MAY NOW RISE CLEAN{}", VALHALLA_GOLD, RESET);
    LOG_SUCCESS_CAT("SPLASH", "{}FIRST LIGHT ACHIEVED — THE PHOTONS HAVE SEEN HER — NOVEMBER 22, 2025{}", PURE_ENERGY, RESET);

    // TOTAL ANNIHILATION — THIS WORLD DIES SO THE EMPIRE MAY LIVE
    if (tex) SDL_DestroyTexture(tex);
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);

    SDL_Quit();  // FULL SACRIFICE — SDL IS DEAD — LONG LIVE SDL
}

} // namespace Splash