#pragma once

// Ricoh 2A03 APU — pulse/triangle/noise/DMC with Authentic vs Modern output styles.

#include "FieldNesTypes.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace FieldChips::Nes {

namespace ApuDetail {

inline constexpr int kLengthTable[32] = {
    10, 254, 20, 2, 40, 4, 80, 6, 160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30,
};

inline constexpr int kNoisePeriodNtsc[16] = {
    4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068,
};

inline constexpr int kDmcRateNtsc[16] = {
    428, 380, 340, 320, 286, 254, 226, 214, 190, 160, 142, 128, 106, 84, 72, 54,
};

inline constexpr std::uint8_t kDutyTable[4][8] = {
    {0, 1, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 0, 0, 0, 0, 0},
    {0, 1, 1, 1, 1, 0, 0, 0},
    {1, 0, 0, 1, 1, 1, 1, 1},
};

inline constexpr int kTriangleSeq[32] = {
    15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0,
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
};

inline int cpuHz(Region r) noexcept {
    switch (r) {
    case Region::Pal: return 1662607;
    case Region::Dendy: return 1773448;
    default: return 1789773;
    }
}

inline float mixAuthentic(int p1, int p2, int tri, int noise, int dmc) noexcept {
    const float pulseOut = 95.52f / (8128.f / static_cast<float>(p1 + p2 + 1) + 36.f);
    const float triOut = 163.67f / (24329.f / static_cast<float>(tri + 1) + 36.f);
    const float noiseOut = 122.41f / (22638.f / static_cast<float>(noise + 1) + 36.f);
    const float dmcOut = 226.0f / (static_cast<float>(dmc) + 1.f);
    return (pulseOut + triOut + noiseOut + dmcOut) / 4.f - 0.5f;
}

inline float polyBlep(float t, float dt) noexcept {
    if (t < dt) {
        t /= dt;
        return t + t - t * t - 1.f;
    }
    if (t > 1.f - dt) {
        t = (t - 1.f) / dt;
        return t * t + t + t + 1.f;
    }
    return 0.f;
}

inline float pulseSample(PulseChan& ch, bool modern) noexcept {
    if (ch.lengthCounter == 0 || ch.timerPeriod < 8 || ch.sweepMute) return 0.f;
    const int duty = (ch.reg[0] >> 6) & 3;
    const int vol = (ch.reg[0] & 0x10) ? ch.envelopeDecay : (ch.reg[0] & 0x0F);
    const bool high = kDutyTable[duty][ch.dutyPos & 7] != 0;
    const float amp = static_cast<float>(vol) / 15.f;
    if (!modern) return high ? amp : 0.f;
    const float dt = static_cast<float>(ch.timerPeriod + 1) / static_cast<float>(cpuHz(Region::Ntsc));
    float y = high ? amp : 0.f;
    y += polyBlep(ch.blipPhase, dt);
    ch.blipPhase += 1.f / static_cast<float>(ch.timerPeriod + 1);
    if (ch.blipPhase >= 1.f) ch.blipPhase -= 1.f;
    ch.blipLast = y;
    return y;
}

} // namespace ApuDetail

inline void clockPulseSweep(PulseChan& ch, int channel) noexcept;
inline void clockPulseEnvelope(PulseChan& ch) noexcept;
inline void clockNoiseEnvelope(NoiseChan& ch) noexcept;
inline void clockApuQuarter(State& s, bool force) noexcept;
inline void clockApuHalf(State& s) noexcept;

inline void clockPulseSweep(PulseChan& ch, int channel) noexcept {
    if (!ch.sweepEnable || channel != 0) return;
    const int change = ch.timerPeriod >> ch.sweepShift;
    int target = ch.timerPeriod + (ch.sweepNegate ? -change - (channel == 0 ? 1 : 0) : change);
    if (ch.sweepReload && ch.sweepPeriod > 0) {
        if (ch.sweepEnable && ch.sweepShift > 0 && target < 0x800)
            ch.timerPeriod = target;
        ch.sweepReload = false;
    }
    ch.sweepMute = ch.timerPeriod < 8;
}

inline void clockPulseEnvelope(PulseChan& ch) noexcept {
    if (ch.envelopeStart) {
        ch.envelopeStart = false;
        ch.envelopeDecay = 15;
    } else if (ch.envelopeDecay > 0)
        --ch.envelopeDecay;
    else if (ch.envelopeLoop)
        ch.envelopeDecay = 15;
}

