#pragma once

#include "FieldA2600Types.hpp"

namespace FieldChips::Atari2600 {

inline void riotTick(State& s, int cycles) noexcept {
    auto& r = s.riot;
    r.divider += cycles;
    while (r.divider >= (1 + r.interval)) {
        r.divider -= (1 + r.interval);
        if (r.intim > 0) --r.intim;
        else s.irqLine = true;
    }
}

inline std::uint8_t riotRead(State& s, std::uint16_t addr) noexcept {
    addr = static_cast<std::uint16_t>(addr & 0x1F);
    switch (addr) {
    case 0x00: return static_cast<std::uint8_t>(s.padLeft ^ 0xFF);
    case 0x01: return s.riot.swac;
    case 0x02: return static_cast<std::uint8_t>(s.padRight ^ 0xFF);
    case 0x03: return s.riot.swbc;
    case 0x04: return s.riot.intim;
    case 0x05: return s.riot.tim1t;
    default: return 0xFF;
    }
}

inline void riotWrite(State& s, std::uint16_t addr, std::uint8_t v) noexcept {
    addr = static_cast<std::uint16_t>(addr & 0x1F);
    switch (addr) {
    case 0x01: s.riot.swac = v; break;
    case 0x03: s.riot.swbc = v; break;
    case 0x04: s.riot.intim = v; s.irqLine = false; break;
    case 0x05: s.riot.tim1t = v; s.riot.intim = v; s.riot.interval = 0; s.riot.divider = 0; break;
    case 0x06: s.riot.intim = v; s.riot.interval = 1; s.riot.divider = 0; break;
    case 0x07: s.riot.intim = v; s.riot.interval = 7; s.riot.divider = 0; break;
    case 0x09: s.riot.intim = v; s.riot.interval = 15; s.riot.divider = 0; break;
    case 0x0A: s.riot.intim = v; s.riot.interval = 31; s.riot.divider = 0; break;
    case 0x0B: s.riot.intim = v; s.riot.interval = 63; s.riot.divider = 0; break;
    case 0x0C: s.riot.intim = v; s.riot.interval = 127; s.riot.divider = 0; break;
    case 0x0D: s.riot.intim = v; s.riot.interval = 255; s.riot.divider = 0; break;
    case 0x0E: s.riot.intim = v; s.riot.interval = 0; s.riot.divider = 0; break;
    case 0x0F: s.riot.intim = v; s.riot.interval = 0; s.riot.divider = 0; break;
    default: break;
    }
}

} // namespace FieldChips::Atari2600