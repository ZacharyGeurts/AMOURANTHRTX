#pragma once

// AmmoText — RTX Notepad clone (spellcheck, menus, help, themes).

#include "FieldAmmoCode.hpp"
#include "FieldAmmoTextFonts.hpp"
#include "FieldAmmoTextSpell.hpp"
#include "FieldAmmoVfs.hpp"
#include "FieldDosViewport.hpp"
#include "FieldRtxGui.hpp"
#include "FieldRtxThemePicker.hpp"
#include "FieldRtxThemes.hpp"
#include "FieldRtxVgaText.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <functional>
#include <string>
#include <vector>

namespace FieldAmmoText {

constexpr int MENU_ROW = 0;
constexpr int ED_TOP = 1;
constexpr int ED_ROWS = 24;
constexpr int ED_COL = 0;
constexpr int ED_W = 80;

constexpr std::uint8_t NP_MENU      = 0x70u;
constexpr std::uint8_t NP_MENU_HI   = 0x17u;
constexpr std::uint8_t NP_MENU_SEL  = 0x1Fu;
constexpr std::uint8_t NP_EDITOR    = 0xF0u;
constexpr std::uint8_t NP_TEXT      = 0xF0u;
constexpr std::uint8_t NP_CURSOR    = 0xF1u;
constexpr std::uint8_t NP_SELECT    = 0x1Fu;
constexpr std::uint8_t NP_SPELL     = 0xF4u;
constexpr std::uint8_t NP_KW       = 0xF1u;
constexpr std::uint8_t NP_STR      = 0xF4u;
constexpr std::uint8_t NP_COMMENT  = 0xF8u;

inline bool active = false;
inline bool insertMode = true;
inline bool wordWrap = false;
inline bool spellOn = true;
inline bool statusBar = false;
inline bool fontPickerOpen = false;
inline int  fontPickerSel = 0;
inline bool helpOpen = false;
inline int  helpScroll = 0;
inline std::string path;
inline std::string clipboard;
inline std::string findNeedle;
inline std::vector<std::string> lines;
inline std::vector<std::string> helpLines;
inline int curRow = 0, curCol = 0, topLine = 0;
inline bool dirty = false;
inline FieldRtxGui::SyntaxLang lang = FieldRtxGui::SyntaxLang::Auto;
inline FieldRtxGui::MenuBarState menuSt;

using EchoFn = std::function<void(std::uint8_t*, char)>;
using NewlineFn = std::function<void(std::uint8_t*)>;

enum Action : int {
    A_NEW = 1, A_OPEN, A_SAVE, A_SAVEAS, A_EXIT,
    A_UNDO, A_CUT, A_COPY, A_PASTE, A_DEL, A_FIND, A_FINDN, A_REPLACE, A_GOTO, A_SELALL, A_DATETIME,
    A_WRAP, A_SPELL, A_FONT, A_FONTSZ_UP, A_FONTSZ_DN, A_STATUS, A_THEMES, A_SYN_AUTO,
    A_HELP, A_ABOUT, A_AMMOCODE
};

static const FieldRtxGui::MenuItem kFile[] = {
    {FieldRtxGui::ICO_FOLDER, "New",        'N', A_NEW},
    {FieldRtxGui::ICO_FOLDER, "Open...",    'O', A_OPEN},
    {FieldRtxGui::ICO_DISK,  "Save",       'S', A_SAVE},
    {FieldRtxGui::ICO_DISK,  "Save As...", 'A', A_SAVEAS},
    {FieldRtxGui::ICO_STOP,  "Exit",       'X', A_EXIT},
};
static const FieldRtxGui::MenuItem kEdit[] = {
    {FieldRtxGui::BOX_X, "Cut",          'T', A_CUT},
    {FieldRtxGui::BOX_X, "Copy",         'C', A_COPY},
    {FieldRtxGui::BOX_X, "Paste",        'P', A_PASTE},
    {FieldRtxGui::BOX_X, "Delete",       'L', A_DEL},
    {FieldRtxGui::BOX_X, "Find...",      'F', A_FIND},
    {FieldRtxGui::BOX_X, "Find Next",    'N', A_FINDN},
    {FieldRtxGui::BOX_X, "Replace...",   'R', A_REPLACE},
    {FieldRtxGui::BOX_X, "Go To Line",   'G', A_GOTO},
    {FieldRtxGui::BOX_X, "Select All",   'A', A_SELALL},
    {FieldRtxGui::BOX_X, "Time/Date",    'D', A_DATETIME},
};
static const FieldRtxGui::MenuItem kFormat[] = {
    {FieldRtxGui::BOX_X, "Word Wrap",    'W', A_WRAP},
    {FieldRtxGui::BOX_X, "Font...",      'F', A_FONT},
    {FieldRtxGui::BOX_X, "Font Size +",  '+', A_FONTSZ_UP},
    {FieldRtxGui::BOX_X, "Font Size -",  '-', A_FONTSZ_DN},
    {FieldRtxGui::BOX_X, "Spellcheck",   'S', A_SPELL},
};
static const FieldRtxGui::MenuItem kView[] = {
    {FieldRtxGui::BOX_X, "Status Bar",   'B', A_STATUS},
    {FieldRtxGui::ICO_BOOK, "Themes...", 'H', A_THEMES},
};
static const FieldRtxGui::MenuItem kHelpMenu[] = {
    {FieldRtxGui::ICO_BOOK, "Help Topics", 'H', A_HELP},
    {FieldRtxGui::ICO_BLOCK,"About",       'A', A_ABOUT},
};

static const FieldRtxGui::Menu kMenus[] = {
    {FieldRtxGui::ICO_FOLDER, "File",   kFile,      5},
    {FieldRtxGui::BOX_X,      "Edit",   kEdit,     10},
    {FieldRtxGui::BOX_X,      "Format", kFormat,    5},
    {FieldRtxGui::ICO_BOOK,   "View",   kView,      2},
    {FieldRtxGui::ICO_BOOK,   "Help",   kHelpMenu,  2},
};
constexpr int kMenuCount = 5;
inline int menuCols[kMenuCount]{};

inline void detectLang() noexcept {
    lang = FieldRtxGui::syntaxLangFromPath(path.c_str());
}

inline void loadHelpText() {
    helpLines.clear();
    static const char* kEmbedded =
        "AmmoText Help\r\n"
        "=============\r\n"
        "File — New Open Save Save As Exit\r\n"
        "Edit — Cut Copy Paste Find Replace Go To Time/Date\r\n"
        "Format — Word Wrap toggles soft wrap; Spellcheck marks unknown words\r\n"
        "View — Status Bar; Themes opens RTX color presets for all panels\r\n"
        "Keys — F1 Help F2 Save F3 Open F7 Spell F10 Menu Tab indent Esc exit\r\n"
        "Spellcheck uses an embedded dictionary plus RTX/DOS terms.\r\n"
        "Themes apply to AmmoText, Device Manager, Registry, and all RTX GUIs.\r\n";
    std::vector<std::uint8_t> data;
    if (FieldAmmoVfs::readPath("C:\\AMMOTEXT.HLP", data) && !data.empty()) {
        std::string line;
        for (std::uint8_t b : data) {
            if (b == '\r') continue;
            if (b == '\n') { helpLines.push_back(line); line.clear(); }
            else if (b >= 32 && b < 127) line += static_cast<char>(b);
        }
        if (!line.empty()) helpLines.push_back(line);
    }
    if (helpLines.empty()) {
        std::string line;
        for (const char* p = kEmbedded; *p; ++p) {
            if (*p == '\r') continue;
            if (*p == '\n') { helpLines.push_back(line); line.clear(); }
            else line += *p;
        }
        if (!line.empty()) helpLines.push_back(line);
    }
}

inline bool loadFile(const char* filePath) {
    lines.clear();
    std::vector<std::uint8_t> data;
    if (FieldAmmoVfs::readPath(filePath, data) && !data.empty()) {
        std::string line;
        for (std::uint8_t b : data) {
            if (b == '\r') continue;
            if (b == '\n') { lines.push_back(line); line.clear(); }
            else if (b >= 32 && b < 127) line += static_cast<char>(b);
            else if (b == '\t') line += "    ";
        }
        if (!line.empty()) lines.push_back(line);
    }
    if (lines.empty()) lines.push_back("");
    curRow = curCol = topLine = 0;
    dirty = false;
    path = filePath;
    detectLang();
    return true;
}

inline bool saveFile() {
    std::string body;
    for (std::size_t i = 0; i < lines.size(); ++i) {
        body += lines[i];
        if (i + 1 < lines.size()) body += "\r\n";
    }
    dirty = false;
    return FieldAmmoVfs::writePath(path.c_str(),
        reinterpret_cast<const std::uint8_t*>(body.data()), body.size());
}

inline std::uint8_t syntaxAttrNp(const std::string& ln, int c) noexcept {
    const std::uint8_t a = FieldRtxGui::syntaxAttrAt(ln, c, lang);
    if (a == FieldRtxGui::ATTR_KW) return NP_KW;
    if (a == FieldRtxGui::ATTR_STR) return NP_STR;
    if (a == FieldRtxGui::ATTR_COMMENT) return NP_COMMENT;
    return NP_TEXT;
}

inline std::uint8_t attrAt(const std::string& ln, int c) noexcept {
    if (!spellOn || helpOpen)
        return syntaxAttrNp(ln, c);
    if (FieldAmmoTextSpell::wordBadAt(ln, c))
        return NP_SPELL;
    return syntaxAttrNp(ln, c);
}

inline void applyNotepadAttrs() noexcept {
    FieldRtxGui::ATTR_MENU      = NP_MENU;
    FieldRtxGui::ATTR_MENU_SEL  = NP_MENU_SEL;
    FieldRtxGui::ATTR_MENU_HI   = NP_MENU_HI;
    FieldRtxGui::ATTR_EDITOR    = NP_EDITOR;
    FieldRtxGui::ATTR_STATUS    = 0x70u;
    FieldRtxGui::ATTR_FRAME     = 0x70u;
    FieldRtxGui::ATTR_HELP      = NP_TEXT;
    FieldRtxGui::ATTR_DIM       = 0x78u;
    FieldRtxGui::ATTR_GOLD      = 0xF0u;
    FieldRtxGui::ATTR_CURSOR    = NP_CURSOR;
    FieldRtxGui::ATTR_BLOCK     = NP_SELECT;
    FieldRtxGui::ATTR_WORD      = 0xF3u;
    FieldRtxGui::ATTR_SPELL     = NP_SPELL;
}

inline void syncFontScale() noexcept {
    FieldDosViewport::fontScale = FieldAmmoTextFonts::activeScale();
    FieldDosViewport::crispFont = true;
    FieldDosViewport::subpixelFont = FieldAmmoTextFonts::activeIndex >= 4;
    FieldDosViewport::scanlines = false;
}

inline void paintEditorLine(std::uint8_t* ram, int row, const std::string& ln, bool cur) noexcept {
    int w0 = 0, w1 = 0;
    if (cur) FieldRtxGui::wordBounds(ln, curCol, w0, w1);
    for (int c = 0; c < ED_W; ++c) {
        char ch = c < static_cast<int>(ln.size()) ? ln[static_cast<std::size_t>(c)] : ' ';
        std::uint8_t attr = c < static_cast<int>(ln.size()) ? attrAt(ln, c) : FieldRtxGui::ATTR_EDITOR;
        if (cur && c >= w0 && c < w1 && c != curCol) attr = FieldRtxGui::ATTR_WORD;
        if (cur && c == curCol) attr = NP_CURSOR;
        FieldRtxGui::put(ram, row, ED_COL + c, ch, attr);
    }
}

inline void statusText(char* buf, std::size_t n) noexcept {
    const char* base = path.empty() ? "untitled" : path.c_str();
    const char* slash = std::strrchr(base, '\\');
    const char* name = slash ? slash + 1 : base;
    int bad = 0;
    if (spellOn && curRow >= 0 && curRow < static_cast<int>(lines.size()))
        bad = FieldAmmoTextSpell::countBadInLine(lines[static_cast<std::size_t>(curRow)]);
    std::snprintf(buf, n, " Ln %d, Col %d | %s | %s%s | spell:%s ",
        curRow + 1, curCol + 1, name, FieldRtxGui::syntaxLangName(lang),
        dirty ? "*" : "", spellOn ? (bad ? "!" : "OK") : "off");
}

inline void paintHelp(std::uint8_t* ram) noexcept {
    FieldRtxGui::drawFrame(ram, 2, 2, 20, 77, FieldRtxGui::ATTR_FRAME, " AmmoText Help ");
    const int vis = 16;
    for (int i = 0; i < vis; ++i) {
        const int hi = helpScroll + i;
        const int row = 4 + i;
        FieldRtxGui::fill(ram, row, ' ', FieldRtxGui::ATTR_HELP);
        if (hi >= 0 && hi < static_cast<int>(helpLines.size()))
            FieldRtxGui::text(ram, row, 4, helpLines[static_cast<std::size_t>(hi)].c_str(),
                FieldRtxGui::ATTR_HELP, 72);
    }
    FieldRtxGui::text(ram, 21, 4, " PgUp/PgDn scroll   Esc closes help ", FieldRtxGui::ATTR_DIM, 72);
}

inline void paintFontPicker(std::uint8_t* ram) noexcept {
    FieldRtxGui::drawFrame(ram, 4, 6, 18, 68, NP_MENU, " Font ");
    for (int i = 0; i < FieldAmmoTextFonts::kCount && i < 12; ++i) {
        const int row = 8 + i;
        const bool sel = fontPickerSel == i;
        FieldRtxGui::fill(ram, row, ' ', sel ? NP_MENU_SEL : NP_EDITOR);
        FieldRtxGui::text(ram, row, 6,
            FieldAmmoTextFonts::kPresets[static_cast<std::size_t>(i)].name,
            sel ? NP_MENU_SEL : NP_TEXT, 40);
    }
    FieldRtxGui::text(ram, 20, 6, " Enter=Apply  Esc=Cancel ", NP_TEXT, 40);
}

inline void paint(std::uint8_t* ram) noexcept {
    FieldRtxGui::initTextMode(ram);
    applyNotepadAttrs();
    syncFontScale();

    const char* base = path.empty() ? "Untitled" : path.c_str();
    const char* slash = std::strrchr(base, '\\');
    const char* name = slash ? slash + 1 : base;
    char title[56];
    std::snprintf(title, sizeof title, " %s%s - AmmoText ",
        name, dirty ? "*" : "");
    FieldRtxGui::paintMenuBarTp(ram, MENU_ROW, kMenus, kMenuCount, menuSt, title, menuCols, kMenuCount);

    if (fontPickerOpen) {
        paintFontPicker(ram);
    } else if (helpOpen) {
        paintHelp(ram);
    } else {
        for (int vis = 0; vis < ED_ROWS; ++vis) {
            const int row = ED_TOP + vis;
            const int dr = topLine + vis;
            FieldRtxGui::fill(ram, row, ' ', NP_EDITOR);
            if (dr >= 0 && dr < static_cast<int>(lines.size()))
                paintEditorLine(ram, row, lines[static_cast<std::size_t>(dr)], dr == curRow);
        }
    }

    if (statusBar) {
        FieldRtxGui::fill(ram, 24, ' ', 0x70u);
        char st[80];
        statusText(st, sizeof st);
        FieldRtxGui::text(ram, 24, 0, st, 0x70u, 78);
    }

    if (menuSt.openMenu >= 0 && menuSt.openMenu < kMenuCount)
        FieldRtxGui::paintDropDown(ram, MENU_ROW + 1, menuCols[menuSt.openMenu],
            kMenus[menuSt.openMenu], menuSt.selItem);
    if (FieldRtxGui::modal.active)
        FieldRtxGui::paintModal(ram);
    if (FieldRtxThemePicker::active)
        FieldRtxThemePicker::paint(ram);

    if (!helpOpen) {
        const int visRow = ED_TOP + (curRow - topLine);
        FieldRtxGui::setCursor(ram, visRow, ED_COL + curCol);
    }
}

inline void open(std::uint8_t* ram, const char* filePath) noexcept {
    FieldRtxVgaText::initClassic(ram);
    FieldAmmoTextFonts::applyIndex(0);
    FieldRtxThemes::init();
    loadHelpText();
    if (filePath && filePath[0])
        loadFile(filePath);
    else {
        lines = {""};
        path.clear();
        dirty = false;
    }
    active = true;
    menuSt = {};
    helpOpen = false;
    paint(ram);
}

inline void close(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    active = false;
    menuSt = {};
    helpOpen = false;
    FieldRtxGui::closeModal();
    FieldRtxThemePicker::close();
    FieldRtxGui::initTextMode(ram);
    for (const char* p = "\r\nAmmoText closed.\r\n"; *p; ++p) {
        if (*p == '\r') continue;
        if (*p == '\n') nl(ram);
        else echo(ram, *p);
    }
}

inline void insertTab() {
    auto& ln = lines[static_cast<std::size_t>(curRow)];
    ln.insert(static_cast<std::size_t>(curCol), "    ");
    curCol += 4;
    dirty = true;
}

inline void insertDateTime() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof buf, "%m/%d/%Y  %I:%M %p", std::localtime(&t));
    auto& ln = lines[static_cast<std::size_t>(curRow)];
    ln.insert(static_cast<std::size_t>(curCol), buf);
    curCol += static_cast<int>(std::strlen(buf));
    dirty = true;
}

