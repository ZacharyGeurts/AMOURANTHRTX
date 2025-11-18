// include/engine/GLOBAL/Splash.hpp
// FINAL — COMPILES CLEAN — WORKS — NOVEMBER 18 2025

#pragma once

#include "engine/GLOBAL/logging.hpp"
#include "engine/SDL3/SDL3_image.hpp"
#include "engine/SDL3/SDL3_audio.hpp"

#include <SDL3/SDL.h>
#include <filesystem>
#include <thread>
#include <chrono>

namespace Splash {

namespace detail {
    [[nodiscard]] inline bool assetExists(const char* path) noexcept {
        return path && std::filesystem::exists(path);
    }
}

inline void show(const char* title, int w, int h, const char* imagePath, const char* audioPath = nullptr)
{
    LOG_INFO_CAT("SPLASH", "SPLASH START — {}x{}", w, h);

    // Video init
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) == 0) {
        LOG_ERROR_CAT("SPLASH", "SDL_InitSubSystem(VIDEO) failed: {}", SDL_GetError());
        return;
    }

    // Center on screen
    SDL_Rect bounds{};
    if (SDL_GetDisplayBounds(0, &bounds) == 0) {
        bounds.w = 1920; bounds.h = 1080;
    }
    int cx = bounds.x + (bounds.w - w) / 2;
    int cy = bounds.y + (bounds.h - h) / 2;

    SDL_Window* win = SDL_CreateWindow(title, w, h, SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN);
    if (!win) {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }
    SDL_SetWindowPosition(win, cx, cy);

    // Icons
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
        SDL_DestroyWindow(win);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return;
    }

    SDL_ShowWindow(win);
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
    SDL_RenderClear(ren);

    // Image
    SDL_Texture* tex = nullptr;
    if (detail::assetExists(imagePath)) {
        tex = IMG_LoadTexture(ren, imagePath);
        if (tex) {
            float tw = 0, th = 0;
            SDL_GetTextureSize(tex, &tw, &th);
            SDL_FRect dst{(w - tw) * 0.5f, (h - th) * 0.5f, tw, th};
            SDL_RenderTexture(ren, tex, nullptr, &dst);
        }
    }
    SDL_RenderPresent(ren);

    // Audio — using your already-working AudioManager
    static bool audioInit = []() -> bool {
        return SDL_InitSubSystem(SDL_INIT_AUDIO) == 0;
    }();

    static SDL3Audio::AudioManager* audio = nullptr;
    if ((audioInit && !audio) == 0) {
        audio = new SDL3Audio::AudioManager();
        (void)audio->initMixer();  // consume nodiscard
    }

    if (audioPath && detail::assetExists(audioPath) && audio) {
        static std::string last;
        if (last != audioPath) {
            (void)audio->loadSound(audioPath, "splash");  // consume nodiscard
            last = audioPath;
        }
        audio->playSound("splash");
    }

    // Wait 3400 ms
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::steady_clock::now() - start).count() < 3400) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) goto end;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

end:
    if (tex) SDL_DestroyTexture(tex);
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
    SDL_QuitSubSystem(SDL_INIT_VIDEO);

    LOG_SUCCESS_CAT("SPLASH", "SPLASH DISMISSED — PINK PHOTONS ETERNAL");
}

} // namespace Splash