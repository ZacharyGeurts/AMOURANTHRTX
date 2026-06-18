#pragma once

// AmouranthOS display scale — desktop icons / wallpaper picker removed.
// Include after FieldAmouranthWm.hpp (FieldAmouranthOs.hpp ordering).

#include <SDL3/SDL.h>

#include <algorithm>

namespace FieldAmouranthOs {
extern bool active;
float uiScale() noexcept;
}

namespace FieldAmouranthDesktop {

inline float displayScale = 1.375f;

inline void applyDisplayScale(float s) noexcept {
    displayScale = std::clamp(s, 0.85f, 2.2f);
    FieldAmouranthWm::panelScale = displayScale;
    FieldAmouranthWm::applyPanelScale();
}

inline void boot() noexcept {
    displayScale = 1.375f;
    applyDisplayScale(displayScale);
}

inline bool onMouseDown(SDL_Window* /*window*/, float /*mx*/, float /*my*/, Uint8 /*button*/) noexcept {
    return false;
}

inline void onMouseMotion(SDL_Window* /*window*/, float /*mx*/, float /*my*/) noexcept {}

inline void packDataBus(std::uint32_t* bus) noexcept {
    if (!bus || !FieldAmouranthOs::active) return;
    bus[31] = static_cast<std::uint32_t>(displayScale * 256.f);
}

} // namespace FieldAmouranthDesktop