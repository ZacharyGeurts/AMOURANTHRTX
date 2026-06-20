#pragma once

// WM input — drag, resize, edge snap (Mutter-style), window actions.

#include "FieldWmChrome.hpp"
#include "FieldWmDock.hpp"
#include "FieldDosChrome.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>

namespace FieldAmouranthOs {
void hideDosPanel() noexcept;
void markFocusedMinimized() noexcept;
void saveFocusedPanelPos() noexcept;
bool shellChromeActive() noexcept;
float chromeViewportW() noexcept;
float uiScale() noexcept;
float desktopTopInset() noexcept;
float scaledTaskbarH() noexcept;
} // fwd

namespace FieldWmInput {

constexpr float SNAP_MARGIN = 28.f;
constexpr float MAX_PANEL_SCALE = FieldWmDock::MAX_PANEL_SCALE;

inline int  pendingMenuAction = 0;
inline bool closeRequested    = false;
inline bool dragging          = false;
inline bool resizing          = false;
inline bool movePending       = false;
inline bool sizePending       = false;
inline FieldWmChrome::ChromeHit resizeEdge = FieldWmChrome::ChromeHit::None;
inline float dragMx0 = 0.f, dragMy0 = 0.f;
inline float dragOx0 = 0.f, dragOy0 = 0.f;
inline float dragW0 = 0.f, dragH0 = 0.f;
inline float dragBaseW0 = 0.f, dragBaseH0 = 0.f;
inline float panelScale = 1.f;

inline float desktopMaxScale() noexcept { return MAX_PANEL_SCALE; }

inline float scaleForViewportPercent(int pct) noexcept {
    pct = std::clamp(pct, 10, 400);
    const float sw = FieldAmouranthOs::chromeViewportW();
    const float margin = FieldWmDock::MARGIN_PX * FieldAmouranthOs::uiScale();
    const float targetW = (sw - margin * 2.f) * (static_cast<float>(pct) / 100.f);
    const float saved = FieldDosViewport::wmPanelScale;
    FieldDosViewport::wmPanelScale = 1.f;
    const float baseW = FieldDosViewport::panelOuterW();
    FieldDosViewport::wmPanelScale = saved;
    return std::clamp(targetW / std::max(baseW, 1.f),
        FieldWmDock::MIN_VIEWPORT_PCT, FieldWmDock::MAX_VIEWPORT_PCT);
}

inline void ensureTitleBarAccessible() noexcept {
    if (Options::Canvas::DosPanelStretch) return;
    const float pw = FieldDosViewport::panelOuterW();
    const float margin = 8.f * FieldWmChrome::wmUiScale();
    const float sw = FieldDosViewport::winW > 0.f ? FieldDosViewport::winW : 1920.f;
    const float sh = FieldDosViewport::winH > 0.f ? FieldDosViewport::winH : 1080.f;
    const float deskTop = FieldAmouranthOs::desktopTopInset();
    const float taskH = FieldAmouranthOs::scaledTaskbarH();
    const float titleH = FieldWmChrome::scaledTitleH();
    FieldDosViewport::panelOy = std::clamp(FieldDosViewport::panelOy, deskTop,
        std::max(deskTop, sh - taskH - titleH));
    if (pw <= sw - margin * 2.f)
        FieldDosViewport::panelOx = std::clamp(FieldDosViewport::panelOx, margin,
            std::max(margin, sw - pw - margin));
    else
        FieldDosViewport::panelOx = margin;
}

inline void applyPanelScale() noexcept {
    FieldDosViewport::fontScale = std::clamp(1.1f * panelScale, 1.0f, 4.0f);
    FieldDosViewport::sharpen = std::clamp(0.50f + panelScale * 0.08f, 0.45f, 0.75f);
    FieldDosViewport::crispFont = true;
    FieldDosViewport::subpixelFont = false;
    Options::Canvas::DosCrispFont = true;
    FieldDosViewport::panelGlow = 0.08f;
    FieldWmChrome::syncViewport(panelScale);
}

inline void applyScalePercent(int pct) noexcept {
    panelScale = scaleForViewportPercent(pct);
    applyPanelScale();
    ensureTitleBarAccessible();
    FieldAmouranthOs::saveFocusedPanelPos();
    FieldWmChrome::openMenu = FieldWmChrome::OpenMenu::None;
    FieldWmChrome::menuItemHover = -1;
}

inline void resetScale() noexcept {
    panelScale = 1.f;
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
        0.55f, desktopMaxScale());
    applyPanelScale();
    FieldDosViewport::panelOx = margin;
    FieldDosViewport::panelOy = deskTop + margin;
    FieldDosViewport::panelPositioned = true;
    FieldDosViewport::panelStretch = false;
    Options::Canvas::DosPanelStretch = false;
    Options::Canvas::ControlFlags &= ~Options::Canvas::ControlDosPanelStretch;
    ensureTitleBarAccessible();
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
        0.55f, desktopMaxScale());
    applyPanelScale();
    FieldDosViewport::panelOx = margin;
    FieldDosViewport::panelOy = deskTop + margin;
    FieldDosViewport::panelPositioned = true;
    ensureTitleBarAccessible();
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
        0.55f, desktopMaxScale());
    applyPanelScale();
    FieldDosViewport::panelOx = sw - margin - FieldDosViewport::panelOuterW();
    FieldDosViewport::panelOy = deskTop + margin;
    FieldDosViewport::panelPositioned = true;
    ensureTitleBarAccessible();
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
    FieldDosChrome::chromePointerPixels(window, lx, ly, mx, my);
    FieldWmChrome::hover = FieldWmChrome::hitTest(mx, my);
    if (FieldWmChrome::hover == FieldWmChrome::ChromeHit::None) return false;

    using CH = FieldWmChrome::ChromeHit;
    using OM = FieldWmChrome::OpenMenu;

    if (FieldWmChrome::hover == CH::AppIcon) {
        const bool opening = FieldWmChrome::openMenu != OM::System;
        FieldWmChrome::openMenu = opening ? OM::System : OM::None;
        if (opening) FieldWmChrome::systemMenuScroll = 0;
        FieldWmChrome::menuItemHover = -1;
        return true;
    }
    if (FieldWmChrome::openMenu == OM::System
            && FieldWmChrome::hover >= CH::MenuItem0
            && FieldWmChrome::hover < static_cast<CH>(
                static_cast<int>(CH::MenuItem0) + FieldWmChrome::SYSTEM_MENU_VISIBLE_ROWS)) {
        const int item = FieldWmChrome::menuItemHover;
        FieldWmChrome::openMenu = OM::None;
        FieldWmChrome::menuItemHover = -1;
        if (item == FieldWmChrome::SYSTEM_MENU_MOVE) {
            movePending = true;
            return true;
        }
        if (item == FieldWmChrome::SYSTEM_MENU_MINIMIZE) {
            FieldAmouranthOs::markFocusedMinimized();
            FieldAmouranthOs::hideDosPanel();
            return true;
        }
        if (item == FieldWmChrome::SYSTEM_MENU_MAXIMIZE) {
            maximizeFocusedWindow();
            return true;
        }
        if (item == FieldWmChrome::SYSTEM_MENU_CLOSE) {
            closeWindow();
            return true;
        }
        if (item >= FieldWmChrome::SYSTEM_MENU_SCALE0
                && item < FieldWmChrome::SYSTEM_MENU_MINIMIZE) {
            const int preset = item - FieldWmChrome::SYSTEM_MENU_SCALE0;
            if (preset >= 0 && preset < FieldWmChrome::SCALE_PRESET_COUNT)
                applyScalePercent(FieldWmChrome::SCALE_PRESETS[preset]);
            return true;
        }
        return true;
    }
    if (FieldWmChrome::hover >= CH::FileMenu && FieldWmChrome::hover <= CH::ExtraMenu) {
        const int idx = static_cast<int>(FieldWmChrome::hover)
            - static_cast<int>(CH::FileMenu);
        const auto picked = FieldWmChrome::menuIndexToOpen(idx);
        FieldWmChrome::openMenu = (FieldWmChrome::openMenu == picked) ? OM::None : picked;
        return true;
    }
    if (FieldWmChrome::hover >= CH::MenuItem0 && FieldWmChrome::hover <= CH::MenuItem7
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
    if (FieldWmChrome::hover == CH::TitleBar && clicks >= 2) {
        maximizeFocusedWindow();
        return true;
    }
    if (FieldWmChrome::hover == CH::TitleBar) {
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
        FieldWmCore::raiseFocusedProgram();
        return false;
    }
    return true;
}

