#pragma once

#include "FieldDreamcastCpu.hpp"
#include "FieldDreamcastGpuDie.hpp"

#include <algorithm>
#include <cstring>

namespace FieldChips::Dreamcast {

inline bool loadCart(State& s, const std::uint8_t* data, std::size_t size) noexcept {
    if (!data || size < 2048u) return false;
    resetState(s);
    s.cart.rom.assign(data, data + size);
    const std::size_t n = std::min(size, sizeof s.ram);
    std::memcpy(s.ram, data, n);
    std::snprintf(s.cart.title, sizeof s.cart.title, "DC FieldDie %.16s", data);
    s.cpu.pc = s.cart.entryPc;
    return true;
}

inline void runFrame(State& s) noexcept {
    sh4Step(s, 4096);
    if (s.gpuDieWave)
        pvrWaveStep(s, FieldChips::scaledDieCycles(16384u));
    s.frame++;
}

} // namespace FieldChips::Dreamcast