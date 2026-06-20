#pragma once

// Super FX (GSU) — thermo-governed RISC coprocessor for Star Fox–class titles.
// Optimized: batched stepping, inlined banked ROM/RAM, fast plot/rpix paths.

#include "FieldSnesTypes.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace FieldChips::Snes {

namespace SuperFxDetail {

constexpr std::uint16_t kRegBase = 0x3000;
constexpr int kRamBytes = 0x20000;
constexpr int kPlotBudgetDefault = 8192;
constexpr int kStepBudgetDefault = 4096;

inline std::uint8_t sfrGo(std::uint8_t sfr) noexcept { return sfr & 0x80u; }
inline std::uint8_t sfrCy(std::uint8_t sfr) noexcept { return sfr & 0x40u; }
inline void setSfrCy(Gsu& g, bool v) noexcept {
    if (v) g.sfr |= 0x40u; else g.sfr &= ~0x40u;
}
inline void setSfrZ(Gsu& g, std::uint16_t v) noexcept {
    if (v == 0) g.sfr |= 0x20u; else g.sfr &= ~0x20u;
}
inline void clearGo(Gsu& g) noexcept { g.sfr &= ~0x80u; }
inline void setGo(Gsu& g) noexcept { g.sfr |= 0x80u; }

inline std::uint32_t romAddr(Gsu& g, std::uint16_t addr) noexcept {
    const std::uint32_t bank = static_cast<std::uint32_t>(g.romb) << 16;
    return bank | addr;
}

inline std::uint8_t readRom8(State& s, std::uint32_t addr) noexcept {
    if (s.cart.rom.empty()) return 0xFF;
    const std::size_t off = static_cast<std::size_t>(addr) % s.cart.rom.size();
    return s.cart.rom[off];
}

inline std::uint8_t readRam8(Gsu& g, std::uint16_t addr) noexcept {
    addr &= 0x1FFFu;
    return g.ram[addr];
}

inline void writeRam8(Gsu& g, std::uint16_t addr, std::uint8_t v) noexcept {
    addr &= 0x1FFFu;
    g.ram[addr] = v;
}

inline std::uint16_t readRam16(Gsu& g, std::uint16_t addr) noexcept {
    const std::uint8_t lo = readRam8(g, addr);
    const std::uint8_t hi = readRam8(g, static_cast<std::uint16_t>(addr + 1u));
    return static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8));
}

inline void writeRam16(Gsu& g, std::uint16_t addr, std::uint16_t v) noexcept {
    writeRam8(g, addr, static_cast<std::uint8_t>(v & 0xFFu));
    writeRam8(g, static_cast<std::uint16_t>(addr + 1u), static_cast<std::uint8_t>(v >> 8));
}

inline std::uint16_t& reg(Gsu& g, int idx) noexcept {
    return g.r[idx & 15];
}

inline std::uint16_t& srcReg(Gsu& g) noexcept {
    return reg(g, (g.sfr >> 2) & 7);
}

inline std::uint16_t& dstReg(Gsu& g) noexcept {
    return reg(g, (g.sfr >> 5) & 7);
}

inline void pushPc(Gsu& g) noexcept {
    g.ram[0x1F0u + g.sp] = static_cast<std::uint8_t>(g.pc & 0xFFu);
    g.ram[0x1F1u + g.sp] = static_cast<std::uint8_t>(g.pc >> 8);
    g.sp = static_cast<std::uint8_t>((g.sp + 2u) & 0x1Fu);
}

inline std::uint16_t popPc(Gsu& g) noexcept {
    g.sp = static_cast<std::uint8_t>((g.sp - 2u) & 0x1Fu);
    const std::uint8_t lo = g.ram[0x1F0u + g.sp];
    const std::uint8_t hi = g.ram[0x1F1u + g.sp];
    return static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8));
}

inline std::uint8_t fetch8(State& s) noexcept {
    auto& g = s.gsu;
    const std::uint8_t v = readRom8(s, romAddr(g, g.pc));
    ++g.pc;
    ++g.cycles;
    return v;
}

