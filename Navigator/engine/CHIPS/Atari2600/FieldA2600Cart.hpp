#pragma once

#include "FieldA2600Types.hpp"

namespace FieldChips::Atari2600 {

inline std::uint8_t readCart(const State& s, std::uint16_t addr) noexcept {
    if (s.cart.rom.empty()) return 0xFF;
    const std::uint16_t off = static_cast<std::uint16_t>(addr - 0x1000);
    if (s.cart.size <= 4096)
        return s.cart.rom[static_cast<std::size_t>(off) % s.cart.rom.size()];
    if (off < s.cart.rom.size()) return s.cart.rom[off];
    return 0xFF;
}

inline bool loadRom(State& s, const std::uint8_t* data, std::size_t size) {
    if (!data || size < 256) return false;
    s.cart = Cart{};
    s.cart.rom.assign(data, data + size);
    s.cart.size = static_cast<int>(size);
    resetState(s);
    s.cpu.pc = static_cast<std::uint16_t>(
        readCart(s, 0x1FFC) | (static_cast<std::uint16_t>(readCart(s, 0x1FFD)) << 8));
    return true;
}

} // namespace FieldChips::Atari2600