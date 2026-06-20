#pragma once

// RTX-AMMOS interactive shell — GPU + host paths, real FAT/CD, full command set.

#include "FieldAmmoFat.hpp"
#include "FieldAmmoVfs.hpp"
#include "FieldRtxVfs.hpp"
#include "FieldRtxThrottle.hpp"
#include "FieldAmouranthFileCmd.hpp"
#include "FieldDeviceViewer.hpp"
#include "FieldExtensionEditor.hpp"
#include "FieldExtensionMap.hpp"
#include "FieldRegistry.hpp"
#include "FieldRtxMemory.hpp"
#include "FieldXms.hpp"
#include "FieldEms.hpp"
#include "FieldX86Emu.hpp"
#include "FieldRegistryEditor.hpp"
#include "FieldRaid.hpp"

#include "FieldAmmoBrowser.hpp"
#include "FieldAmmoCode.hpp"
#include "FieldAmmoNes.hpp"
#include "FieldAmmoA2600.hpp"
#include "FieldAmmoSms.hpp"
#include "FieldAmmoGenesis.hpp"
#include "FieldAmmoSnes.hpp"
#include "FieldPadTest.hpp"
#include "FieldPorts.hpp"
#include "FieldRuntimeInfo.hpp"
#include "FieldRtxBasic.hpp"
#include "FieldRtxBasicIde.hpp"
#include "FieldAmmoText.hpp"
#include "FieldRtxEdit.hpp"
#include "FieldRtxThemes.hpp"
#include "FieldRtxThemePicker.hpp"
#include "FieldRtxMouse.hpp"
#include "FieldRtxHelp.hpp"
#include "FieldRtxCmdHelp.hpp"
#include "FieldAmmoToolchain.hpp"
#include "FieldAmmoTools.hpp"
#include "FieldAudioRack.hpp"
#include "FieldCdRom.hpp"
#include "FieldDos.hpp"
#include "FieldAmmoBios.hpp"
namespace FieldAmmoExec {
bool isActive() noexcept;
bool launchFromGuest(std::uint8_t* ram, const char* dosPath) noexcept;
}
#include "FieldDrives.hpp"
#include "FieldDosDisplay.hpp"
#include "FieldDosViewport.hpp"
#include "FieldLayerShell.hpp"
#include "FieldMscdex.hpp"
#include "FieldRtxDrivers.hpp"
#include "OptionsMenu.hpp"
#include "FieldPlatform.hpp"
#include "FieldRtxAmmos.hpp"
#include "FieldAmouranthOs.hpp"
#include "FieldRtxTerm.hpp"

namespace FieldRtxBoot {
void paintWelcome(std::uint8_t* ram);
void paintVerbosePost(std::uint8_t* ram) noexcept;
extern bool verboseBoot;
}
#include "FieldRtxVgaText.hpp"
#include "FieldVga.hpp"

