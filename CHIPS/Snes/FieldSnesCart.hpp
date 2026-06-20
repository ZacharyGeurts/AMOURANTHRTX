#pragma once

#include "FieldSnesTypes.hpp"
#include "FieldSnesSuperFx.hpp"

#include <cstring>

namespace FieldChips::Snes {

inline void parseRomHeader(State& s) noexcept {
    if (s.cart.rom.size() < 0x8000) return;
    const std::size_t hdr = (s.cart.map == MapMode::HiRom && s.cart.rom.size() >= 0x10000)
        ? 0xFFC0 : 0x7FC0;
    if (hdr + 21 < s.cart.rom.size()) {
        std::memcpy(s.cart.title, s.cart.rom.data() + hdr, 21);
        s.cart.title[21] = '\0';
    }
    s.cart.hasSuperFx = detectSuperFxRom(s.cart.rom.data(), s.cart.rom.size());
    s.gsu.present = s.cart.hasSuperFx;
}

inline bool loadRom(State& s, const std::uint8_t* data, std::size_t size) {
    if (!data || size < 512) return false;
    s.cart.rom.assign(data, data + size);
    s.cart.map = (size >= 0x100000 && (size & 0x7FFF) == 0) ? MapMode::HiRom : MapMode::LoRom;
    resetState(s);
    parseRomHeader(s);
    if (s.gsu.present) {
        resetGsu(s.gsu);
        s.gsu.present = true;
        bootGsuFromRom(s);
    }
    s.cpu.emulation = false;
    s.cpu.pc = 0x8000;
    return true;
}

inline std::uint8_t readCart8(const State& s, std::uint32_t addr) noexcept {
    if (s.cart.rom.empty()) return 0xFF;
    std::size_t off = 0;
    if (s.cart.map == MapMode::LoRom) {
        const std::uint32_t bank = (addr >> 16) & 0x7Fu;
        const std::uint16_t lo = static_cast<std::uint16_t>(addr & 0xFFFFu);
        if (lo < 0x8000) return 0xFF;
        off = static_cast<std::size_t>(bank) * 0x8000u + (lo - 0x8000u);
    } else {
        off = addr & 0x3FFFFu;
    }
    if (off < s.cart.rom.size()) return s.cart.rom[off];
    return 0xFF;
}

} // namespace FieldChips::Snes