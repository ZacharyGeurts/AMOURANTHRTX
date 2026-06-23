#pragma once

#include "FieldDreamcastTypes.hpp"

namespace FieldChips::Dreamcast {

inline void sh4Step(State& s, int steps = 1) noexcept {
    auto& c = s.cpu;
    for (int i = 0; i < steps; ++i) {
        const std::uint32_t off = (c.pc - s.cart.entryPc) >> 1;
        const std::uint32_t idx = off & 0xFFFFu;
        const std::uint16_t op = *reinterpret_cast<const std::uint16_t*>(s.ram + (idx & 0xFFFu) * 2u);
        c.gpr[0] ^= op;
        c.pc += 2u;
        c.cycles += 1u;
    }
}

} // namespace FieldChips::Dreamcast