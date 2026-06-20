#pragma once

// WDC 65816 — SNES main CPU scaffold.

#include "FieldChipTypes.hpp"

namespace FieldChips {

struct CpuWdc65816 {
    std::uint16_t pc = 0;
    std::uint8_t a = 0;
    std::uint8_t x = 0;
    std::uint8_t y = 0;
    std::uint8_t sp = 0xFD;
    std::uint8_t status = 0x24;
    std::uint8_t db = 0;
    std::uint16_t pb = 0;
    std::uint16_t d = 0;
    bool emulation = true;
    int cycles = 0;
    int totalCycles = 0;
};

inline void resetWdc65816(CpuWdc65816& c) noexcept { c = CpuWdc65816{}; }

} // namespace FieldChips