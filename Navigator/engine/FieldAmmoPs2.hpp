#pragma once

#include "CHIPS/PS2/FieldPs2.hpp"
#include "FieldVga.hpp"

namespace FieldPs2 {

inline FieldChips::Ps2::State chip{};
inline bool active = false;

inline void open() noexcept { active = true; }
inline void close(std::uint8_t*) noexcept { active = false; }

inline void runFrame(std::uint8_t* guestRam) noexcept {
    if (!active) return;
    FieldChips::Ps2::runFrame(chip);
    if (!guestRam) return;
    FieldVga::setMode(FieldVga::MODE_VGA_13, guestRam);
    const int sw = 320, sh = 200;
    for (int y = 0; y < sh; ++y)
        for (int x = 0; x < sw; ++x) {
            const int sx = x * chip.gs.fbW / sw;
            const int sy = y * chip.gs.fbH / sh;
            const std::uint32_t px = chip.gs.fb[static_cast<std::size_t>(sy * chip.gs.fbW + sx)];
            guestRam[FieldVga::VGA_FB + static_cast<std::size_t>(y * 320 + x)] =
                static_cast<std::uint8_t>(px & 0xFFu);
        }
}

inline void tick(std::uint8_t* ram, const bool*) noexcept { runFrame(ram); }

} // namespace FieldPs2