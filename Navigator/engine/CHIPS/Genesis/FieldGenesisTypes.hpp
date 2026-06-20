#pragma once

// Sega Genesis / Mega Drive — M68000 + Z80 sound + YM2612 + SN76489 + VDP.

#include "../Common/FieldChipTypes.hpp"
#include "../Common/FieldChipAudio.hpp"
#include "../Common/FieldChipSn76489.hpp"
#include "../Common/FieldChipYm2612.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace FieldChips::Genesis {

using AudioStyle = FieldChips::AudioStyle;

struct Cart {
    std::vector<std::uint8_t> rom;
    int size = 0;
};

struct Vdp {
    std::uint8_t vram[0x10000]{};
    std::uint8_t cram[128]{};
    std::uint8_t vsram[80]{};
    std::uint32_t ctrl = 0;
    std::uint16_t addr = 0;
    bool code = false;
    int scanline = 0;
    int cycle = 0;
    int frame = 0;
    bool vblank = false;
    bool hint = false;
};

struct State {
    Cart cart;
    CpuM68000 m68k;
    CpuZ80 z80;
    Vdp vdp;
    Ym2612 ym;
    Sn76489 psg;
    std::uint8_t ram[0x10000]{};
    std::uint8_t z80Ram[0x8000]{};
    std::uint8_t pad1 = 0;
    std::uint8_t pad2 = 0;
    bool m68kIrq = false;
    bool z80Irq = false;
    bool z80Reset = false;
    AudioStyle audioStyle = AudioStyle::Modern;
};

inline void resetState(State& s) noexcept {
    FieldChips::resetM68000(s.m68k);
    FieldChips::resetZ80(s.z80);
    s.vdp = Vdp{};
    s.ym = Ym2612{};
    s.psg = Sn76489{};
    s.ym.style = s.audioStyle;
    s.psg.style = s.audioStyle;
    std::memset(s.ram, 0, sizeof s.ram);
    std::memset(s.z80Ram, 0, sizeof s.z80Ram);
    s.m68kIrq = s.z80Irq = false;
    s.z80Reset = false;
}

} // namespace FieldChips::Genesis