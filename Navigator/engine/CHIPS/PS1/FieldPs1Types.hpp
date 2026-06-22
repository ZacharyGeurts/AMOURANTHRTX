#pragma once

#include "../Common/FieldChipTypes.hpp"
#include "../Common/FieldChipAudio.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace FieldChips::Ps1 {

using AudioStyle = FieldChips::AudioStyle;

struct Cart {
    std::vector<std::uint8_t> rom;
    bool psxExe = false;
    bool cdImage = false;
    char title[32]{};
    std::uint32_t loadAddr = 0x80010000u;
    std::uint32_t entryPc = 0x80010000u;
};

struct MipsRegs {
    std::uint32_t gpr[32]{};
    std::uint32_t pc = 0xBFC00000u;
    std::uint32_t hi = 0;
    std::uint32_t lo = 0;
    std::uint32_t cycles = 0;
};

struct GpuDie {
    std::uint32_t gp0 = 0;
    std::uint32_t gp1 = 0;
    std::uint8_t vram[512 * 1024]{};
    std::uint32_t fb[320 * 240]{};
    int fbW = 320;
    int fbH = 240;
    std::uint32_t dieCycles = 0;
    double wavePhase = 0.0;
    bool waveActive = false;
};

struct Gte {
    std::int32_t mtx[3][3]{};
    std::int32_t tr[3]{};
    std::int32_t ir[3]{};
    std::uint32_t flag = 0;
    int ops = 0;
};

struct Spu {
    AudioStyle style = AudioStyle::Modern;
    int sampleRate = 44100;
    float level = 0.f;
};

struct State {
    Cart cart;
    MipsRegs cpu;
    GpuDie gpu;
    Gte gte;
    Spu spu;
    std::uint8_t ram[2 * 1024 * 1024]{};
    std::uint8_t scratch[64 * 1024]{};
    AudioStyle audioStyle = AudioStyle::Modern;
    bool gpuDieWave = true;
    int frame = 0;
};

inline void resetState(State& s) noexcept {
    s.cpu = MipsRegs{};
    s.gpu = GpuDie{};
    s.gte = Gte{};
    s.spu = Spu{};
    s.spu.style = s.audioStyle;
    s.frame = 0;
    std::memset(s.ram, 0, sizeof s.ram);
    std::memset(s.scratch, 0, sizeof s.scratch);
}

} // namespace FieldChips::Ps1