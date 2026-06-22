#pragma once

#include "FieldPs1Types.hpp"

namespace FieldChips::Ps1 {

inline void gteRtps(State& s, std::int32_t vx, std::int32_t vy, std::int32_t vz) noexcept {
    auto& g = s.gte;
    g.ir[0] = vx + g.tr[0];
    g.ir[1] = vy + g.tr[1];
    g.ir[2] = vz + g.tr[2];
    g.ops++;
    g.flag |= 1u;
}

inline void gteNclip(State& s) noexcept {
    auto& g = s.gte;
    const std::int32_t z = g.ir[0] * g.mtx[0][2] + g.ir[1] * g.mtx[1][2] + g.ir[2] * g.mtx[2][2];
    if (z <= 0) g.flag |= 2u;
    g.ops++;
}

} // namespace FieldChips::Ps1