inline std::uint8_t fetchImm8(State& s) noexcept {
    return fetch8(s);
}

inline std::uint16_t fetchImm16(State& s) noexcept {
    const std::uint8_t lo = fetch8(s);
    const std::uint8_t hi = fetch8(s);
    return static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(hi) << 8));
}

inline void plotPixel(Gsu& g, std::uint16_t x, std::uint16_t y) noexcept {
    const std::uint16_t color = g.colr & 0xFFu;
    const std::uint32_t scbr = static_cast<std::uint32_t>(g.scbr) << 10;
    const std::uint32_t off = scbr + (static_cast<std::uint32_t>(y) << 8) + x;
    if (off + 1u < sizeof g.ram) {
        g.ram[off] = static_cast<std::uint8_t>(color);
        g.ram[off + 1u] = static_cast<std::uint8_t>(color >> 8);
    }
    ++g.plotOps;
}

inline std::uint16_t rpixSample(Gsu& g, std::uint16_t x, std::uint16_t y) noexcept {
    const std::uint32_t scbr = static_cast<std::uint32_t>(g.scbr) << 10;
    const std::uint32_t off = scbr + (static_cast<std::uint32_t>(y) << 8) + x;
    if (off + 1u >= sizeof g.ram) return 0;
    return static_cast<std::uint16_t>(g.ram[off] | (static_cast<std::uint16_t>(g.ram[off + 1u]) << 8));
}

inline bool branch(State& s, std::uint8_t op, bool take) noexcept {
    if (!take) return true;
    const std::int8_t disp = static_cast<std::int8_t>(fetch8(s));
    s.gsu.pc = static_cast<std::uint16_t>(s.gsu.pc + static_cast<std::int16_t>(disp));
    return true;
}

inline void opPlot(State& s) noexcept {
    auto& g = s.gsu;
    const std::uint16_t x = reg(g, 1);
    const std::uint16_t y = reg(g, 2);
    plotPixel(g, x, y);
    reg(g, 1) = static_cast<std::uint16_t>(x + 1u);
}

inline void opRpix(State& s) noexcept {
    auto& g = s.gsu;
    dstReg(g) = rpixSample(g, reg(g, 1), reg(g, 2));
}

