#pragma once

// AmmoCode — Turbo Pascal 7-style master IDE (mouse + Alt keys + ASM/BASIC debugger).

#include "FieldAmmoCc.hpp"
#include "FieldAmmoDbg.hpp"
#include "FieldAmmoDisasm.hpp"
#include "FieldAmmoRun.hpp"
#include "FieldAmmoTools.hpp"
#include "FieldPlatform.hpp"
#include "FieldAmmoVfs.hpp"
#include "FieldRtxBasic.hpp"
#include "FieldRtxBasicTextbook.hpp"
#include "FieldFieldCc.hpp"
#include "FieldRuntimeInfo.hpp"
#include "FieldRtxGui.hpp"
#include "FieldRtxMouse.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace FieldAmmoCode {

enum class Lang : std::uint8_t { Basic, Asm, C, Pascal, Field };
enum class Era : std::uint8_t { Dos, Win16, Rtx, Any };

constexpr int MENU_ROW = 0;
constexpr int TOOL_ROW = 1;
constexpr int ED_TOP = 2;
constexpr int ED_ROWS = 16;
constexpr int ED_COL = 6;
constexpr int ED_W = 52;
constexpr int MINI_SEP0 = 59;
constexpr int MINI_SEP1 = 60;
constexpr int MINI_COL = 62;
constexpr int MINI_W = 18;
constexpr int MSG_TOP = 19;
constexpr int MSG_ROWS = 4;
constexpr int FKEY_ROW = 23;
constexpr int STATUS_ROW = 24;

enum Act : int {
    A_NEW = 1, A_OPEN, A_SAVE, A_SAVEAS, A_CLOSE, A_EXIT,
    A_FIND = 10,
    A_RUN = 20, A_STEP, A_TRACE,
    A_COMPILE = 30, A_BUILD, A_LINK,
    A_BP = 40, A_WATCH, A_REGS, A_ERA,
    A_LANG_BAS = 50, A_LANG_ASM, A_LANG_C,
    A_HELP = 60, A_TEXTBOOK, A_ABOUT,
};

inline bool active = false;
inline Lang lang = Lang::Basic;
inline Era era = Era::Any;
inline bool insertMode = true;
inline bool helpOpen = false;
inline bool asmDebug = false;
inline bool traceAll = false;
inline bool dirty = false;
inline int editRow = 0;
inline int editCol = 0;
inline int editTop = 0;
inline int runPc = 0;
inline int helpChapter = 0;
inline int helpPage = 0;
inline std::string path = "C:\\AMMOCODE\\UNTITLED.BAS";
inline std::vector<std::string> source;
inline std::vector<std::string> messages;
inline std::vector<bool> breakpoints;
inline FieldRtxGui::MenuBarState menuSt;
inline int menuCols[12]{};
inline FieldAmmoRun::Cpu asmCpu;
inline std::string findNeedle;

using EchoFn = FieldRtxBasic::EchoFn;
using NewlineFn = FieldRtxBasic::NewlineFn;

static const FieldRtxGui::MenuItem kFile[] = {
    { FieldRtxGui::ICO_FOLDER, "New",        'N', A_NEW },
    { FieldRtxGui::ICO_FOLDER, "Open...",    'O', A_OPEN },
    { FieldRtxGui::ICO_DISK,  "Save",       'S', A_SAVE },
    { FieldRtxGui::ICO_DISK,  "Save As...", 'A', A_SAVEAS },
    { FieldRtxGui::ICO_STOP,  "Close",      'C', A_CLOSE },
    { FieldRtxGui::ICO_STOP,  "Exit  Alt+X",'X', A_EXIT },
};
static const FieldRtxGui::MenuItem kEdit[] = {
    { FieldRtxGui::BOX_X, "Find...", 'F', A_FIND },
};
static const FieldRtxGui::MenuItem kSearch[] = {
    { FieldRtxGui::BOX_X, "Find next", 'N', A_FIND },
};
static const FieldRtxGui::MenuItem kRun[] = {
    { FieldRtxGui::ICO_PLAY, "Run (F5)",      'R', A_RUN },
    { FieldRtxGui::ICO_STEP, "Step (F8)",    'S', A_STEP },
    { FieldRtxGui::ICO_BREAK,"Trace all",    'T', A_TRACE },
};
static const FieldRtxGui::MenuItem kCompile[] = {
    { FieldRtxGui::ICO_DISK, "Compile (F9)", 'C', A_COMPILE },
    { FieldRtxGui::ICO_DISK, "Build",        'B', A_BUILD },
    { FieldRtxGui::ICO_DISK, "Link",         'L', A_LINK },
};
static const FieldRtxGui::MenuItem kDebug[] = {
    { FieldRtxGui::ICO_BREAK, "Breakpoint F9", 'B', A_BP },
    { FieldRtxGui::BOX_X,     "Watch",         'W', A_WATCH },
    { FieldRtxGui::BOX_X,     "Registers",     'R', A_REGS },
};
static const FieldRtxGui::MenuItem kOptions[] = {
    { FieldRtxGui::ICO_PLAY, "Era: DOS",      'D', A_ERA },
    { FieldRtxGui::ICO_BOOK, "Lang: BASIC",   'B', A_LANG_BAS },
    { FieldRtxGui::ICO_BOOK, "Lang: ASM",     'A', A_LANG_ASM },
    { FieldRtxGui::ICO_BOOK, "Lang: C",       'C', A_LANG_C },
};
static const FieldRtxGui::MenuItem kWindow[] = {
    { FieldRtxGui::BOX_X, "Messages", 'M', 0 },
};
static const FieldRtxGui::MenuItem kHelp[] = {
    { FieldRtxGui::ICO_BOOK, "Contents F1",  'C', A_HELP },
    { FieldRtxGui::ICO_BOOK, "Textbook",     'T', A_TEXTBOOK },
    { FieldRtxGui::ICO_BLOCK,"About",        'A', A_ABOUT },
};

