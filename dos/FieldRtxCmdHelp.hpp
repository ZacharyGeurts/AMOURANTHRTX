#pragma once

// RTX-DOS 7.0 — per-command /? help (COMMAND /?  HELP topic  TYPE COMMAND.TXT).

#include <cctype>
#include <cstdio>
#include <cstring>
#include <functional>

namespace FieldRtxCmdHelp {

struct Entry {
    const char* cmd;
    const char* text;
};

inline bool ciEq(const char* a, const char* b) noexcept {
    if (!a || !b) return false;
    while (*a && *b) {
        if (std::toupper(static_cast<unsigned char>(*a))
            != std::toupper(static_cast<unsigned char>(*b)))
            return false;
        ++a; ++b;
    }
    return *a == *b;
}

using EchoFn = std::function<void(std::uint8_t*, char)>;
using NewlineFn = std::function<void(std::uint8_t*)>;

inline void printBlock(std::uint8_t* ram, const EchoFn& echo, const NewlineFn& nl,
                       const char* text) noexcept {
    for (const char* p = text; *p; ++p) {
        if (*p == '\r') continue;
        if (*p == '\n') nl(ram);
        else echo(ram, *p);
    }
}

inline const Entry* find(const char* cmd) noexcept {
    if (!cmd || !*cmd) return nullptr;
    static const Entry kTable[] = {
        {"VER",
            "\r\nVER — RTX-DOS 7.0 version and Field Die status\r\n"
            "  Syntax: VER\r\n"
            "  Shows COMMAND.COM build, RAM, layer stack, drive rack.\r\n"},
        {"DIR",
            "\r\nDIR — list directory (color + DESCRIPT.ION metadata)\r\n"
            "  Syntax: DIR [path] [/W] [/V] [/S]\r\n"
            "  /W  wide listing   /V  verbose + descriptions   /S  subdirs\r\n"
            "  See also: LS TREE ATTRIB WHERE\r\n"},
        {"LS",
            "\r\nLS — alias for DIR\r\n"
            "  Syntax: LS [path] [switches]\r\n"},
        {"TREE",
            "\r\nTREE — directory tree\r\n"
            "  Syntax: TREE [path]\r\n"},
        {"CD",
            "\r\nCD — change directory\r\n"
            "  Syntax: CD [path]   CD ..   CD \\\r\n"
            "  Drive C: uses AMMOFAT paths (C:\\SUB\\FILE)\r\n"},
        {"PWD",
            "\r\nPWD — print working directory\r\n"
            "  Syntax: PWD\r\n"},
        {"MD",
            "\r\nMD / MKDIR — create directory\r\n"
            "  Syntax: MD dirname\r\n"},
        {"RD",
            "\r\nRD / RMDIR — remove empty directory\r\n"
            "  Syntax: RD dirname\r\n"},
        {"DEL",
            "\r\nDEL / ERASE — delete file(s)\r\n"
            "  Syntax: DEL file   DEL *.COM\r\n"},
        {"COPY",
            "\r\nCOPY — copy files\r\n"
            "  Syntax: COPY src dst   COPY a+b c   COPY dir\\*.* dest\\ /S\r\n"},
        {"REN",
            "\r\nREN / RENAME — rename file\r\n"
            "  Syntax: REN old new\r\n"},
        {"TYPE",
            "\r\nTYPE — display file contents\r\n"
            "  Syntax: TYPE file.txt   TYPE C:\\FIELDLAY.TXT\r\n"},
        {"MORE",
            "\r\nMORE — paginated TYPE\r\n"
            "  Syntax: MORE file\r\n"},
        {"FIND",
            "\r\nFIND — search text in files\r\n"
            "  Syntax: FIND \"needle\" file\r\n"},
        {"ATTRIB",
            "\r\nATTRIB — show/set file attributes\r\n"
            "  Syntax: ATTRIB file   ATTRIB +R file\r\n"},
        {"WHERE",
            "\r\nWHERE — locate file on PATH\r\n"
            "  Syntax: WHERE command\r\n"},
        {"CLS",
            "\r\nCLS — clear screen and repaint welcome banner\r\n"
            "  Syntax: CLS\r\n"},
        {"EDIT",
            "\r\nEDIT / AMMOTEXT / NOTEPAD — AmmoText Notepad clone\r\n"
            "  Syntax: AMMOTEXT [file]   EDIT and EDLIN are aliases\r\n"
            "  Spellcheck F7 | Themes (View or THEMES) | F1 help F2 save\r\n"},
        {"AMMOTEXT",
            "\r\nAMMOTEXT — RTX Notepad with spellcheck and RTX themes\r\n"
            "  Syntax: AMMOTEXT [file]   NOTEPAD alias\r\n"},
        {"THEMES",
            "\r\nTHEMES — RTX windowing color presets for all DOS panels\r\n"
            "  Syntax: THEMES   or View-Themes in AmmoText\r\n"},
        {"HELP",
            "\r\nHELP — command reference\r\n"
            "  Syntax: HELP   HELP topic   ?   topic /?\r\n"
            "  TYPE COMMAND.TXT for full on-disk catalog\r\n"},
        {"?",
            "\r\n? — same as HELP\r\n"
            "  Syntax: ?   ? topic\r\n"},
        {"CHKDSK",
            "\r\nCHKDSK — volume and layer health\r\n"
            "  Syntax: CHKDSK [drive:]\r\n"
            "  Reports AMMOFAT clusters, CD-ROM, host bridge.\r\n"},
        {"SCANDISK",
            "\r\nSCANDISK — deep surface + FAT + RAID journal scan\r\n"
            "  Syntax: SCANDISK [drive:]\r\n"
            "  Alias: SCANDSK. Repairs via AMMOFAT sync when needed.\r\n"},
        {"MEMORYUP",
            "\r\nMEMORYUP — MemMaker-class memory optimizer (2026)\r\n"
            "  Syntax: MEMORYUP [/S]\r\n"
            "  Alias: MEMMAKER. /S applies optimal CONFIG.SYS profile.\r\n"},
        {"EXTMAP",
            "\r\nEXTMAP — extension association editor\r\n"
            "  Syntax: EXTMAP   aliases: ASSOC\r\n"
            "  Edits HKRTX\\Associations (synced to RTXREG.INI).\r\n"},
        {"REG",
            "\r\nREG — RTX Registry 2026 hierarchical configuration\r\n"
            "  Syntax: REG QUERY [path] [key]\r\n"
            "          REG ADD path key value\r\n"
            "          REG DELETE path key\r\n"
            "          REG LOAD   REG SAVE\r\n"
            "  Store: C:\\TOOLS\\RTXREG.INI  Journal: RTXREG.JRN\r\n"},
        {"REGEDIT",
            "\r\nREGEDIT — graphical HKRTX registry editor\r\n"
            "  Syntax: REGEDIT\r\n"
            "  Tab switch pane | + add key | F2 edit | Esc save+close\r\n"},
        {"MEM",
            "\r\nMEM — conventional/XMS/UMB summary\r\n"
            "  Syntax: MEM\r\n"},
        {"PATH",
            "\r\nPATH — show or set search path\r\n"
            "  Syntax: PATH   PATH C:\\;C:\\TOOLS;C:\\DOS\r\n"},
        {"DATE",
            "\r\nDATE — display host-synchronized date\r\n"
            "  Syntax: DATE\r\n"},
        {"TIME",
            "\r\nTIME — display host-synchronized time\r\n"
            "  Syntax: TIME\r\n"},
        {"VOL",
            "\r\nVOL — volume label for current drive\r\n"
            "  Syntax: VOL [drive:]\r\n"},
        {"LABEL",
            "\r\nLABEL — volume label for C: RTXDOS\r\n"
            "  Syntax: LABEL\r\n"},
        {"MODE",
            "\r\nMODE — video/con port mode\r\n"
            "  Syntax: MODE [options]\r\n"},
        {"ECHO",
            "\r\nECHO — command-line echo on/off\r\n"
            "  Syntax: ECHO ON   ECHO OFF\r\n"},
        {"PROMPT",
            "\r\nPROMPT — shell prompt style\r\n"
            "  Syntax: PROMPT CLASSIC|RTX|FIELD|MATRIX\r\n"},
        {"VERBOSE",
            "\r\nVERBOSE — POST boot detail\r\n"
            "  Syntax: VERBOSE ON|OFF\r\n"},
        {"BOOT",
            "\r\nBOOT — replay RTX POST banner\r\n"
            "  Syntax: BOOT\r\n"},
        {"BIOS",
            "\r\nBIOS / AMMOBIOS / CMOS — setup utility (mouse + arrows)\r\n"
            "  Syntax: BIOS\r\n"},
        {"GPU",
            "\r\nGPU — Vulkan Field Die engine status\r\n"
            "  Syntax: GPU\r\n"},
        {"THROTTLE",
            "\r\nTHROTTLE — per-program MZ/PE speed governor\r\n"
            "  Syntax: THROTTLE   THROTTLE ON|OFF\r\n"},
        {"DEVICES",
            "\r\nDEVICES — RTX Device Manager (PC chassis GUI)\r\n"
            "  Syntax: DEVICES   DEVICES /T for text list\r\n"
            "  Arrows/click select | Space toggle audio/input | Esc close\r\n"},
        {"PORTS",
            "\r\nPORTS — serial/parallel/USB summary\r\n"
            "  Syntax: PORTS   SERIAL   PARALLEL   USB   BT\r\n"},
        {"DRIVES",
            "\r\nDRIVES — field drive rack A/B/C/D/E\r\n"
            "  Syntax: DRIVES   TYPE DRIVES.TXT\r\n"},
        {"DRIVERS",
            "\r\nDRIVERS — RTX field driver table (layer IDs)\r\n"
            "  Syntax: DRIVERS\r\n"},
        {"FIELD",
            "\r\nFIELD — Field Die layer status (VGA/FAT/audio)\r\n"
            "  Syntax: FIELD\r\n"},
        {"LAYER",
            "\r\nLAYER — toggle or inspect field layers\r\n"
            "  Syntax: LAYER [name]\r\n"},
        {"AMMOFAT",
            "\r\nAMMOFAT — GPU-native FAT16+ tools\r\n"
            "  Syntax: AMMOFAT   TYPE AMMOFAT.TXT\r\n"},
        {"MOUNT",
            "\r\nMOUNT — attach CD/ZIP/ISO archive\r\n"
            "  Syntax: MOUNT CD   MOUNT ISO file   MOUNT UNMOUNT ALL\r\n"},
        {"MSCDEX",
            "\r\nMSCDEX / CDROM — CD-ROM redirector status\r\n"
            "  Syntax: MSCDEX   CDROM\r\n"},
        {"SOUND",
            "\r\nSOUND — SB16/OPL audio rack\r\n"
            "  Syntax: SOUND DIR   SOUND TEST   SOUND BEEP   SOUND RUN file\r\n"},
        {"TOOLS",
            "\r\nTOOLS / DEVKIT — RTX-AMMOS programming toolchain\r\n"
            "  Syntax: TOOLS\r\n"
            "  C:\\TOOLS\\AMMOASM AMMOCC AMMOLINK AMMODECOMP AMMOZIP AMMORUN AMMODBG FIELDC\r\n"
            "  INCLUDE=C:\\AMMOINC   samples in C:\\SAMPLES\r\n"},
        {"AMMOASM",
            "\r\nAMMOASM v4 — MASM subset assembler (.ASM → .OBJ)\r\n"
            "  Syntax: AMMOASM file.asm\r\n"
            "  Supports PUBLIC/EXTRN, ORG 100h, INT 21h, branches.\r\n"
            "  See: BUILD HELLO   TYPE C:\\SAMPLES\\HELLO.ASM\r\n"},
        {"AMMOCC",
            "\r\nAMMOCC v4 — tiny C compiler (.C → .OBJ)\r\n"
            "  Syntax: AMMOCC file.c\r\n"
            "  int vars, + - * /, if/while, rtx_puts/rtx_out, return\r\n"
            "  See: TYPE C:\\SAMPLES\\HELLO.C   #include RTX.H\r\n"},
        {"AMMOLINK",
            "\r\nAMMOLINK v4 — link AMMO objects → .COM\r\n"
            "  Syntax: AMMOLINK a.obj [b.obj ...]\r\n"
            "  Multi-module: AMMOLINK UTIL.OBJ MAIN.OBJ\r\n"},
        {"AMMODECOMP",
            "\r\nAMMODECOMP v4 — decompiler listing (.COM/.OBJ → .LST)\r\n"
            "  Syntax: AMMODECOMP file.com\r\n"},
        {"AMMOZIP",
            "\r\nAMMOZIP — PKZIP archive extractor (stored + deflate)\r\n"
            "  Syntax: AMMOZIP archive.zip [dest\\]\r\n"
            "          AMMOZIP -d dest\\ archive.zip\r\n"
            "  Example: AMMOZIP C:\\GAMES\\WOLF3D\\WOLF3D.ZIP\r\n"},
        {"AMMORUN",
            "\r\nAMMORUN v4 — load and run .COM in guest RAM\r\n"
            "  Syntax: AMMORUN file.com\r\n"},
        {"AMMODBG",
            "\r\nAMMODBG v4 — interactive debugger session\r\n"
            "  Syntax: AMMODBG file.com\r\n"
            "  LOAD file   STEP [n]   GO   U [addr]   D addr   R\r\n"
            "  BP addr   BC   VGA   GUEST   RUN file\r\n"},
        {"BUILD",
            "\r\nBUILD — one-shot asm/compile + link\r\n"
            "  Syntax: BUILD [stem]\r\n"
            "  Tries stem.ASM, then stem.FLD, then stem.C\r\n"},
        {"FIELDC",
            "\r\nFIELDC v4 — Field language compiler (.FLD → .OBJ)\r\n"
            "  Syntax: FIELDC file.fld\r\n"
            "  print \"text\"  return N  field name  era any\r\n"},
        {"AMMOCODE",
            "\r\nAMMOCODE — Turbo Pascal 7-style IDE\r\n"
            "  Syntax: AMMOCODE [file]   aliases: ACODE TP\r\n"
            "  F5 Run  F8 Step  F9 Compile  ASM/C/BASIC/Field modes\r\n"},
        {"QBASIC",
            "\r\nQBASIC — interactive BASIC interpreter\r\n"
            "  Syntax: QBASIC   QBASIC /HELP\r\n"
            "  F1 help menu, textbook chapters, F5 run F8 step\r\n"},
        {"PADTEST",
            "\r\nPADTEST — Xbox360 / SDL gamepad live tester\r\n"
            "  Syntax: PADTEST   aliases: JOYTEST GAMEPAD\r\n"},
        {"NES",
            "\r\nNES / FAMICOM — AmmoNES iNES emulator\r\n"
            "  Syntax: NES [rom] [flags]   NES HELP   NES SETUP   NES PAD   NES IMPORT\r\n"
            "  Start menu: Games → AmmoNES / Setup / Controls\r\n"},
        {"BROWSER",
            "\r\nBROWSER — open embedded Field Web panel (no host window)\r\n"
            "  Syntax: BROWSER [url]\r\n"},
        {"AMOURANTHOS",
            "\r\nAMOURANTHOS / AOS — RTX window manager desktop\r\n"
            "  Syntax: AMOURANTHOS   AMOURANTHOS OFF\r\n"},
        {"FILECMD",
            "\r\nFILECMD — dual-pane Field Commander (2026)\r\n"
            "  Syntax: FILECMD   aliases: FILES\r\n"
            "  Mouse, wheel, Enter run, F6 extension map editor.\r\n"},
        {"AMMOS",
            "\r\nAMMOS — RTX-AMMOS product summary\r\n"
            "  Syntax: AMMOS\r\n"},
        {"ERA",
            "\r\nERA — switch presentation era\r\n"
            "  Syntax: ERA DOS|WIN31|WIN98\r\n"},
        {"WIN31",
            "\r\nWIN31 / WIN — Windows 3.1 preview or boot\r\n"
            "  Syntax: WIN31   SETUP31   BOOT31.BAT\r\n"
            "  TYPE WIN31.TXT for staging licensed media\r\n"},
        {"WIN98",
            "\r\nWIN98 — Windows 98 desktop preview\r\n"
            "  Syntax: WIN98   BOOT98.BAT\r\n"},
        {"LINUX",
            "\r\nLINUX — host POSIX bridge info\r\n"
            "  Syntax: LINUX\r\n"},
        {"FDISK",
            "\r\nFDISK — partition table (RTX: 1 active FAT16 2048 MiB)\r\n"
            "  Syntax: FDISK\r\n"},
        {"FORMAT",
            "\r\nFORMAT — volume format (RTX: FAT16 RTXDOS OEM=RTXDOS70)\r\n"
            "  Syntax: FORMAT drive:\r\n"},
        {"EXIT",
            "\r\nEXIT / GRAPHICS — leave graphics/desktop to text shell\r\n"
            "  Syntax: EXIT   GRAPHICS\r\n"},
        {"SCALE",
            "\r\nSCALE / FONT — DOS panel font scale\r\n"
            "  Syntax: SCALE [factor]\r\n"},
        {nullptr, nullptr}
    };
    for (const Entry* e = kTable; e->cmd; ++e)
        if (ciEq(cmd, e->cmd)) return e;
    return nullptr;
}

inline bool print(const char* cmd, std::uint8_t* ram, const EchoFn& echo,
                  const NewlineFn& nl) noexcept {
    const Entry* e = find(cmd);
    if (!e) return false;
    printBlock(ram, echo, nl, e->text);
    printBlock(ram, echo, nl,
        "\r\nRTX-DOS 7.0 — any command: COMMAND /?   HELP topic   TYPE COMMAND.TXT\r\n");
    return true;
}

inline void printUnknown(const char* cmd, std::uint8_t* ram, const EchoFn& echo,
                         const NewlineFn& nl) noexcept {
    char buf[96];
    std::snprintf(buf, sizeof buf,
        "\r\nNo help for %s — try HELP or TYPE COMMAND.TXT\r\n", cmd ? cmd : "(null)");
    printBlock(ram, echo, nl, buf);
}

} // namespace FieldRtxCmdHelp