#pragma once

#include "../Common/FieldChipTypes.hpp"
#include "../Common/FieldChipAudio.hpp"

#include <cstdint>
#include <vector>

namespace FieldChips::Spectrum {

using AudioStyle = FieldChips::AudioStyle;

struct Cart {
    std::vector<std::uint8_t> tap;
};

struct State {
    Cart cart;
    CpuZ80 z80;
    std::uint8_t ram[0x10000]{};
    AudioStyle audioStyle = AudioStyle::Modern;
};

inline void resetState(State& s) noexcept {
    FieldChips::resetZ80(s.z80);
}

} // namespace FieldChips::Spectrum