#pragma once

// RTX Golden Era GUI — DOS menus, CP437 frames, RTX color iconography.
// Case-insensitive matching throughout.

#include "FieldPlatform.hpp"
#include "FieldRtxVgaText.hpp"
#include "FieldVga.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace FieldRtxGui {

constexpr std::uint32_t VGA = FieldRtxVgaText::VGA;
constexpr std::uint32_t BDA_COL = FieldRtxVgaText::BDA_COL;
constexpr std::uint32_t BDA_ROW = FieldRtxVgaText::BDA_ROW;

// VGA attributes — RTX palette (themeable via FieldRtxThemes)
inline std::uint8_t ATTR_MENU     = 0x31;
inline std::uint8_t ATTR_MENU_SEL = 0x13;
inline std::uint8_t ATTR_MENU_HI  = 0x1E;
inline std::uint8_t ATTR_EDITOR   = 0x1F;
inline std::uint8_t ATTR_IMMED    = 0x3F;
inline std::uint8_t ATTR_STATUS   = 0x70;
inline std::uint8_t ATTR_FRAME    = 0x3B;
inline std::uint8_t ATTR_TITLE    = 0x5E;
inline std::uint8_t ATTR_RTX      = 0x5D;
inline std::uint8_t ATTR_GOLD     = 0x1E;
inline std::uint8_t ATTR_DEBUG    = 0x2A;
inline std::uint8_t ATTR_BREAK    = 0x4C;
inline std::uint8_t ATTR_CURSOR   = 0x70;
inline std::uint8_t ATTR_HELP     = 0x1B;
inline std::uint8_t ATTR_DIM      = 0x17;
inline std::uint8_t ATTR_KW       = 0x2E;
inline std::uint8_t ATTR_STR      = 0x4C;
inline std::uint8_t ATTR_NUM      = 0x5D;
inline std::uint8_t ATTR_COMMENT  = 0x18;
inline std::uint8_t ATTR_IDENT    = 0x1F;
inline std::uint8_t ATTR_WORD     = 0x59;
inline std::uint8_t ATTR_BLOCK    = 0x0F;
inline std::uint8_t ATTR_ASM      = 0x3A;
inline std::uint8_t ATTR_DIRECTIVE= 0x5B;
inline std::uint8_t ATTR_TYPE     = 0x6B;
inline std::uint8_t ATTR_SPELL    = 0x4E;
inline std::uint8_t ATTR_BUSINESS = 0xF0; /* black on light gray — AmouranthOS windows */

// CP437 box + icons (Golden Era DOS look)
constexpr char ICO_BLOCK   = '\xFE'; // ■
constexpr char ICO_PLAY    = '\x10'; // ►
constexpr char ICO_STOP    = '\xFE';
constexpr char ICO_STEP    = '\x11'; // ◄ step marker
constexpr char ICO_BOOK    = '\x14';
constexpr char ICO_DISK    = '\xDB'; // █ save
constexpr char ICO_FOLDER  = '\xB0'; // ░
constexpr char ICO_BREAK   = '\xB2'; // ▓
constexpr char BOX_TL      = '\xDA'; // ┌
constexpr char BOX_TR      = '\xBF'; // ┐
constexpr char BOX_BL      = '\xC0'; // └
constexpr char BOX_BR      = '\xD9'; // ┘
constexpr char BOX_H       = '\xC4'; // ─
constexpr char BOX_V       = '\xB3'; // │
constexpr char BOX_T       = '\xC2'; // ┬
constexpr char BOX_B       = '\xC1'; // ┴
constexpr char BOX_L       = '\xC3'; // ├
constexpr char BOX_R       = '\xB4'; // ┤
constexpr char BOX_X       = '\xC5'; // ┼

inline void put(std::uint8_t* ram, int row, int col, char ch, std::uint8_t attr) noexcept {
    if (!FieldRtxVgaText::inBounds(row, col)) return;
    const std::uint32_t off = FieldRtxVgaText::cellOffset(row, col);
    if (off + 1u >= FieldPlatform::GUEST_RAM_BYTES) return;
    ram[off] = static_cast<std::uint8_t>(static_cast<unsigned char>(ch));
    ram[off + 1u] = attr;
}