namespace FieldRtxConsoleGui {
extern bool active;
extern bool pendingHelp;
extern bool pendingNewSession;
void open(std::uint8_t* ram) noexcept;
void handleMouse(std::uint8_t* ram) noexcept;
void afterShellOutput(std::uint8_t* ram) noexcept;
bool pumpKey(std::uint16_t key, std::uint8_t* ram) noexcept;
}

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <functional>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace FieldRtxShell {

constexpr std::uint32_t VGA_BASE = 0x000B8000u;
constexpr std::uint32_t BDA_CUR_COL = 0x450u;
constexpr std::uint32_t BDA_CUR_ROW = 0x451u;

inline bool graphicsActive = false;
inline std::string currentDir = "C:\\";
inline std::string dirSubPath;  // e.g. SOUND\ nested under current drive

inline char shellLine[128]{};
inline int shellLen = 0;
inline bool echoCommands = false;
inline std::uint8_t shellInputAttr = 0x02u;  // green on black — RTX terminal
inline std::uint8_t shellEchoAttr  = 0x02u;

struct PromptSeg {
    const char* text;
    std::uint8_t attr;
};

enum class PromptStyle : std::uint8_t {
    Classic = 0,  // C:\>
    Rtx     = 1,  // RTX[C:\]>
    Field   = 2,  // field://C/>
    Matrix  = 3,  // λ C:\>
};

inline PromptStyle promptStyle = PromptStyle::Classic;

inline constexpr const char* kDefaultPathDirs[] = {
    "C:\\", "C:\\DOS\\", "C:\\TOOLS\\", "C:\\WINDOWS\\", "C:\\WIN\\",
    "C:\\SOUND\\", "C:\\SAMPLES\\", "C:\\AMMOINC\\", "C:\\QBASIC\\", "C:\\AMMOCODE\\"
};

using EchoFn = std::function<void(std::uint8_t* ram, char ch)>;
using NewlineFn = std::function<void(std::uint8_t* ram)>;
using PromptFn = std::function<void(std::uint8_t* ram)>;

inline void execLine(const char* line, std::uint8_t* ram,
                     EchoFn echo, NewlineFn nl, PromptFn prompt);

inline void formatPathLine(char* buf, std::size_t len) noexcept {
    std::snprintf(buf, len,
        "PATH=C:\\;C:\\DOS;C:\\TOOLS;C:\\WINDOWS;C:\\WIN;"
        "C:\\SOUND;C:\\SAMPLES;C:\\AMMOINC;E:\\HOST");
}

inline int shellCols() noexcept { return FieldRtxTerm::screenCols(); }
inline int shellRows() noexcept { return FieldRtxTerm::screenRows(); }

inline std::uint16_t cursorPos(std::uint8_t* ram) noexcept {
    (void)ram;
    return static_cast<std::uint16_t>(ram[BDA_CUR_ROW] * shellCols() + ram[BDA_CUR_COL]);
}

inline void setCursor(std::uint8_t* ram, std::uint16_t cell) noexcept {
    const int cols = shellCols();
    ram[BDA_CUR_COL] = static_cast<std::uint8_t>(cell % static_cast<std::uint16_t>(cols));
    ram[BDA_CUR_ROW] = static_cast<std::uint8_t>(cell / static_cast<std::uint16_t>(cols));
}

inline void vgaPut(std::uint8_t* ram, std::uint16_t cell, std::uint8_t ch, std::uint8_t attr) noexcept {
    const int cols = shellCols();
    const int row = static_cast<int>(cell / static_cast<std::uint16_t>(cols));
    const int col = static_cast<int>(cell % static_cast<std::uint16_t>(cols));
    const std::uint32_t off = FieldRtxTerm::cellOffset(row, col);
    if (off + 1u >= FieldPlatform::GUEST_RAM_BYTES) return;
    ram[off] = ch;
    ram[off + 1u] = attr;
}

inline void scrollUp(std::uint8_t* ram) noexcept {
    FieldRtxTerm::onScrollUp(ram);
    const int cols = shellCols();
    const int rows = shellRows();
    const int last = rows - 1;
    for (int row = 0; row < last; ++row) {
        for (int col = 0; col < cols; ++col) {
            const std::uint32_t dst = FieldRtxTerm::cellOffset(row, col);
            const std::uint32_t src = FieldRtxTerm::cellOffset(row + 1, col);
            ram[dst]     = ram[src];
            ram[dst + 1u] = ram[src + 1u];
        }
    }
    for (int col = 0; col < cols; ++col)
        vgaPut(ram, static_cast<std::uint16_t>(last * cols + col), ' ', 0x02u);
    FieldRtxTerm::syncLiveFromVga(ram);
}

inline void echoChar(std::uint8_t* ram, char ch) noexcept {
    if (ch != '\0')
        FieldRtxTerm::onUserInput();
    if (ch == '\r') return;
    if (ch == '\n') {
        std::uint16_t pos = cursorPos(ram);
        const int cols = shellCols();
        const int rows = shellRows();
        const std::uint16_t row = static_cast<std::uint16_t>(pos / static_cast<std::uint16_t>(cols) + 1u);
        if (row >= static_cast<std::uint16_t>(rows)) {
            scrollUp(ram);
            setCursor(ram, static_cast<std::uint16_t>((rows - 1) * cols));
        } else {
            setCursor(ram, static_cast<std::uint16_t>(row * static_cast<std::uint16_t>(cols)));
        }
        return;
    }
    if (ch == '\b') {
        std::uint16_t pos = cursorPos(ram);
        if (pos > 0) {
            --pos;
            vgaPut(ram, pos, ' ', shellInputAttr);
            setCursor(ram, pos);
        }
        return;
    }
    std::uint16_t pos = cursorPos(ram);
    const int cols = shellCols();
    const int rows = shellRows();
    if (pos / static_cast<std::uint16_t>(cols) >= static_cast<std::uint16_t>(rows)) {
        scrollUp(ram);
        pos = static_cast<std::uint16_t>((rows - 1) * cols);
    }
    vgaPut(ram, pos, static_cast<std::uint8_t>(ch), shellInputAttr);
    ++pos;
    if (pos / static_cast<std::uint16_t>(cols) >= static_cast<std::uint16_t>(rows)) {
        scrollUp(ram);
        pos = static_cast<std::uint16_t>((rows - 1) * cols);
    }
    setCursor(ram, pos);
    FieldRtxTerm::liveDirty = true;
}

inline void shellPrint(std::uint8_t* ram, EchoFn echo, NewlineFn nl, const char* text) noexcept {
    for (const char* p = text; *p; ++p) {
        if (*p == '\r') continue;
        if (*p == '\n') nl(ram);
        else echo(ram, *p);
    }
}

inline void shellPrintAttr(std::uint8_t* ram, EchoFn /*echo*/, NewlineFn nl,
                           const char* text, std::uint8_t attr) noexcept {
    for (const char* p = text; *p; ++p) {
        if (*p == '\r') continue;
        if (*p == '\n') {
            nl(ram);
            continue;
        }
        std::uint16_t pos = cursorPos(ram);
        if (pos / 80u >= 25u) {
            scrollUp(ram);
            pos = static_cast<std::uint16_t>(24 * 80u);
        }
        vgaPut(ram, pos, static_cast<std::uint8_t>(*p), attr);
        ++pos;
        setCursor(ram, pos);
    }
}

inline void defaultNewline(std::uint8_t* ram) noexcept { echoChar(ram, '\n'); }

inline void syncPromptFromDrives() noexcept {
    FieldDrives::refresh();
    char buf[96];
    if (FieldDrives::currentLetter == 'C' && !dirSubPath.empty())
        std::snprintf(buf, sizeof buf, "C:\\%s", dirSubPath.c_str());
    else
        std::snprintf(buf, sizeof buf, "%c:\\", FieldDrives::currentLetter);
    currentDir = buf;
}

inline std::string normalizeSubPath(std::string p) {
    for (auto& c : p) {
        if (c == '/') c = '\\';
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    while (!p.empty() && p.front() == '\\') p.erase(0, 1);
    while (!p.empty() && p.back() == '\\') p.pop_back();
    return p;
}

inline std::string resolveCPath(const std::string& arg) {
    std::string a = normalizeSubPath(arg);
    if (a.empty()) return "C:\\";
    if (a.size() >= 2 && a[1] == ':') {
        if (a.size() > 2 && a[2] != '\\') a.insert(2, "\\");
        return a;
    }
    if (!dirSubPath.empty())
        return "C:\\" + dirSubPath + "\\" + a;
    return "C:\\" + a;
}

inline bool popSubPath() noexcept {
    if (dirSubPath.empty()) return false;
    const auto pos = dirSubPath.find_last_of('\\');
    if (pos == std::string::npos) {
        dirSubPath.clear();
        return true;
    }
    dirSubPath.erase(pos);
    if (!dirSubPath.empty() && dirSubPath.back() != '\\') { /* keep internal form without trailing */ }
    return true;
}

inline bool pushSubPath(const std::string& name) {
    std::string n = normalizeSubPath(name);
    if (n.empty() || n == ".") return true;
    if (n == "..") return popSubPath();
    if (!dirSubPath.empty())
        dirSubPath += "\\" + n;
    else
        dirSubPath = n;
    return true;
}

inline std::string currentCPath() {
    if (dirSubPath.empty()) return "C:\\";
    return "C:\\" + dirSubPath;
}

inline std::string resolveAnyPath(char drive, const std::string& fname) {
    if (drive == 'C') {
        std::string f = fname;
        for (auto& c : f) {
            if (c == '/') c = '\\';
            c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        if (f.find(':') != std::string::npos || f.find('\\') != std::string::npos)
            return resolveCPath(f);
        if (!dirSubPath.empty())
            return "C:\\" + dirSubPath + "\\" + normalizeSubPath(f);
        return "C:\\" + normalizeSubPath(f);
    }
    char buf[96];
    std::snprintf(buf, sizeof buf, "%c:\\%s", drive, fname.c_str());
    return buf;
}

inline bool resolveOnPath(const std::string& cmd, std::string& outPath) noexcept {
    auto tryOne = [&](const std::string& p) -> bool {
        if (FieldAmmoVfs::pathExists(p.c_str())) {
            outPath = p;
            return true;
        }
        return false;
    };
    if (tryOne(resolveAnyPath('C', cmd))) return true;
    const bool hasDot = cmd.find('.') != std::string::npos;
    static const char* kTryExt[] = {".COM", ".EXE", ".BAT", ".SYS", nullptr};
    for (const char* dir : kDefaultPathDirs) {
        if (!hasDot) {
            for (const char* ext : kTryExt) {
                if (!ext) break;
                char path[160];
                std::snprintf(path, sizeof path, "%s%s%s", dir, cmd.c_str(), ext);
                if (tryOne(path)) return true;
            }
        }
        char path[160];
        std::snprintf(path, sizeof path, "%s%s", dir, cmd.c_str());
        if (tryOne(path)) return true;
    }
    return false;
}

inline bool cdTo(const std::string& rawArg) {
    std::string arg = rawArg;
    for (auto& c : arg)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    if (arg == "\\" || arg == "/") {
        dirSubPath.clear();
        return true;
    }
    if (arg == "..") {
        popSubPath();
        return true;
    }
    if (arg.size() >= 2 && arg[1] == ':') {
        const char d = static_cast<char>(std::toupper(static_cast<unsigned char>(arg[0])));
        if (d != 'C') {
            dirSubPath.clear();
            return FieldDrives::setCurrent(d);
        }
        FieldDrives::setCurrent('C');
        const auto slash = arg.find('\\');
        if (slash == std::string::npos) {
            dirSubPath.clear();
            return true;
        }
        const std::string sub = normalizeSubPath(arg.substr(slash + 1));
        char full[128];
        std::snprintf(full, sizeof full, "C:\\%s", sub.c_str());
        if (!sub.empty() && !FieldAmmoVfs::isDirectory(full)) return false;
        dirSubPath = sub;
        return true;
    }
    if (FieldDrives::currentLetter != 'C') return false;
    std::vector<std::string> parts;
    std::string comp;
    for (char c : arg) {
        if (c == '\\' || c == '/') {
            if (!comp.empty()) { parts.push_back(comp); comp.clear(); }
        } else
            comp += c;
    }
    if (!comp.empty()) parts.push_back(comp);
    for (const auto& part : parts) {
        const std::string p = normalizeSubPath(part);
        if (p == "..") { popSubPath(); continue; }
        if (p == "." || p.empty()) continue;
        std::string trial = dirSubPath;
        if (!trial.empty()) trial += "\\";
        trial += p;
        char full[128];
        std::snprintf(full, sizeof full, "C:\\%s", trial.c_str());
        if (!FieldAmmoVfs::isDirectory(full)) return false;
        pushSubPath(p);
    }
    return true;
}

inline char parseDriveToken(const std::string& tok) noexcept {
    if (tok.size() < 2) return '\0';
    const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(tok[0])));
    if ((tok[1] == ':' || tok[1] == '\\') && c >= 'A' && c <= 'Z')
        return c;
    return '\0';
}

inline void formatPromptString(char* buf, std::size_t len) noexcept {
    syncPromptFromDrives();
    switch (promptStyle) {
    case PromptStyle::Classic:
        std::snprintf(buf, len, "%s>", currentDir.c_str());
        break;
    case PromptStyle::Rtx:
        std::snprintf(buf, len, "RTX[%s]>", currentDir.c_str());
        break;
    case PromptStyle::Field:
        if (!dirSubPath.empty())
            std::snprintf(buf, len, "field://C/%s/>", dirSubPath.c_str());
        else
            std::snprintf(buf, len, "field://%c/>",
                FieldDrives::currentLetter ? FieldDrives::currentLetter : 'C');
        break;
    case PromptStyle::Matrix:
    default:
        std::snprintf(buf, len, "\xE6 %s>", currentDir.c_str());
        break;
    }
}

inline int promptSegmentCount() noexcept {
    switch (promptStyle) {
    case PromptStyle::Classic: return 3;
    case PromptStyle::Rtx:     return 5;
    case PromptStyle::Field:   return 2;
    case PromptStyle::Matrix:  return 3;
    default: return 3;
    }
}

inline void buildPromptSegments(PromptSeg* segs, int maxSegs) noexcept {
    if (!segs || maxSegs < 1) return;
    syncPromptFromDrives();
    int n = 0;
    switch (promptStyle) {
    case PromptStyle::Classic:
        segs[n++] = { "C:\\", 0x02u };
        segs[n++] = { ">", 0x02u };
        break;
    case PromptStyle::Rtx:
        segs[n++] = { "RTX", 0x5Du };
        segs[n++] = { "[", 0x08u };
        segs[n++] = { currentDir.c_str(), 0x3Bu };
        segs[n++] = { "]>", 0x0Fu };
        break;
    case PromptStyle::Field: {
        char fld[64]{};
        if (!dirSubPath.empty())
            std::snprintf(fld, sizeof fld, "C/%s/>", dirSubPath.c_str());
        else
            std::snprintf(fld, sizeof fld, "%c/>",
                FieldDrives::currentLetter ? FieldDrives::currentLetter : 'C');
        segs[n++] = { "field://", 0x2Au };
        segs[n++] = { fld, 0x3Bu };
        break;
    }
    case PromptStyle::Matrix:
    default:
        segs[n++] = { "\xE6 ", 0x2Au };
        segs[n++] = { currentDir.c_str(), 0x2Fu };
        segs[n++] = { ">", 0x0Au };
        break;
    }
}

inline std::uint16_t paintPromptAt(std::uint8_t* ram, std::uint16_t baseCell) noexcept {
    const int cols = shellCols();
    for (int col = 0; col < cols; ++col)
        vgaPut(ram, static_cast<std::uint16_t>(baseCell + col), ' ', 0x02u);
    PromptSeg segs[6]{};
    buildPromptSegments(segs, 6);
    std::uint16_t cell = baseCell;
    const std::uint16_t limit = static_cast<std::uint16_t>(baseCell + cols - 2u);
    for (int s = 0; s < promptSegmentCount() && segs[s].text; ++s) {
        for (int i = 0; segs[s].text[i] && cell < limit; ++i)
            vgaPut(ram, cell++, static_cast<std::uint8_t>(segs[s].text[i]), segs[s].attr);
    }
    return cell;
}

inline std::uint16_t promptBaseCell(std::uint8_t* ram) noexcept {
    const int cols = shellCols();
    const int rows = shellRows();
    std::uint16_t row = ram[BDA_CUR_ROW];
    if (ram[BDA_CUR_COL] > 0u) {
        echoChar(ram, '\n');
        row = ram[BDA_CUR_ROW];
    }
    if (row >= static_cast<std::uint16_t>(rows)) {
        scrollUp(ram);
        row = static_cast<std::uint16_t>(rows - 1);
    }
    return static_cast<std::uint16_t>(row * static_cast<std::uint16_t>(cols));
}

inline void replayPrompt(std::uint8_t* ram, EchoFn echo) noexcept {
    PromptSeg segs[6]{};
    buildPromptSegments(segs, 6);
    for (int s = 0; s < promptSegmentCount() && segs[s].text; ++s) {
        for (int i = 0; segs[s].text[i]; ++i) {
            std::uint16_t pos = cursorPos(ram);
            vgaPut(ram, pos, static_cast<std::uint8_t>(segs[s].text[i]), segs[s].attr);
            setCursor(ram, static_cast<std::uint16_t>(pos + 1u));
        }
    }
    (void)echo;
}

inline void defaultPrompt(std::uint8_t* ram) noexcept {
    const std::uint16_t base = promptBaseCell(ram);
    const std::uint16_t cell = paintPromptAt(ram, base);
    setCursor(ram, cell);
    shellLen = 0;
    shellLine[0] = '\0';
    FieldRtxTerm::liveDirty = true;
}

inline bool istreq(const char* a, const char* b) noexcept {
    while (*a && *b) {
        if (std::tolower(static_cast<unsigned char>(*a))
            != std::tolower(static_cast<unsigned char>(*b)))
            return false;
        ++a; ++b;
    }
    return *a == *b;
}

inline void tokenize(const char* line, std::vector<std::string>& out) {
    out.clear();
    const char* p = line;
    while (*p == ' ' || *p == '\t') ++p;
    while (*p) {
        while (*p == ' ' || *p == '\t') ++p;
        if (!*p) break;
        const char* start = p;
        while (*p && *p != ' ' && *p != '\t') ++p;
        out.emplace_back(start, p);
    }
}

inline void vgaPlotRect(std::uint8_t* ram, std::uint32_t x0, std::uint32_t y0,
                        std::uint32_t x1, std::uint32_t y1, std::uint8_t color) noexcept {
    constexpr std::uint32_t fb = 0xA0000u;
    x1 = std::min(x1, 319u);
    y1 = std::min(y1, 199u);
    for (std::uint32_t y = y0; y <= y1; ++y)
        for (std::uint32_t x = x0; x <= x1; ++x)
            ram[fb + y * 320u + x] = color;
}

inline void paintWinDesktop(std::uint8_t* ram, FieldRtxAmmos::Era era) noexcept {
    graphicsActive = true;
    FieldRtxAmmos::activeEra = era;
    FieldVga::setMode(0x13u, ram);
    ram[0x449u] = 0x13u;
    constexpr std::uint32_t fb = 0xA0000u;
    const bool win98 = era == FieldRtxAmmos::Era::Win98;
    const std::uint8_t desktop = win98 ? 3u : 3u;
    const std::uint8_t titleHi = win98 ? 12u : 9u;
    const std::uint8_t titleLo = 7u;
    const std::uint8_t chrome = 8u;
    const std::uint8_t window = 7u;
    const std::uint8_t shadow = 8u;
    const std::uint8_t iconHi = 15u;
    const std::uint8_t iconLo = win98 ? 1u : 4u;

    for (std::uint32_t y = 0; y < 200u; ++y)
        for (std::uint32_t x = 0; x < 320u; ++x)
            ram[fb + y * 320u + x] = desktop;

    if (!win98) {
        vgaPlotRect(ram, 0, 0, 319, 17, titleHi);
        vgaPlotRect(ram, 0, 16, 319, 17, titleLo);
        vgaPlotRect(ram, 8, 22, 220, 140, window);
        vgaPlotRect(ram, 10, 24, 218, 34, titleHi);
        vgaPlotRect(ram, 11, 35, 217, 138, desktop);
        vgaPlotRect(ram, 222, 24, 318, 140, shadow);
        vgaPlotRect(ram, 12, 40, 36, 64, iconHi);
        vgaPlotRect(ram, 14, 42, 34, 62, iconLo);
        vgaPlotRect(ram, 44, 40, 68, 64, iconHi);
        vgaPlotRect(ram, 46, 42, 66, 62, iconLo);
        vgaPlotRect(ram, 76, 40, 100, 64, iconHi);
        vgaPlotRect(ram, 78, 42, 98, 62, iconLo);
        vgaPlotRect(ram, 0, 184, 319, 199, chrome);
    } else {
        vgaPlotRect(ram, 0, 0, 319, 20, titleHi);
        vgaPlotRect(ram, 40, 30, 200, 130, window);
        vgaPlotRect(ram, 0, 182, 319, 199, 1u);
    }
}

inline void setTextMode(std::uint8_t* ram) noexcept {
    graphicsActive = false;
    FieldRtxAmmos::activeEra = FieldRtxAmmos::Era::Dos;
    FieldVga::setMode(3u, ram);
    ram[0x449u] = 3u;
    ram[0x44Au] = 80u;
}

inline void enterGraphicsMode(std::uint8_t* ram, std::uint8_t mode) noexcept {
    graphicsActive = true;
    FieldVga::setMode(mode, ram);
    FieldVga::paintModeDemo(ram, mode);
    ram[0x449u] = mode;
}

inline void cmdMode(std::uint8_t* ram, const std::vector<std::string>& args,
                    EchoFn echo, NewlineFn nl) noexcept {
    if (args.size() < 2) {
        char buf[384];
        std::snprintf(buf, sizeof buf,
            "\r\nVideo mode %02Xh — MODE TEXT|CGA|EGA|VGA|VOODOO|MONO|13|12|4|5|6\r\n"
            "  TEXT/C080 = 80x25 VGA color  |  MONO = CGA 640x200 B&W\r\n"
            "  CGA/4 = 320x200 4-color     |  5 = CGA composite\r\n"
            "  EGA/12 = 640x480 16-color   |  VGA/13 = 320x200 256-color\r\n"
            "  VOODOO = RTX 3dfx-style upscale (mode A0h)\r\n",
            FieldVga::mode);
        shellPrint(ram, echo, nl, buf);
        return;
    }
    const std::string& m = args[1];
    if (istreq(m.c_str(), "TEXT") || istreq(m.c_str(), "CO80") || istreq(m.c_str(), "80")) {
        setTextMode(ram);
        shellPrintAttr(ram, echo, nl, "\r\n80x25 VGA text mode\r\n", 0x3Bu);
        return;
    }
    if (istreq(m.c_str(), "MONO") || istreq(m.c_str(), "6") || istreq(m.c_str(), "BW")) {
        enterGraphicsMode(ram, 6u);
        shellPrintAttr(ram, echo, nl, "\r\nCGA 640x200 monochrome — ESC for text\r\n", 0x07u);
        return;
    }
    if (istreq(m.c_str(), "CGA") || istreq(m.c_str(), "4")) {
        enterGraphicsMode(ram, 4u);
        shellPrintAttr(ram, echo, nl, "\r\nCGA 320x200 4-color — ESC for text\r\n", 0x1Bu);
        return;
    }
    if (istreq(m.c_str(), "5")) {
        enterGraphicsMode(ram, 5u);
        shellPrintAttr(ram, echo, nl, "\r\nCGA 320x200 composite — ESC for text\r\n", 0x1Bu);
        return;
    }
    if (istreq(m.c_str(), "EGA") || istreq(m.c_str(), "12")) {
        enterGraphicsMode(ram, 0x12u);
        shellPrintAttr(ram, echo, nl, "\r\nEGA 640x480 16-color — ESC for text\r\n", 0x5Bu);
        return;
    }
    if (istreq(m.c_str(), "VGA") || istreq(m.c_str(), "13")) {
        enterGraphicsMode(ram, 0x13u);
        shellPrintAttr(ram, echo, nl, "\r\nVGA 320x200 256-color — ESC for text\r\n", 0x5Du);
        return;
    }
    if (istreq(m.c_str(), "VOODOO") || istreq(m.c_str(), "3DFX")) {
        enterGraphicsMode(ram, FieldVga::MODE_VOODOO);
        shellPrintAttr(ram, echo, nl,
            "\r\nRTX Voodoo layer — bilinear 13h + scan (mode A0h) — ESC for text\r\n", 0x2Au);
        return;
    }
    shellPrint(ram, echo, nl, "\r\nUsage: MODE TEXT|CGA|EGA|VGA|VOODOO|MONO|4|5|6|12|13\r\n");
}

inline void formatHostDateTime(char* dateBuf, std::size_t dateLen,
                               char* timeBuf, std::size_t timeLen) noexcept {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    static const char* days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    std::snprintf(dateBuf, dateLen, "%s %02d-%02d-%04d",
        days[tm.tm_wday], tm.tm_mon + 1, tm.tm_mday, tm.tm_year + 1900);
    std::snprintf(timeBuf, timeLen, "%02d:%02d:%02d.00",
        tm.tm_hour, tm.tm_min, tm.tm_sec);
}

inline bool readDriveFile(char drive, const std::string& name, std::vector<std::uint8_t>& out) {
    FieldDrives::refresh();
    if (drive == 'E')
        return FieldDrives::readHostBridgeFile(name.c_str(), out);
    char path[96];
    std::snprintf(path, sizeof path, "%c:\\%s", drive, name.c_str());
    if (drive == 'C')
        return FieldAmmoFat::readFile(path, out);
    return FieldDos::readHostFile(path, out);
}

inline bool win31FilePresent(const char* path, std::uint32_t minBytes = 1u) noexcept {
    std::vector<std::uint8_t> data;
    return readDriveFile('C', path, data) && data.size() >= minBytes;
}

inline bool win31LicensedReady() noexcept {
    constexpr std::uint32_t kStubBytes = 64u;
    if (!win31FilePresent("WINDOWS\\WIN.COM", kStubBytes)) return false;
    if (!win31FilePresent("WINDOWS\\USER.EXE")) return false;
    if (!win31FilePresent("WINDOWS\\GDI.EXE")) return false;
    if (!win31FilePresent("WINDOWS\\PROGMAN.EXE")) return false;
    if (!win31FilePresent("WINDOWS\\SYSTEM.INI")) return false;
    return win31FilePresent("WINDOWS\\KERNEL.EXE")
        || win31FilePresent("WINDOWS\\KRNL386.EXE");
}

inline void cmdSetup31(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    shellPrint(ram, echo, nl,
        "\r\nWindows 3.1 Setup — Zachary Geurts MCSE+I\r\n"
        "==========================================\r\n");
    struct Row { const char* file; const char* note; };
    const Row req[] = {
        {"WINDOWS\\WIN.COM", "shell launcher"},
        {"WINDOWS\\KERNEL.EXE", "16-bit kernel (or KRNL386.EXE)"},
        {"WINDOWS\\USER.EXE", "USER module"},
        {"WINDOWS\\GDI.EXE", "GDI module"},
        {"WINDOWS\\PROGMAN.EXE", "Program Manager"},
        {"WINDOWS\\SYSTEM.INI", "system config"},
    };
    for (const auto& r : req) {
        bool ok = win31FilePresent(r.file);
        if (!ok && std::strstr(r.file, "KERNEL"))
            ok = win31FilePresent("WINDOWS\\KRNL386.EXE");
        char buf[96];
        std::snprintf(buf, sizeof buf, "  [%s] %s — %s\r\n",
            ok ? " OK " : "MISS", r.file, r.note);
        shellPrint(ram, echo, nl, buf);
    }
    if (win31LicensedReady())
        shellPrint(ram, echo, nl,
            "\r\nSTATUS: READY — CONFIG.SYS [WIN31] or BOOT31.BAT\r\n");
    else
        shellPrint(ram, echo, nl,
            "\r\nSTATUS: INCOMPLETE — drop media in assets/dos/incoming/win31/\r\n"
            "  Host: ./linux.sh win31 --stage && ./linux.sh dos --force\r\n"
            "  Preview: WIN31 (RTX VGA desktop)\r\n"
            "  Help: TYPE WIN31.TXT\r\n");
}

inline void cmdHelp(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    shellPrint(ram, echo, nl,
        "\r\nRTX COMMAND.COM 7.0 — PSYCHO HISTORIC MONSTER — Golden Era forever\r\n"
        "  Zachary Geurts MCSE+I — human intent meets silicon on the Field Die\r\n"
        "  Files: DIR [/W /V] LS TREE CD PWD MD RD DEL COPY [/S] REN TYPE EDIT\r\n"
        "         MORE FIND ATTRIB WHERE CLS\r\n"
        "  Sys:   VER VOL LABEL DATE TIME PATH CHKDSK MEM GPU MODE THROTTLE REG REGEDIT HELP /?\r\n"
        "  Mount: MOUNT CD|ZIP|ISO|LIST  MOUNT UNMOUNT ALL|label\r\n"
        "  Apps:  AMMOCODE [file] — Turbo Pascal IDE mouse+Alt+ASM debug\r\n"
        "         PADTEST — Xbox360/SDL gamepad live tester\r\n"
        "         NES rom.nes — AmmoNES iNES emulator\r\n"
        "         QBASIC / EDIT (light) — case insensitive\r\n"
        "  Sound: SOUND DIR TEST BEEP RUN — C:\\SOUND\\\r\n"
        "  Field: DEVICES DRIVES DRIVERS FIELD LAYER AMMOFAT MSCDEX BIOS\r\n"
        "  DevKit: FIELDC AMMOASM AMMOSYS AMMOCC AMMOLINK AMMODECOMP AMMOZIP AMMORUN AMMODBG BUILD TOOLS\r\n"
        "  OS:    AMOURANTHOS / AOS — RTX WM desktop (Start, programs, clock)\r\n"
        "         LAUNCHER — GUI program picker (AmmoCode, Doom, PADTEST…)\r\n"
        "         ERA WIN31 SETUP31 WIN98 LINUX AMMOS EXIT GRAPHICS\r\n"
        "  VFS:   DESCRIPT.ION metadata | MOUNT ZIP|ISO | colorful DIR\r\n"
        "  Speed: THROTTLE — deep MZ/PE analysis per program launch\r\n"
        "  Golden: TYPE GOLDEN.TXT | Host binary: AMOURANTHRTX /help\r\n"
        "  Help:   COMMAND /?  — detailed syntax for every built-in\r\n"
        "          HELP topic   TYPE COMMAND.TXT\r\n"
        "  Panel: double-click zoom | drag anywhere | HUD shows mem/disk\r\n"
        "  PATH=C:\\;C:\\TOOLS;C:\\SOUND;C:\\SAMPLES;E:\\HOST\r\n");
}

inline void cmdBoot(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    shellPrint(ram, echo, nl, "\r\nReplaying RTX-AMMOS POST...\r\n");
    FieldRtxBoot::paintVerbosePost(ram);
    defaultPrompt(ram);
}

inline void cmdBios(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    shellPrint(ram, echo, nl, "\r\nAmmoBIOS Setup — CMOS/NVRAM (mouse + arrows)\r\n");
    FieldAmmoBios::open(ram);
}

inline void cmdVerbose(std::uint8_t* ram, const std::vector<std::string>& args,
                       EchoFn echo, NewlineFn nl) noexcept {
    if (args.size() >= 2) {
        const std::string& v = args[1];
        if (istreq(v.c_str(), "ON") || istreq(v.c_str(), "1"))
            FieldRtxBoot::verboseBoot = true;
        else if (istreq(v.c_str(), "OFF") || istreq(v.c_str(), "0"))
            FieldRtxBoot::verboseBoot = false;
        else {
            shellPrint(ram, echo, nl, "\r\nUsage: VERBOSE ON|OFF\r\n");
            return;
        }
    }
    char buf[64];
    std::snprintf(buf, sizeof buf, "\r\nVerbose boot: %s\r\n",
        FieldRtxBoot::verboseBoot ? "ON" : "OFF");
    shellPrint(ram, echo, nl, buf);
}

inline void cmdEcho(std::uint8_t* ram, const std::vector<std::string>& args,
                    EchoFn echo, NewlineFn nl) noexcept {
    if (args.size() >= 2) {
        if (istreq(args[1].c_str(), "ON")) echoCommands = true;
        else if (istreq(args[1].c_str(), "OFF")) echoCommands = false;
        else {
            shellPrint(ram, echo, nl, "\r\nUsage: ECHO ON|OFF\r\n");
            return;
        }
    }
    char buf[48];
    std::snprintf(buf, sizeof buf, "\r\nCommand echo: %s\r\n",
        echoCommands ? "ON" : "OFF");
    shellPrint(ram, echo, nl, buf);
}

inline void cmdPromptStyle(std::uint8_t* ram, const std::vector<std::string>& args,
                           EchoFn echo, NewlineFn nl) noexcept {
    if (args.size() < 2) {
        const char* name = "RTX";
        switch (promptStyle) {
        case PromptStyle::Classic: name = "CLASSIC"; break;
        case PromptStyle::Field:   name = "FIELD"; break;
        case PromptStyle::Matrix:  name = "MATRIX"; break;
        default: break;
        }
        char buf[64];
        std::snprintf(buf, sizeof buf,
            "\r\nPrompt style: %s (CLASSIC|RTX|FIELD|MATRIX)\r\n", name);
        shellPrint(ram, echo, nl, buf);
        return;
    }
    const std::string& s = args[1];
    if (istreq(s.c_str(), "CLASSIC")) promptStyle = PromptStyle::Classic;
    else if (istreq(s.c_str(), "RTX")) promptStyle = PromptStyle::Rtx;
    else if (istreq(s.c_str(), "FIELD")) promptStyle = PromptStyle::Field;
    else if (istreq(s.c_str(), "MATRIX")) promptStyle = PromptStyle::Matrix;
    else {
        shellPrint(ram, echo, nl, "\r\nUsage: PROMPT CLASSIC|RTX|FIELD|MATRIX\r\n");
        return;
    }
    defaultPrompt(ram);
}

inline void cmdVer(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    char layerSum[48]{};
    FieldLayer::formatShellSummary(layerSum, sizeof layerSum);
    char rt[256]{};
    FieldRuntimeInfo::formatVerBlock(rt, sizeof rt);
    char buf[768];
    std::snprintf(buf, sizeof buf,
        "\r\nRTX COMMAND.COM 7.0 — %s %s\r\n"
        "%s — Zachary Geurts MCSE+I Field Die shell\r\n"
        "%.220s"
        "%llu bytes RAM | AMMOFAT VFS\r\n"
        "%.40s\r\n"
        "Field drives: %.20s (current %c:) | layers fat[%s] mscdex[%s] viewport[%.1fx]\r\n",
        FieldRtxAmmos::PRODUCT, FieldRtxAmmos::DOS_LAYER,
        FieldRtxAmmos::TAGLINE,
        rt,
        static_cast<unsigned long long>(FieldPlatform::REPORTED_RAM_BYTES),
        layerSum,
        FieldDrives::readyLetters(), FieldDrives::currentLetter,
        FieldLayer::isShellActive(FieldLayer::LayerId::Fat) ? "ON" : "off",
        FieldLayer::isShellActive(FieldLayer::LayerId::Mscdex) ? "ON" : "off",
        static_cast<double>(FieldDosViewport::fontScale));
    shellPrint(ram, echo, nl, buf);
}

inline void printRichDirListing(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                                const std::vector<FieldRtxVfs::RichEntry>& entries,
                                bool wide, bool verbose, int& count, std::uint32_t& total,
                                int& /*shown*/, int /*kMaxLines*/) {
    if (wide) {
        int col = 0;
        for (const auto& fe : entries) {
            ++count;
            if (!fe.isDir) total += fe.size;
            char line[32];
            std::snprintf(line, sizeof line, "%.14s", fe.name);
            shellPrintAttr(ram, echo, nl, line, fe.vgaAttr);
            ++col;
            if (col >= 5) {
                shellPrint(ram, echo, nl, "\r\n");
                col = 0;
            }
        }
        if (col) shellPrint(ram, echo, nl, "\r\n");
        return;
    }
    for (const auto& fe : entries) {
        ++count;
        if (!fe.isDir) total += fe.size;
        char line[96];
        if (fe.isDir)
            std::snprintf(line, sizeof line, " %-12s <DIR>          ", fe.name);
        else
            std::snprintf(line, sizeof line, " %-12s %14u ", fe.name, fe.size);
        shellPrintAttr(ram, echo, nl, line, fe.vgaAttr);
        if (fe.desc && fe.desc[0]) {
            char meta[96];
            std::snprintf(meta, sizeof meta, " %s", fe.desc);
            shellPrintAttr(ram, echo, nl, meta, 0x08u);
            if (verbose) {
                FieldRtxVfs::FileMeta fm;
                if (FieldRtxVfs::lookupMeta(fe.name, fm) && !fm.url.empty()) {
                    char url[80];
                    std::snprintf(url, sizeof url, " [%s]", fm.url.c_str());
                    shellPrintAttr(ram, echo, nl, url, 0x03u);
                }
            }
            shellPrint(ram, echo, nl, "\r\n");
        } else {
            shellPrint(ram, echo, nl, "\r\n");
        }
    }
}

inline void cmdDir(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                   const std::vector<std::string>& args) {
    FieldRtxVfs::vfsReload();
    FieldDrives::refresh();
    bool wide = false;
    bool verbose = false;
    std::string wildcard;
    char drive = FieldDrives::currentLetter;
    std::string listPathArg;
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (istreq(args[i].c_str(), "/W") || istreq(args[i].c_str(), "-W")) {
            wide = true;
            continue;
        }
        if (istreq(args[i].c_str(), "/V") || istreq(args[i].c_str(), "-V")
            || istreq(args[i].c_str(), "/M")) {
            verbose = true;
            continue;
        }
        const char d = parseDriveToken(args[i]);
        if (d) drive = d;
        else if (drive == 'C' && listPathArg.empty()) {
            if (FieldRtxVfs::hasWildcard(args[i].c_str()))
                wildcard = args[i];
            else
                listPathArg = resolveCPath(args[i]);
        }
    }
    const FieldDrives::Drive* slot = FieldDrives::byLetter(drive);
    if (!slot || !slot->ready) {
        char buf[64];
        std::snprintf(buf, sizeof buf, "\r\nDrive %c: not ready\r\n", drive);
        shellPrint(ram, echo, nl, buf);
        return;
    }
    char hdr[128];
    if (drive == 'C' && !listPathArg.empty())
        std::snprintf(hdr, sizeof hdr,
            "\r\n Volume in drive %c is %s\r\n Directory of %s\r\n\r\n",
            drive, slot->volume[0] ? slot->volume : "-", listPathArg.c_str());
    else if (drive == 'C' && !dirSubPath.empty())
        std::snprintf(hdr, sizeof hdr,
            "\r\n Volume in drive %c is %s\r\n Directory of C:\\%s\r\n\r\n",
            drive, slot->volume[0] ? slot->volume : "-", dirSubPath.c_str());
    else
        std::snprintf(hdr, sizeof hdr,
            "\r\n Volume in drive %c is %s\r\n Directory of %c:\\\r\n\r\n",
            drive, slot->volume[0] ? slot->volume : "-", drive);
    shellPrint(ram, echo, nl, hdr);

    int shown = 0;
    int count = 0;
    int dirCount = 0;
    std::uint32_t total = 0;

    if (drive == 'C') {
        std::vector<FieldRtxVfs::RichEntry> entries;
        char pathBuf[128];
        if (!listPathArg.empty())
            std::snprintf(pathBuf, sizeof pathBuf, "%s", listPathArg.c_str());
        else if (!dirSubPath.empty())
            std::snprintf(pathBuf, sizeof pathBuf, "C:\\%s", dirSubPath.c_str());
        else
            std::snprintf(pathBuf, sizeof pathBuf, "C:\\");
        const bool ok = FieldRtxVfs::listPathRich(pathBuf, wide, true, entries);
        if (ok) {
            if (!wildcard.empty()) {
                const char* pat = wildcard.c_str();
                const auto slash = wildcard.find_last_of('\\');
                if (slash != std::string::npos)
                    pat = wildcard.c_str() + slash + 1;
                FieldRtxVfs::filterByWildcard(entries, pat);
            }
            for (const auto& fe : entries)
                if (fe.isDir) ++dirCount;
            printRichDirListing(ram, echo, nl, entries, wide, verbose, count, total, shown, 0);
            char sum[96];
            std::snprintf(sum, sizeof sum,
                "       %d file(s)  %d dir(s)  %u bytes\r\n", count - dirCount, dirCount, total);
            shellPrint(ram, echo, nl, sum);
        } else {
            shellPrint(ram, echo, nl, " File Not Found\r\n");
        }
        char freeLine[80];
        const std::uint32_t freeB = FieldAmmoFat::freeBytes();
        std::snprintf(freeLine, sizeof freeLine, "                      %u bytes free\r\n", freeB);
        shellPrint(ram, echo, nl, freeLine);
        return;
    }

    if (drive == 'A' || drive == 'D' || drive == 'E') {
        std::vector<FieldRtxVfs::RichEntry> entries;
        if (drive == 'A') {
            std::vector<FieldDos::FatRootEntry> fat;
            if (FieldDrives::listFloppyRoot(fat)) {
                for (const auto& fe : fat)
                    entries.push_back(FieldRtxVfs::richFromName(fe.name, fe.size, fe.isDir));
            }
        } else if (drive == 'D') {
            std::vector<std::string> names;
            if (FieldCdRom::listRoot(names)) {
                for (const auto& n : names)
                    entries.push_back(FieldRtxVfs::richFromName(n.c_str(), 0, true));
            }
        } else {
            std::vector<std::string> names;
            if (FieldDrives::listHostBridge(names)) {
                for (const auto& n : names)
                    entries.push_back(FieldRtxVfs::richFromName(n.c_str(), 0, false));
            }
        }
        if (!wildcard.empty()) {
            const char* pat = wildcard.c_str();
            FieldRtxVfs::filterByWildcard(entries, pat);
        }
        if (!entries.empty()) {
            for (const auto& fe : entries)
                if (fe.isDir) ++dirCount;
            printRichDirListing(ram, echo, nl, entries, wide, verbose, count, total, shown, 0);
            char sum[80];
            std::snprintf(sum, sizeof sum, "       %d file(s)  %d dir(s)  %u bytes\r\n",
                count - dirCount, dirCount, total);
            shellPrint(ram, echo, nl, sum);
        } else {
            shellPrint(ram, echo, nl,
                drive == 'E'
                    ? " (host bridge empty — drop files in assets/dos/incoming/host/)\r\n"
                    : " File Not Found\r\n");
        }
        return;
    }
}

