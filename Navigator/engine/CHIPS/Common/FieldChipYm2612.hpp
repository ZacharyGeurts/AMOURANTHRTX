#pragma once

// Yamaha YM2612 FM — Genesis. Classic + Modern output modes.

#include "FieldChipAudio.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace FieldChips {

struct Ym2612Chan {
    std::uint8_t reg[4]{};
    int phase = 0;
    int phaseInc = 0;
    int envelope = 0;
    int envRate = 0;
    bool keyOn = false;
    float blipPhase = 0.f;
};

struct Ym2612 {
    Ym2612Chan chan[6]{};
    std::uint8_t regSel = 0;
    AudioStyle style = AudioStyle::Modern;
    int sampleRate = 44100;
    float sampleAccum = 0.f;
    int sampleAccumCycles = 0;
    std::uint32_t sampleClock = 0;
    float modernLp = 0.f;
    int dacEnable = 0;
    int dacOut = 0;
};

namespace Ym2612Detail {

inline int phaseIncFromReg(const Ym2612Chan& ch) noexcept {
    const int fnum = static_cast<int>(ch.reg[0]) | ((ch.reg[1] & 7) << 8);
    const int block = (ch.reg[1] >> 3) & 7;
    return std::max(1, (fnum << block) >> 2);
}

inline float fmClassic(const Ym2612Chan& ch) noexcept {
    if (!ch.keyOn || ch.envelope <= 0) return 0.f;
    const float t = static_cast<float>(ch.phase) / 65536.f;
    const float mod = std::sin(t * 6.2831853f);
    const float amp = static_cast<float>(ch.envelope) / 127.f * 0.25f;
    return mod * amp;
}

inline float fmModern(Ym2612Chan& ch) noexcept {
    if (!ch.keyOn || ch.envelope <= 0) return 0.f;
    const float t = static_cast<float>(ch.phase) / 65536.f;
    float mod = std::sin(t * 6.2831853f);
    const float amp = static_cast<float>(ch.envelope) / 127.f * 0.25f;
    const float dt = 1.f / static_cast<float>(std::max(1, ch.phaseInc));
    mod += AudioDetail::polyBlep(ch.blipPhase, dt);
    ch.blipPhase += dt;
    if (ch.blipPhase >= 1.f) ch.blipPhase -= 1.f;
    return mod * amp;
}

} // namespace Ym2612Detail

inline void ym2612WriteAddr(Ym2612& fm, std::uint8_t part, std::uint8_t reg) noexcept {
    (void)part;
    fm.regSel = reg;
}

inline void ym2612WriteData(Ym2612& fm, std::uint8_t part, std::uint8_t v) noexcept {
    (void)part;
    if (fm.regSel >= 0xA0 && fm.regSel <= 0xA6) {
        const int ch = fm.regSel - 0xA0;
        fm.chan[ch].reg[0] = v;
        fm.chan[ch].phaseInc = Ym2612Detail::phaseIncFromReg(fm.chan[ch]);
    } else if (fm.regSel >= 0xA4 && fm.regSel <= 0xAA) {
        const int ch = fm.regSel - 0xA4;
        fm.chan[ch].reg[1] = v;
        fm.chan[ch].phaseInc = Ym2612Detail::phaseIncFromReg(fm.chan[ch]);
    } else if (fm.regSel >= 0x28 && fm.regSel <= 0x28) {
        const int ch = v & 7;
        if (ch < 6) {
            fm.chan[ch].keyOn = (v & 0xF0) != 0;
            if (fm.chan[ch].keyOn) fm.chan[ch].envelope = 127;
        }
    } else if (fm.regSel == 0x2A) {
        fm.dacOut = static_cast<int>(static_cast<std::int8_t>(v));
    } else if (fm.regSel == 0x2B) {
        fm.dacEnable = v & 0x80;
    }
}

inline float ym2612MixSample(Ym2612& fm) noexcept {
    float out = 0.f;
    for (int i = 0; i < 6; ++i) {
        auto& ch = fm.chan[i];
        out += (fm.style == AudioStyle::Classic)
            ? Ym2612Detail::fmClassic(ch)
            : Ym2612Detail::fmModern(ch);
    }
    if (fm.dacEnable)
        out += static_cast<float>(fm.dacOut) / 127.f * 0.3f;
    return AudioDetail::finalizeSample(out, fm.style, fm.modernLp, 0.14f, 1.3f);
}

inline void ym2612Clock(Ym2612& fm, int cpuCycles, int chipHz = 7670454) noexcept {
    for (int c = 0; c < cpuCycles; ++c) {
        for (int i = 0; i < 6; ++i) {
            auto& ch = fm.chan[i];
            ch.phase = (ch.phase + ch.phaseInc) & 0xFFFF;
            if (ch.keyOn && ch.envelope > 0 && (c & 63) == 0)
                ch.envelope = std::max(0, ch.envelope - 1);
        }
    }
    fm.sampleClock += static_cast<std::uint32_t>(cpuCycles);
    const int threshold = std::max(1, chipHz / fm.sampleRate);
    while (static_cast<int>(fm.sampleClock) >= threshold) {
        fm.sampleClock -= static_cast<std::uint32_t>(threshold);
        fm.sampleAccum += ym2612MixSample(fm);
        ++fm.sampleAccumCycles;
    }
}

inline int ym2612DrainSamples(Ym2612& fm, float* out, int maxSamples) noexcept {
    if (!out || maxSamples <= 0) return 0;
    int n = 0;
    if (fm.sampleAccumCycles > 0) {
        out[n++] = fm.sampleAccum / static_cast<float>(fm.sampleAccumCycles);
        fm.sampleAccum = 0.f;
        fm.sampleAccumCycles = 0;
    }
    while (n < maxSamples)
        out[n++] = ym2612MixSample(fm);
    return n;
}

} // namespace FieldChips