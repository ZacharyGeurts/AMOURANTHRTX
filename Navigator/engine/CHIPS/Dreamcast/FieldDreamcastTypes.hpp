#pragma once

#include "../Common/FieldChipTypes.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace FieldChips::Dreamcast {

struct Cart {
    std::vector<std::uint8_t> rom;
    char title[32]{};
    std::uint32_t entryPc = 0x8C010000u;
};

struct Sh4Cpu {
    std::uint32_t gpr[16]{};
    std::uint32_t pc = 0x8C010000u;
    std::uint32_t cycles = 0;
};

struct PvrDie {
    std::uint32_t dieCycles = 0;
    double wavePhase = 0.0;
    bool waveActive = false;
    std::uint32_t fb[640 * 480]{};
    int fbW = 640;
    int fbH = 480;
};

struct State {
    Cart cart;
    Sh4Cpu cpu;
    PvrDie pvr;
    std::uint8_t ram[16 * 1024 * 1024]{};
    bool gpuDieWave = true;
    int frame = 0;
};

inline void resetState(State& s) noexcept {
    s.cpu = Sh4Cpu{};
    s.pvr = PvrDie{};
    s.frame = 0;
    std::memset(s.ram, 0, sizeof s.ram);
}

} // namespace FieldChips::Dreamcast