inline void cmdChkdsk(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    FieldDrives::refresh();
    const FieldAmmoFat::CheckResult chk = FieldAmmoFat::checkVolume();
    const std::uint32_t totalB = FieldPlatform::HD_SIZE_BYTES32;
    const std::uint32_t freeB = chk.freeClusters
        * FieldAmmoFat::geo.spc * FieldAmmoFat::geo.bps;
    char buf[420];
    std::snprintf(buf, sizeof buf,
        "\r\nCHKDSK %s — %s (AMMOFAT field layer)\r\n"
        "%u bytes total disk space\r\n"
        "%u bytes in %u user files\r\n"
        "%u bytes available on disk\r\n"
        "%u bytes total conventional memory\r\n"
        "%u bytes extended memory (XMS)\r\n"
        "Field drives: %s (current %c:)\r\n"
        "CD-ROM D: %s (%u sectors)\r\n"
        "%u allocation units on disk.\r\n"
        "%u files checked — %s\r\n",
        FieldRtxAmmos::DOS_LAYER, currentDir.c_str(),
        totalB,
        totalB > freeB ? totalB - freeB : 0u, chk.filesChecked,
        freeB,
        static_cast<std::uint32_t>(FieldRtxMemory::conventionalKb) * 1024u,
        static_cast<std::uint32_t>(FieldPlatform::EXTENDED_KB) * 1024u,
        FieldDrives::readyLetters(), FieldDrives::currentLetter,
        FieldCdRom::ready ? FieldCdRom::volumeLabel.c_str() : "not mounted",
        FieldCdRom::sectorCount(),
        FieldAmmoFat::geo.spc * FieldAmmoFat::geo.bps,
        chk.filesChecked,
        chk.ok ? "All specified file(s) are OK" : "Volume errors detected");
    shellPrint(ram, echo, nl, buf);
    if (chk.crossLinks)
        shellPrint(ram, echo, nl, "  Cross-linked clusters detected\r\n");
    if (chk.badChains)
        shellPrint(ram, echo, nl, "  Invalid cluster chains detected\r\n");
}

inline void cmdMemoryup(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                        const std::vector<std::string>& args) noexcept {
    const bool opt = args.size() >= 2 && istreq(args[1].c_str(), "/S");
    char buf[520];
    std::snprintf(buf, sizeof buf,
        "\r\nRTX MEMORYUP v7.0 — MemMaker-class optimizer (2026)\r\n"
        "  Conventional   %u KB total / %u KB free\r\n"
        "  Extended       %u KB below 16M\r\n"
        "  XMS pool       %u KB (%s)\r\n"
        "  EMM386/UMB     %s\r\n"
        "  MSCDEX         %s\r\n"
        "  Field Die      %u MB fast RAM\r\n"
        "  Spill RAID     %llu bytes logical\r\n"
        "  Upper (UMB)    %u KB total / %u KB free\r\n"
        "\r\nRecommendations:\r\n"
        "  DOS=HIGH,UMB,UMBNOAUTO  FILES=255  BUFFERS=60,3\r\n"
        "  DEVICE=HIMEM.SYS  DEVICE=EMM386.EXE NOEMS\r\n"
        "  STACKS=18,512  LASTDRIVE=Z\r\n"
        "  Boot tier %uK — grow to %uK on demand (MEMORYUP /S)\r\n",
        static_cast<unsigned>(FieldRtxMemory::conventionalKb),
        FieldRtxVfs::getFreeConvMem() / 1024u,
        static_cast<unsigned>(FieldPlatform::EXTENDED_KB),
        FieldRtxMemory::activeXmsKb(),
        FieldRtxMemory::xmsLive() ? "live" : "idle",
        FieldRtxMemory::emmLive() ? "live" : "idle",
        FieldRtxMemory::mscdexLive() ? "live" : "idle",
        FieldRtxMemory::guestFastMb,
        static_cast<unsigned long long>(FieldPlatform::REPORTED_RAM_BYTES),
        static_cast<unsigned>(FieldPlatform::UMB_KB),
        FieldRtxVfs::getFreeUmbMem() / 1024u,
        static_cast<unsigned>(FieldRtxMemory::bootConventionalKb),
        static_cast<unsigned>(FieldRtxMemory::maxConventionalKb));
    shellPrint(ram, echo, nl, buf);
    if (opt) {
        const bool grew = FieldRtxMemory::growConventional(
            FieldRtxMemory::maxConventionalKb, ram);
        if (grew)
            FieldRtxMemory::guestFastMb = std::max(FieldRtxMemory::guestFastMb, 8u);
        FieldRtxMemory::popGuestFast(ram);
        FieldRtxMemory::growExtenders(ram, FieldCdRom::ready);
        FieldXms::activate(FieldX86Emu::emu);
        FieldEms::activate(FieldX86Emu::emu);
        if (FieldRtxMemory::mscdexLive())
            FieldMscdex::install();
        FieldRtxMemory::syncBios(ram);
        char growBuf[160];
        std::snprintf(growBuf, sizeof growBuf,
            "\r\nMEMORYUP /S — %u KiB conv, %u MiB fast, XMS/EMM386/MSCDEX warm.\r\n"
            "  Upper memory blocks simulated via RTXHOST + AMMOFAT spill.\r\n",
            static_cast<unsigned>(FieldRtxMemory::conventionalKb),
            FieldRtxMemory::guestFastMb);
        shellPrint(ram, echo, nl, growBuf);
        FieldExtensionMap::appendJournal("MEMORYUP", "/S optimize");
    } else {
        shellPrint(ram, echo, nl, "\r\nRun MEMORYUP /S to apply optimal memory profile.\r\n");
    }
}

inline void cmdScandisk(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    FieldDrives::refresh();
    const FieldAmmoFat::CheckResult chk = FieldAmmoFat::checkVolume();
    char buf[480];
    std::snprintf(buf, sizeof buf,
        "\r\nRTX SCANDISK v7.0 — surface + FAT + RAID journal check\r\n"
        "  Volume %s — %s\r\n"
        "  Files checked     %u\r\n"
        "  Cross-links       %u\r\n"
        "  Bad chains        %u\r\n"
        "  Free clusters     %u\r\n"
        "  RAID layer        %s\r\n"
        "  Journal           %s\r\n",
        FieldDos::hdVolumeLabel().c_str(),
        chk.ok ? "OK" : "ERRORS FOUND",
        chk.filesChecked, chk.crossLinks, chk.badChains, chk.freeClusters,
        FieldRaid::ready ? "ready" : "offline",
        std::filesystem::exists(FieldRaid::journalPath()) ? "present" : "clean");
    shellPrint(ram, echo, nl, buf);
    if (!chk.ok) {
        shellPrint(ram, echo, nl, "\r\nSCANDISK recommends CHKDSK /F and AMMOFAT sync.\r\n");
        FieldAmmoFat::syncToDisk();
    } else {
        shellPrint(ram, echo, nl, "\r\nNo errors found. Drive C: is healthy.\r\n");
    }
    FieldExtensionMap::appendJournal("SCANDISK", chk.ok ? "OK" : "REPAIR");
    (void)ram;
}

inline void cmdExtMap(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    FieldExtensionMap::ensure();
    FieldExtensionEditor::open();
    FieldExtensionEditor::paint(ram);
    char buf[64];
    std::snprintf(buf, sizeof buf, "\r\nExtension map — %u entries (F6 in FILECMD)\r\n",
        static_cast<unsigned>(FieldExtensionMap::entries.size()));
    shellPrint(ram, echo, nl, buf);
}

inline void cmdRegQuery(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                        const std::vector<std::string>& args) noexcept {
    FieldRegistry::ensure();
    if (args.size() < 2) {
        shellPrint(ram, echo, nl,
            "\r\nREG QUERY — list HKRTX values\r\n"
            "  Syntax: REG QUERY [path] [key]\r\n"
            "  REG QUERY HKRTX\\Software\\RTX-DOS\\Shell Path\r\n");
        return;
    }
    const std::string path = args[1];
    if (args.size() >= 3) {
        const std::string val = FieldRegistry::getValue(path, args[2]);
        char buf[160];
        std::snprintf(buf, sizeof buf, "\r\n%s\\%s = %s\r\n",
            FieldRegistry::normPath(path).c_str(), args[2].c_str(), val.c_str());
        shellPrint(ram, echo, nl, buf);
        return;
    }
    const std::string sec = FieldRegistry::normPath(path);
    const auto it = FieldRegistry::hive.find(sec);
    shellPrint(ram, echo, nl, "\r\n");
    if (it == FieldRegistry::hive.end()) {
        shellPrint(ram, echo, nl, "  (section not found)\r\n");
        return;
    }
    char hdr[96];
    std::snprintf(hdr, sizeof hdr, "[%s]\r\n", sec.c_str());
    shellPrint(ram, echo, nl, hdr);
    for (const auto& [k, v] : it->second) {
        char line[120];
        std::snprintf(line, sizeof line, "  %s = %s\r\n", k.c_str(), v.c_str());
        shellPrint(ram, echo, nl, line);
    }
}

