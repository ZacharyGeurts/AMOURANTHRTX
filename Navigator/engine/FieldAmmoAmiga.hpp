#pragma once

#include "CHIPS/Amiga/FieldAmiga.hpp"
#include "FieldVga.hpp"

namespace FieldAmiga {

inline FieldChips::Amiga::State chip{};
inline bool active = false;
inline bool loveMode = false;

inline void open(bool love = true) noexcept {
    active = true;
    loveMode = love;
}

inline void close(std::uint8_t*) noexcept { active = false; }

inline void runFrame(std::uint8_t* guestRam) noexcept {
    if (!active) return;
    FieldChips::Amiga::runFrame(chip);
    if (!guestRam) return;
    FieldVga::setMode(FieldVga::MODE_VGA_13, guestRam);
    const int sw = 320, sh = 200;
    for (int y = 0; y < sh; ++y)
        for (int x = 0; x < sw; ++x) {
            const int sx = x * chip.denise.fbW / sw;
            const int sy = y * chip.denise.fbH / sh;
            const std::uint32_t px = chip.denise.fb[static_cast<std::size_t>(sy * chip.denise.fbW + sx)];
            guestRam[FieldVga::VGA_FB + static_cast<std::size_t>(y * 320 + x)] =
                static_cast<std::uint8_t>((px >> 8) & 0xFFu);
        }
}

inline void tick(std::uint8_t* ram, const bool*) noexcept { runFrame(ram); }

} // namespace FieldAmiga