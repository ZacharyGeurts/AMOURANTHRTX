#pragma once

// Ricoh 2C02 PPU — scanline timing, sprite limit, background/sprite compositor.

#include "FieldNesBus.hpp"
#include "FieldVga.hpp"

namespace FieldChips::Nes {

inline std::uint8_t nesPaletteToVga(std::uint8_t idx) noexcept {
    return static_cast<std::uint8_t>(idx & 0x3F);
}

inline std::uint8_t nesPixelColor(const State& s, std::uint8_t pix) noexcept {
    if (pix == 0) return 0;
    const std::uint8_t entry = readPpuMem(s, static_cast<std::uint16_t>(0x3F00u + pix));
    if (entry != 0) return nesPaletteToVga(entry);
    static const std::uint8_t kChrPreviewPal[4] = {0x0F, 0x30, 0x27, 0x16};
    return nesPaletteToVga(kChrPreviewPal[pix & 3u]);
}

inline void installNesVgaPalette(std::uint8_t* guestRam) noexcept {
    static const std::uint8_t kNesRgb[64][3] = {
        {84,84,84},{0,30,116},{8,16,144},{48,0,136},{68,0,100},{92,0,48},{84,4,0},{60,24,0},
        {32,42,0},{8,58,0},{0,64,0},{0,60,0},{0,50,24},{0,0,0},{0,0,0},{0,0,0},
        {188,188,188},{0,120,248},{0,88,248},{104,68,252},{216,0,204},{228,0,88},{248,56,0},{228,92,16},
        {248,124,0},{248,160,0},{248,184,0},{184,248,0},{0,184,0},{0,248,64},{0,248,212},{0,0,0},
        {248,248,248},{60,188,252},{104,136,252},{152,120,248},{248,120,248},{248,88,152},{248,120,88},{252,184,32},
        {248,216,120},{216,248,120},{184,248,184},{184,216,248},{0,0,248},{248,248,248},{0,0,0},{0,0,0},
        {0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
        {0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},{0,0,0},
    };
    for (int i = 0; i < 64; ++i) {
        FieldVga::palette[static_cast<std::size_t>(i) * 3u + 0u] =
            static_cast<std::uint8_t>(kNesRgb[i][0] >> 2);
        FieldVga::palette[static_cast<std::size_t>(i) * 3u + 1u] =
            static_cast<std::uint8_t>(kNesRgb[i][1] >> 2);
        FieldVga::palette[static_cast<std::size_t>(i) * 3u + 2u] =
            static_cast<std::uint8_t>(kNesRgb[i][2] >> 2);
    }
    FieldVga::syncPaletteToGuest(guestRam);
}

inline void tickPpu(State& s, int ppuCycles) noexcept {
    auto& p = s.ppu;
    for (int i = 0; i < ppuCycles; ++i) {
        ++p.cycle;
        if (p.cycle >= 341) {
            p.cycle = 0;
            ++p.scanline;
            if (p.scanline == 241 && (p.ctrl & 0x80))
                p.nmiPending = true;
            if (p.scanline >= 241 && p.scanline <= 260)
                p.status |= 0x80u;
            else
                p.status &= ~0x80u;
            if (p.scanline >= 262) {
                p.scanline = 0;
                p.scrollV = p.scrollT;
                p.sprite0Hit = false;
                p.spriteOverflow = false;
            }
        }
    }
}

inline std::uint8_t sampleBgPixel(const State& s, int nx, int ny, bool renderBg) noexcept {
    if (!renderBg || (s.ppu.mask & 0x08) == 0) return 0;

    const int patBase = (s.ppu.ctrl & 0x10) ? 0x1000 : 0x0000;
    const std::uint16_t v = s.ppu.scrollV;
    const int scrollX = static_cast<int>((v & 0x1Fu) * 8u + s.ppu.fineX);
    const int scrollY = static_cast<int>(((v >> 5) & 0x1Fu) * 8u + ((v >> 12) & 7u));
    int x = nx + scrollX;
    int y = ny + scrollY;
    int nt = (v >> 10) & 3;
    if (x >= 256) { x -= 256; nt ^= 1; }
    if (y >= 240) { y -= 240; nt ^= 2; }
    if (x < 0) { x += 256; nt ^= 1; }
    if (y < 0) { y += 240; nt ^= 2; }
    const int tileX = x / 8;
    const int tileY = y / 8;
    const int fineX = x % 8;
    const int fineY = y % 8;

    const std::uint16_t ntAddr = static_cast<std::uint16_t>(
        0x2000 + (nt & 3) * 0x400 + tileY * 32 + tileX);
    const std::uint8_t tile = readPpuMem(s, ntAddr);

    const std::uint32_t chrOff = static_cast<std::uint32_t>(patBase)
        + static_cast<std::uint32_t>(tile) * 16u + static_cast<std::uint32_t>(fineY);
    const std::uint8_t lo = readChrByte(s, chrOff);
    const std::uint8_t hi = readChrByte(s, chrOff + 8u);
    const int bit = 7 - fineX;
    const std::uint8_t pix = static_cast<std::uint8_t>(((hi >> bit) & 1) << 1 | ((lo >> bit) & 1));
    if (pix == 0) return 0;

    const std::uint16_t attrBase = static_cast<std::uint16_t>((ntAddr & 0x2C00u) + 0x3C0u);
    const std::uint16_t attrAddr = static_cast<std::uint16_t>(
        attrBase + static_cast<std::uint16_t>((tileY / 4) * 8 + (tileX / 4)));
    const std::uint8_t attr = readPpuMem(s, attrAddr);
    const int shift = ((tileY & 2) << 1) | (tileX & 2);
    const std::uint8_t palIdx = static_cast<std::uint8_t>((attr >> shift) & 3);
    const std::uint8_t palEntry = readPpuMem(s,
        static_cast<std::uint16_t>(0x3F00 + (static_cast<std::uint16_t>(palIdx) << 2) + pix));
    return nesPaletteToVga(palEntry);
}

inline std::uint8_t sampleSpritePixel(State& s, int nx, int ny, bool renderSprites) noexcept {
    if (!renderSprites || (s.ppu.mask & 0x10) == 0) return 0;
    const int patBase = (s.ppu.ctrl & 0x08) ? 0x1000 : 0x0000;
    const bool tall = (s.ppu.ctrl & 0x20) != 0;
    const int spriteH = tall ? 16 : 8;
    int drawn = 0;
    const int limit = s.spriteLimit ? 8 : 64;
    for (int si = 63; si >= 0; --si) {
        const int base = si * 4;
        const int y = static_cast<int>(s.ppu.oam[static_cast<std::size_t>(base)]) + 1;
        const std::uint8_t tile = s.ppu.oam[static_cast<std::size_t>(base + 1)];
        const std::uint8_t attr = s.ppu.oam[static_cast<std::size_t>(base + 2)];
        const int x = static_cast<int>(s.ppu.oam[static_cast<std::size_t>(base + 3)]);
        if (ny < y || ny >= y + spriteH || nx < x || nx >= x + 8) continue;
        if (drawn >= limit) {
            if (s.spriteLimit) return 0;
            continue;
        }
        ++drawn;
        int localY = ny - y;
        int tileNum = static_cast<int>(tile);
        if (tall) {
            tileNum = static_cast<int>(tile & 0xFEu) + (localY >= 8 ? 1 : 0);
            localY &= 7;
        }
        const int fineX = nx - x;
        const bool flipY = (attr & 0x80) != 0;
        const bool flipX = (attr & 0x40) != 0;
        const int ry = flipY ? (7 - localY) : localY;
        const int rx = flipX ? (7 - fineX) : fineX;
        const std::uint32_t chrOff = static_cast<std::uint32_t>(patBase)
            + static_cast<std::uint32_t>(tileNum) * 16u + static_cast<std::uint32_t>(ry);
        const std::uint8_t lo = readChrByte(s, chrOff);
        const std::uint8_t hi = readChrByte(s, chrOff + 8u);
        const int bit = 7 - rx;
        const std::uint8_t pix = static_cast<std::uint8_t>(((hi >> bit) & 1) << 1 | ((lo >> bit) & 1));
        if (pix == 0) continue;
        if (si == 0 && !s.ppu.sprite0Hit) s.ppu.sprite0Hit = true;
        const std::uint8_t palIdx = static_cast<std::uint8_t>(attr & 3);
        const std::uint8_t palEntry = readPpuMem(s,
            static_cast<std::uint16_t>(0x3F10 + (static_cast<std::uint16_t>(palIdx) << 2) + pix));
        return nesPaletteToVga(palEntry);
    }
    return 0;
}

inline bool nametableEmpty(const State& s) noexcept {
    for (std::size_t i = 0; i < 0x700u; ++i) {
        if (s.ppu.vram[i] != 0) return false;
    }
    return true;
}

inline void renderChrPreview(State& s, std::uint8_t* guestRam, std::uint32_t fbBase,
                             int fbW, int fbH) noexcept {
    if (!guestRam || s.cart.chr.empty()) return;
    constexpr int kCols = 16;
    constexpr int kRows = 16;
    const int tw = fbW / kCols;
    const int th = fbH / kRows;
    const std::uint8_t backdrop = nesPaletteToVga(readPpuMem(s, 0x3F00));
    for (int ty = 0; ty < kRows; ++ty) {
        for (int tx = 0; tx < kCols; ++tx) {
            const int tile = ty * kCols + tx;
            for (int py = 0; py < th; ++py) {
                const int fineY = py * 8 / th;
                const std::uint32_t chrOff = static_cast<std::uint32_t>(tile) * 16u
                    + static_cast<std::uint32_t>(fineY);
                const std::uint8_t lo = readChrByte(s, chrOff);
                const std::uint8_t hi = readChrByte(s, chrOff + 8u);
                for (int px = 0; px < tw; ++px) {
                    const int fineX = px * 8 / tw;
                    const int bit = 7 - fineX;
                    const std::uint8_t pix = static_cast<std::uint8_t>(
                        (((hi >> bit) & 1) << 1) | ((lo >> bit) & 1));
                    std::uint8_t col = pix == 0 ? backdrop : nesPixelColor(s, pix);
                    const std::uint32_t off = fbBase
                        + static_cast<std::uint32_t>((ty * th + py) * fbW + tx * tw + px);
                    if (off < 0xC0000u) guestRam[off] = col;
                }
            }
        }
    }
}

inline void renderFrame(State& s, std::uint8_t* guestRam, std::uint32_t fbBase,
                        int fbW, int fbH, bool renderBg, bool renderSprites) noexcept {
    if (!guestRam || s.cart.chr.empty()) return;
    if (nametableEmpty(s)) {
        renderChrPreview(s, guestRam, fbBase, fbW, fbH);
        return;
    }
    const std::uint8_t backdrop = nesPaletteToVga(readPpuMem(s, 0x3F00));
    for (int py = 0; py < fbH; ++py) {
        const int ny = py * 240 / fbH;
        for (int px = 0; px < fbW; ++px) {
            const int nx = px * 256 / fbW;
            std::uint8_t col = sampleBgPixel(s, nx, ny, renderBg);
            if (col == 0) col = backdrop;
            const std::uint8_t spr = sampleSpritePixel(s, nx, ny, renderSprites);
            if (spr != 0) col = spr;
            const std::uint32_t off = fbBase + static_cast<std::uint32_t>(py * fbW + px);
            if (off < 0xC0000u) guestRam[off] = col;
        }
    }
}

} // namespace FieldChips::Nes