inline void fill(std::uint8_t* ram, int row, char ch, std::uint8_t attr) noexcept {
    for (int col = 0; col < FieldRtxVgaText::cols(); ++col) put(ram, row, col, ch, attr);
}

inline void text(std::uint8_t* ram, int row, int col, const char* s,
                 std::uint8_t attr, int max = -1) noexcept {
    if (max < 0) max = FieldRtxVgaText::cols();
    const int limit = FieldRtxVgaText::cols() - col;
    if (limit <= 0) return;
    max = std::min(max, limit);
    for (int i = 0; s[i] && i < max; ++i)
        put(ram, row, col + i, s[i], attr);
}

inline void setCursor(std::uint8_t* ram, int row, int col) noexcept {
    const int maxC = FieldRtxVgaText::cols() - 1;
    const int maxR = FieldRtxVgaText::rows() - 1;
    ram[BDA_COL] = static_cast<std::uint8_t>(col < 0 ? 0 : (col > maxC ? maxC : col));
    ram[BDA_ROW] = static_cast<std::uint8_t>(row < 0 ? 0 : (row > maxR ? maxR : row));
}

inline void initTextMode(std::uint8_t* ram) noexcept {
    FieldRtxVgaText::initMonaco(ram);
}

inline bool ciEq(const char* a, const char* b) noexcept {
    if (!a || !b) return a == b;
    while (*a && *b) {
        if (std::tolower(static_cast<unsigned char>(*a))
            != std::tolower(static_cast<unsigned char>(*b)))
            return false;
        ++a; ++b;
    }
    return *a == *b;
}

inline bool ciEq(const std::string& a, const char* b) noexcept {
    return ciEq(a.c_str(), b);
}

inline bool ciEq(const std::string& a, const std::string& b) noexcept {
    return ciEq(a.c_str(), b.c_str());
}

inline std::string ciUpper(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

inline void trim(std::string& s) {
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) s.erase(0, 1);
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) s.pop_back();
}

inline bool ciHasPrefix(const std::string& s, const char* prefix) noexcept {
    std::string u = ciUpper(s);
    std::string p = ciUpper(std::string(prefix));
    return u.size() >= p.size() && u.compare(0, p.size(), p) == 0;
}

struct MenuItem {
    char icon;
    const char* label;
    char hotkey;      // 0 or letter
    int actionId;
};

struct Menu {
    char icon;
    const char* title;
    const MenuItem* items;
    int itemCount;
};

inline void drawHLine(std::uint8_t* ram, int row, int c0, int c1, std::uint8_t attr) noexcept {
    for (int c = c0; c <= c1; ++c) put(ram, row, c, BOX_H, attr);
}

inline void drawFrame(std::uint8_t* ram, int r0, int c0, int r1, int c1,
                      std::uint8_t attr, const char* title = nullptr) noexcept {
    put(ram, r0, c0, BOX_TL, attr);
    put(ram, r0, c1, BOX_TR, attr);
    put(ram, r1, c0, BOX_BL, attr);
    put(ram, r1, c1, BOX_BR, attr);
    drawHLine(ram, r0, c0 + 1, c1 - 1, attr);
    drawHLine(ram, r1, c0 + 1, c1 - 1, attr);
    for (int r = r0 + 1; r < r1; ++r) {
        put(ram, r, c0, BOX_V, attr);
        put(ram, r, c1, BOX_V, attr);
    }
    if (title && title[0]) {
        char buf[64];
        std::snprintf(buf, sizeof buf, " %.54s ", title);
        text(ram, r0, c0 + 2, buf, ATTR_TITLE, c1 - c0 - 4);
    }
}

struct MenuBarState {
    int openMenu = -1;   // -1 closed
    int selItem = 0;
    int active = 0;      // F10 menu mode
};

inline int menuWidth(const Menu& m) noexcept {
    int w = 0;
    for (int i = 0; i < m.itemCount; ++i) {
        int lw = static_cast<int>(std::strlen(m.items[i].label)) + 4;
        if (lw > w) w = lw;
    }
    return w + 2;
}

