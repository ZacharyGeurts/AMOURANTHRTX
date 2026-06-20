#pragma once

// Field WinFrame — consistent File/Edit/View/Help chrome, scrollbars, log minimap panel.

#include "FieldRtxGui.hpp"
#include "FieldRtxVgaText.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace FieldWinFrame {

constexpr int SCROLL_COLS = 2;
constexpr int MIN_LOG_COLS  = 18;
constexpr int TOOLBAR_ROWS  = 1;

struct Options {
    bool toolbar     = true;
    bool statusBar   = true;
    bool logPanel    = false;
    int  logCols     = 22;
    int  hScrollRows = 1;
};

struct Layout {
    int toolbarRow = 0;
    int menuRow    = 1;
    int statusRow  = 0;
    int clientR0   = 2;
    int clientC0  = 0;
    int clientR1  = 0;
    int clientC1  = 0;
    int logC0     = 0;
    int logC1     = 0;
    int vScrollC0 = 0;
    int vScrollC1 = 0;
    int hScrollR0 = 0;
    int clientRows = 0;
    int clientCols = 0;
};

struct ScrollState {
    int topLine    = 0;
    int hScroll    = 0;
    int totalLines = 0;
    int maxCol     = 0;
};

struct MenuBarState : FieldRtxGui::MenuBarState {
    int menuCols[8]{};
};

inline void clearScreen(std::uint8_t* ram, std::uint8_t attr = 0x0Fu) noexcept {
    if (!ram) return;
    const int cols = FieldRtxVgaText::cols();
    const int rows = FieldRtxVgaText::rows();
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < cols; ++c)
            FieldRtxGui::put(ram, r, c, ' ', attr);
}

inline Layout computeLayout(const Options& opt) noexcept {
    Layout L{};
    const int cols = FieldRtxVgaText::cols();
    const int rows = FieldRtxVgaText::rows();
    L.toolbarRow = opt.toolbar ? 0 : -1;
    L.menuRow = opt.toolbar ? TOOLBAR_ROWS : 0;
    L.statusRow = opt.statusBar ? rows - 1 : -1;
    L.clientR0 = L.menuRow + 1;
    L.clientC0 = 0;
    L.clientR1 = L.statusRow >= 0 ? L.statusRow - opt.hScrollRows : rows - opt.hScrollRows;
    L.hScrollR0 = L.clientR1;
    int right = cols;
    if (opt.logPanel) {
        const int lc = std::clamp(opt.logCols, MIN_LOG_COLS, cols / 3);
        L.logC1 = right;
        right -= lc;
        L.logC0 = right;
    }
    L.vScrollC1 = right;
    right -= SCROLL_COLS;
    L.vScrollC0 = right;
    L.clientC1 = right;
    L.clientRows = std::max(1, L.clientR1 - L.clientR0);
    L.clientCols = std::max(1, L.clientC1 - L.clientC0);
    return L;
}

inline void paintVScroll(std::uint8_t* ram, const Layout& L, const ScrollState& sc,
                         std::uint8_t trackAttr = 0x08u,
                         std::uint8_t thumbAttr = 0x9Fu) noexcept {
    if (L.vScrollC0 >= L.vScrollC1) return;
    const int span = std::max(1, sc.totalLines);
    const int vis  = std::max(1, L.clientRows);
    const int thumbH = std::max(1, vis * L.clientRows / span);
    const int travel = std::max(1, L.clientRows - thumbH);
    const int thumbY = (span <= vis) ? 0
        : sc.topLine * travel / std::max(1, span - vis);
    for (int r = L.clientR0; r < L.clientR1; ++r) {
        for (int c = L.vScrollC0; c < L.vScrollC1; ++c) {
            const bool thumb = (r - L.clientR0) >= thumbY && (r - L.clientR0) < thumbY + thumbH;
            FieldRtxGui::put(ram, r, c, thumb ? '\xDB' : '\xB0', thumb ? thumbAttr : trackAttr);
        }
    }
}

