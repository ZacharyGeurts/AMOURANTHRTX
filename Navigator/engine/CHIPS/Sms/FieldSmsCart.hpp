#pragma once

#include "FieldSmsTypes.hpp"

namespace FieldChips::Sms {

inline std::uint8_t readCart(const State& s, std::uint16_t addr) noexcept {
    if (s.cart.rom.empty()) return 0xFF;
    const std::size_t off = addr;
    if (off < s.cart.rom.size()) return s.cart.rom[off];
    return 0xFF;
}

inline bool loadRom(State& s, const std::uint8_t* data, std::size_t size) {
    if (!data || size < 256) return false;
    s.cart = Cart{};
    s.cart.rom.assign(data, data + size);
    s.cart.size = static_cast<int>(size);
    resetState(s);
    s.z80.pc = 0;
    return true;
}

} // namespace FieldChips::Sms