inline void paintMenuBar(std::uint8_t* ram, int row, const Menu* menus, int menuCount,
                         MenuBarState& st, const char* rightTitle) noexcept {
    fill(ram, row, ' ', ATTR_MENU);
    put(ram, row, 0, ICO_BLOCK, ATTR_RTX);
    int col = 2;
    for (int m = 0; m < menuCount; ++m) {
        char label[24];
        std::snprintf(label, sizeof label, " %c %s ", menus[m].icon, menus[m].title);
        std::uint8_t attr = (st.openMenu == m || st.active == m + 1) ? ATTR_MENU_HI : ATTR_MENU;
        text(ram, row, col, label, attr, 20);
        col += static_cast<int>(std::strlen(label));
    }
    if (rightTitle) {
        const int cols = FieldRtxVgaText::cols();
        const int len = static_cast<int>(std::strlen(rightTitle));
        text(ram, row, std::max(0, cols - len - 1), rightTitle, ATTR_GOLD, len + 1);
    }
}

inline void paintDropDown(std::uint8_t* ram, int row, int col, const Menu& menu,
                          int sel, std::uint8_t frameAttr = ATTR_FRAME) noexcept {
    const int w = menuWidth(menu);
    const int h = menu.itemCount + 2;
    drawFrame(ram, row, col, row + h, col + w, frameAttr, menu.title);
    for (int i = 0; i < menu.itemCount; ++i) {
        const int r = row + 1 + i;
        fill(ram, r, ' ', (i == sel) ? ATTR_MENU_SEL : ATTR_EDITOR);
        put(ram, r, col + 1, ' ', ATTR_EDITOR);
        put(ram, r, col + 2, menu.items[i].icon, ATTR_GOLD);
        text(ram, r, col + 4, menu.items[i].label,
             (i == sel) ? ATTR_MENU_SEL : ATTR_EDITOR, w - 6);
        if (menu.items[i].hotkey) {
            char hk[4] = { menu.items[i].hotkey, 0, 0, 0 };
            put(ram, r, col + w - 3, hk[0], ATTR_GOLD);
        }
    }
}

inline bool matchMenuHotkey(const Menu& menu, char ch, int& outAction) noexcept {
    const char cu = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    for (int i = 0; i < menu.itemCount; ++i) {
        if (menu.items[i].hotkey
            && std::toupper(static_cast<unsigned char>(menu.items[i].hotkey)) == cu) {
            outAction = menu.items[i].actionId;
            return true;
        }
    }
    return false;
}

struct TextPageView {
    const char* const* lines;
    int lineCount = 0;
    int topLine = 0;
    int pageRows = 16;
};

inline void paintTextPage(std::uint8_t* ram, int r0, int c0, int r1, int c1,
                            const TextPageView& view, const char* header,
                            std::uint8_t textAttr = ATTR_HELP) noexcept {
    drawFrame(ram, r0, c0, r1, c1, ATTR_FRAME, header);
    for (int r = r0 + 1; r < r1; ++r) {
        fill(ram, r, ' ', textAttr);
        const int li = view.topLine + (r - r0 - 1);
        if (li >= 0 && li < view.lineCount && view.lines[li])
            text(ram, r, c0 + 1, view.lines[li], textAttr, c1 - c0 - 2);
    }
}

inline bool argIsHelp(const std::string& arg) noexcept {
    return ciEq(arg, "/h") || ciEq(arg, "/help") || ciEq(arg, "-h")
        || ciEq(arg, "--help") || ciEq(arg, "/?") || ciEq(arg, "help");
}

constexpr std::uint16_t ALT_KEY_FLAG = 0x8000u;

inline bool isAltMenuKey(std::uint16_t key, char& letter) noexcept {
    if ((key & 0xFF00u) != ALT_KEY_FLAG) return false;
    letter = static_cast<char>(key & 0xFFu);
    return letter >= 'A' && letter <= 'Z';
}

struct TpMenuEntry {
    char altLetter;
    int menuIndex;
};

