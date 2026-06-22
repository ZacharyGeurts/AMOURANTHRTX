#pragma once

#include <algorithm>
#include <cmath>

#include "FieldPs1Bus.hpp"
#include "FieldPs1Cart.hpp"
#include "FieldPs1Cpu.hpp"
#include "FieldPs1GpuDie.hpp"
#include "FieldPs1Gte.hpp"

namespace FieldChips::Ps1 {

inline void applyAudioConfig(State& s, AudioStyle style, int sampleRate = 44100) noexcept {
    s.audioStyle = style;
    s.spu.style = style;
    s.spu.sampleRate = sampleRate > 0 ? sampleRate : 44100;
}

inline float mixAudioSample(State& s) noexcept {
    const float wave = static_cast<float>(std::sin(s.gpu.wavePhase));
    s.spu.level = std::clamp(wave * 0.25f + 0.1f, 0.f, 1.f);
    return s.spu.level;
}

inline int renderAudioFrame(State& s, float* out, int maxSamples) noexcept {
    if (!out || maxSamples <= 0) return 0;
    int n = 0;
    while (n < maxSamples)
        out[n++] = mixAudioSample(s);
    return n;
}

inline void runFrame(State& s) noexcept {
    mipsStep(s, 4096);
    gteRtps(s, 100, 50, 200);
    gteNclip(s);
    if (s.gpuDieWave)
        gpuQueueGp0(s, 0x02000000u);
    s.frame++;
}

} // namespace FieldChips::Ps1