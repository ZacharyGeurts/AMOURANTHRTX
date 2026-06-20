#pragma once

#include "FieldSnesTypes.hpp"
#include "FieldSnesCart.hpp"
#include "FieldSnesPpu.hpp"
#include "FieldSnesSuperFx.hpp"

#include <cmath>

namespace FieldChips::Snes {

inline void applyAudioConfig(State& s, AudioStyle style, int sampleRate = 44100) noexcept {
    s.audioStyle = style;
    s.apu.style = style;
    s.apu.sampleRate = sampleRate > 0 ? sampleRate : 44100;
}

inline float apuMixSample(State& s) noexcept {
    float out = 0.f;
    if (s.gsu.present && s.gsu.plotOps > 0)
        out += 0.02f * std::sin(static_cast<float>(s.ppu.frame) * 0.11f);
    return AudioDetail::finalizeSample(out, s.apu.style, s.apu.modernLp);
}

inline int renderAudioFrame(State& s, float* out, int maxSamples) noexcept {
    if (!out || maxSamples <= 0) return 0;
    for (int i = 0; i < maxSamples; ++i)
        out[i] = apuMixSample(s);
    return maxSamples;
}

inline constexpr int kFrameCycles = 1364 * 262;

inline void runFrameCpu(State& s) noexcept {
    if (s.gsu.present)
        runGsuFrame(s, s.thermoSteps);
    s.cpu.totalCycles += kFrameCycles / 8;
    s.cpu.cycles += kFrameCycles / 8;
}

inline void renderFrame(State& s, std::uint8_t* fb, int w, int h) noexcept {
    if (!fb) return;
    if (s.gsu.present)
        renderGsuScreen(s, fb, w, h);
    else
        renderChrPreview(s, fb, w, h);
    ++s.ppu.frame;
}

} // namespace FieldChips::Snes