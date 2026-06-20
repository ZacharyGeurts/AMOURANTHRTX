#pragma once

#include "../Common/FieldChipZ80.hpp"
#include "../Common/FieldChipFieldExec.hpp"
#include "FieldGenesisBus.hpp"

namespace FieldChips::Genesis {

struct GenesisM68kTraits {
    static std::uint8_t read8(State& s, std::uint32_t addr) noexcept {
        return readM68k8(s, addr);
    }
    static void write8(State& s, std::uint32_t addr, std::uint8_t v) noexcept {
        writeM68k8(s, addr, v);
    }
};

struct GenesisZ80Traits {
    static std::uint8_t read(State& s, std::uint16_t addr) noexcept {
        return readZ80(s, addr);
    }
    static void write(State& s, std::uint16_t addr, std::uint8_t v) noexcept {
        writeZ80(s, addr, v);
    }
    static std::uint8_t inPort(State& s, std::uint8_t port) noexcept {
        return z80InPort(s, port);
    }
    static void outPort(State& s, std::uint8_t port, std::uint8_t v) noexcept {
        z80OutPort(s, port, v);
    }
};

struct GenesisDevices {
    static void endCpuFrame(State& s, int cpuCycles) noexcept {
        tickVdp(s, cpuCycles);
        if (!s.z80Reset) {
            const int z80Cycles = cpuCycles / 3;
            for (int i = 0; i < z80Cycles; ++i)
                stepZ80<State, GenesisZ80Traits>(s);
            ym2612Clock(s.ym, z80Cycles);
            sn76489Clock(s.psg, z80Cycles);
        } else {
            ym2612Clock(s.ym, cpuCycles / 3);
            sn76489Clock(s.psg, cpuCycles / 3);
        }
    }
};

inline int stepCpu(State& s) noexcept {
    return stepM68000<State, GenesisM68kTraits>(s);
}

inline constexpr int kFrameCycles = 7670454 / 60;

inline void runFrameCpu(State& s, int targetCycles = kFrameCycles) noexcept {
    runM68000FieldFrame<State, GenesisM68kTraits, GenesisDevices>(s, targetCycles);
}

} // namespace FieldChips::Genesis