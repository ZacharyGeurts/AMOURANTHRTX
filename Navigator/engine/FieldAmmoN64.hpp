#pragma once

#include "CHIPS/N64/FieldN64.hpp"
#include "FieldVga.hpp"

namespace FieldN64 {

inline FieldChips::N64::State chip{};
inline bool active = false;

inline void open() noexcept { active = true; }
inline void close(std::uint8_t*) noexcept { active = false; }

inline void runFrame(std::uint8_t* guestRam) noexcept {
    if (!active) return;
    FieldChips::N64::runFrame(chip);
    if (!guestRam) return;
    FieldVga::setMode(FieldVga::MODE_VGA_13, guestRam);
    const int sw = 320, sh = 200;
    for (int y = 0; y < sh; ++y)
        for (int x = 0; x < sw; ++x) {
            const int sx = x * chip.rsp.fbW / sw;
            const int sy = y * chip.rsp.fbH / sh;
            const std::uint32_t px = chip.rsp.fb[static_cast<std::size_t>(sy * chip.rsp.fbW + sx)];
            guestRam[FieldVga::VGA_FB + static_cast<std::size_t>(y * 320 + x)] =
                static_cast<std::uint8_t>((px >> 16) & 0xFFu);
        }
}

inline void tick(std::uint8_t* ram, const bool*) noexcept { runFrame(ram); }

} // namespace FieldN64