#pragma once

// Zilog Z80 — Golden Era workhorse (SMS, Genesis sound CPU, MSX, Spectrum).

#include "FieldChipTypes.hpp"

namespace FieldChips {

template<typename Ctx, typename Traits>
inline int stepZ80(Ctx& ctx) noexcept {
    auto& z = ctx.z80;
    if (z.halted) {
        z.cycles += 4;
        z.totalCycles += 4;
        return 4;
    }
    const std::uint8_t op = Traits::read(ctx, z.pc++);
    int cyc = 4;
    switch (op) {
    case 0x00: break;
    case 0x3E: z.a = Traits::read(ctx, z.pc++); cyc = 7; break;
    case 0x06: z.c = Traits::read(ctx, z.pc++); cyc = 7; break;
    case 0x0E: z.e = Traits::read(ctx, z.pc++); cyc = 7; break;
    case 0x16: z.d = Traits::read(ctx, z.pc++); cyc = 7; break;
    case 0x1E: z.e = Traits::read(ctx, z.pc++); cyc = 7; break;
    case 0x26: z.h = Traits::read(ctx, z.pc++); cyc = 7; break;
    case 0x2E: z.l = Traits::read(ctx, z.pc++); cyc = 7; break;
    case 0x36: Traits::write(ctx, z80Hl(z), Traits::read(ctx, z.pc++)); cyc = 10; break;
    case 0x7E: z.a = Traits::read(ctx, z80Hl(z)); cyc = 7; break;
    case 0x77: Traits::write(ctx, z80Hl(z), z.a); cyc = 7; break;
    case 0x21: {
        const std::uint8_t lo = Traits::read(ctx, z.pc++);
        z80SetHl(z, static_cast<std::uint16_t>(lo | (Traits::read(ctx, z.pc++) << 8)));
        cyc = 10; break;
    }
    case 0x31: {
        const std::uint8_t lo = Traits::read(ctx, z.pc++);
        z.sp = static_cast<std::uint16_t>(lo | (Traits::read(ctx, z.pc++) << 8));
        cyc = 10; break;
    }
    case 0x01: case 0x11: {
        const std::uint8_t lo = Traits::read(ctx, z.pc++);
        const std::uint16_t v = static_cast<std::uint16_t>(lo | (Traits::read(ctx, z.pc++) << 8));
        if (op == 0x01) z80SetBc(z, v); else z80SetDe(z, v);
        cyc = 10; break;
    }
    case 0xC3: {
        const std::uint8_t lo = Traits::read(ctx, z.pc++);
        z.pc = static_cast<std::uint16_t>(lo | (Traits::read(ctx, z.pc++) << 8));
        cyc = 10; break;
    }
    case 0x18: {
        const std::int8_t d = static_cast<std::int8_t>(Traits::read(ctx, z.pc++));
        z.pc = static_cast<std::uint16_t>(static_cast<std::int32_t>(z.pc) + d);
        cyc = 12; break;
    }
    case 0x32: {
        const std::uint16_t addr = static_cast<std::uint16_t>(
            Traits::read(ctx, z.pc++) | (Traits::read(ctx, z.pc++) << 8));
        Traits::write(ctx, addr, z.a);
        cyc = 13; break;
    }
    case 0x3A: {
        const std::uint16_t addr = static_cast<std::uint16_t>(
            Traits::read(ctx, z.pc++) | (Traits::read(ctx, z.pc++) << 8));
        z.a = Traits::read(ctx, addr);
        cyc = 13; break;
    }
    case 0xDB: {
        const std::uint8_t port = Traits::read(ctx, z.pc++);
        Traits::inPort(ctx, port);
        cyc = 12; break;
    }
    case 0xD3: {
        const std::uint8_t port = Traits::read(ctx, z.pc++);
        Traits::outPort(ctx, port, z.a);
        cyc = 11; break;
    }
    case 0x23: z80SetHl(z, static_cast<std::uint16_t>(z80Hl(z) + 1)); cyc = 6; break;
    case 0x2B: z80SetHl(z, static_cast<std::uint16_t>(z80Hl(z) - 1)); cyc = 6; break;
    case 0x13: ++z.sp; cyc = 6; break;
    case 0x1B: --z.sp; cyc = 6; break;
    case 0x76: z.halted = true; cyc = 4; break;
    case 0xFB: z.iff1 = z.iff2 = 1; cyc = 4; break;
    case 0xF3: z.iff1 = z.iff2 = 0; cyc = 4; break;
    case 0xC9: {
        const std::uint8_t lo = Traits::read(ctx, z.sp++);
        z.pc = static_cast<std::uint16_t>(lo | (Traits::read(ctx, z.sp++) << 8));
        cyc = 10; break;
    }
    case 0xCD: {
        const std::uint8_t lo = Traits::read(ctx, z.pc++);
        const std::uint16_t target = static_cast<std::uint16_t>(lo | (Traits::read(ctx, z.pc++) << 8));
        Traits::write(ctx, static_cast<std::uint16_t>(z.sp - 1), static_cast<std::uint8_t>(z.pc >> 8));
        Traits::write(ctx, static_cast<std::uint16_t>(z.sp - 2), static_cast<std::uint8_t>(z.pc));
        z.sp = static_cast<std::uint16_t>(z.sp - 2);
        z.pc = target;
        cyc = 17; break;
    }
    default: break;
    }
    z.cycles += cyc;
    z.totalCycles += cyc;
    return cyc;
}

} // namespace FieldChips