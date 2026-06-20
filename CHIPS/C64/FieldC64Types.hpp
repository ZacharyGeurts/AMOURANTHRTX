#pragma once

#include "../Common/FieldChipTypes.hpp"
#include "../Common/FieldChipAudio.hpp"
#include "../Common/FieldChipSid.hpp"

#include <cstdint>
#include <vector>

namespace FieldChips::C64 {

using AudioStyle = FieldChips::AudioStyle;

struct Cart {
    std::vector<std::uint8_t> rom;
};

struct State {
    Cart cart;
    Cpu6502 cpu;
    Sid6581 sid;
    std::uint8_t ram[0x10000]{};
    AudioStyle audioStyle = AudioStyle::Modern;
};

inline void resetState(State& s) noexcept {
    FieldChips::reset6502(s.cpu);
    s.sid = Sid6581{};
    s.sid.style = s.audioStyle;
}

} // namespace FieldChips::C64