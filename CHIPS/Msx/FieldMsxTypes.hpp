#pragma once

#include "../Common/FieldChipTypes.hpp"
#include "../Common/FieldChipAudio.hpp"
#include "../Common/FieldChipSn76489.hpp"

#include <cstdint>
#include <vector>

namespace FieldChips::Msx {

using AudioStyle = FieldChips::AudioStyle;

struct Cart {
    std::vector<std::uint8_t> rom;
};

struct State {
    Cart cart;
    CpuZ80 z80;
    Sn76489 psg;
    std::uint8_t ram[0x10000]{};
    std::uint8_t vram[0x4000]{};
    AudioStyle audioStyle = AudioStyle::Modern;
};

inline void resetState(State& s) noexcept {
    FieldChips::resetZ80(s.z80);
    s.psg = Sn76489{};
    s.psg.style = s.audioStyle;
}

} // namespace FieldChips::Msx