inline void paintHScroll(std::uint8_t* ram, const Layout& L, const ScrollState& sc,
                         int hScrollRows = 1,
                         std::uint8_t trackAttr = 0x08u,
                         std::uint8_t thumbAttr = 0x9Fu) noexcept {
    if (L.hScrollR0 < 0 || hScrollRows <= 0) return;
    const int maxC = std::max(1, sc.maxCol);
    const int visC = std::max(1, L.clientCols);
    const int thumbW = std::max(2, visC * L.clientCols / maxC);
    const int travel = std::max(1, L.clientCols - thumbW);
    const int thumbX = sc.hScroll * travel / std::max(1, maxC - visC);
    FieldRtxGui::fill(ram, L.hScrollR0, ' ', trackAttr);
    for (int c = L.clientC0; c < L.clientC1; ++c) {
        const bool thumb = (c - L.clientC0) >= thumbX && (c - L.clientC0) < thumbX + thumbW;
        FieldRtxGui::put(ram, L.hScrollR0, c, thumb ? '\xDB' : '\xB0', thumb ? thumbAttr : trackAttr);
    }
}

inline void paintLogPanel(std::uint8_t* ram, const Layout& L,
                          const std::vector<std::string>& lines, int logScroll,
                          const char* header = " Log ",
                          std::uint8_t frameAttr = 0x18u,
                          std::uint8_t textAttr  = 0x08u) noexcept {
    if (L.logC0 >= L.logC1) return;
    const int w = L.logC1 - L.logC0;
    FieldRtxGui::drawFrame(ram, L.clientR0, L.logC0, L.clientR1 - 1, L.logC1 - 1,
        frameAttr, header);
    const int innerRows = L.clientRows - 2;
    const int total = static_cast<int>(lines.size());
    logScroll = std::clamp(logScroll, 0, std::max(0, total - innerRows));
    for (int vis = 0; vis < innerRows; ++vis) {
        const int row = L.clientR0 + 1 + vis;
        const int di = logScroll + vis;
        FieldRtxGui::fill(ram, row, ' ', textAttr);
        if (di >= 0 && di < total) {
            const std::string& ln = lines[static_cast<std::size_t>(di)];
            std::uint8_t attr = textAttr;
            if (ln.size() > 4 && ln[0] == '>' && ln[1] == ' ') attr = 0x1Eu;
            else if (ln.find("error") != std::string::npos
                  || ln.find("Error") != std::string::npos) attr = 0x4Cu;
            FieldRtxGui::text(ram, row, L.logC0 + 1, ln.c_str(), attr, w - 2);
        }
    }
    if (total > innerRows) {
        char hint[32];
        std::snprintf(hint, sizeof hint, " %d/%d ", logScroll + 1, total);
        FieldRtxGui::text(ram, L.clientR1 - 1, L.logC0 + 1, hint, 0x17u, w - 2);
    }
}

inline void paintLogMinimap(std::uint8_t* ram, const Layout& L,
                            const std::vector<std::string>& lines, int viewTop,
                            std::uint8_t dimAttr = 0x08u,
                            std::uint8_t hiAttr  = 0x1Eu) noexcept {
    if (L.logC0 >= L.logC1) return;
    const int h = L.clientRows;
    const int total = static_cast<int>(lines.size());
    for (int vis = 0; vis < h; ++vis) {
        const int row = L.clientR0 + vis;
        const float frac = static_cast<float>(vis) / std::max(1, h - 1);
        const int doc = static_cast<int>(frac * std::max(0, total - 1));
        char ch = ' ';
        std::uint8_t attr = dimAttr;
        if (doc >= 0 && doc < total) {
            const std::string& ln = lines[static_cast<std::size_t>(doc)];
            ch = ln.empty() ? '.' : ln[0];
            if (ln.size() > 2 && ln[0] == '>' && ln[1] == ' ') ch = '\xFE';
            if (doc >= viewTop && doc < viewTop + h) attr = hiAttr;
        }
        for (int c = L.logC0; c < L.logC1; ++c)
            FieldRtxGui::put(ram, row, c, ch, attr);
    }
}

