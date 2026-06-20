#pragma once

// Atari 2600 (VCS) — 6507 + TIA + RIOT 6532.

#include "../Common/FieldChipTypes.hpp"
#include "../Common/FieldChipAudio.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

namespace FieldChips::Atari2600 {

using AudioStyle = FieldChips::AudioStyle;

using Cpu = FieldChips::Cpu6502;

struct Cart {
    std::vector<std::uint8_t> rom;
    int size = 4096;
};

struct Tia {
    std::uint8_t vsync = 0;
    std::uint8_t vblank = 0;
    std::uint8_t wsync = 0;
    std::uint8_t rsync = 0;
    std::uint8_t coluP0 = 0, coluP1 = 0, coluPf = 0, coluBk = 0;
    std::uint8_t cntlp0 = 0, cntlp1 = 0, cntlm0 = 0, cntlm1 = 0;
    std::uint8_t cntlpf = 0, cntlbal = 0;
    std::uint8_t pf0 = 0, pf1 = 0, pf2 = 0;
    std::uint8_t resp0 = 0, resp1 = 0, resm0 = 0, resm1 = 0, resbl = 0;
    std::uint8_t audc0 = 0, audc1 = 0, audf0 = 0, audf1 = 0;
    std::uint8_t audv0 = 0;
    std::uint8_t audv1 = 0;
    int scanline = 0;
    int cycle = 0;
    int frame = 0;
    AudioStyle audioStyle = AudioStyle::Modern;
    float sampleAccum = 0.f;
    int sampleAccumCycles = 0;
    std::uint32_t sampleClock = 0;
    int sampleRate = 44100;
    float modernLp = 0.f;
    int pfReflect = 0;
    int pfScore = 0;
    int pfPriority = 0;
    int p0x = 0, p1x = 0, m0x = 0, m1x = 0, blx = 0;
    int p0Gfx = 0, p1Gfx = 0;
    int m0En = 0, m1En = 0, blEn = 0;
};

struct Riot {
    std::uint8_t swcha = 0xFF;
    std::uint8_t swac = 0;
    std::uint8_t swchb = 0xFF;
    std::uint8_t swbc = 0;
    std::uint8_t intim = 0;
    std::uint8_t tim1t = 0;
    int interval = 0;
    int divider = 0;
};

struct State {
    Cart cart;
    Cpu cpu;
    Tia tia;
    Riot riot;
    std::uint8_t ram[128]{};
    std::uint8_t padLeft = 0xFF;
    std::uint8_t padRight = 0xFF;
    bool irqLine = false;
};

inline void resetState(State& s) noexcept {
    FieldChips::reset6502(s.cpu);
    s.tia = Tia{};
    s.riot = Riot{};
    std::memset(s.ram, 0, sizeof s.ram);
    s.irqLine = false;
}

} // namespace FieldChips::Atari2600