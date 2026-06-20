#pragma once

#include "FieldA2600Cart.hpp"
#include "FieldA2600Riot.hpp"
#include "FieldA2600Tia.hpp"

namespace FieldChips::Atari2600 {

inline std::uint8_t readCpuMem(State& s, std::uint16_t addr) noexcept {
    addr &= 0x1FFF;
    if (addr < 0x0080) return tiaRead(s, addr);
    if (addr < 0x0100) return s.ram[addr & 0x7F];
    if (addr >= 0x0280 && addr < 0x0300) return riotRead(s, static_cast<std::uint16_t>(addr - 0x0280));
    if (addr >= 0x1000) return readCart(s, addr);
    return 0xFF;
}

inline void writeCpuMem(State& s, std::uint16_t addr, std::uint8_t v) noexcept {
    addr &= 0x1FFF;
    if (addr < 0x0040) { tiaWrite(s, addr, v); return; }
    if (addr < 0x0080) return;
    if (addr < 0x0100) { s.ram[addr & 0x7F] = v; return; }
    if (addr >= 0x0280 && addr < 0x0300) { riotWrite(s, static_cast<std::uint16_t>(addr - 0x0280), v); return; }
}

} // namespace FieldChips::Atari2600