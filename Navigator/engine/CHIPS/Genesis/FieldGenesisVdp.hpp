#pragma once

#include "FieldGenesisTypes.hpp"

namespace FieldChips::Genesis {

inline void vdpWriteData(State& s, std::uint8_t v) noexcept {
    if (s.vdp.addr < sizeof s.vdp.vram)
        s.vdp.vram[s.vdp.addr++] = v;
}

inline void vdpWriteCtrl(State& s, std::uint16_t v) noexcept {
    if (!s.vdp.code) {
        s.vdp.addr = static_cast<std::uint16_t>((s.vdp.addr & 0xFC00) | (v & 0x3FFF));
        s.vdp.code = true;
    } else {
        s.vdp.ctrl = v;
        if (v & 0x8000) {
            const int reg = (v >> 8) & 0x1F;
            (void)reg;
        }
        s.vdp.code = false;
    }
}

inline std::uint8_t vdpReadData(State& s) noexcept {
    const std::uint8_t v = (s.vdp.addr < sizeof s.vdp.vram) ? s.vdp.vram[s.vdp.addr] : 0;
    ++s.vdp.addr;
    return v;
}

inline void tickVdp(State& s, int cpuCycles) noexcept {
    auto& v = s.vdp;
    v.cycle += cpuCycles;
    while (v.cycle >= 488) {
        v.cycle -= 488;
        if (++v.scanline >= 262) {
            v.scanline = 0;
            ++v.frame;
            v.vblank = true;
            s.m68kIrq = true;
        }
        if (v.scanline == 224)
            v.vblank = true;
        if (v.scanline < 224)
            v.vblank = false;
    }
}

inline void renderFrame(State& s, std::uint8_t* fb, int w, int h) noexcept {
    if (!fb) return;
    const std::uint8_t bg = 0x20;
    for (int y = 0; y < h; ++y) {
        const int sl = y * 224 / h;
        for (int x = 0; x < w; ++x) {
            const int px = x * 320 / w;
            std::uint8_t col = bg;
            if (sl < 224) {
                const int tileX = px >> 3;
                const int tileY = sl >> 3;
                const int nameAddr = 0xC000 + tileY * 64 + tileX * 2;
                if (nameAddr + 1 < static_cast<int>(sizeof s.vdp.vram)) {
                    const int tile = s.vdp.vram[nameAddr];
                    const int line = sl & 7;
                    const int patAddr = tile * 32 + line * 4;
                    if (patAddr < static_cast<int>(sizeof s.vdp.vram)) {
                        const std::uint8_t bits = s.vdp.vram[patAddr];
                        const int bit = 7 - (px & 7);
                        if ((bits >> bit) & 1) col = 0x60;
                    }
                }
            }
            fb[y * w + x] = col;
        }
    }
}

} // namespace FieldChips::Genesis