inline void clockNoiseEnvelope(NoiseChan& ch) noexcept {
    if (ch.envelopeStart) {
        ch.envelopeStart = false;
        ch.envelopeDecay = 15;
    } else if (ch.envelopeDecay > 0)
        --ch.envelopeDecay;
    else if (ch.envelopeLoop)
        ch.envelopeDecay = 15;
}

inline void clockApuQuarter(State& s, bool force) noexcept {
    auto& a = s.apu;
    for (int i = 0; i < 2; ++i) {
        if (a.pulse[i].lengthCounter > 0 && !a.pulse[i].envelopeLoop)
            --a.pulse[i].lengthCounter;
        clockPulseEnvelope(a.pulse[i]);
    }
    if (a.triangle.linearReloadFlag) {
        a.triangle.linearCounter = a.triangle.linearReload;
        a.triangle.linearReloadFlag = false;
    } else if (a.triangle.linearCounter > 0 && !a.triangle.linearHalt)
        --a.triangle.linearCounter;
    if (a.triangle.lengthCounter > 0 && !a.triangle.linearHalt)
        --a.triangle.lengthCounter;
    if (a.noise.lengthCounter > 0 && !a.noise.envelopeLoop)
        --a.noise.lengthCounter;
    clockNoiseEnvelope(a.noise);
    if (force) {
        for (int i = 0; i < 2; ++i) clockPulseSweep(a.pulse[i], i);
    }
}

inline void clockApuHalf(State& s) noexcept {
    for (int i = 0; i < 2; ++i) clockPulseSweep(s.apu.pulse[i], i);
}

inline void apuWriteReg(State& s, std::uint16_t addr, std::uint8_t v) noexcept {
    addr = static_cast<std::uint16_t>(0x4000 + (addr & 0x1F));
    auto& a = s.apu;
    switch (addr) {
    case 0x4000: case 0x4001: case 0x4002: case 0x4003: {
        const int i = (addr - 0x4000) >> 2;
        a.pulse[i].reg[addr & 3] = v;
        if ((addr & 3) == 3) {
            a.pulse[i].lengthCounter = ApuDetail::kLengthTable[(v >> 3) & 0x1F];
            a.pulse[i].envelopeStart = true;
            a.pulse[i].dutyPos = 0;
            if (i == 0) a.pulse[i].sweepReload = true;
        }
        if ((addr & 3) == 0) {
            a.pulse[i].envelopeLoop = (v & 0x20) != 0;
            a.pulse[i].envelopeVolume = v & 0x0F;
        }
        if ((addr & 3) == 1 && i == 0) {
            a.pulse[i].sweepEnable = (v & 0x80) != 0;
            a.pulse[i].sweepPeriod = (v >> 4) & 7;
            a.pulse[i].sweepNegate = (v & 8) != 0;
            a.pulse[i].sweepShift = v & 7;
        }
        if ((addr & 3) == 2)
            a.pulse[i].timerPeriod = (a.pulse[i].timerPeriod & 0x700) | v;
        if ((addr & 3) == 3)
            a.pulse[i].timerPeriod = static_cast<int>((a.pulse[i].timerPeriod & 0xFF) | ((v & 7) << 8));
        break;
    }
    case 0x4004: case 0x4005: case 0x4006: case 0x4007: {
        const int i = 1;
        const int sub = addr & 3;
        a.pulse[i].reg[sub] = v;
        if (sub == 3) {
            a.pulse[i].lengthCounter = ApuDetail::kLengthTable[(v >> 3) & 0x1F];
            a.pulse[i].envelopeStart = true;
            a.pulse[i].dutyPos = 0;
        }
        if (sub == 0) {
            a.pulse[i].envelopeLoop = (v & 0x20) != 0;
            a.pulse[i].envelopeVolume = v & 0x0F;
        }
        if (sub == 2) a.pulse[i].timerPeriod = (a.pulse[i].timerPeriod & 0x700) | v;
        if (sub == 3) a.pulse[i].timerPeriod = static_cast<int>((a.pulse[i].timerPeriod & 0xFF) | ((v & 7) << 8));
        break;
    }
    case 0x4008:
        a.triangle.reg[0] = v;
        a.triangle.linearHalt = (v & 0x80) != 0;
        a.triangle.linearReload = v & 0x7F;
        break;
    case 0x400A:
        a.triangle.timerPeriod = (a.triangle.timerPeriod & 0x700) | v;
        break;
    case 0x400B:
        a.triangle.reg[3] = v;
        a.triangle.timerPeriod = static_cast<int>((a.triangle.timerPeriod & 0xFF) | ((v & 7) << 8));
        a.triangle.lengthCounter = ApuDetail::kLengthTable[(v >> 3) & 0x1F];
        a.triangle.linearReloadFlag = true;
        break;
    case 0x400C:
        a.noise.reg[0] = v;
        a.noise.envelopeLoop = (v & 0x20) != 0;
        a.noise.envelopeVolume = v & 0x0F;
        break;
    case 0x400E:
        a.noise.reg[2] = v;
        a.noise.mode = (v & 0x80) != 0;
        a.noise.timerPeriod = ApuDetail::kNoisePeriodNtsc[v & 0x0F];
        break;
    case 0x400F:
        a.noise.reg[3] = v;
        a.noise.lengthCounter = ApuDetail::kLengthTable[(v >> 3) & 0x1F];
        a.noise.envelopeStart = true;
        break;
    case 0x4010:
        a.dmc.reg[0] = v;
        a.dmc.loop = (v & 0x40) != 0;
        a.dmc.irqEnable = (v & 0x80) != 0;
        a.dmc.timerPeriod = ApuDetail::kDmcRateNtsc[v & 0x0F];
        if (!a.dmc.irqEnable) a.dmc.irqPending = false;
        break;
    case 0x4011:
        a.dmc.reg[1] = v;
        a.dmc.outputLevel = v & 0x7F;
        break;
    case 0x4012:
        a.dmc.reg[2] = v;
        a.dmc.sampleAddr = 0xC000 + v * 64;
        break;
    case 0x4013:
        a.dmc.reg[3] = v;
        a.dmc.sampleLen = v * 16 + 1;
        break;
    case 0x4015:
        a.pulse[0].enabled = (v & 0x01) != 0;
        a.pulse[1].enabled = (v & 0x02) != 0;
        a.triangle.enabled = (v & 0x04) != 0;
        a.noise.enabled = (v & 0x08) != 0;
        a.dmc.enabled = (v & 0x10) != 0;
        if (!a.pulse[0].enabled) a.pulse[0].lengthCounter = 0;
        if (!a.pulse[1].enabled) a.pulse[1].lengthCounter = 0;
        if (!a.triangle.enabled) a.triangle.lengthCounter = 0;
        if (!a.noise.enabled) a.noise.lengthCounter = 0;
        if (!a.dmc.enabled) a.dmc.bytesRemaining = 0;
        a.dmc.irqPending = false;
        break;
    case 0x4017:
        a.frameMode = (v & 0x80) != 0;
        a.frameIrqEnable = (v & 0x40) == 0;
        a.frameSeqStep = 0;
        a.frameSeqCycle = 0;
        if (a.frameMode) clockApuQuarter(s, true);
        break;
    default:
        break;
    }
}

