#pragma once

// RTX-DOS GPU boot — verbose POST + welcome screen + HD image handoff.

#include "FieldCdRom.hpp"
#include "FieldDos.hpp"
#include "FieldDosConfig.hpp"
#include "FieldPlatform.hpp"
#include "FieldDrives.hpp"
#include "FieldRtxAmmos.hpp"
#include "FieldRtxVfs.hpp"
#include "FieldRuntimeInfo.hpp"
#include "FieldRegistry.hpp"
#include "FieldRtxMemory.hpp"
#include "FieldRtxThemes.hpp"
#include "FieldRtxTerm.hpp"
#include "FieldRtxVgaText.hpp"
#include "FieldRtxConsoleGui.hpp"

namespace FieldAmouranthOs { extern bool consoleShell; }
namespace FieldRtxShell { void paintTerminalShell(std::uint8_t* ram) noexcept; }

#include <cstdio>
#include <filesystem>

#include <cstdint>
#include <cstring>

namespace FieldRtxBoot {

constexpr std::uint32_t VGA_BASE = 0x000B8000u;
constexpr std::uint32_t BDA_CUR_COL = 0x450u;
constexpr std::uint32_t BDA_CUR_ROW = 0x451u;
constexpr std::uint32_t BDA_VIDEO_MODE = 0x449u;
constexpr std::uint32_t BDA_COLS = 0x44Au;

inline bool verboseBoot = false;

inline void vgaPut(std::uint8_t* ram, std::uint16_t cell, std::uint8_t ch, std::uint8_t attr) {
    const std::uint32_t off = VGA_BASE + static_cast<std::uint32_t>(cell) * 2u;
    if (off + 1u >= FieldPlatform::GUEST_RAM_BYTES) return;
    ram[off] = ch;
    ram[off + 1u] = attr;
}

inline void setCursor(std::uint8_t* ram, std::uint16_t cell) {
    ram[BDA_CUR_COL] = static_cast<std::uint8_t>(cell % 80u);
    ram[BDA_CUR_ROW] = static_cast<std::uint8_t>(cell / 80u);
}

inline void clearScreen(std::uint8_t* ram, std::uint8_t attr = 0x07) noexcept {
    if (!ram) return;
    const int cols = FieldRtxVgaText::cols();
    const int rows = FieldRtxVgaText::rows();
    for (int row = 0; row < rows; ++row)
        for (int col = 0; col < cols; ++col) {
            const std::uint32_t off = FieldRtxVgaText::cellOffset(row, col);
            if (off + 1u >= FieldPlatform::GUEST_RAM_BYTES) continue;
            ram[off] = ' ';
            ram[off + 1u] = attr;
        }
}

inline void putLine(std::uint8_t* ram, int row, const char* text, std::uint8_t attr) noexcept {
    if (!ram || row < 0 || row >= 25) return;
    for (int col = 0; col < 80; ++col)
        vgaPut(ram, static_cast<std::uint16_t>(row * 80 + col), ' ', attr);
    for (int i = 0; text[i] && i < 80; ++i)
        vgaPut(ram, static_cast<std::uint16_t>(row * 80 + i),
            static_cast<std::uint8_t>(text[i]), attr);
}

inline void logBoot(const char* line) noexcept {
    std::fprintf(stderr, "[BOOT] %s\n", line);
}

inline void paintVerbosePost(std::uint8_t* ram) noexcept {
    if (!ram) return;
    clearScreen(ram, 0x07);
    ram[BDA_VIDEO_MODE] = 3u;
    ram[BDA_COLS] = 80u;

    static const struct { const char* text; std::uint8_t attr; } kLines[] = {
        {" RTX-AMMOS Field Die POST v7.0 — 1970s terminal to present day", 0x1F},
        {"", 0x07},
        {" [OK] Vulkan x86 Field Die mapped — 64 MiB GPU fast RAM", 0x3B},
        {" [OK] BIOS E820 — 4096 MiB extended memory profile", 0x3B},
        {" [OK] A20 gate enabled — conventional memory tier + XMS", 0x3B},
        {" [OK] INT 13h fixed disk C: — 2048 MiB RTXDOS FAT16", 0x3B},
        {" [OK] AMMOFAT.SYS L2 — GPU-native FAT + HD mirror hot set", 0x3B},
        {" [OK] MSCDEX 2.1 — ISO9660 CD-ROM redirector", 0x3B},
        {" [OK] SB16@220 + OPL@388 + LAPC-1@330 + DSS@378 + GUS/ESS/Covox", 0x3B},
        {" [OK] RTXKERNEL.SYS — INT 2Fh mux AH=52h field layers L0-L9", 0x3B},
        {" [OK] Viewport SDL3 — runtime version on VER / AmmoCode bar", 0x3B},
        {" [OK] AmouranthOS — RTX WM · taskbar · Start · diag desktop", 0x3B},
        {" [OK] Era catalog: DOS | Win31 | Win95 | Win98 | Linux | RTX GUI", 0x3B},
        {"", 0x07},
        {" --- Golden Era Man+Machine — RTX-DOS 7.0 forever place ---", 0x1E},
        {" HELP EDIT QBASIC /HELP | TYPE GOLDEN.TXT | dbl-click panel = zoom", 0x17},
        {"", 0x07},
    };

    char memLine[81]{};
    std::snprintf(memLine, sizeof memLine,
        " [OK] A20 gate — %u KiB boot, grow %u KiB + XMS/EMM386/MSCDEX on demand",
        static_cast<unsigned>(FieldRtxMemory::bootConventionalKb),
        static_cast<unsigned>(FieldRtxMemory::maxConventionalKb));

    for (std::size_t i = 0; i < sizeof(kLines) / sizeof(kLines[0]); ++i) {
        const char* text = (i == 4u) ? memLine : kLines[i].text;
        putLine(ram, static_cast<int>(i), text, kLines[i].attr);
        if (text[0]) logBoot(text);
    }
    char rt[96];
    FieldRuntimeInfo::refresh();
    std::snprintf(rt, sizeof rt, " [OK] %.40s FC%s NES PADTEST FIELDC",
        FieldRuntimeInfo::masterStatusLine(), FieldRuntimeInfo::FIELD_COMPILER_VER);
    putLine(ram, 16, rt, 0x3B);
    logBoot(rt);

    FieldRtxVfs::vfsInit();
    FieldDrives::refresh();
    char dyn[80];
    std::snprintf(dyn, sizeof dyn, " [OK] Drives %s | VFS %zu meta | CD %s",
        FieldDrives::readyLetters(), FieldRtxVfs::metadata.size(),
        FieldCdRom::ready ? FieldCdRom::volumeLabel.c_str() : "—");
    putLine(ram, 15, dyn, 0x3B);
    logBoot(dyn);
}

inline void paintCompactWelcome(std::uint8_t* ram) noexcept {
    if (!ram) return;
    ram[BDA_VIDEO_MODE] = 3u;
    ram[BDA_COLS] = 80u;
    clearScreen(ram, 0x07);
    putLine(ram, 0,
        " RTX-DOS 7.0  |  AmouranthOS  |  Vulkan Field Die  |  SDL3", 0x1F);
    putLine(ram, 1,
        " Golden Era shell — HELP for commands  |  START for programs", 0x17);
    FieldRuntimeInfo::refresh();
    char rt[80];
    std::snprintf(rt, sizeof rt, " %.72s", FieldRuntimeInfo::masterStatusLine());
    putLine(ram, 2, rt, 0x3B);
    putLine(ram, 3,
        " ----------------------------------------------------------------", 0x08);
    for (int row = 4; row < 24; ++row)
        putLine(ram, row, "", 0x07);
}

constexpr std::uint8_t TERM_ATTR = 0x02u;  // green on black

inline void paintPromptRow(std::uint8_t* ram, int row = 24) noexcept {
    if (!ram || row < 0 || row >= 25) return;
    static const char prompt[] = "C:\\>";
    const std::uint16_t base = static_cast<std::uint16_t>(row * 80);
    for (int col = 0; col < 80; ++col)
        vgaPut(ram, static_cast<std::uint16_t>(base + col), ' ', TERM_ATTR);
    for (int i = 0; i < 4; ++i)
        vgaPut(ram, static_cast<std::uint16_t>(base + i),
            static_cast<std::uint8_t>(prompt[i]), TERM_ATTR);
    setCursor(ram, static_cast<std::uint16_t>(base + 4));
}

inline void paintTerminalShell(std::uint8_t* ram) noexcept {
    if (!ram) return;
    FieldRtxShell::paintTerminalShell(ram);
}

inline void paintWelcome(std::uint8_t* ram) {
    if (!ram) return;
    if (FieldAmouranthOs::active) {
        ram[BDA_VIDEO_MODE] = 3u;
        ram[BDA_COLS] = 80u;
        clearScreen(ram, 0x07u);
    } else if (FieldAmouranthOs::consoleShell)
        paintTerminalShell(ram);
    else if (verboseBoot && FieldDosConfig::cfg.preset == FieldDosConfig::Preset::Ammos)
        paintVerbosePost(ram);
    else
        paintCompactWelcome(ram);
    if (!FieldAmouranthOs::consoleShell && !FieldAmouranthOs::active)
        paintPromptRow(ram, 24);
    if (FieldAmouranthOs::consoleShell) {
        FieldRtxTerm::history.clear();
        FieldRtxTerm::scrollOffset = 0;
        FieldRtxTerm::liveDirty = true;
    } else {
        FieldRtxTerm::setClientRect(0, 0, FieldRtxVgaText::cols(), FieldRtxVgaText::rows());
        FieldRtxTerm::history.clear();
        FieldRtxTerm::scrollOffset = 0;
        FieldRtxTerm::liveDirty = true;
        FieldRtxTerm::syncLiveFromVga(ram);
    }
}

inline void seedGpuDos(void* mapped, std::size_t headerBytes, bool hdReady) {
    if (!mapped || !hdReady) return;
    auto* ram = static_cast<std::uint8_t*>(mapped) + headerBytes;
    logBoot("RTX-DOS 7.0 GPU Super DOSBox — seeding guest RAM");
    FieldRtxVfs::vfsInit();
    FieldRegistry::init();
    FieldRtxThemes::init();
    paintWelcome(ram);
    logBoot("Boot handoff complete — interactive shell active");
}

} // namespace FieldRtxBoot