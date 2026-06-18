#pragma once

// Field Monaco — WinFrame chrome, line gutter, syntax, scrollbars, outline minimap.

#include "FieldAmmoVfs.hpp"
#include "FieldDosViewport.hpp"
#include "FieldRtxGui.hpp"
#include "FieldRtxThemePicker.hpp"
#include "FieldRtxThemes.hpp"
#include "FieldRtxVgaText.hpp"
#include "FieldRtxMouse.hpp"
#include "FieldWinFrame.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace FieldMonacoEdit {

constexpr int GUTTER_COLS = 6;

inline bool active = false;
inline bool dirty = false;
inline std::string path;
inline std::vector<std::string> lines;
inline int curRow = 0;
inline int curCol = 0;
inline int topLine = 0;
inline int hScroll = 0;
inline FieldRtxGui::SyntaxLang lang = FieldRtxGui::SyntaxLang::Auto;
inline FieldWinFrame::Options frameOpt;
inline FieldWinFrame::Layout layout;
inline FieldWinFrame::MenuBarState menuSt;
inline std::vector<std::string> outlineLog;

constexpr std::uint8_t MC_BG      = 0x00u;
constexpr std::uint8_t MC_TEXT    = 0x0Fu;
constexpr std::uint8_t MC_GUTTER  = 0x08u;
constexpr std::uint8_t MC_LINE    = 0x18u;
constexpr std::uint8_t MC_STATUS  = 0x70u;
constexpr std::uint8_t MC_CURSOR  = 0x9Fu;

inline int edRows() noexcept { return std::max(1, layout.clientRows); }
inline int edWidth() noexcept { return std::max(1, layout.clientCols - GUTTER_COLS); }

inline void syncLayout() noexcept {
    layout = FieldWinFrame::computeLayout(frameOpt);
}

inline void rebuildOutline() noexcept {
    outlineLog.clear();
    for (std::size_t i = 0; i < lines.size(); ++i) {
        const std::string& ln = lines[i];
        if (ln.empty()) continue;
        char ch = ln[0];
        if (ln.size() > 1 && ln[0] == '/' && ln[1] == '/') ch = ';';
        if (ln.size() > 0 && std::isalpha(static_cast<unsigned char>(ln[0]))) ch = '#';
        char buf[48];
        std::snprintf(buf, sizeof buf, "%c %3zu %s", ch,
            i + 1, ln.size() > 20 ? ln.substr(0, 20).c_str() : ln.c_str());
        outlineLog.emplace_back(buf);
        if (outlineLog.size() > 512u) break;
    }
}

inline void detectLang() noexcept {
    lang = FieldRtxGui::syntaxLangFromPath(path.c_str());
}

inline void applyMonacoTheme() noexcept {
    for (int i = 0; i < FieldRtxThemes::kPresetCount; ++i) {
        if (std::strcmp(FieldRtxThemes::kPresets[i].name, "Monaco Dark") == 0) {
            FieldRtxThemes::applyIndex(i);
            break;
        }
    }
    FieldRtxGui::ATTR_EDITOR   = MC_TEXT;
    FieldRtxGui::ATTR_KW       = 0x9Bu;
    FieldRtxGui::ATTR_STR      = 0x6Cu;
    FieldRtxGui::ATTR_COMMENT  = 0x18u;
    FieldRtxGui::ATTR_CURSOR   = MC_CURSOR;
    FieldDosViewport::fontScale = 1.35f;
    FieldDosViewport::crispFont = true;
    FieldDosViewport::subpixelFont = true;
}

inline bool loadFile(const char* p) {
    if (!p || !p[0]) return false;
    std::vector<std::uint8_t> data;
    if (!FieldAmmoVfs::readPath(p, data)) return false;
    lines.clear();
    std::string cur;
    for (std::uint8_t b : data) {
        if (b == '\r') continue;
        if (b == '\n') { lines.push_back(cur); cur.clear(); }
        else cur += static_cast<char>(b);
    }
    if (!cur.empty() || lines.empty()) lines.push_back(cur);
    path = p;
    dirty = false;
    curRow = curCol = topLine = hScroll = 0;
    detectLang();
    rebuildOutline();
    return true;
}

inline bool saveFile() {
    if (path.empty()) return false;
    std::string blob;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        blob += lines[i];
        if (i + 1 < lines.size()) blob += "\r\n";
    }
    if (!FieldAmmoVfs::writePath(path.c_str(),
            reinterpret_cast<const std::uint8_t*>(blob.data()), blob.size()))
        return false;
    dirty = false;
    return true;
}

inline int maxLineCol() noexcept {
    int m = 0;
    for (const std::string& ln : lines)
        m = std::max(m, static_cast<int>(ln.size()));
    return std::max(m, 1);
}

inline void paintGutter(std::uint8_t* ram, int visRow, int docRow) noexcept {
    const int c0 = layout.clientC0;
    for (int c = c0; c < c0 + GUTTER_COLS; ++c)
        FieldRtxGui::put(ram, visRow, c, ' ', MC_GUTTER);
    if (docRow < 0 || docRow >= static_cast<int>(lines.size())) return;
    char num[12];
    std::snprintf(num, sizeof num, "%4d", docRow + 1);
    FieldRtxGui::text(ram, visRow, c0, num, MC_LINE, GUTTER_COLS);
    FieldRtxGui::put(ram, visRow, c0 + GUTTER_COLS - 1, '|', MC_LINE);
}