inline bool dispatch(State& s, std::uint8_t op) noexcept {
    auto& g = s.gsu;
    const std::uint8_t alt = (op & 0x80u) ? 1u : 0u;
    const std::uint8_t base = op & 0x7Fu;

    if (base <= 0x0Fu) {
        const int idx = base & 0x0F;
        if (alt) reg(g, idx) = fetchImm16(s);
        else reg(g, idx) = static_cast<std::uint16_t>(fetchImm8(s));
        return true;
    }
    if (base >= 0x10u && base <= 0x1Fu) {
        const int idx = base & 0x0F;
        if (alt) writeRam16(g, reg(g, idx), reg(g, idx));
        else writeRam8(g, static_cast<std::uint16_t>(reg(g, idx)), static_cast<std::uint8_t>(reg(g, idx)));
        return true;
    }
    if (base >= 0x20u && base <= 0x2Fu) {
        const int idx = base & 0x0F;
        if (alt) reg(g, idx) = readRam16(g, reg(g, idx));
        else reg(g, idx) = readRam8(g, static_cast<std::uint16_t>(reg(g, idx)));
        return true;
    }
    if (base >= 0x30u && base <= 0x3Fu) {
        const int idx = base & 0x0F;
        if (alt) {
            const std::uint16_t imm = fetchImm16(s);
            reg(g, idx) = readRom8(s, romAddr(g, imm));
        } else {
            const std::uint8_t imm = fetchImm8(s);
            reg(g, idx) = readRom8(s, romAddr(g, imm));
        }
        return true;
    }

    switch (base) {
    case 0x40: // PLOT / RPIX
        if (alt) opRpix(s);
        else opPlot(s);
        return true;
    case 0x49: // COLOR
        g.colr = srcReg(g);
        return true;
    case 0x4A: // NOT
        dstReg(g) = static_cast<std::uint16_t>(~srcReg(g));
        setSfrZ(g, dstReg(g));
        return true;
    case 0x4B: // ADD
        {
            const std::uint32_t sum = static_cast<std::uint32_t>(srcReg(g)) + dstReg(g);
            setSfrCy(g, sum > 0xFFFFu);
            dstReg(g) = static_cast<std::uint16_t>(sum);
            setSfrZ(g, dstReg(g));
        }
        return true;
    case 0x4C: // SUB
        {
            const std::uint32_t diff = static_cast<std::uint32_t>(dstReg(g)) - srcReg(g);
            setSfrCy(g, diff <= 0xFFFFu);
            dstReg(g) = static_cast<std::uint16_t>(diff);
            setSfrZ(g, dstReg(g));
        }
        return true;
    case 0x4D: // INC
        reg(g, base & 0x0F) = static_cast<std::uint16_t>(reg(g, base & 0x0F) + 1u);
        return true;
    case 0x4E: // ASR
        {
            std::uint16_t v = dstReg(g);
            setSfrCy(g, (v & 1u) != 0u);
            dstReg(g) = static_cast<std::uint16_t>((sfrCy(g.sfr) ? 0x8000u : 0u) | (v >> 1));
            setSfrZ(g, dstReg(g));
        }
        return true;
    case 0x4F: // ROR
        {
            std::uint16_t v = dstReg(g);
            const bool oldCy = sfrCy(g.sfr) != 0u;
            setSfrCy(g, (v & 1u) != 0u);
            dstReg(g) = static_cast<std::uint16_t>((oldCy ? 0x8000u : 0u) | (v >> 1));
            setSfrZ(g, dstReg(g));
        }
        return true;
    case 0x50: // JMP
        g.pc = srcReg(g);
        return true;
    case 0x51: // LJMP
        g.pc = fetchImm16(s);
        return true;
    case 0x54: // STOP
        clearGo(g);
        g.irq = true;
        return false;
    case 0x55: // NOP
        return true;
    case 0x56: // XOR
        dstReg(g) = static_cast<std::uint16_t>(dstReg(g) ^ srcReg(g));
        setSfrZ(g, dstReg(g));
        return true;
    case 0x58: // FROM
        g.sfr = static_cast<std::uint8_t>((g.sfr & 0xE3u) | ((base & 7u) << 2));
        return true;
    case 0x59: // TO
        g.sfr = static_cast<std::uint8_t>((g.sfr & 0x1Fu) | ((base & 7u) << 5));
        return true;
    case 0x5A: // WITH
        g.sfr = static_cast<std::uint8_t>((g.sfr & 0x03u) | ((base & 7u) << 2) | ((base & 7u) << 5));
        return true;
    case 0x5C: // BCC
        return branch(s, op, sfrCy(g.sfr) == 0u);
    case 0x5D: // BCS
        return branch(s, op, sfrCy(g.sfr) != 0u);
    case 0x5E: // BNE
        return branch(s, op, (g.sfr & 0x20u) == 0u);
    case 0x5F: // BEQ
        return branch(s, op, (g.sfr & 0x20u) != 0u);
    case 0x60: // BPL
        return branch(s, op, (dstReg(g) & 0x8000u) == 0u);
    case 0x61: // BMI
        return branch(s, op, (dstReg(g) & 0x8000u) != 0u);
    case 0x68: // CACHE
        g.cbr = static_cast<std::uint8_t>(srcReg(g) >> 9);
        return true;
    case 0x69: // LSR
        {
            std::uint16_t v = dstReg(g);
            setSfrCy(g, (v & 1u) != 0u);
            dstReg(g) = static_cast<std::uint16_t>(v >> 1);
            setSfrZ(g, dstReg(g));
        }
        return true;
    case 0x6A: // ROL
        {
            std::uint16_t v = dstReg(g);
            const bool oldCy = sfrCy(g.sfr) != 0u;
            setSfrCy(g, (v & 0x8000u) != 0u);
            dstReg(g) = static_cast<std::uint16_t>((v << 1) | (oldCy ? 1u : 0u));
            setSfrZ(g, dstReg(g));
        }
        return true;
    case 0x70: // MERGE
        {
            const std::uint16_t a = reg(g, 7);
            const std::uint16_t b = reg(g, 8);
            reg(g, 7) = static_cast<std::uint16_t>((a & 0xF0F0u) | ((b & 0xF0F0u) >> 4));
            reg(g, 8) = static_cast<std::uint16_t>(((a & 0x0F0Fu) << 4) | (b & 0x0F0Fu));
        }
        return true;
    case 0x90: // IWT
        reg(g, base & 0x0F) = fetchImm16(s);
        return true;
    case 0x94: // IBT
        reg(g, base & 0x0F) = static_cast<std::uint16_t>(fetchImm8(s));
        return true;
    case 0x98: // LINK
        g.sfr = static_cast<std::uint8_t>((g.sfr & 0xF3u) | ((base & 3u) << 2));
        return true;
    case 0x9A: // SEX
        dstReg(g) = static_cast<std::uint16_t>(static_cast<std::int16_t>(static_cast<std::int8_t>(dstReg(g) & 0xFFu)));
        return true;
    case 0x9B: // ASR / DIV2 family — use ASR fallback
        {
            std::uint16_t v = dstReg(g);
            setSfrCy(g, (v & 1u) != 0u);
            dstReg(g) = static_cast<std::uint16_t>((v & 0x8000u) | (v >> 1));
            setSfrZ(g, dstReg(g));
        }
        return true;
    case 0x9C: // RPIX alt path
        opRpix(s);
        return true;
    case 0x9D: // PLOT alt path
        opPlot(s);
        return true;
    default:
        ++g.pc;
        return true;
    }
}

} // namespace SuperFxDetail

