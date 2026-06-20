#pragma once

// Linux / desktop WM identity — consistent ammo icon + app ID on the host taskbar.

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

namespace FieldPlatformChrome {

inline void initAppIdentity() noexcept {
    SDL_SetAppMetadata("AMOURANTHRTX", "7.0", "com.amouranth.rtx");
    SDL_SetHint(SDL_HINT_APP_ID, "com.amouranth.rtx");
    SDL_SetHint(SDL_HINT_APP_NAME, "AMOURANTHRTX");
}

inline bool applyWindowIcon(SDL_Window* window) noexcept {
    if (!window) return false;
    const char* paths[] = {
        "assets/textures/ammo.ico",
        "assets/textures/ammo.png",
        "assets/data/icons/amouranthos.png",
        nullptr
    };
    for (int i = 0; paths[i]; ++i) {
        if (SDL_Surface* surf = IMG_Load(paths[i])) {
            const bool ok = SDL_SetWindowIcon(window, surf);
            SDL_DestroySurface(surf);
            if (ok) return true;
        }
    }
    return false;
}

} // namespace FieldPlatformChrome