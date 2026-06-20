#pragma once

#include "FieldSmsTypes.hpp"

namespace FieldChips::Sms {

inline void vdpWriteData(State& s, std::uint8_t v) noexcept {
    if (s.vdp.addr < sizeof s.vdp.vram)
        s.vdp.vram[s.vdp.addr++] = v;
}

inline void vdpWriteCtrl(State& s, std::uint8_t v) noexcept {
    if (!s.vdp.code) {
        s.vdp.addr = static_cast<std::uint16_t>((s.vdp.addr & 0x3F00) | v);
        s.vdp.code = true;
    } else {
        s.vdp.addr = static_cast<std::uint16_t>(((v & 0x3F) << 8) | (s.vdp.addr & 0xFF));
        if (v & 0x80) {
            const int reg = v & 0x0F;
            if (reg < 16) s.vdp.reg[reg] = static_cast<std::uint8_t>(s.vdp.addr & 0xFF);
        }
        s.vdp.code = false;
    }
}

inline std::uint8_t vdpReadData(State& s) noexcept {
    const std::uint8_t v = (s.vdp.addr < sizeof s.vdp.vram) ? s.vdp.vram[s.vdp.addr] : 0;
    ++s.vdp.addr;
    return v;
}

inline std::uint8_t vdpReadStatus(State& s) noexcept {
    s.vdp.code = false;
    const std::uint8_t st = s.vdp.status;
    s.vdp.status &= ~0xE0;
    return st;
}

inline void tickVdp(State& s, int cpuCycles) noexcept {
    auto& v = s.vdp;
    v.cycle += cpuCycles;
    while (v.cycle >= 228) {
        v.cycle -= 228;
        if (++v.scanline >= 262) {
            v.scanline = 0;
            ++v.frame;
            v.status |= 0x80;
            if (v.reg[1] & 0x20) s.irqLine = true;
        }
        if (v.scanline < 192 && (v.reg[1] & 0x20))
            v.status |= 0x40;
    }
}

inline void renderFrame(State& s, std::uint8_t* fb, int w, int h) noexcept {
    if (!fb) return;
    const int bgIdx = s.vdp.reg[7] & 0x0F;
    const std::uint8_t bg = static_cast<std::uint8_t>((bgIdx & 3) * 64 + 32);
    for (int y = 0; y < h; ++y) {
        const int sl = y * 192 / h;
        for (int x = 0; x < w; ++x) {
            const int px = x * 256 / w;
            std::uint8_t col = bg;
            if (sl < 192) {
                const int tileX = px >> 3;
                const int tileY = sl >> 3;
                const int nameBase = (s.vdp.reg[2] & 0x0E) << 10;
                const int nameAddr = nameBase + tileY * 32 + tileX;
                if (nameAddr + 1 < static_cast<int>(sizeof s.vdp.vram)) {
                    const int tile = s.vdp.vram[nameAddr];
                    const int line = sl & 7;
                    const int patBase = (s.vdp.reg[4] & 7) << 11;
                    const int patAddr = patBase + tile * 8 + line;
                    if (patAddr < static_cast<int>(sizeof s.vdp.vram)) {
                        const std::uint8_t bits = s.vdp.vram[patAddr];
                        const int bit = 7 - (px & 7);
                        if ((bits >> bit) & 1)
                            col = static_cast<std::uint8_t>(((s.vdp.reg[7] >> 4) & 0x0F) * 16 + 16);
                    }
                }
            }
            fb[y * w + x] = col;
        }
    }
}

} // namespace FieldChips::Sms