#pragma once

// AmouranthOS desktop — scale + left-rail icon hit-test (opens folder thumbnail view).

#include "FieldAmouranthFolderView.hpp"
#include "FieldAmouranthWm.hpp"

#include <SDL3/SDL.h>

#include <algorithm>

namespace FieldAmouranthOs {
extern bool active;
extern int  winW, winH;
float uiScale() noexcept;
}

namespace FieldAmouranthDesktop {

constexpr std::uint32_t DESKTOP_RAM = 0x000BD580u;
constexpr int           ICON_COUNT  = 9;

inline const std::uint8_t kIconSlots[ICON_COUNT] = {
    4, 5, 6, 14, 8, 11, 15, 16, 12
};

inline const char* kIconPaths[ICON_COUNT] = {
    "C:\\GAMES",
    "C:\\AMMOCODE",
    "C:\\",
    "C:\\GAMES\\DOOM",
    "C:\\QBASIC",
    "C:\\GAMES\\NES",
    "C:\\WALLPAPERS",
    "C:\\",
    "C:\\TOOLS",
};

inline float displayScale = 1.375f;
inline int   iconHover = -1;

inline void applyDisplayScale(float s) noexcept {
    displayScale = std::clamp(s, 0.85f, 2.2f);
    FieldAmouranthWm::panelScale = displayScale;
    FieldAmouranthWm::applyPanelScale();
}

inline void boot() noexcept {
    displayScale = 1.375f;
    iconHover = -1;
    applyDisplayScale(displayScale);
}

inline float deskScale() noexcept {
    return displayScale * FieldAmouranthOs::uiScale();
}

inline int iconAt(float mx, float my) noexcept {
    if (!FieldAmouranthOs::active) return -1;
    const float scale = deskScale();
    const float cellW = 108.f * scale;
    const float cellH = 118.f * scale;
    const float padX = 28.f * scale;
    const float topBar = 28.f * scale;
    const float padY = 36.f * scale;
    for (int i = 0; i < ICON_COUNT; ++i) {
        const float ix = padX;
        const float iy = topBar + padY + static_cast<float>(i) * cellH;
        if (mx >= ix && mx < ix + cellW && my >= iy && my < iy + cellH)
            return i;
    }
    return -1;
}

inline void packDesktopRam(std::uint8_t* ram) noexcept {
    if (!ram) return;
    ram[DESKTOP_RAM] = static_cast<std::uint8_t>(ICON_COUNT);
    ram[DESKTOP_RAM + 1u] = iconHover >= 0
        ? static_cast<std::uint8_t>(iconHover) : 0xFFu;
}

inline bool onMouseDown(SDL_Window* /*window*/, float mx, float my, Uint8 button) noexcept {
    if (button != SDL_BUTTON_LEFT) return false;
    const int idx = iconAt(mx, my);
    if (idx < 0) return false;
    FieldAmouranthFolderView::show(kIconPaths[idx]);
    return true;
}

inline void onMouseMotion(SDL_Window* /*window*/, float mx, float my) noexcept {
    iconHover = iconAt(mx, my);
}

inline void packDataBus(std::uint32_t* bus) noexcept {
    if (!bus || !FieldAmouranthOs::active) return;
    bus[31] = static_cast<std::uint32_t>(displayScale * 256.f);
}

} // namespace FieldAmouranthDesktop