inline bool findNext() {
    if (findNeedle.empty()) return false;
    for (int r = curRow; r < static_cast<int>(lines.size()); ++r) {
        const std::string& ln = lines[static_cast<std::size_t>(r)];
        const std::size_t start = (r == curRow)
            ? static_cast<std::size_t>(curCol + 1) : 0u;
        const auto pos = ln.find(findNeedle, start);
        if (pos != std::string::npos) {
            curRow = r;
            curCol = static_cast<int>(pos);
            return true;
        }
    }
    return false;
}

inline bool applyModalOk(std::uint8_t* ram) noexcept {
    if (!FieldRtxGui::modal.active) return false;
    const int p = FieldRtxGui::modal.purpose;
    if (p == 1 || p == 2) {
        if (!FieldRtxGui::modal.value.empty()) {
            if (p == 2) path = FieldRtxGui::modal.value;
            loadFile(FieldRtxGui::modal.value.c_str());
            if (p == 2) dirty = true;
        }
    } else if (p == 3) {
        findNeedle = FieldRtxGui::modal.value;
        findNext();
    } else if (p == 4) {
        const int line = std::max(1, std::atoi(FieldRtxGui::modal.value.c_str())) - 1;
        curRow = std::clamp(line, 0, static_cast<int>(lines.size()) - 1);
        curCol = 0;
    }
    FieldRtxGui::closeModal();
    paint(ram);
    return true;
}

