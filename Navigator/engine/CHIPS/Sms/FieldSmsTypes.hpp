#pragma once

// Sega Master System — Z80 + VDP (TMS9918-class) + SN76489.

#include "../Common/FieldChipTypes.hpp"
#include "../Common/FieldChipAudio.hpp"
#include "../Common/FieldChipSn76489.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace FieldChips::Sms {

using AudioStyle = FieldChips::AudioStyle;

struct Cart {
    std::vector<std::uint8_t> rom;
    int size = 0;
};

struct Vdp {
    std::uint8_t vram[0x4000]{};
    std::uint8_t cram[32]{};
    std::uint8_t reg[16]{};
    std::uint8_t status = 0;
    std::uint16_t addr = 0;
    bool code = false;
    int scanline = 0;
    int cycle = 0;
    int frame = 0;
};

struct State {
    Cart cart;
    CpuZ80 z80;
    Vdp vdp;
    Sn76489 psg;
    std::uint8_t ram[0x2000]{};
    std::uint8_t pad1 = 0;
    std::uint8_t pad2 = 0;
    bool irqLine = false;
    AudioStyle audioStyle = AudioStyle::Modern;
};

inline void resetState(State& s) noexcept {
    FieldChips::resetZ80(s.z80);
    s.vdp = Vdp{};
    s.psg = Sn76489{};
    s.psg.style = s.audioStyle;
    std::memset(s.ram, 0, sizeof s.ram);
    s.irqLine = false;
}

} // namespace FieldChips::Sms