#pragma once

#include "../Common/FieldChipTypes.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace FieldChips::Ps2 {

struct Cart {
    std::vector<std::uint8_t> rom;
    char title[32]{};
    std::uint32_t loadAddr = 0xBFC00000u;
    std::uint32_t entryPc = 0xBFC00000u;
};

struct EeMips {
    std::uint32_t gpr[32]{};
    std::uint32_t pc = 0xBFC00000u;
    std::uint32_t cycles = 0;
};

struct GsDie {
    std::uint32_t dieCycles = 0;
    double wavePhase = 0.0;
    bool waveActive = false;
    std::uint32_t fb[640 * 448]{};
    int fbW = 640;
    int fbH = 448;
};

struct State {
    Cart cart;
    EeMips cpu;
    GsDie gs;
    std::uint8_t ram[32 * 1024 * 1024]{};
    bool gpuDieWave = true;
    int frame = 0;
};

inline void resetState(State& s) noexcept {
    s.cpu = EeMips{};
    s.gs = GsDie{};
    s.frame = 0;
    std::memset(s.ram, 0, sizeof s.ram);
}

} // namespace FieldChips::Ps2