inline void paintEditorLine(std::uint8_t* ram, int visRow, const std::string& ln,
                            bool isCur) noexcept {
    paintGutter(ram, visRow, topLine + (visRow - layout.clientR0));
    const int c0 = layout.clientC0 + GUTTER_COLS;
    const int c1 = layout.clientC1;
    for (int c = layout.clientC0; c < c1; ++c)
        FieldRtxGui::put(ram, visRow, c, ' ', MC_BG);
    const int w = std::min(edWidth(), c1 - c0);
    const int start = hScroll;
    for (int c = 0; c < w; ++c) {
        const int src = start + c;
        char ch = ' ';
        if (src < static_cast<int>(ln.size())) ch = ln[static_cast<std::size_t>(src)];
        std::uint8_t attr = MC_TEXT;
        if (src < static_cast<int>(ln.size()))
            attr = FieldRtxGui::syntaxAttrAt(ln, src, lang);
        if (isCur && src == curCol) attr = MC_CURSOR;
        FieldRtxGui::put(ram, visRow, c0 + c, ch, attr);
    }
}

inline void paintChrome(std::uint8_t* ram) noexcept {
    syncLayout();
    FieldWinFrame::ScrollState sc{};
    sc.topLine = topLine;
    sc.totalLines = std::max(1, static_cast<int>(lines.size()));
    sc.hScroll = hScroll;
    sc.maxCol = maxLineCol();
    char title[96];
    const char* base = path.empty() ? "Untitled" : path.c_str();
    const char* slash = std::strrchr(base, '\\');
    const char* name = slash ? slash + 1 : base;
    std::snprintf(title, sizeof title, " %s%s — Field Monaco ", name, dirty ? "*" : "");
    char status[180];
    std::snprintf(status, sizeof status,
        " Ln %d Col %d | %s | %zu lines %s | Esc quit Ctrl+S save ",
        curRow + 1, curCol + 1, FieldRtxGui::syntaxLangName(lang),
        lines.size(), dirty ? "*" : "");
    FieldWinFrame::paintToolbar(ram, layout, title);
    FieldWinFrame::paintMenuBar(ram, layout, FieldWinFrame::kStdMenus,
        FieldWinFrame::kStdMenuCount, menuSt, nullptr);
    FieldWinFrame::paintVScroll(ram, layout, sc);
    if (frameOpt.hScrollRows > 0)
        FieldWinFrame::paintHScroll(ram, layout, sc, frameOpt.hScrollRows);
    if (frameOpt.logPanel)
        FieldWinFrame::paintLogMinimap(ram, layout, outlineLog, topLine);
    FieldWinFrame::paintStatus(ram, layout, status);
}

inline void paint(std::uint8_t* ram) noexcept {
    FieldRtxVgaText::initMonaco(ram);
    applyMonacoTheme();
    FieldWinFrame::clearScreen(ram, MC_BG);
    frameOpt.toolbar = true;
    frameOpt.statusBar = true;
    frameOpt.logPanel = true;
    frameOpt.logCols = 22;
    frameOpt.hScrollRows = 1;
    syncLayout();
    for (int r = layout.clientR0; r < layout.clientR1; ++r)
        for (int c = layout.clientC0; c < layout.clientC1; ++c)
            FieldRtxGui::put(ram, r, c, ' ', MC_BG);
    for (int vis = 0; vis < edRows(); ++vis) {
        const int row = layout.clientR0 + vis;
        const int dr = topLine + vis;
        if (dr >= 0 && dr < static_cast<int>(lines.size()))
            paintEditorLine(ram, row, lines[static_cast<std::size_t>(dr)], dr == curRow);
        else
            paintGutter(ram, row, dr);
    }
    paintChrome(ram);
    if (curRow >= topLine && curRow < topLine + edRows()) {
        const int visCol = layout.clientC0 + GUTTER_COLS + (curCol - hScroll);
        if (visCol >= layout.clientC0 + GUTTER_COLS && visCol < layout.clientC1)
            FieldRtxGui::setCursor(ram, layout.clientR0 + (curRow - topLine), visCol);
    }
}

inline void ensureCursorVisible() noexcept {
    if (curRow < topLine) topLine = curRow;
    if (curRow >= topLine + edRows()) topLine = curRow - edRows() + 1;
    if (curCol < hScroll) hScroll = curCol;
    if (curCol >= hScroll + edWidth()) hScroll = curCol - edWidth() + 1;
    if (hScroll < 0) hScroll = 0;
}

inline void handleMenuAction(int action, std::uint8_t* ram) noexcept {
    switch (action) {
    case 103:
    case 104:
        saveFile();
        break;
    case 109:
        active = false;
        FieldRtxVgaText::initMonaco(ram);
        return;
    case 301:
        topLine = 0;
        break;
    case 302:
        topLine = std::max(0, static_cast<int>(lines.size()) - edRows());
        break;
    case 304:
        FieldRtxThemePicker::open();
        break;
    default:
        break;
    }
    paint(ram);
}

