// include/engine/KeyBindings.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — KEYBINDINGS — FINAL PRODUCTION (November 17, 2025)
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <array>

namespace KeyBind {

inline constexpr SDL_Scancode FULLSCREEN      = SDL_SCANCODE_F;      // F  → fullscreen
inline constexpr SDL_Scancode OVERLAY         = SDL_SCANCODE_O;
inline constexpr SDL_Scancode TONEMAP         = SDL_SCANCODE_T;
inline constexpr SDL_Scancode HYPERTRACE      = SDL_SCANCODE_H;
inline constexpr SDL_Scancode MAXIMIZE_MUTE   = SDL_SCANCODE_M;      // M = maximize + mute
inline constexpr SDL_Scancode HDR_TOGGLE      = SDL_SCANCODE_F12;    // F12 → HDR toggle (on/off)
inline constexpr SDL_Scancode QUIT            = SDL_SCANCODE_ESCAPE;

// =============================================================================
// IMGUI DEBUG CONSOLE — PRESS ` OR ~ TO TOGGLE FULL DEBUG OVERLAY
// =============================================================================
inline constexpr SDL_Scancode IMGUI_CONSOLE    = SDL_SCANCODE_GRAVE;  // `~ key — opens/closes ImGui console

inline constexpr std::array<SDL_Scancode, 9> RENDER_MODE{{
    SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3,
    SDL_SCANCODE_4, SDL_SCANCODE_5, SDL_SCANCODE_6,
    SDL_SCANCODE_7, SDL_SCANCODE_8, SDL_SCANCODE_9
}};

} // namespace KeyBind