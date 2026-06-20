#pragma once

// Sharp SM83 (LR35902) — Game Boy CPU scaffold.

#include "FieldChipTypes.hpp"

namespace FieldChips {

struct CpuSm83 {
    std::uint16_t pc = 0;
    std::uint8_t a = 0;
    std::uint8_t b = 0;
    std::uint8_t c = 0;
    std::uint8_t d = 0;
    std::uint8_t e = 0;
    std::uint8_t h = 0;
    std::uint8_t l = 0;
    std::uint8_t sp = 0xFD;
    std::uint8_t flags = 0x80;
    bool halted = false;
    bool ime = false;
    int cycles = 0;
    int totalCycles = 0;
};

inline void resetSm83(CpuSm83& c) noexcept { c = CpuSm83{}; c.flags = 0x80; }

} // namespace FieldChips