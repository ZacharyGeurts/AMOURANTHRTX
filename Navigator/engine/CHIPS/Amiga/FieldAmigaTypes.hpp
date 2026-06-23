#pragma once

#include "../Common/FieldChipTypes.hpp"
#include "../Common/FieldChipAudio.hpp"
#include "../Common/FieldChipM68000.hpp"

#include <cstdint>
#include <vector>

namespace FieldChips::Amiga {

using AudioStyle = FieldChips::AudioStyle;

struct M68kCtx {
    FieldChips::CpuM68000 m68k{};
};

struct Paula {
    int sampleRate = 44100;
    float level = 0.f;
    int loveBeat = 0;
};

struct Denise {
    int fbW = 320;
    int fbH = 256;
    std::vector<std::uint32_t> fb;
    bool aga = true;
};

struct State {
    M68kCtx cpu;
    Paula paula;
    Denise denise;
    std::vector<std::uint8_t> chipRam;
    AudioStyle audioStyle = AudioStyle::Modern;
    int frame = 0;
    int loveScore = 0;
};

inline void resetState(State& s) noexcept {
    s.cpu = M68kCtx{};
    s.paula = Paula{};
    s.denise = Denise{};
    s.denise.fb.assign(static_cast<std::size_t>(s.denise.fbW * s.denise.fbH), 0u);
    s.chipRam.assign(512 * 1024, 0);
    s.frame = 0;
    s.loveScore = 0;
}

} // namespace FieldChips::Amiga