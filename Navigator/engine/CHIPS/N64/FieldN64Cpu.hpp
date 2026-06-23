#pragma once

#include "FieldN64Types.hpp"

namespace FieldChips::N64 {

inline void mipsStep(State& s, int steps = 1) noexcept {
    auto& c = s.cpu;
    for (int i = 0; i < steps; ++i) {
        const std::uint32_t off = (c.pc >= s.cart.loadAddr)
            ? ((c.pc - s.cart.loadAddr) >> 2) : 0u;
        const std::uint32_t idx = off & ((sizeof s.ram / 4u) - 1u);
        const std::uint32_t op = *reinterpret_cast<const std::uint32_t*>(s.ram + idx * 4u);
        const std::uint32_t funct = op & 0x3Fu;
        if (funct == 0x08u)
            c.pc = c.gpr[(op >> 21) & 0x1Fu];
        else {
            c.gpr[8] ^= op;
            c.pc += 4u;
        }
        c.cycles += 2u;
    }
}

} // namespace FieldChips::N64