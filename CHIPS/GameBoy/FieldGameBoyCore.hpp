#pragma once

#include "FieldGameBoyTypes.hpp"

namespace FieldChips::GameBoy {

inline void applyAudioConfig(State& s, AudioStyle style, int sampleRate = 44100) noexcept {
    s.audioStyle = style;
    s.apu.style = style;
    s.apu.sampleRate = sampleRate > 0 ? sampleRate : 44100;
}

inline bool loadRom(State& s, const std::uint8_t* data, std::size_t size) {
    if (!data || size < 256) return false;
    s.cart.rom.assign(data, data + size);
    resetState(s);
    return true;
}

inline float apuMixSample(State& s) noexcept {
    float out = 0.f;
    return AudioDetail::finalizeSample(out, s.apu.style, s.apu.modernLp);
}

inline void runFrameCpu(State&) noexcept {}

inline int renderAudioFrame(State& s, float* out, int maxSamples) noexcept {
    if (!out || maxSamples <= 0) return 0;
    for (int i = 0; i < maxSamples; ++i)
        out[i] = apuMixSample(s);
    return maxSamples;
}

inline void renderFrame(State&, std::uint8_t*, int, int) noexcept {}

} // namespace FieldChips::GameBoy