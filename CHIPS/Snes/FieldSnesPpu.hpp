#pragma once

#include "FieldSnesTypes.hpp"

namespace FieldChips::Snes {

inline void installSnesVgaPalette(std::uint8_t* guestRam) noexcept {
    if (!guestRam) return;
    constexpr std::uint32_t kPal = 0x000A0000u;
    static const std::uint8_t pal[256 * 3] = {
        0x00,0x00,0x00, 0x10,0x10,0x18, 0x20,0x20,0x30, 0x30,0x30,0x48,
        0x40,0x40,0x60, 0x50,0x50,0x78, 0x60,0x60,0x90, 0x70,0x70,0xA8,
        0x80,0x40,0x20, 0xA0,0x50,0x28, 0xC0,0x60,0x30, 0xE0,0x70,0x38,
        0x20,0x60,0xA0, 0x28,0x78,0xC0, 0x30,0x90,0xE0, 0x38,0xA8,0xFF,
        0x18,0x78,0x28, 0x20,0x98,0x30, 0x28,0xB8,0x38, 0x30,0xD8,0x40,
        0x90,0x90,0x20, 0xB0,0xB0,0x28, 0xD0,0xD0,0x30, 0xF0,0xF0,0x38,
        0x80,0x20,0x60, 0xA0,0x28,0x78, 0xC0,0x30,0x90, 0xE0,0x38,0xA8,
        0x60,0x60,0x60, 0x78,0x78,0x78, 0x90,0x90,0x90, 0xA8,0xA8,0xA8,
        0xC0,0xC0,0xC0, 0xD8,0xD8,0xD8, 0xF0,0xF0,0xF0, 0xFF,0xFF,0xFF,
    };
    for (int i = 0; i < 256 * 3; ++i)
        guestRam[kPal + static_cast<std::size_t>(i)] = pal[i];
}

inline void renderGsuScreen(const State& s, std::uint8_t* fb, int w, int h) noexcept {
    if (!fb || !s.gsu.present) return;
    const std::uint32_t scbr = static_cast<std::uint32_t>(s.gsu.scbr) << 10;
    for (int y = 0; y < h; ++y) {
        const int sy = y * 224 / h;
        for (int x = 0; x < w; ++x) {
            const int sx = x * 256 / w;
            const std::uint32_t off = scbr + (static_cast<std::uint32_t>(sy) << 8) + static_cast<std::uint32_t>(sx);
            std::uint8_t col = 0;
            if (off + 1u < sizeof s.gsu.ram) {
                const std::uint16_t px = static_cast<std::uint16_t>(
                    s.gsu.ram[off] | (static_cast<std::uint16_t>(s.gsu.ram[off + 1u]) << 8));
                col = static_cast<std::uint8_t>((px & 0xFFu) ? (px & 0xFFu) : ((px >> 8) & 0xFFu));
            }
            if (col == 0) col = static_cast<std::uint8_t>(8 + ((sx ^ sy) & 7));
            fb[y * w + x] = col;
        }
    }
}

inline void renderChrPreview(const State& s, std::uint8_t* fb, int w, int h) noexcept {
    if (!fb) return;
    const std::uint8_t* rom = s.cart.rom.data();
    const std::size_t romSize = s.cart.rom.size();
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            std::uint8_t col = 0x08;
            if (!romSize) {
                fb[y * w + x] = col;
                continue;
            }
            const std::size_t tile = static_cast<std::size_t>((y >> 3) * 32 + (x >> 3));
            const std::size_t pat = (tile * 32 + (y & 7) * 2) % romSize;
            const std::uint8_t bits = rom[pat];
            if ((bits >> (7 - (x & 7))) & 1) col = 0x1C;
            fb[y * w + x] = col;
        }
    }
}

} // namespace FieldChips::Snes