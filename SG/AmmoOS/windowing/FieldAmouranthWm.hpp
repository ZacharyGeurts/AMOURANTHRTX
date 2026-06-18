#pragma once

// AmouranthOS RTX window manager — title bar, controls, resize grips, surface stack.

#include "FieldDosChrome.hpp"
#include "FieldDosViewport.hpp"
#include "OptionsMenu.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace FieldAmouranthOs {
enum class AppId : std::uint8_t;
struct Program;
extern bool active;
extern bool consoleShell;
extern bool panelVisible;
extern int  winW;
extern std::vector<Program> programs;
extern AppId focusedApp;
extern int focusedProgId;
bool shellChromeActive() noexcept;
void hideDosPanel() noexcept;
void syncDesktopState() noexcept;
void removeTopProgram() noexcept;
void markFocusedMinimized() noexcept;
void saveFocusedPanelPos() noexcept;
float desktopTopInset() noexcept;
float scaledTaskbarH() noexcept;
} // fwd

namespace FieldAmouranthWm {

inline float wmUiScale() noexcept {
    const float ref = FieldAmouranthOs::winW > 0
        ? static_cast<float>(FieldAmouranthOs::winW) / 1920.f : 1.f;
    return std::max(ref, 0.65f);
}

constexpr float TITLE_H    = 22.f;
constexpr float BTN_W      = 30.f;
constexpr float GRIP       = 4.f;
constexpr float MIN_PW     = 420.f;
constexpr float MIN_PH     = 280.f;

constexpr int           MAX_SURFACES = 8;
constexpr std::uint32_t SURFACE_RAM  = 0x000B9000u;
constexpr int           SURF_STRIDE  = 16;

constexpr std::uint32_t BUS_SURF_COUNT_SHIFT  = 24u;
constexpr std::uint32_t BUS_SURF_FOCUS_SHIFT  = 16u;
constexpr std::uint32_t BUS_SURF_STACK_SHIFT  = 24u;
constexpr std::uint32_t BUS_SURF_FLAGS_SHIFT  = 16u;
constexpr std::uint32_t SURF_FLAG_MINIMIZED   = 1u << 0u;
constexpr std::uint32_t SURF_FLAG_FOCUSED     = 1u << 1u;
constexpr std::uint32_t SURF_FLAG_VISIBLE     = 1u << 2u;

enum class ChromeHit : std::uint8_t {
    None = 0, TitleBar, Close, Minimize, Maximize,
    ResizeN, ResizeS, ResizeE, ResizeW,
    ResizeNE, ResizeNW, ResizeSE, ResizeSW,
    FileMenu, EditMenu, ViewMenu, HelpMenu,
    MenuItem0, MenuItem1, MenuItem2, MenuItem3,
    Content
};

enum class OpenMenu : std::uint8_t { None = 0, File = 1, Edit = 2, View = 3, Help = 4 };

struct SurfaceSlot {
    int         programIdx = -1;
    float       ox = 0.f;
    float       oy = 0.f;
    float       scale = 1.f;
    bool        minimized = false;
    std::uint8_t tabIdx = 0u;
};

inline ChromeHit hover     = ChromeHit::None;
inline OpenMenu openMenu   = OpenMenu::None;
inline int menuItemHover   = -1;
inline int pendingMenuAction = 0;
inline bool closeRequested = false;
inline bool dragging       = false;
inline bool resizing       = false;
inline ChromeHit resizeEdge = ChromeHit::None;
inline float dragMx0 = 0.f, dragMy0 = 0.f;
inline float dragOx0 = 0.f, dragOy0 = 0.f;
inline float dragW0 = 0.f, dragH0 = 0.f;
inline float dragBaseW0 = 0.f, dragBaseH0 = 0.f;
inline float panelScale    = 1.f;
inline std::uint8_t stackRevision = 0u;
inline SurfaceSlot surfaces[MAX_SURFACES]{};

/* Match CANVAS.comp rtxWmTitleH / rtxWmBtn / rtxWmResizeGrip (fixed grip + btn sizes). */
inline float shaderTitleH() noexcept {
    const float refW = FieldDosViewport::winW > 0.f ? FieldDosViewport::winW : 1920.f;
    return TITLE_H * std::max(refW / 1920.f, 0.65f);
}

inline float scaledTitleH() noexcept {
    return shaderTitleH();
}

inline void syncViewport() noexcept {
    FieldDosViewport::wmPanelScale = panelScale;
    FieldDosViewport::chromeTitleH = FieldAmouranthOs::shellChromeActive()
        ? scaledTitleH() : 0.f;
}

inline float scaledGrip() noexcept {
    return GRIP;
}

inline float scaledBtnW() noexcept {
    return BTN_W;
}

inline float scaledMenuBtnW() noexcept {
    return 46.f * wmUiScale();
}

inline float scaledMenuSpacing() noexcept {
    return 50.f * wmUiScale();
}

inline float scaledFileW() noexcept {
    return scaledMenuBtnW();
}

inline float menuBtnX0(const FieldDosViewport::Rect& win, int idx) noexcept {
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

inline float scaledMenuDropH() noexcept {
    return 24.f * wmUiScale();
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

inline int focusedStackIndex() noexcept {
    int tab = 0;
    for (std::size_t i = 0; i < FieldAmouranthOs::programs.size(); ++i) {
        const auto& p = FieldAmouranthOs::programs[i];
        if (!p.running) continue;
        if (p.id == FieldAmouranthOs::focusedProgId)
            return tab;
        ++tab;
    }
    return tab > 0 ? 0 : -1;
}

inline void rebuildSurfaceStack() noexcept {
    int tab = 0;
    for (int i = 0; i < MAX_SURFACES; ++i)
        surfaces[i] = SurfaceSlot{};
    for (std::size_t i = 0; i < FieldAmouranthOs::programs.size() && tab < MAX_SURFACES; ++i) {
        auto& p = FieldAmouranthOs::programs[i];
        if (!p.running) continue;
        auto& s = surfaces[tab];
        s.programIdx = static_cast<int>(i);
        s.minimized = p.minimized;
        s.tabIdx = static_cast<std::uint8_t>(tab);
        s.scale = panelScale;
        if (p.id == FieldAmouranthOs::focusedProgId
                && FieldAmouranthOs::panelVisible && !p.minimized) {
            s.ox = FieldDosViewport::panelOx;
            s.oy = FieldDosViewport::panelOy;
            s.scale = panelScale;
        } else if (p.panelOx >= 0.f) {
            s.ox = p.panelOx;
            s.oy = p.panelOy;
            s.scale = p.panelScale > 0.f ? p.panelScale : panelScale;
        }
        ++tab;
    }
}

inline void raiseFocusedProgram() noexcept {
    if (FieldAmouranthOs::focusedProgId <= 0) return;
    auto& progs = FieldAmouranthOs::programs;
    for (std::size_t i = 0; i < progs.size(); ++i) {
        if (progs[i].running && progs[i].id == FieldAmouranthOs::focusedProgId) {
            FieldAmouranthOs::Program top = progs[i];
            progs.erase(progs.begin() + static_cast<std::ptrdiff_t>(i));
            progs.push_back(top);
            ++stackRevision;
            return;
        }
    }
}

inline void focusTitleBar() noexcept {
    raiseFocusedProgram();
}

inline bool wmPanelActive() noexcept {
    return FieldAmouranthOs::shellChromeActive()
        && (FieldAmouranthOs::panelVisible || FieldAmouranthOs::consoleShell);
}

inline ChromeHit hitTest(float mx, float my) noexcept {
    if (!wmPanelActive()) return ChromeHit::None;
    const FieldDosViewport::Rect win = windowRect();
    if (!win.contains(mx, my)) return ChromeHit::None;

    const float g = scaledGrip();
    const float tb = titleBarBottom(win);
    const bool allowResize = FieldAmouranthOs::panelVisible
        && !Options::Canvas::DosPanelStretch;
    const bool top = allowResize && my < win.y0 + g;
    const bool bot = allowResize && my >= win.y1 - g - FieldDosViewport::DOS_HUD_H;
    const bool left = allowResize && mx < win.x0 + g;
    const bool right = allowResize && mx >= win.x1 - g;

    if (top && left)  return ChromeHit::ResizeNW;
    if (top && right) return ChromeHit::ResizeNE;
    if (bot && left)  return ChromeHit::ResizeSW;
    if (bot && right) return ChromeHit::ResizeSE;
    if (top)    return ChromeHit::ResizeN;
    if (bot)    return ChromeHit::ResizeS;
    if (left)   return ChromeHit::ResizeW;
    if (right)  return ChromeHit::ResizeE;

    if (my < tb) {
        const float btnW = scaledMenuBtnW();
        const float dropH = scaledMenuDropH();
        if (openMenu != OpenMenu::None) {
            const int mIdx = static_cast<int>(openMenu) - 1;
            const float dx0 = menuBtnX0(win, mIdx);
            const float dy0 = tb;
            const float dx1 = dx0 + 196.f * wmUiScale();
            const int nItems = menuItemCount(openMenu);
            const float dy1 = dy0 + dropH * static_cast<float>(nItems);
            if (mx >= dx0 && mx < dx1 && my >= dy0 && my < dy1) {
                const int item = static_cast<int>((my - dy0) / dropH);
                if (item >= 0 && item < nItems) {
                    menuItemHover = item;
                    return static_cast<ChromeHit>(
                        static_cast<int>(ChromeHit::MenuItem0) + item);
                }
            }
        }
        for (int i = 0; i < 4; ++i) {
            const float x0 = menuBtnX0(win, i);
            if (mx >= x0 && mx < x0 + btnW) {
                menuItemHover = -1;
                return static_cast<ChromeHit>(static_cast<int>(ChromeHit::FileMenu) + i);
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

    if (FieldDosViewport::contentRect().contains(mx, my))
        return ChromeHit::Content;
    return ChromeHit::TitleBar;
}

inline void applyPanelScale() noexcept {
    FieldDosViewport::fontScale = std::clamp(1.1f * panelScale, 1.0f, 2.5f);
    FieldDosViewport::sharpen = std::clamp(0.50f + panelScale * 0.08f, 0.45f, 0.75f);
    FieldDosViewport::crispFont = true;
    FieldDosViewport::subpixelFont = false;
    FieldDosViewport::panelGlow = 0.08f;
    syncViewport();
}

inline void resetScale() noexcept {
    panelScale = 1.25f;
    applyPanelScale();
}

inline void closeWindow() noexcept {
    openMenu = OpenMenu::None;
    menuItemHover = -1;
    closeRequested = true;
    FieldAmouranthOs::hideDosPanel();
}

inline void maximizeFocusedWindow() noexcept {
    const float sw = FieldDosViewport::winW > 0.f ? FieldDosViewport::winW : 1920.f;
    const float sh = FieldDosViewport::winH > 0.f ? FieldDosViewport::winH : 1080.f;
    const float deskTop = FieldAmouranthOs::desktopTopInset();
    const float taskH = FieldAmouranthOs::scaledTaskbarH();
    const float margin = 10.f * wmUiScale();
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
    openMenu = OpenMenu::None;
    menuItemHover = -1;
}

inline bool onMouseDown(SDL_Window* window, float lx, float ly, Uint8 clicks) noexcept {
    if (!wmPanelActive()) return false;
    float mx = 0.f, my = 0.f;
    FieldDosChrome::pointerPixels(window, lx, ly, mx, my);
    hover = hitTest(mx, my);
    if (hover == ChromeHit::None) return false;

    if (hover >= ChromeHit::FileMenu && hover <= ChromeHit::HelpMenu) {
        const auto picked = static_cast<OpenMenu>(
            static_cast<int>(OpenMenu::File)
            + (static_cast<int>(hover) - static_cast<int>(ChromeHit::FileMenu)));
        openMenu = (openMenu == picked) ? OpenMenu::None : picked;
        return true;
    }
    if (hover >= ChromeHit::MenuItem0 && hover <= ChromeHit::MenuItem3
            && openMenu != OpenMenu::None) {
        const int item = static_cast<int>(hover) - static_cast<int>(ChromeHit::MenuItem0);
        const int action = menuItemAction(openMenu, item);
        openMenu = OpenMenu::None;
        menuItemHover = -1;
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
    if (hover == ChromeHit::Close) {
        closeWindow();
        return true;
    }
    if (hover == ChromeHit::Minimize) {
        openMenu = OpenMenu::None;
        menuItemHover = -1;
        FieldAmouranthOs::markFocusedMinimized();
        FieldAmouranthOs::hideDosPanel();
        return true;
    }
    if (hover == ChromeHit::Maximize) {
        maximizeFocusedWindow();
        return true;
    }

    const FieldDosViewport::Rect win = windowRect();
    if (hover == ChromeHit::TitleBar && clicks < 2) {
        openMenu = OpenMenu::None;
        menuItemHover = -1;
        focusTitleBar();
        dragging = true;
        dragMx0 = mx; dragMy0 = my;
        dragOx0 = FieldDosViewport::panelOx;
        dragOy0 = FieldDosViewport::panelOy;
        return true;
    }
    if (hover >= ChromeHit::ResizeN && hover <= ChromeHit::ResizeSW) {
        focusTitleBar();
        resizing = true;
        resizeEdge = hover;
        dragMx0 = mx; dragMy0 = my;
        dragOx0 = FieldDosViewport::panelOx;
        dragOy0 = FieldDosViewport::panelOy;
        dragW0 = win.w();
        dragH0 = win.h();
        dragBaseW0 = dragW0 / std::max(panelScale, 0.01f);
        dragBaseH0 = dragH0 / std::max(panelScale, 0.01f);
        return true;
    }
    if (hover == ChromeHit::Content) {
        openMenu = OpenMenu::None;
        menuItemHover = -1;
        return false;
    }
    return true;
}

inline void onMouseMotion(SDL_Window* window, float lx, float ly) noexcept {
    if (!FieldAmouranthOs::shellChromeActive()) return;
    float mx = 0.f, my = 0.f;
    FieldDosChrome::pointerPixels(window, lx, ly, mx, my);
    hover = hitTest(mx, my);

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
        case ChromeHit::ResizeE:  nw = dragW0 + dx; break;
        case ChromeHit::ResizeW:  nw = dragW0 - dx; nx = dragOx0 + dx; break;
        case ChromeHit::ResizeS:  nh = dragH0 + dy; break;
        case ChromeHit::ResizeN:  nh = dragH0 - dy; ny = dragOy0 + dy; break;
        case ChromeHit::ResizeSE: nw = dragW0 + dx; nh = dragH0 + dy; break;
        case ChromeHit::ResizeSW: nw = dragW0 - dx; nh = dragH0 + dy; nx = dragOx0 + dx; break;
        case ChromeHit::ResizeNE: nw = dragW0 + dx; nh = dragH0 - dy; ny = dragOy0 + dy; break;
        case ChromeHit::ResizeNW: nw = dragW0 - dx; nh = dragH0 - dy; nx = dragOx0 + dx; ny = dragOy0 + dy; break;
        default: break;
        }
        const float aspect = dragBaseW0 / std::max(dragBaseH0, 1.f);
        if (resizeEdge >= ChromeHit::ResizeNE && resizeEdge <= ChromeHit::ResizeSW) {
            nh = nw / std::max(aspect, 0.01f);
        }
        nw = std::max(MIN_PW, nw);
        nh = std::max(MIN_PH, nh);
        panelScale = std::clamp(
            std::min(nw / std::max(dragBaseW0, 1.f), nh / std::max(dragBaseH0, 1.f)),
            0.55f, 2.2f);
        applyPanelScale();
        const float pw = FieldDosViewport::panelOuterW();
        const float ph = FieldDosViewport::panelOuterH();
        if (resizeEdge == ChromeHit::ResizeW || resizeEdge == ChromeHit::ResizeNW
            || resizeEdge == ChromeHit::ResizeSW)
            nx = dragOx0 + dragW0 - pw;
        if (resizeEdge == ChromeHit::ResizeN || resizeEdge == ChromeHit::ResizeNW
            || resizeEdge == ChromeHit::ResizeNE)
            ny = dragOy0 + dragH0 - ph;
        FieldDosViewport::panelOx = nx;
        FieldDosViewport::panelOy = ny;
        FieldDosViewport::panelPositioned = true;
        FieldDosViewport::clampPanelPosition();
    }
}

inline void onMouseUp() noexcept {
    if (dragging || resizing)
        FieldAmouranthOs::saveFocusedPanelPos();
    dragging = false;
    resizing = false;
    resizeEdge = ChromeHit::None;
}

inline void writeRamU16(std::uint8_t* ram, std::uint32_t off, std::uint16_t v) noexcept {
    if (!ram) return;
    ram[off] = static_cast<std::uint8_t>(v & 0xFFu);
    ram[off + 1u] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
}

inline void packSurfaceRam(std::uint8_t* ram) noexcept {
    if (!ram) return;
    rebuildSurfaceStack();
    int surfCount = 0;
    int focusIdx = focusedStackIndex();
    for (std::size_t i = 0; i < FieldAmouranthOs::programs.size() && surfCount < MAX_SURFACES; ++i) {
        if (FieldAmouranthOs::programs[i].running) ++surfCount;
    }
    ram[SURFACE_RAM] = static_cast<std::uint8_t>(surfCount);
    ram[SURFACE_RAM + 1u] = static_cast<std::uint8_t>(focusIdx >= 0 ? focusIdx : 0xFFu);
    ram[SURFACE_RAM + 2u] = stackRevision;
    ram[SURFACE_RAM + 3u] = FieldAmouranthOs::panelVisible ? 1u : 0u;

    int tab = 0;
    for (std::size_t i = 0; i < FieldAmouranthOs::programs.size() && tab < MAX_SURFACES; ++i) {
        const auto& p = FieldAmouranthOs::programs[i];
        if (!p.running) continue;
        const std::uint32_t base = SURFACE_RAM + 0x10u
            + static_cast<std::uint32_t>(tab * SURF_STRIDE);
        float ox = 0.f, oy = 0.f;
        float sc = panelScale;
        const bool isFocus = p.id == FieldAmouranthOs::focusedProgId;
        const bool visible = FieldAmouranthOs::panelVisible && isFocus && !p.minimized;
        if (visible) {
            ox = FieldDosViewport::panelOx;
            oy = FieldDosViewport::panelOy;
            sc = panelScale;
        } else if (p.panelOx >= 0.f) {
            ox = p.panelOx;
            oy = p.panelOy;
            sc = p.panelScale > 0.f ? p.panelScale : panelScale;
        }
        const float pw = FieldDosViewport::panelOuterW();
        const float ph = FieldDosViewport::panelOuterH();
        writeRamU16(ram, base, static_cast<std::uint16_t>(std::clamp(ox, 0.f, 65534.f)));
        writeRamU16(ram, base + 2u, static_cast<std::uint16_t>(std::clamp(oy, 0.f, 65534.f)));
        writeRamU16(ram, base + 4u, static_cast<std::uint16_t>(std::clamp(pw, 1.f, 65535.f)));
        writeRamU16(ram, base + 6u, static_cast<std::uint16_t>(std::clamp(ph, 1.f, 65535.f)));
        writeRamU16(ram, base + 8u, static_cast<std::uint16_t>(sc * 256.f) & 0xFFFFu);
        ram[base + 10u] = static_cast<std::uint8_t>(tab);
        std::uint8_t flags = 0u;
        if (p.minimized) flags |= 1u;
        if (isFocus) flags |= 2u;
        if (visible) flags |= 4u;
        ram[base + 11u] = flags;
        ram[base + 12u] = static_cast<std::uint8_t>(tab);
        ++tab;
    }
}

inline void packCompositorBus(std::uint32_t* bus) noexcept {
    if (!bus) return;
    int surfCount = 0;
    int focusIdx = focusedStackIndex();
    std::uint32_t flags = 0u;
    for (const auto& p : FieldAmouranthOs::programs) {
        if (p.running) ++surfCount;
    }
    if (!FieldAmouranthOs::panelVisible || surfCount <= 0) {
        bus[60] = 0u;
        bus[61] = 0u;
        return;
    }
    const FieldDosViewport::Rect win = windowRect();
    const float csx = FieldDosViewport::coordScaleX();
    const float csy = FieldDosViewport::coordScaleY();
    const std::uint32_t rw = static_cast<std::uint32_t>(win.w() * csx) & 0xFFFFu;
    const std::uint32_t rh = static_cast<std::uint32_t>(win.h() * csy) & 0xFFFFu;
    for (const auto& p : FieldAmouranthOs::programs) {
        if (p.id == FieldAmouranthOs::focusedProgId) {
            if (p.minimized) flags |= SURF_FLAG_MINIMIZED;
            break;
        }
    }
    flags |= SURF_FLAG_FOCUSED | SURF_FLAG_VISIBLE;
    bus[60] = (static_cast<std::uint32_t>(std::min(surfCount, MAX_SURFACES))
                << BUS_SURF_COUNT_SHIFT)
            | (static_cast<std::uint32_t>(std::max(focusIdx, 0)) & 0xFFu
                << BUS_SURF_FOCUS_SHIFT)
            | rw;
    bus[61] = (static_cast<std::uint32_t>(stackRevision) << BUS_SURF_STACK_SHIFT)
            | (flags << BUS_SURF_FLAGS_SHIFT)
            | rh;
}

inline void packIntoBus(std::uint32_t* bus) noexcept {
    if (!bus || !FieldAmouranthOs::shellChromeActive()) return;
    syncViewport();
    bus[52] = static_cast<std::uint32_t>(hover) & 0xFu;
    if (dragging) bus[52] |= 1u << 4;
    if (resizing) bus[52] |= 1u << 5;
    if (Options::Canvas::DosPanelStretch) bus[52] |= 1u << 6;
    if (openMenu != OpenMenu::None) {
        bus[52] |= 1u << 7;
        bus[52] |= (static_cast<std::uint32_t>(openMenu) & 7u) << 8;
        if (menuItemHover >= 0)
            bus[52] |= (static_cast<std::uint32_t>(menuItemHover) & 0xFu) << 11;
    }
    bus[53] = static_cast<std::uint32_t>(panelScale * 256.f) & 0xFFFFu;
    packCompositorBus(bus);
}

} // namespace FieldAmouranthWm