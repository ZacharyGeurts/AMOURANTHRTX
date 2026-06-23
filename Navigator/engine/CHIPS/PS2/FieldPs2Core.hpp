#pragma once

#include "FieldPs2Cpu.hpp"
#include "FieldPs2GpuDie.hpp"

#include <algorithm>
#include <cstring>

namespace FieldChips::Ps2 {

inline bool loadCart(State& s, const std::uint8_t* data, std::size_t size) noexcept {
    if (!data || size < 4096u) return false;
    resetState(s);
    s.cart.rom.assign(data, data + size);
    const std::size_t n = std::min(size, sizeof s.ram);
    std::memcpy(s.ram, data, n);
    std::snprintf(s.cart.title, sizeof s.cart.title, "PS2 FieldDie %.16s", data);
    s.cpu.pc = s.cart.entryPc;
    return true;
}

inline void runFrame(State& s) noexcept {
    eeStep(s, 3072);
    if (s.gpuDieWave)
        gsWaveStep(s, FieldChips::scaledDieCycles(20480u));
    s.frame++;
}

} // namespace FieldChips::Ps2