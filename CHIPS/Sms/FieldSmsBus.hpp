#pragma once

#include "FieldSmsCart.hpp"
#include "FieldSmsVdp.hpp"

namespace FieldChips::Sms {

inline std::uint8_t readMem(State& s, std::uint16_t addr) noexcept {
    if (addr < 0x8000 && !s.cart.rom.empty())
        return readCart(s, addr);
    if (addr >= 0xC000 && addr < 0xE000)
        return s.ram[addr - 0xC000];
    if (addr >= 0xE000)
        return s.ram[addr - 0xE000];
    return 0xFF;
}

inline void writeMem(State& s, std::uint16_t addr, std::uint8_t v) noexcept {
    if (addr >= 0xC000 && addr < 0xE000)
        s.ram[addr - 0xC000] = v;
    else if (addr >= 0xE000)
        s.ram[addr - 0xE000] = v;
}

inline std::uint8_t readIo(State& s, std::uint16_t addr) noexcept {
    switch (addr & 0xFF) {
    case 0x7E: return vdpReadData(s);
    case 0x7F: return vdpReadStatus(s);
    case 0xDD: return s.pad1;
    case 0xDC: return s.pad2;
    default: return 0xFF;
    }
}

inline void writeIo(State& s, std::uint16_t addr, std::uint8_t v) noexcept {
    switch (addr & 0xFF) {
    case 0x7E: vdpWriteData(s, v); break;
    case 0x7F: vdpWriteCtrl(s, v); break;
    case 0x3F: sn76489Write(s.psg, v); break;
    default: break;
    }
}

} // namespace FieldChips::Sms