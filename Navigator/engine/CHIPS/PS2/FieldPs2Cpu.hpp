#pragma once

#include "FieldPs2Types.hpp"

namespace FieldChips::Ps2 {

inline void eeStep(State& s, int steps = 1) noexcept {
    auto& c = s.cpu;
    for (int i = 0; i < steps; ++i) {
        const std::uint32_t off = (c.pc >= s.cart.loadAddr)
            ? ((c.pc - s.cart.loadAddr) >> 2) : 0u;
        const std::uint32_t idx = off & ((sizeof s.ram / 4u) - 1u);
        const std::uint32_t op = *reinterpret_cast<const std::uint32_t*>(s.ram + idx * 4u);
        c.gpr[9] ^= op;
        c.pc += 4u;
        c.cycles += 2u;
    }
}

} // namespace FieldChips::Ps2