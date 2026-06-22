#pragma once

#include "FieldPs1Types.hpp"

namespace FieldChips::Ps1 {

inline std::uint8_t busRead8(const State& s, std::uint32_t addr) noexcept {
    if (addr >= 0x1F000000u && addr < 0x1F000400u)
        return static_cast<std::uint8_t>((addr >> 2) & 0xFFu);
    if (addr >= 0x80000000u)
        return s.ram[addr & (sizeof s.ram - 1u)];
    return 0;
}

inline void busWrite8(State& s, std::uint32_t addr, std::uint8_t v) noexcept {
    if (addr >= 0x80000000u)
        s.ram[addr & (sizeof s.ram - 1u)] = v;
}

} // namespace FieldChips::Ps1