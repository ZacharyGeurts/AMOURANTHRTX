#pragma once

// Field WinApp — program window bootstrap under GPU SDF chrome (FieldWmShell sizing).
// Widgets: FieldRtxWidgets.hpp (separate layer).

#include "OptionsMenu.hpp"
#include "FieldRtxMouse.hpp"

namespace FieldAmouranthOs {
bool shellChromeActive() noexcept;
extern bool panelVisible;
}
#include "FieldRtxThemes.hpp"
#include "FieldRtxVgaText.hpp"
#include "FieldRtxWidgets.hpp"
#include "FieldWinFrame.hpp"

#include <cstdio>

namespace FieldWinApp {

inline FieldWinFrame::Options opt;
inline FieldWinFrame::Layout layout;
inline FieldWinFrame::MenuBarState menuSt;
inline const FieldRtxGui::Menu* menus = FieldWinFrame::kStdMenus;
inline int menuCount = FieldWinFrame::kStdMenuCount;

inline bool useGpuChrome() noexcept {
    return FieldAmouranthOs::shellChromeActive()
        && FieldAmouranthOs::panelVisible
        && Options::AmouranthOs::EnableTaskbar;
}

inline void begin(std::uint8_t* ram, const char* title,
                  bool logPanel = false, const char* status = nullptr) noexcept {
    if (!ram) return;
    FieldRtxVgaText::initMonaco(ram);
    FieldRtxThemes::applyIndex(FieldRtxThemes::activeIndex);
    const bool gpu = useGpuChrome();
    opt.toolbar = !gpu;
    opt.statusBar = !gpu;
    opt.logPanel = gpu ? false : logPanel;
    opt.logCols = 22;
    opt.hScrollRows = 0;
    layout = FieldWinFrame::computeLayout(opt);
    if (gpu) {
        const int cols = FieldRtxVgaText::cols();
        const int rows = FieldRtxVgaText::rows();
        layout.clientR0 = 0;
        layout.clientC0 = 0;
        layout.clientR1 = rows;
        layout.clientC1 = cols;
        layout.clientRows = rows;
        layout.clientCols = cols;
    }
    if (!gpu) {
        FieldWinFrame::clearScreen(ram, 0x07u);
        FieldWinFrame::paintToolbar(ram, layout, title);
        FieldWinFrame::paintMenuBar(ram, layout, menus,
            menuCount, menuSt, nullptr);
        if (status)
            FieldWinFrame::paintStatus(ram, layout, status);
    } else {
        FieldRtxWidgets::clearRam(ram);
        FieldWinFrame::paintClientClear(ram, layout, 0x07u);
    }
}

inline void repaintChrome(std::uint8_t* ram, const char* title,
                          const char* status = nullptr) noexcept {
    if (!ram || useGpuChrome()) return;
    layout = FieldWinFrame::computeLayout(opt);
    FieldWinFrame::paintToolbar(ram, layout, title);
    FieldWinFrame::paintMenuBar(ram, layout, menus,
        menuCount, menuSt, nullptr);
    if (status)
        FieldWinFrame::paintStatus(ram, layout, status);
}

inline bool pumpMouse(std::uint8_t* ram, int& outAction) noexcept {
    if (!ram) return false;
    const FieldRtxMouse::Frame m = FieldRtxMouse::capture();
    if (!m.visible) return false;
    layout = FieldWinFrame::computeLayout(opt);
    int action = 0;
    if (FieldWinFrame::pumpMouse(ram, layout, m.col, m.row, m.leftClick,
            menus, menuCount, menuSt, action)) {
        outAction = action;
        repaintChrome(ram, "", nullptr);
        return true;
    }
    if (m.leftClick || m.rightClick)
        FieldRtxMouse::paintPointer(ram, m.col, m.row);
    return false;
}

inline void useStdMenus() noexcept {
    menus = FieldWinFrame::kStdMenus;
    menuCount = FieldWinFrame::kStdMenuCount;
}

inline void useEmuMenus() noexcept {
    menus = FieldWinFrame::kEmuMenus;
    menuCount = FieldWinFrame::kEmuMenuCount;
}

inline void useEmuMenusNes() noexcept {
    menus = nullptr;
    menuCount = 0;
}

inline void beginEmu(std::uint8_t* ram, const char* title, const char* status = nullptr,
                     bool nesControls = false) noexcept {
    if (nesControls) useEmuMenusNes();
    else useEmuMenus();
    begin(ram, title, false, status);
}

inline void beginEmuMenus(std::uint8_t* ram, const char* title, const char* status,
                          const FieldRtxGui::Menu* m, int count) noexcept {
    menus = m;
    menuCount = count;
    begin(ram, title, false, status);
}

inline bool pumpMenuKey(std::uint16_t key, int& outAction) noexcept {
    return FieldWinFrame::pumpMenuKey(key, menus, menuCount, menuSt, outAction);
}

inline void reset() noexcept {
    menuSt = {};
    opt = {};
    useStdMenus();
}

} // namespace FieldWinApp