inline void onMouseMotion(SDL_Window* window, float lx, float ly) noexcept {
    if (!FieldAmouranthOs::shellChromeActive()) return;
    float mx = 0.f, my = 0.f;
    FieldDosChrome::chromePointerPixels(window, lx, ly, mx, my);
    FieldWmChrome::hover = FieldWmChrome::hitTest(mx, my);

    using CH = FieldWmChrome::ChromeHit;

    if (movePending && !dragging && !Options::Canvas::DosPanelStretch) {
        dragging = true;
        dragMx0 = mx;
        dragMy0 = my;
        dragOx0 = FieldDosViewport::panelOx;
        dragOy0 = FieldDosViewport::panelOy;
        movePending = false;
    }
    if (sizePending && !resizing && !Options::Canvas::DosPanelStretch) {
        const FieldDosViewport::Rect win = FieldWmChrome::windowRect();
        resizing = true;
        resizeEdge = CH::ResizeSE;
        dragMx0 = mx;
        dragMy0 = my;
        dragOx0 = FieldDosViewport::panelOx;
        dragOy0 = FieldDosViewport::panelOy;
        dragW0 = win.w();
        dragH0 = win.h();
        dragBaseW0 = dragW0 / std::max(panelScale, 0.01f);
        dragBaseH0 = dragH0 / std::max(panelScale, 0.01f);
        sizePending = false;
    }
    if (dragging && !Options::Canvas::DosPanelStretch) {
        FieldDosViewport::panelOx = dragOx0 + (mx - dragMx0);
        FieldDosViewport::panelOy = dragOy0 + (my - dragMy0);
        FieldDosViewport::panelPositioned = true;
        ensureTitleBarAccessible();
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
        nw = std::max(FieldWmChrome::MIN_PW, nw);
        nh = std::max(FieldWmChrome::MIN_PH, nh);
        const float scaleW = nw / std::max(dragBaseW0, 1.f);
        const float scaleH = nh / std::max(dragBaseH0, 1.f);
        float targetSc = std::max(scaleW, scaleH);
        switch (resizeEdge) {
        case CH::ResizeE:
        case CH::ResizeW:
            targetSc = scaleW;
            break;
        case CH::ResizeS:
            targetSc = scaleH;
            break;
        case CH::ResizeSW:
        case CH::ResizeSE:
            targetSc = std::max(scaleW, scaleH);
            break;
        default:
            break;
        }
        panelScale = std::clamp(targetSc, 0.55f, desktopMaxScale());
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
        ensureTitleBarAccessible();
    }
}

inline void onMouseWheel(float wheelY) noexcept {
    if (FieldWmChrome::openMenu != FieldWmChrome::OpenMenu::System) return;
    const int maxScroll = FieldWmChrome::systemMenuMaxScroll();
    if (wheelY > 0.f)
        FieldWmChrome::systemMenuScroll = std::max(0, FieldWmChrome::systemMenuScroll - 1);
    else if (wheelY < 0.f)
        FieldWmChrome::systemMenuScroll = std::min(maxScroll,
            FieldWmChrome::systemMenuScroll + 1);
}

inline void onMouseUp() noexcept {
    if (dragging)
        applyEdgeSnap();
    if (dragging || resizing)
        FieldAmouranthOs::saveFocusedPanelPos();
    dragging = false;
    resizing = false;
    movePending = false;
    sizePending = false;
    resizeEdge = FieldWmChrome::ChromeHit::None;
}

} // namespace FieldWmInput