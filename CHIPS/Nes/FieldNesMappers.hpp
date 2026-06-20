#pragma once

// NES cart mappers — NROM, MMC1, UNROM, CNROM, MMC3, AxROM, GNROM.

#include "FieldNesTypes.hpp"

#include <algorithm>
#include <cstddef>

namespace FieldChips::Nes {

inline std::size_t prgBankCount(const Cart& c) noexcept {
    return c.prg.empty() ? 0 : c.prg.size() / 16384u;
}

inline std::size_t chrBankCount(const Cart& c) noexcept {
    return c.chr.empty() ? 0 : c.chr.size() / 8192u;
}

inline std::uint8_t readPrgFixed(const State& s, std::uint16_t addr) noexcept {
    if (s.cart.prg.size() < 16384) return 0;
    const std::size_t bankSize = 16384u;
    const int banks = static_cast<int>(prgBankCount(s.cart));
    if (banks <= 0) return 0;
    if (addr < 0xC000) {
        const std::size_t off = static_cast<std::size_t>(s.map.prgBank % banks) * bankSize
            + static_cast<std::size_t>(addr - 0x8000);
        return off < s.cart.prg.size() ? s.cart.prg[off] : 0;
    }
    const std::size_t off = static_cast<std::size_t>(banks - 1) * bankSize
        + static_cast<std::size_t>(addr - 0xC000);
    return off < s.cart.prg.size() ? s.cart.prg[off] : 0;
}

inline std::uint8_t readPrgNrom(const State& s, std::uint16_t addr) noexcept {
    if (s.cart.prg.size() < 16384) return 0;
    const std::size_t bankSize = 16384u;
    const int banks = static_cast<int>(prgBankCount(s.cart));
    if (banks == 1) {
        const std::size_t off = static_cast<std::size_t>(addr - 0x8000) % bankSize;
        return s.cart.prg[off];
    }
    const std::size_t off = static_cast<std::size_t>(addr - 0x8000);
    return off < s.cart.prg.size() ? s.cart.prg[off] : 0;
}

inline std::uint8_t readPrgMmc1(const State& s, std::uint16_t addr) noexcept {
    const int banks = static_cast<int>(prgBankCount(s.cart));
    if (banks <= 0) return 0;
    int bank = 0;
    if (addr >= 0xC000) {
        if (s.map.prgMode == 1) bank = 0;
        else bank = s.map.prgBank;
    } else {
        if (s.map.prgMode == 1) bank = banks - 1;
        else if (s.map.prgMode == 2) bank = s.map.prgBank & ~1;
        else bank = s.map.prgBank;
    }
    bank %= banks;
    const std::size_t off = static_cast<std::size_t>(bank) * 16384u + static_cast<std::size_t>(addr & 0x3FFF);
    return off < s.cart.prg.size() ? s.cart.prg[off] : 0;
}

inline std::uint8_t readPrgMmc3(const State& s, std::uint16_t addr) noexcept {
    const int banks = static_cast<int>(prgBankCount(s.cart));
    if (banks <= 0) return 0;
    int bank = 0;
    if (addr < 0xA000)
        bank = s.map.mmc3PrgBank[0];
    else if (addr < 0xC000)
        bank = s.map.mmc3PrgBank[1];
    else if (s.map.mmc3Reg[6] & 0x40)
        bank = s.map.mmc3PrgBank[1];
    else
        bank = banks - 1;
    bank = std::clamp(bank, 0, std::max(0, banks - 1));
    const std::size_t off = static_cast<std::size_t>(bank) * 16384u + static_cast<std::size_t>(addr & 0x3FFF);
    return off < s.cart.prg.size() ? s.cart.prg[off] : 0;
}

inline std::uint8_t readPrgAxrom(const State& s, std::uint16_t addr) noexcept {
    const int banks = static_cast<int>(prgBankCount(s.cart));
    if (banks <= 0) return 0;
    const int bank = s.map.axromBank % banks;
    const std::size_t off = static_cast<std::size_t>(bank) * 16384u + static_cast<std::size_t>(addr & 0x3FFF);
    return off < s.cart.prg.size() ? s.cart.prg[off] : 0;
}

inline std::uint8_t readPrgGnrom(const State& s, std::uint16_t addr) noexcept {
    const int banks = static_cast<int>(prgBankCount(s.cart));
    if (banks <= 0) return 0;
    const int bank = s.map.gnromPrgBank % banks;
    const std::size_t off = static_cast<std::size_t>(bank) * 16384u + static_cast<std::size_t>(addr & 0x3FFF);
    return off < s.cart.prg.size() ? s.cart.prg[off] : 0;
}

inline std::uint8_t readCartPrg(const State& s, std::uint16_t addr) noexcept {
    switch (s.cart.mapper) {
    case 0: return readPrgNrom(s, addr);
    case 1: return readPrgMmc1(s, addr);
    case 2: return readPrgFixed(s, addr);
    case 3: return readPrgNrom(s, addr);
    case 4: return readPrgMmc3(s, addr);
    case 7: return readPrgAxrom(s, addr);
    case 66: return readPrgGnrom(s, addr);
    default: return readPrgFixed(s, addr);
    }
}

inline std::uint8_t readChrByte(const State& s, std::uint32_t off) noexcept {
    if (s.cart.chr.empty()) return 0;
    const int chrBanks = static_cast<int>(chrBankCount(s.cart));
    std::uint32_t mapped = off;
    switch (s.cart.mapper) {
    case 1: {
        int bank = (off < 0x1000) ? s.map.chrBank : s.map.chrBank + 1;
        if (s.map.chrMode == 1) bank = (off < 0x1000) ? s.map.chrBank + 1 : s.map.chrBank;
        if (chrBanks > 0) bank %= chrBanks;
        mapped = static_cast<std::uint32_t>(bank) * 0x1000u + (off & 0x0FFFu);
        break;
    }
    case 3:
    case 66: {
        int bank = (s.cart.mapper == 3) ? s.map.cnromChrBank : s.map.gnromChrBank;
        if (chrBanks > 0) bank %= chrBanks;
        mapped = static_cast<std::uint32_t>(bank) * 0x2000u + (off & 0x1FFFu);
        break;
    }
    case 4: {
        const int slot = static_cast<int>((off & 0x1C00) >> 10);
        int bank = s.map.mmc3ChrBank[slot];
        if (chrBanks > 0) bank %= chrBanks;
        mapped = static_cast<std::uint32_t>(bank) * 0x0400u + (off & 0x03FFu);
        break;
    }
    default:
        break;
    }
    return mapped < s.cart.chr.size() ? s.cart.chr[mapped] : 0;
}

inline void mmc1Write(State& s, std::uint16_t addr, std::uint8_t v) noexcept {
    if (v & 0x80) {
        s.map.mmc1Shift = 0;
        s.map.mmc1WriteCount = 0;
        s.map.prgMode = 1;
        return;
    }
    s.map.mmc1Shift = static_cast<std::uint8_t>((s.map.mmc1Shift >> 1) | ((v & 1) << 4));
    ++s.map.mmc1WriteCount;
    if (s.map.mmc1WriteCount < 5) return;
    s.map.mmc1WriteCount = 0;
    const std::uint8_t reg = static_cast<std::uint8_t>((addr >> 13) & 3);
    const std::uint8_t data = s.map.mmc1Shift;
    switch (reg) {
    case 0:
        s.mirroring = (data & 1) ? Mirror::Vertical : Mirror::Horizontal;
        s.map.chrMode = (data & 0x10) ? 1 : 0;
        break;
    case 1:
        s.map.chrBank = data & 0x1F;
        break;
    case 2:
        if (s.map.chrMode == 0) s.map.chrBank = data & 0x1F;
        else s.map.chrBank = data & 0x1E;
        break;
    case 3:
        s.map.prgBank = data & 0x0F;
        s.map.prgMode = (data >> 2) & 3;
        break;
    default: break;
    }
    s.map.mmc1Shift = 0;
}

inline void mmc3Write(State& s, std::uint16_t addr, std::uint8_t v) noexcept {
    const bool a12 = (addr & 0x8000) != 0;
    if (!a12) {
        s.map.mmc3Next = static_cast<std::uint8_t>(addr & 7);
        const std::uint8_t reg = s.map.mmc3Next;
        if (reg == 0) {
            s.mirroring = (v & 1) ? Mirror::Vertical : Mirror::Horizontal;
            s.map.mmc3PrgRamEnable = (v & 0x80) == 0;
        } else if (reg == 1) {
            s.map.mmc3IrqReload = true;
            s.map.mmc3IrqLatch = v;
        } else if (reg == 2) {
            s.map.mmc3IrqEnable = false;
            s.irqLine = false;
        }
        return;
    }
    const int chrBanks = static_cast<int>(chrBankCount(s.cart));
    const int prgBanks = static_cast<int>(prgBankCount(s.cart));
    switch (s.map.mmc3Next) {
    case 0: case 1: case 2: case 3: case 4: case 5:
        s.map.mmc3ChrBank[s.map.mmc3Next] = v | (s.map.mmc3Reg[0] & 0x80 ? 0x100 : 0);
        if (chrBanks > 0) s.map.mmc3ChrBank[s.map.mmc3Next] %= chrBanks;
        break;
    case 6:
        s.map.mmc3PrgBank[0] = v & 0x3F;
        if (prgBanks > 0) s.map.mmc3PrgBank[0] %= prgBanks;
        break;
    case 7:
        s.map.mmc3PrgBank[1] = v & 0x3F;
        if (prgBanks > 0) s.map.mmc3PrgBank[1] %= prgBanks;
        break;
    default: break;
    }
    s.map.mmc3Reg[s.map.mmc3Next] = v;
}

inline void mapperWrite(State& s, std::uint16_t addr, std::uint8_t v) noexcept {
    switch (s.cart.mapper) {
    case 1:
        if (addr >= 0x8000) mmc1Write(s, addr, v);
        break;
    case 2:
        if (addr >= 0x8000) s.map.prgBank = v & 0x0F;
        break;
    case 3:
        if (addr >= 0x8000) s.map.cnromChrBank = v & 0x03;
        break;
    case 4:
        if (addr >= 0x8000) mmc3Write(s, addr, v);
        break;
    case 7:
        if (addr >= 0x8000) {
            s.map.axromBank = v & 0x0F;
            s.mirroring = (v & 0x10) ? Mirror::SingleHi : Mirror::SingleLo;
        }
        break;
    case 66:
        if (addr >= 0x8000) {
            s.map.gnromChrBank = v & 0x03;
            s.map.gnromPrgBank = (v >> 4) & 0x03;
        }
        break;
    default:
        break;
    }
}

inline void mapperOnScanline(State& s) noexcept {
    if (s.cart.mapper != 4) return;
    if (s.map.mmc3IrqReload || s.map.mmc3IrqCounter == 0) {
        s.map.mmc3IrqCounter = s.map.mmc3IrqLatch;
        s.map.mmc3IrqReload = false;
    } else if (s.map.mmc3IrqCounter > 0)
        --s.map.mmc3IrqCounter;
    if (s.map.mmc3IrqCounter == 0 && s.map.mmc3IrqEnable)
        s.irqLine = true;
}

inline const char* mapperName(int id) noexcept {
    switch (id) {
    case 0: return "NROM";
    case 1: return "MMC1";
    case 2: return "UNROM";
    case 3: return "CNROM";
    case 4: return "MMC3";
    case 7: return "AxROM";
    case 9: return "MMC2";
    case 10: return "MMC4";
    case 11: return "Color Dreams";
    case 34: return "BNROM";
    case 66: return "GNROM";
    default: return "Mapper";
    }
}

} // namespace FieldChips::Nes