inline void cmdRegAdd(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                      const std::vector<std::string>& args) noexcept {
    if (args.size() < 4) {
        shellPrint(ram, echo, nl,
            "\r\nREG ADD — set registry value\r\n"
            "  Syntax: REG ADD path key value\r\n");
        return;
    }
    std::string val = args[3];
    for (std::size_t i = 4; i < args.size(); ++i) {
        val += ' ';
        val += args[i];
    }
    FieldRegistry::setValue(args[1], args[2], val);
    FieldRegistry::journal("ADD", args[1].c_str());
    FieldRegistry::applyDosConfig();
    FieldRegistry::syncExtensionMapFromRegistry();
    char buf[120];
    std::snprintf(buf, sizeof buf, "\r\nThe operation completed successfully.\r\n");
    shellPrint(ram, echo, nl, buf);
}

inline void cmdRegDelete(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                         const std::vector<std::string>& args) noexcept {
    if (args.size() < 3) {
        shellPrint(ram, echo, nl,
            "\r\nREG DELETE — remove registry value\r\n"
            "  Syntax: REG DELETE path key\r\n");
        return;
    }
    const bool ok = FieldRegistry::deleteValue(args[1], args[2]);
    if (ok) {
        FieldRegistry::journal("DELETE", args[1].c_str());
        FieldRegistry::syncExtensionMapFromRegistry();
    }
    char buf[96];
    std::snprintf(buf, sizeof buf, "\r\n%s\r\n",
        ok ? "The operation completed successfully." : "ERROR: value not found.");
    shellPrint(ram, echo, nl, buf);
}

inline void cmdRegLoad(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    FieldRegistry::loaded = false;
    FieldRegistry::load();
    FieldRegistry::syncExtensionMapFromRegistry();
    FieldRegistry::applyDosConfig();
    shellPrint(ram, echo, nl, "\r\nRTX Registry reloaded from C:\\TOOLS\\RTXREG.INI\r\n");
}

inline void cmdRegSave(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    FieldRegistry::save();
    shellPrint(ram, echo, nl, "\r\nRTX Registry saved to C:\\TOOLS\\RTXREG.INI\r\n");
}

inline void cmdRegEdit(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    FieldRegistry::ensure();
    FieldRegistryEditor::open();
    FieldRegistryEditor::paint(ram);
    shellPrint(ram, echo, nl, "\r\nRTX Registry Editor — Tab pane | + key | F2 edit | Esc close\r\n");
}

inline void cmdReg(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                   const std::vector<std::string>& args) noexcept {
    if (args.size() < 2) {
        shellPrint(ram, echo, nl,
            "\r\nREG — RTX Registry 2026 (HKRTX hive)\r\n"
            "  REG QUERY [path] [key]   REG ADD path key value\r\n"
            "  REG DELETE path key      REG LOAD   REG SAVE\r\n"
            "  Store: C:\\TOOLS\\RTXREG.INI  |  REGEDIT for GUI\r\n");
        return;
    }
    const std::string& sub = args[1];
    if (istreq(sub.c_str(), "QUERY")) cmdRegQuery(ram, echo, nl, args);
    else if (istreq(sub.c_str(), "ADD")) cmdRegAdd(ram, echo, nl, args);
    else if (istreq(sub.c_str(), "DELETE") || istreq(sub.c_str(), "DEL"))
        cmdRegDelete(ram, echo, nl, args);
    else if (istreq(sub.c_str(), "LOAD")) cmdRegLoad(ram, echo, nl);
    else if (istreq(sub.c_str(), "SAVE")) cmdRegSave(ram, echo, nl);
    else
        shellPrint(ram, echo, nl, "\r\nUnknown REG subcommand. REG /? for help.\r\n");
}

inline void cmdMem(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    const std::uint32_t freeConv = FieldRtxVfs::getFreeConvMem() / 1024u;
    const std::uint32_t freeUmb = FieldRtxVfs::getFreeUmbMem() / 1024u;
    char buf[512];
    std::snprintf(buf, sizeof buf,
        "\r\nMemory Status — RTX-AMMOS DOS 7:\r\n"
        "  Conventional  %uK active (%uK boot / %uK max), %uK free\r\n"
        "  Upper (UMB)   %uK total, %uK free — load HIGH in CONFIG.SYS\r\n"
        "  HMA           %uK at FFFF0h (DOS=HIGH moves kernel here)\r\n"
        "  Extended      %u KB below 1M boundary\r\n"
        "  XMS free      %u KB above 1M (%s)\r\n"
        "  EMM386/UMB    %s | MSCDEX %s\r\n"
        "  Reported RAM  %llu bytes\r\n"
        "  Throttle      %s (%u cycles/frame)\r\n",
        static_cast<unsigned>(FieldRtxMemory::conventionalKb),
        static_cast<unsigned>(FieldRtxMemory::bootConventionalKb),
        static_cast<unsigned>(FieldRtxMemory::maxConventionalKb),
        freeConv,
        static_cast<unsigned>(FieldPlatform::UMB_KB),
        freeUmb,
        static_cast<unsigned>(FieldPlatform::HMA_KB),
        static_cast<unsigned>(FieldPlatform::EXTENDED_KB),
        FieldRtxMemory::activeXmsKb(),
        FieldRtxMemory::xmsLive() ? "live" : "idle",
        FieldRtxMemory::emmLive() ? "live" : "idle",
        FieldRtxMemory::mscdexLive() ? "live" : "idle",
        static_cast<unsigned long long>(FieldPlatform::REPORTED_RAM_BYTES),
        FieldRtxThrottle::activeLabel(),
        FieldRtxThrottle::activeCycles());
    shellPrint(ram, echo, nl, buf);
}

inline void cmdGpu(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    char buf[320];
    std::snprintf(buf, sizeof buf,
        "\r\nRTX Engine — Fields live, Thermo balanced, Sub-micron active.\r\n"
        "  GPU %s | %s\r\n"
        "  Vulkan Field Die | AMMOFAT VFS | deep MZ/PE throttle\r\n",
        FieldRuntimeInfo::gpuShort, FieldRuntimeInfo::masterStatusLine());
    shellPrint(ram, echo, nl, buf);
}

inline void cmdThrottle(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                        const std::vector<std::string>& args) noexcept {
    if (args.size() >= 2) {
        if (istreq(args[1].c_str(), "ON") || istreq(args[1].c_str(), "AUTO")) {
            FieldRtxThrottle::autoEnabled = true;
            shellPrint(ram, echo, nl, "\r\n[THROTTLE] Automagic ON — MZ/PE analysis enabled.\r\n");
            return;
        }
        if (istreq(args[1].c_str(), "OFF")) {
            FieldRtxThrottle::autoEnabled = false;
            FieldRtxThrottle::fieldSetCycles(0);
            shellPrint(ram, echo, nl, "\r\n[THROTTLE] Automagic OFF — full Field Die speed.\r\n");
            return;
        }
        if (args.size() >= 3 && istreq(args[1].c_str(), "TEST")) {
            FieldRtxThrottle::autoThrottleDeep(args[2].c_str());
            char buf[160];
            const auto& a = FieldRtxThrottle::lastAnalysis;
            std::snprintf(buf, sizeof buf,
                "\r\n[THROTTLE] %s — %u cycles/frame (MZ=%s PE=%s pages=%u reloc=%04X)\r\n",
                a.label, FieldRtxThrottle::activeCycles(),
                a.isMz ? "yes" : "no", a.isPe ? "yes" : "no", a.pages, a.reloc);
            shellPrint(ram, echo, nl, buf);
            return;
        }
    }
    char buf[200];
    std::snprintf(buf, sizeof buf,
        "\r\n[THROTTLE] Automagic %s — %s\r\n"
        "  Usage: THROTTLE ON|OFF | THROTTLE TEST file.exe\r\n"
        "  Very old MZ -> %u | Classic MZ -> %u | PE -> full speed\r\n",
        FieldRtxThrottle::autoEnabled ? "ON" : "OFF",
        FieldRtxThrottle::activeLabel(),
        FieldRtxThrottle::kLegacyCycles, FieldRtxThrottle::kClassicCycles);
    shellPrint(ram, echo, nl, buf);
}

inline bool dispatchToolCom(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                            const std::string& name,
                            const std::vector<std::string>& args);

inline bool tryExecExternal(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                            const std::string& cmd,
                            const std::vector<std::string>& args) {
    std::string base = cmd;
    std::string ext;
    const auto dot = base.find_last_of('.');
    if (dot != std::string::npos) {
        ext = base.substr(dot);
        for (auto& c : ext) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        base = base.substr(0, dot);
    } else {
        ext = ".COM";
        base = cmd;
    }
    std::string path;
    if (!resolveOnPath(cmd, path)) {
        path = resolveAnyPath('C', cmd);
        if (!FieldAmmoVfs::pathExists(path.c_str())) return false;
    }
    const auto pdot = path.find_last_of('.');
    if (pdot != std::string::npos) {
        ext = path.substr(pdot);
        for (auto& c : ext) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }

    if (ext != ".COM" && ext != ".EXE" && ext != ".BAT") {
        FieldExtensionMap::ensure();
        if (FieldExtensionMap::findForName(path.c_str())) {
            const std::string line = FieldExtensionMap::buildLaunchLine(path.c_str(), args);
            execLine(line.c_str(), ram, echo, nl, defaultPrompt);
            return true;
        }
        return false;
    }

    if (dispatchToolCom(ram, echo, nl, base, args))
        return true;

    FieldRtxThrottle::autoThrottleDeep(path.c_str());
    char tbuf[200];
    std::snprintf(tbuf, sizeof tbuf, "\r\n[THROTTLE] %s\r\n",
        FieldRtxThrottle::lastAnalysis.label);
    shellPrintAttr(ram, echo, nl, tbuf,
        FieldRtxThrottle::lastAnalysis.cycles == FieldRtxThrottle::kLegacyCycles ? 0x0Eu : 0x0Au);

    if (ext == ".BAT") {
        std::vector<std::uint8_t> data;
        if (!FieldAmmoVfs::readPath(path.c_str(), data) || data.empty()) {
            shellPrint(ram, echo, nl, "\r\nBatch file not found\r\n");
            return true;
        }
        std::string script(reinterpret_cast<const char*>(data.data()), data.size());
        int depth = 0;
        for (std::size_t i = 0; i < script.size(); ) {
            std::size_t end = script.find('\n', i);
            if (end == std::string::npos) end = script.size();
            std::string line = script.substr(i, end - i);
            i = end + 1;
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
            while (!line.empty() && line.front() == ' ') line.erase(0, 1);
            if (line.empty() || line[0] == ';' || line[0] == '#') continue;
            if (line.size() >= 3 && istreq(line.substr(0, 3).c_str(), "REM")) continue;
            if (line.size() >= 8 && istreq(line.substr(0, 8).c_str(), "@ECHO OFF")) continue;
            if (++depth > 48) break;
            execLine(line.c_str(), ram, echo, nl, defaultPrompt);
        }
        return true;
    }
    if (FieldAmmoExec::launchFromGuest(ram, path.c_str())) {
        char buf[160];
        std::snprintf(buf, sizeof buf, "\r\nRunning %s — Esc quits to shell\r\n", path.c_str());
        shellPrint(ram, echo, nl, buf);
    } else {
        shellPrint(ram, echo, nl, "\r\nCannot launch program (missing file or load error)\r\n");
    }
    (void)args;
    return true;
}

inline void cmdDevices(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                       const std::vector<std::string>& args) noexcept {
    if (args.size() >= 2 && (istreq(args[1].c_str(), "/T") || istreq(args[1].c_str(), "/TEXT"))) {
        shellPrint(ram, echo, nl, "\r\nRTX Desktop device rack:\r\n");
        for (std::size_t i = 0; i < FieldRtxAmmos::deviceCount(); ++i) {
            const auto& d = FieldRtxAmmos::kDevices[i];
            char line[96];
            std::snprintf(line, sizeof line, "  [%s] %s — %s  port=%04X irq=%u\r\n",
                d.enabled ? "ON " : "off",
                d.id, d.name, d.basePort, d.irq);
            shellPrint(ram, echo, nl, line);
        }
        char ports[384]{};
        FieldPorts::formatPortsBlock(ports, sizeof ports);
        shellPrint(ram, echo, nl, ports);
        return;
    }
    FieldDeviceViewer::open();
    FieldDeviceViewer::paint(ram);
    shellPrint(ram, echo, nl,
        "\r\nRTX Device Manager — PC chassis view (Esc close, DEVICES /T for text list)\r\n");
}

inline void cmdPorts(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    char buf[384]{};
    FieldPorts::formatPortsBlock(buf, sizeof buf);
    shellPrint(ram, echo, nl, buf);
}

inline void cmdAmmoRun(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                       const std::vector<std::string>& args);
inline void cmdFieldc(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                      const std::vector<std::string>& args);
inline void cmdAmmoAsm(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                       const std::vector<std::string>& args);
inline void cmdAmmoLink(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                        const std::vector<std::string>& args);
inline void cmdAmmoCc(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                      const std::vector<std::string>& args);
inline void cmdAmmoDbg(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                       const std::vector<std::string>& args);

inline void cmdSound(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                     const std::vector<std::string>& args) noexcept {
    if (args.size() >= 2 && istreq(args[1].c_str(), "DIR")) {
        cmdDir(ram, echo, nl, {"DIR", "C:\\SOUND"});
        return;
    }
    if (args.size() >= 2 && istreq(args[1].c_str(), "TEST")) {
        FieldAudioRack::playTestTone();
        shellPrint(ram, echo, nl, "\r\nSOUND TEST — OPL tone via SDL3 audio rack\r\n");
        return;
    }
    if (args.size() >= 2 && istreq(args[1].c_str(), "BEEP")) {
        FieldAudioRack::playTestTone();
        shellPrint(ram, echo, nl, "\r\nPC speaker / OPL beep queued\r\n");
        return;
    }
    if (args.size() >= 3 && istreq(args[1].c_str(), "RUN")) {
        cmdAmmoRun(ram, echo, nl, {"AMMORUN", std::string("C:\\SOUND\\") + args[2]});
        return;
    }
    FieldAudioRack::playTestTone();
    shellPrint(ram, echo, nl,
        "\r\nRTX COMMAND.COM — SOUND folder (C:\\SOUND)\r\n"
        "  Devices: SB16 OPL GUS ESS Covox MPU-401 PC Speaker\r\n"
        "  SOUND DIR     — list C:\\SOUND test programs\r\n"
        "  SOUND TEST    — OPL test tone\r\n"
        "  SOUND BEEP    — speaker beep\r\n"
        "  SOUND RUN file — AMMORUN C:\\SOUND\\file\r\n"
        "  AMMOASM/AMMOLINK/AMMORUN on .ASM in SOUND\\\r\n");
    std::vector<FieldDos::FatRootEntry> entries;
    if (FieldAmmoVfs::listPath("C:\\SOUND", entries) && !entries.empty()) {
        shellPrint(ram, echo, nl, "\r\n  C:\\SOUND contents:\r\n");
        for (const auto& fe : entries) {
            char line[72];
            if (fe.isDir)
                std::snprintf(line, sizeof line, "    %-12s <DIR>\r\n", fe.name);
            else
                std::snprintf(line, sizeof line, "    %-12s %u bytes\r\n", fe.name, fe.size);
            shellPrint(ram, echo, nl, line);
        }
    } else {
        shellPrint(ram, echo, nl, "  (C:\\SOUND empty — rebuild HD with ./linux.sh dos)\r\n");
    }
}

inline void cmdCdRom(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    FieldRtxMemory::growMscdexExtender();
    FieldMscdex::install();
    shellPrint(ram, echo, nl, "\r\n");
    shellPrint(ram, echo, nl, FieldMscdex::statusLine().c_str());
    shellPrint(ram, echo, nl, "\r\n");
    if (!FieldCdRom::ready) {
        shellPrint(ram, echo, nl,
            "  Plug USB DVD (/dev/sr0) or drop .iso in assets/dos/incoming/cd/ then MOUNT CD\r\n");
        return;
    }
    char buf[256];
    std::snprintf(buf, sizeof buf, "  %s: %s\r\n  Sectors: %u (2048-byte)\r\n",
        FieldCdRom::isHostDevice() ? "Device" : "ISO",
        FieldCdRom::isoPath.c_str(), FieldCdRom::sectorCount());
    shellPrint(ram, echo, nl, buf);
    std::vector<std::string> names;
    if (FieldCdRom::listRoot(names) && !names.empty()) {
        shellPrint(ram, echo, nl, "  Root entries:\r\n");
        for (const auto& n : names) {
            shellPrint(ram, echo, nl, "    ");
            shellPrint(ram, echo, nl, n.c_str());
            shellPrint(ram, echo, nl, "\r\n");
        }
    }
}

