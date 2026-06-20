#pragma once

// TIA television interface — playfield, sprites, audio.

#include "FieldA2600Types.hpp"

#include <algorithm>
#include <cmath>

namespace FieldChips::Atari2600 {

namespace TiaDetail {

inline constexpr std::uint8_t kNtscPalette[16] = {
    0x00, 0x40, 0x50, 0x60, 0x70, 0x80, 0x90, 0xA0,
    0xB0, 0xC0, 0xD0, 0xE0, 0xF0, 0x02, 0x14, 0x25,
};

inline std::uint8_t colToVga(std::uint8_t col) noexcept {
    return kNtscPalette[col & 0x0F];
}

inline float tiaTone(const Tia& t, int ch, bool modern) noexcept {
    const std::uint8_t audv = (ch == 0) ? t.audv0 : t.audv1;
    const std::uint8_t audf = (ch == 0) ? t.audf0 : t.audf1;
    if (audv == 0) return 0.f;
    const int div = (audf & 0x1F) + 1;
    const float phase = static_cast<float>((t.frame * 228 + t.scanline) % (div * 16))
        / static_cast<float>(div * 16);
    float s = (phase < 0.5f) ? 1.f : -1.f;
    if (modern) s = std::tanh(s * 0.8f);
    return s * static_cast<float>(audv) / 15.f * 0.2f;
}

} // namespace TiaDetail

inline void tiaWrite(State& s, std::uint16_t addr, std::uint8_t v) noexcept {
    addr = static_cast<std::uint16_t>(addr & 0x3F);
    auto& t = s.tia;
    switch (addr) {
    case 0x00: t.vsync = v; break;
    case 0x01: t.vblank = v; break;
    case 0x02: t.wsync = v; break;
    case 0x04: t.coluP0 = v; break;
    case 0x05: t.coluP1 = v; break;
    case 0x06: t.coluPf = v; break;
    case 0x07: t.coluBk = v; break;
    case 0x0A:
        t.cntlpf = v;
        t.pfReflect = v & 1;
        t.pfScore = (v >> 1) & 1;
        t.pfPriority = (v >> 2) & 1;
        break;
    case 0x0D: t.pf0 = v; break;
    case 0x0E: t.pf1 = v; break;
    case 0x0F: t.pf2 = v; break;
    case 0x15: t.audc0 = v; break;
    case 0x16: t.audc1 = v; break;
    case 0x17: t.audf0 = v; break;
    case 0x18: t.audf1 = v; break;
    case 0x19: t.audv0 = v; break;
    case 0x1A: t.audv1 = v; break;
    case 0x1B: t.resp0 = v; t.p0x = 0; break;
    case 0x1C: t.resp1 = v; t.p1x = 0; break;
    case 0x1D: t.resm0 = v; t.m0x = 0; t.m0En = 1; break;
    case 0x1E: t.resm1 = v; t.m1x = 0; t.m1En = 1; break;
    case 0x1F: t.resbl = v; t.blx = 0; t.blEn = 1; break;
    default: break;
    }
}

inline std::uint8_t tiaRead(State&, std::uint16_t addr) noexcept {
    addr = static_cast<std::uint16_t>(addr & 0x0F);
    if (addr >= 0x0C && addr <= 0x0F) return 0;
    return 0;
}

inline bool pfPixel(const State& s, int x) noexcept {
    int bit = 0;
    if (x < 4) bit = (s.tia.pf0 >> (3 - x)) & 1;
    else if (x < 12) bit = (s.tia.pf1 >> (11 - x)) & 1;
    else if (x < 20) bit = (s.tia.pf2 >> (19 - x)) & 1;
    else if (s.tia.pfReflect) {
        if (x < 24) bit = (s.tia.pf0 >> (23 - x)) & 1;
        else if (x < 32) bit = (s.tia.pf1 >> (31 - x)) & 1;
        else bit = (s.tia.pf2 >> (39 - x)) & 1;
    }
    return bit != 0;
}

inline void tickTia(State& s, int cycles) noexcept {
    auto& t = s.tia;
    for (int i = 0; i < cycles; ++i) {
        ++t.cycle;
        if (t.cycle >= 228) {
            t.cycle = 0;
            ++t.scanline;
            if (t.scanline >= 262) {
                t.scanline = 0;
                ++t.frame;
            }
        }
        if ((t.cycle & 3) == 0) {
            if (t.p0x < 160) ++t.p0x;
            if (t.p1x < 160) ++t.p1x;
            if (t.m0En && t.m0x < 160) ++t.m0x;
            if (t.m1En && t.m1x < 160) ++t.m1x;
            if (t.blEn && t.blx < 160) ++t.blx;
        }
    }
}

inline void renderFrame(State& s, std::uint8_t* fb, int w, int h) noexcept {
    if (!fb) return;
    const std::uint8_t bg = TiaDetail::colToVga(s.tia.coluBk);
    const std::uint8_t pf = TiaDetail::colToVga(s.tia.coluPf);
    for (int y = 0; y < h; ++y) {
        const int sl = y * 192 / h;
        for (int x = 0; x < w; ++x) {
            const int px = x * 160 / w;
            std::uint8_t col = bg;
            if (sl >= 16 && sl < 208 && pfPixel(s, px)) col = pf;
            fb[y * w + x] = col;
        }
    }
}

inline float tiaMixSample(const State& s) noexcept {
    const bool modern = s.tia.audioStyle != AudioStyle::Classic;
    float out = TiaDetail::tiaTone(s.tia, 0, modern) + TiaDetail::tiaTone(s.tia, 1, modern);
    if (modern) {
        auto& t = const_cast<Tia&>(s.tia);
        constexpr float kLp = 0.1f;
        t.modernLp += kLp * (out - t.modernLp);
        out = std::tanh(t.modernLp * 1.2f);
    }
    return std::clamp(out, -1.f, 1.f);
}

inline void clockTiaAudio(State& s, int cpuCycles) noexcept {
    auto& t = s.tia;
    t.sampleClock += static_cast<std::uint32_t>(cpuCycles);
    const int threshold = std::max(1, 1193182 / t.sampleRate);
    while (static_cast<int>(t.sampleClock) >= threshold) {
        t.sampleClock -= static_cast<std::uint32_t>(threshold);
        t.sampleAccum += tiaMixSample(s);
        ++t.sampleAccumCycles;
    }
}

inline int renderAudioFrame(State& s, float* out, int maxSamples) noexcept {
    if (!out || maxSamples <= 0) return 0;
    auto& t = s.tia;
    int n = 0;
    if (t.sampleAccumCycles > 0) {
        out[n++] = t.sampleAccum / static_cast<float>(t.sampleAccumCycles);
        t.sampleAccum = 0.f;
        t.sampleAccumCycles = 0;
    }
    while (n < maxSamples) out[n++] = tiaMixSample(s);
    return n;
}

} // namespace FieldChips::Atari2600