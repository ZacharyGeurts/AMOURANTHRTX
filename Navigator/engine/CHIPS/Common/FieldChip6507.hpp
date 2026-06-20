#pragma once

// MOS 6507 — 6502 die with 13-bit address bus (Atari 2600, some arcade boards).

#include "FieldChip6502.hpp"

namespace FieldChips {

inline constexpr Cpu6502Config k6507Config{
    .decimalMode = false,
    .addrMask = 0x1FFF,
    .irqVector = 0xFFFE,
    .nmiVector = 0xFFFA,
    .resetVector = 0xFFFC,
};

} // namespace FieldChips