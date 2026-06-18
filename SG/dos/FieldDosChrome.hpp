#pragma once

// RTX DOS panel chrome — double-click zoom, drag from anywhere on panel.

#include "FieldDosDisplay.hpp"
#include "FieldDosViewport.hpp"
#include "OptionsMenu.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>

namespace FieldDosChrome {

constexpr float HUD_H   = FieldDosViewport::DOS_HUD_H;
constexpr float BORDER  = 4.f;

enum class Hit : std::uint8_t { None = 0, Border = 1, Hud = 2, Content = 3 };

inline Hit hover = Hit::None;
inline bool dragging = false;
inline float dragStartMx = 0.f;
inline float dragStartMy = 0.f;
inline float dragStartOx = 0.f;
inline float dragStartOy = 0.f;

inline void pointerPixels(SDL_Window* window, float lx, float ly, float& px, float& py) noexcept {
    int pixW = 0, pixH = 0, logW = 0, logH = 0;
    if (window) {
        SDL_GetWindowSizeInPixels(window, &pixW, &pixH);
        SDL_GetWindowSize(window, &logW, &logH);
    } else {
        pixW = FieldDosDisplay::pixelW;
        pixH = FieldDosDisplay::pixelH;
        logW = FieldDosDisplay::logicalW > 0 ? FieldDosDisplay::logicalW : pixW;
        logH = FieldDosDisplay::logicalH > 0 ? FieldDosDisplay::logicalH : pixH;
    }
    const float sx = logW > 0 ? static_cast<float>(pixW) / static_cast<float>(logW) : 1.f;
    const float sy = logH > 0 ? static_cast<float>(pixH) / static_cast<float>(logH) : 1.f;
    px = lx * sx;
    py = ly * sy;
}

inline FieldDosViewport::Rect windowRect() noexcept {
    return FieldDosViewport::panelRect();
}

inline Hit hitTest(float mx, float my) noexcept {
    const FieldDosViewport::Rect win = windowRect();
    if (!win.contains(mx, my)) return Hit::None;
    if (FieldDosViewport::contentRect().contains(mx, my)) return Hit::Content;
    if (my >= win.y1 - HUD_H) return Hit::Hud;
    if (mx - win.x0 < BORDER || win.x1 - mx < BORDER
        || my - win.y0 < BORDER || win.y1 - my < BORDER)
        return Hit::Border;
    return Hit::Hud;
}

inline void updateHover(float mx, float my) noexcept {
    hover = hitTest(mx, my);
}

inline void updateHoverFromLogical(SDL_Window* window, float lx, float ly) noexcept {
    float mx = 0.f, my = 0.f;
    pointerPixels(window, lx, ly, mx, my);
    updateHover(mx, my);
}

inline void applyControlFlags() noexcept {
    Options::Canvas::ControlFlags &= ~Options::Canvas::ControlDosPanelFs;
    if (Options::Canvas::DosPanelStretch)
        Options::Canvas::ControlFlags |= Options::Canvas::ControlDosPanelStretch;
    else
        Options::Canvas::ControlFlags &= ~Options::Canvas::ControlDosPanelStretch;
}

inline void setStamp() noexcept {
    Options::Canvas::DosPanelStretch = false;
    FieldDosViewport::panelStretch = false;
    applyControlFlags();
}

inline void setZoom() noexcept {
    Options::Canvas::DosPanelStretch = true;
    FieldDosViewport::panelStretch = true;
    applyControlFlags();
}

inline void toggleZoom() noexcept {
    if (Options::Canvas::DosPanelStretch)
        setStamp();
    else
        setZoom();
}

inline bool onMouseDown(SDL_Window* window, float lx, float ly, Uint8 clicks) noexcept {
    float mx = 0.f, my = 0.f;
    pointerPixels(window, lx, ly, mx, my);
    const Hit h = hitTest(mx, my);
    hover = h;

    if (h == Hit::None) return false;

    if (clicks >= 2) {
        toggleZoom();
        std::fprintf(stderr, "[WINDOW] DOS panel — double-click %s\n",
            Options::Canvas::DosPanelStretch ? "zoom" : "stamp");
        return true;
    }

    if (!Options::Canvas::DosPanelStretch) {
        dragging = true;
        dragStartMx = mx;
        dragStartMy = my;
        dragStartOx = FieldDosViewport::panelOx;
        dragStartOy = FieldDosViewport::panelOy;
        return true;
    }

    return true;
}

inline void onMouseMotion(SDL_Window* window, float lx, float ly) noexcept {
    float mx = 0.f, my = 0.f;
    pointerPixels(window, lx, ly, mx, my);
    updateHover(mx, my);
    if (!dragging || Options::Canvas::DosPanelStretch) return;
    FieldDosViewport::panelOx = dragStartOx + (mx - dragStartMx);
    FieldDosViewport::panelOy = dragStartOy + (my - dragStartMy);
    FieldDosViewport::panelPositioned = true;
    FieldDosViewport::clampPanelPosition();
}

inline void onMouseUp() noexcept {
    dragging = false;
}

inline std::uint32_t packHover() noexcept {
    return static_cast<std::uint32_t>(hover) & 0xFu;
}

} // namespace FieldDosChrome