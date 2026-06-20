#pragma once

#include "FieldGenesisTypes.hpp"

namespace FieldChips::Genesis {

inline std::uint8_t readCart8(const State& s, std::uint32_t addr) noexcept {
    if (s.cart.rom.empty()) return 0xFF;
    const std::size_t off = addr & 0x7FFFF;
    if (off < s.cart.rom.size()) return s.cart.rom[off];
    return 0xFF;
}

inline bool loadRom(State& s, const std::uint8_t* data, std::size_t size) {
    if (!data || size < 512) return false;
    s.cart = Cart{};
    s.cart.rom.assign(data, data + size);
    s.cart.size = static_cast<int>(size);
    resetState(s);
    s.m68k.pc = 0;
    s.m68k.a[7] = 0x00FF0000;
    return true;
}

} // namespace FieldChips::Genesis