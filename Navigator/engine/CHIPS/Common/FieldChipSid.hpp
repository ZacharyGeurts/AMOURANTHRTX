#pragma once

// MOS 6581 SID — C64 audio scaffold with Classic + Modern modes.

#include "FieldChipAudio.hpp"

#include <cmath>
#include <cstdint>

namespace FieldChips {

struct SidVoice {
    std::uint16_t freq = 0;
    std::uint16_t pw = 0;
    std::uint8_t ctrl = 0;
    std::uint8_t ad = 0;
    std::uint8_t sr = 0;
    int phase = 0;
    int env = 0;
    float blipPhase = 0.f;
};

struct Sid6581 {
    SidVoice voice[3]{};
    std::uint8_t filt = 0;
    std::uint8_t resFilt = 0;
    AudioStyle style = AudioStyle::Modern;
    int sampleRate = 44100;
    float sampleAccum = 0.f;
    int sampleAccumCycles = 0;
    std::uint32_t sampleClock = 0;
    float modernLp = 0.f;
};

inline float sidMixSample(Sid6581& sid) noexcept {
    float out = 0.f;
    for (int i = 0; i < 3; ++i) {
        const auto& v = sid.voice[i];
        if (v.freq == 0) continue;
        const bool high = (v.phase & 0x800) != 0;
        const float amp = static_cast<float>(v.env) / 255.f * 0.3f;
        if (sid.style == AudioStyle::Classic)
            out += high ? amp : 0.f;
        else {
            const float dt = 1.f / static_cast<float>(std::max(1, v.freq));
            auto& mut = sid.voice[i];
            out += AudioDetail::squareModern(high, amp, mut.blipPhase, dt);
        }
    }
    return AudioDetail::finalizeSample(out, sid.style, sid.modernLp);
}

inline void sidClock(Sid6581& sid, int cpuCycles, int chipHz = 985248) noexcept {
    for (int c = 0; c < cpuCycles; ++c) {
        for (int i = 0; i < 3; ++i) {
            auto& v = sid.voice[i];
            v.phase = (v.phase + std::max(1, v.freq)) & 0xFFF;
            if ((c & 127) == 0 && v.env > 0) --v.env;
        }
    }
    sid.sampleClock += static_cast<std::uint32_t>(cpuCycles);
    const int threshold = std::max(1, chipHz / sid.sampleRate);
    while (static_cast<int>(sid.sampleClock) >= threshold) {
        sid.sampleClock -= static_cast<std::uint32_t>(threshold);
        sid.sampleAccum += sidMixSample(sid);
        ++sid.sampleAccumCycles;
    }
}

inline int sidDrainSamples(Sid6581& sid, float* out, int maxSamples) noexcept {
    if (!out || maxSamples <= 0) return 0;
    int n = 0;
    if (sid.sampleAccumCycles > 0) {
        out[n++] = sid.sampleAccum / static_cast<float>(sid.sampleAccumCycles);
        sid.sampleAccum = 0.f;
        sid.sampleAccumCycles = 0;
    }
    while (n < maxSamples)
        out[n++] = sidMixSample(sid);
    return n;
}

} // namespace FieldChips