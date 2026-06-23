#pragma once

#include "FieldPs1Types.hpp"
#include "../Common/FieldChipFabricScale.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace FieldChips::Ps1 {

// FieldDie GPU wave — PS1 GP0/GP1 command stream mapped to die-cycle raster bursts.
inline void gpuDieWaveStep(State& s, std::uint32_t busCycles) noexcept {
    auto& g = s.gpu;
    g.dieCycles += busCycles;
    g.wavePhase = std::fmod(g.wavePhase + static_cast<double>(busCycles) * 1e-5, 6.28318530718);
    g.waveActive = true;

    const int band = static_cast<int>((std::sin(g.wavePhase) + 1.0) * 0.5 * 31.0);
    const std::uint32_t color = 0xFF000000u
        | (static_cast<std::uint32_t>(band * 4) << 16)
        | (static_cast<std::uint32_t>((31 - band) * 6) << 8)
        | static_cast<std::uint32_t>(band * 2);

    for (int y = 0; y < g.fbH; ++y) {
        for (int x = 0; x < g.fbW; ++x) {
            const int idx = y * g.fbW + x;
            const bool stripe = ((x + y + static_cast<int>(g.dieCycles >> 4)) & 7) == 0;
            g.fb[idx] = stripe ? color : (color ^ 0x00202020u);
        }
    }
}

inline void gpuQueueGp0(State& s, std::uint32_t cmd) noexcept {
    s.gpu.gp0 = cmd;
    if ((cmd & 0xFF000000u) == 0x02000000u)
        gpuDieWaveStep(s, FieldChips::scaledDieCycles(8192u));
}

inline int gpuCopyFb(const State& s, std::uint8_t* out, int maxBytes) noexcept {
    if (!out || maxBytes <= 0) return 0;
    const int pixels = s.gpu.fbW * s.gpu.fbH;
    const int need = pixels * 4;
    const int n = std::min(maxBytes, need);
    std::memcpy(out, s.gpu.fb, static_cast<std::size_t>(n));
    return n;
}

} // namespace FieldChips::Ps1