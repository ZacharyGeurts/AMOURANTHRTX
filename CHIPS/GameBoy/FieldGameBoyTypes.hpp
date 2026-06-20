#pragma once

#include "../Common/FieldChipTypes.hpp"
#include "../Common/FieldChipAudio.hpp"
#include "../Common/FieldChipSm83.hpp"

#include <cstdint>
#include <vector>

namespace FieldChips::GameBoy {

using AudioStyle = FieldChips::AudioStyle;

struct Cart {
    std::vector<std::uint8_t> rom;
};

struct Apu {
    AudioStyle style = AudioStyle::Modern;
    int sampleRate = 44100;
    float modernLp = 0.f;
};

struct State {
    Cart cart;
    CpuSm83 cpu;
    Apu apu;
    std::uint8_t ram[0x2000]{};
    std::uint8_t vram[0x2000]{};
    AudioStyle audioStyle = AudioStyle::Modern;
};

inline void resetState(State& s) noexcept {
    FieldChips::resetSm83(s.cpu);
    s.apu = Apu{};
    s.apu.style = s.audioStyle;
}

} // namespace FieldChips::GameBoy