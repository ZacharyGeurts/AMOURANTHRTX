#pragma once

#include "FieldPs2Types.hpp"
#include "../Common/FieldChipFabricScale.hpp"

#include <cmath>

namespace FieldChips::Ps2 {

inline void gsWaveStep(State& s, std::uint32_t busCycles) noexcept {
    auto& g = s.gs;
    g.dieCycles += busCycles;
    g.wavePhase = std::fmod(g.wavePhase + static_cast<double>(busCycles) * 7e-6, 6.28318530718);
    g.waveActive = true;
    const std::uint32_t color = 0xFF000000u
        | (static_cast<std::uint32_t>((std::sin(g.wavePhase * 1.1) + 1.0) * 110.0) << 16)
        | (static_cast<std::uint32_t>((std::cos(g.wavePhase * 0.7) + 1.0) * 95.0) << 8)
        | static_cast<std::uint32_t>(g.dieCycles & 0xFFu);
    for (int y = 0; y < g.fbH; ++y)
        for (int x = 0; x < g.fbW; ++x)
            g.fb[y * g.fbW + x] = ((x * y + static_cast<int>(g.dieCycles >> 5)) & 31) < 4
                ? color : (color ^ 0x00202040u);
}

} // namespace FieldChips::Ps2