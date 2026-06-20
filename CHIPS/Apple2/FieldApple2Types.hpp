#pragma once

#include "../Common/FieldChipTypes.hpp"

#include <cstdint>
#include <vector>

namespace FieldChips::Apple2 {

struct Cart {
    std::vector<std::uint8_t> rom;
};

struct State {
    Cart cart;
    Cpu6502 cpu;
    std::uint8_t ram[0x10000]{};
};

inline void resetState(State& s) noexcept {
    FieldChips::reset6502(s.cpu);
}

} // namespace FieldChips::Apple2