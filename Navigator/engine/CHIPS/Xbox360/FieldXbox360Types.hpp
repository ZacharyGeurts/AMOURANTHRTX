#pragma once

#include "../Common/FieldChipTypes.hpp"
#include "../Common/FieldChipAudio.hpp"

#include <cstdint>
#include <vector>

namespace FieldChips::Xbox360 {

using AudioStyle = FieldChips::AudioStyle;

struct XenonCpu {
    std::uint32_t gpr[32]{};
    std::uint32_t pc = 0x80000000u;
    std::uint32_t cycles = 0;
};

struct XenosGpuDie {
    std::uint32_t dieCycles = 0;
    double wavePhase = 0.0;
    bool waveActive = false;
    int fbW = 1280;
    int fbH = 720;
    std::vector<std::uint32_t> fb;
};

struct State {
    XenonCpu cpu;
    XenosGpuDie gpu;
    std::vector<std::uint8_t> mem;
    AudioStyle audioStyle = AudioStyle::Modern;
    bool gpuDieWave = true;
    int frame = 0;
};

inline void resetState(State& s) noexcept {
    s.cpu = XenonCpu{};
    s.gpu = XenosGpuDie{};
    s.gpu.fb.assign(static_cast<std::size_t>(s.gpu.fbW * s.gpu.fbH), 0u);
    s.mem.assign(64 * 1024 * 1024, 0);
    s.frame = 0;
}

} // namespace FieldChips::Xbox360