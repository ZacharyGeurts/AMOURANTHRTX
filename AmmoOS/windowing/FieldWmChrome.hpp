#pragma once

// WM chrome — title bar hit testing, menus, classic vs GNOME header-bar skins.

#include "FieldWmCore.hpp"
#include "FieldDosViewport.hpp"
#include "OptionsMenu.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace FieldAmouranthOs {
extern bool active;
extern bool consoleShell;
extern bool panelVisible;
extern int  winW;
extern float desktopTopInset() noexcept;
extern float scaledTaskbarH() noexcept;
bool shellChromeActive() noexcept;
} // fwd

namespace FieldWmChrome {

constexpr float TITLE_H    = 32.f;
constexpr float BTN_W      = 30.f;
constexpr float GRIP       = 4.f;
constexpr float MIN_PW     = 420.f;
constexpr float MIN_PH     = 280.f;

enum class ChromeSkin : std::uint8_t { Classic = 0, Gnome = 1 };

enum class ChromeHit : std::uint8_t {
    None = 0, TitleBar, Close, Minimize, Maximize,
    ResizeN, ResizeS, ResizeE, ResizeW,
    ResizeNE, ResizeNW, ResizeSE, ResizeSW,
    FileMenu, EditMenu, ViewMenu, HelpMenu,
    MenuItem0, MenuItem1, MenuItem2, MenuItem3,
    Content
};

enum class OpenMenu : std::uint8_t { None = 0, File = 1, Edit = 2, View = 3, Help = 4 };

inline ChromeSkin chromeSkin  = ChromeSkin::Classic;
inline ChromeHit  hover       = ChromeHit::None;
inline OpenMenu   openMenu    = OpenMenu::None;
inline int        menuItemHover = -1;

inline float wmUiScale() noexcept {
    const float ref = FieldAmouranthOs::winW > 0
        ? static_cast<float>(FieldAmouranthOs::winW) / 1920.f : 1.f;
    return std::max(ref, 0.75f) * 1.35f;
}

inline float shaderTitleH() noexcept {
    const float refW = FieldDosViewport::winW > 0.f ? FieldDosViewport::winW : 1920.f;
    const float h = TITLE_H * std::max(refW / 1920.f, 0.75f) * 1.35f;
    return chromeSkin == ChromeSkin::Gnome ? h * 1.08f : h;
}

inline float scaledTitleH() noexcept { return shaderTitleH(); }
inline float scaledGrip() noexcept { return GRIP; }
inline float scaledBtnW() noexcept { return BTN_W; }
inline float scaledMenuBtnW() noexcept { return 46.f * wmUiScale(); }
inline float scaledMenuSpacing() noexcept { return 50.f * wmUiScale(); }
inline float scaledMenuDropH() noexcept { return 28.f * wmUiScale(); }

inline float menuBtnX0(const FieldDosViewport::Rect& win, int idx) noexcept {
    if (chromeSkin == ChromeSkin::Gnome) return -1.f;
    return win.x0 + 10.f * wmUiScale()
        + static_cast<float>(idx) * scaledMenuSpacing();
}

inline int menuItemCount(OpenMenu m) noexcept {
    switch (m) {
    case OpenMenu::File: return 3;
    case OpenMenu::Edit: return 3;
    case OpenMenu::View: return 4;
    case OpenMenu::Help: return 2;
    default: return 0;
    }
}

inline int menuItemAction(OpenMenu m, int idx) noexcept {
    switch (m) {
    case OpenMenu::File:
        if (idx == 0) return 101;
        if (idx == 1) return 103;
        if (idx == 2) return 109;
        break;
    case OpenMenu::Edit:
        if (idx == 0) return 201;
        if (idx == 1) return 206;
        if (idx == 2) return 205;
        break;
    case OpenMenu::View:
        if (idx == 0) return 301;
        if (idx == 1) return 302;
        if (idx == 2) return 303;
        if (idx == 3) return 304;
        break;
    case OpenMenu::Help:
        if (idx == 0) return 401;
        if (idx == 1) return 402;
        break;
    default: break;
    }
    return 0;
}

inline FieldDosViewport::Rect windowRect() noexcept {
    FieldDosViewport::Rect r = FieldDosViewport::panelRect();
    if (FieldAmouranthOs::consoleShell && Options::Canvas::DosPanelStretch) {
        const float taskH = FieldAmouranthOs::scaledTaskbarH();
        r.y1 = std::max(r.y0 + 1.f, r.y1 - taskH);
    }
    return r;
}

inline float titleBarBottom(const FieldDosViewport::Rect& win) noexcept {
    return win.y0 + scaledTitleH();
}

inline bool wmPanelActive() noexcept {
    return FieldAmouranthOs::shellChromeActive()
        && (FieldAmouranthOs::panelVisible || FieldAmouranthOs::consoleShell);
}

inline ChromeHit hitResizeEdges(const FieldDosViewport::Rect& win, float mx, float my,
        float tb, bool allowResize) noexcept {
    if (!allowResize) return ChromeHit::None;
    const float g = scaledGrip();
    const bool top = my < win.y0 + g;
    const bool bot = my >= win.y1 - g - FieldDosViewport::DOS_HUD_H;
    const bool left = mx < win.x0 + g;
    const bool right = mx >= win.x1 - g;
    if (top && left)  return ChromeHit::ResizeNW;
    if (top && right) return ChromeHit::ResizeNE;
    if (bot && left)  return ChromeHit::ResizeSW;
    if (bot && right) return ChromeHit::ResizeSE;
    if (top)    return ChromeHit::ResizeN;
    if (bot)    return ChromeHit::ResizeS;
    if (left)   return ChromeHit::ResizeW;
    if (right)  return ChromeHit::ResizeE;
    return ChromeHit::None;
}

inline ChromeHit hitMenuDropdown(const FieldDosViewport::Rect& win, float mx, float my,
        float tb) noexcept {
    if (chromeSkin == ChromeSkin::Gnome || openMenu == OpenMenu::None)
        return ChromeHit::None;
    const int mIdx = static_cast<int>(openMenu) - 1;
    const float dx0 = menuBtnX0(win, mIdx);
    const float dy0 = tb;
    const float dx1 = dx0 + 196.f * wmUiScale();
    const int nItems = menuItemCount(openMenu);
    const float dy1 = dy0 + scaledMenuDropH() * static_cast<float>(nItems);
    if (mx >= dx0 - 4.f * wmUiScale() && mx < dx1 && my >= dy0 && my < dy1) {
        const int item = static_cast<int>((my - dy0) / scaledMenuDropH());
        if (item >= 0 && item < nItems) {
            menuItemHover = item;
            return static_cast<ChromeHit>(
                static_cast<int>(ChromeHit::MenuItem0) + item);
        }
    }
    return ChromeHit::None;
}

inline ChromeHit hitTitleBar(const FieldDosViewport::Rect& win, float mx, float my,
        float tb) noexcept {
    if (my >= tb) return ChromeHit::None;

    if (chromeSkin == ChromeSkin::Classic) {
        const float btnW = scaledMenuBtnW();
        for (int i = 0; i < 4; ++i) {
            const float x0 = menuBtnX0(win, i);
            if (mx >= x0 && mx < x0 + btnW) {
                menuItemHover = -1;
                return static_cast<ChromeHit>(static_cast<int>(ChromeHit::FileMenu) + i);
            }
        }
    }

    if (FieldAmouranthOs::panelVisible) {
        const float bw = scaledBtnW();
        const float bx = win.x1 - bw * 3.f - 10.f;
        if (mx >= bx + bw * 2.f) return ChromeHit::Close;
        if (mx >= bx + bw)       return ChromeHit::Maximize;
        if (mx >= bx)            return ChromeHit::Minimize;
    }
    return ChromeHit::TitleBar;
}

inline ChromeHit hitTest(float mx, float my) noexcept {
    if (!wmPanelActive()) return ChromeHit::None;
    const FieldDosViewport::Rect win = windowRect();
    if (!win.contains(mx, my)) return ChromeHit::None;

    const float tb = titleBarBottom(win);
    const bool allowResize = FieldAmouranthOs::panelVisible
        && !Options::Canvas::DosPanelStretch;

    if (ChromeHit edge = hitResizeEdges(win, mx, my, tb, allowResize);
            edge != ChromeHit::None)
        return edge;

    if (ChromeHit menu = hitMenuDropdown(win, mx, my, tb);
            menu != ChromeHit::None)
        return menu;

    if (ChromeHit bar = hitTitleBar(win, mx, my, tb);
            bar != ChromeHit::None)
        return bar;

    if (FieldDosViewport::contentRect().contains(mx, my))
        return ChromeHit::Content;
    return ChromeHit::TitleBar;
}

inline void syncViewport(float panelScale) noexcept {
    FieldDosViewport::wmPanelScale = panelScale;
    FieldDosViewport::chromeTitleH = FieldAmouranthOs::shellChromeActive()
        ? scaledTitleH() : 0.f;
}

} // namespace FieldWmChrome