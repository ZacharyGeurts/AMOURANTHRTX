#pragma once

#include "FieldA2600Cpu.hpp"

namespace FieldChips::Atari2600 {

inline void applyAudioConfig(State& s, AudioStyle style, int sampleRate = 44100) noexcept {
    s.tia.audioStyle = style;
    s.tia.sampleRate = sampleRate > 0 ? sampleRate : 44100;
}

} // namespace FieldChips::Atari2600