#pragma once

#include "../Common/FieldChipTypes.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace FieldChips::N64 {

struct Cart {
    std::vector<std::uint8_t> rom;
    char title[32]{};
    std::uint32_t loadAddr = 0x80000400u;
    std::uint32_t entryPc = 0x80000400u;
};

struct MipsRegs {
    std::uint32_t gpr[32]{};
    std::uint32_t pc = 0x80000400u;
    std::uint32_t cycles = 0;
};

struct RspDie {
    std::uint32_t pc = 0x1000u;
    std::uint32_t dieCycles = 0;
    double wavePhase = 0.0;
    bool waveActive = false;
    std::uint32_t fb[320 * 240]{};
    int fbW = 320;
    int fbH = 240;
};

struct State {
    Cart cart;
    MipsRegs cpu;
    RspDie rsp;
    std::uint8_t ram[4 * 1024 * 1024]{};
    bool gpuDieWave = true;
    int frame = 0;
};

inline void resetState(State& s) noexcept {
    s.cpu = MipsRegs{};
    s.rsp = RspDie{};
    s.frame = 0;
    std::memset(s.ram, 0, sizeof s.ram);
}

} // namespace FieldChips::N64