inline void cmdMount(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                     const std::vector<std::string>& args) noexcept {
    FieldRtxVfs::vfsInit();
    if (args.size() >= 2 && istreq(args[1].c_str(), "CD")) {
        if (FieldDrives::mountCd()) {
            char buf[128];
            std::snprintf(buf, sizeof buf, "\r\nMounted CD-ROM D: %s\r\n",
                FieldCdRom::volumeLabel.c_str());
            shellPrint(ram, echo, nl, buf);
        } else {
            shellPrint(ram, echo, nl,
                "\r\nNo host optical drive and no ISO in assets/dos/incoming/cd/\r\n");
        }
        return;
    }
    if (args.size() >= 3 && istreq(args[1].c_str(), "ZIP")) {
        const std::string zipPath = resolveAnyPath('C', args[2]);
        if (FieldRtxVfs::mountZip(zipPath.c_str())) {
            const auto& mnt = FieldRtxVfs::mounts.back();
            std::string label = mnt.mountPoint.size() > 3u
                ? mnt.mountPoint.substr(3u) : mnt.mountPoint;
            char buf[160];
            std::snprintf(buf, sizeof buf,
                "\r\nMounted ZIP archive at C:\\%s (%zu entries)\r\n"
                "  DIR C:\\%s to browse | colorful metadata on files\r\n",
                label.c_str(), mnt.entries.size(), label.c_str());
            shellPrint(ram, echo, nl, buf);
        } else {
            shellPrint(ram, echo, nl, "\r\nZIP mount failed — file not found or invalid archive\r\n");
        }
        return;
    }
    if (args.size() >= 3 && istreq(args[1].c_str(), "ISO")) {
        const std::string isoPath = resolveAnyPath('C', args[2]);
        if (FieldRtxVfs::mountIsoFromC(isoPath.c_str())) {
            char buf[160];
            std::snprintf(buf, sizeof buf,
                "\r\nMounted ISO from %s as D: %s\r\n",
                isoPath.c_str(), FieldCdRom::volumeLabel.c_str());
            shellPrint(ram, echo, nl, buf);
            FieldDrives::invalidate();
        } else {
            shellPrint(ram, echo, nl, "\r\nISO mount failed\r\n");
        }
        return;
    }
    if (args.size() >= 2 && istreq(args[1].c_str(), "LIST")) {
        char buf[512];
        FieldRtxVfs::formatMountList(buf, sizeof buf);
        shellPrint(ram, echo, nl, "\r\nArchive mounts:\r\n");
        shellPrint(ram, echo, nl, buf);
        return;
    }
    if (args.size() >= 3 && istreq(args[1].c_str(), "UNMOUNT")) {
        if (istreq(args[2].c_str(), "ALL")) {
            FieldRtxVfs::unmountAll();
            FieldDrives::invalidate();
            shellPrint(ram, echo, nl, "\r\nAll archive mounts removed.\r\n");
        } else if (istreq(args[2].c_str(), "ISO") || istreq(args[2].c_str(), "CD")) {
            FieldRtxVfs::unmountIso();
            FieldDrives::invalidate();
            shellPrint(ram, echo, nl, "\r\nISO/CD unmounted.\r\n");
        } else if (FieldRtxVfs::unmountZip(args[2].c_str())) {
            shellPrint(ram, echo, nl, "\r\nZIP unmounted.\r\n");
        } else {
            shellPrint(ram, echo, nl, "\r\nUNMOUNT failed — unknown mount\r\n");
        }
        return;
    }
    shellPrint(ram, echo, nl,
        "\r\nUsage: MOUNT CD|ZIP file|ISO file|LIST\r\n"
        "       MOUNT UNMOUNT label|ISO|CD|ALL\r\n");
}

inline void cmdCd(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                  const std::vector<std::string>& args) noexcept {
    if (args.size() < 2) {
        shellPrint(ram, echo, nl, "\r\n");
        shellPrint(ram, echo, nl, currentDir.c_str());
        shellPrint(ram, echo, nl, "\r\n");
        return;
    }
    if (cdTo(args[1])) {
        syncPromptFromDrives();
        return;
    }
    shellPrint(ram, echo, nl, "\r\nInvalid directory\r\n");
}

inline void cmdPwd(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    syncPromptFromDrives();
    shellPrint(ram, echo, nl, "\r\n");
    shellPrint(ram, echo, nl, currentDir.c_str());
    shellPrint(ram, echo, nl, "\r\n");
}

inline void cmdAmmoText(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                        const std::vector<std::string>& args) noexcept {
    if (args.size() >= 2 && FieldAmmoText::wantsHelpSwitch(args)) {
        FieldAmmoText::printHelp(echo, nl, ram);
        return;
    }
    if (FieldDrives::currentLetter != 'C') {
        shellPrint(ram, echo, nl, "\r\nAmmoText only on C: (AMMOFAT)\r\n");
        return;
    }
    if (args.size() >= 2) {
        const std::string p = resolveAnyPath('C', args[1]);
        FieldAmmoText::open(ram, p.c_str());
    } else {
        FieldAmmoText::open(ram, "");
    }
}

inline void cmdEdit(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                    const std::vector<std::string>& args) noexcept {
    (void)echo; (void)nl;
    if (FieldDrives::currentLetter != 'C') {
        shellPrint(ram, echo, nl, "\r\nEDIT requires C: (AMMOFAT)\r\n");
        return;
    }
    if (args.size() >= 2) {
        const std::string p = resolveAnyPath('C', args[1]);
        FieldMonacoEdit::open(ram, p.c_str());
    } else {
        FieldMonacoEdit::open(ram, "");
    }
    graphicsActive = true;
}

inline void cmdThemes(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    (void)echo; (void)nl;
    FieldRtxThemes::init();
    FieldRtxGui::initTextMode(ram);
    FieldRtxThemePicker::open();
    FieldRtxThemePicker::paint(ram);
    graphicsActive = true;
}

inline void cmdMd(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                  const std::vector<std::string>& args) noexcept {
    if (args.size() < 2) {
        shellPrint(ram, echo, nl, "\r\nRequired parameter missing\r\n");
        return;
    }
    if (FieldDrives::currentLetter != 'C') {
        shellPrint(ram, echo, nl, "\r\nMD only on C: (AMMOFAT)\r\n");
        return;
    }
    const std::string path = resolveCPath(args[1]);
    if (FieldAmmoVfs::mkdirPath(path.c_str()))
        shellPrint(ram, echo, nl, "\r\n");
    else
        shellPrint(ram, echo, nl, "\r\nUnable to create directory\r\n");
}

inline void cmdRd(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                  const std::vector<std::string>& args) noexcept {
    if (args.size() < 2) {
        shellPrint(ram, echo, nl, "\r\nRequired parameter missing\r\n");
        return;
    }
    const std::string path = resolveCPath(args[1]);
    if (FieldAmmoVfs::rmdirPath(path.c_str()))
        shellPrint(ram, echo, nl, "\r\n");
    else
        shellPrint(ram, echo, nl, "\r\nDirectory not empty or not found\r\n");
}

inline void cmdDel(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                   const std::vector<std::string>& args) noexcept {
    if (args.size() < 2) {
        shellPrint(ram, echo, nl, "\r\nRequired parameter missing\r\n");
        return;
    }
    std::string arg = args[1];
    const auto slash = arg.find_last_of('\\');
    const char* pat = slash != std::string::npos ? arg.c_str() + slash + 1 : arg.c_str();
    if (FieldRtxVfs::hasWildcard(pat)) {
        char dir[128];
        if (slash != std::string::npos)
            std::snprintf(dir, sizeof dir, "%s", resolveCPath(arg.substr(0, slash)).c_str());
        else if (!dirSubPath.empty())
            std::snprintf(dir, sizeof dir, "C:\\%s", dirSubPath.c_str());
        else
            std::snprintf(dir, sizeof dir, "C:\\");
        std::vector<std::string> hits;
        if (!FieldRtxVfs::expandWildcardInDir(dir, pat, hits)) {
            shellPrint(ram, echo, nl, "\r\nFile not found\r\n");
            return;
        }
        int n = 0;
        for (const auto& h : hits)
            if (FieldAmmoVfs::deletePath(h.c_str())) ++n;
        char buf[64];
        std::snprintf(buf, sizeof buf, "\r\n        %d file(s) deleted\r\n", n);
        shellPrint(ram, echo, nl, buf);
        FieldRtxVfs::vfsReload();
        return;
    }
    const std::string path = resolveCPath(arg);
    if (FieldAmmoVfs::deletePath(path.c_str())) {
        shellPrint(ram, echo, nl, "\r\n");
        FieldRtxVfs::vfsReload();
    } else
        shellPrint(ram, echo, nl, "\r\nFile not found\r\n");
}

inline void cmdCopy(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                    const std::vector<std::string>& args) noexcept {
    bool recursive = false;
    std::size_t srcIdx = 1;
    std::size_t dstIdx = 2;
    if (args.size() >= 2 && istreq(args[1].c_str(), "/S")) {
        recursive = true;
        srcIdx = 2;
        dstIdx = 3;
    }
    if (args.size() <= dstIdx) {
        shellPrint(ram, echo, nl, "\r\nRequired parameter missing\r\n");
        return;
    }
    const std::string src = resolveCPath(args[srcIdx]);
    const std::string dst = resolveCPath(args[dstIdx]);
    if (recursive && FieldAmmoVfs::isDirectory(src.c_str())) {
        const int n = FieldAmmoVfs::copyTree(src.c_str(), dst.c_str());
        char buf[64];
        std::snprintf(buf, sizeof buf, "\r\n        %d file(s) copied\r\n", n);
        shellPrint(ram, echo, nl, buf);
        FieldRtxVfs::vfsReload();
        FieldDrives::invalidate();
        return;
    }
    std::string arg = args[srcIdx];
    const auto slash = arg.find_last_of('\\');
    const char* pat = slash != std::string::npos ? arg.c_str() + slash + 1 : arg.c_str();
    if (FieldRtxVfs::hasWildcard(pat)) {
        char dir[128];
        if (slash != std::string::npos)
            std::snprintf(dir, sizeof dir, "%s", resolveCPath(arg.substr(0, slash)).c_str());
        else if (!dirSubPath.empty())
            std::snprintf(dir, sizeof dir, "C:\\%s", dirSubPath.c_str());
        else
            std::snprintf(dir, sizeof dir, "C:\\");
        std::vector<std::string> hits;
        if (!FieldRtxVfs::expandWildcardInDir(dir, pat, hits)) {
            shellPrint(ram, echo, nl, "\r\nFile not found\r\n");
            return;
        }
        int n = 0;
        for (const auto& h : hits) {
            char leaf[32];
            const char* name = std::strrchr(h.c_str(), '\\');
            std::snprintf(leaf, sizeof leaf, "%s", name ? name + 1 : h.c_str());
            char target[160];
            std::snprintf(target, sizeof target, "%s\\%s", dst.c_str(), leaf);
            if (FieldAmmoVfs::copyPath(h.c_str(), target)) ++n;
        }
        char buf[64];
        std::snprintf(buf, sizeof buf, "\r\n        %d file(s) copied\r\n", n);
        shellPrint(ram, echo, nl, buf);
        FieldRtxVfs::vfsReload();
        return;
    }
    if (FieldAmmoVfs::copyPath(src.c_str(), dst.c_str())) {
        shellPrint(ram, echo, nl, "\r\n        1 file(s) copied\r\n");
        FieldRtxVfs::vfsReload();
    } else {
        shellPrint(ram, echo, nl, "\r\nFile not found\r\n");
    }
}

inline void cmdRen(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                   const std::vector<std::string>& args) noexcept {
    if (args.size() < 3) {
        shellPrint(ram, echo, nl, "\r\nRequired parameter missing\r\n");
        return;
    }
    const std::string src = resolveCPath(args[1]);
    std::string dstArg = args[2];
    std::string dst;
    if (dstArg.find('\\') != std::string::npos || dstArg.find(':') != std::string::npos)
        dst = resolveCPath(dstArg);
    else {
        FieldAmmoVfs::DirView view{};
        std::string leaf;
        if (!FieldAmmoVfs::openDirPath(src.c_str(), view, leaf, true)) {
            shellPrint(ram, echo, nl, "\r\nFile not found\r\n");
            return;
        }
        std::string parent = dirSubPath;
        if (!parent.empty())
            dst = "C:\\" + parent + "\\" + normalizeSubPath(dstArg);
        else
            dst = "C:\\" + normalizeSubPath(dstArg);
    }
    if (FieldAmmoVfs::renamePath(src.c_str(), dst.c_str()))
        shellPrint(ram, echo, nl, "\r\n        1 file(s) renamed\r\n");
    else
        shellPrint(ram, echo, nl, "\r\nFile not found\r\n");
}

inline void cmdTree(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                    const std::vector<std::string>& args) noexcept {
    FieldRtxVfs::vfsInit();
    std::string root = "C:\\";
    if (args.size() >= 2)
        root = resolveCPath(args[1]);
    shellPrint(ram, echo, nl, "\r\n");
    char line[96];
    std::snprintf(line, sizeof line, "Directory PATH listing for %s\r\n", root.c_str());
    shellPrint(ram, echo, nl, line);
    std::function<void(const std::string&, const std::string&)> walk;
    walk = [&](const std::string& path, const std::string& prefix) {
        std::vector<FieldRtxVfs::RichEntry> entries;
        if (!FieldRtxVfs::listPathRich(path.c_str(), false, true, entries)) return;
        for (const auto& fe : entries) {
            char row[80];
            std::snprintf(row, sizeof row, "%s%s", prefix.c_str(), fe.name);
            shellPrintAttr(ram, echo, nl, row, fe.vgaAttr);
            shellPrint(ram, echo, nl, "\r\n");
            if (fe.isDir) {
                std::string sub = path;
                if (sub.back() != '\\') sub += "\\";
                sub += fe.name;
                walk(sub, prefix + "  ");
            }
        }
    };
    char rootLine[64];
    std::snprintf(rootLine, sizeof rootLine, "%s\r\n", root.c_str());
    shellPrint(ram, echo, nl, rootLine);
    walk(root, "");
}

inline void cmdWhere(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                     const std::vector<std::string>& args) noexcept {
    if (args.size() < 2) {
        shellPrint(ram, echo, nl, "\r\nUsage: WHERE command\r\n");
        return;
    }
    std::string cmd = args[1];
    if (cmd.find('.') == std::string::npos) cmd += ".COM";
    std::string path;
    shellPrint(ram, echo, nl, "\r\n");
    if (resolveOnPath(cmd, path))
        shellPrint(ram, echo, nl, (path + "\r\n").c_str());
    else
        shellPrint(ram, echo, nl, "File not found\r\n");
}

inline void cmdMore(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                    const std::vector<std::string>& args) noexcept {
    if (args.size() < 2) {
        shellPrint(ram, echo, nl, "\r\nUsage: MORE filename\r\n");
        return;
    }
    std::vector<std::uint8_t> data;
    const std::string path = resolveAnyPath('C', args[1]);
    if (!FieldAmmoVfs::readPath(path.c_str(), data) || data.empty()) {
        shellPrint(ram, echo, nl, "\r\nFile not found\r\n");
        return;
    }
    shellPrint(ram, echo, nl, "\r\n");
    int lines = 0;
    for (std::uint8_t b : data) {
        if (b == '\r') continue;
        if (b == '\n') {
            nl(ram);
            if (++lines >= 20) {
                shellPrint(ram, echo, nl, "-- More --");
                lines = 0;
            }
        } else if (b >= 32 && b < 127)
            echo(ram, static_cast<char>(b));
    }
    shellPrint(ram, echo, nl, "\r\n");
}

inline void cmdFind(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                    const std::vector<std::string>& args) noexcept {
    if (args.size() < 3) {
        shellPrint(ram, echo, nl, "\r\nUsage: FIND \"text\" file\r\n");
        return;
    }
    std::string needle = args[1];
    if (needle.size() >= 2 && needle.front() == '"' && needle.back() == '"')
        needle = needle.substr(1, needle.size() - 2);
    std::vector<std::uint8_t> data;
    const std::string path = resolveAnyPath('C', args[2]);
    if (!FieldAmmoVfs::readPath(path.c_str(), data)) {
        shellPrint(ram, echo, nl, "\r\nFile not found\r\n");
        return;
    }
    std::string text(reinterpret_cast<const char*>(data.data()), data.size());
    int hits = 0;
    for (std::size_t p = 0; (p = text.find(needle, p)) != std::string::npos; ++p) {
        ++hits;
        if (hits <= 8) {
            char buf[96];
            std::snprintf(buf, sizeof buf, "[%s] match at offset %zu\r\n",
                path.c_str(), p);
            shellPrint(ram, echo, nl, buf);
        }
    }
    char sum[48];
    std::snprintf(sum, sizeof sum, "\r\n%d occurrence(s)\r\n", hits);
    shellPrint(ram, echo, nl, sum);
}

inline void cmdAttrib(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                      const std::vector<std::string>& args) noexcept {
    if (args.size() < 2) {
        shellPrint(ram, echo, nl, "\r\nUsage: ATTRIB file\r\n");
        return;
    }
    const std::string path = resolveAnyPath('C', args[1]);
    std::vector<FieldRtxVfs::RichEntry> entries;
    std::string dir = path;
    std::string leaf = args[1];
    const auto slash = path.find_last_of('\\');
    if (slash != std::string::npos) {
        dir = path.substr(0, slash);
        leaf = path.substr(slash + 1);
    }
    if (!FieldRtxVfs::listPathRich(dir.c_str(), false, true, entries)) {
        shellPrint(ram, echo, nl, "\r\nFile not found\r\n");
        return;
    }
    for (const auto& e : entries) {
        if (!istreq(e.name, leaf.c_str())) continue;
        char buf[160];
        std::snprintf(buf, sizeof buf,
            "\r\n%s  %s  %u bytes  attr=%02X\r\n",
            path.c_str(), e.isDir ? "<DIR>" : "FILE", e.size, e.vgaAttr);
        shellPrint(ram, echo, nl, buf);
        if (e.desc && e.desc[0]) {
            char meta[96];
            std::snprintf(meta, sizeof meta, "  %s\r\n", e.desc);
            shellPrint(ram, echo, nl, meta);
        }
        FieldRtxVfs::FileMeta fm;
        if (FieldRtxVfs::lookupMeta(e.name, fm) && !fm.url.empty()) {
            char url[96];
            std::snprintf(url, sizeof url, "  %s\r\n", fm.url.c_str());
            shellPrint(ram, echo, nl, url);
        }
        return;
    }
    shellPrint(ram, echo, nl, "\r\nFile not found\r\n");
}

inline void cmdPath(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    char buf[160];
    formatPathLine(buf, sizeof buf);
    shellPrint(ram, echo, nl, "\r\n");
    shellPrint(ram, echo, nl, buf);
    shellPrint(ram, echo, nl, "\r\n  C: AMMOFAT VFS | D: MSCDEX | E: host incoming bridge\r\n");
}

inline void cmdAmmofat(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    char buf[256];
    if (FieldAmmoFat::formatInfo(buf, sizeof buf))
        shellPrint(ram, echo, nl, "\r\n");
    shellPrint(ram, echo, nl, buf);
}

inline void toolReply(std::uint8_t* ram, EchoFn echo, NewlineFn nl, const FieldAmmoTools::ToolResult& tr) {
    char buf[160];
    std::snprintf(buf, sizeof buf, "\r\n%s\r\n", tr.ok ? tr.message.c_str() : tr.message.c_str());
    shellPrint(ram, echo, nl, tr.ok ? "\r\n" : "\r\nError: ");
    shellPrint(ram, echo, nl, buf);
}

inline void cmdAmmoAsm(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                       const std::vector<std::string>& args) {
    toolReply(ram, echo, nl, FieldAmmoTools::asmFile(args));
}

