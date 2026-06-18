#pragma once

// IBM PC/AT + MS-DOS interrupt catalog and host trap helpers.
// All guest software (RTX-DOS shell, MZ/COM, DOS/4GW LE) routes through FieldBios::handleInt;
// this module documents every vector and supplies shared dispatch utilities.

#include "FieldDpmi.hpp"
#include "FieldPlatform.hpp"

#include <cstdio>
#include <cstdlib>

#include <x86emu.h>

namespace FieldDosInt {

enum class Coverage : std::uint8_t {
    Full,       // Complete host implementation
    Partial,    // Common AH/AL subset
    FaultEmu,   // CPU fault re-routed (INT 06/0D in PM)
    Chain,      // Pass through to guest IVT when hooked
    Stub,       // Success NOP for extenders
    None        // Unimplemented — chain or NOP per policy
};

struct VectorInfo {
    std::uint8_t  num;
    const char*   name;
    const char*   description;
    Coverage      coverage;
};

/* Full IBM PC + MS-DOS + extender vector catalog (implemented subset marked Full/Partial). */
inline constexpr VectorInfo kCatalog[] = {
    {0x00, "Divide",           "CPU #DE — divide by zero", Coverage::Chain},
    {0x01, "Debug",            "CPU #DB — debug / single-step", Coverage::Chain},
    {0x02, "NMI",              "Non-maskable interrupt", Coverage::Chain},
    {0x03, "Breakpoint",       "CPU #BP — INT 3", Coverage::Chain},
    {0x04, "Overflow",         "CPU #OF — INT 4 / INTO", Coverage::Chain},
    {0x05, "PrintScreen",      "BIOS print screen", Coverage::Full},
    {0x06, "InvalidOpcode",    "CPU #UD — extender insn emulation in PM", Coverage::FaultEmu},
    {0x07, "Coprocessor",      "CPU #NM — FPU not available", Coverage::Chain},
    {0x08, "IRQ0",             "PIT timer tick (BIOS 40:6C)", Coverage::Full},
    {0x09, "IRQ1",             "Keyboard hardware IRQ", Coverage::Full},
    {0x0A, "IRQ2",             "Cascade / secondary PIC", Coverage::Full},
    {0x0B, "IRQ3",             "COM2 / LPT2", Coverage::Full},
    {0x0C, "IRQ4",             "COM1", Coverage::Full},
    {0x0D, "IRQ5",             "LPT2 / SB16 / CPU #GP in PM", Coverage::FaultEmu},
    {0x0E, "IRQ6",             "Floppy controller", Coverage::Full},
    {0x0F, "IRQ7",             "LPT1", Coverage::Full},
    {0x10, "Video",            "BIOS video services (mode, cursor, scroll, VESA 4Fh)", Coverage::Full},
    {0x11, "Equipment",        "BIOS equipment list (0040:0010)", Coverage::Full},
    {0x12, "Memory",           "BIOS conventional memory size (KB)", Coverage::Full},
    {0x13, "Disk",             "BIOS disk (floppy + HD INT13h extensions)", Coverage::Full},
    {0x14, "Serial",           "BIOS async communications (COM ports)", Coverage::Full},
    {0x15, "System",           "BIOS system services (memory map, APM, gameport)", Coverage::Full},
    {0x16, "Keyboard",         "BIOS keyboard (synthesized — avoids IVT spin)", Coverage::Full},
    {0x17, "Printer",          "BIOS printer (LPT stub)", Coverage::Full},
    {0x18, "ROMBasic",         "ROM BASIC entry (unimplemented PC)", Coverage::Full},
    {0x19, "Boot",             "BIOS warm boot", Coverage::Full},
    {0x1A, "RTC",              "BIOS real-time clock / CMOS", Coverage::Full},
    {0x1B, "CtrlBreak",        "BIOS Ctrl-Break handler", Coverage::Full},
    {0x1C, "TimerTick",        "BIOS user timer tick", Coverage::Full},
    {0x1D, "VideoParams",      "BIOS video parameter table pointer", Coverage::Full},
    {0x1E, "DiskBase",         "BIOS disk base table pointer", Coverage::Full},
    {0x1F, "FontData",         "BIOS 8x8 font pointer", Coverage::Full},
    {0x20, "DOSProgram",       "DOS program terminate (obsolete)", Coverage::Full},
    {0x21, "DOS",              "MS-DOS kernel services", Coverage::Full},
    {0x22, "DOSAbort",         "DOS terminate address", Coverage::Chain},
    {0x23, "DOSCtrlC",         "DOS Ctrl-C handler", Coverage::Chain},
    {0x24, "DOSCritical",      "DOS critical error", Coverage::Chain},
    {0x25, "DOSAbsRead",       "DOS absolute disk read (obsolete)", Coverage::Chain},
    {0x26, "DOSAbsWrite",      "DOS absolute disk write (obsolete)", Coverage::Chain},
    {0x27, "DOSTermStay",      "DOS TSR terminate", Coverage::Chain},
    {0x28, "DOSIdle",          "DOS idle / scheduler", Coverage::Full},
    {0x29, "DOSFastCon",       "DOS fast console output", Coverage::Full},
    {0x2A, "DOSNet",           "DOS network / redirector", Coverage::Full},
    {0x2B, "DOSIdle2",         "DOS secondary idle", Coverage::Full},
    {0x2C, "DOSNet2",          "DOS network hooks", Coverage::Full},
    {0x2D, "DOSNet3",          "DOS network hooks", Coverage::Full},
    {0x2E, "DOSCrit2",         "DOS critical section", Coverage::Full},
    {0x2F, "Multiplex",        "DOS multiplex (PRINT/APPEND/DPMI/RTX drivers)", Coverage::Full},
    {0x30, "AHComp",           "DOS compatibility", Coverage::Full},
    {0x31, "DPMI",             "DOS Protected Mode Interface", Coverage::Full},
    {0x32, "DPMI2",            "DPMI alternate", Coverage::Chain},
    {0x33, "Mouse",            "Microsoft mouse driver", Coverage::Full},
    {0x34, "EMM",              "Expanded memory manager", Coverage::Full},
    {0x35, "PhysMem",          "Physical memory access", Coverage::Full},
    {0x36, "VCPI",             "Virtual Control Program Interface", Coverage::Full},
    {0x38, "CDROM",            "MSCDEX CD-ROM", Coverage::Full},
    {0x3F, "Overlay",          "Overlay manager", Coverage::Full},
    {0x4A, "Watchdog",         "Alarm / watchdog", Coverage::Chain},
    {0x67, "VDS",              "Virtual DMA Services (DOS/4GW)", Coverage::Full},
    {0x68, "VDS2",             "VDS extended", Coverage::Full},
    {0x6B, "UNIX",             "COHERENT UNIX syscall bridge", Coverage::Full},
    {0x6C, "ABIOS",            "ABIOS services", Coverage::Full},
    {0x6D, "HPFS",             "OS/2 HPFS", Coverage::Full},
    {0x70, "IRQ8",             "RTC alarm", Coverage::Full},
    {0x71, "IRQ9",             "Redirected cascade", Coverage::Full},
    {0x72, "IRQ10",            "Reserved", Coverage::Full},
    {0x73, "IRQ11",            "Reserved", Coverage::Full},
    {0x74, "IRQ12",            "Mouse IRQ (PS/2)", Coverage::Full},
    {0x75, "IRQ13",            "FPU exception", Coverage::Full},
    {0x76, "IRQ14",            "Primary IDE", Coverage::Full},
    {0x77, "IRQ15",            "Secondary IDE", Coverage::Full},
};

inline constexpr std::size_t kCatalogSize = sizeof(kCatalog) / sizeof(kCatalog[0]);

inline const VectorInfo* lookup(std::uint8_t num) noexcept {
    for (std::size_t i = 0; i < kCatalogSize; ++i)
        if (kCatalog[i].num == num) return &kCatalog[i];
    return nullptr;
}

inline bool traceEnabled() noexcept {
    return std::getenv("DOS_TRACE") || std::getenv("DOOM_TRACE");
}

inline void traceInt(x86emu_t* e, std::uint8_t num) noexcept {
    if (!traceEnabled() || !e) return;
    const std::uint8_t ah = static_cast<std::uint8_t>(e->x86.R_EAX >> 8);
    const char* name = lookup(num) ? lookup(num)->name : "?";
    std::fprintf(stderr, "[DOS] INT %02X (%s) ah=%02X ax=%04X cs=%04x ip=%04x\n",
        static_cast<unsigned>(num), name,
        static_cast<unsigned>(ah),
        static_cast<unsigned>(e->x86.R_EAX & 0xFFFFu),
        static_cast<unsigned>(e->x86.R_CS & 0xFFFFu),
        static_cast<unsigned>(FieldDpmi::isProtected(e)
            ? (e->x86.R_EIP & 0xFFFFu) : (e->x86.R_EIP & 0xFFFFu)));
}

inline void traceGpFault(x86emu_t* e, std::uint8_t b0) noexcept {
    if (!traceEnabled() || !e) return;
    std::fprintf(stderr,
        "[DOS] GP fault cs=%04x eip=%08x eax=%08x b0=%02x ds=%04x base=%08x\n",
        static_cast<unsigned>(e->x86.R_CS & 0xFFFFu),
        static_cast<unsigned>(FieldDpmi::pmFaultEip(e)),
        static_cast<unsigned>(e->x86.R_EAX),
        static_cast<unsigned>(b0),
        static_cast<unsigned>(e->x86.R_DS & 0xFFFFu),
        static_cast<unsigned>(e->x86.R_DS_BASE));
}

/* INT 05h — print screen (no-op success). */
inline int handleInt05(x86emu_t* e) noexcept {
    (void)e;
    return 1;
}

/* INT 14h — serial port services (stub COM1 ready). */
inline int handleInt14(x86emu_t* e) noexcept {
    const std::uint8_t ah = static_cast<std::uint8_t>(e->x86.R_EAX >> 8);
    const std::uint8_t dx = static_cast<std::uint8_t>(e->x86.R_EDX & 0xFFu);
    if (dx > 3u) {
        e->x86.R_AH = 0x80u;
        e->x86.R_FLG |= F_CF;
        return 1;
    }
    switch (ah) {
    case 0x00: /* init port */
        e->x86.R_AH = 0u;
        e->x86.R_AL = 0u;
        e->x86.R_FLG &= ~F_CF;
        return 1;
    case 0x01: /* write char */
        e->x86.R_AH = 0u;
        e->x86.R_FLG &= ~F_CF;
        return 1;
    case 0x02: /* read char */
        e->x86.R_AH = 0u;
        e->x86.R_AL = 0u;
        e->x86.R_FLG &= ~F_CF;
        return 1;
    case 0x03: /* get port status */
        e->x86.R_AH = 0x60u; /* TX empty + RX ready */
        e->x86.R_AL = 0u;
        e->x86.R_FLG &= ~F_CF;
        return 1;
    default:
        e->x86.R_AH = 0u;
        e->x86.R_FLG &= ~F_CF;
        return 1;
    }
}

/* INT 17h — printer (LPT1 ready). */
inline int handleInt17(x86emu_t* e) noexcept {
    const std::uint8_t ah = static_cast<std::uint8_t>(e->x86.R_EAX >> 8);
    const std::uint8_t dx = static_cast<std::uint8_t>(e->x86.R_EDX & 0xFFu);
    if (dx > 2u) {
        e->x86.R_AH = 1u;
        e->x86.R_FLG |= F_CF;
        return 1;
    }
    switch (ah) {
    case 0x00: /* write char */
        e->x86.R_AH = 0x90u; /* busy + acknowledge */
        e->x86.R_FLG &= ~F_CF;
        return 1;
    case 0x01: /* init port */
        e->x86.R_AH = 0x90u;
        e->x86.R_FLG &= ~F_CF;
        return 1;
    case 0x02: /* get status */
        e->x86.R_AH = 0x90u;
        e->x86.R_FLG &= ~F_CF;
        return 1;
    default:
        e->x86.R_AH = 0x90u;
        e->x86.R_FLG &= ~F_CF;
        return 1;
    }
}

/* INT 1Bh — Ctrl-Break (acknowledge, no action). */
inline int handleInt1B(x86emu_t* e) noexcept {
    (void)e;
    return 1;
}

/* True when IVT[num] holds a non-null segment:offset (DOS kernel or TSR hook). */
inline bool ivtHooked(x86emu_t* e, std::uint8_t num) noexcept {
    const std::uint32_t iv  = static_cast<std::uint32_t>(num) * 4u;
    const std::uint16_t off = static_cast<std::uint16_t>(x86emu_read_word(e, iv));
    const std::uint16_t seg = static_cast<std::uint16_t>(x86emu_read_word(e, iv + 2));
    return off != 0 || seg != 0;
}

/* INT 28h / 2Bh — DOS idle (COMMAND.COM poll loop, TSR time slice).
 * Return 0 to dispatch the IVT handler; 1 when vector is empty (caller IRETs). */
inline int handleDosIdle(x86emu_t* e, std::uint8_t num) noexcept {
    return ivtHooked(e, num) ? 0 : 1;
}

/* INT 1Dh/1Eh/1Fh — BIOS vector/table pointers (IVT slots). */
inline int handleInt1D1E1F(x86emu_t* e, std::uint8_t num) noexcept {
    switch (num) {
    case 0x1D:
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | 0x00F0u;
        e->x86.R_ES  = (e->x86.R_ES & 0xFFFF0000u) | 0xF000u;
        return 1;
    case 0x1E:
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | 0xEFC0u;
        e->x86.R_ES  = (e->x86.R_ES & 0xFFFF0000u) | 0x0000u;
        return 1;
    case 0x1F:
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | 0xFA6Eu;
        e->x86.R_ES  = (e->x86.R_ES & 0xFFFF0000u) | 0xF000u;
        return 1;
    default:
        return 0;
    }
}

/* Chain when IVT hooked; otherwise host IRET. */
inline int chainOrIret(x86emu_t* e, std::uint8_t num) noexcept {
    return ivtHooked(e, num) ? 0 : 1;
}

/* INT 18h — ROM BASIC not present on this machine. */
inline int handleInt18(x86emu_t* e) noexcept {
    e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | 0x0003u;
    return 1;
}

/* INT 20h — obsolete program terminate (maps to INT 21 AH=4Ch). */
inline int handleInt20(x86emu_t* e) noexcept {
    const std::uint8_t code = static_cast<std::uint8_t>(e->x86.R_EAX & 0xFFu);
    e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | 0x4C00u | code;
    return 2; /* caller re-dispatches INT 21 */
}

/* INT 29h — DOS fast console output (DL = character). */
inline int handleInt29(x86emu_t* e) noexcept {
    (void)e;
    return 3; /* caller echoes DL */
}

/* INT 2Ah/2Ch/2Dh — network redirector not installed. */
inline int handleIntNet(x86emu_t* e, std::uint8_t num) noexcept {
    if (ivtHooked(e, num)) return 0;
    e->x86.R_AL = 0u;
    return 1;
}

/* INT 2Eh — DOS critical section enter/leave. */
inline int handleInt2E(x86emu_t* e) noexcept {
    (void)e;
    return 1;
}

/* INT 30h — DOS compatibility flag (not active). */
inline int handleInt30(x86emu_t* e) noexcept {
    e->x86.R_AX = 0u;
    return 1;
}



/* INT 35h — physical memory driver not loaded. */
inline int handleInt35(x86emu_t* e) noexcept {
    e->x86.R_AH = 0xFFu;
    e->x86.R_AL = 0u;
    return 1;
}

/* INT 36h — VCPI not present (DPMI via INT 31 is used instead). */
inline int handleInt36(x86emu_t* e) noexcept {
    const std::uint8_t ah = static_cast<std::uint8_t>(e->x86.R_EAX >> 8);
    if (ah == 0x00u || ah == 0x01u) {
        e->x86.R_AL = 0u;
        return 1;
    }
    e->x86.R_AH = 0xFFu;
    return 1;
}

/* INT 38h — MSCDEX/CD-ROM driver IOCTL (not installed unless IVT hooked). */
inline int handleInt38(x86emu_t* e) noexcept {
    return chainOrIret(e, 0x38);
}

/* INT 3Fh — overlay manager not present. */
inline int handleInt3F(x86emu_t* e) noexcept {
    e->x86.R_AL = 0u;
    return 1;
}

/* INT 68h — VDS extended (mirror INT 67 subset). */
inline int handleInt68(x86emu_t* e) noexcept {
    const std::uint8_t ah = static_cast<std::uint8_t>(e->x86.R_EAX >> 8);
    if (ah == 0x00u) {
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF00FFu) | 0x0300u;
        return 1;
    }
    if (ah == 0x08u) {
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | FieldPlatform::EXTENDED_KB;
        return 1;
    }
    e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF00FFu) | 0x0080u;
    e->x86.R_FLG |= F_CF;
    return 1;
}

/* INT 6Bh/6Ch/6Dh — foreign OS bridges not available. */
inline int handleIntForeign(x86emu_t* e) noexcept {
    e->x86.R_AX = 0xFFFFu;
    return 1;
}

/* Hardware IRQ vectors — synthesize EOI when BIOS IVT slot is empty. */
inline int handleIrq(x86emu_t* e, std::uint8_t num) noexcept {
    if (num == 0x09u) {
        x86emu_write_byte(e, 0x0417u, 0u);
        x86emu_write_byte(e, 0x0418u, 0u);
    }
    return chainOrIret(e, num);
}

/* Count catalog entries by coverage (for self-test metrics). */
inline std::size_t countByCoverage(Coverage c) noexcept {
    std::size_t n = 0;
    for (std::size_t i = 0; i < kCatalogSize; ++i)
        if (kCatalog[i].coverage == c) ++n;
    return n;
}

} // namespace FieldDosInt