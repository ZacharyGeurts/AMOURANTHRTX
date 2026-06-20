#pragma once

#include "FieldGenesisCart.hpp"
#include "FieldGenesisVdp.hpp"

namespace FieldChips::Genesis {

inline std::uint8_t readM68k8(State& s, std::uint32_t addr) noexcept {
    addr &= 0xFFFFFF;
    if (addr < 0x400000) return readCart8(s, addr);
    if (addr >= 0xFF0000 && addr < 0xFFFFFF) return s.ram[addr - 0xFF0000];
    if (addr >= 0xA10000 && addr < 0xA10020) {
        switch (addr & 0x0F) {
        case 0x00: return s.pad1;
        case 0x02: return s.pad2;
        default: return 0xFF;
        }
    }
    if (addr >= 0xC00000 && addr < 0xC00010) {
        if ((addr & 1) == 0) return vdpReadData(s);
    }
    return 0;
}

inline void writeM68k8(State& s, std::uint32_t addr, std::uint8_t v) noexcept {
    addr &= 0xFFFFFF;
    if (addr >= 0xFF0000 && addr < 0xFFFFFF)
        s.ram[addr - 0xFF0000] = v;
    else if (addr >= 0xC00000 && addr < 0xC00010) {
        if (addr & 1)
            vdpWriteCtrl(s, static_cast<std::uint16_t>(v | (v << 8)));
        else
            vdpWriteData(s, v);
    } else if (addr >= 0xA11100 && addr < 0xA11200) {
        s.z80Reset = (v & 1) == 0;
    }
}

inline std::uint8_t readZ80(State& s, std::uint16_t addr) noexcept {
    if (addr < 0x4000) return readCart8(s, addr);
    if (addr >= 0x8000 && addr < 0xFFFF) return s.z80Ram[addr - 0x8000];
    if (addr >= 0x4000 && addr < 0x4003) {
        switch (addr) {
        case 0x4000: return 0;
        case 0x4001: return 0;
        case 0x4002: return 0;
        case 0x4003: return 0;
        }
    }
    if (addr == 0x7F11 || addr == 0x7F12) return 0;
    return 0xFF;
}

inline void writeZ80(State& s, std::uint16_t addr, std::uint8_t v) noexcept {
    if (addr >= 0x8000 && addr < 0xFFFF)
        s.z80Ram[addr - 0x8000] = v;
    else if (addr == 0x4000)
        ym2612WriteAddr(s.ym, 0, v);
    else if (addr == 0x4001)
        ym2612WriteData(s.ym, 0, v);
    else if (addr == 0x4002)
        ym2612WriteAddr(s.ym, 1, v);
    else if (addr == 0x4003)
        ym2612WriteData(s.ym, 1, v);
}

inline std::uint8_t z80InPort(State& s, std::uint8_t port) noexcept {
    (void)port;
    return 0xFF;
}

inline void z80OutPort(State& s, std::uint8_t port, std::uint8_t v) noexcept {
    if (port == 0x7F || port == 0x3F)
        sn76489Write(s.psg, v);
}

} // namespace FieldChips::Genesis