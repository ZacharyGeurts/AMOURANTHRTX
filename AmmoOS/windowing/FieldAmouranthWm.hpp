#pragma once

// AmouranthOS RTX window manager — facade over modular WM layers.
//   FieldWmCore.hpp       — window pool, z-order (ported from microui MIT)
//   FieldWmChrome.hpp     — hit testing, classic / GNOME header-bar skins
//   FieldWmInput.hpp      — drag, resize, edge snap
//   FieldWmCompositor.hpp — SURFACE_RAM + data_bus packing

#include "FieldWmCompositor.hpp"

namespace FieldAmouranthWm {

using ChromeHit  = FieldWmChrome::ChromeHit;
using OpenMenu   = FieldWmChrome::OpenMenu;
using ChromeSkin = FieldWmChrome::ChromeSkin;
using SurfaceSlot = FieldWmCore::SurfaceSlot;

constexpr float TITLE_H = FieldWmChrome::TITLE_H;
constexpr float BTN_W   = FieldWmChrome::BTN_W;
constexpr float GRIP    = FieldWmChrome::GRIP;
constexpr float MIN_PW  = FieldWmChrome::MIN_PW;
constexpr float MIN_PH  = FieldWmChrome::MIN_PH;

constexpr int           MAX_SURFACES = FieldWmCompositor::MAX_SURFACES;
constexpr std::uint32_t SURFACE_RAM  = FieldWmCompositor::SURFACE_RAM;
constexpr int           SURF_STRIDE    = FieldWmCompositor::SURF_STRIDE;

constexpr std::uint32_t BUS_SURF_COUNT_SHIFT = FieldWmCompositor::BUS_SURF_COUNT_SHIFT;
constexpr std::uint32_t BUS_SURF_FOCUS_SHIFT = FieldWmCompositor::BUS_SURF_FOCUS_SHIFT;
constexpr std::uint32_t BUS_SURF_STACK_SHIFT = FieldWmCompositor::BUS_SURF_STACK_SHIFT;
constexpr std::uint32_t BUS_SURF_FLAGS_SHIFT = FieldWmCompositor::BUS_SURF_FLAGS_SHIFT;
constexpr std::uint32_t SURF_FLAG_MINIMIZED  = FieldWmCompositor::SURF_FLAG_MINIMIZED;
constexpr std::uint32_t SURF_FLAG_FOCUSED    = FieldWmCompositor::SURF_FLAG_FOCUSED;
constexpr std::uint32_t SURF_FLAG_VISIBLE    = FieldWmCompositor::SURF_FLAG_VISIBLE;

inline ChromeSkin& chromeSkin = FieldWmChrome::chromeSkin;
inline ChromeHit&  hover      = FieldWmChrome::hover;
inline OpenMenu&   openMenu   = FieldWmChrome::openMenu;
inline int&        menuItemHover = FieldWmChrome::menuItemHover;
inline int&        pendingMenuAction = FieldWmInput::pendingMenuAction;
inline bool&       closeRequested = FieldWmInput::closeRequested;
inline bool&       dragging    = FieldWmInput::dragging;
inline bool&       resizing    = FieldWmInput::resizing;
inline ChromeHit&  resizeEdge  = FieldWmInput::resizeEdge;
inline float&      panelScale  = FieldWmInput::panelScale;
inline std::uint8_t& stackRevision = FieldWmCore::stackRevision;
inline SurfaceSlot (&surfaces)[MAX_SURFACES] = FieldWmCore::surfaces;

inline float wmUiScale() noexcept { return FieldWmChrome::wmUiScale(); }
inline float shaderTitleH() noexcept { return FieldWmChrome::shaderTitleH(); }
inline float scaledTitleH() noexcept { return FieldWmChrome::scaledTitleH(); }
inline float scaledGrip() noexcept { return FieldWmChrome::scaledGrip(); }
inline float scaledBtnW() noexcept { return FieldWmChrome::scaledBtnW(); }
inline float scaledMenuBtnW() noexcept { return FieldWmChrome::scaledMenuBtnW(); }
inline float scaledMenuSpacing() noexcept { return FieldWmChrome::scaledMenuSpacing(); }
inline float scaledFileW() noexcept { return FieldWmChrome::scaledMenuBtnW(); }
inline float scaledMenuDropH() noexcept { return FieldWmChrome::scaledMenuDropH(); }

inline float menuBtnX0(const FieldDosViewport::Rect& win, int idx) noexcept {
    return FieldWmChrome::menuBtnX0(win, idx);
}
inline int menuItemCount(OpenMenu m) noexcept { return FieldWmChrome::menuItemCount(m); }
inline int menuItemAction(OpenMenu m, int idx) noexcept {
    return FieldWmChrome::menuItemAction(m, idx);
}

inline void syncViewport() noexcept {
    FieldWmChrome::syncViewport(FieldWmInput::panelScale);
}
inline FieldDosViewport::Rect windowRect() noexcept { return FieldWmChrome::windowRect(); }
inline float titleBarBottom(const FieldDosViewport::Rect& win) noexcept {
    return FieldWmChrome::titleBarBottom(win);
}
inline int focusedStackIndex() noexcept { return FieldWmCore::focusedStackIndex(); }
inline void rebuildSurfaceStack() noexcept {
    FieldWmCore::rebuildSurfaceStack(
        FieldWmInput::panelScale,
        FieldDosViewport::panelOx,
        FieldDosViewport::panelOy);
}
inline void raiseFocusedProgram() noexcept { FieldWmCore::raiseFocusedProgram(); }
inline void focusTitleBar() noexcept { FieldWmCore::raiseFocusedProgram(); }
inline bool wmPanelActive() noexcept { return FieldWmChrome::wmPanelActive(); }
inline ChromeHit hitTest(float mx, float my) noexcept { return FieldWmChrome::hitTest(mx, my); }
inline void applyPanelScale() noexcept { FieldWmInput::applyPanelScale(); }
inline void resetScale() noexcept { FieldWmInput::resetScale(); }
inline void closeWindow() noexcept { FieldWmInput::closeWindow(); }
inline void maximizeFocusedWindow() noexcept { FieldWmInput::maximizeFocusedWindow(); }

inline bool onMouseDown(SDL_Window* w, float lx, float ly, Uint8 clicks) noexcept {
    return FieldWmInput::onMouseDown(w, lx, ly, clicks);
}
inline void onMouseMotion(SDL_Window* w, float lx, float ly) noexcept {
    FieldWmInput::onMouseMotion(w, lx, ly);
}
inline void onMouseUp() noexcept { FieldWmInput::onMouseUp(); }
inline void onMouseWheel(float wheelY) noexcept { FieldWmInput::onMouseWheel(wheelY); }
inline int hitTestProgramStack(float mx, float my, bool skipFocused = true) noexcept {
    return FieldWmCore::hitTestProgramStack(mx, my, skipFocused);
}

inline void writeRamU16(std::uint8_t* ram, std::uint32_t off, std::uint16_t v) noexcept {
    FieldWmCompositor::writeRamU16(ram, off, v);
}
inline void packSurfaceRam(std::uint8_t* ram) noexcept {
    FieldWmCompositor::packSurfaceRam(ram);
}
inline void packCompositorBus(std::uint32_t* bus) noexcept {
    FieldWmCompositor::packCompositorBus(bus);
}
inline void packIntoBus(std::uint32_t* bus) noexcept {
    FieldWmCompositor::packIntoBus(bus);
}

} // namespace FieldAmouranthWm