inline void cmdAmmoSys(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                       const std::vector<std::string>& args) {
    if (args.size() >= 2 && FieldRtxGui::argIsHelp(args[1])) {
        shellPrint(ram, echo, nl,
            "\r\nAMMOSYS — assemble RTXFL field-layer .SYS driver\r\n"
            "  usage: AMMOSYS C:\\DRIVERS\\RTXSB.ASM [minsize]\r\n"
            "  Example: AMMOSYS RTXSB.ASM 160\r\n");
        return;
    }
    toolReply(ram, echo, nl, FieldAmmoTools::sysFile(args));
}

inline void cmdAmmoLink(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                        const std::vector<std::string>& args) {
    toolReply(ram, echo, nl, FieldAmmoTools::linkFile(args));
}

inline void cmdAmmoCc(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                      const std::vector<std::string>& args) {
    toolReply(ram, echo, nl, FieldAmmoTools::ccFile(args));
}

inline void cmdFieldc(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                      const std::vector<std::string>& args) {
    if (args.size() >= 2 && FieldRtxGui::argIsHelp(args[1])) {
        shellPrint(ram, echo, nl,
            "\r\nFIELDC — Field Compiler v4 (.fld → AMMO .OBJ)\r\n"
            "  usage: FIELDC file.fld\r\n"
            "  print \"text\"  return N  field name  era any\r\n"
            "  BUILD PADTEST — one-shot if PADTEST.FLD on C:\r\n");
        return;
    }
    toolReply(ram, echo, nl, FieldAmmoTools::fieldcFile(args));
}

inline void cmdPadTest(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                       const std::vector<std::string>& args) noexcept {
    if (args.size() >= 2 && FieldRtxGui::argIsHelp(args[1])) {
        shellPrint(ram, echo, nl,
            "\r\nPADTEST — Xbox360 / SDL gamepad tester\r\n"
            "  Live button, stick, trigger view\r\n"
            "  Compile: FIELDC C:\\SAMPLES\\PADTEST.FLD  BUILD PADTEST\r\n");
        return;
    }
    FieldPadTest::open(ram);
    (void)echo; (void)nl;
}

inline void cmdBrowser(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                       const std::vector<std::string>& args) noexcept {
    std::string url = "https://amouranth.com";
    if (args.size() >= 2 && !FieldRtxGui::argIsHelp(args[1]))
        url = args[1];
    if (args.size() >= 2 && FieldRtxGui::argIsHelp(args[1])) {
        shellPrint(ram, echo, nl,
            "\r\nBROWSER [url] — hook OS default browser (Firefox/Chrome) to panel\r\n"
            "  detect: python3 scripts/detect_browsers.py\r\n");
        return;
    }
    if (FieldAmmoBrowser::openUrl(url.c_str(), ram))
        shellPrint(ram, echo, nl, "\r\nBrowser hooked to OS default — Esc close panel\r\n");
    else
        shellPrint(ram, echo, nl, "\r\nBrowser panel failed to open\r\n");
}

inline void cmdDoom(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                    const std::vector<std::string>& args) noexcept {
    if (args.size() >= 2 && FieldRtxGui::argIsHelp(args[1])) {
        shellPrint(ram, echo, nl,
            "\r\nDOOM — same as any DOS game: copy to C:\\ then run DOOM.EXE\r\n"
            "  install: python3 scripts/install_shareware_doom.py\r\n");
        return;
    }
    execLine("DOOM.EXE", ram, echo, nl, defaultPrompt);
}

inline std::string emuCmdPath(const std::vector<std::string>& args, bool& setup) noexcept {
    setup = !args.empty() && istreq(args[0].c_str(), "SETUP");
    return setup ? FieldAmmoTools::dosPath(args, 1, "")
                 : FieldAmmoTools::dosPath(args, 0, "");
}

inline void cmdSnes(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                    const std::vector<std::string>& args) noexcept {
    (void)echo;
    (void)nl;
    bool setup = false;
    const std::string path = emuCmdPath(args, setup);
    if (!path.empty()) FieldSnes::open(ram, path.c_str(), setup);
    else FieldSnes::open(ram, nullptr, setup);
}

inline void cmdGenesis(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                       const std::vector<std::string>& args) noexcept {
    (void)echo;
    (void)nl;
    bool setup = false;
    const std::string path = emuCmdPath(args, setup);
    if (!path.empty()) FieldGenesis::open(ram, path.c_str(), setup);
    else FieldGenesis::open(ram, nullptr, setup);
}

inline void cmdSms(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                   const std::vector<std::string>& args) noexcept {
    (void)echo;
    (void)nl;
    bool setup = false;
    const std::string path = emuCmdPath(args, setup);
    if (!path.empty()) FieldSms::open(ram, path.c_str(), setup);
    else FieldSms::open(ram, nullptr, setup);
}

inline void cmdA2600(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                     const std::vector<std::string>& args) noexcept {
    (void)echo;
    (void)nl;
    bool setup = false;
    const std::string path = emuCmdPath(args, setup);
    if (!path.empty()) FieldA2600::open(ram, path.c_str(), setup);
    else FieldA2600::open(ram, nullptr, setup);
}

inline void cmdNes(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                   const std::vector<std::string>& args) noexcept {
    const auto req = FieldAmmoNesCli::parse(args);
    if (req.showHelp) {
        shellPrint(ram, echo, nl, FieldAmmoNesCli::kHelpText);
        return;
    }
    if (req.openSetup) {
        FieldNes::open(ram, nullptr, nullptr, true, false);
        return;
    }
    if (req.openPadMap) {
        FieldNes::open(ram, nullptr, nullptr, true, true);
        return;
    }
    if (req.applyOnly) {
        FieldNes::applyConfig(req.opts);
        FieldAmmoNesConfig::save(req.opts);
        shellPrint(ram, echo, nl, "\r\nAmmoNES options applied and saved.\r\n");
        return;
    }
    if (req.doImport) {
        std::vector<std::string> copied, skipped;
        const int n = FieldNesImport::importAll(copied, skipped);
        shellPrint(ram, echo, nl, "\r\nNES IMPORT — ");
        char buf[48];
        std::snprintf(buf, sizeof buf, "%d ROM(s) copied to C:\\NES\\\r\n", n);
        shellPrint(ram, echo, nl, buf);
        for (const auto& c : copied) {
            shellPrint(ram, echo, nl, "  ");
            shellPrint(ram, echo, nl, c.c_str());
            shellPrint(ram, echo, nl, "\r\n");
        }
        std::string any;
        if (FieldNesImport::findAnyRom(any)) {
            shellPrint(ram, echo, nl, "\r\nReady — NES ");
            shellPrint(ram, echo, nl, any.c_str());
            shellPrint(ram, echo, nl, "\r\n");
        } else
            shellPrint(ram, echo, nl,
                "\r\nDrop .nes files in assets/dos/incoming/nes/ then NES IMPORT\r\n");
        return;
    }
    FieldNes::applyConfig(req.opts);
    FieldAmmoNesConfig::save(req.opts);
    std::string p = req.romPath;
    if (p.empty() && !FieldNesImport::findAnyRom(p))
        p = "C:\\NES\\DEMO.NES";
    if (!p.empty()) {
        FieldAmmoNesConfig::g.lastRom = p;
        FieldNes::open(ram, p.c_str(), &FieldAmmoNesConfig::g);
    }
    if (req.saveState && FieldNes::active) {
        FieldNes::saveState(req.stateSlot);
        shellPrint(ram, echo, nl, "\r\nAmmoNES state saved.\r\n");
    }
    if (req.loadState && FieldNes::active) {
        FieldNes::loadState(req.stateSlot);
        shellPrint(ram, echo, nl, "\r\nAmmoNES state loaded.\r\n");
    }
    if (!p.empty() && !FieldNes::active)
        shellPrint(ram, echo, nl, "\r\nNES: cannot load ROM — try NES IMPORT\r\n");
}

inline void cmdAmmoBuild(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                         const std::vector<std::string>& args) {
    toolReply(ram, echo, nl, FieldAmmoTools::buildAll(args));
}

inline void cmdAmmoRun(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                       const std::vector<std::string>& args) {
    std::string path = FieldAmmoTools::dosPath(args, 1, "");
    if (path.empty()) {
        shellPrint(ram, echo, nl, "\r\nUsage: AMMORUN file.exe|.com|.elf\r\n");
        return;
    }
    FieldRtxThrottle::autoThrottleDeep(path.c_str());
    if (FieldAmmoExec::launchFromGuest(ram, path.c_str())) {
        char buf[160];
        std::snprintf(buf, sizeof buf, "\r\nAmmoDOS running %s — Esc quits to shell\r\n",
            path.c_str());
        shellPrint(ram, echo, nl, buf);
    } else {
        shellPrint(ram, echo, nl, "\r\nAmmoDOS cannot launch (missing file or unsupported format)\r\n");
    }
}

inline void cmdAmmoDecomp(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                          const std::vector<std::string>& args) {
    toolReply(ram, echo, nl, FieldAmmoTools::decompFile(args));
}

inline void cmdAmmoZip(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                       const std::vector<std::string>& args) {
    toolReply(ram, echo, nl, FieldAmmoTools::zipFile(args));
}

inline void cmdAmmoDbg(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                       const std::vector<std::string>& args) {
    if (args.size() >= 2 && istreq(args[1].c_str(), "VGA"))
        FieldAmmoDbg::dumpVga(ram, echo, nl);
    else if (args.size() >= 2 && istreq(args[1].c_str(), "GUEST"))
        FieldAmmoDbg::dumpGuest(ram, echo, nl);
    else if (args.size() >= 2 && istreq(args[1].c_str(), "R"))
        FieldAmmoDbg::dumpRegs(FieldAmmoDbg::session.cpu, ram, echo, nl);
    else if (args.size() >= 2 && istreq(args[1].c_str(), "BC"))
        FieldAmmoDbg::clearBreakpoints();
    else if (args.size() >= 3 && istreq(args[1].c_str(), "RUN"))
        cmdAmmoRun(ram, echo, nl, {"AMMORUN", args[2]});
    else if (args.size() >= 3 && istreq(args[1].c_str(), "LOAD")) {
        std::string com = args[2];
        if (com.find(':') == std::string::npos) com = "C:\\" + com;
        if (com.find('.') == std::string::npos) com += ".COM";
        if (FieldAmmoDbg::loadSession(ram, com.c_str()))
            shellPrint(ram, echo, nl, "\r\nAMMODBG loaded session\r\n");
        else
            shellPrint(ram, echo, nl, "\r\nAMMODBG LOAD failed\r\n");
    } else if (args.size() >= 2 && istreq(args[1].c_str(), "STEP")) {
        std::uint32_t n = 1u;
        if (args.size() >= 3 && !istreq(args[2].c_str(), "STEP")) {
            std::string com = args[2];
            if (com.find(':') == std::string::npos) com = "C:\\" + com;
            if (com.find('.') == std::string::npos) com += ".COM";
            auto sr = FieldAmmoDbg::stepCom(ram, com.c_str(), echo, nl, 32u);
            if (!sr.ok) {
                char buf[96];
                std::snprintf(buf, sizeof buf, "\r\nSTEP fail: %s\r\n", sr.error.c_str());
                shellPrint(ram, echo, nl, buf);
            }
            return;
        }
        if (args.size() >= 3) n = static_cast<std::uint32_t>(std::strtoul(args[2].c_str(), nullptr, 10));
        auto sr = FieldAmmoDbg::stepSession(ram, echo, nl, n);
        if (!sr.ok) {
            char buf[96];
            std::snprintf(buf, sizeof buf, "\r\nSTEP fail: %s\r\n", sr.error.c_str());
            shellPrint(ram, echo, nl, buf);
        } else {
            FieldAmmoDbg::dumpRegs(FieldAmmoDbg::session.cpu, ram, echo, nl);
            FieldAmmoDbg::unasmAt(ram, FieldAmmoDbg::session.cpu.ip, 4, echo, nl);
        }
    } else if (args.size() >= 2 && istreq(args[1].c_str(), "GO")) {
        auto gr = FieldAmmoDbg::goSession(ram, echo, nl);
        if (!gr.ok) {
            char buf[96];
            std::snprintf(buf, sizeof buf, "\r\nGO fail: %s\r\n", gr.error.c_str());
            shellPrint(ram, echo, nl, buf);
        } else {
            FieldAmmoDbg::dumpRegs(FieldAmmoDbg::session.cpu, ram, echo, nl);
        }
    } else if (args.size() >= 2 && (istreq(args[1].c_str(), "U") || istreq(args[1].c_str(), "UNASM"))) {
        std::uint32_t addr = FieldAmmoDbg::session.cpu.ip;
        int lines = 8;
        if (args.size() >= 3) addr = static_cast<std::uint32_t>(std::strtoul(args[2].c_str(), nullptr, 16));
        if (args.size() >= 4) lines = static_cast<int>(std::strtoul(args[3].c_str(), nullptr, 10));
        FieldAmmoDbg::unasmAt(ram, addr, lines, echo, nl);
    } else if (args.size() >= 3 && istreq(args[1].c_str(), "D")) {
        const std::uint32_t addr = static_cast<std::uint32_t>(std::strtoul(args[2].c_str(), nullptr, 16));
        int bytes = 64;
        if (args.size() >= 4) bytes = static_cast<int>(std::strtoul(args[3].c_str(), nullptr, 10));
        FieldAmmoDbg::dumpMem(ram, addr, bytes, echo, nl);
    } else if (args.size() >= 3 && istreq(args[1].c_str(), "BP")) {
        const std::uint32_t addr = static_cast<std::uint32_t>(std::strtoul(args[2].c_str(), nullptr, 16));
        FieldAmmoDbg::addBreakpoint(addr);
        shellPrint(ram, echo, nl, "\r\nbreakpoint set\r\n");
    } else
        FieldAmmoDbg::usage(ram, echo, nl);
}

inline void cmdTools(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    shellPrint(ram, echo, nl, "\r\nRTX-AMMOS DevKit ");
    shellPrint(ram, echo, nl, FieldAmmoToolchain::VERSION);
    shellPrint(ram, echo, nl, "\r\n");
    for (std::size_t i = 0; i < FieldAmmoToolchain::toolCount(); ++i) {
        const auto& t = FieldAmmoToolchain::kTools[i];
        char line[96];
        std::snprintf(line, sizeof line, "  %-10s %s\r\n", t.name, t.desc);
        shellPrint(ram, echo, nl, line);
    }
}

inline void cmdScale(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                      const std::vector<std::string>& args) noexcept {
    if (args.size() >= 2) {
        if (istreq(args[1].c_str(), "AUTO")) {
            Options::Canvas::DosAutoScale = true;
            FieldDosDisplay::syncViewport(
                static_cast<int>(FieldDosViewport::winW),
                static_cast<int>(FieldDosViewport::winH));
            FieldLayer::setShellActive(FieldLayer::LayerId::Viewport, true);
            FieldRtxDrivers::syncLayerToDriver(FieldLayer::LayerId::Viewport);
            char buf[160];
            FieldDosDisplay::formatStatus(buf + 2, sizeof buf - 2);
            buf[0] = '\r'; buf[1] = '\n';
            shellPrint(ram, echo, nl, buf);
            shellPrint(ram, echo, nl, "\r\n");
            return;
        }
        const float s = static_cast<float>(std::atof(args[1].c_str()));
        if (s >= 1.f && s <= 12.f) {
            Options::Canvas::DosAutoScale = false;
            FieldDosViewport::fontScale = s;
            Options::Canvas::DosFontScale = s;
            FieldLayer::setShellActive(FieldLayer::LayerId::Viewport, true);
            FieldRtxDrivers::syncLayerToDriver(FieldLayer::LayerId::Viewport);
            char buf[96];
            std::snprintf(buf, sizeof buf,
                "\r\nVIEWPORT layer: font scale %.1fx (manual)\r\n", s);
            shellPrint(ram, echo, nl, buf);
            return;
        }
    }
    char status[160]{};
    FieldDosDisplay::formatStatus(status, sizeof status);
    char buf[320];
    std::snprintf(buf, sizeof buf,
        "\r\nVIEWPORT layer [%s]: scale=%.1fx auto=%s crisp=%s scanlines=%s\r\n"
        "%s\r\n"
        "Usage: SCALE AUTO | SCALE 1.0-12.0 | 4K auto-zoom | double-click stamp/zoom\r\n",
        FieldLayer::isShellActive(FieldLayer::LayerId::Viewport) ? "ON" : "off",
        FieldDosViewport::fontScale,
        Options::Canvas::DosAutoScale ? "on" : "off",
        FieldDosViewport::crispFont ? "on" : "off",
        FieldDosViewport::scanlines ? "on" : "off",
        status);
    shellPrint(ram, echo, nl, buf);
}

inline void cmdField(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    char sum[48]{};
    FieldLayer::formatShellSummary(sum, sizeof sum);
    shellPrint(ram, echo, nl, "\r\nRTX-AMMOS field layers — ");
    shellPrint(ram, echo, nl, sum);
    shellPrint(ram, echo, nl, "\r\n");
    for (const auto& L : FieldLayer::kShellRegistry) {
        char line[80];
        std::snprintf(line, sizeof line, "  L%u %-8s [%s]\r\n",
            static_cast<unsigned>(L.id),
            L.name,
            FieldLayer::isShellActive(L.id) ? "ON " : "off");
        shellPrint(ram, echo, nl, line);
    }
}

inline void cmdLayer(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                     const std::vector<std::string>& args) noexcept {
    if (args.size() < 2) {
        shellPrint(ram, echo, nl,
            "\r\nUsage: LAYER <name> [ON|OFF|?]\r\n"
            "  Names: ram vga fat drives viewport audio mscdex input io bios\r\n"
            "  Aliases: AMMOFAT DRIVES MSCDEX VIEWPORT DOS\r\n");
        return;
    }
    const FieldLayer::ShellEntry* L = FieldLayer::findShellByName(args[1].c_str());
    if (!L) {
        shellPrint(ram, echo, nl, "\r\nUnknown field layer\r\n");
        return;
    }
    if (args.size() >= 3) {
        if (istreq(args[2].c_str(), "?") || istreq(args[2].c_str(), "STATUS")) {
            char buf[80];
            std::snprintf(buf, sizeof buf, "\r\nL%u %s layer: %s\r\n",
                static_cast<unsigned>(L->id), L->name,
                FieldLayer::isShellActive(L->id) ? "ON" : "off");
            shellPrint(ram, echo, nl, buf);
            return;
        }
        if (istreq(args[2].c_str(), "ON")) FieldLayer::setShellActive(L->id, true);
        else if (istreq(args[2].c_str(), "OFF")) FieldLayer::setShellActive(L->id, false);
        else {
            shellPrint(ram, echo, nl, "\r\nUsage: LAYER <name> [ON|OFF|?]\r\n");
            return;
        }
    } else {
        FieldLayer::toggleShell(L->id);
    }
    FieldRtxDrivers::syncLayerToDriver(L->id);
    char buf[80];
    std::snprintf(buf, sizeof buf, "\r\nL%u %s layer now %s\r\n",
        static_cast<unsigned>(L->id), L->name,
        FieldLayer::isShellActive(L->id) ? "ON" : "off");
    shellPrint(ram, echo, nl, buf);
}

