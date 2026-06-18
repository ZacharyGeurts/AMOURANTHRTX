#pragma once

// WM input — drag, resize, edge snap (Mutter-style), window actions.

#include "FieldWmChrome.hpp"
#include "FieldDosChrome.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace FieldAmouranthOs {
void hideDosPanel() noexcept;
void markFocusedMinimized() noexcept;
void saveFocusedPanelPos() noexcept;
bool shellChromeActive() noexcept;
} // fwd

namespace FieldWmInput {

constexpr float SNAP_MARGIN = 28.f;

inline int  pendingMenuAction = 0;
inline bool closeRequested    = false;
inline bool dragging          = false;
inline bool resizing          = false;
inline FieldWmChrome::ChromeHit resizeEdge = FieldWmChrome::ChromeHit::None;
inline float dragMx0 = 0.f, dragMy0 = 0.f;
inline float dragOx0 = 0.f, dragOy0 = 0.f;
inline float dragW0 = 0.f, dragH0 = 0.f;
inline float dragBaseW0 = 0.f, dragBaseH0 = 0.f;
inline float panelScale = 1.f;

inline void applyPanelScale() noexcept {
    FieldDosViewport::fontScale = std::clamp(1.1f * panelScale, 1.0f, 2.5f);
    FieldDosViewport::sharpen = std::clamp(0.50f + panelScale * 0.08f, 0.45f, 0.75f);
    FieldDosViewport::crispFont = true;
    FieldDosViewport::subpixelFont = false;
    FieldDosViewport::panelGlow = 0.08f;
    FieldWmChrome::syncViewport(panelScale);
}

inline void resetScale() noexcept {
    panelScale = 1.25f;
    applyPanelScale();
}

inline void closeWindow() noexcept {
    FieldWmChrome::openMenu = FieldWmChrome::OpenMenu::None;
    FieldWmChrome::menuItemHover = -1;
    closeRequested = true;
    FieldAmouranthOs::hideDosPanel();
}

inline void maximizeFocusedWindow() noexcept {
    const float sw = FieldDosViewport::winW > 0.f ? FieldDosViewport::winW : 1920.f;
    const float sh = FieldDosViewport::winH > 0.f ? FieldDosViewport::winH : 1080.f;
    const float deskTop = FieldAmouranthOs::desktopTopInset();
    const float taskH = FieldAmouranthOs::scaledTaskbarH();
    const float margin = 10.f * FieldWmChrome::wmUiScale();
    const float targetW = sw - margin * 2.f;
    const float targetH = sh - deskTop - taskH - margin * 2.f;
    const float curW = FieldDosViewport::panelOuterW();
    const float curH = FieldDosViewport::panelOuterH();
    const float baseW = curW / std::max(panelScale, 0.01f);
    const float baseH = curH / std::max(panelScale, 0.01f);
    panelScale = std::clamp(
        std::min(targetW / std::max(baseW, 1.f), targetH / std::max(baseH, 1.f)),
        0.55f, 2.5f);
    applyPanelScale();
    FieldDosViewport::panelOx = margin;
    FieldDosViewport::panelOy = deskTop + margin;
    FieldDosViewport::panelPositioned = true;
    FieldDosViewport::panelStretch = false;
    Options::Canvas::DosPanelStretch = false;
    Options::Canvas::ControlFlags &= ~Options::Canvas::ControlDosPanelStretch;
    FieldDosViewport::clampPanelPosition();
    FieldAmouranthOs::saveFocusedPanelPos();
    FieldWmChrome::openMenu = FieldWmChrome::OpenMenu::None;
    FieldWmChrome::menuItemHover = -1;
}

inline void snapHalfLeft() noexcept {
    const float sw = FieldDosViewport::winW > 0.f ? FieldDosViewport::winW : 1920.f;
    const float deskTop = FieldAmouranthOs::desktopTopInset();
    const float taskH = FieldAmouranthOs::scaledTaskbarH();
    const float margin = 8.f * FieldWmChrome::wmUiScale();
    const float targetW = (sw - margin * 3.f) * 0.5f;
    const float targetH = FieldDosViewport::winH - deskTop - taskH - margin * 2.f;
    const float baseW = FieldDosViewport::panelOuterW() / std::max(panelScale, 0.01f);
    const float baseH = FieldDosViewport::panelOuterH() / std::max(panelScale, 0.01f);
    panelScale = std::clamp(
        std::min(targetW / std::max(baseW, 1.f), targetH / std::max(baseH, 1.f)),
        0.55f, 2.2f);
    applyPanelScale();
    FieldDosViewport::panelOx = margin;
    FieldDosViewport::panelOy = deskTop + margin;
    FieldDosViewport::panelPositioned = true;
    FieldDosViewport::clampPanelPosition();
}

inline void snapHalfRight() noexcept {
    const float sw = FieldDosViewport::winW > 0.f ? FieldDosViewport::winW : 1920.f;
    const float deskTop = FieldAmouranthOs::desktopTopInset();
    const float taskH = FieldAmouranthOs::scaledTaskbarH();
    const float margin = 8.f * FieldWmChrome::wmUiScale();
    const float targetW = (sw - margin * 3.f) * 0.5f;
    const float targetH = FieldDosViewport::winH - deskTop - taskH - margin * 2.f;
    const float baseW = FieldDosViewport::panelOuterW() / std::max(panelScale, 0.01f);
    const float baseH = FieldDosViewport::panelOuterH() / std::max(panelScale, 0.01f);
    panelScale = std::clamp(
        std::min(targetW / std::max(baseW, 1.f), targetH / std::max(baseH, 1.f)),
        0.55f, 2.2f);
    applyPanelScale();
    FieldDosViewport::panelOx = sw - margin - FieldDosViewport::panelOuterW();
    FieldDosViewport::panelOy = deskTop + margin;
    FieldDosViewport::panelPositioned = true;
    FieldDosViewport::clampPanelPosition();
}

inline void applyEdgeSnap() noexcept {
    if (Options::Canvas::DosPanelStretch) return;
    const float sw = FieldDosViewport::winW > 0.f ? FieldDosViewport::winW : 1920.f;
    const float deskTop = FieldAmouranthOs::desktopTopInset();
    const float snap = SNAP_MARGIN * FieldWmChrome::wmUiScale();
    const float ox = FieldDosViewport::panelOx;
    const float oy = FieldDosViewport::panelOy;
    const float pw = FieldDosViewport::panelOuterW();

    if (oy <= deskTop + snap)
        maximizeFocusedWindow();
    else if (ox <= snap)
        snapHalfLeft();
    else if (ox + pw >= sw - snap)
        snapHalfRight();
}

inline bool onMouseDown(SDL_Window* window, float lx, float ly, Uint8 clicks) noexcept {
    if (!FieldWmChrome::wmPanelActive()) return false;
    float mx = 0.f, my = 0.f;
    FieldDosChrome::pointerPixels(window, lx, ly, mx, my);
    FieldWmChrome::hover = FieldWmChrome::hitTest(mx, my);
    if (FieldWmChrome::hover == FieldWmChrome::ChromeHit::None) return false;

    using CH = FieldWmChrome::ChromeHit;
    using OM = FieldWmChrome::OpenMenu;

    if (FieldWmChrome::hover >= CH::FileMenu && FieldWmChrome::hover <= CH::HelpMenu) {
        const auto picked = static_cast<OM>(
            static_cast<int>(OM::File)
            + (static_cast<int>(FieldWmChrome::hover) - static_cast<int>(CH::FileMenu)));
        FieldWmChrome::openMenu = (FieldWmChrome::openMenu == picked) ? OM::None : picked;
        return true;
    }
    if (FieldWmChrome::hover >= CH::MenuItem0 && FieldWmChrome::hover <= CH::MenuItem3
            && FieldWmChrome::openMenu != OM::None) {
        const int item = static_cast<int>(FieldWmChrome::hover)
            - static_cast<int>(CH::MenuItem0);
        const int action = FieldWmChrome::menuItemAction(FieldWmChrome::openMenu, item);
        FieldWmChrome::openMenu = OM::None;
        FieldWmChrome::menuItemHover = -1;
        if (action == 109) {
            closeWindow();
            return true;
        }
        if (action > 0) {
            pendingMenuAction = action;
            return true;
        }
        return true;
    }
    if (FieldWmChrome::hover == CH::Close) {
        closeWindow();
        return true;
    }
    if (FieldWmChrome::hover == CH::Minimize) {
        FieldWmChrome::openMenu = OM::None;
        FieldWmChrome::menuItemHover = -1;
        FieldAmouranthOs::markFocusedMinimized();
        FieldAmouranthOs::hideDosPanel();
        return true;
    }
    if (FieldWmChrome::hover == CH::Maximize) {
        maximizeFocusedWindow();
        return true;
    }

    const FieldDosViewport::Rect win = FieldWmChrome::windowRect();
    if (FieldWmChrome::hover == CH::TitleBar && clicks < 2) {
        FieldWmChrome::openMenu = OM::None;
        FieldWmChrome::menuItemHover = -1;
        FieldWmCore::raiseFocusedProgram();
        dragging = true;
        dragMx0 = mx; dragMy0 = my;
        dragOx0 = FieldDosViewport::panelOx;
        dragOy0 = FieldDosViewport::panelOy;
        return true;
    }
    if (FieldWmChrome::hover >= CH::ResizeN && FieldWmChrome::hover <= CH::ResizeSW) {
        FieldWmCore::raiseFocusedProgram();
        resizing = true;
        resizeEdge = FieldWmChrome::hover;
        dragMx0 = mx; dragMy0 = my;
        dragOx0 = FieldDosViewport::panelOx;
        dragOy0 = FieldDosViewport::panelOy;
        dragW0 = win.w();
        dragH0 = win.h();
        dragBaseW0 = dragW0 / std::max(panelScale, 0.01f);
        dragBaseH0 = dragH0 / std::max(panelScale, 0.01f);
        return true;
    }
    if (FieldWmChrome::hover == CH::Content) {
        FieldWmChrome::openMenu = OM::None;
        FieldWmChrome::menuItemHover = -1;
        return false;
    }
    return true;
}

inline void onMouseMotion(SDL_Window* window, float lx, float ly) noexcept {
    if (!FieldAmouranthOs::shellChromeActive()) return;
    float mx = 0.f, my = 0.f;
    FieldDosChrome::pointerPixels(window, lx, ly, mx, my);
    FieldWmChrome::hover = FieldWmChrome::hitTest(mx, my);

    using CH = FieldWmChrome::ChromeHit;

    if (dragging && !Options::Canvas::DosPanelStretch) {
        FieldDosViewport::panelOx = dragOx0 + (mx - dragMx0);
        FieldDosViewport::panelOy = dragOy0 + (my - dragMy0);
        FieldDosViewport::panelPositioned = true;
        FieldDosViewport::clampPanelPosition();
    }
    if (resizing && !Options::Canvas::DosPanelStretch) {
        const float dx = mx - dragMx0;
        const float dy = my - dragMy0;
        float nw = dragW0;
        float nh = dragH0;
        float nx = dragOx0;
        float ny = dragOy0;
        switch (resizeEdge) {
        case CH::ResizeE:  nw = dragW0 + dx; break;
        case CH::ResizeW:  nw = dragW0 - dx; nx = dragOx0 + dx; break;
        case CH::ResizeS:  nh = dragH0 + dy; break;
        case CH::ResizeN:  nh = dragH0 - dy; ny = dragOy0 + dy; break;
        case CH::ResizeSE: nw = dragW0 + dx; nh = dragH0 + dy; break;
        case CH::ResizeSW: nw = dragW0 - dx; nh = dragH0 + dy; nx = dragOx0 + dx; break;
        case CH::ResizeNE: nw = dragW0 + dx; nh = dragH0 - dy; ny = dragOy0 + dy; break;
        case CH::ResizeNW: nw = dragW0 - dx; nh = dragH0 - dy; nx = dragOx0 + dx; ny = dragOy0 + dy; break;
        default: break;
        }
        const float aspect = dragBaseW0 / std::max(dragBaseH0, 1.f);
        if (resizeEdge >= CH::ResizeNE && resizeEdge <= CH::ResizeSW)
            nh = nw / std::max(aspect, 0.01f);
        nw = std::max(FieldWmChrome::MIN_PW, nw);
        nh = std::max(FieldWmChrome::MIN_PH, nh);
        panelScale = std::clamp(
            std::min(nw / std::max(dragBaseW0, 1.f), nh / std::max(dragBaseH0, 1.f)),
            0.55f, 2.2f);
        applyPanelScale();
        const float pw = FieldDosViewport::panelOuterW();
        const float ph = FieldDosViewport::panelOuterH();
        if (resizeEdge == CH::ResizeW || resizeEdge == CH::ResizeNW
            || resizeEdge == CH::ResizeSW)
            nx = dragOx0 + dragW0 - pw;
        if (resizeEdge == CH::ResizeN || resizeEdge == CH::ResizeNW
            || resizeEdge == CH::ResizeNE)
            ny = dragOy0 + dragH0 - ph;
        FieldDosViewport::panelOx = nx;
        FieldDosViewport::panelOy = ny;
        FieldDosViewport::panelPositioned = true;
        FieldDosViewport::clampPanelPosition();
    }
}

inline void onMouseUp() noexcept {
    if (dragging)
        applyEdgeSnap();
    if (dragging || resizing)
        FieldAmouranthOs::saveFocusedPanelPos();
    dragging = false;
    resizing = false;
    resizeEdge = FieldWmChrome::ChromeHit::None;
}

} // namespace FieldWmInput