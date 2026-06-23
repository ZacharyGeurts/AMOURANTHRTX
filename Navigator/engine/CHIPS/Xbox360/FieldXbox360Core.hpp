#pragma once

#include <algorithm>
#include <cstring>

#include "FieldXbox360Cpu.hpp"
#include "../Common/FieldChipFabricScale.hpp"
#include "FieldXbox360GpuDie.hpp"

namespace FieldChips::Xbox360 {

inline bool loadImage(State& s, const std::uint8_t* data, std::size_t size) noexcept {
    if (!data || size < 512u) return false;
    resetState(s);
    const std::size_t n = std::min(size, s.mem.size());
    std::memcpy(s.mem.data(), data, n);
    s.cpu.pc = 0x80000000u;
    return true;
}

inline void runFrame(State& s) noexcept {
    xenonStep(s, 2048);
    if (s.gpuDieWave)
        xenosDieWaveStep(s, FieldChips::scaledDieCycles(16384u));
    s.frame++;
}

} // namespace FieldChips::Xbox360