static const FieldRtxGui::Menu kMenus[] = {
    { FieldRtxGui::ICO_FOLDER, "File",    kFile,    6 },
    { FieldRtxGui::BOX_X,      "Edit",    kEdit,    1 },
    { FieldRtxGui::BOX_X,      "Search",  kSearch,  1 },
    { FieldRtxGui::ICO_PLAY,   "Run",     kRun,     3 },
    { FieldRtxGui::ICO_DISK,   "Compile", kCompile, 3 },
    { FieldRtxGui::ICO_BREAK,  "Debug",   kDebug,   3 },
    { FieldRtxGui::BOX_X,      "Options", kOptions, 4 },
    { FieldRtxGui::BOX_X,      "Window",  kWindow,  1 },
    { FieldRtxGui::ICO_BOOK,   "Help",    kHelp,    3 },
};
constexpr int kMenuCount = 9;

static const FieldRtxGui::TpMenuEntry kAltMap[] = {
    { 'F', 0 }, { 'E', 1 }, { 'S', 2 }, { 'R', 3 }, { 'C', 4 },
    { 'D', 5 }, { 'O', 6 }, { 'W', 7 }, { 'H', 8 },
};

inline void logMsg(const std::string& s) {
    messages.push_back(s);
    if (messages.size() > 60) messages.erase(messages.begin());
}

inline void detectLangFromPath() noexcept {
    const auto dot = path.find_last_of('.');
    if (dot == std::string::npos) return;
    std::string ext = path.substr(dot);
    for (auto& c : ext) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (ext == ".ASM") lang = Lang::Asm;
    else if (ext == ".BAS") lang = Lang::Basic;
    else if (ext == ".PAS") lang = Lang::Pascal;
    else if (ext == ".FLD") lang = Lang::Field;
    else if (ext == ".C") lang = Lang::C;
}

inline bool isMachineLang() noexcept {
    return lang == Lang::Asm || lang == Lang::C || lang == Lang::Field;
}

inline bool loadSource(const char* p) {
    source.clear();
    std::vector<std::uint8_t> data;
    if (!FieldAmmoVfs::readPath(p, data)) {
        source.push_back("");
        return false;
    }
    std::string line;
    for (std::uint8_t b : data) {
        if (b == '\r') continue;
        if (b == '\n') { source.push_back(line); line.clear(); }
        else if (b >= 32 && b < 127) line += static_cast<char>(b);
    }
    if (!line.empty()) source.push_back(line);
    if (source.empty()) source.push_back("");
    path = p;
    detectLangFromPath();
    dirty = false;
    editRow = editCol = editTop = runPc = 0;
    return true;
}

inline bool saveSource() {
    std::string body;
    for (std::size_t i = 0; i < source.size(); ++i) {
        body += source[i];
        if (i + 1 < source.size()) body += "\r\n";
    }
    dirty = false;
    return FieldAmmoVfs::writePath(path.c_str(),
        reinterpret_cast<const std::uint8_t*>(body.data()), body.size());
}

inline void syncBasicProgram() {
    FieldRtxBasic::program = source;
    FieldRtxBasic::vars.clear();
}

inline void ensureBp() {
    breakpoints.resize(source.size(), false);
}