inline void resetGsu(Gsu& g) noexcept {
    g = Gsu{};
    g.r[12] = 0x0000;
    g.r[13] = 0x0000;
    g.r[14] = 0x0000;
    g.r[15] = 0x0000;
    g.sp = 0;
    g.scmr = 0;
    g.scbr = 0xFC;
    g.colr = 0;
    g.por = 0;
    g.fro = 0;
    g.bramr = 0;
    g.vcr = 0;
    g.stepBudget = SuperFxDetail::kStepBudgetDefault;
    g.plotBudget = SuperFxDetail::kPlotBudgetDefault;
}

inline bool detectSuperFxRom(const std::uint8_t* data, std::size_t size) noexcept {
    if (!data || size < 0x8000) return false;
    for (std::size_t off = 0x7FC0; off + 0x40 < size; off += 0x8000) {
        if (std::memcmp(data + off + 0x10, "Super FX", 8) == 0) return true;
        if (std::memcmp(data + off + 0x10, "SUPER FX", 8) == 0) return true;
        if (std::memcmp(data + off + 0x10, "GSU-1", 5) == 0) return true;
        if (std::memcmp(data + off + 0x10, "GSU-2", 5) == 0) return true;
    }
    return false;
}

inline int gsuThermoBurst(const State& s, int thermoSteps) noexcept {
    if (!s.thermoGovernor) return s.gsu.maxBurst;
    return std::clamp(1 + thermoSteps, 1, s.gsu.maxBurst);
}

inline int stepGsu(State& s) noexcept {
    using namespace SuperFxDetail;
    auto& g = s.gsu;
    if (!SuperFxDetail::sfrGo(g.sfr)) return 0;
    const std::uint8_t op = fetch8(s);
    if (!dispatch(s, op)) return 0;
    return g.cycles;
}

inline int runGsuBatch(State& s, int maxSteps, int maxPlots) noexcept {
    using namespace SuperFxDetail;
    auto& g = s.gsu;
    if (!SuperFxDetail::sfrGo(g.sfr)) return 0;
    int steps = 0;
    const int plotStart = g.plotOps;
    while (steps < maxSteps && SuperFxDetail::sfrGo(g.sfr)) {
        const int before = g.cycles;
        if (!dispatch(s, fetch8(s))) break;
        steps += std::max(1, g.cycles - before);
        if (g.plotOps - plotStart >= maxPlots) break;
    }
    return steps;
}

