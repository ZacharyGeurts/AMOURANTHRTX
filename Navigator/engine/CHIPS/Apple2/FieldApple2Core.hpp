#pragma once

#include "FieldApple2Types.hpp"

namespace FieldChips::Apple2 {

inline bool loadRom(State& s, const std::uint8_t* data, std::size_t size) {
    if (!data || size < 256) return false;
    s.cart.rom.assign(data, data + size);
    resetState(s);
    return true;
}

inline void runFrameCpu(State&) noexcept {}

inline void renderFrame(State&, std::uint8_t*, int, int) noexcept {}

} // namespace FieldChips::Apple2