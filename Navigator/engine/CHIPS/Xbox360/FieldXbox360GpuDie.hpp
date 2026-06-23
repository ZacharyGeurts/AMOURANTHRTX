#pragma once

#include "FieldXbox360Types.hpp"

#include <cmath>

namespace FieldChips::Xbox360 {

inline void xenosDieWaveStep(State& s, std::uint32_t busCycles) noexcept {
    auto& g = s.gpu;
    g.dieCycles += busCycles;
    g.wavePhase = std::fmod(g.wavePhase + static_cast<double>(busCycles) * 8e-6, 6.28318530718);
    g.waveActive = true;
    const int band = static_cast<int>((std::sin(g.wavePhase) + 1.0) * 0.5 * 255.0);
    const std::uint32_t color = 0xFF000000u
        | (static_cast<std::uint32_t>(band) << 16)
        | (static_cast<std::uint32_t>(255 - band) << 8)
        | static_cast<std::uint32_t>(band / 2);
    for (int i = 0; i < g.fbW * g.fbH; ++i)
        g.fb[static_cast<std::size_t>(i)] = (i & 31) == 0 ? color : (color ^ 0x00101010u);
}

} // namespace FieldChips::Xbox360