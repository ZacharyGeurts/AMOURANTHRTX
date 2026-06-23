#pragma once

#include "FieldDreamcastTypes.hpp"
#include "../Common/FieldChipFabricScale.hpp"

#include <cmath>

namespace FieldChips::Dreamcast {

inline void pvrWaveStep(State& s, std::uint32_t busCycles) noexcept {
    auto& p = s.pvr;
    p.dieCycles += busCycles;
    p.wavePhase = std::fmod(p.wavePhase + static_cast<double>(busCycles) * 9e-6, 6.28318530718);
    p.waveActive = true;
    const std::uint32_t color = 0xFF000000u
        | (static_cast<std::uint32_t>((std::sin(p.wavePhase * 0.9) + 1.0) * 100.0) << 16)
        | (static_cast<std::uint32_t>((std::cos(p.wavePhase) + 1.0) * 90.0) << 8)
        | static_cast<std::uint32_t>((p.dieCycles >> 4) & 0xFFu);
    for (int y = 0; y < p.fbH; ++y)
        for (int x = 0; x < p.fbW; ++x)
            p.fb[y * p.fbW + x] = color ^ static_cast<std::uint32_t>((x + y + p.dieCycles) & 0x0F0F0Fu);
}

} // namespace FieldChips::Dreamcast