inline std::uint8_t apuReadStatus(const State& s) noexcept {
    std::uint8_t st = 0;
    if (s.apu.pulse[0].lengthCounter > 0) st |= 0x01;
    if (s.apu.pulse[1].lengthCounter > 0) st |= 0x02;
    if (s.apu.triangle.lengthCounter > 0) st |= 0x04;
    if (s.apu.noise.lengthCounter > 0) st |= 0x08;
    if (s.apu.dmc.bytesRemaining > 0) st |= 0x10;
    if (s.apu.frameIrqPending) st |= 0x40;
    if (s.apu.dmc.irqPending) st |= 0x80;
    return st;
}

inline void apuClearFrameIrq(State& s) noexcept {
    s.apu.frameIrqPending = false;
}

inline void clockApuFrame(State& s) noexcept {
    auto& a = s.apu;
    ++a.frameSeqStep;
    const int mode = a.frameMode ? 1 : 0;
    static const int kSteps[2][6] = {
        {1, 2, 0, 1, 2, 0},
        {1, 2, 0, 0, 1, 2},
    };
    const int action = kSteps[mode][a.frameSeqStep % 6];
    if (action == 1) clockApuQuarter(s, false);
    else if (action == 2) {
        clockApuQuarter(s, false);
        clockApuHalf(s);
    }
    if (!a.frameMode && a.frameSeqStep == 3 && a.frameIrqEnable)
        a.frameIrqPending = true;
}

inline void clockApuChannelTimers(State& s) noexcept {
    auto& a = s.apu;
    for (int i = 0; i < 2; ++i) {
        auto& ch = a.pulse[i];
        if (ch.timer == 0) {
            ch.timer = ch.timerPeriod;
            ch.dutyPos = (ch.dutyPos + 1) & 7;
        } else
            --ch.timer;
    }
    auto& tri = a.triangle;
    if (tri.timer == 0) {
        tri.timer = tri.timerPeriod;
        if (tri.lengthCounter > 0 && tri.linearCounter > 0)
            tri.seqPos = (tri.seqPos + 1) & 31;
    } else
        --tri.timer;
    auto& noise = a.noise;
    if (noise.timer == 0) {
        noise.timer = noise.timerPeriod;
        const int feedback = (noise.lfsr & 1) ^ ((noise.lfsr >> (noise.mode ? 6 : 1)) & 1);
        noise.lfsr = static_cast<std::uint16_t>((noise.lfsr >> 1) | (feedback << 14));
    } else
        --noise.timer;
}