inline void paintToolbar(std::uint8_t* ram, const Layout& L, const char* title,
                         std::uint8_t attr = 0x1Fu) noexcept {
    if (L.toolbarRow < 0) return;
    const int cols = FieldRtxVgaText::cols();
    FieldRtxGui::fill(ram, L.toolbarRow, ' ', attr);
    FieldRtxGui::put(ram, L.toolbarRow, 0, FieldRtxGui::ICO_BLOCK, FieldRtxGui::ATTR_RTX);
    if (title && title[0])
        FieldRtxGui::text(ram, L.toolbarRow, 2, title, FieldRtxGui::ATTR_TITLE, cols - 4);
    FieldRtxGui::text(ram, L.toolbarRow, cols - 14, " _ □ X ", FieldRtxGui::ATTR_DIM, 13);
}

inline void paintMenuBar(std::uint8_t* ram, const Layout& L,
                         const FieldRtxGui::Menu* menus, int menuCount,
                         MenuBarState& st, const char* rightTitle) noexcept {
    const int cols = FieldRtxVgaText::cols();
    FieldRtxGui::fill(ram, L.menuRow, ' ', FieldRtxGui::ATTR_MENU);
    FieldRtxGui::put(ram, L.menuRow, 0, FieldRtxGui::ICO_BLOCK, FieldRtxGui::ATTR_RTX);
    int col = 2;
    for (int m = 0; m < menuCount; ++m) {
        if (m < 8) st.menuCols[m] = col;
        const bool hi = st.openMenu == m || st.active == m + 1;
        char label[28];
        std::snprintf(label, sizeof label, " %c %s ", menus[m].icon, menus[m].title);
        FieldRtxGui::text(ram, L.menuRow, col, label,
            hi ? FieldRtxGui::ATTR_MENU_HI : FieldRtxGui::ATTR_MENU, 24);
        col += static_cast<int>(std::strlen(label));
    }
    if (rightTitle && rightTitle[0]) {
        const int len = static_cast<int>(std::strlen(rightTitle));
        FieldRtxGui::text(ram, L.menuRow, std::max(0, cols - len - 1),
            rightTitle, FieldRtxGui::ATTR_GOLD, len + 1);
    }
    if (st.openMenu >= 0 && st.openMenu < menuCount)
        FieldRtxGui::paintDropDown(ram, L.menuRow + 1, st.menuCols[st.openMenu],
            menus[st.openMenu], st.selItem);
}

inline int menuHitRow(const Layout& L) noexcept { return L.menuRow; }

inline int dropdownBaseRow(const Layout& L) noexcept { return L.menuRow + 1; }

inline bool pumpMouse(std::uint8_t* ram, const Layout& L, int col, int row, bool leftClick,
                      const FieldRtxGui::Menu* menus, int menuCount,
                      MenuBarState& st, int& outAction) noexcept {
    outAction = 0;
    if (!ram || !leftClick) return false;
    if (row == menuHitRow(L)) {
        const int hit = FieldRtxGui::hitMenuBar(st.menuCols, menuCount, col, row);
        if (hit >= 0) {
            st.openMenu = (st.openMenu == hit) ? -1 : hit;
            st.selItem = 0;
            st.active = st.openMenu >= 0 ? 1 : 0;
            return true;
        }
    }
    if (st.openMenu >= 0 && st.openMenu < menuCount) {
        const int item = FieldRtxGui::hitDropDown(menus[st.openMenu],
            st.menuCols[st.openMenu], dropdownBaseRow(L), col, row);
        if (item >= 0) {
            outAction = menus[st.openMenu].items[item].actionId;
            st.openMenu = -1;
            st.active = 0;
            return true;
        }
        st.openMenu = -1;
        st.active = 0;
        return true;
    }
    return false;
}

inline void paintStatus(std::uint8_t* ram, const Layout& L, const char* text,
                        std::uint8_t attr = 0x70u) noexcept {
    if (L.statusRow < 0 || !text) return;
    FieldRtxGui::fill(ram, L.statusRow, ' ', attr);
    FieldRtxGui::text(ram, L.statusRow, 0, text, attr, FieldRtxVgaText::cols() - 1);
}

inline void paintClientClear(std::uint8_t* ram, const Layout& L,
                             std::uint8_t attr = 0x0Fu) noexcept {
    for (int r = L.clientR0; r < L.clientR1; ++r)
        for (int c = L.clientC0; c < L.clientC1; ++c)
            FieldRtxGui::put(ram, r, c, ' ', attr);
}