inline void paintMenuBarTp(std::uint8_t* ram, int row, const Menu* menus, int menuCount,
                           MenuBarState& st, const char* rightTitle,
                           int* outCols, int outColsCap) noexcept {
    fill(ram, row, ' ', ATTR_MENU);
    put(ram, row, 0, ICO_BLOCK, ATTR_RTX);
    int col = 2;
    for (int m = 0; m < menuCount; ++m) {
        if (outCols && m < outColsCap) outCols[m] = col;
        const bool hi = st.openMenu == m || st.active == m + 1;
        put(ram, row, col++, ' ', hi ? ATTR_MENU_HI : ATTR_MENU);
        put(ram, row, col++, menus[m].icon, ATTR_GOLD);
        put(ram, row, col++, ' ', ATTR_MENU);
        const char* t = menus[m].title;
        for (int i = 0; t[i]; ++i) {
            const bool isHot = i == 0;
            put(ram, row, col++, t[i], isHot ? ATTR_GOLD : (hi ? ATTR_MENU_HI : ATTR_MENU));
        }
        put(ram, row, col++, ' ', ATTR_MENU);
    }
    if (rightTitle) {
        const int cols = FieldRtxVgaText::cols();
        const int len = static_cast<int>(std::strlen(rightTitle));
        text(ram, row, std::max(0, cols - len - 1), rightTitle, ATTR_GOLD, len + 1);
    }
}

inline int hitMenuBar(const int* cols, int menuCount, int col, int row) noexcept {
    if (row != 0 || col < 2) return -1;
    for (int m = menuCount - 1; m >= 0; --m) {
        if (cols[m] >= 0 && col >= cols[m]) return m;
    }
    return -1;
}

inline int hitDropDown(const Menu& menu, int baseCol, int baseRow, int col, int row) noexcept {
    const int w = menuWidth(menu);
    const int h = menu.itemCount + 2;
    if (col < baseCol || col > baseCol + w || row <= baseRow || row > baseRow + h) return -1;
    const int item = row - baseRow - 1;
    return (item >= 0 && item < menu.itemCount) ? item : -1;
}

inline int altLetterToMenu(char letter, const TpMenuEntry* map, int count) noexcept {
    const char u = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
    for (int i = 0; i < count; ++i)
        if (map[i].altLetter == u) return map[i].menuIndex;
    return -1;
}

inline bool isIdentChar(char c) noexcept {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '.' || c == '$';
}

inline void wordBounds(const std::string& ln, int col, int& w0, int& w1) noexcept {
    w0 = w1 = col;
    if (col < 0 || col >= static_cast<int>(ln.size())) return;
    while (w0 > 0 && isIdentChar(ln[static_cast<std::size_t>(w0 - 1)])) --w0;
    while (w1 < static_cast<int>(ln.size()) && isIdentChar(ln[static_cast<std::size_t>(w1)])) ++w1;
}

// Token rules follow common open highlighter keyword sets (MASM, C, Pascal, INI, batch).
enum class SyntaxLang : std::uint8_t { Basic, Asm, Pascal, Field, C, Ini, Batch, Auto };

inline bool ciWordEq(const std::string& w, const char* kw) noexcept {
    if (w.size() != std::strlen(kw)) return false;
    for (std::size_t i = 0; kw[i]; ++i)
        if (std::tolower(static_cast<unsigned char>(w[i]))
            != std::tolower(static_cast<unsigned char>(kw[i])))
            return false;
    return true;
}

