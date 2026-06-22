#pragma once

// Sony PlayStation 2 — scaffold (EE MIPS + GS wave tier).

#include <cstdint>

namespace FieldChips::Ps2 {

struct State {
    std::uint32_t pc = 0xBFC00000u;
    std::uint8_t ram[32 * 1024 * 1024]{};
    bool gsWave = false;
    int tier = 2;
};

inline bool loadStub(State& s) noexcept {
    s.pc = 0xBFC00000u;
    s.gsWave = true;
    return true;
}

} // namespace FieldChips::Ps2