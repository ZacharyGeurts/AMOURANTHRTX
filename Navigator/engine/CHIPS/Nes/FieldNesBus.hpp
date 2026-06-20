#pragma once

// NES CPU bus — RAM, PPU regs, APU, joypad, cart PRG/CHR.

#include "FieldNes2A03.hpp"
#include "FieldNesMappers.hpp"

namespace FieldChips::Nes {

inline std::uint16_t mirrorNametableAddr(const State& s, std::uint16_t addr) noexcept {
    addr = static_cast<std::uint16_t>(0x2000 + (addr & 0x0FFF));
    const std::uint16_t table = static_cast<std::uint16_t>((addr >> 10) & 3);
    std::uint16_t off = static_cast<std::uint16_t>(addr & 0x03FF);
    switch (s.mirroring) {
    case Mirror::Vertical:
        off = static_cast<std::uint16_t>(((table & 1) << 10) | (addr & 0x03FF));
        break;
    case Mirror::Horizontal:
        off = static_cast<std::uint16_t>((((table >> 1) & 1) << 10) | (addr & 0x03FF));
        break;
    case Mirror::SingleLo:
        off = static_cast<std::uint16_t>(addr & 0x03FF);
        break;
    case Mirror::SingleHi:
        off = static_cast<std::uint16_t>(0x400 | (addr & 0x03FF));
        break;
    }
    return off;
}

inline std::uint8_t readPpuMem(const State& s, std::uint16_t addr) noexcept {
    addr &= 0x3FFF;
    if (addr < 0x2000)
        return readChrByte(s, addr);
    if (addr < 0x3F00) {
        const std::uint16_t m = mirrorNametableAddr(s, addr);
        return s.ppu.vram[m & 0x07FF];
    }
    if (addr >= 0x3F00) {
        const std::uint16_t pal = static_cast<std::uint16_t>(addr & 0x1F);
        const std::size_t idx = 0x700u + pal;
        if (pal >= 0x10 && (pal & 3) == 0) return s.ppu.vram[0x700u + (pal & 0x0Cu)];
        return s.ppu.vram[idx];
    }
    return 0;
}

inline void writePpuMem(State& s, std::uint16_t addr, std::uint8_t v) noexcept {
    addr &= 0x3FFF;
    if (addr < 0x2000) {
        if (s.cart.chrBanks == 0 && addr < s.cart.chr.size())
            s.cart.chr[addr] = v;
        return;
    }
    if (addr < 0x3F00) {
        const std::uint16_t m = mirrorNametableAddr(s, addr);
        s.ppu.vram[m & 0x07FF] = v;
        return;
    }
    if (addr >= 0x3F00) {
        const std::uint16_t pal = static_cast<std::uint16_t>(addr & 0x1F);
        s.ppu.vram[0x700u + pal] = v;
        if ((pal & 3) == 0)
            s.ppu.vram[0x700u + (pal ^ 0x10u)] = v;
    }
}

inline void ppuWriteReg(State& s, std::uint16_t addr, std::uint8_t v) noexcept {
    switch (addr & 7) {
    case 0:
        s.ppu.ctrl = v;
        break;
    case 1:
        s.ppu.mask = v;
        break;
    case 2:
        break;
    case 3:
        s.ppu.oamAddr = v;
        break;
    case 4:
        s.ppu.oam[s.ppu.oamAddr++] = v;
        break;
    case 5:
        if (s.ppu.addrLatch == 0) {
            s.ppu.fineX = static_cast<std::uint8_t>(v & 7u);
            s.ppu.scrollT = static_cast<std::uint16_t>(
                (s.ppu.scrollT & ~0x001Fu) | (static_cast<std::uint16_t>(v >> 3) & 0x001Fu));
            s.ppu.scrollX = v;
            s.ppu.addrLatch = 1;
        } else {
            s.ppu.scrollT = static_cast<std::uint16_t>(
                (s.ppu.scrollT & ~0x73E0u)
                | (static_cast<std::uint16_t>(v & 0x07u) << 12)
                | (static_cast<std::uint16_t>(v & 0xF8u) << 2));
            s.ppu.scrollY = v;
            s.ppu.addrLatch = 0;
        }
        break;
    case 6:
        if (s.ppu.addrLatch == 0) {
            s.ppu.scrollT = static_cast<std::uint16_t>((s.ppu.scrollT & 0x00FFu)
                | (static_cast<std::uint16_t>(v) << 8));
            s.ppu.vramAddr = static_cast<std::uint16_t>((s.ppu.vramAddr & 0xFF00u) | v);
            s.ppu.addrLatch = 1;
        } else {
            s.ppu.scrollT = static_cast<std::uint16_t>((s.ppu.scrollT & 0xFF00u) | v);
            s.ppu.scrollV = s.ppu.scrollT;
            s.ppu.vramAddr = s.ppu.scrollT;
            s.ppu.addrLatch = 0;
        }
        break;
    case 7:
        writePpuMem(s, s.ppu.vramAddr, v);
        s.ppu.vramAddr += (s.ppu.ctrl & 0x04) ? 32 : 1;
        break;
    default: break;
    }
}

inline std::uint8_t readPad(State& s, std::uint8_t pad, std::uint8_t& shift) noexcept {
    if (shift == 0) shift = 1;
    else {
        const std::uint8_t bit = (pad >> (8 - shift)) & 1;
        ++shift;
        if (shift > 8) shift = 1;
        return static_cast<std::uint8_t>(0x40 | bit);
    }
    return static_cast<std::uint8_t>(0x40 | (pad & 1));
}

inline std::uint8_t readCpuMem(State& s, std::uint16_t addr) noexcept {
    if (addr < 0x2000) return s.ram[addr & 0x7FF];
    if (addr < 0x4000) {
        const std::uint16_t reg = static_cast<std::uint16_t>(addr & 7);
        if (reg == 2) {
            const std::uint8_t st = s.ppu.status;
            s.ppu.status &= ~0x80u;
            s.ppu.addrLatch = 0;
            return st;
        }
        if (reg == 4) return s.ppu.oam[s.ppu.oamAddr++];
        if (reg == 7) {
            const std::uint8_t v = readPpuMem(s, s.ppu.vramAddr);
            s.ppu.vramAddr += (s.ppu.ctrl & 0x04) ? 32 : 1;
            return v;
        }
        return 0;
    }
    if (addr == 0x4015) return apuReadStatus(s);
    if (addr == 0x4016) return readPad(s, s.pad1, s.pad1Shift);
    if (addr == 0x4017) return readPad(s, s.pad2, s.pad2Shift);
    if (addr >= 0x5000 && addr < 0x6000) return 0;
    if (addr >= 0x6000 && addr < 0x8000) return s.prgRam[addr - 0x6000];
    if (addr >= 0x8000) return readCartPrg(s, addr);
    return 0;
}

inline void writeCpuMem(State& s, std::uint16_t addr, std::uint8_t v) noexcept {
    if (addr < 0x2000) {
        s.ram[addr & 0x7FF] = v;
        return;
    }
    if (addr < 0x4000) {
        ppuWriteReg(s, addr, v);
        return;
    }
    if (addr >= 0x4000 && addr <= 0x4017) {
        if (addr == 0x4014) {
            const std::uint16_t base = static_cast<std::uint16_t>(v << 8);
            for (int i = 0; i < 256; ++i)
                s.ppu.oam[static_cast<std::size_t>(i)] = s.ram[(base + i) & 0x7FF];
            s.cpu.stall = true;
            s.cpu.stallCycles = 513;
            return;
        }
        if (addr == 0x4016) {
            if (v & 1) {
                s.pad1Shift = 0;
                s.pad2Shift = 0;
            }
            return;
        }
        apuWriteReg(s, addr, v);
        return;
    }
    if (addr >= 0x6000 && addr < 0x8000) {
        s.prgRam[addr - 0x6000] = v;
        return;
    }
    if (addr >= 0x8000) mapperWrite(s, addr, v);
}

} // namespace FieldChips::Nes