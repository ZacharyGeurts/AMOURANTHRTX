#pragma once

#include "CHIPS/Dreamcast/FieldDreamcast.hpp"
#include "FieldVga.hpp"

namespace FieldDreamcast {

inline FieldChips::Dreamcast::State chip{};
inline bool active = false;

inline void open() noexcept { active = true; }
inline void close(std::uint8_t*) noexcept { active = false; }

inline void runFrame(std::uint8_t* guestRam) noexcept {
    if (!active) return;
    FieldChips::Dreamcast::runFrame(chip);
    if (!guestRam) return;
    FieldVga::setMode(FieldVga::MODE_VGA_13, guestRam);
    const int sw = 320, sh = 200;
    for (int y = 0; y < sh; ++y)
        for (int x = 0; x < sw; ++x) {
            const int sx = x * chip.pvr.fbW / sw;
            const int sy = y * chip.pvr.fbH / sh;
            const std::uint32_t px = chip.pvr.fb[static_cast<std::size_t>(sy * chip.pvr.fbW + sx)];
            guestRam[FieldVga::VGA_FB + static_cast<std::size_t>(y * 320 + x)] =
                static_cast<std::uint8_t>((px >> 8) & 0xFFu);
        }
}

inline void tick(std::uint8_t* ram, const bool*) noexcept { runFrame(ram); }

} // namespace FieldDreamcast