inline void cmdDrives(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    FieldRtxDrivers::syncAllLayers();
    char buf[640];
    FieldDrives::formatTable(buf, sizeof buf);
    shellPrint(ram, echo, nl, buf);
}

inline void cmdDrivers(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    FieldRtxDrivers::syncAllLayers();
    shellPrint(ram, echo, nl, "\r\nNative driver table (field layer IDs):\r\n");
    for (std::size_t i = 0; i < FieldRtxDrivers::count(); ++i) {
        char line[96];
        FieldRtxDrivers::formatTableLine(line, sizeof line, i);
        shellPrint(ram, echo, nl, line);
    }
}

inline void cmdAmmos(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    char buf[384];
    std::snprintf(buf, sizeof buf,
        "\r\n%s — %s\r\n"
        "%s\r\n"
        "Era: DOS | Win31 | Win98 | Linux (bridge) | RTX GUI (slot)\r\n"
        "Toolchain slot reserved — custom drivers + compile tools TBD\r\n",
        FieldRtxAmmos::PRODUCT, FieldRtxAmmos::ARCHITECTURE, FieldRtxAmmos::TAGLINE);
    shellPrint(ram, echo, nl, buf);
}

inline void cmdFileCmd(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                       const std::vector<std::string>& args) noexcept {
    if (args.size() >= 2 && FieldRtxGui::argIsHelp(args[1])) {
        shellPrint(ram, echo, nl,
            "\r\nField Commander — dual-pane scrollable file browser\r\n"
            "  FILECMD       Open popup panel (PgUp/Dn scroll, Tab panes)\r\n"
            "  F3 in panel   Open file in AmmoCode\r\n");
        return;
    }
    if (FieldAmouranthOs::active) {
        FieldAmouranthOs::openNewWindow(FieldAmouranthOs::AppId::FileCmd);
    }
    FieldAmouranthFileCmd::open();
    FieldAmouranthFileCmd::paint(ram);
    shellPrint(ram, echo, nl, "\r\nField Commander open — scroll with PgUp/Dn or wheel.\r\n");
}

inline void cmdLauncher(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    if (!FieldAmouranthOs::active)
        FieldAmouranthOs::boot();
    FieldAmouranthOs::consoleShell = false;
    FieldAmouranthOs::openNewWindow(FieldAmouranthOs::AppId::Shell);
    FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Shell);
    shellPrint(ram, echo, nl,
        "\r\nProgram Launcher — pick AmmoCode, Field Commander, Doom, etc.\r\n");
}

inline void cmdAmouranthOs(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                           const std::vector<std::string>& args) noexcept {
    if (args.size() >= 2 && FieldRtxGui::argIsHelp(args[1])) {
        shellPrint(ram, echo, nl,
            "\r\nAmouranthOS — RTX window manager desktop\r\n"
            "  AMOURANTHOS        Boot WM shell (title bar, Start, programs)\r\n"
            "  AMOURANTHOS OFF    Return to immersive DOS panel\r\n"
            "  Start menu opens panels on demand; center shows version/quality\r\n"
            "  Start menu + folder icon | FILECMD | AMMOCODE minimap in IDE\r\n");
        return;
    }
    if (args.size() >= 2 && (istreq(args[1].c_str(), "OFF") || istreq(args[1].c_str(), "0"))) {
        FieldAmouranthOs::deactivate();
        shellPrint(ram, echo, nl, "\r\nAmouranthOS shell exited — immersive panel restored.\r\n");
        return;
    }
    FieldAmouranthOs::boot();
    char buf[160];
    std::snprintf(buf, sizeof buf,
        "\r\nAmouranthOS online — %.64s | Start for programs | DevKit info panel\r\n",
        FieldRuntimeInfo::masterStatusLine());
    shellPrint(ram, echo, nl, buf);
}

inline void cmdLinux(std::uint8_t* ram, EchoFn echo, NewlineFn nl) noexcept {
    shellPrint(ram, echo, nl,
        "\r\nRTX-AMMOS Linux bridge: host POSIX + Vulkan co-runtime\r\n"
        "  You are on Linux now — Field Die runs DOS inside Vulkan.\r\n"
        "  Native Linux guest slot reserved for RTX driver layer.\r\n");
}

inline void cmdEra(std::uint8_t* ram, const std::vector<std::string>& args,
                   EchoFn echo, NewlineFn nl) noexcept {
    if (args.size() < 2) {
        const char* name = "DOS";
        if (FieldRtxAmmos::activeEra == FieldRtxAmmos::Era::Win98) name = "WIN98";
        else if (FieldRtxAmmos::activeEra == FieldRtxAmmos::Era::Win31) name = "WIN31";
        char buf[48];
        std::snprintf(buf, sizeof buf, "\r\nCurrent ERA: %s\r\n", name);
        shellPrint(ram, echo, nl, buf);
        return;
    }
    const std::string& arg = args[1];
    if (istreq(arg.c_str(), "DOS")) {
        setTextMode(ram);
        shellPrint(ram, echo, nl, "\r\nERA set to RTX-DOS 7.0\r\n");
    } else if (istreq(arg.c_str(), "WIN31") || istreq(arg.c_str(), "WIN")) {
        if (win31LicensedReady()) {
            shellPrint(ram, echo, nl,
                "\r\nWindows 3.1 files on C:\\WINDOWS — use BOOT31.BAT or CONFIG [WIN31]\r\n"
                "  RTX preview: loading VGA desktop...\r\n");
        } else {
            shellPrint(ram, echo, nl,
                "\r\nWindows 3.1 preview (Zachary Geurts MCSE+I RTX shell)\r\n"
                "  Licensed WIN.COM not staged — TYPE WIN31.TXT | SETUP31\r\n");
        }
        paintWinDesktop(ram, FieldRtxAmmos::Era::Win31);
        shellPrint(ram, echo, nl, "\r\nESC or EXIT for C:\\>\r\n");
    } else if (istreq(arg.c_str(), "WIN98")) {
        paintWinDesktop(ram, FieldRtxAmmos::Era::Win98);
        shellPrint(ram, echo, nl, "\r\nWindows 98 — ESC for C:\\>\r\n");
    } else {
        shellPrint(ram, echo, nl, "\r\nUsage: ERA DOS|WIN31|WIN98\r\n");
    }
}

inline bool dispatchToolCom(std::uint8_t* ram, EchoFn echo, NewlineFn nl,
                            const std::string& name,
                            const std::vector<std::string>& args) {
    if (istreq(name.c_str(), "FIELDC")) { cmdFieldc(ram, echo, nl, args); return true; }
    if (istreq(name.c_str(), "AMMOASM")) { cmdAmmoAsm(ram, echo, nl, args); return true; }
    if (istreq(name.c_str(), "AMMOSYS")) { cmdAmmoSys(ram, echo, nl, args); return true; }
    if (istreq(name.c_str(), "AMMOLINK")) { cmdAmmoLink(ram, echo, nl, args); return true; }
    if (istreq(name.c_str(), "AMMOCC")) { cmdAmmoCc(ram, echo, nl, args); return true; }
    if (istreq(name.c_str(), "AMMORUN")) { cmdAmmoRun(ram, echo, nl, args); return true; }
    if (istreq(name.c_str(), "AMMODBG")) { cmdAmmoDbg(ram, echo, nl, args); return true; }
    if (istreq(name.c_str(), "AMMODECOMP")) { cmdAmmoDecomp(ram, echo, nl, args); return true; }
    if (istreq(name.c_str(), "AMMOZIP")) { cmdAmmoZip(ram, echo, nl, args); return true; }
    if (istreq(name.c_str(), "MEMORYUP") || istreq(name.c_str(), "MEMMAKER"))
        { cmdMemoryup(ram, echo, nl, args); return true; }
    if (istreq(name.c_str(), "SCANDISK")) { cmdScandisk(ram, echo, nl); return true; }
    if (istreq(name.c_str(), "REGEDIT")) { cmdRegEdit(ram, echo, nl); return true; }
    return false;
}

inline void execLine(const char* line, std::uint8_t* ram,
                     EchoFn echo, NewlineFn nl, PromptFn prompt) {
    FieldRtxVfs::vfsInit();
    if (FieldRtxBasic::active) {
        if (FieldRtxBasic::handleBasicLine(ram, line, echo, nl)) {
            if (!FieldRtxBasic::active && !graphicsActive) prompt(ram);
            return;
        }
    }
    if (FieldAmmoText::active || FieldMonacoEdit::active) {
        prompt(ram);
        return;
    }
    std::vector<std::string> args;
    tokenize(line, args);
    if (args.empty()) {
        prompt(ram);
        return;
    }
    const std::string& cmd = args[0];
    for (std::size_t i = 1; i < args.size(); ++i) {
        if (FieldRtxGui::argIsHelp(args[i])) {
            if (FieldRtxCmdHelp::print(cmd.c_str(), ram, echo, nl)) {
                if (!graphicsActive) prompt(ram);
                return;
            }
            FieldRtxCmdHelp::printUnknown(cmd.c_str(), ram, echo, nl);
            if (!graphicsActive) prompt(ram);
            return;
        }
    }
    if (istreq(cmd.c_str(), "VER")) cmdVer(ram, echo, nl);
    else if (istreq(cmd.c_str(), "DIR") || (cmd.size() >= 3 && istreq(cmd.substr(0, 3).c_str(), "DIR")))
        cmdDir(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "LS"))
        cmdDir(ram, echo, nl, args.size() >= 2 ? args : std::vector<std::string>{"DIR"});
    else if (istreq(cmd.c_str(), "PWD"))
        cmdPwd(ram, echo, nl);
    else if (istreq(cmd.c_str(), "EDIT") || istreq(cmd.c_str(), "EDLIN"))
        cmdEdit(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "AMMOTEXT") || istreq(cmd.c_str(), "NOTEPAD"))
        cmdAmmoText(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "THEMES") || istreq(cmd.c_str(), "RTXTHEMES"))
        cmdThemes(ram, echo, nl);
    else if (istreq(cmd.c_str(), "AMMOCODE") || istreq(cmd.c_str(), "ACODE") || istreq(cmd.c_str(), "TP"))
        FieldAmmoCode::cmdShell(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "QBASIC") || istreq(cmd.c_str(), "BASIC") || istreq(cmd.c_str(), "QBX"))
        FieldRtxBasic::cmdQbasic(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "CHKDSK") || (cmd.size() >= 6 && istreq(cmd.substr(0, 6).c_str(), "CHKDSK")))
        cmdChkdsk(ram, echo, nl);
    else if (istreq(cmd.c_str(), "SCANDISK") || istreq(cmd.c_str(), "SCANDSK"))
        cmdScandisk(ram, echo, nl);
    else if (istreq(cmd.c_str(), "MEMORYUP") || istreq(cmd.c_str(), "MEMMAKER"))
        cmdMemoryup(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "EXTMAP") || istreq(cmd.c_str(), "ASSOC"))
        cmdExtMap(ram, echo, nl);
    else if (istreq(cmd.c_str(), "REG")) cmdReg(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "REGEDIT")) cmdRegEdit(ram, echo, nl);
    else if (istreq(cmd.c_str(), "CLS")) FieldRtxBoot::paintWelcome(ram);
    else if (istreq(cmd.c_str(), "BOOT")) cmdBoot(ram, echo, nl);
    else if (istreq(cmd.c_str(), "BIOS") || istreq(cmd.c_str(), "AMMOBIOS") || istreq(cmd.c_str(), "CMOS"))
        cmdBios(ram, echo, nl);
    else if (istreq(cmd.c_str(), "VERBOSE")) cmdVerbose(ram, args, echo, nl);
    else if (istreq(cmd.c_str(), "ECHO")) cmdEcho(ram, args, echo, nl);
    else if (istreq(cmd.c_str(), "PROMPT")) cmdPromptStyle(ram, args, echo, nl);
    else if (istreq(cmd.c_str(), "MODE")) cmdMode(ram, args, echo, nl);
    else if (istreq(cmd.c_str(), "HELP") || istreq(cmd.c_str(), "?")) {
        if (args.size() >= 2 && !FieldRtxGui::argIsHelp(args[1])) {
            if (!FieldRtxCmdHelp::print(args[1].c_str(), ram, echo, nl))
                FieldRtxCmdHelp::printUnknown(args[1].c_str(), ram, echo, nl);
        } else
            cmdHelp(ram, echo, nl);
    }
    else if (istreq(cmd.c_str(), "DATE")) {
        char d[48];
        char t[32];
        formatHostDateTime(d, sizeof d, t, sizeof t);
        char buf[80];
        std::snprintf(buf, sizeof buf, "\r\nCurrent date is %s\r\n", d);
        shellPrint(ram, echo, nl, buf);
    } else if (istreq(cmd.c_str(), "TIME")) {
        char d[48];
        char t[32];
        formatHostDateTime(d, sizeof d, t, sizeof t);
        char buf[64];
        std::snprintf(buf, sizeof buf, "\r\nCurrent time is %s\r\n", t);
        shellPrint(ram, echo, nl, buf);
    } else if (istreq(cmd.c_str(), "VOL")) {
        FieldDrives::refresh();
        const char drv = FieldDrives::currentLetter;
        const FieldDrives::Drive* slot = FieldDrives::byLetter(drv);
        char buf[80];
        std::snprintf(buf, sizeof buf, "\r\n Volume in drive %c is %s\r\n", drv,
            slot && slot->volume[0] ? slot->volume : "-");
        shellPrint(ram, echo, nl, buf);
    } else if (istreq(cmd.c_str(), "LABEL")) {
        FieldDrives::refresh();
        const FieldDrives::Drive* slot = FieldDrives::byLetter('C');
        char buf[72];
        std::snprintf(buf, sizeof buf, "\r\nVolume label is %s\r\n",
            slot && slot->volume[0] ? slot->volume : FieldDos::hdVolumeLabel().c_str());
        shellPrint(ram, echo, nl, buf);
    } else if (istreq(cmd.c_str(), "MEM")) cmdMem(ram, echo, nl);
    else if (istreq(cmd.c_str(), "GPU")) cmdGpu(ram, echo, nl);
    else if (istreq(cmd.c_str(), "THROTTLE")) cmdThrottle(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "DEVICES")) cmdDevices(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "PORTS") || istreq(cmd.c_str(), "SERIAL")
             || istreq(cmd.c_str(), "PARALLEL") || istreq(cmd.c_str(), "USB")
             || istreq(cmd.c_str(), "BLUETOOTH") || istreq(cmd.c_str(), "BT"))
        cmdPorts(ram, echo, nl);
    else if (istreq(cmd.c_str(), "SOUND") || istreq(cmd.c_str(), "SBTEST"))
        cmdSound(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "CDROM") || istreq(cmd.c_str(), "MSCDEX"))
        cmdCdRom(ram, echo, nl);
    else if (istreq(cmd.c_str(), "AMMOFAT"))
        cmdAmmofat(ram, echo, nl);
    else if (istreq(cmd.c_str(), "TOOLS") || istreq(cmd.c_str(), "DEVKIT"))
        cmdTools(ram, echo, nl);
    else if (istreq(cmd.c_str(), "AMMOASM"))
        cmdAmmoAsm(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "AMMOSYS"))
        cmdAmmoSys(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "AMMOLINK"))
        cmdAmmoLink(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "AMMOCC"))
        cmdAmmoCc(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "FIELDC") || istreq(cmd.c_str(), "FIELDCC") || istreq(cmd.c_str(), "FC"))
        cmdFieldc(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "PADTEST") || istreq(cmd.c_str(), "JOYTEST") || istreq(cmd.c_str(), "GAMEPAD"))
        cmdPadTest(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "BROWSER") || istreq(cmd.c_str(), "WEB"))
        cmdBrowser(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "DOOM"))
        cmdDoom(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "NES") || istreq(cmd.c_str(), "FAMICOM"))
        cmdNes(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "SNES") || istreq(cmd.c_str(), "SUPERFAMICOM"))
        cmdSnes(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "GENESIS") || istreq(cmd.c_str(), "MEGADRIVE") || istreq(cmd.c_str(), "MD"))
        cmdGenesis(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "SMS") || istreq(cmd.c_str(), "MASTERSYSTEM"))
        cmdSms(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "A2600") || istreq(cmd.c_str(), "ATARI2600") || istreq(cmd.c_str(), "VCS"))
        cmdA2600(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "AMMORUN"))
        cmdAmmoRun(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "AMMODBG"))
        cmdAmmoDbg(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "AMMODECOMP"))
        cmdAmmoDecomp(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "AMMOZIP") || istreq(cmd.c_str(), "UNZIP"))
        cmdAmmoZip(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "BUILD"))
        cmdAmmoBuild(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "SCALE") || istreq(cmd.c_str(), "FONT"))
        cmdScale(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "FIELD"))
        cmdField(ram, echo, nl);
    else if (istreq(cmd.c_str(), "LAYER"))
        cmdLayer(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "DRIVERS"))
        cmdDrivers(ram, echo, nl);
    else if (istreq(cmd.c_str(), "DRIVES"))
        cmdDrives(ram, echo, nl);
    else if (istreq(cmd.c_str(), "MOUNT")) cmdMount(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "CD")) cmdCd(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "MD") || istreq(cmd.c_str(), "MKDIR"))
        cmdMd(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "RD") || istreq(cmd.c_str(), "RMDIR"))
        cmdRd(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "DEL") || istreq(cmd.c_str(), "ERASE"))
        cmdDel(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "COPY"))
        cmdCopy(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "REN") || istreq(cmd.c_str(), "RENAME"))
        cmdRen(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "TREE"))
        cmdTree(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "PATH")) cmdPath(ram, echo, nl);
    else if (istreq(cmd.c_str(), "WHERE")) cmdWhere(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "MORE")) cmdMore(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "FIND")) cmdFind(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "ATTRIB")) cmdAttrib(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "FDISK"))
        shellPrint(ram, echo, nl,
            "\r\nRTX FDISK: 1 active FAT16 partition 2048 MiB (RTXDOS)\r\n"
            "  Use FDISK /? for syntax\r\n");
    else if (istreq(cmd.c_str(), "FORMAT"))
        shellPrint(ram, echo, nl,
            "\r\nRTX FORMAT: FAT16 volume RTXDOS OEM=RTXDOS70\r\n"
            "  Use FORMAT /? for syntax\r\n");
    else if (istreq(cmd.c_str(), "DOS"))
        cmdEra(ram, {"ERA", "DOS"}, echo, nl);
    else if (istreq(cmd.c_str(), "ERA"))
        cmdEra(ram, args, echo, nl);
    else if (istreq(cmd.c_str(), "SETUP31") || istreq(cmd.c_str(), "WINSETUP"))
        cmdSetup31(ram, echo, nl);
    else if (istreq(cmd.c_str(), "WIN31") || istreq(cmd.c_str(), "WIN"))
        cmdEra(ram, {"ERA", "WIN31"}, echo, nl);
    else if (istreq(cmd.c_str(), "WIN98"))
        cmdEra(ram, {"ERA", "WIN98"}, echo, nl);
    else if (istreq(cmd.c_str(), "LINUX"))
        cmdLinux(ram, echo, nl);
    else if (istreq(cmd.c_str(), "LAUNCHER") || istreq(cmd.c_str(), "PROGRAMS")
            || istreq(cmd.c_str(), "START"))
        cmdLauncher(ram, echo, nl);
    else if (istreq(cmd.c_str(), "AMOURANTHOS") || istreq(cmd.c_str(), "AOS"))
        cmdAmouranthOs(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "FILECMD") || istreq(cmd.c_str(), "FILES"))
        cmdFileCmd(ram, echo, nl, args);
    else if (istreq(cmd.c_str(), "VSCODIUM") || istreq(cmd.c_str(), "CODE"))
        FieldAmouranthOs::launchVscodium();
    else if (istreq(cmd.c_str(), "HOSTTERM") || istreq(cmd.c_str(), "MINTTERM")
            || istreq(cmd.c_str(), "TERMINAL"))
        FieldAmouranthOs::launchDosConsole();
    else if (istreq(cmd.c_str(), "AMMOS"))
        cmdAmmos(ram, echo, nl);
    else if (istreq(cmd.c_str(), "EXIT") || istreq(cmd.c_str(), "GRAPHICS")) {
        setTextMode(ram);
        shellPrint(ram, echo, nl, "\r\nReturned to RTX-DOS text mode.\r\n");
    } else if (istreq(cmd.c_str(), "TYPE") && args.size() >= 2) {
        std::vector<std::uint8_t> data;
        const char drive = FieldDrives::currentLetter;
        bool ok = false;
        if (drive == 'C') {
            const std::string path = resolveAnyPath('C', args[1]);
            ok = FieldAmmoVfs::readPath(path.c_str(), data);
        } else {
            ok = readDriveFile(drive, args[1], data);
        }
        if (ok && !data.empty()) {
            shellPrint(ram, echo, nl, "\r\n");
            for (std::uint8_t b : data) {
                if (b == '\r') continue;
                if (b == '\n') nl(ram);
                else if (b >= 32 && b < 127) echo(ram, static_cast<char>(b));
            }
            shellPrint(ram, echo, nl, "\r\n");
        } else {
            shellPrint(ram, echo, nl, "\r\nFile not found\r\n");
        }
    } else {
        std::string base = cmd;
        const auto dot = base.find_last_of('.');
        if (dot != std::string::npos) {
            std::string ext = base.substr(dot);
            for (auto& c : ext) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            if (ext == ".COM") {
                base = base.substr(0, dot);
                std::vector<std::string> comArgs = {base};
                for (std::size_t i = 1; i < args.size(); ++i) comArgs.push_back(args[i]);
                if (dispatchToolCom(ram, echo, nl, base, comArgs)) {
                    if (!graphicsActive) prompt(ram);
                    return;
                }
            }
        }
        if (tryExecExternal(ram, echo, nl, cmd, args)) {
            if (!graphicsActive && !FieldAmmoExec::isActive()) prompt(ram);
            return;
        }
        char buf[96];
        std::snprintf(buf, sizeof buf, "\r\nBad command or file name: %s\r\n", cmd.c_str());
        shellPrint(ram, echo, nl, buf);
    }
    if (!graphicsActive && !FieldAmmoExec::isActive())
        prompt(ram);
}

