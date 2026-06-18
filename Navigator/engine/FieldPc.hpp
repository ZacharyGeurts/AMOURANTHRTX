#pragma once

// RTX Field PC — complete IBM PC/AT + MS-DOS machine model for the Field Die.
//
// Layer stack (bottom → top):
//   L0  Hardware chips   — PIC 8259, PIT 8254, DMA 8237, CMOS, VGA DAC/CRTC, FDC, IDE, SB16
//   L1  BIOS firmware    — POST, BDA, equipment, INT 10/11/12/13/14/15/16/17/19/1A vectors
//   L2  DOS kernel       — INT 21/28/2F chain when guest kernel installed; host INT 21 when empty
//   L3  Extenders        — XMS, EMS, DPMI 0.9, MSCDEX, VDS
//   L4  CPU execution    — GPU RM decoder (shell/boot) + host field_x86 (games/DOS4GW/PM)
//
// Policy:
//   • Documented vectors: catalog in FieldDosInt (71 entries, target Coverage::Full)
//   • Undocumented opcodes: host #UD/#GP in FieldDpmi + FieldX86Native; GPU halts → host trap
//   • IRQ delivery: FieldPic8259 → IVT vector → FieldBios::handleInt (never spin on empty IVT)
//   • Timing: FieldPit8254 channel 0 → IRQ0 → INT08/1C BDA tick

#include "FieldDevices.hpp"
#include "FieldDosInt.hpp"
#include "FieldPic8259.hpp"
#include "FieldPit8254.hpp"

#include <cstdint>
#include <x86emu.h>

namespace FieldPc {

enum class Layer : std::uint8_t {
    Hardware,
    Bios,
    Dos,
    Extender,
    Cpu,
};

struct SubsystemInfo {
    const char* name;
    Layer       layer;
    bool        hostImplemented;
    const char* module;
};

/* Complete subsystem manifest — drives QA coverage and implementation priority. */
inline constexpr SubsystemInfo kSubsystems[] = {
    {"8259 PIC",           Layer::Hardware, true,  "FieldPic8259"},
    {"8254 PIT",           Layer::Hardware, true,  "FieldPit8254"},
    {"8237 DMA",           Layer::Hardware, true,  "FieldDma"},
    {"CMOS/RTC",           Layer::Hardware, true,  "FieldCmos"},
    {"VGA DAC/modes",      Layer::Hardware, true,  "FieldVga"},
    {"VGA CRTC/SVGA",      Layer::Hardware, true,  "FieldVgaCrtc"},
    {"FDC 765",            Layer::Hardware, true,  "FieldFdc765"},
    {"IDE PIO",            Layer::Hardware, true,  "FieldIde"},
    {"COM/LPT",            Layer::Hardware, true,  "FieldPorts"},
    {"SB16/MPU/GUS",       Layer::Hardware, true,  "FieldAudioRack"},
    {"BIOS POST/BDA",      Layer::Bios,     true,  "FieldBios"},
    {"INT 10 video",       Layer::Bios,     true,  "FieldBios"},
    {"INT 13 disk",        Layer::Bios,     true,  "FieldBios/FieldDos"},
    {"INT 16 keyboard",    Layer::Bios,     true,  "FieldBios"},
    {"INT 21 DOS API",     Layer::Dos,      true,  "FieldBios/FieldInt21"},
    {"INT 28 idle",        Layer::Dos,      true,  "FieldDosInt"},
    {"INT 2F mux",         Layer::Dos,      true,  "FieldXms/FieldDpmi/FieldMscdex"},
    {"EMS LIM 4.0",        Layer::Extender, true,  "FieldEms"},
    {"XMS 3.0",            Layer::Extender, true,  "FieldXms/FieldDpmi"},
    {"DPMI 0.9",           Layer::Extender, true,  "FieldDpmi"},
    {"MSCDEX 2.x",         Layer::Extender, true,  "FieldMscdex"},
    {"VDS",                Layer::Extender, true,  "FieldBios"},
    {"GPU RM CPU",         Layer::Cpu,      true,  "x86_die_exec.inc"},
    {"Host field_x86",     Layer::Cpu,      true,  "FieldX86Core"},
    {"PM/DOS4GW",          Layer::Cpu,      true,  "FieldDpmi"},
};

inline constexpr std::size_t kSubsystemCount = sizeof(kSubsystems) / sizeof(kSubsystems[0]);

inline std::size_t implementedCount() noexcept {
    std::size_t n = 0;
    for (std::size_t i = 0; i < kSubsystemCount; ++i)
        if (kSubsystems[i].hostImplemented) ++n;
    return n;
}

inline void bindInterruptDelivery(x86emu_t* emu) noexcept {
    if (!emu) return;
    FieldPic8259::deliverIntr = [emu](std::uint8_t vector) {
        if ((emu->x86.R_FLG & F_IF) != 0)
            x86emu_intr_raise(emu, vector, 0u, 0);
    };
}

inline void reset() noexcept {
    FieldPic8259::init();
    FieldPit8254::reset();
    FieldDevices::reset();
}

inline void init(x86emu_t* emu) noexcept {
    reset();
    bindInterruptDelivery(emu);
}

inline void tick(x86emu_t* emu, std::uint64_t cycles) noexcept {
    FieldPit8254::tick(cycles);
    while (emu && FieldPic8259::dispatchPending()) { }
}

inline std::size_t catalogFullCount() noexcept {
    return FieldDosInt::countByCoverage(FieldDosInt::Coverage::Full);
}

inline std::size_t catalogStubCount() noexcept {
    return FieldDosInt::countByCoverage(FieldDosInt::Coverage::Stub);
}

} // namespace FieldPc