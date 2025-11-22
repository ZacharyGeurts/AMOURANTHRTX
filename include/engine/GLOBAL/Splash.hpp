// include/engine/GLOBAL/Splash.hpp
// FINAL — COMPILES CLEAN — WORKS — NOVEMBER 21, 2025 — FIRST LIGHT ACHIEVED

#pragma once

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/SDL3.hpp"

#include <SDL3/SDL.h>
#include <filesystem>
#include <thread>
#include <chrono>
#include <string>

namespace Splash {

namespace detail {
    [[nodiscard]] inline bool assetExists(const char* path) noexcept {
        return path && std::filesystem::exists(path);
    }
}

inline void show(const char* title, int w, int h, const char* imagePath, const char* audioPath = nullptr)
{
    LOG_INFO_CAT("SPLASH", "{}SPLASH SEQUENCE INITIATED — {}×{} — PHOTONS AWAKEN{}", VALHALLA_GOLD, w, h, RESET);

    // Video subsystem
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
        LOG_ERROR_CAT("SPLASH", "{}SDL_InitSubSystem(VIDEO) failed: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        return;
    }

    // Center window
    SDL_Rect bounds{};
    SDL_GetDisplayBounds(0, &bounds);
    int cx = bounds.x + (bounds.w - w) / 2;
    int cy = bounds.y + (bounds.h - h) / 2;

    SDL_Window* win = SDL_CreateWindow(title, w, h, SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN);
    if (!win) {
        LOG_ERROR_CAT("SPLASH", "{}Failed to create splash window: {}{}", BLOOD_RED, SDL_GetError(), RESET);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }
    SDL_SetWindowPosition(win, cx, cy);

    // Icon setup
    SDL_Surface* base = IMG_Load("assets/textures/ammo32.ico");
    SDL_Surface* hdpi = IMG_Load("assets/textures/ammo.ico");
    if (base && hdpi) {
        SDL_AddSurfaceAlternateImage(base, hdpi);
        SDL_SetWindowIcon(win, base);
        SDL_DestroySurface(base);
        SDL_DestroySurface(hdpi);
    } else if (base) {
        SDL_SetWindowIcon(win, base);
        SDL_DestroySurface(base);
    }

    SDL_Renderer* ren = SDL_CreateRenderer(win, nullptr);
    if (!ren) {
        LOG_ERROR_CAT("SPLASH", "{}Failed to create renderer: {}{}", CRIMSON_MAGENTA, SDL_GetError(), RESET);
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    SDL_ShowWindow(win);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);

    // Load and center splash image
    SDL_Texture* tex = nullptr;
    if (detail::assetExists(imagePath)) {
        tex = IMG_LoadTexture(ren, imagePath);
        if (tex) {
            float tw = 0, th = 0;
            SDL_GetTextureSize(tex, &tw, &th);
            SDL_FRect dst = { (w - tw) * 0.5f, (h - th) * 0.5f, tw, th };
            SDL_RenderTexture(ren, tex, nullptr, &dst);
            LOG_SUCCESS_CAT("SPLASH", "{}Splash image loaded and centered: {}{}", RASPBERRY_PINK, imagePath, RESET);
        } else {
            LOG_WARN_CAT("SPLASH", "{}Failed to load splash texture: {}{}", AMBER_YELLOW, imagePath, RESET);
        }
    } else {
        LOG_WARN_CAT("SPLASH", "{}Splash image not found: {}{}", AMBER_YELLOW, imagePath, RESET);
    }

    SDL_RenderPresent(ren);

    // === AUDIO SYSTEM — PINK PHOTONS SING ===
    static bool audioInit = []() -> bool {
        return SDL_InitSubSystem(SDL_INIT_AUDIO) == 0;
    }();

    static SDL3Audio::AudioManager* audio = nullptr;

    // Initialize audio manager once
    if ((audioInit && audio) == 0) {
        audio = new SDL3Audio::AudioManager();
        if (!audio->initMixer()) {
            delete audio;
            audio = nullptr;
            LOG_WARN_CAT("SPLASH", "{}AudioManager init failed — proceeding in silence{}", AMBER_YELLOW, RESET);
        } else {
            LOG_SUCCESS_CAT("SPLASH", "{}AudioManager initialized — PINK PHOTONS HAVE VOICE{}", PARTY_PINK, RESET);
        }
    }

    // Play startup sound
    if (audioPath && detail::assetExists(audioPath) && audio) {
        static std::string lastPlayed;
        if (lastPlayed != audioPath) {
            if (audio->loadSound(audioPath, "splash")) {
                lastPlayed = audioPath;
                LOG_SUCCESS_CAT("SPLASH", "{}Startup sound loaded: {}{}", AURORA_PINK, audioPath, RESET);
            }
        }
        audio->playSound("splash");
        LOG_INFO_CAT("SPLASH", "{}AMOURANTH HAS SPOKEN — PHOTONS RESONATE{}", PURE_ENERGY, RESET);
    }

    // Hold splash for 3400ms
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::steady_clock::now() - start).count() < 3400) 
    {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) {
                goto cleanup;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

cleanup:
    if (tex) SDL_DestroyTexture(tex);
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    LOG_SUCCESS_CAT("SPLASH", "{}SPLASH SEQUENCE COMPLETE — FIRST LIGHT ACHIEVED — THE EMPIRE AWAKENS{}", DIAMOND_SPARKLE, RESET);
}

} // namespace Splash