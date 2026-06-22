#pragma once

// Nintendo 64 — scaffold (MIPS VR4300 + RDP/RSP wave tier).

#include <cstdint>

namespace FieldChips::N64 {

struct State {
    std::uint32_t pc = 0x80000400u;
    std::uint8_t ram[4 * 1024 * 1024]{};
    bool rspWave = false;
    int tier = 1;
};

inline bool loadStub(State& s) noexcept {
    s.pc = 0x80000400u;
    s.rspWave = true;
    return true;
}

} // namespace FieldChips::N64