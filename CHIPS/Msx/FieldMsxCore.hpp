#pragma once

#include "FieldMsxTypes.hpp"

namespace FieldChips::Msx {

inline void applyAudioConfig(State& s, AudioStyle style, int sampleRate = 44100) noexcept {
    s.audioStyle = style;
    s.psg.style = style;
    s.psg.sampleRate = sampleRate > 0 ? sampleRate : 44100;
}

inline bool loadRom(State& s, const std::uint8_t* data, std::size_t size) {
    if (!data || size < 256) return false;
    s.cart.rom.assign(data, data + size);
    resetState(s);
    return true;
}

inline void runFrameCpu(State& s) noexcept { sn76489Clock(s.psg, 3579545 / 60); }

inline int renderAudioFrame(State& s, float* out, int maxSamples) noexcept {
    return sn76489DrainSamples(s.psg, out, maxSamples);
}

inline void renderFrame(State&, std::uint8_t*, int, int) noexcept {}

} // namespace FieldChips::Msx