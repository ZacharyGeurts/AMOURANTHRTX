#pragma once

// MOS 6502 — shared Golden Era CPU core (NES, Atari 2600/8-bit, Apple, Commodore, …).
// Platform buses plug in via Cpu6502Traits — no console labels on this die.

#include "FieldChipTypes.hpp"

namespace FieldChips {

inline bool flagC(const Cpu6502& c) noexcept { return (c.status & 0x01) != 0; }
inline bool flagZ(const Cpu6502& c) noexcept { return (c.status & 0x02) != 0; }
inline bool flagI(const Cpu6502& c) noexcept { return (c.status & 0x04) != 0; }
inline bool flagD(const Cpu6502& c) noexcept { return (c.status & 0x08) != 0; }
inline bool flagV(const Cpu6502& c) noexcept { return (c.status & 0x40) != 0; }
inline bool flagN(const Cpu6502& c) noexcept { return (c.status & 0x80) != 0; }

inline void setZ(Cpu6502& c, std::uint8_t v) noexcept {
    if (v == 0) c.status |= 0x02; else c.status &= ~0x02u;
}
inline void setN(Cpu6502& c, std::uint8_t v) noexcept {
    if (v & 0x80) c.status |= 0x80; else c.status &= ~0x80u;
}
inline void setC(Cpu6502& c, bool v) noexcept {
    if (v) c.status |= 0x01; else c.status &= ~0x01u;
}
inline void setV(Cpu6502& c, bool v) noexcept {
    if (v) c.status |= 0x40; else c.status &= ~0x40u;
}

template<typename Ctx, typename Traits>
inline std::uint8_t fetch6502(Ctx& ctx) noexcept {
    return Traits::read(ctx, static_cast<std::uint16_t>(ctx.cpu.pc++ & Traits::config().addrMask));
}
template<typename Ctx, typename Traits>
inline std::uint16_t fetch16_6502(Ctx& ctx) noexcept {
    const std::uint8_t lo = fetch6502<Ctx, Traits>(ctx);
    return static_cast<std::uint16_t>(lo | (static_cast<std::uint16_t>(fetch6502<Ctx, Traits>(ctx)) << 8));
}

template<typename Ctx, typename Traits>
inline void push6502(Ctx& ctx, std::uint8_t v) noexcept {
    Traits::write(ctx, static_cast<std::uint16_t>(0x0100 + ctx.cpu.sp--), v);
}
template<typename Ctx, typename Traits>
inline std::uint8_t pop6502(Ctx& ctx) noexcept {
    return Traits::read(ctx, static_cast<std::uint16_t>(0x0100 + ++ctx.cpu.sp));
}

template<typename Ctx, typename Traits>
inline void irq6502(Ctx& ctx) noexcept {
    if (flagI(ctx.cpu)) return;
    push6502<Ctx, Traits>(ctx, static_cast<std::uint8_t>((ctx.cpu.pc >> 8) & 0xFF));
    push6502<Ctx, Traits>(ctx, static_cast<std::uint8_t>(ctx.cpu.pc & 0xFF));
    push6502<Ctx, Traits>(ctx, static_cast<std::uint8_t>(ctx.cpu.status & ~0x10u));
    ctx.cpu.status |= 0x04;
    const std::uint16_t vec = Traits::config().irqVector;
    ctx.cpu.pc = static_cast<std::uint16_t>(
        Traits::read(ctx, vec) | (static_cast<std::uint16_t>(Traits::read(ctx, vec + 1)) << 8));
    ctx.cpu.cycles += 7;
    Traits::onIrqServiced(ctx);
}

template<typename Ctx, typename Traits>
inline void nmi6502(Ctx& ctx) noexcept {
    push6502<Ctx, Traits>(ctx, static_cast<std::uint8_t>((ctx.cpu.pc >> 8) & 0xFF));
    push6502<Ctx, Traits>(ctx, static_cast<std::uint8_t>(ctx.cpu.pc & 0xFF));
    push6502<Ctx, Traits>(ctx, static_cast<std::uint8_t>(ctx.cpu.status & ~0x10u));
    ctx.cpu.status |= 0x04;
    const std::uint16_t vec = Traits::config().nmiVector;
    ctx.cpu.pc = static_cast<std::uint16_t>(
        Traits::read(ctx, vec) | (static_cast<std::uint16_t>(Traits::read(ctx, vec + 1)) << 8));
    ctx.cpu.cycles += 7;
}

template<typename Ctx, typename Traits>
inline int step6502(Ctx& ctx) noexcept {
    if (ctx.cpu.stall) {
        if (ctx.cpu.stallCycles > 0) {
            --ctx.cpu.stallCycles;
            return 1;
        }
        ctx.cpu.stall = false;
    }
    if (Traits::irqPending(ctx)) {
        irq6502<Ctx, Traits>(ctx);
        Traits::clearIrq(ctx);
        return 7;
    }
    if (Traits::consumeNmi(ctx)) {
        nmi6502<Ctx, Traits>(ctx);
        return 7;
    }

    const std::uint8_t op = fetch6502<Ctx, Traits>(ctx);
    int cyc = 0;

    auto imm = [&]() -> std::uint8_t { return fetch6502<Ctx, Traits>(ctx); };
    auto zp = [&]() -> std::uint16_t { return fetch6502<Ctx, Traits>(ctx); };
    auto zpx = [&]() -> std::uint16_t { return static_cast<std::uint16_t>((fetch6502<Ctx, Traits>(ctx) + ctx.cpu.x) & 0xFF); };
    auto zpy = [&]() -> std::uint16_t { return static_cast<std::uint16_t>((fetch6502<Ctx, Traits>(ctx) + ctx.cpu.y) & 0xFF); };
    auto abs = [&]() -> std::uint16_t { return fetch16_6502<Ctx, Traits>(ctx); };
    auto abx = [&]() -> std::uint16_t {
        const std::uint16_t a = fetch16_6502<Ctx, Traits>(ctx);
        const std::uint16_t r = static_cast<std::uint16_t>(a + ctx.cpu.x);
        if ((a & 0xFF00) != (r & 0xFF00)) ++cyc;
        return r;
    };
    auto aby = [&]() -> std::uint16_t {
        const std::uint16_t a = fetch16_6502<Ctx, Traits>(ctx);
        const std::uint16_t r = static_cast<std::uint16_t>(a + ctx.cpu.y);
        if ((a & 0xFF00) != (r & 0xFF00)) ++cyc;
        return r;
    };
    auto ind = [&]() -> std::uint16_t {
        const std::uint16_t ptr = fetch16_6502<Ctx, Traits>(ctx);
        const std::uint16_t lo = Traits::read(ctx, ptr);
        const std::uint16_t hi = Traits::read(ctx, static_cast<std::uint16_t>((ptr & 0xFF00) | ((ptr + 1) & 0xFF)));
        return static_cast<std::uint16_t>(lo | (hi << 8));
    };
    auto idx = [&]() -> std::uint16_t {
        const std::uint8_t z = fetch6502<Ctx, Traits>(ctx);
        const std::uint16_t lo = Traits::read(ctx, static_cast<std::uint16_t>((z + ctx.cpu.x) & 0xFF));
        const std::uint16_t hi = Traits::read(ctx, static_cast<std::uint16_t>((z + ctx.cpu.x + 1) & 0xFF));
        return static_cast<std::uint16_t>(lo | (hi << 8));
    };
    auto idy = [&]() -> std::uint16_t {
        const std::uint8_t z = fetch6502<Ctx, Traits>(ctx);
        const std::uint16_t lo = Traits::read(ctx, z);
        const std::uint16_t hi = Traits::read(ctx, static_cast<std::uint16_t>((z + 1) & 0xFF));
        const std::uint16_t base = static_cast<std::uint16_t>(lo | (hi << 8));
        const std::uint16_t r = static_cast<std::uint16_t>(base + ctx.cpu.y);
        if ((base & 0xFF00) != (r & 0xFF00)) ++cyc;
        return r;
    };

    switch (op) {
    case 0x00: push6502<Ctx, Traits>(ctx, static_cast<std::uint8_t>((ctx.cpu.pc >> 8) & 0xFF)); push6502<Ctx, Traits>(ctx, static_cast<std::uint8_t>(ctx.cpu.pc & 0xFF)); push6502<Ctx, Traits>(ctx, ctx.cpu.status); ctx.cpu.status |= 0x04; ctx.cpu.pc = fetch16_6502<Ctx, Traits>(ctx); cyc = 7; break;
    case 0x10: { const std::int8_t r = static_cast<std::int8_t>(fetch6502<Ctx, Traits>(ctx)); if (!flagN(ctx.cpu)) { ctx.cpu.pc = static_cast<std::uint16_t>(ctx.cpu.pc + r); ++cyc; } cyc += 2; break; }
    case 0x30: { const std::int8_t r = static_cast<std::int8_t>(fetch6502<Ctx, Traits>(ctx)); if (flagN(ctx.cpu)) { ctx.cpu.pc = static_cast<std::uint16_t>(ctx.cpu.pc + r); ++cyc; } cyc += 2; break; }
    case 0x50: { const std::int8_t r = static_cast<std::int8_t>(fetch6502<Ctx, Traits>(ctx)); if (!flagV(ctx.cpu)) { ctx.cpu.pc = static_cast<std::uint16_t>(ctx.cpu.pc + r); ++cyc; } cyc += 2; break; }
    case 0x70: { const std::int8_t r = static_cast<std::int8_t>(fetch6502<Ctx, Traits>(ctx)); if (flagV(ctx.cpu)) { ctx.cpu.pc = static_cast<std::uint16_t>(ctx.cpu.pc + r); ++cyc; } cyc += 2; break; }
    case 0x90: { const std::int8_t r = static_cast<std::int8_t>(fetch6502<Ctx, Traits>(ctx)); if (!flagC(ctx.cpu)) { ctx.cpu.pc = static_cast<std::uint16_t>(ctx.cpu.pc + r); ++cyc; } cyc += 2; break; }
    case 0xB0: { const std::int8_t r = static_cast<std::int8_t>(fetch6502<Ctx, Traits>(ctx)); if (flagC(ctx.cpu)) { ctx.cpu.pc = static_cast<std::uint16_t>(ctx.cpu.pc + r); ++cyc; } cyc += 2; break; }
    case 0xD0: { const std::int8_t r = static_cast<std::int8_t>(fetch6502<Ctx, Traits>(ctx)); if (!flagZ(ctx.cpu)) { ctx.cpu.pc = static_cast<std::uint16_t>(ctx.cpu.pc + r); ++cyc; } cyc += 2; break; }
    case 0xF0: { const std::int8_t r = static_cast<std::int8_t>(fetch6502<Ctx, Traits>(ctx)); if (flagZ(ctx.cpu)) { ctx.cpu.pc = static_cast<std::uint16_t>(ctx.cpu.pc + r); ++cyc; } cyc += 2; break; }
    case 0x18: ctx.cpu.status &= ~0x01u; cyc = 2; break;
    case 0x38: ctx.cpu.status |= 0x01; cyc = 2; break;
    case 0x58: ctx.cpu.status &= ~0x04u; cyc = 2; break;
    case 0x78: ctx.cpu.status |= 0x04; cyc = 2; break;
    case 0xB8: ctx.cpu.status &= ~0x40u; cyc = 2; break;
    case 0xD8: ctx.cpu.status &= ~0x08u; cyc = 2; break;
    case 0xF8: ctx.cpu.status |= 0x08; cyc = 2; break;
    case 0xEA: cyc = 2; break;
    case 0x08: push6502<Ctx, Traits>(ctx, static_cast<std::uint8_t>(ctx.cpu.status | 0x10)); cyc = 3; break;
    case 0x28: ctx.cpu.status = static_cast<std::uint8_t>((pop6502<Ctx, Traits>(ctx) & ~0x10) | 0x20); cyc = 4; break;
    case 0x48: push6502<Ctx, Traits>(ctx, ctx.cpu.a); cyc = 3; break;
    case 0x68: ctx.cpu.a = pop6502<Ctx, Traits>(ctx); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 4; break;
    case 0xAA: ctx.cpu.x = ctx.cpu.a; setZ(ctx.cpu, ctx.cpu.x); setN(ctx.cpu, ctx.cpu.x); cyc = 2; break;
    case 0xA8: ctx.cpu.y = ctx.cpu.a; setZ(ctx.cpu, ctx.cpu.y); setN(ctx.cpu, ctx.cpu.y); cyc = 2; break;
    case 0x8A: ctx.cpu.a = ctx.cpu.x; setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 2; break;
    case 0x98: ctx.cpu.a = ctx.cpu.y; setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 2; break;
    case 0xBA: ctx.cpu.x = ctx.cpu.sp; setZ(ctx.cpu, ctx.cpu.x); setN(ctx.cpu, ctx.cpu.x); cyc = 2; break;
    case 0x9A: ctx.cpu.sp = ctx.cpu.x; cyc = 2; break;
    case 0xCA: --ctx.cpu.x; setZ(ctx.cpu, ctx.cpu.x); setN(ctx.cpu, ctx.cpu.x); cyc = 2; break;
    case 0x88: --ctx.cpu.y; setZ(ctx.cpu, ctx.cpu.y); setN(ctx.cpu, ctx.cpu.y); cyc = 2; break;
    case 0xE8: ++ctx.cpu.x; setZ(ctx.cpu, ctx.cpu.x); setN(ctx.cpu, ctx.cpu.x); cyc = 2; break;
    case 0xC8: ++ctx.cpu.y; setZ(ctx.cpu, ctx.cpu.y); setN(ctx.cpu, ctx.cpu.y); cyc = 2; break;
    case 0x4C: ctx.cpu.pc = abs(); cyc = 3; break;
    case 0x6C: ctx.cpu.pc = ind(); cyc = 5; break;
    case 0x20: { const std::uint16_t t = fetch16_6502<Ctx, Traits>(ctx); push6502<Ctx, Traits>(ctx, static_cast<std::uint8_t>((ctx.cpu.pc >> 8) & 0xFF)); push6502<Ctx, Traits>(ctx, static_cast<std::uint8_t>(ctx.cpu.pc & 0xFF)); ctx.cpu.pc = t; cyc = 6; break; }
    case 0x60: ctx.cpu.pc = static_cast<std::uint16_t>(pop6502<Ctx, Traits>(ctx) | (static_cast<std::uint16_t>(pop6502<Ctx, Traits>(ctx)) << 8)); ++ctx.cpu.pc; cyc = 6; break;
    case 0x40: ctx.cpu.status = static_cast<std::uint8_t>((pop6502<Ctx, Traits>(ctx) & ~0x10) | 0x20); ctx.cpu.pc = static_cast<std::uint16_t>(pop6502<Ctx, Traits>(ctx) | (static_cast<std::uint16_t>(pop6502<Ctx, Traits>(ctx)) << 8)); ++ctx.cpu.pc; cyc = 6; break;
    case 0xA9: ctx.cpu.a = imm(); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 2; break;
    case 0xA2: ctx.cpu.x = imm(); setZ(ctx.cpu, ctx.cpu.x); setN(ctx.cpu, ctx.cpu.x); cyc = 2; break;
    case 0xA0: ctx.cpu.y = imm(); setZ(ctx.cpu, ctx.cpu.y); setN(ctx.cpu, ctx.cpu.y); cyc = 2; break;
    case 0xA5: ctx.cpu.a = Traits::read(ctx, zp()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 3; break;
    case 0xB5: ctx.cpu.a = Traits::read(ctx, zpx()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 4; break;
    case 0xAD: ctx.cpu.a = Traits::read(ctx, abs()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 4; break;
    case 0xBD: ctx.cpu.a = Traits::read(ctx, abx()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc += 4; break;
    case 0xB9: ctx.cpu.a = Traits::read(ctx, aby()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc += 4; break;
    case 0xA1: ctx.cpu.a = Traits::read(ctx, idx()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 6; break;
    case 0xB1: ctx.cpu.a = Traits::read(ctx, idy()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc += 5; break;
    case 0x85: Traits::write(ctx, zp(), ctx.cpu.a); cyc = 3; break;
    case 0x95: Traits::write(ctx, zpx(), ctx.cpu.a); cyc = 4; break;
    case 0x8D: Traits::write(ctx, abs(), ctx.cpu.a); cyc = 4; break;
    case 0x9D: Traits::write(ctx, abx(), ctx.cpu.a); cyc = 5; break;
    case 0x99: Traits::write(ctx, aby(), ctx.cpu.a); cyc = 5; break;
    case 0x81: Traits::write(ctx, idx(), ctx.cpu.a); cyc = 6; break;
    case 0x91: Traits::write(ctx, idy(), ctx.cpu.a); cyc = 6; break;
    case 0xA6: ctx.cpu.x = Traits::read(ctx, zp()); setZ(ctx.cpu, ctx.cpu.x); setN(ctx.cpu, ctx.cpu.x); cyc = 3; break;
    case 0xB6: ctx.cpu.x = Traits::read(ctx, zpy()); setZ(ctx.cpu, ctx.cpu.x); setN(ctx.cpu, ctx.cpu.x); cyc = 4; break;
    case 0xAE: ctx.cpu.x = Traits::read(ctx, abs()); setZ(ctx.cpu, ctx.cpu.x); setN(ctx.cpu, ctx.cpu.x); cyc = 4; break;
    case 0x86: Traits::write(ctx, zp(), ctx.cpu.x); cyc = 3; break;
    case 0x96: Traits::write(ctx, zpy(), ctx.cpu.x); cyc = 4; break;
    case 0x8E: Traits::write(ctx, abs(), ctx.cpu.x); cyc = 4; break;
    case 0xA4: ctx.cpu.y = Traits::read(ctx, zp()); setZ(ctx.cpu, ctx.cpu.y); setN(ctx.cpu, ctx.cpu.y); cyc = 3; break;
    case 0xB4: ctx.cpu.y = Traits::read(ctx, zpx()); setZ(ctx.cpu, ctx.cpu.y); setN(ctx.cpu, ctx.cpu.y); cyc = 4; break;
    case 0xAC: ctx.cpu.y = Traits::read(ctx, abs()); setZ(ctx.cpu, ctx.cpu.y); setN(ctx.cpu, ctx.cpu.y); cyc = 4; break;
    case 0x84: Traits::write(ctx, zp(), ctx.cpu.y); cyc = 3; break;
    case 0x94: Traits::write(ctx, zpx(), ctx.cpu.y); cyc = 4; break;
    case 0x8C: Traits::write(ctx, abs(), ctx.cpu.y); cyc = 4; break;
    case 0x29: ctx.cpu.a &= imm(); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 2; break;
    case 0x25: ctx.cpu.a &= Traits::read(ctx, zp()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 3; break;
    case 0x35: ctx.cpu.a &= Traits::read(ctx, zpx()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 4; break;
    case 0x2D: ctx.cpu.a &= Traits::read(ctx, abs()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 4; break;
    case 0x3D: ctx.cpu.a &= Traits::read(ctx, abx()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc += 4; break;
    case 0x39: ctx.cpu.a &= Traits::read(ctx, aby()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc += 4; break;
    case 0x21: ctx.cpu.a &= Traits::read(ctx, idx()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 6; break;
    case 0x31: ctx.cpu.a &= Traits::read(ctx, idy()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc += 5; break;
    case 0x09: ctx.cpu.a |= imm(); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 2; break;
    case 0x05: ctx.cpu.a |= Traits::read(ctx, zp()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 3; break;
    case 0x15: ctx.cpu.a |= Traits::read(ctx, zpx()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 4; break;
    case 0x0D: ctx.cpu.a |= Traits::read(ctx, abs()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 4; break;
    case 0x1D: ctx.cpu.a |= Traits::read(ctx, abx()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc += 4; break;
    case 0x19: ctx.cpu.a |= Traits::read(ctx, aby()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc += 4; break;
    case 0x01: ctx.cpu.a |= Traits::read(ctx, idx()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 6; break;
    case 0x11: ctx.cpu.a |= Traits::read(ctx, idy()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc += 5; break;
    case 0x49: ctx.cpu.a ^= imm(); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 2; break;
    case 0x45: ctx.cpu.a ^= Traits::read(ctx, zp()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 3; break;
    case 0x55: ctx.cpu.a ^= Traits::read(ctx, zpx()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 4; break;
    case 0x4D: ctx.cpu.a ^= Traits::read(ctx, abs()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 4; break;
    case 0x5D: ctx.cpu.a ^= Traits::read(ctx, abx()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc += 4; break;
    case 0x59: ctx.cpu.a ^= Traits::read(ctx, aby()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc += 4; break;
    case 0x41: ctx.cpu.a ^= Traits::read(ctx, idx()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 6; break;
    case 0x51: ctx.cpu.a ^= Traits::read(ctx, idy()); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc += 5; break;
    case 0xE9: { const std::uint8_t m = imm(); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + 0x100 - m; setC(ctx.cpu, r >= 0x100); ctx.cpu.a = static_cast<std::uint8_t>(r); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 2; break; }
    case 0x69: { const std::uint8_t m = imm(); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + m + (flagC(ctx.cpu) ? 1 : 0); setC(ctx.cpu, r > 0xFF); ctx.cpu.a = static_cast<std::uint8_t>(r); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 2; break; }
    case 0xE5: { const std::uint8_t m = Traits::read(ctx, zp()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + 0x100 - m; setC(ctx.cpu, r >= 0x100); ctx.cpu.a = static_cast<std::uint8_t>(r); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 3; break; }
    case 0xF5: { const std::uint8_t m = Traits::read(ctx, zpx()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + 0x100 - m; setC(ctx.cpu, r >= 0x100); ctx.cpu.a = static_cast<std::uint8_t>(r); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 4; break; }
    case 0xED: { const std::uint8_t m = Traits::read(ctx, abs()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + 0x100 - m; setC(ctx.cpu, r >= 0x100); ctx.cpu.a = static_cast<std::uint8_t>(r); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 4; break; }
    case 0xFD: { const std::uint8_t m = Traits::read(ctx, abx()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + 0x100 - m; setC(ctx.cpu, r >= 0x100); ctx.cpu.a = static_cast<std::uint8_t>(r); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc += 4; break; }
    case 0xF9: { const std::uint8_t m = Traits::read(ctx, aby()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + 0x100 - m; setC(ctx.cpu, r >= 0x100); ctx.cpu.a = static_cast<std::uint8_t>(r); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc += 4; break; }
    case 0xE1: { const std::uint8_t m = Traits::read(ctx, idx()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + 0x100 - m; setC(ctx.cpu, r >= 0x100); ctx.cpu.a = static_cast<std::uint8_t>(r); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 6; break; }
    case 0xF1: { const std::uint8_t m = Traits::read(ctx, idy()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + 0x100 - m; setC(ctx.cpu, r >= 0x100); ctx.cpu.a = static_cast<std::uint8_t>(r); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc += 5; break; }
    case 0x65: { const std::uint8_t m = Traits::read(ctx, zp()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + m + (flagC(ctx.cpu) ? 1 : 0); setC(ctx.cpu, r > 0xFF); ctx.cpu.a = static_cast<std::uint8_t>(r); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 3; break; }
    case 0x75: { const std::uint8_t m = Traits::read(ctx, zpx()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + m + (flagC(ctx.cpu) ? 1 : 0); setC(ctx.cpu, r > 0xFF); ctx.cpu.a = static_cast<std::uint8_t>(r); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 4; break; }
    case 0x6D: { const std::uint8_t m = Traits::read(ctx, abs()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + m + (flagC(ctx.cpu) ? 1 : 0); setC(ctx.cpu, r > 0xFF); ctx.cpu.a = static_cast<std::uint8_t>(r); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 4; break; }
    case 0x7D: { const std::uint8_t m = Traits::read(ctx, abx()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + m + (flagC(ctx.cpu) ? 1 : 0); setC(ctx.cpu, r > 0xFF); ctx.cpu.a = static_cast<std::uint8_t>(r); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc += 4; break; }
    case 0x79: { const std::uint8_t m = Traits::read(ctx, aby()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + m + (flagC(ctx.cpu) ? 1 : 0); setC(ctx.cpu, r > 0xFF); ctx.cpu.a = static_cast<std::uint8_t>(r); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc += 4; break; }
    case 0x61: { const std::uint8_t m = Traits::read(ctx, idx()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + m + (flagC(ctx.cpu) ? 1 : 0); setC(ctx.cpu, r > 0xFF); ctx.cpu.a = static_cast<std::uint8_t>(r); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 6; break; }
    case 0x71: { const std::uint8_t m = Traits::read(ctx, idy()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + m + (flagC(ctx.cpu) ? 1 : 0); setC(ctx.cpu, r > 0xFF); ctx.cpu.a = static_cast<std::uint8_t>(r); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc += 5; break; }
    case 0x24: { const std::uint8_t m = Traits::read(ctx, zp()); setZ(ctx.cpu, static_cast<std::uint8_t>(ctx.cpu.a & m)); setN(ctx.cpu, m); setV(ctx.cpu, (m & 0x40) != 0); cyc = 3; break; }
    case 0x2C: { const std::uint8_t m = Traits::read(ctx, abs()); setZ(ctx.cpu, static_cast<std::uint8_t>(ctx.cpu.a & m)); setN(ctx.cpu, m); setV(ctx.cpu, (m & 0x40) != 0); cyc = 4; break; }
    case 0xC9: { const std::uint8_t m = imm(); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + 0x100 - m; setC(ctx.cpu, r >= 0x100); setZ(ctx.cpu, static_cast<std::uint8_t>(r)); setN(ctx.cpu, static_cast<std::uint8_t>(r)); cyc = 2; break; }
    case 0xE0: { const std::uint8_t m = imm(); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.x) + 0x100 - m; setC(ctx.cpu, r >= 0x100); setZ(ctx.cpu, static_cast<std::uint8_t>(r)); setN(ctx.cpu, static_cast<std::uint8_t>(r)); cyc = 2; break; }
    case 0xC0: { const std::uint8_t m = imm(); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.y) + 0x100 - m; setC(ctx.cpu, r >= 0x100); setZ(ctx.cpu, static_cast<std::uint8_t>(r)); setN(ctx.cpu, static_cast<std::uint8_t>(r)); cyc = 2; break; }
    case 0xC5: { const std::uint8_t m = Traits::read(ctx, zp()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + 0x100 - m; setC(ctx.cpu, r >= 0x100); setZ(ctx.cpu, static_cast<std::uint8_t>(r)); setN(ctx.cpu, static_cast<std::uint8_t>(r)); cyc = 3; break; }
    case 0xCD: { const std::uint8_t m = Traits::read(ctx, abs()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + 0x100 - m; setC(ctx.cpu, r >= 0x100); setZ(ctx.cpu, static_cast<std::uint8_t>(r)); setN(ctx.cpu, static_cast<std::uint8_t>(r)); cyc = 4; break; }
    case 0xD5: { const std::uint8_t m = Traits::read(ctx, zpx()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + 0x100 - m; setC(ctx.cpu, r >= 0x100); setZ(ctx.cpu, static_cast<std::uint8_t>(r)); setN(ctx.cpu, static_cast<std::uint8_t>(r)); cyc = 4; break; }
    case 0xDD: { const std::uint8_t m = Traits::read(ctx, abx()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + 0x100 - m; setC(ctx.cpu, r >= 0x100); setZ(ctx.cpu, static_cast<std::uint8_t>(r)); setN(ctx.cpu, static_cast<std::uint8_t>(r)); cyc += 4; break; }
    case 0xD9: { const std::uint8_t m = Traits::read(ctx, aby()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + 0x100 - m; setC(ctx.cpu, r >= 0x100); setZ(ctx.cpu, static_cast<std::uint8_t>(r)); setN(ctx.cpu, static_cast<std::uint8_t>(r)); cyc += 4; break; }
    case 0xC1: { const std::uint8_t m = Traits::read(ctx, idx()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + 0x100 - m; setC(ctx.cpu, r >= 0x100); setZ(ctx.cpu, static_cast<std::uint8_t>(r)); setN(ctx.cpu, static_cast<std::uint8_t>(r)); cyc = 6; break; }
    case 0xD1: { const std::uint8_t m = Traits::read(ctx, idy()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.a) + 0x100 - m; setC(ctx.cpu, r >= 0x100); setZ(ctx.cpu, static_cast<std::uint8_t>(r)); setN(ctx.cpu, static_cast<std::uint8_t>(r)); cyc += 5; break; }
    case 0xE4: { const std::uint8_t m = Traits::read(ctx, zp()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.x) + 0x100 - m; setC(ctx.cpu, r >= 0x100); setZ(ctx.cpu, static_cast<std::uint8_t>(r)); setN(ctx.cpu, static_cast<std::uint8_t>(r)); cyc = 3; break; }
    case 0xEC: { const std::uint8_t m = Traits::read(ctx, abs()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.x) + 0x100 - m; setC(ctx.cpu, r >= 0x100); setZ(ctx.cpu, static_cast<std::uint8_t>(r)); setN(ctx.cpu, static_cast<std::uint8_t>(r)); cyc = 4; break; }
    case 0xC4: { const std::uint8_t m = Traits::read(ctx, zp()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.y) + 0x100 - m; setC(ctx.cpu, r >= 0x100); setZ(ctx.cpu, static_cast<std::uint8_t>(r)); setN(ctx.cpu, static_cast<std::uint8_t>(r)); cyc = 3; break; }
    case 0xCC: { const std::uint8_t m = Traits::read(ctx, abs()); const std::uint16_t r = static_cast<std::uint16_t>(ctx.cpu.y) + 0x100 - m; setC(ctx.cpu, r >= 0x100); setZ(ctx.cpu, static_cast<std::uint8_t>(r)); setN(ctx.cpu, static_cast<std::uint8_t>(r)); cyc = 4; break; }
    case 0x06: { const std::uint16_t a = zp(); std::uint8_t v = Traits::read(ctx, a); setC(ctx.cpu, (v & 0x80) != 0); v = static_cast<std::uint8_t>(v << 1); Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc = 5; break; }
    case 0x16: { const std::uint16_t a = zpx(); std::uint8_t v = Traits::read(ctx, a); setC(ctx.cpu, (v & 0x80) != 0); v = static_cast<std::uint8_t>(v << 1); Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc = 6; break; }
    case 0x0E: { const std::uint16_t a = abs(); std::uint8_t v = Traits::read(ctx, a); setC(ctx.cpu, (v & 0x80) != 0); v = static_cast<std::uint8_t>(v << 1); Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc = 6; break; }
    case 0x1E: { const std::uint16_t a = abx(); std::uint8_t v = Traits::read(ctx, a); setC(ctx.cpu, (v & 0x80) != 0); v = static_cast<std::uint8_t>(v << 1); Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc += 6; break; }
    case 0x46: { const std::uint16_t a = zp(); std::uint8_t v = Traits::read(ctx, a); setC(ctx.cpu, (v & 1) != 0); v >>= 1; Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc = 5; break; }
    case 0x56: { const std::uint16_t a = zpx(); std::uint8_t v = Traits::read(ctx, a); setC(ctx.cpu, (v & 1) != 0); v >>= 1; Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc = 6; break; }
    case 0x4E: { const std::uint16_t a = abs(); std::uint8_t v = Traits::read(ctx, a); setC(ctx.cpu, (v & 1) != 0); v >>= 1; Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc = 6; break; }
    case 0x5E: { const std::uint16_t a = abx(); std::uint8_t v = Traits::read(ctx, a); setC(ctx.cpu, (v & 1) != 0); v >>= 1; Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc += 6; break; }
    case 0x26: { const std::uint16_t a = zp(); std::uint8_t v = Traits::read(ctx, a); const bool carry = flagC(ctx.cpu); setC(ctx.cpu, (v & 0x80) != 0); v = static_cast<std::uint8_t>((v << 1) | (carry ? 1 : 0)); Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc = 5; break; }
    case 0x36: { const std::uint16_t a = zpx(); std::uint8_t v = Traits::read(ctx, a); const bool carry = flagC(ctx.cpu); setC(ctx.cpu, (v & 0x80) != 0); v = static_cast<std::uint8_t>((v << 1) | (carry ? 1 : 0)); Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc = 6; break; }
    case 0x2E: { const std::uint16_t a = abs(); std::uint8_t v = Traits::read(ctx, a); const bool carry = flagC(ctx.cpu); setC(ctx.cpu, (v & 0x80) != 0); v = static_cast<std::uint8_t>((v << 1) | (carry ? 1 : 0)); Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc = 6; break; }
    case 0x3E: { const std::uint16_t a = abx(); std::uint8_t v = Traits::read(ctx, a); const bool carry = flagC(ctx.cpu); setC(ctx.cpu, (v & 0x80) != 0); v = static_cast<std::uint8_t>((v << 1) | (carry ? 1 : 0)); Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc += 6; break; }
    case 0x66: { const std::uint16_t a = zp(); std::uint8_t v = Traits::read(ctx, a); const bool carry = flagC(ctx.cpu); setC(ctx.cpu, (v & 1) != 0); v = static_cast<std::uint8_t>((v >> 1) | (carry ? 0x80 : 0)); Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc = 5; break; }
    case 0x76: { const std::uint16_t a = zpx(); std::uint8_t v = Traits::read(ctx, a); const bool carry = flagC(ctx.cpu); setC(ctx.cpu, (v & 1) != 0); v = static_cast<std::uint8_t>((v >> 1) | (carry ? 0x80 : 0)); Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc = 6; break; }
    case 0x6E: { const std::uint16_t a = abs(); std::uint8_t v = Traits::read(ctx, a); const bool carry = flagC(ctx.cpu); setC(ctx.cpu, (v & 1) != 0); v = static_cast<std::uint8_t>((v >> 1) | (carry ? 0x80 : 0)); Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc = 6; break; }
    case 0x7E: { const std::uint16_t a = abx(); std::uint8_t v = Traits::read(ctx, a); const bool carry = flagC(ctx.cpu); setC(ctx.cpu, (v & 1) != 0); v = static_cast<std::uint8_t>((v >> 1) | (carry ? 0x80 : 0)); Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc += 6; break; }
    case 0xBE: ctx.cpu.x = Traits::read(ctx, aby()); setZ(ctx.cpu, ctx.cpu.x); setN(ctx.cpu, ctx.cpu.x); cyc += 4; break;
    case 0xBC: ctx.cpu.y = Traits::read(ctx, abx()); setZ(ctx.cpu, ctx.cpu.y); setN(ctx.cpu, ctx.cpu.y); cyc += 4; break;
    case 0xE6: { const std::uint16_t a = zp(); std::uint8_t v = Traits::read(ctx, a); ++v; Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc = 5; break; }
    case 0xF6: { const std::uint16_t a = zpx(); std::uint8_t v = Traits::read(ctx, a); ++v; Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc = 6; break; }
    case 0xC6: { const std::uint16_t a = zp(); std::uint8_t v = Traits::read(ctx, a); --v; Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc = 5; break; }
    case 0xD6: { const std::uint16_t a = zpx(); std::uint8_t v = Traits::read(ctx, a); --v; Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc = 6; break; }
    case 0xEE: { const std::uint16_t a = abs(); std::uint8_t v = Traits::read(ctx, a); ++v; Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc = 6; break; }
    case 0xFE: { const std::uint16_t a = abx(); std::uint8_t v = Traits::read(ctx, a); ++v; Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc += 6; break; }
    case 0xCE: { const std::uint16_t a = abs(); std::uint8_t v = Traits::read(ctx, a); --v; Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc = 6; break; }
    case 0xDE: { const std::uint16_t a = abx(); std::uint8_t v = Traits::read(ctx, a); --v; Traits::write(ctx, a, v); setZ(ctx.cpu, v); setN(ctx.cpu, v); cyc += 6; break; }
    case 0x0A: { setC(ctx.cpu, (ctx.cpu.a & 0x80) != 0); ctx.cpu.a <<= 1; setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 2; break; }
    case 0x4A: { setC(ctx.cpu, (ctx.cpu.a & 1) != 0); ctx.cpu.a >>= 1; setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 2; break; }
    case 0x2A: { const bool carry = flagC(ctx.cpu); setC(ctx.cpu, (ctx.cpu.a & 0x80) != 0); ctx.cpu.a = static_cast<std::uint8_t>((ctx.cpu.a << 1) | (carry ? 1 : 0)); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 2; break; }
    case 0x6A: { const bool carry = flagC(ctx.cpu); setC(ctx.cpu, (ctx.cpu.a & 1) != 0); ctx.cpu.a = static_cast<std::uint8_t>((ctx.cpu.a >> 1) | (carry ? 0x80 : 0)); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 2; break; }
    // undocumented LAX/SAX family
    case 0xA7: { const std::uint16_t a = zp(); ctx.cpu.a = ctx.cpu.x = Traits::read(ctx, a); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 3; break; }
    case 0xB7: { const std::uint16_t a = zpy(); ctx.cpu.a = ctx.cpu.x = Traits::read(ctx, a); setZ(ctx.cpu, ctx.cpu.a); setN(ctx.cpu, ctx.cpu.a); cyc = 4; break; }
    case 0x87: { const std::uint16_t a = zp(); Traits::write(ctx, a, static_cast<std::uint8_t>(ctx.cpu.a & ctx.cpu.x)); cyc = 3; break; }
    case 0x97: { const std::uint16_t a = zpy(); Traits::write(ctx, a, static_cast<std::uint8_t>(ctx.cpu.a & ctx.cpu.x)); cyc = 4; break; }
    default: cyc = 2; break;
    }

    if (cyc == 0) cyc = 2;
    ctx.cpu.cycles += cyc;
    ctx.cpu.totalCycles += cyc;
    return cyc;
}

} // namespace FieldChips