inline void closeIde(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept; // fwd

inline void closeIdeImpl(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    active = false;
    FieldRtxBasic::active = false;
    asmDebug = false;
    helpOpen = false;
    menuSt = {};
    FieldRtxGui::closeModal();
    FieldRtxGui::initTextMode(ram);
    FieldRtxBasic::shellPrint(ram, echo, nl, "\r\nAmmoCode closed.\r\n");
}

inline bool findNext(const std::string& needle, bool fromCaret) noexcept {
    if (needle.empty()) return false;
    int startRow = editRow;
    int startCol = fromCaret ? editCol : 0;
    for (int pass = 0; pass < 2; ++pass) {
        for (int r = startRow; r < static_cast<int>(source.size()); ++r) {
            const std::string& ln = source[static_cast<std::size_t>(r)];
            const int c0 = (r == startRow) ? startCol : 0;
            const auto pos = ln.find(needle, static_cast<std::size_t>(c0));
            if (pos != std::string::npos) {
                editRow = r;
                editCol = static_cast<int>(pos);
                if (editRow < editTop) editTop = editRow;
                if (editRow >= editTop + ED_ROWS) editTop = editRow - ED_ROWS + 1;
                return true;
            }
        }
        startRow = 0;
        startCol = 0;
    }
    return false;
}

inline void closeIde(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    closeIdeImpl(ram, echo, nl);
}

inline void paintHelp(std::uint8_t* ram) noexcept {
    const int start = FieldRtxBasicTextbook::chapterStart(helpChapter);
    const int total = FieldRtxBasicTextbook::chapterLineCount(helpChapter);
    const int top = helpPage * 12;
    char hdr[48];
    std::snprintf(hdr, sizeof hdr, "%s", FieldRtxBasicTextbook::kChapterNames[helpChapter]);
    FieldRtxGui::drawFrame(ram, 5, 3, 20, 76, FieldRtxGui::ATTR_FRAME, hdr);
    for (int r = 6; r < 20; ++r) {
        FieldRtxGui::fill(ram, r, ' ', FieldRtxGui::ATTR_HELP);
        const int li = top + (r - 6);
        if (li < total && FieldRtxBasicTextbook::kLines[start + li])
            FieldRtxGui::text(ram, r, 5, FieldRtxBasicTextbook::kLines[start + li],
                FieldRtxGui::ATTR_HELP, 70);
    }
}

inline const char* eraName() noexcept {
    switch (era) {
    case Era::Dos: return "DOS";
    case Era::Win16: return "WIN16";
    case Era::Rtx: return "RTX";
    default: return "ANY";
    }
}

inline const char* langName() noexcept {
    switch (lang) {
    case Lang::Asm: return "ASM";
    case Lang::C: return "C";
    case Lang::Pascal: return "PAS";
    case Lang::Field: return "FLD";
    default: return "BASIC";
    }
}

inline FieldRtxGui::SyntaxLang syntaxLang() noexcept {
    switch (lang) {
    case Lang::Asm: return FieldRtxGui::SyntaxLang::Asm;
    case Lang::C: return FieldRtxGui::SyntaxLang::Pascal;
    case Lang::Pascal: return FieldRtxGui::SyntaxLang::Pascal;
    case Lang::Field: return FieldRtxGui::SyntaxLang::Field;
    default: return FieldRtxGui::SyntaxLang::Basic;
    }
}

inline char minimapDensityChar(const std::string& ln, FieldRtxGui::SyntaxLang sl) noexcept {
    if (ln.empty()) return '.';
    int alpha = 0, sym = 0;
    for (char c : ln) {
        if (std::isalpha(static_cast<unsigned char>(c))) ++alpha;
        else if (c == '"' || c == '\'') ++sym;
        else if (c == ';' || c == '#') return ';';
    }
    if (sl == FieldRtxGui::SyntaxLang::Asm && ln.size() >= 3) {
        const std::string u = FieldRtxGui::ciUpper(ln.substr(0, 3));
        if (u == "MOV" || u == "ADD" || u == "SUB" || u == "JMP") return '\xDB';
    }
    if (alpha > static_cast<int>(ln.size()) / 2) return '\xB2';
    if (sym > 0) return '\xB1';
    return '\xB0';
}

inline void paintMinimap(std::uint8_t* ram) noexcept {
    const int total = std::max(1, static_cast<int>(source.size()));
    const float rowH = static_cast<float>(total) / static_cast<float>(ED_ROWS);
    for (int vis = 0; vis < ED_ROWS; ++vis) {
        const int row = ED_TOP + vis;
        FieldRtxGui::put(ram, row, MINI_SEP0, FieldRtxGui::BOX_V, FieldRtxGui::ATTR_FRAME);
        FieldRtxGui::put(ram, row, MINI_SEP1, FieldRtxGui::BOX_V, FieldRtxGui::ATTR_FRAME);
        const int line0 = static_cast<int>(vis * rowH);
        const int line1 = static_cast<int>((vis + 1) * rowH);
        const bool inView = editTop >= line0 && editTop < line1;
        const std::uint8_t rowAttr = inView ? FieldRtxGui::ATTR_WORD : FieldRtxGui::ATTR_DIM;
        for (int mc = 0; mc < MINI_W; ++mc)
            FieldRtxGui::put(ram, row, MINI_COL + mc, ' ', rowAttr);
        if (vis == 0)
            FieldRtxGui::text(ram, row, MINI_COL, "Micro", FieldRtxGui::ATTR_RTX, 5);
        for (int li = line0; li < line1 && li < total; ++li) {
            const int slot = MINI_COL + ((li - line0) * MINI_W) / std::max(1, line1 - line0);
            if (slot >= MINI_COL + MINI_W) break;
            const char ch = minimapDensityChar(source[static_cast<std::size_t>(li)], syntaxLang());
            std::uint8_t attr = FieldRtxGui::ATTR_DIM;
            if (li == editRow) attr = FieldRtxGui::ATTR_GOLD;
            else if (li < static_cast<int>(breakpoints.size()) && breakpoints[static_cast<std::size_t>(li)])
                attr = FieldRtxGui::ATTR_BREAK;
            FieldRtxGui::put(ram, row, slot, ch, attr);
        }
        if (inView) {
            const int caretCol = MINI_COL + (editRow - line0) * MINI_W / std::max(1, line1 - line0);
            FieldRtxGui::put(ram, row, std::min(MINI_COL + MINI_W - 1, caretCol),
                '\xDB', FieldRtxGui::ATTR_CURSOR);
        }
    }
    FieldRtxGui::text(ram, TOOL_ROW, MINI_COL, "MINIMAP", FieldRtxGui::ATTR_RTX, 8);
}

inline void paint(std::uint8_t* ram) noexcept {
    FieldRtxGui::initTextMode(ram);
    char masterTitle[64];
    std::snprintf(masterTitle, sizeof masterTitle, " %.54s ",
        FieldRuntimeInfo::masterStatusLine());
    FieldRtxGui::paintMenuBarTp(ram, MENU_ROW, kMenus, kMenuCount, menuSt,
        masterTitle, menuCols, kMenuCount);

    FieldRtxGui::fill(ram, TOOL_ROW, ' ', FieldRtxGui::ATTR_DIM);
    FieldRtxGui::text(ram, TOOL_ROW, 1,
        " AmmoCode TP7 | F5 Run F8 Step | micro-zoom minimap -> | beats VSCode ",
        FieldRtxGui::ATTR_DIM, MINI_SEP0);

    ensureBp();
    for (int vis = 0; vis < ED_ROWS; ++vis) {
        const int row = ED_TOP + vis;
        const int pi = editTop + vis;
        FieldRtxGui::fill(ram, row, ' ', FieldRtxGui::ATTR_EDITOR);
        if (pi >= 0 && pi < static_cast<int>(source.size())) {
            FieldRtxGui::put(ram, row, 0,
                (pi < static_cast<int>(breakpoints.size()) && breakpoints[static_cast<std::size_t>(pi)])
                    ? FieldRtxGui::ICO_BREAK : ' ',
                FieldRtxGui::ATTR_BREAK);
            char num[12];
            std::snprintf(num, sizeof num, "%4d", pi + 1);
            FieldRtxGui::text(ram, row, 1, num, FieldRtxGui::ATTR_GOLD, 5);
            const std::string& ln = source[static_cast<std::size_t>(pi)];
            if (asmDebug && pi == runPc) {
                for (int c = 0; c < ED_W; ++c) {
                    char ch = c < static_cast<int>(ln.size()) ? ln[static_cast<std::size_t>(c)] : ' ';
                    FieldRtxGui::put(ram, row, ED_COL + c, ch, FieldRtxGui::ATTR_DEBUG);
                }
            } else if (pi == editRow && !helpOpen && menuSt.openMenu < 0) {
                FieldRtxGui::paintEditorLine(ram, row, ED_COL, ln, editCol, true, syntaxLang(), ED_W);
            } else {
                FieldRtxGui::paintEditorLine(ram, row, ED_COL, ln, -1, false, syntaxLang(), ED_W);
            }
        }
    }
    paintMinimap(ram);

    FieldRtxGui::drawHLine(ram, 18, 0, MINI_SEP0, FieldRtxGui::ATTR_FRAME);
    FieldRtxGui::text(ram, 18, 1, " Messages ", FieldRtxGui::ATTR_TITLE, 12);
    const int msgStart = std::max(0, static_cast<int>(messages.size()) - MSG_ROWS);
    for (int vis = 0; vis < MSG_ROWS; ++vis) {
        const int row = MSG_TOP + vis;
        FieldRtxGui::fill(ram, row, ' ', FieldRtxGui::ATTR_IMMED);
        const int mi = msgStart + vis;
        if (mi < static_cast<int>(messages.size()))
            FieldRtxGui::text(ram, row, 1, messages[static_cast<std::size_t>(mi)].c_str(),
                FieldRtxGui::ATTR_IMMED, 78);
    }

    FieldRtxGui::fill(ram, FKEY_ROW, ' ', FieldRtxGui::ATTR_DIM);
    FieldRtxGui::text(ram, FKEY_ROW, 1,
        " F1 Help F2 Save F3 Open F5 Run F8 Step F9 Compile F10 Menu Alt+Letter ",
        FieldRtxGui::ATTR_DIM, 78);

    FieldRtxGui::fill(ram, STATUS_ROW, ' ', FieldRtxGui::ATTR_STATUS);
    char st[56];
    std::snprintf(st, sizeof st, " %d:%d %s %s %s ",
        editRow + 1, editCol + 1, insertMode ? "INS" : "OVR", langName(), eraName());
    FieldRtxGui::text(ram, STATUS_ROW, 0, st, FieldRtxGui::ATTR_STATUS, 40);
    std::string fn = path;
    if (fn.size() > 26) fn = "..." + fn.substr(fn.size() - 23);
    if (dirty) fn += '*';
    FieldRtxGui::text(ram, STATUS_ROW, 80 - static_cast<int>(fn.size()) - 1,
        fn.c_str(), FieldRtxGui::ATTR_STATUS, 30);

    if (menuSt.openMenu >= 0)
        FieldRtxGui::paintDropDown(ram, 1, menuCols[menuSt.openMenu],
            kMenus[menuSt.openMenu], menuSt.selItem);
    if (helpOpen) paintHelp(ram);
    FieldRtxGui::paintModal(ram);
}

inline void applyModalOk(std::uint8_t* ram) noexcept {
    auto& m = FieldRtxGui::modal;
    if (!m.active) return;
    const int p = m.purpose;
    const std::string v = m.value;
    FieldRtxGui::closeModal();
    if (p == 1) {
        if (!v.empty()) {
            if (loadSource(v.c_str())) logMsg("Opened " + path);
            else logMsg("Open failed: " + v);
        }
    } else if (p == 2) {
        if (!v.empty()) {
            path = v;
            detectLangFromPath();
            if (saveSource()) logMsg("Saved " + path);
            else logMsg("Save failed");
        }
    } else if (p == 3) {
        findNeedle = v;
        if (!findNeedle.empty()) {
            if (findNext(findNeedle, true)) logMsg("Found");
            else logMsg("Not found: " + findNeedle);
        }
    }
    paint(ram);
}

inline void doCompile(std::uint8_t* ram) noexcept {
    saveSource();
    if (lang == Lang::C) {
        auto cr = FieldAmmoCc::compilePath(path.c_str());
        if (!cr.ok) { logMsg("AMMOCC: " + cr.error); paint(ram); return; }
        const std::string obj = FieldAmmoTools::swapExt(path, ".OBJ");
        if (!FieldAmmoVfs::writePath(obj.c_str(), cr.asmResult.object.data(), cr.asmResult.object.size())) {
            logMsg("AMMOCC write failed"); paint(ram); return;
        }
        logMsg("AMMOCC → " + obj);
        auto lr = FieldAmmoTools::linkFile({"AMMOLINK", obj});
        logMsg(lr.ok ? lr.message : ("LINK: " + lr.message));
    } else if (lang == Lang::Field) {
        auto fr = FieldFieldCc::compilePath(path.c_str());
        if (!fr.ok) { logMsg("FIELDC: " + fr.error); paint(ram); return; }
        const std::string obj = FieldAmmoTools::swapExt(path, ".OBJ");
        if (!FieldAmmoVfs::writePath(obj.c_str(), fr.asmResult.object.data(), fr.asmResult.object.size())) {
            logMsg("FIELDC write failed"); paint(ram); return;
        }
        logMsg("FIELDC → " + obj);
        auto lr = FieldAmmoTools::linkFile({"AMMOLINK", obj});
        logMsg(lr.ok ? lr.message : ("LINK: " + lr.message));
    } else if (lang == Lang::Asm) {
        auto ar = FieldAmmoTools::asmFile({"AMMOASM", path});
        if (!ar.ok) { logMsg("ASM: " + ar.message); paint(ram); return; }
        logMsg(ar.message);
        const std::string obj = FieldAmmoTools::swapExt(path, ".OBJ");
        auto lr = FieldAmmoTools::linkFile({"AMMOLINK", obj});
        logMsg(lr.ok ? lr.message : ("LINK: " + lr.message));
    } else {
        syncBasicProgram();
        std::vector<std::string> errs;
        const int n = FieldRtxBasic::compileCheck(errs);
        if (n == 0) logMsg("Compile: 0 errors — " + std::string(langName()) + " ready.");
        else for (const auto& e : errs) logMsg(e);
    }
    paint(ram);
}

inline void doMachineRun(std::uint8_t* ram, EchoFn echo, NewlineFn nl, bool step) noexcept {
    const std::string com = FieldAmmoTools::swapExt(path, ".COM");
    if (!asmDebug) {
        if (!FieldAmmoDbg::loadSession(ram, com.c_str())) {
            logMsg("Debug: cannot load " + com);
            paint(ram);
            return;
        }
        asmCpu = FieldAmmoDbg::session.cpu;
        asmDebug = true;
        runPc = editRow;
    }
    auto r = step
        ? FieldAmmoDbg::stepSession(ram, echo, nl, 1u)
        : FieldAmmoDbg::goSession(ram, echo, nl, 65536u);
    if (!r.ok) {
        logMsg("Debug: " + r.error);
        paint(ram);
        return;
    }
    asmCpu = r.cpu;
    char buf[96];
    std::snprintf(buf, sizeof buf, "IP=%04X AX=%04X steps=%u",
        asmCpu.ip & 0xFFFFu, asmCpu.ax, r.steps);
    logMsg(buf);
    const auto ins = FieldAmmoDisasm::decodeAt(ram, asmCpu.ip, FieldPlatform::GUEST_RAM_BYTES);
    if (ins.len) logMsg(std::string("  ") + ins.text);
    if (asmCpu.halted) {
        asmDebug = false;
        logMsg("Program halted");
    }
    paint(ram);
}

inline void doBasicRun(std::uint8_t* ram, EchoFn echo, NewlineFn nl, bool step) noexcept {
    syncBasicProgram();
    FieldRtxBasic::active = true;
    if (runPc >= static_cast<int>(FieldRtxBasic::program.size())) runPc = 0;
    int pc = runPc;
    if (step) {
        if (pc < static_cast<int>(FieldRtxBasic::program.size()))
            FieldRtxBasic::runProgram(ram, echo, nl, pc, true, pc);
        runPc = pc;
        logMsg("BASIC step");
    } else {
        while (pc < static_cast<int>(FieldRtxBasic::program.size())) {
            if (traceAll && pc > 0) break;
            if (!FieldRtxBasic::runProgram(ram, echo, nl, pc, true, pc)) break;
            if (!traceAll && pc < static_cast<int>(breakpoints.size())
                && breakpoints[static_cast<std::size_t>(pc)]) break;
        }
        runPc = pc;
        logMsg("BASIC run paused");
    }
    FieldRtxBasic::active = active;
    paint(ram);
}

inline void dispatch(int act, std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    menuSt.openMenu = -1;
    switch (act) {
    case A_NEW:
        source = { "" }; breakpoints.clear(); editRow = 0; dirty = true;
        logMsg("New file");
        break;
    case A_OPEN:
        FieldRtxGui::openModal(1, " Open File ", path);
        paint(ram);
        return;
    case A_SAVE:
        if (saveSource()) logMsg("Saved " + path);
        else logMsg("Save failed");
        break;
    case A_SAVEAS:
        FieldRtxGui::openModal(2, " Save As ", path);
        paint(ram);
        return;
    case A_FIND:
        FieldRtxGui::openModal(3, " Find ", findNeedle);
        paint(ram);
        return;
    case A_CLOSE:
    case A_EXIT:
        closeIde(ram, echo, nl);
        return;
    case A_COMPILE:
    case A_BUILD:
        doCompile(ram);
        return;
    case A_LINK:
        doCompile(ram);
        return;
    case A_RUN:
        if (isMachineLang()) doMachineRun(ram, echo, nl, false);
        else doBasicRun(ram, echo, nl, false);
        return;
    case A_STEP:
        if (isMachineLang()) doMachineRun(ram, echo, nl, true);
        else doBasicRun(ram, echo, nl, true);
        return;
    case A_TRACE:
        traceAll = !traceAll;
        logMsg(traceAll ? "Trace ON" : "Trace OFF");
        break;
    case A_BP:
        ensureBp();
        if (editRow < static_cast<int>(breakpoints.size()))
            breakpoints[static_cast<std::size_t>(editRow)] =
                !breakpoints[static_cast<std::size_t>(editRow)];
        logMsg("Breakpoint toggled");
        break;
    case A_WATCH:
        syncBasicProgram();
        if (FieldRtxBasic::vars.empty()) logMsg("Watch: empty");
        else for (const auto& kv : FieldRtxBasic::vars) {
            char b[40];
            std::snprintf(b, sizeof b, "  %s=%g", kv.first.c_str(), kv.second);
            logMsg(b);
        }
        break;
    case A_REGS:
        if (asmDebug) {
            char b[72];
            std::snprintf(b, sizeof b, "AX=%04X BX=%04X CX=%04X DX=%04X IP=%04X",
                asmCpu.ax, asmCpu.bx, asmCpu.cx, asmCpu.dx, asmCpu.ip & 0xFFFFu);
            logMsg(b);
        } else logMsg("Registers: run ASM debug first");
        break;
    case A_ERA:
        era = static_cast<Era>((static_cast<int>(era) + 1) % 4);
        logMsg(std::string("Era ") + eraName());
        break;
    case A_LANG_BAS: lang = Lang::Basic; logMsg("Language BASIC"); break;
    case A_LANG_ASM: lang = Lang::Asm; logMsg("Language ASM"); break;
    case A_LANG_C: lang = Lang::C; logMsg("Language C (AMMOCC)"); break;
    case A_HELP:
        helpChapter = 0; helpPage = 0; helpOpen = true;
        break;
    case A_TEXTBOOK:
        helpChapter = 0; helpPage = 0; helpOpen = true;
        break;
    case A_ABOUT:
        helpChapter = 7; helpPage = 0; helpOpen = true;
        break;
    default: break;
    }
    paint(ram);
}

inline void open(std::uint8_t* ram, Lang l, const char* filePath) noexcept {
    active = true;
    FieldRtxBasic::active = true;
    lang = l;
    menuSt = {};
    messages.clear();
    helpOpen = false;
    asmDebug = false;
    if (filePath && filePath[0]) loadSource(filePath);
    else {
        source = { "" };
        if (l == Lang::Asm) path = "C:\\AMMOCODE\\UNTITLED.ASM";
        else if (l == Lang::C) path = "C:\\AMMOCODE\\UNTITLED.C";
        else if (l == Lang::Field) path = "C:\\AMMOCODE\\UNTITLED.FLD";
        else path = "C:\\QBASIC\\UNTITLED.BAS";
    }
    logMsg("AmmoCode — Turbo Pascal IDE — mouse + Alt menus");
    paint(ram);
}

inline bool handleMouse(std::uint8_t* ram, const FieldRtxMouse::Frame& m,
                        EchoFn echo, NewlineFn nl) noexcept {
    if (!active || !m.visible) return false;
    if (helpOpen) return true;

    if (FieldRtxGui::modal.active && m.leftClick) {
        bool ok = false;
        if (FieldRtxGui::modalHit(m.row, m.col, ok)) {
            if (m.row == FieldRtxGui::modal.row0 + 4) {
                if (ok) applyModalOk(ram);
                else { FieldRtxGui::closeModal(); paint(ram); }
            } else
                paint(ram);
            return true;
        }
    }
    if (FieldRtxGui::modal.active) {
        FieldRtxMouse::paintPointer(ram, m.col, m.row);
        return true;
    }

    if (m.leftClick) {
        if (m.row == MENU_ROW) {
            const int hit = FieldRtxGui::hitMenuBar(menuCols, kMenuCount, m.col, m.row);
            if (hit >= 0) {
                menuSt.openMenu = hit;
                menuSt.selItem = 0;
                paint(ram);
                return true;
            }
        }
        if (menuSt.openMenu >= 0) {
            const int item = FieldRtxGui::hitDropDown(kMenus[menuSt.openMenu],
                menuCols[menuSt.openMenu], 1, m.col, m.row);
            if (item >= 0) {
                dispatch(kMenus[menuSt.openMenu].items[item].actionId, ram, echo, nl);
                return true;
            }
            menuSt.openMenu = -1;
            paint(ram);
            return true;
        }
        if (m.col >= MINI_COL && m.row >= ED_TOP && m.row < ED_TOP + ED_ROWS) {
            const int total = std::max(1, static_cast<int>(source.size()));
            const float rowH = static_cast<float>(total) / static_cast<float>(ED_ROWS);
            const int vis = m.row - ED_TOP;
            const int line0 = static_cast<int>(vis * rowH);
            const int line1 = static_cast<int>((vis + 1) * rowH);
            const int slot = m.col - MINI_COL;
            const int pick = line0 + (slot * std::max(1, line1 - line0)) / MINI_W;
            editRow = std::clamp(pick, 0, total - 1);
            editTop = std::clamp(editRow - ED_ROWS / 2, 0, std::max(0, total - ED_ROWS));
            editCol = 0;
            paint(ram);
            return true;
        }
        if (m.row >= ED_TOP && m.row < ED_TOP + ED_ROWS) {
            editRow = editTop + (m.row - ED_TOP);
            editCol = std::max(0, m.col - ED_COL);
            if (m.col < 2) {
                ensureBp();
                if (editRow < static_cast<int>(breakpoints.size()))
                    breakpoints[static_cast<std::size_t>(editRow)] =
                        !breakpoints[static_cast<std::size_t>(editRow)];
            }
            if (editRow >= static_cast<int>(source.size())) {
                while (editRow >= static_cast<int>(source.size())) source.push_back("");
            }
            editCol = std::min(editCol, static_cast<int>(
                source[static_cast<std::size_t>(editRow)].size()));
            paint(ram);
            return true;
        }
    }
    if (m.wheel != 0 && m.row >= ED_TOP && m.row < ED_TOP + ED_ROWS) {
        if (m.wheel > 0) editTop = std::max(0, editTop - 1);
        else editTop = std::min(std::max(0, static_cast<int>(source.size()) - ED_ROWS), editTop + 1);
        paint(ram);
        return true;
    }
    FieldRtxMouse::paintPointer(ram, m.col, m.row);
    return true;
}

inline bool handleKey(std::uint8_t* ram, std::uint16_t key,
                      EchoFn echo, NewlineFn nl) noexcept {
    if (!active) return false;

    if (FieldRtxGui::modal.active) {
        if (key == 0x1C0Du) { applyModalOk(ram); return true; }
        if (FieldRtxGui::pumpModalKey(ram, key)) return true;
    }

    char altCh = 0;
    if (FieldRtxGui::isAltMenuKey(key, altCh)) {
        if (altCh == 'X') { dispatch(A_EXIT, ram, echo, nl); return true; }
        const int mi = FieldRtxGui::altLetterToMenu(altCh, kAltMap, 9);
        if (mi >= 0) {
            menuSt.openMenu = mi;
            menuSt.selItem = 0;
            paint(ram);
            return true;
        }
    }

    if (helpOpen) {
        if (key == 0x011Bu || key == 0x3B00u) { helpOpen = false; paint(ram); }
        else if (key == 0x4900u && helpPage > 0) { --helpPage; paint(ram); }
        else if (key == 0x5100u) { ++helpPage; paint(ram); }
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

    if (menuSt.active > 0) {
        if (key == 0x011Bu) menuSt.active = 0;
        else if (key == 0x4B00u && menuSt.active > 1) --menuSt.active;
        else if (key == 0x4D00u && menuSt.active < kMenuCount) ++menuSt.active;
        else if (key == 0x5000u || key == 0x1C0Du) {
            menuSt.openMenu = menuSt.active - 1;
            menuSt.selItem = 0;
        }
        paint(ram);
        return true;
    }

    switch (key) {
    case 0x4400u: menuSt.active = 1; paint(ram); return true;
    case 0x3B00u: dispatch(A_HELP, ram, echo, nl); return true;
    case 0x3C00u: dispatch(A_SAVE, ram, echo, nl); return true;
    case 0x3D00u: dispatch(A_OPEN, ram, echo, nl); return true;
    case 0x3F00u: dispatch(A_RUN, ram, echo, nl); return true;
    case 0x4200u: dispatch(A_STEP, ram, echo, nl); return true;
    case 0x4300u: dispatch(A_COMPILE, ram, echo, nl); return true;
    case 0x5200u: insertMode = !insertMode; paint(ram); return true; // Insert
    case 0x011Bu: dispatch(A_EXIT, ram, echo, nl); return true;
    case 0x4800u: if (editRow > 0) --editRow; break;
    case 0x5000u: if (editRow + 1 < static_cast<int>(source.size())) ++editRow; break;
    case 0x4B00u:
        if (editCol > 0) --editCol;
        else if (editRow > 0) {
            --editRow;
            editCol = static_cast<int>(source[static_cast<std::size_t>(editRow)].size());
        }
        break;
    case 0x4D00u:
        if (editCol < static_cast<int>(source[static_cast<std::size_t>(editRow)].size())) ++editCol;
        else if (editRow + 1 < static_cast<int>(source.size())) { ++editRow; editCol = 0; }
        break;
    case 0x0E08u:
        if (editCol > 0) {
            source[static_cast<std::size_t>(editRow)].erase(
                static_cast<std::size_t>(editCol - 1), 1);
            --editCol; dirty = true;
        }
        break;
    case 0x1C0Du: {
        std::string& ln = source[static_cast<std::size_t>(editRow)];
        std::string tail = ln.substr(static_cast<std::size_t>(editCol));
        ln.erase(static_cast<std::size_t>(editCol));
        source.insert(source.begin() + editRow + 1, tail);
        ++editRow; editCol = 0; dirty = true;
        break;
    }
    default: {
        const char ch = static_cast<char>(key & 0xFFu);
        if (ch >= 32 && ch < 127) {
            std::string& ln = source[static_cast<std::size_t>(editRow)];
            if (insertMode)
                ln.insert(static_cast<std::size_t>(editCol), 1, ch);
            else if (editCol < static_cast<int>(ln.size()))
                ln[static_cast<std::size_t>(editCol)] = ch;
            else ln += ch;
            ++editCol;
            dirty = true;
        } else return true;
        break;
    }
    }
    if (editRow >= static_cast<int>(source.size())) source.push_back("");
    editCol = std::min(editCol, static_cast<int>(source[static_cast<std::size_t>(editRow)].size()));
    if (editRow < editTop) editTop = editRow;
    if (editRow >= editTop + ED_ROWS) editTop = editRow - ED_ROWS + 1;
    paint(ram);
    return true;
}

inline void pump(std::uint8_t* ram, std::uint16_t key, bool keyDown,
                 EchoFn echo, NewlineFn nl) noexcept {
    if (!active) return;
    const FieldRtxMouse::Frame m = FieldRtxMouse::capture();
    handleMouse(ram, m, echo, nl);
    if (keyDown && key) handleKey(ram, key, echo, nl);
}

inline void cmdShell(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                     const std::vector<std::string>& args) {
    if (args.size() >= 2 && FieldRtxGui::argIsHelp(args[1])) {
        FieldRtxBasic::shellPrint(ram, echo, nl,
            "\r\nAmmoCode — Turbo Pascal 7 RTX IDE\r\n"
            "  ammocode [file.asm|file.bas]  Master editor + debugger\r\n"
            "  Mouse | Alt+F/E/S/R/C/D/O/W/H | F5 Run F8 Step F9 Compile\r\n"
            "  qbasic — opens AmmoCode in BASIC mode\r\n");
        return;
    }
    Lang l = Lang::Basic;
    std::string p = "C:\\AMMOCODE\\MAIN.ASM";
    if (args.size() >= 2) {
        p = args[1];
        for (auto& c : p) if (c == '/') c = '\\';
        std::string ext;
        const auto dot = p.find_last_of('.');
        if (dot != std::string::npos) ext = FieldRtxGui::ciUpper(p.substr(dot));
        if (ext == ".ASM") l = Lang::Asm;
        else if (ext == ".FLD") l = Lang::Field;
        else if (ext == ".BAS") { l = Lang::Basic; if (p.find(':') == std::string::npos) p = "C:\\QBASIC\\" + p; }
        if (p.find(':') == std::string::npos && p.find('\\') == std::string::npos)
            p = "C:\\AMMOCODE\\" + p;
    }
    open(ram, l, p.c_str());
}

} // namespace FieldAmmoCode