inline bool syntaxIsKeyword(const std::string& tok, SyntaxLang lang) noexcept {
    static const char* kAsmOps[] = {
        "mov", "int", "jmp", "call", "ret", "push", "pop", "add", "sub", "cmp",
        "je", "jne", "jz", "jnz", "xor", "inc", "dec", "nop", "include", nullptr
    };
    static const char* kAsmDir[] = {
        ".model", ".code", ".data", "org", "db", "dw", "end", "public", "extrn", "equ", nullptr
    };
    static const char* kC[] = {
        "if", "else", "for", "while", "return", "void", "int", "char", "const",
        "static", "struct", "typedef", "include", "define", nullptr
    };
    static const char* kBasic[] = {
        "print", "dim", "let", "if", "then", "else", "for", "next", "while", "wend",
        "gosub", "return", "end", "rem", "def", "fn", nullptr
    };
    static const char* kPascal[] = {
        "program", "begin", "end", "var", "procedure", "function", "if", "then",
        "else", "while", "do", "repeat", "until", "writeln", nullptr
    };
    static const char* kField[] = {
        "field", "print", "return", "era", "pad", "nes", nullptr
    };
    static const char* kBatch[] = {
        "echo", "goto", "if", "else", "set", "call", "exit", "rem", "pause", nullptr
    };
    static const char* kIni[] = {
        "true", "false", "yes", "no", "on", "off", nullptr
    };
    auto hit = [&](const char** tbl) {
        for (int i = 0; tbl[i]; ++i)
            if (ciWordEq(tok, tbl[i])) return true;
        return false;
    };
    if (lang == SyntaxLang::Asm) return hit(kAsmOps) || hit(kAsmDir);
    if (lang == SyntaxLang::C) return hit(kC);
    if (lang == SyntaxLang::Basic) return hit(kBasic);
    if (lang == SyntaxLang::Pascal) return hit(kPascal);
    if (lang == SyntaxLang::Field) return hit(kField);
    if (lang == SyntaxLang::Batch) return hit(kBatch);
    if (lang == SyntaxLang::Ini) return hit(kIni);
    if (lang == SyntaxLang::Auto) return hit(kAsmOps) || hit(kC) || hit(kBasic);
    return false;
}

inline std::uint8_t syntaxAttrForToken(const std::string& tok, SyntaxLang lang) noexcept {
    if (lang == SyntaxLang::Asm) {
        if (syntaxIsKeyword(tok, SyntaxLang::Asm)) return ATTR_ASM;
        static const char* kDir[] = {
            ".model", ".code", ".data", "org", "db", "dw", "end", "public", "extrn", "equ", nullptr
        };
        for (int i = 0; kDir[i]; ++i)
            if (ciWordEq(tok, kDir[i])) return ATTR_DIRECTIVE;
    }
    if (lang == SyntaxLang::C) {
        if (syntaxIsKeyword(tok, SyntaxLang::C)) return ATTR_KW;
        if (tok.size() > 1 && tok.back() == 'f' && (tok == "0f" || tok.find('.') != std::string::npos))
            return ATTR_NUM;
    }
    if (lang == SyntaxLang::Field && syntaxIsKeyword(tok, SyntaxLang::Field)) return ATTR_KW;
    if (lang == SyntaxLang::Basic && syntaxIsKeyword(tok, SyntaxLang::Basic)) return ATTR_KW;
    if (lang == SyntaxLang::Pascal && syntaxIsKeyword(tok, SyntaxLang::Pascal)) return ATTR_KW;
    if (lang == SyntaxLang::Batch && syntaxIsKeyword(tok, SyntaxLang::Batch)) return ATTR_KW;
    if (lang == SyntaxLang::Ini && syntaxIsKeyword(tok, SyntaxLang::Ini)) return ATTR_TYPE;
    if (lang == SyntaxLang::Auto && syntaxIsKeyword(tok, SyntaxLang::Auto)) return ATTR_KW;
    bool allNum = !tok.empty();
    for (char c : tok) {
        if (!std::isdigit(static_cast<unsigned char>(c)) && c != '.' && c != 'h' && c != 'H'
            && c != 'x' && c != 'X')
            allNum = false;
    }
    if (allNum && !tok.empty()) return ATTR_NUM;
    return ATTR_IDENT;
}

inline SyntaxLang syntaxLangFromPath(const char* filePath) noexcept {
    if (!filePath) return SyntaxLang::Auto;
    const char* dot = std::strrchr(filePath, '.');
    if (!dot) return SyntaxLang::Auto;
    const char* ext = dot + 1;
    if (ciEq(ext, "ASM") || ciEq(ext, "INC") || ciEq(ext, "SYS")) return SyntaxLang::Asm;
    if (ciEq(ext, "C") || ciEq(ext, "H") || ciEq(ext, "CC") || ciEq(ext, "CPP")) return SyntaxLang::C;
    if (ciEq(ext, "BAS")) return SyntaxLang::Basic;
    if (ciEq(ext, "PAS")) return SyntaxLang::Pascal;
    if (ciEq(ext, "FLD")) return SyntaxLang::Field;
    if (ciEq(ext, "INI") || ciEq(ext, "CFG") || ciEq(ext, "JRN")) return SyntaxLang::Ini;
    if (ciEq(ext, "BAT") || ciEq(ext, "CMD") || ciEq(ext, "DO")) return SyntaxLang::Batch;
    return SyntaxLang::Auto;
}

