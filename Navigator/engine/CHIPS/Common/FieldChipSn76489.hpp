#pragma once

// TI SN76489 PSG — SMS, Genesis, ColecoVision. Classic + Modern output modes.

#include "FieldChipAudio.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace FieldChips {

struct Sn76489Tone {
    int period = 0;
    int counter = 0;
    bool high = false;
    int attenuation = 15;
    float blipPhase = 0.f;
};

struct Sn76489 {
    Sn76489Tone tone[3]{};
    int noisePeriod = 0;
    int noiseCounter = 0;
    int noiseAtten = 15;
    std::uint16_t lfsr = 0x8000;
    bool noiseWhite = true;
    bool latchTone = true;
    int latchChannel = 0;
    AudioStyle style = AudioStyle::Modern;
    int sampleRate = 44100;
    float sampleAccum = 0.f;
    int sampleAccumCycles = 0;
    std::uint32_t sampleClock = 0;
    float modernLp = 0.f;
};

namespace Sn76489Detail {

inline constexpr int kNoisePeriod[8] = { 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400, 0x400 };

inline void clockTone(Sn76489Tone& ch, int chipHz) noexcept {
    if (ch.period == 0) return;
    if (ch.counter <= 0) {
        ch.counter = (ch.period + 1) * 16;
        ch.high = !ch.high;
    }
    ch.counter -= chipHz / 44100;
    if (ch.counter < 0) ch.counter = 0;
}

inline float toneLevel(const Sn76489Tone& ch, AudioStyle style, int chipHz) noexcept {
    if (ch.attenuation >= 15) return 0.f;
    const float amp = (15.f - static_cast<float>(ch.attenuation)) / 15.f * 0.35f;
    if (style == AudioStyle::Classic)
        return AudioDetail::squareClassic(ch.high, amp);
    const float dt = static_cast<float>((ch.period + 1) * 16) / static_cast<float>(chipHz);
    auto& mut = const_cast<Sn76489Tone&>(ch);
    return AudioDetail::squareModern(ch.high, amp, mut.blipPhase, dt);
}

inline float noiseLevel(Sn76489& psg, int chipHz) noexcept {
    if (psg.noiseAtten >= 15) return 0.f;
    const float amp = (15.f - static_cast<float>(psg.noiseAtten)) / 15.f * 0.3f;
    const bool bit = (psg.lfsr & 1) == 0;
    if (psg.style == AudioStyle::Classic)
        return bit ? amp : 0.f;
    static float nPhase = 0.f;
    const float dt = 1.f / static_cast<float>(chipHz) * static_cast<float>(psg.noisePeriod + 1) * 32.f;
    return AudioDetail::squareModern(bit, amp, nPhase, std::max(dt, 1e-6f));
}

} // namespace Sn76489Detail

inline void sn76489Write(Sn76489& psg, std::uint8_t v) noexcept {
    if (v & 0x80) {
        psg.latchChannel = (v >> 5) & 3;
        psg.latchTone = (v & 0x10) == 0;
        if (psg.latchTone) {
            auto& ch = psg.tone[psg.latchChannel];
            ch.attenuation = v & 0x0F;
        } else if (psg.latchChannel == 3) {
            psg.noiseAtten = v & 0x0F;
        }
    } else if (psg.latchTone && psg.latchChannel < 3) {
        auto& ch = psg.tone[psg.latchChannel];
        if (psg.latchChannel == 0)
            ch.period = (ch.period & 0x3F0) | (v & 0x0F);
        else
            ch.period = (ch.period & 0x00F) | ((v & 0x3F) << 4);
    } else if (!psg.latchTone && psg.latchChannel == 3) {
        psg.noisePeriod = v & 3;
        psg.noiseWhite = (v & 4) != 0;
        psg.lfsr = 0x8000;
    }
}

inline float sn76489MixSample(Sn76489& psg, int chipHz = 3579545) noexcept {
    float out = 0.f;
    for (int i = 0; i < 3; ++i)
        out += Sn76489Detail::toneLevel(psg.tone[i], psg.style, chipHz);
    out += Sn76489Detail::noiseLevel(psg, chipHz);
    return AudioDetail::finalizeSample(out, psg.style, psg.modernLp, 0.1f, 1.2f);
}

inline void sn76489Clock(Sn76489& psg, int cpuCycles, int chipHz = 3579545) noexcept {
    for (int i = 0; i < cpuCycles; ++i) {
        for (int t = 0; t < 3; ++t) {
            auto& ch = psg.tone[t];
            if (ch.period == 0) continue;
            if (--ch.counter <= 0) {
                ch.counter = (ch.period + 1) * 16;
                ch.high = !ch.high;
            }
        }
        if (--psg.noiseCounter <= 0) {
            psg.noiseCounter = (psg.noisePeriod + 1) * 16;
            const int fb = (psg.lfsr & 1) ^ ((psg.lfsr >> (psg.noiseWhite ? 6 : 8)) & 1);
            psg.lfsr = static_cast<std::uint16_t>((psg.lfsr >> 1) | (fb << 15));
        }
    }
    psg.sampleClock += static_cast<std::uint32_t>(cpuCycles);
    const int threshold = std::max(1, chipHz / psg.sampleRate);
    while (static_cast<int>(psg.sampleClock) >= threshold) {
        psg.sampleClock -= static_cast<std::uint32_t>(threshold);
        psg.sampleAccum += sn76489MixSample(psg, chipHz);
        ++psg.sampleAccumCycles;
    }
}

inline int sn76489DrainSamples(Sn76489& psg, float* out, int maxSamples) noexcept {
    if (!out || maxSamples <= 0) return 0;
    int n = 0;
    if (psg.sampleAccumCycles > 0) {
        out[n++] = psg.sampleAccum / static_cast<float>(psg.sampleAccumCycles);
        psg.sampleAccum = 0.f;
        psg.sampleAccumCycles = 0;
    }
    while (n < maxSamples)
        out[n++] = sn76489MixSample(psg);
    return n;
}

} // namespace FieldChips