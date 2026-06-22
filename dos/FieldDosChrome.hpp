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
inline float lastPixelX = 0.f;
inline float lastPixelY = 0.f;
inline std::uint8_t lastButtons = 0u;
inline std::uint8_t prevButtons = 0u;
inline bool dragging = false;
inline float dragStartMx = 0.f;
inline float dragStartMy = 0.f;
inline float dragStartOx = 0.f;
inline float dragStartOy = 0.f;

inline void refreshWindowMetrics(SDL_Window* window) noexcept {
    if (!window) return;
    int pixW = 0, pixH = 0, logW = 0, logH = 0;
    SDL_GetWindowSizeInPixels(window, &pixW, &pixH);
    SDL_GetWindowSize(window, &logW, &logH);
    if (pixW <= 0 || pixH <= 0) return;
    int dispW = FieldDosDisplay::displayPixelW;
    int dispH = FieldDosDisplay::displayPixelH;
    const SDL_DisplayID did = SDL_GetDisplayForWindow(window);
    SDL_Rect bounds{};
    if (did && SDL_GetDisplayBounds(did, &bounds) == 0) {
        dispW = bounds.w;
        dispH = bounds.h;
    }
    FieldDosDisplay::setWindowMetrics(pixW, pixH, SDL_GetWindowDisplayScale(window),
        dispW, dispH, logW > 0 ? logW : pixW, logH > 0 ? logH : pixH);
}

inline void pointerPixels(SDL_Window* window, float lx, float ly, float& px, float& py) noexcept {
    refreshWindowMetrics(window);
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
    const float flogW = logW > 0 ? static_cast<float>(logW) : static_cast<float>(pixW);
    const float flogH = logH > 0 ? static_cast<float>(logH) : static_cast<float>(pixH);
    const float fpixW = pixW > 0 ? static_cast<float>(pixW) : flogW;
    const float fpixH = pixH > 0 ? static_cast<float>(pixH) : flogH;
    // SDL3 mouse events are window-relative (SDL_GetWindowSize space). Map logical → pixels
    // via window size ratio only — SDL_GetWindowPixelDensity double-scales on Linux fullscreen.
    const float scaleX = flogW > 1.f ? (fpixW / flogW) : 1.f;
    const float scaleY = flogH > 1.f ? (fpixH / flogH) : 1.f;
    const bool coordsLookLikePixels = (lx > flogW + 2.f || ly > flogH + 2.f);
    if (coordsLookLikePixels) {
        px = lx;
        py = ly;
    } else {
        px = lx * scaleX;
        py = ly * scaleY;
    }
    if (fpixW > 1.f) px = std::clamp(px, 0.f, fpixW - 1.f);
    if (fpixH > 1.f) py = std::clamp(py, 0.f, fpixH - 1.f);
}

inline bool chromeUsesRenderSpace() noexcept {
    return FieldDosViewport::renderW > 1.f && FieldDosViewport::renderH > 1.f;
}

// Map SDL window event coords → render framebuffer space (matches shader viewport_w/h).
inline void chromePointerPixels(SDL_Window* window, float lx, float ly, float& px, float& py) noexcept {
    refreshWindowMetrics(window);
    const float renderW = std::max(FieldDosViewport::renderW, 1.f);
    const float renderH = std::max(FieldDosViewport::renderH, 1.f);

    if (!window) {
        px = lx;
        py = ly;
        if (chromeUsesRenderSpace()) {
            px = std::clamp(px, 0.f, renderW - 1.f);
            py = std::clamp(py, 0.f, renderH - 1.f);
        }
        return;
    }

    // SDL3 reports logical coords on macOS and pixel-native coords on X11/Windows — reuse
    // pointerPixels so hit tests agree with DOS panel chrome and the shader viewport.
    float winPx = lx, winPy = ly;
    pointerPixels(window, lx, ly, winPx, winPy);

    int pixW = 0, pixH = 0;
    SDL_GetWindowSizeInPixels(window, &pixW, &pixH);
    const float fpixW = pixW > 0 ? static_cast<float>(pixW)
        : (FieldDosDisplay::pixelW > 0 ? static_cast<float>(FieldDosDisplay::pixelW) : renderW);
    const float fpixH = pixH > 0 ? static_cast<float>(pixH)
        : (FieldDosDisplay::pixelH > 0 ? static_cast<float>(FieldDosDisplay::pixelH) : renderH);
    px = (winPx / fpixW) * renderW;
    py = (winPy / fpixH) * renderH;
    px = std::clamp(px, 0.f, renderW - 1.f);
    py = std::clamp(py, 0.f, renderH - 1.f);
}

inline FieldDosViewport::Rect windowRect() noexcept {
    if (chromeUsesRenderSpace())
        return FieldDosViewport::panelRectRender();
    return FieldDosViewport::panelRect();
}

inline Hit hitTest(float mx, float my) noexcept {
    const FieldDosViewport::Rect win = windowRect();
    if (!win.contains(mx, my)) return Hit::None;
    const FieldDosViewport::Rect content = chromeUsesRenderSpace()
        ? FieldDosViewport::contentRectRender() : FieldDosViewport::contentRect();
    if (content.contains(mx, my)) return Hit::Content;
    if (my >= win.y1 - HUD_H) return Hit::Hud;
    if (mx - win.x0 < BORDER || win.x1 - mx < BORDER
        || my - win.y0 < BORDER || win.y1 - my < BORDER)
        return Hit::Border;
    return Hit::Hud;
}

inline void updateHover(float mx, float my) noexcept {
    hover = hitTest(mx, my);
}

inline void storePointer(float px, float py, std::uint32_t buttons) noexcept {
    lastPixelX = px;
    lastPixelY = py;
    lastButtons = static_cast<std::uint8_t>(buttons & 0xFFu);
}

inline void updateHoverFromLogical(SDL_Window* window, float lx, float ly) noexcept {
    float mx = 0.f, my = 0.f;
    chromePointerPixels(window, lx, ly, mx, my);
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
    chromePointerPixels(window, lx, ly, mx, my);
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
    chromePointerPixels(window, lx, ly, mx, my);
    storePointer(mx, my, SDL_GetMouseState(nullptr, nullptr));
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