inline char biosKeyToChar(std::uint16_t key) noexcept {
    if (key == 0x1C0Du || key == 0x0Du) return '\r';
    if (key == 0x0E08u) return '\b';
    return static_cast<char>(key & 0xFFu);
}

inline void shellExec(std::uint8_t* ram, EchoFn echo, NewlineFn nl, PromptFn prompt) noexcept {
    if (shellLen <= 0) {
        prompt(ram);
        return;
    }
    shellLine[shellLen] = '\0';
    FieldRtxTerm::onUserInput();
    char logBuf[140];
    std::snprintf(logBuf, sizeof logBuf, "> %s", shellLine);
    FieldRtxTerm::appendLog(logBuf);
    nl(ram);
    if (echoCommands) {
        replayPrompt(ram, echo);
        shellPrintAttr(ram, echo, nl, shellLine, shellInputAttr);
    }
    execLine(shellLine, ram, echo, nl, prompt);
    shellLen = 0;
    shellLine[0] = '\0';
    FieldRtxConsoleGui::afterShellOutput(ram);
}

inline void seedTerminalWelcome(std::uint8_t* ram) noexcept;

inline void pumpInteractive(std::uint8_t* ram, std::uint32_t hostKey, bool keyDown) noexcept {
    static std::uint32_t prevKey = 0;
    if (!ram) return;
    if (FieldAmmoBios::active) {
        FieldAmmoBios::handleMouse(ram);
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        if (keyDown && hostKey != 0u) {
            FieldAmmoBios::handleKey(key, ram);
            if (!FieldAmmoBios::active) {
                FieldRtxBoot::paintWelcome(ram);
                defaultPrompt(ram);
                shellLen = 0;
                shellLine[0] = '\0';
            }
        }
        prevKey = hostKey;
        return;
    }
    if (FieldAmmoExec::isActive()) {
        prevKey = hostKey;
        return;
    }
    if (FieldDeviceViewer::active) {
        FieldDeviceViewer::handleMouseFrame(ram);
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        if (keyDown && hostKey != 0u)
            FieldDeviceViewer::handleKey(ram, key);
        else
            FieldDeviceViewer::paint(ram);
        prevKey = hostKey;
        return;
    }
    if (FieldRegistryEditor::active) {
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        if (keyDown && hostKey != 0u)
            FieldRegistryEditor::handleKey(ram, key);
        else
            FieldRegistryEditor::paint(ram);
        prevKey = hostKey;
        return;
    }
    if (FieldExtensionEditor::active) {
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        if (keyDown && hostKey != 0u)
            FieldExtensionEditor::handleKey(ram, key);
        else
            FieldExtensionEditor::paint(ram);
        prevKey = hostKey;
        return;
    }
    if (FieldAmouranthFileCmd::active) {
        FieldAmouranthFileCmd::handleMouseFrame(ram);
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        if (keyDown && hostKey != 0u)
            FieldAmouranthFileCmd::handleKey(ram, key);
        else
            FieldAmouranthFileCmd::paint(ram);
        prevKey = hostKey;
        return;
    }
    if (FieldAmmoNesSetup::active) {
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        FieldAmmoNesSetup::pump(ram, key, keyDown && hostKey != 0u,
            SDL_GetKeyboardState(nullptr));
        if (!FieldAmmoNesSetup::active) {
            FieldRtxBoot::paintWelcome(ram);
            defaultPrompt(ram);
            shellLen = 0;
            shellLine[0] = '\0';
        }
        prevKey = hostKey;
        return;
    }
    if (FieldAmmoA2600Setup::active) {
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        FieldAmmoA2600Setup::pump(ram, key, keyDown && hostKey != 0u);
        if (!FieldAmmoA2600Setup::active) {
            FieldRtxBoot::paintWelcome(ram);
            defaultPrompt(ram);
            shellLen = 0;
            shellLine[0] = '\0';
        }
        prevKey = hostKey;
        return;
    }
    if (FieldAmmoSmsSetup::active) {
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        FieldAmmoSmsSetup::pump(ram, key, keyDown && hostKey != 0u);
        if (!FieldAmmoSmsSetup::active) {
            FieldRtxBoot::paintWelcome(ram);
            defaultPrompt(ram);
            shellLen = 0;
            shellLine[0] = '\0';
        }
        prevKey = hostKey;
        return;
    }
    if (FieldAmmoGenesisSetup::active) {
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        FieldAmmoGenesisSetup::pump(ram, key, keyDown && hostKey != 0u);
        if (!FieldAmmoGenesisSetup::active) {
            FieldRtxBoot::paintWelcome(ram);
            defaultPrompt(ram);
            shellLen = 0;
            shellLine[0] = '\0';
        }
        prevKey = hostKey;
        return;
    }
    if (FieldAmmoSnesSetup::active) {
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        FieldAmmoSnesSetup::pump(ram, key, keyDown && hostKey != 0u);
        if (!FieldAmmoSnesSetup::active) {
            FieldRtxBoot::paintWelcome(ram);
            defaultPrompt(ram);
            shellLen = 0;
            shellLine[0] = '\0';
        }
        prevKey = hostKey;
        return;
    }
    if (FieldPadTest::active) {
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        FieldPadTest::pump(ram, key, keyDown && hostKey != 0u);
        prevKey = hostKey;
        return;
    }
    if (FieldRtxConsoleGui::active && !FieldAmmoCode::active && !FieldMonacoEdit::active) {
        FieldRtxConsoleGui::handleMouse(ram);
    }

    if (FieldNes::active) {
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        FieldNes::pump(ram, key, keyDown && hostKey != 0u);
        if (!FieldNes::active) {
            FieldRtxBoot::paintWelcome(ram);
            defaultPrompt(ram);
            shellLen = 0;
            shellLine[0] = '\0';
        }
        prevKey = hostKey;
        return;
    }
    if (FieldA2600::active) {
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        FieldA2600::pump(ram, key, keyDown && hostKey != 0u);
        if (!FieldA2600::active) {
            FieldRtxBoot::paintWelcome(ram);
            defaultPrompt(ram);
            shellLen = 0;
            shellLine[0] = '\0';
        }
        prevKey = hostKey;
        return;
    }
    if (FieldSms::active) {
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        FieldSms::pump(ram, key, keyDown && hostKey != 0u);
        if (!FieldSms::active) {
            FieldRtxBoot::paintWelcome(ram);
            defaultPrompt(ram);
            shellLen = 0;
            shellLine[0] = '\0';
        }
        prevKey = hostKey;
        return;
    }
    if (FieldGenesis::active) {
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        FieldGenesis::pump(ram, key, keyDown && hostKey != 0u);
        if (!FieldGenesis::active) {
            FieldRtxBoot::paintWelcome(ram);
            defaultPrompt(ram);
            shellLen = 0;
            shellLine[0] = '\0';
        }
        prevKey = hostKey;
        return;
    }
    if (FieldSnes::active) {
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        FieldSnes::pump(ram, key, keyDown && hostKey != 0u);
        if (!FieldSnes::active) {
            FieldRtxBoot::paintWelcome(ram);
            defaultPrompt(ram);
            shellLen = 0;
            shellLine[0] = '\0';
        }
        prevKey = hostKey;
        return;
    }
    if (FieldAmmoBrowser::isActive()) {
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        if (keyDown && hostKey != 0u && (key == 0x011Bu || (key & 0xFFu) == 27u))
            FieldAmmoBrowser::close();
        if (!FieldAmmoBrowser::isActive()) {
            FieldRtxBoot::paintWelcome(ram);
            defaultPrompt(ram);
            shellLen = 0;
            shellLine[0] = '\0';
        }
        prevKey = hostKey;
        return;
    }
    if (FieldAmmoCode::active) {
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        FieldAmmoCode::pump(ram, key, keyDown && hostKey != 0u, echoChar, defaultNewline);
        if (!FieldAmmoCode::active) {
            FieldRtxBoot::paintWelcome(ram);
            defaultPrompt(ram);
            shellLen = 0;
            shellLine[0] = '\0';
        }
        prevKey = hostKey;
        return;
    }
    if (FieldRtxThemePicker::active && !FieldAmmoText::active) {
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        if (keyDown && hostKey != 0u)
            FieldRtxThemePicker::handleKey(key);
        FieldRtxThemePicker::paint(ram);
        if (!FieldRtxThemePicker::active) {
            graphicsActive = false;
            FieldRtxBoot::paintWelcome(ram);
            defaultPrompt(ram);
            shellLen = 0;
            shellLine[0] = '\0';
        }
        prevKey = hostKey;
        return;
    }
    if (FieldMonacoEdit::active) {
        FieldMonacoEdit::handleMouse(ram);
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        if (keyDown && hostKey != 0u)
            FieldMonacoEdit::handleKey(ram, key);
        if (!FieldMonacoEdit::active) {
            graphicsActive = false;
            FieldRtxBoot::paintWelcome(ram);
            defaultPrompt(ram);
            shellLen = 0;
            shellLine[0] = '\0';
        }
        prevKey = hostKey;
        return;
    }
    if (FieldAmmoText::active) {
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        if (keyDown && hostKey != 0u)
            FieldAmmoText::handleKey(ram, key, echoChar, defaultNewline);
        if (!FieldAmmoText::active) {
            FieldRtxVgaText::initClassic(ram);
            FieldRtxBoot::paintWelcome(ram);
            defaultPrompt(ram);
            shellLen = 0;
            shellLine[0] = '\0';
        }
        prevKey = hostKey;
        return;
    }
    if (graphicsActive && keyDown && hostKey != 0u) {
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        if (key == 0x011Bu || (key & 0xFFu) == 27u) {
            setTextMode(ram);
            FieldRtxBoot::paintWelcome(ram);
            shellLen = 0;
            shellLine[0] = '\0';
            prevKey = hostKey;
            return;
        }
    }
    if (!keyDown || hostKey == 0u) {
        prevKey = 0;
        return;
    }
    if (hostKey == prevKey) return;
    prevKey = hostKey;
    if (graphicsActive || FieldAmmoText::active || FieldMonacoEdit::active
        || FieldRtxThemePicker::active || FieldAmmoCode::active || FieldPadTest::active
        || FieldNes::active || FieldAmmoBrowser::isActive()) return;
    const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
    if (FieldRtxConsoleGui::active) {
        if (FieldRtxConsoleGui::pendingNewSession) {
            FieldRtxConsoleGui::pendingNewSession = false;
            FieldRtxConsoleGui::open(ram);
            setCursor(ram, 0);
            seedTerminalWelcome(ram);
            defaultPrompt(ram);
            FieldRtxTerm::syncLiveFromVga(ram);
            return;
        }
        if (FieldRtxConsoleGui::pendingHelp) {
            FieldRtxConsoleGui::pendingHelp = false;
            shellLine[0] = 'H'; shellLine[1] = 'E'; shellLine[2] = 'L'; shellLine[3] = 'P';
            shellLine[4] = '\0';
            shellLen = 4;
            shellExec(ram, echoChar, defaultNewline, defaultPrompt);
            return;
        }
        if (FieldRtxConsoleGui::pumpKey(key, ram))
            return;
    }
    if (key == 0x4900u) {
        FieldRtxTerm::scrollBy(-3);
        FieldRtxTerm::applyView(ram);
        FieldRtxConsoleGui::afterShellOutput(ram);
        return;
    }
    if (key == 0x5100u) {
        FieldRtxTerm::scrollBy(3);
        FieldRtxTerm::applyView(ram);
        FieldRtxConsoleGui::afterShellOutput(ram);
        return;
    }
    const char ch = biosKeyToChar(key);
    if (ch == '\r') {
        shellExec(ram, echoChar, defaultNewline, defaultPrompt);
        return;
    }
    if (ch == '\b') {
        if (shellLen > 0) {
            --shellLen;
            shellLine[shellLen] = '\0';
            echoChar(ram, '\b');
        }
        return;
    }
    if (ch >= 32 && shellLen + 1 < static_cast<int>(sizeof shellLine)) {
        shellLine[shellLen++] = ch;
        shellLine[shellLen] = '\0';
        echoChar(ram, ch);
    }
}

} // namespace FieldRtxShell

#include "FieldRtxBoot.hpp"
#include "FieldRtxConsoleGui.hpp"

namespace FieldRtxShell {

inline void seedTerminalWelcome(std::uint8_t* ram) noexcept {
    if (!ram) return;
    FieldRuntimeInfo::refresh();
    shellPrintAttr(ram, echoChar, defaultNewline,
        " RTX-DOS 7.0 — AmouranthOS Golden Era command shell\r\n", 0x1Fu);
    shellPrintAttr(ram, echoChar, defaultNewline,
        " Type HELP for all commands  |  LAUNCHER for program picker\r\n", 0x17u);
    char rt[96];
    std::snprintf(rt, sizeof rt, " %.72s\r\n", FieldRuntimeInfo::masterStatusLine());
    shellPrintAttr(ram, echoChar, defaultNewline, rt, 0x3Bu);
    shellPrintAttr(ram, echoChar, defaultNewline,
        " Quick: DIR  VER  AMOURANTHOS  AMMOCODE  FILECMD  PADTEST\r\n", 0x07u);
    shellPrintAttr(ram, echoChar, defaultNewline,
        " Games: DOOM  NES  |  F1 help  |  PgUp/Dn scroll history\r\n", 0x08u);
    shellPrint(ram, echoChar, defaultNewline, "\r\n");
}

inline void paintTerminalShell(std::uint8_t* ram) noexcept {
    if (!ram) return;
    FieldRtxConsoleGui::open(ram);
    setCursor(ram, 0);
    seedTerminalWelcome(ram);
    defaultPrompt(ram);
    FieldRtxTerm::syncLiveFromVga(ram);
}

} // namespace FieldRtxShell

#include "FieldAmouranthDeactivate.hpp"