inline bool dispatch(int act, std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    switch (act) {
    case A_NEW:
        lines = {""}; path.clear(); curRow = curCol = 0; dirty = false;
        return true;
    case A_OPEN: FieldRtxGui::openModal(1, "Open", path); return true;
    case A_SAVE: saveFile(); return true;
    case A_SAVEAS: FieldRtxGui::openModal(2, "Save As", path); return true;
    case A_EXIT: close(ram, echo, nl); return true;
    case A_CUT:
        if (curRow < static_cast<int>(lines.size())) {
            clipboard = lines[static_cast<std::size_t>(curRow)];
            lines.erase(lines.begin() + curRow);
            if (lines.empty()) lines.push_back("");
            curRow = std::min(curRow, static_cast<int>(lines.size()) - 1);
            dirty = true;
        }
        return true;
    case A_COPY:
        if (curRow < static_cast<int>(lines.size()))
            clipboard = lines[static_cast<std::size_t>(curRow)];
        return true;
    case A_PASTE:
        if (!clipboard.empty()) {
            lines.insert(lines.begin() + curRow + 1, clipboard);
            ++curRow;
            curCol = 0;
            dirty = true;
        }
        return true;
    case A_DEL:
        if (curCol < static_cast<int>(lines[static_cast<std::size_t>(curRow)].size())) {
            lines[static_cast<std::size_t>(curRow)].erase(static_cast<std::size_t>(curCol), 1);
            dirty = true;
        }
        return true;
    case A_FIND: FieldRtxGui::openModal(3, "Find", findNeedle); return true;
    case A_FINDN: findNext(); return true;
    case A_REPLACE: FieldRtxGui::openModal(3, "Find (then edit)", findNeedle); return true;
    case A_GOTO: FieldRtxGui::openModal(4, "Go To Line", "1"); return true;
    case A_SELALL: curRow = 0; curCol = 0; return true;
    case A_DATETIME: insertDateTime(); return true;
    case A_WRAP: wordWrap = !wordWrap; return true;
    case A_SPELL: spellOn = !spellOn; return true;
    case A_FONT:
        fontPickerOpen = true;
        fontPickerSel = FieldAmmoTextFonts::activeIndex;
        return true;
    case A_FONTSZ_UP:
        FieldAmmoTextFonts::applyIndex(
            std::min(FieldAmmoTextFonts::activeIndex + 1, FieldAmmoTextFonts::kCount - 1));
        syncFontScale();
        return true;
    case A_FONTSZ_DN:
        FieldAmmoTextFonts::applyIndex(
            std::max(FieldAmmoTextFonts::activeIndex - 1, 0));
        syncFontScale();
        return true;
    case A_STATUS: statusBar = !statusBar; return true;
    case A_THEMES: FieldRtxThemePicker::open(); return true;
    case A_HELP:
        loadHelpText();
        helpOpen = true;
        helpScroll = 0;
        return true;
    case A_ABOUT:
        helpLines = {
            "About AmmoText",
            "RTX-DOS 7.0 Notepad clone",
            "Spellcheck | Themes | Syntax colors",
            "Golden Era Man+Machine — AmouranthOS 2026",
        };
        helpOpen = true;
        helpScroll = 0;
        return true;
    case A_AMMOCODE:
        close(ram, echo, nl);
        FieldAmmoCode::open(ram, FieldAmmoCode::Lang::Basic, path.c_str());
        return true;
    default: return true;
    }
}

