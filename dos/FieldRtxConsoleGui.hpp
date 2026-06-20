#pragma once

// RTX Shell GUI — File/Edit/View/Help chrome, terminal client pane, log minimap on right.

#include "FieldAmouranthOs.hpp"
#include "FieldDosViewport.hpp"
#include "OptionsMenu.hpp"
#include "FieldRtxGui.hpp"
#include "FieldRtxTerm.hpp"
#include "FieldRtxThemePicker.hpp"
#include "FieldRtxThemes.hpp"
#include "FieldRtxVgaText.hpp"
#include "FieldRuntimeInfo.hpp"
#include "FieldRtxMouse.hpp"
#include "FieldWinFrame.hpp"

#include <cstdio>
#include <cstring>
#include <string>

namespace FieldRtxConsoleGui {

inline bool active = false;
inline bool pendingHelp = false;
inline bool pendingNewSession = false;
inline FieldWinFrame::Options opt;
inline FieldWinFrame::Layout layout;
inline FieldWinFrame::MenuBarState menuSt;
inline int logScroll = 0;
inline bool logExpanded = false;

inline const FieldRtxGui::MenuItem kFileShell[] = {
    {FieldRtxGui::ICO_FOLDER, "New Session",  'N', 101},
    {FieldRtxGui::ICO_DISK,  "Save Log...",  'S', 103},
    {FieldRtxGui::ICO_STOP,  "Exit Shell",   'X', 109},
};
inline const FieldRtxGui::MenuItem kEditShell[] = {
    {FieldRtxGui::BOX_X, "Copy",        'C', 201},
    {FieldRtxGui::BOX_X, "Clear Log",   'L', 206},
    {FieldRtxGui::BOX_X, "Select All",  'A', 205},
};
inline const FieldRtxGui::MenuItem kViewShell[] = {
    {FieldRtxGui::BOX_X, "Scroll Top",    'T', 301},
    {FieldRtxGui::BOX_X, "Scroll Bottom", 'B', 302},
    {FieldRtxGui::BOX_X, "Log Panel",     'L', 303},
    {FieldRtxGui::ICO_BOOK, "Themes...", 'H', 304},
};
inline const FieldRtxGui::MenuItem kHelpShell[] = {
    {FieldRtxGui::ICO_BOOK, "Help Topics", 'H', 401},
    {FieldRtxGui::ICO_BLOCK,"About RTX",   'A', 402},
};
inline const FieldRtxGui::Menu kShellMenus[] = {
    {FieldRtxGui::ICO_FOLDER, "File", kFileShell,  3},
    {FieldRtxGui::BOX_X,      "Edit", kEditShell,  3},
    {FieldRtxGui::ICO_BOOK,   "View", kViewShell,  4},
    {FieldRtxGui::ICO_BOOK,   "Help", kHelpShell,  2},
};
constexpr int kShellMenuCount = 4;

inline bool useGpuChrome() noexcept {
    return FieldAmouranthOs::shellChromeActive()
        && Options::AmouranthOs::EnableTaskbar;
}

inline void clearVgaChromeRows(std::uint8_t* ram) noexcept {
    if (!ram) return;
    constexpr std::uint32_t vga = 0x000B8000u;
    for (int row = 0; row < 4; ++row) {
        for (int col = 0; col < 80; ++col) {
            const std::uint32_t off = vga + static_cast<std::uint32_t>((row * 80 + col) * 2);
            ram[off] = ' ';
            ram[off + 1u] = 0x07u;
        }
    }
}

inline void syncLayout() noexcept {
    if (useGpuChrome()) {
        opt.toolbar = false;
        opt.statusBar = false;
        opt.logPanel = false;
        opt.hScrollRows = 0;
    }
    layout = FieldWinFrame::computeLayout(opt);
    if (useGpuChrome()) {
        const int cols = FieldRtxVgaText::cols();
        const int rows = FieldRtxVgaText::rows();
        layout.clientR0 = 0;
        layout.clientC0 = 0;
        layout.clientR1 = rows;
        layout.clientC1 = cols;
        layout.clientRows = rows;
        layout.clientCols = cols;
    }
    FieldRtxTerm::setClientRect(layout.clientC0, layout.clientR0,
        layout.clientCols, layout.clientRows);
}

inline FieldWinFrame::ScrollState scrollState() noexcept {
    FieldWinFrame::ScrollState sc{};
    sc.topLine = FieldRtxTerm::scrollOffset;
    sc.totalLines = FieldRtxTerm::totalLines();
    sc.hScroll = 0;
    sc.maxCol = FieldRtxTerm::screenCols();
    return sc;
}

inline void paintChrome(std::uint8_t* ram) noexcept {
    if (!ram || useGpuChrome()) return;
    syncLayout();
    const FieldWinFrame::ScrollState sc = scrollState();
    char title[96];
    title[0] = '\0';
    std::strncpy(title, " RTX Shell — ", sizeof title - 1);
    std::strncat(title, FieldRuntimeInfo::masterStatusLine(), sizeof title - std::strlen(title) - 2);
    std::strncat(title, " ", sizeof title - std::strlen(title) - 1);
    char status[160];
    std::snprintf(status, sizeof status,
        " PgUp/Dn scroll | Wheel | %d lines | Log %zu | F1 help | Alt+F menus ",
        FieldRtxTerm::totalLines(), FieldRtxTerm::commandLog.size());
    FieldWinFrame::paintToolbar(ram, layout, title);
    FieldWinFrame::paintMenuBar(ram, layout, kShellMenus, kShellMenuCount, menuSt, nullptr);
    FieldWinFrame::paintVScroll(ram, layout, sc);
    if (opt.logPanel) {
        if (logExpanded)
            FieldWinFrame::paintLogPanel(ram, layout, FieldRtxTerm::commandLog, logScroll,
                " Command Log ");
        else
            FieldWinFrame::paintLogMinimap(ram, layout, FieldRtxTerm::commandLog,
                FieldRtxTerm::scrollOffset);
    }
    FieldWinFrame::paintStatus(ram, layout, status);
}

inline void paint(std::uint8_t* ram) noexcept {
    if (!ram) return;
    FieldRtxVgaText::initMonaco(ram);
    FieldRtxThemes::applyIndex(FieldRtxThemes::activeIndex);
    if (useGpuChrome())
        clearVgaChromeRows(ram);
    syncLayout();
    FieldWinFrame::paintClientClear(ram, layout, 0x02u);
    paintChrome(ram);
    FieldRtxTerm::applyView(ram);
    active = true;
}

inline void open(std::uint8_t* ram) noexcept {
    if (!ram) return;
    opt.toolbar = true;
    opt.statusBar = true;
    opt.logPanel = true;
    opt.logCols = 24;
    opt.hScrollRows = 0;
    logScroll = 0;
    logExpanded = false;
    menuSt = {};
    pendingHelp = false;
    pendingNewSession = false;
    FieldRtxTerm::history.clear();
    FieldRtxTerm::commandLog.clear();
    FieldRtxTerm::scrollOffset = 0;
    FieldRtxTerm::liveDirty = true;
    FieldRtxThemes::applyIndex(3);
    FieldDosViewport::crispFont = true;
    FieldDosViewport::sharpen = 0.72f;
    FieldDosViewport::fontScale = 1.35f;
    paint(ram);
}

inline void handleMenuAction(int action, std::uint8_t* ram) noexcept {
    if (!ram) return;
    switch (action) {
    case 101:
        pendingNewSession = true;
        break;
    case 103:
        FieldRtxTerm::appendLog(" (save log — use LOGSAVE shell command) ");
        break;
    case 109:
        if (FieldAmouranthOs::active)
            FieldAmouranthOs::removeTopProgram();
        break;
    case 206:
        FieldRtxTerm::commandLog.clear();
        logScroll = 0;
        break;
    case 301:
        FieldRtxTerm::scrollOffset = FieldRtxTerm::maxScroll();
        FieldRtxTerm::applyView(ram);
        break;
    case 302:
        FieldRtxTerm::pinLive();
        FieldRtxTerm::applyView(ram);
        break;
    case 303:
        logExpanded = !logExpanded;
        break;
    case 304:
        FieldRtxThemePicker::open();
        break;
    case 401:
        pendingHelp = true;
        break;
    case 402:
        FieldRtxTerm::appendLog(" RTX-DOS 7.0 — AmouranthOS Golden Era GUI ");
        break;
    default:
        break;
    }
    paintChrome(ram);
}

inline bool pumpMenuKey(std::uint16_t key, std::uint8_t* ram) noexcept {
    if (useGpuChrome()) return false;
    int action = 0;
    if (FieldWinFrame::pumpMenuKey(key, kShellMenus, kShellMenuCount, menuSt, action)) {
        if (action) handleMenuAction(action, ram);
        else paintChrome(ram);
        return true;
    }
    return false;
}

inline bool pumpKey(std::uint16_t key, std::uint8_t* ram) noexcept {
    if (!active || !ram) return false;
    if (pumpMenuKey(key, ram)) return true;
    if (key == 0x3B00u) {
        pendingHelp = true;
        return true;
    }
    return false;
}

inline void afterShellOutput(std::uint8_t* ram) noexcept {
    (void)ram;
}

inline void handleMouse(std::uint8_t* ram) noexcept {
    if (!active || !ram || useGpuChrome()) return;
    const FieldRtxMouse::Frame m = FieldRtxMouse::capture();
    if (!m.visible) return;
    int action = 0;
    if (FieldWinFrame::pumpMouse(ram, layout, m.col, m.row, m.leftClick,
            kShellMenus, kShellMenuCount, menuSt, action)) {
        if (action) handleMenuAction(action, ram);
        else paintChrome(ram);
        return;
    }
    if (m.leftClick || m.rightClick)
        FieldRtxMouse::paintPointer(ram, m.col, m.row);
}

inline void close() noexcept {
    active = false;
    pendingHelp = false;
    pendingNewSession = false;
}

} // namespace FieldRtxConsoleGui