inline void paintChrome(std::uint8_t* ram, const Layout& L, const Options& opt,
                        const ScrollState& sc,
                        const FieldRtxGui::Menu* menus, int menuCount,
                        MenuBarState& st, const char* toolbarTitle,
                        const char* menuTitle, const char* status,
                        const std::vector<std::string>* logLines = nullptr,
                        int logScroll = 0) noexcept {
    if (opt.toolbar)
        paintToolbar(ram, L, toolbarTitle ? toolbarTitle : menuTitle);
    paintMenuBar(ram, L, menus, menuCount, st, menuTitle);
    paintVScroll(ram, L, sc);
    if (opt.hScrollRows > 0) paintHScroll(ram, L, sc, opt.hScrollRows);
    if (opt.logPanel && logLines)
        paintLogPanel(ram, L, *logLines, logScroll);
    if (status) paintStatus(ram, L, status);
}

inline bool pumpMenuKey(std::uint16_t key, const FieldRtxGui::Menu* menus, int menuCount,
                        MenuBarState& st, int& outAction) noexcept {
    outAction = 0;
    const std::uint8_t ch = static_cast<std::uint8_t>(key & 0xFFu);
    if (key == 0x3F00u) { st.openMenu = -1; st.active = 0; return false; }
    if (key == 0x3D00u) {
        st.active = (st.active == 0) ? 1 : 0;
        st.openMenu = st.active ? 0 : -1;
        st.selItem = 0;
        return true;
    }
    if (st.openMenu >= 0) {
        if (key == 0x4800u) { if (st.selItem > 0) --st.selItem; return true; }
        if (key == 0x5000u) {
            if (st.selItem < menus[st.openMenu].itemCount - 1) ++st.selItem;
            return true;
        }
        if (key == 0x4B00u) {
            if (st.openMenu > 0) { --st.openMenu; st.selItem = 0; }
            return true;
        }
        if (key == 0x4D00u) {
            if (st.openMenu < menuCount - 1) { ++st.openMenu; st.selItem = 0; }
            return true;
        }
        if (key == 0x011Bu) { st.openMenu = -1; st.active = 0; return true; }
        if (key == 0x1C0Du) {
            outAction = menus[st.openMenu].items[st.selItem].actionId;
            st.openMenu = -1;
            return true;
        }
        if (FieldRtxGui::matchMenuHotkey(menus[st.openMenu], static_cast<char>(ch), outAction)) {
            st.openMenu = -1;
            return true;
        }
    }
    char alt = 0;
    if (FieldRtxGui::isAltMenuKey(key, alt)) {
        for (int m = 0; m < menuCount; ++m) {
            if (menus[m].title[0]
                && std::toupper(static_cast<unsigned char>(menus[m].title[0]))
                   == std::toupper(static_cast<unsigned char>(alt))) {
                st.openMenu = m;
                st.selItem = 0;
                return true;
            }
        }
    }
    return false;
}

// Standard menu templates (apps wire action IDs)

inline const FieldRtxGui::MenuItem kFileCommon[] = {
    {FieldRtxGui::ICO_FOLDER, "New",        'N', 101},
    {FieldRtxGui::ICO_FOLDER, "Open...",    'O', 102},
    {FieldRtxGui::ICO_DISK,  "Save",       'S', 103},
    {FieldRtxGui::ICO_DISK,  "Save As...", 'A', 104},
    {FieldRtxGui::ICO_STOP,  "Exit",       'X', 109},
};
inline const FieldRtxGui::MenuItem kEditCommon[] = {
    {FieldRtxGui::BOX_X, "Cut",         'T', 201},
    {FieldRtxGui::BOX_X, "Copy",        'C', 202},
    {FieldRtxGui::BOX_X, "Paste",       'P', 203},
    {FieldRtxGui::BOX_X, "Find...",     'F', 204},
    {FieldRtxGui::BOX_X, "Select All",  'A', 205},
};
inline const FieldRtxGui::MenuItem kViewCommon[] = {
    {FieldRtxGui::BOX_X, "Scroll Top",    'T', 301},
    {FieldRtxGui::BOX_X, "Scroll Bottom", 'B', 302},
    {FieldRtxGui::BOX_X, "Log Panel",     'L', 303},
    {FieldRtxGui::ICO_BOOK, "Themes...", 'H', 304},
};
inline const FieldRtxGui::MenuItem kHelpCommon[] = {
    {FieldRtxGui::ICO_BOOK, "Help Topics", 'H', 401},
    {FieldRtxGui::ICO_BLOCK,"About",       'A', 402},
};

