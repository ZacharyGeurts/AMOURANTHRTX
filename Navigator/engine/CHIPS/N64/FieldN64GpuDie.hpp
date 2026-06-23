#pragma once

#include "FieldN64Types.hpp"
#include "../Common/FieldChipFabricScale.hpp"

#include <cmath>

namespace FieldChips::N64 {

inline void rspRdpWaveStep(State& s, std::uint32_t busCycles) noexcept {
    auto& r = s.rsp;
    r.dieCycles += busCycles;
    r.wavePhase = std::fmod(r.wavePhase + static_cast<double>(busCycles) * 1.2e-5, 6.28318530718);
    r.waveActive = true;
    const std::uint32_t color = 0xFF000000u
        | (static_cast<std::uint32_t>((std::sin(r.wavePhase) + 1.0) * 80.0) << 16)
        | (static_cast<std::uint32_t>((std::cos(r.wavePhase * 1.3) + 1.0) * 60.0) << 8)
        | static_cast<std::uint32_t>(r.dieCycles & 0xFFu);
    for (int y = 0; y < r.fbH; ++y)
        for (int x = 0; x < r.fbW; ++x) {
            const int idx = y * r.fbW + x;
            r.fb[idx] = ((x ^ y ^ static_cast<int>(r.dieCycles >> 6)) & 15) == 0 ? color : (color ^ 0x00101010u);
        }
}

} // namespace FieldChips::N64