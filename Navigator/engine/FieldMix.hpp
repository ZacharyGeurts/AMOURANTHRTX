#pragma once

// SDL3_mixer play helpers — property-bag API (not SDL2 loop counts).

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

namespace FieldMix {

inline bool playTrack(MIX_Track* track, int loops = 0) noexcept {
    if (!track) return false;
    SDL_PropertiesID opts = SDL_CreateProperties();
    if (!opts) return false;
    SDL_SetNumberProperty(opts, MIX_PROP_PLAY_LOOPS_NUMBER, loops);
    const bool ok = MIX_PlayTrack(track, opts);
    SDL_DestroyProperties(opts);
    return ok;
}

} // namespace FieldMix