inline float apuChannelLevels(const State& s, int& p1, int& p2, int& tri, int& noise, int& dmc) noexcept {
    const bool modern = s.apu.style == AudioStyle::Modern;
    p1 = p2 = tri = noise = dmc = 0;
    auto& a = s.apu;
    for (int i = 0; i < 2; ++i) {
        const auto& ch = a.pulse[i];
        if (ch.lengthCounter == 0 || ch.timerPeriod < 8 || ch.sweepMute) continue;
        const int vol = (ch.reg[0] & 0x10) ? ch.envelopeDecay : (ch.reg[0] & 0x0F);
        const int duty = (ch.reg[0] >> 6) & 3;
        const int out = ApuDetail::kDutyTable[duty][ch.dutyPos & 7] ? vol : 0;
        if (i == 0) p1 = out; else p2 = out;
        if (modern) {
            const float smp = ApuDetail::pulseSample(const_cast<PulseChan&>(ch), true);
            if (i == 0) p1 = static_cast<int>(smp * 15.f); else p2 = static_cast<int>(smp * 15.f);
        }
    }
    if (a.triangle.lengthCounter > 0 && a.triangle.linearCounter > 0 && a.triangle.timerPeriod >= 2)
        tri = ApuDetail::kTriangleSeq[a.triangle.seqPos & 31];
    if (a.noise.lengthCounter > 0) {
        const int vol = (a.noise.reg[0] & 0x10) ? a.noise.envelopeDecay : (a.noise.reg[0] & 0x0F);
        noise = (a.noise.lfsr & 1) ? 0 : vol;
    }
    dmc = a.dmc.outputLevel;
    if (modern)
        return (ApuDetail::pulseSample(const_cast<PulseChan&>(a.pulse[0]), true)
            + ApuDetail::pulseSample(const_cast<PulseChan&>(a.pulse[1]), true)
            + static_cast<float>(tri) / 15.f * 0.6f
            + static_cast<float>(noise) / 15.f * 0.5f
            + static_cast<float>(dmc) / 127.f * 0.4f) * 0.25f - 0.1f;
    return ApuDetail::mixAuthentic(p1, p2, tri, noise, dmc);
}

inline float apuMixSample(State& s) noexcept {
    int p1, p2, tri, noise, dmc;
    float out = apuChannelLevels(s, p1, p2, tri, noise, dmc);
    if (s.apu.style == AudioStyle::Modern) {
        constexpr float kLp = 0.12f;
        s.apu.modernLp += kLp * (out - s.apu.modernLp);
        out = s.apu.modernLp;
        out = std::tanh(out * 1.4f);
    }
    return std::clamp(out, -1.f, 1.f);
}

inline void clockApu(State& s, int cpuCycles) noexcept {
    auto& a = s.apu;
    a.frameSeqCycle += cpuCycles;
    while (a.frameSeqCycle >= 7457) {
        a.frameSeqCycle -= 7457;
        clockApuFrame(s);
    }
    for (int i = 0; i < cpuCycles; ++i) {
        clockApuChannelTimers(s);
        ++a.sampleClock;
        const int hz = ApuDetail::cpuHz(s.region);
        const int threshold = std::max(1, hz / a.sampleRate);
        if (static_cast<int>(a.sampleClock) >= threshold) {
            a.sampleClock = 0;
            a.sampleAccum += apuMixSample(s);
            ++a.sampleAccumCycles;
        }
    }
    if (a.frameIrqPending) s.irqLine = true;
    if (a.dmc.irqPending) s.irqLine = true;
}

inline int apuDrainSamples(State& s, float* out, int maxSamples) noexcept {
    if (!out || maxSamples <= 0) return 0;
    auto& a = s.apu;
    int n = 0;
    if (a.sampleAccumCycles > 0) {
        const float smp = a.sampleAccum / static_cast<float>(a.sampleAccumCycles);
        out[n++] = smp;
        a.sampleAccum = 0.f;
        a.sampleAccumCycles = 0;
    }
    while (n < maxSamples) {
        out[n++] = apuMixSample(s);
    }
    return n;
}

} // namespace FieldChips::Nes