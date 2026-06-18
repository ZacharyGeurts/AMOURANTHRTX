#pragma once

// Field WinApp — fast SDL3/Vulkan guest window bootstrap (toolbar + menus + mouse).

#include "OptionsMenu.hpp"
#include "FieldRtxMouse.hpp"

namespace FieldAmouranthOs {
bool shellChromeActive() noexcept;
extern bool panelVisible;
}
#include "FieldRtxThemes.hpp"
#include "FieldRtxVgaText.hpp"
#include "FieldWinFrame.hpp"

#include <cstdio>

namespace FieldWinApp {

inline FieldWinFrame::Options opt;
inline FieldWinFrame::Layout layout;
inline FieldWinFrame::MenuBarState menuSt;

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
    FieldWinFrame::clearScreen(ram, 0x0Fu);
    if (!gpu) {
        FieldWinFrame::paintToolbar(ram, layout, title);
        FieldWinFrame::paintMenuBar(ram, layout, FieldWinFrame::kStdMenus,
            FieldWinFrame::kStdMenuCount, menuSt, nullptr);
        if (status)
            FieldWinFrame::paintStatus(ram, layout, status);
    } else {
        FieldWinFrame::paintClientClear(ram, layout, FieldRtxGui::ATTR_EDITOR);
    }
}

inline void repaintChrome(std::uint8_t* ram, const char* title,
                          const char* status = nullptr) noexcept {
    if (!ram || useGpuChrome()) return;
    layout = FieldWinFrame::computeLayout(opt);
    FieldWinFrame::paintToolbar(ram, layout, title);
    FieldWinFrame::paintMenuBar(ram, layout, FieldWinFrame::kStdMenus,
        FieldWinFrame::kStdMenuCount, menuSt, nullptr);
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
            FieldWinFrame::kStdMenus, FieldWinFrame::kStdMenuCount, menuSt, action)) {
        outAction = action;
        repaintChrome(ram, "", nullptr);
        return true;
    }
    if (m.leftClick || m.rightClick)
        FieldRtxMouse::paintPointer(ram, m.col, m.row);
    return false;
}

inline void reset() noexcept {
    menuSt = {};
    opt = {};
}

} // namespace FieldWinApp