#pragma once

// Sega Dreamcast — scaffold (SH-4 + PowerVR2 wave tier).

#include <cstdint>

namespace FieldChips::Dreamcast {

struct State {
    std::uint32_t pc = 0x8C010000u;
    std::uint8_t ram[16 * 1024 * 1024]{};
    bool pvrWave = false;
    int tier = 1;
};

inline bool loadStub(State& s) noexcept {
    s.pc = 0x8C010000u;
    s.pvrWave = true;
    return true;
}

} // namespace FieldChips::Dreamcast