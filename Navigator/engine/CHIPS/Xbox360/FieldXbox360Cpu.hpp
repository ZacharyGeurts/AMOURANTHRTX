#pragma once

#include "FieldXbox360Types.hpp"

namespace FieldChips::Xbox360 {

inline void xenonStep(State& s, int steps = 1) noexcept {
    auto& c = s.cpu;
    for (int i = 0; i < steps; ++i) {
        c.gpr[3] ^= c.pc;
        c.pc += 4u;
        c.cycles += 6u;
    }
}

} // namespace FieldChips::Xbox360