inline const FieldRtxGui::MenuItem kOptionsEmu[] = {
    {FieldRtxGui::BOX_X, "Settings...",  'S', 501},
    {FieldRtxGui::BOX_X, "Controls...",  'C', 502},
};
inline const FieldRtxGui::MenuItem kOptionsEmuSimple[] = {
    {FieldRtxGui::BOX_X, "Settings...",  'S', 501},
};

constexpr int A_FILE_OPEN        = 102;
constexpr int A_FILE_EXIT        = 109;
constexpr int A_FILE_RECENT_BASE = 110;  // 110..117 recent ROM slots
constexpr int A_FILE_RECENT_MAX  = 8;
constexpr int A_FILE_CLEAR_RECENT = 119;

constexpr int A_EMU_PAUSE      = 201;
constexpr int A_EMU_RESUME     = 202;
constexpr int A_EMU_RESET      = 203;
constexpr int A_EMU_FRAME      = 204;
constexpr int A_EMU_TURBO      = 205;
constexpr int A_EMU_UNLIMITED  = 206;
constexpr int A_EMU_MUTE       = 207;

constexpr int A_OPT_SETTINGS = 501;
constexpr int A_OPT_CONTROLS = 502;

constexpr int A_HELP_ABOUT = 602;

inline const FieldRtxGui::Menu kStdMenus[] = {
    {FieldRtxGui::ICO_FOLDER, "File", kFileCommon,  5},
    {FieldRtxGui::BOX_X,      "Edit", kEditCommon,  5},
    {FieldRtxGui::ICO_BOOK,   "View", kViewCommon,  4},
    {FieldRtxGui::ICO_BOOK,   "Help", kHelpCommon,  2},
};
constexpr int kStdMenuCount = 4;

inline const FieldRtxGui::Menu kEmuMenus[] = {
    {FieldRtxGui::ICO_FOLDER, "File",    kFileCommon,       5},
    {FieldRtxGui::BOX_X,      "Edit",    kEditCommon,       5},
    {FieldRtxGui::ICO_BOOK,   "View",    kViewCommon,       4},
    {FieldRtxGui::BOX_X,      "Options", kOptionsEmuSimple, 1},
    {FieldRtxGui::ICO_BOOK,   "Help",    kHelpCommon,       2},
};
constexpr int kEmuMenuCount = 5;

// AmmoNES builds menus at runtime (File / Recent / Emulation / Options / Help).
inline const FieldRtxGui::MenuItem kNesFileStatic[] = {
    {FieldRtxGui::ICO_FOLDER, "Open...", 'O', A_FILE_OPEN},
    {FieldRtxGui::ICO_STOP,  "Exit",    'X', A_FILE_EXIT},
};
inline const FieldRtxGui::MenuItem kNesEmuStatic[] = {
    {FieldRtxGui::BOX_X, "Pause",            'P', A_EMU_PAUSE},
    {FieldRtxGui::BOX_X, "Resume",           'R', A_EMU_RESUME},
    {FieldRtxGui::BOX_X, "Reset",            'T', A_EMU_RESET},
    {FieldRtxGui::BOX_X, "Frame Advance",    'F', A_EMU_FRAME},
    {FieldRtxGui::BOX_X, "Turbo",            'U', A_EMU_TURBO},
    {FieldRtxGui::BOX_X, "Unlimited Speed",  'L', A_EMU_UNLIMITED},
    {FieldRtxGui::BOX_X, "Mute Sound",       'M', A_EMU_MUTE},
};
inline const FieldRtxGui::MenuItem kNesHelpStatic[] = {
    {FieldRtxGui::ICO_BLOCK, "About AmmoNES", 'A', A_HELP_ABOUT},
};
constexpr int kNesFileStaticCount = 2;
constexpr int kNesEmuStaticCount  = 7;
constexpr int kNesHelpStaticCount = 1;

} // namespace FieldWinFrame