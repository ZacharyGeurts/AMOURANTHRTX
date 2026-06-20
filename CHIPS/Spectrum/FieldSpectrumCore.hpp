#pragma once

#include "FieldSpectrumTypes.hpp"
#include "../Common/FieldChipSn76489.hpp"

namespace FieldChips::Spectrum {

struct SpectrumPsg {
    Sn76489 beeper;
};

inline void applyAudioConfig(State& s, AudioStyle style, int sampleRate = 44100) noexcept {
    s.audioStyle = style;
}

inline bool loadTap(State& s, const std::uint8_t* data, std::size_t size) {
    if (!data || size < 32) return false;
    s.cart.tap.assign(data, data + size);
    resetState(s);
    return true;
}

inline void runFrameCpu(State&) noexcept {}

inline void renderFrame(State&, std::uint8_t*, int, int) noexcept {}

} // namespace FieldChips::Spectrum