inline const char* syntaxLangName(SyntaxLang lang) noexcept {
    switch (lang) {
    case SyntaxLang::Asm: return "MASM";
    case SyntaxLang::C: return "C";
    case SyntaxLang::Basic: return "BASIC";
    case SyntaxLang::Pascal: return "Pascal";
    case SyntaxLang::Field: return "Field";
    case SyntaxLang::Ini: return "INI";
    case SyntaxLang::Batch: return "Batch";
    default: return "Plain Text";
    }
}

inline std::uint8_t syntaxAttrAt(const std::string& ln, int col, SyntaxLang lang) noexcept {
    if (col < 0 || col >= static_cast<int>(ln.size())) return ATTR_EDITOR;
    const auto trimStart = [&]() {
        std::size_t i = 0;
        while (i < ln.size() && ln[i] == ' ') ++i;
        return i;
    };
    if (lang == SyntaxLang::Ini) {
        const auto ts = trimStart();
        if (ln.size() >= ts + 1 && ln[ts] == '[') return ATTR_DIRECTIVE;
        if (ln.find('=') != std::string::npos && col <= static_cast<int>(ln.find('=')))
            return ATTR_KW;
    }
    if (lang == SyntaxLang::Batch && ln.size() >= static_cast<std::size_t>(col + 1)
        && ln[static_cast<std::size_t>(col)] == '@')
        return ATTR_DIRECTIVE;
    if (ln.size() >= trimStart() + 1 && ln[trimStart()] == '\'') return ATTR_COMMENT;
    if (ln.size() >= static_cast<std::size_t>(col + 1) && ln[static_cast<std::size_t>(col)] == ';')
        return ATTR_COMMENT;
    if ((lang == SyntaxLang::C || lang == SyntaxLang::Field || lang == SyntaxLang::Auto)
        && ln.size() >= 2 && col + 1 < static_cast<int>(ln.size())
        && ln[static_cast<std::size_t>(col)] == '/' && ln[static_cast<std::size_t>(col + 1)] == '/')
        return ATTR_COMMENT;
    if (lang == SyntaxLang::Field && ln.size() >= 2
        && ln[static_cast<std::size_t>(col)] == '/' && col + 1 < static_cast<int>(ln.size())
        && ln[static_cast<std::size_t>(col + 1)] == '/')
        return ATTR_COMMENT;
    bool inStr = false;
    for (int i = 0; i < col; ++i)
        if (ln[static_cast<std::size_t>(i)] == '"') inStr = !inStr;
    if (inStr || ln[static_cast<std::size_t>(col)] == '"') return ATTR_STR;
    int w0 = 0, w1 = 0;
    wordBounds(ln, col, w0, w1);
    if (w1 <= w0) return ATTR_EDITOR;
    return syntaxAttrForToken(ln.substr(static_cast<std::size_t>(w0),
        static_cast<std::size_t>(w1 - w0)), lang);
}

inline void paintEditorLine(std::uint8_t* ram, int row, int colBase, const std::string& ln,
                            int cursorCol, bool cursorRow, SyntaxLang lang, int maxCols = 73) noexcept {
    int w0 = 0, w1 = 0;
    if (cursorRow) wordBounds(ln, cursorCol, w0, w1);
    for (int c = 0; c < maxCols; ++c) {
        char ch = c < static_cast<int>(ln.size()) ? ln[static_cast<std::size_t>(c)] : ' ';
        std::uint8_t attr = (c < static_cast<int>(ln.size()))
            ? syntaxAttrAt(ln, c, lang) : ATTR_EDITOR;
        if (cursorRow && c >= w0 && c < w1 && c != cursorCol)
            attr = ATTR_WORD;
        if (cursorRow && c == cursorCol)
            attr = ATTR_BLOCK;
        put(ram, row, colBase + c, ch, attr);
    }
}

