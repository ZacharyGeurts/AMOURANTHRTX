#pragma once

// NES CPU glue — shared MOS 6502 + Ricoh 2A03 IRQ/NMI wiring.

#include "../Common/FieldChip6502.hpp"
#include "../Common/FieldChipFieldExec.hpp"
#include "FieldNes2A03.hpp"
#include "FieldNes2C02.hpp"
#include "FieldNesBus.hpp"
#include "FieldNesMappers.hpp"

namespace FieldChips::Nes {

struct NesCpuTraits {
    static Cpu6502Config config() noexcept {
        return {.decimalMode = false, .addrMask = 0xFFFF};
    }
    static std::uint8_t read(State& s, std::uint16_t addr) noexcept {
        return readCpuMem(s, addr);
    }
    static void write(State& s, std::uint16_t addr, std::uint8_t v) noexcept {
        writeCpuMem(s, addr, v);
    }
    static bool irqPending(State& s) noexcept { return s.irqLine; }
    static void clearIrq(State& s) noexcept { s.irqLine = false; }
    static bool consumeNmi(State& s) noexcept {
        if (!s.ppu.nmiPending) return false;
        s.ppu.nmiPending = false;
        return true;
    }
    static void onIrqServiced(State& s) noexcept { apuClearFrameIrq(s); }
};

struct NesDevices {
    static void endCpuFrame(State& s, int cpuCycles) noexcept {
        clockApu(s, cpuCycles);
        s.ppu.scrollV = s.ppu.scrollT;
        ++s.ppu.frame;
    }
};

inline int stepCpu(State& s) noexcept {
    const int cyc = step6502<State, NesCpuTraits>(s);
    tickPpu(s, (cyc > 0 ? cyc : 2) * 3);
    return cyc;
}

inline int frameCycles(Region r) noexcept {
    switch (r) {
    case Region::Pal: return 33247;
    case Region::Dendy: return 29780;
    default: return 29780; // NTSC: 341×262 PPU dots ÷ 3
    }
}

inline void runFrameCpu(State& s, int targetCycles = -1) noexcept {
    if (targetCycles < 0) targetCycles = frameCycles(s.region);
    int elapsed = 0;
    int guard = 0;
    while (elapsed < targetCycles && guard++ < kMax6502StepsPerFrame) {
        const int cyc = stepCpu(s);
        elapsed += (cyc > 0) ? cyc : 2;
        if (elapsed > 0 && (elapsed % 114) == 0)
            mapperOnScanline(s);
    }
    NesDevices::endCpuFrame(s, targetCycles);
}

} // namespace FieldChips::Nes