inline void insertChar(char ch) noexcept {
    if (curRow < 0) curRow = 0;
    if (curRow >= static_cast<int>(lines.size())) lines.resize(static_cast<std::size_t>(curRow + 1));
    std::string& ln = lines[static_cast<std::size_t>(curRow)];
    if (curCol < 0) curCol = 0;
    if (curCol > static_cast<int>(ln.size())) curCol = static_cast<int>(ln.size());
    ln.insert(static_cast<std::size_t>(curCol), 1, ch);
    ++curCol;
    dirty = true;
    rebuildOutline();
}

inline void backspace() noexcept {
    if (curRow < 0 || curRow >= static_cast<int>(lines.size())) return;
    std::string& ln = lines[static_cast<std::size_t>(curRow)];
    if (curCol > 0) {
        ln.erase(static_cast<std::size_t>(curCol - 1), 1);
        --curCol;
        dirty = true;
    } else if (curRow > 0) {
        curCol = static_cast<int>(lines[static_cast<std::size_t>(curRow - 1)].size());
        lines[static_cast<std::size_t>(curRow - 1)] += ln;
        lines.erase(lines.begin() + curRow);
        --curRow;
        dirty = true;
    }
    rebuildOutline();
}

inline void newline() noexcept {
    if (curRow < 0) curRow = 0;
    if (curRow >= static_cast<int>(lines.size())) lines.push_back("");
    std::string& ln = lines[static_cast<std::size_t>(curRow)];
    std::string tail;
    if (curCol < static_cast<int>(ln.size())) {
        tail = ln.substr(static_cast<std::size_t>(curCol));
        ln.erase(static_cast<std::size_t>(curCol));
    }
    lines.insert(lines.begin() + curRow + 1, tail);
    ++curRow;
    curCol = 0;
    dirty = true;
    rebuildOutline();
}

inline void handleMouse(std::uint8_t* ram) noexcept {
    if (!active || !ram) return;
    const FieldRtxMouse::Frame m = FieldRtxMouse::capture();
    if (!m.visible) return;
    int action = 0;
    if (FieldWinFrame::pumpMouse(ram, layout, m.col, m.row, m.leftClick,
            FieldWinFrame::kStdMenus, FieldWinFrame::kStdMenuCount, menuSt, action)) {
        if (action) handleMenuAction(action, ram);
        else paint(ram);
        return;
    }
    if (m.leftClick || m.rightClick)
        FieldRtxMouse::paintPointer(ram, m.col, m.row);
}

inline bool handleKey(std::uint8_t* ram, std::uint16_t key) noexcept {
    if (!active) return false;
    int menuAction = 0;
    if (FieldWinFrame::pumpMenuKey(key, FieldWinFrame::kStdMenus,
            FieldWinFrame::kStdMenuCount, menuSt, menuAction)) {
        if (menuAction) handleMenuAction(menuAction, ram);
        else paint(ram);
        return true;
    }
    const std::uint8_t ch = static_cast<std::uint8_t>(key & 0xFFu);
    const bool ctrl = (key & 0x4000u) != 0u;

    if (key == 0x011Bu) { active = false; FieldRtxVgaText::initMonaco(ram); return true; }
    if (ctrl && (ch == 's' || ch == 'S')) { saveFile(); paint(ram); return true; }

    switch (key) {
    case 0x4800u: if (curRow > 0) --curRow; break;
    case 0x5000u: if (curRow + 1 < static_cast<int>(lines.size())) ++curRow; break;
    case 0x4B00u: if (curCol > 0) --curCol; break;
    case 0x4D00u: ++curCol; break;
    case 0x4700u: topLine = std::max(0, topLine - edRows()); paint(ram); return true;
    case 0x5100u: topLine = std::min(std::max(0, static_cast<int>(lines.size()) - edRows()),
                                     topLine + edRows()); paint(ram); return true;
    case 0x5300u: hScroll = std::max(0, hScroll - 8); paint(ram); return true;
    case 0x4F00u: hScroll += 8; paint(ram); return true;
    case 0x0E08u: backspace(); break;
    case 0x1C0Du: newline(); break;
    case 0x3909u: insertChar('\t'); break;
    default:
        if (ch >= 32u && ch < 127u) insertChar(static_cast<char>(ch));
        else return false;
    }
    ensureCursorVisible();
    paint(ram);
    return true;
}

inline void open(std::uint8_t* ram, const char* filePath) noexcept {
    FieldRtxThemes::init();
    menuSt = {};
    lines = {""};
    path.clear();
    dirty = false;
    curRow = curCol = topLine = hScroll = 0;
    if (filePath && filePath[0]) loadFile(filePath);
    else rebuildOutline();
    active = true;
    paint(ram);
}

inline void close(std::uint8_t* ram) noexcept {
    active = false;
    FieldRtxVgaText::initMonaco(ram);
}

} // namespace FieldMonacoEdit