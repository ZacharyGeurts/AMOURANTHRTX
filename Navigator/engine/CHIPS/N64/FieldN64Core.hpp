#pragma once

#include "FieldN64Cpu.hpp"
#include "FieldN64GpuDie.hpp"

#include <algorithm>
#include <cstring>

namespace FieldChips::N64 {

inline bool loadCart(State& s, const std::uint8_t* data, std::size_t size) noexcept {
    if (!data || size < 1024u) return false;
    resetState(s);
    s.cart.rom.assign(data, data + size);
    const std::size_t n = std::min(size, sizeof s.ram);
    std::memcpy(s.ram, data, n);
    std::snprintf(s.cart.title, sizeof s.cart.title, "N64 FieldDie %.16s", data);
    s.cpu.pc = s.cart.entryPc;
    return true;
}

inline void runFrame(State& s) noexcept {
    mipsStep(s, 2048);
    s.rsp.pc += 4u;
    if (s.gpuDieWave)
        rspRdpWaveStep(s, FieldChips::scaledDieCycles(12288u));
    s.frame++;
}

} // namespace FieldChips::N64