// Modal text input — Open / Save As / Find (keyboard + mouse OK/Cancel)
struct ModalInput {
    bool active = false;
    int purpose = 0;  // 1=open 2=saveas 3=find
    int row0 = 8;
    int col0 = 18;
    int width = 44;
    std::string title;
    std::string value;
    int caret = 0;
};

inline ModalInput modal;

inline void openModal(int purpose, const char* title, const std::string& initial) noexcept {
    modal.active = true;
    modal.purpose = purpose;
    modal.title = title ? title : "";
    modal.value = initial;
    modal.caret = static_cast<int>(modal.value.size());
}

inline void closeModal() noexcept { modal = {}; }

inline void paintModal(std::uint8_t* ram) noexcept {
    if (!modal.active) return;
    const int h = 5;
    const int w = modal.width;
    drawFrame(ram, modal.row0, modal.col0, modal.row0 + h, modal.col0 + w,
        ATTR_FRAME, modal.title.c_str());
    fill(ram, modal.row0 + 2, ' ', ATTR_EDITOR);
    const int fieldCol = modal.col0 + 2;
    const int fieldW = w - 4;
    text(ram, modal.row0 + 2, fieldCol, modal.value.c_str(), ATTR_EDITOR, fieldW);
    if (modal.caret >= 0 && modal.caret < fieldW) {
        char ch = modal.caret < static_cast<int>(modal.value.size())
            ? modal.value[static_cast<std::size_t>(modal.caret)] : ' ';
        put(ram, modal.row0 + 2, fieldCol + modal.caret, ch, ATTR_BLOCK);
    }
    text(ram, modal.row0 + 3, modal.col0 + 2, " Enter=OK  Esc=Cancel ", ATTR_DIM, w - 4);
    const int okCol = modal.col0 + 4;
    const int cxCol = modal.col0 + w - 14;
    text(ram, modal.row0 + 4, okCol, " OK ", ATTR_MENU_SEL);
    text(ram, modal.row0 + 4, cxCol, " Cancel ", ATTR_MENU);
}

inline bool modalHit(int row, int col, bool& okOut) noexcept {
    if (!modal.active) return false;
    const int h = 5;
    const int w = modal.width;
    if (row < modal.row0 || row > modal.row0 + h || col < modal.col0 || col > modal.col0 + w)
        return false;
    if (row == modal.row0 + 4) {
        const int okCol = modal.col0 + 4;
        const int cxCol = modal.col0 + w - 14;
        if (col >= okCol && col < okCol + 4) { okOut = true; return true; }
        if (col >= cxCol && col < cxCol + 8) { okOut = false; return true; }
    }
    if (row == modal.row0 + 2 && col >= modal.col0 + 2 && col < modal.col0 + w - 2) {
        modal.caret = std::clamp(col - (modal.col0 + 2), 0,
            static_cast<int>(modal.value.size()));
        okOut = false;
        return true;
    }
    okOut = false;
    return true;
}

inline bool pumpModalKey(std::uint8_t* ram, std::uint16_t key) noexcept {
    if (!modal.active) return false;
    if (key == 0x011Bu) { closeModal(); paintModal(ram); return true; }
    if (key == 0x1C0Du) { return false; }  // caller handles OK
    if (key == 0x0E08u) {
        if (modal.caret > 0) {
            modal.value.erase(static_cast<std::size_t>(modal.caret - 1), 1);
            --modal.caret;
        }
        paintModal(ram);
        return true;
    }
    if (key == 0x4B00u && modal.caret > 0) { --modal.caret; paintModal(ram); return true; }
    if (key == 0x4D00u && modal.caret < static_cast<int>(modal.value.size())) {
        ++modal.caret; paintModal(ram); return true;
    }
    const char ch = static_cast<char>(key & 0xFFu);
    if (ch >= 32 && ch < 127) {
        modal.value.insert(static_cast<std::size_t>(modal.caret), 1, ch);
        ++modal.caret;
        paintModal(ram);
        return true;
    }
    return true;
}

} // namespace FieldRtxGui