inline void runGsuFrame(State& s, int thermoSteps = 1) noexcept {
    auto& g = s.gsu;
    if (!g.present || !SuperFxDetail::sfrGo(g.sfr)) return;
    const int burst = gsuThermoBurst(s, thermoSteps);
    const int stepCap = std::min(g.stepBudget * burst, 65536);
    const int plotCap = std::min(g.plotBudget * burst, 32768);
    runGsuBatch(s, stepCap, plotCap);
}

inline void gsuWriteReg(State& s, std::uint16_t addr, std::uint8_t v) noexcept {
    auto& g = s.gsu;
    const std::uint16_t rel = static_cast<std::uint16_t>(addr - SuperFxDetail::kRegBase);
    switch (rel) {
    case 0x00: g.sfr = v; break;
    case 0x01: g.scmr = v; break;
    case 0x02: g.colr = v; break;
    case 0x03: g.por = v; break;
    case 0x04: g.fro = v; break;
    case 0x05: g.scbr = v; break;
    case 0x06: g.bramr = v; break;
    case 0x07: g.vcr = v; break;
    case 0x08: g.romb = v; break;
    case 0x09: g.ramb = v; break;
    case 0x0C: g.cbr = v; break;
    case 0x0E:
        g.pc = static_cast<std::uint16_t>((g.pc & 0xFF00u) | v);
        break;
    case 0x0F:
        g.pc = static_cast<std::uint16_t>((g.pc & 0x00FFu) | (static_cast<std::uint16_t>(v) << 8));
        break;
    default:
        if (rel >= 0x10 && rel < 0x30) {
            const int ri = (rel - 0x10) >> 1;
            if ((rel & 1u) == 0u)
                g.r[ri] = static_cast<std::uint16_t>((g.r[ri] & 0xFF00u) | v);
            else
                g.r[ri] = static_cast<std::uint16_t>((g.r[ri] & 0x00FFu) | (static_cast<std::uint16_t>(v) << 8));
        }
        break;
    }
    if (rel == 0x00 && SuperFxDetail::sfrGo(v))
        g.cycles = 0;
}

inline std::uint8_t gsuReadReg(const State& s, std::uint16_t addr) noexcept {
    const auto& g = s.gsu;
    const std::uint16_t rel = static_cast<std::uint16_t>(addr - SuperFxDetail::kRegBase);
    switch (rel) {
    case 0x00: return g.sfr;
    case 0x01: return g.scmr;
    case 0x02: return g.colr;
    case 0x03: return g.por;
    case 0x04: return g.fro;
    case 0x05: return g.scbr;
    case 0x06: return g.bramr;
    case 0x07: return g.vcr;
    case 0x08: return g.romb;
    case 0x09: return g.ramb;
    case 0x0C: return g.cbr;
    case 0x0E: return static_cast<std::uint8_t>(g.pc & 0xFFu);
    case 0x0F: return static_cast<std::uint8_t>(g.pc >> 8);
    default:
        if (rel >= 0x10 && rel < 0x30) {
            const int ri = (rel - 0x10) >> 1;
            return (rel & 1u) == 0u
                ? static_cast<std::uint8_t>(g.r[ri] & 0xFFu)
                : static_cast<std::uint8_t>(g.r[ri] >> 8);
        }
        break;
    }
    return 0;
}

inline void bootGsuFromRom(State& s) noexcept {
    if (!s.gsu.present || s.cart.rom.size() < 0x8000) return;
    auto& g = s.gsu;
    const std::uint16_t vec = static_cast<std::uint16_t>(
        s.cart.rom[0x7FFC] | (static_cast<std::uint16_t>(s.cart.rom[0x7FFD]) << 8));
    g.pc = vec ? vec : 0x8000;
    g.romb = 0x00;
    g.scbr = 0x70;
    g.colr = 0xFF;
    SuperFxDetail::setGo(g);
}

} // namespace FieldChips::Snes