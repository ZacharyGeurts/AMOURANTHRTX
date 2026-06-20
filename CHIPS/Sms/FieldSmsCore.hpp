#pragma once

#include "FieldSmsCpu.hpp"

namespace FieldChips::Sms {

inline void applyAudioConfig(State& s, AudioStyle style, int sampleRate = 44100) noexcept {
    s.audioStyle = style;
    s.psg.style = style;
    s.psg.sampleRate = sampleRate > 0 ? sampleRate : 44100;
}

inline int renderAudioFrame(State& s, float* out, int maxSamples) noexcept {
    return sn76489DrainSamples(s.psg, out, maxSamples);
}

} // namespace FieldChips::Sms