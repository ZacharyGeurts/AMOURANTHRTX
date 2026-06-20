#pragma once

#include "FieldC64Types.hpp"

namespace FieldChips::C64 {

inline void applyAudioConfig(State& s, AudioStyle style, int sampleRate = 44100) noexcept {
    s.audioStyle = style;
    s.sid.style = style;
    s.sid.sampleRate = sampleRate > 0 ? sampleRate : 44100;
}

inline bool loadRom(State& s, const std::uint8_t* data, std::size_t size) {
    if (!data || size < 256) return false;
    s.cart.rom.assign(data, data + size);
    resetState(s);
    return true;
}

inline void runFrameCpu(State& s) noexcept { sidClock(s.sid, 985248 / 60); }

inline int renderAudioFrame(State& s, float* out, int maxSamples) noexcept {
    return sidDrainSamples(s.sid, out, maxSamples);
}

inline void renderFrame(State&, std::uint8_t*, int, int) noexcept {}

} // namespace FieldChips::C64