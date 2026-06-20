#pragma once

#include "FieldGenesisCpu.hpp"

#include <algorithm>

namespace FieldChips::Genesis {

inline void applyAudioConfig(State& s, AudioStyle style, int sampleRate = 44100) noexcept {
    s.audioStyle = style;
    s.ym.style = style;
    s.psg.style = style;
    s.ym.sampleRate = sampleRate > 0 ? sampleRate : 44100;
    s.psg.sampleRate = sampleRate > 0 ? sampleRate : 44100;
}

inline float mixAudioSample(State& s) noexcept {
    const float fm = ym2612MixSample(s.ym);
    const float psg = sn76489MixSample(s.psg);
    return std::clamp((fm + psg) * 0.5f, -1.f, 1.f);
}

inline int renderAudioFrame(State& s, float* out, int maxSamples) noexcept {
    if (!out || maxSamples <= 0) return 0;
    int n = 0;
    while (n < maxSamples)
        out[n++] = mixAudioSample(s);
    return n;
}

} // namespace FieldChips::Genesis