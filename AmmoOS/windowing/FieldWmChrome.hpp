#pragma once

// WM chrome — title bar hit testing, menus, classic vs GNOME header-bar skins.

#include "FieldWmCore.hpp"
#include "FieldDosChrome.hpp"
#include "FieldDosViewport.hpp"
#include "FieldAmouranthHudRam.hpp"
#include "FieldWinFrame.hpp"
#include "FieldWmNesMenu.hpp"
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
bool taskbarChromeCapturesPointer(float mx, float my) noexcept;
} // fwd

namespace FieldWmChrome {

constexpr float TITLE_H    = 32.f;
constexpr float BTN_W      = 30.f;
constexpr float GRIP       = 10.f;
constexpr float MIN_PW     = 420.f;
constexpr float MIN_PH     = 280.f;

enum class ChromeSkin : std::uint8_t { Classic = 0, Gnome = 1 };

enum class ChromeHit : std::uint8_t {
    None = 0, TitleBar, Close, Minimize, Maximize, AppIcon,
    ResizeN, ResizeS, ResizeE, ResizeW,
    ResizeNE, ResizeNW, ResizeSE, ResizeSW,
    FileMenu, EditMenu, ViewMenu, HelpMenu, ExtraMenu,
    MenuItem0, MenuItem1, MenuItem2, MenuItem3,
    MenuItem4, MenuItem5, MenuItem6, MenuItem7,
    Content
};

enum class OpenMenu : std::uint8_t {
    None = 0, File = 1, Edit = 2, View = 3, Help = 4, Extra = 5, System = 6
};

enum class MenuProfile : std::uint8_t { Standard = 0, NesEmu = 1 };

inline ChromeSkin chromeSkin  = ChromeSkin::Classic;
inline ChromeHit  hover       = ChromeHit::None;
inline OpenMenu   openMenu    = OpenMenu::None;
inline MenuProfile menuProfile = MenuProfile::Standard;
inline int        menuItemHover = -1;
inline int        systemMenuScroll = 0;

constexpr int SCALE_PRESET_COUNT = 12;
constexpr int SCALE_PRESETS[SCALE_PRESET_COUNT] = {
    10, 25, 50, 75, 100, 125, 150, 200, 250, 300, 350, 400
};
constexpr int SYSTEM_MENU_MOVE      = 0;
constexpr int SYSTEM_MENU_SCALE0    = 1;
constexpr int SYSTEM_MENU_MINIMIZE  = SYSTEM_MENU_SCALE0 + SCALE_PRESET_COUNT;
constexpr int SYSTEM_MENU_MAXIMIZE  = SYSTEM_MENU_MINIMIZE + 1;
constexpr int SYSTEM_MENU_CLOSE     = SYSTEM_MENU_MAXIMIZE + 1;
constexpr int SYSTEM_MENU_ITEM_COUNT = SYSTEM_MENU_CLOSE + 1;
constexpr int SYSTEM_MENU_VISIBLE_ROWS = 9;

inline float wmUiScale() noexcept {
    static float cached = 1.f;
    static int   lastW  = 0;
    const int w = FieldAmouranthOs::winW;
    if (w != lastW) {
        const float ref = w > 0 ? static_cast<float>(w) / 1920.f : 1.f;
        cached = std::max(ref, 0.75f) * 1.35f;
        lastW  = w;
    }
    return cached;
}

inline float shaderTitleH() noexcept {
    const float refW = FieldDosViewport::winW > 0.f ? FieldDosViewport::winW : 1920.f;
    const float h = TITLE_H * std::max(refW / 1920.f, 0.75f) * 1.35f;
    return chromeSkin == ChromeSkin::Gnome ? h * 1.08f : h;
}

inline float scaledTitleH() noexcept { return shaderTitleH(); }
inline float scaledGrip() noexcept { return GRIP * wmUiScale(); }
inline float scaledBtnW() noexcept { return BTN_W; }

inline float scaledBtnSz(float titleH) noexcept {
    return std::clamp(titleH - 6.f, 26.f, 34.f);
}

inline float iconBandW(float titleH) noexcept {
    return scaledBtnSz(titleH) * 0.58f + 18.f * wmUiScale();
}

inline FieldDosViewport::Rect iconRect(const FieldDosViewport::Rect& win, float tb) noexcept {
    const float uiSc = wmUiScale();
    const float titleH = tb - win.y0;
    const float iconSz = titleH * 0.58f;
    const float iconX = win.x0 + 10.f * uiSc;
    const float iconY = win.y0 + (titleH - iconSz) * 0.5f;
    return {iconX, iconY, iconX + iconSz, iconY + iconSz};
}
inline float scaledMenuBtnW() noexcept { return 46.f * wmUiScale(); }
inline float scaledMenuSpacing() noexcept { return 50.f * wmUiScale(); }
inline float scaledMenuDropH() noexcept { return 28.f * wmUiScale(); }

inline int menuBarCount() noexcept {
    return menuProfile == MenuProfile::NesEmu ? 5 : 4;
}

inline OpenMenu menuIndexToOpen(int idx) noexcept {
    if (menuProfile == MenuProfile::NesEmu) {
        switch (idx) {
        case 0: return OpenMenu::File;
        case 1: return OpenMenu::Edit;
        case 2: return OpenMenu::View;
        case 3: return OpenMenu::Extra;
        case 4: return OpenMenu::Help;
        default: return OpenMenu::None;
        }
    }
    switch (idx) {
    case 0: return OpenMenu::File;
    case 1: return OpenMenu::Edit;
    case 2: return OpenMenu::View;
    case 3: return OpenMenu::Help;
    default: return OpenMenu::None;
    }
}

inline int openMenuToIndex(OpenMenu m) noexcept {
    if (menuProfile == MenuProfile::NesEmu) {
        switch (m) {
        case OpenMenu::File: return 0;
        case OpenMenu::Edit: return 1;
        case OpenMenu::View: return 2;
        case OpenMenu::Extra: return 3;
        case OpenMenu::Help: return 4;
        default: return -1;
        }
    }
    switch (m) {
    case OpenMenu::File: return 0;
    case OpenMenu::Edit: return 1;
    case OpenMenu::View: return 2;
    case OpenMenu::Help: return 3;
    default: return -1;
    }
}

inline float menuBtnX0(const FieldDosViewport::Rect& win, int idx) noexcept {
    if (chromeSkin == ChromeSkin::Gnome) return -1.f;
    const float gap = menuProfile == MenuProfile::NesEmu
        ? 44.f * wmUiScale() : scaledMenuSpacing();
    const float base = win.x0 + 10.f * wmUiScale() + iconBandW(scaledTitleH());
    return base + static_cast<float>(idx) * gap;
}

inline int menuItemCount(OpenMenu m) noexcept {
    if (menuProfile == MenuProfile::NesEmu) {
        switch (m) {
        case OpenMenu::File: return 2;
        case OpenMenu::Edit: return FieldWmNesMenu::dropdownRows;
        case OpenMenu::View: return 7;
        case OpenMenu::Extra: return 2;
        case OpenMenu::Help: return 1;
        default: return 0;
        }
    }
    switch (m) {
    case OpenMenu::File: return 3;
    case OpenMenu::Edit: return 3;
    case OpenMenu::View: return 4;
    case OpenMenu::Help: return 2;
    default: return 0;
    }
}

inline int menuItemAction(OpenMenu m, int idx) noexcept {
    if (menuProfile == MenuProfile::NesEmu) {
        switch (m) {
        case OpenMenu::File:
            if (idx == 0) return FieldWinFrame::A_FILE_OPEN;
            if (idx == 1) return FieldWinFrame::A_FILE_EXIT;
            break;
        case OpenMenu::Edit:
            if (idx >= 0 && idx < FieldWmNesMenu::dropdownRows)
                return FieldWmNesMenu::dropdownActions[idx];
            break;
        case OpenMenu::View:
            if (idx == 0) return FieldWinFrame::A_EMU_PAUSE;
            if (idx == 1) return FieldWinFrame::A_EMU_RESUME;
            if (idx == 2) return FieldWinFrame::A_EMU_RESET;
            if (idx == 3) return FieldWinFrame::A_EMU_FRAME;
            if (idx == 4) return FieldWinFrame::A_EMU_TURBO;
            if (idx == 5) return FieldWinFrame::A_EMU_UNLIMITED;
            if (idx == 6) return FieldWinFrame::A_EMU_MUTE;
            break;
        case OpenMenu::Extra:
            if (idx == 0) return FieldWinFrame::A_OPT_SETTINGS;
            if (idx == 1) return FieldWinFrame::A_OPT_CONTROLS;
            break;
        case OpenMenu::Help:
            if (idx == 0) return FieldWinFrame::A_HELP_ABOUT;
            break;
        default: break;
        }
        return 0;
    }
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
    FieldDosViewport::Rect r = FieldDosChrome::chromeUsesRenderSpace()
        ? FieldDosViewport::panelRectRender() : FieldDosViewport::panelRect();
    if (FieldAmouranthOs::shellChromeActive() && Options::Canvas::DosPanelStretch
            && Options::AmouranthOs::EnableTaskbar) {
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

inline float visibleTitleRight(const FieldDosViewport::Rect& win) noexcept {
    const float sw = FieldDosViewport::winW > 0.f ? FieldDosViewport::winW : 1920.f;
    return std::min(win.x1, sw - 8.f * wmUiScale());
}

inline float titleButtonX0(const FieldDosViewport::Rect& win, float tb) noexcept {
    const float titleH = tb - win.y0;
    const float btn = scaledBtnSz(titleH);
    return visibleTitleRight(win) - btn * 3.f - 8.f;
}

inline FieldDosViewport::Rect titleButtonRect(const FieldDosViewport::Rect& win, float tb) noexcept {
    const float bx = titleButtonX0(win, tb);
    return {bx, win.y0, visibleTitleRight(win), tb};
}

inline ChromeHit hitResizeEdges(const FieldDosViewport::Rect& win, float mx, float my,
        float tb, bool allowResize) noexcept {
    if (!allowResize) return ChromeHit::None;
    const float g = scaledGrip();
    const bool bot = my >= win.y1 - g - FieldDosViewport::DOS_HUD_H;
    bool left = mx < win.x0 + g;
    bool right = mx >= win.x1 - g;
    if (my < tb) {
        const FieldDosViewport::Rect icon = iconRect(win, tb);
        if (mx < icon.x1 + g) left = false;
        const FieldDosViewport::Rect btns = titleButtonRect(win, tb);
        if (mx >= btns.x0 - g) right = false;
    }
    if (bot && left)  return ChromeHit::ResizeSW;
    if (bot && right) return ChromeHit::ResizeSE;
    if (bot)    return ChromeHit::ResizeS;
    if (left)   return ChromeHit::ResizeW;
    if (right)  return ChromeHit::ResizeE;
    return ChromeHit::None;
}

inline int systemMenuItemCount() noexcept { return SYSTEM_MENU_ITEM_COUNT; }

inline int systemMenuMaxScroll() noexcept {
    return std::max(0, SYSTEM_MENU_ITEM_COUNT - SYSTEM_MENU_VISIBLE_ROWS);
}

inline ChromeHit hitSystemMenuDropdown(const FieldDosViewport::Rect& win, float mx, float my,
        float tb) noexcept {
    if (openMenu != OpenMenu::System) return ChromeHit::None;
    const float uiSc = wmUiScale();
    const float dx0 = win.x0 + 10.f * uiSc;
    const float dy0 = tb;
    const float dx1 = dx0 + 196.f * uiSc;
    const float rowH = scaledMenuDropH();
    const float dy1 = dy0 + rowH * static_cast<float>(SYSTEM_MENU_VISIBLE_ROWS);
    if (mx >= dx0 - 4.f * uiSc && mx < dx1 && my >= dy0 && my < dy1) {
        const int row = static_cast<int>((my - dy0) / rowH);
        if (row >= 0 && row < SYSTEM_MENU_VISIBLE_ROWS) {
            const int item = systemMenuScroll + row;
            if (item >= 0 && item < SYSTEM_MENU_ITEM_COUNT) {
                menuItemHover = item;
                return static_cast<ChromeHit>(
                    static_cast<int>(ChromeHit::MenuItem0) + row);
            }
        }
    }
    return ChromeHit::None;
}

inline ChromeHit hitMenuDropdown(const FieldDosViewport::Rect& win, float mx, float my,
        float tb) noexcept {
    if (chromeSkin == ChromeSkin::Gnome || openMenu == OpenMenu::None)
        return ChromeHit::None;
    const int mIdx = openMenuToIndex(openMenu);
    if (mIdx < 0) return ChromeHit::None;
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

    const FieldDosViewport::Rect icon = iconRect(win, tb);
    if (icon.contains(mx, my)) {
        menuItemHover = -1;
        return ChromeHit::AppIcon;
    }

    if (chromeSkin == ChromeSkin::Classic) {
        const float btnW = scaledMenuBtnW();
        const int nMenus = menuBarCount();
        for (int i = 0; i < nMenus; ++i) {
            const float x0 = menuBtnX0(win, i);
            if (mx >= x0 && mx < x0 + btnW) {
                menuItemHover = -1;
                return static_cast<ChromeHit>(static_cast<int>(ChromeHit::FileMenu) + i);
            }
        }
    }

    if (wmPanelActive()) {
        const float titleH = tb - win.y0;
        const float btn = scaledBtnSz(titleH);
        const float bx = titleButtonX0(win, tb);
        if (mx >= bx + btn * 2.f) return ChromeHit::Close;
        if (mx >= bx + btn)       return ChromeHit::Maximize;
        if (mx >= bx)             return ChromeHit::Minimize;
    }
    return ChromeHit::TitleBar;
}

inline ChromeHit hitTest(float mx, float my) noexcept {
    if (!wmPanelActive()) return ChromeHit::None;
    if (FieldAmouranthOs::taskbarChromeCapturesPointer(mx, my)) return ChromeHit::None;
    const FieldDosViewport::Rect win = windowRect();
    const float tb = titleBarBottom(win);
    const float sw = FieldDosViewport::winW > 0.f ? FieldDosViewport::winW : 1920.f;
    const bool onTitleRow = my >= win.y0 && my < tb && mx >= 0.f && mx < sw;
    if (!win.contains(mx, my) && !onTitleRow) return ChromeHit::None;

    const bool allowResize = FieldAmouranthOs::panelVisible
        && !Options::Canvas::DosPanelStretch;

    if (my < tb) {
        if (ChromeHit menu = hitMenuDropdown(win, mx, my, tb);
                menu != ChromeHit::None)
            return menu;
        if (ChromeHit sys = hitSystemMenuDropdown(win, mx, my, tb);
                sys != ChromeHit::None)
            return sys;
        if (ChromeHit bar = hitTitleBar(win, mx, my, tb);
                bar != ChromeHit::None)
            return bar;
    }

    if (ChromeHit edge = hitResizeEdges(win, mx, my, tb, allowResize);
            edge != ChromeHit::None)
        return edge;

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