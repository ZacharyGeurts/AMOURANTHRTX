#pragma once

#include "CHIPS/Xbox360/FieldXbox360.hpp"
#include "FieldVga.hpp"

namespace FieldXbox360 {

inline FieldChips::Xbox360::State chip{};
inline bool active = false;
inline std::uint8_t fb[320 * 200]{};

inline void open() noexcept { active = true; }
inline void close(std::uint8_t*) noexcept { active = false; }

inline void runFrame(std::uint8_t* guestRam) noexcept {
    if (!active) return;
    FieldChips::Xbox360::runFrame(chip);
    if (!guestRam) return;
    FieldVga::setMode(FieldVga::MODE_VGA_13, guestRam);
    const int sw = 320, sh = 200;
    for (int y = 0; y < sh; ++y)
        for (int x = 0; x < sw; ++x) {
            const int sx = x * chip.gpu.fbW / sw;
            const int sy = y * chip.gpu.fbH / sh;
            const std::uint32_t px = chip.gpu.fb[static_cast<std::size_t>(sy * chip.gpu.fbW + sx)];
            guestRam[FieldVga::VGA_FB + static_cast<std::size_t>(y * 320 + x)] =
                static_cast<std::uint8_t>((px >> 16) & 0xFFu);
        }
}

inline void tick(std::uint8_t* ram, const bool*) noexcept { runFrame(ram); }

} // namespace FieldXbox360