inline bool handleKey(std::uint8_t* ram, std::uint16_t key, EchoFn echo, NewlineFn nl) noexcept {
    if (!active) return false;

    if (fontPickerOpen) {
        if (key == 0x011Bu) { fontPickerOpen = false; paint(ram); return true; }
        if (key == 0x4800u && fontPickerSel > 0) { --fontPickerSel; paint(ram); return true; }
        if (key == 0x5000u && fontPickerSel + 1 < FieldAmmoTextFonts::kCount) {
            ++fontPickerSel; paint(ram); return true;
        }
        if (key == 0x1C0Du) {
            FieldAmmoTextFonts::applyIndex(fontPickerSel);
            fontPickerOpen = false;
            syncFontScale();
            paint(ram);
            return true;
        }
        return true;
    }

    if (FieldRtxThemePicker::active) {
        FieldRtxThemePicker::handleKey(key);
        paint(ram);
        return true;
    }

    if (FieldRtxGui::modal.active) {
        if (key == 0x1C0Du) { applyModalOk(ram); return true; }
        if (FieldRtxGui::pumpModalKey(ram, key)) return true;
    }

    if (helpOpen) {
        if (key == 0x011Bu || key == 0x3B00u) { helpOpen = false; paint(ram); return true; }
        if (key == 0x4900u && helpScroll > 0) { --helpScroll; paint(ram); return true; }
        if (key == 0x5100u && helpScroll + 16 < static_cast<int>(helpLines.size())) {
            ++helpScroll; paint(ram); return true;
        }
        return true;
    }

    if (menuSt.openMenu >= 0) {
        if (key == 0x011Bu) menuSt.openMenu = -1;
        else if (key == 0x4800u && menuSt.selItem > 0) --menuSt.selItem;
        else if (key == 0x5000u && menuSt.selItem + 1 < kMenus[menuSt.openMenu].itemCount)
            ++menuSt.selItem;
        else if (key == 0x1C0Du)
            dispatch(kMenus[menuSt.openMenu].items[menuSt.selItem].actionId, ram, echo, nl);
        else {
            int act = 0;
            if (FieldRtxGui::matchMenuHotkey(kMenus[menuSt.openMenu],
                    static_cast<char>(key & 0xFFu), act))
                dispatch(act, ram, echo, nl);
        }
        paint(ram);
        return true;
    }

    switch (key) {
    case 0x3B00u: dispatch(A_HELP, ram, echo, nl); paint(ram); return true;
    case 0x3C00u: saveFile(); paint(ram); return true;
    case 0x3D00u: FieldRtxGui::openModal(1, "Open", path); paint(ram); return true;
    case 0x4100u: spellOn = !spellOn; paint(ram); return true;
    case 0x4400u:
        menuSt.openMenu = (menuSt.openMenu < 0) ? 0 : -1;
        menuSt.selItem = 0;
        paint(ram);
        return true;
    case 0x011Bu: close(ram, echo, nl); return true;
    case 0x4800u: if (curRow > 0) --curRow; break;
    case 0x5000u: if (curRow + 1 < static_cast<int>(lines.size())) ++curRow; break;
    case 0x4B00u: if (curCol > 0) --curCol; break;
    case 0x4D00u:
        if (curCol < static_cast<int>(lines[static_cast<std::size_t>(curRow)].size())) ++curCol;
        break;
    case 0x0F09u: insertTab(); break;
    case 0x0E08u:
        if (curCol > 0) {
            lines[static_cast<std::size_t>(curRow)].erase(static_cast<std::size_t>(curCol - 1), 1);
            --curCol; dirty = true;
        }
        break;
    case 0x1C0Du: {
        auto& ln = lines[static_cast<std::size_t>(curRow)];
        std::string tail = ln.substr(static_cast<std::size_t>(curCol));
        ln.erase(static_cast<std::size_t>(curCol));
        lines.insert(lines.begin() + curRow + 1, tail);
        ++curRow; curCol = 0; dirty = true;
        break;
    }
    default: {
        const char ch = static_cast<char>(key & 0xFFu);
        if (ch >= 32 && ch < 127) {
            auto& ln = lines[static_cast<std::size_t>(curRow)];
            if (insertMode) ln.insert(static_cast<std::size_t>(curCol), 1, ch);
            else if (curCol < static_cast<int>(ln.size())) ln[static_cast<std::size_t>(curCol)] = ch;
            else ln += ch;
            ++curCol; dirty = true;
        } else return true;
        break;
    }
    }
    if (curRow < topLine) topLine = curRow;
    if (curRow >= topLine + ED_ROWS) topLine = curRow - ED_ROWS + 1;
    paint(ram);
    return true;
}

inline bool wantsHelpSwitch(const std::vector<std::string>& args) noexcept {
    return args.size() >= 2 && FieldRtxGui::argIsHelp(args[1]);
}

inline void printHelp(EchoFn echo, NewlineFn nl, std::uint8_t* ram) noexcept {
    static const char* help =
        "\r\nAmmoText — RTX Notepad (spellcheck, themes, syntax colors)\r\n"
        "  AMMOTEXT [file]   NOTEPAD and EDIT are aliases\r\n"
        "  THEMES            RTX windowing color presets\r\n";
    for (const char* p = help; *p; ++p) {
        if (*p == '\r') continue;
        if (*p == '\n') nl(ram);
        else echo(ram, *p);
    }
}

} // namespace FieldAmmoText