#pragma once

#include "../Common/FieldChip6502.hpp"
#include "../Common/FieldChip6507.hpp"
#include "../Common/FieldChipFieldExec.hpp"
#include "FieldA2600Bus.hpp"
#include "FieldA2600Riot.hpp"
#include "FieldA2600Tia.hpp"

namespace FieldChips::Atari2600 {

struct A2600CpuTraits {
    static Cpu6502Config config() noexcept { return k6507Config; }
    static std::uint8_t read(State& s, std::uint16_t addr) noexcept {
        return readCpuMem(s, static_cast<std::uint16_t>(addr & k6507Config.addrMask));
    }
    static void write(State& s, std::uint16_t addr, std::uint8_t v) noexcept {
        writeCpuMem(s, static_cast<std::uint16_t>(addr & k6507Config.addrMask), v);
    }
    static bool irqPending(State& s) noexcept { return s.irqLine; }
    static void clearIrq(State& s) noexcept { s.irqLine = false; }
    static bool consumeNmi(State&) noexcept { return false; }
    static void onIrqServiced(State& s) noexcept { s.irqLine = false; }
};

struct A2600Devices {
    static void endCpuFrame(State& s, int cpuCycles) noexcept {
        riotTick(s, cpuCycles);
        tickTia(s, cpuCycles);
        clockTiaAudio(s, cpuCycles);
    }
};

inline int stepCpu(State& s) noexcept {
    return step6502<State, A2600CpuTraits>(s);
}

inline constexpr int kFrameCycles = 1193182 / 60;

inline void runFrameCpu(State& s, int targetCycles = kFrameCycles) noexcept {
    runFieldFrame<State, A2600CpuTraits, A2600Devices>(s, targetCycles, nullptr);
}

} // namespace FieldChips::Atari2600