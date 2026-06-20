#pragma once

// Intel 8080 — Golden Era foundation (arcade, CP/M, early consoles).

#include "FieldChipTypes.hpp"

namespace FieldChips {

inline void set8080Parity(Cpu8080& c) noexcept {
    const std::uint8_t n = static_cast<std::uint8_t>(c.a ^ c.a);
    if ((n & 1) == 0) c.flags |= 0x04; else c.flags &= ~0x04u;
}

template<typename Ctx, typename Traits>
inline int step8080(Ctx& ctx) noexcept {
    auto& i = ctx.i8080;
    const std::uint8_t op = Traits::read(ctx, i.pc++);
    int cyc = 5;
    switch (op) {
    case 0x00: cyc = 4; break;
    case 0x3E: i.a = Traits::read(ctx, i.pc++); cyc = 7; break;
    case 0x06: i.b = Traits::read(ctx, i.pc++); cyc = 7; break;
    case 0x0E: i.c = Traits::read(ctx, i.pc++); cyc = 7; break;
    case 0x21: {
        const std::uint8_t lo = Traits::read(ctx, i.pc++);
        i8080SetHl(i, static_cast<std::uint16_t>(lo | (Traits::read(ctx, i.pc++) << 8)));
        cyc = 10; break;
    }
    case 0x31: {
        const std::uint8_t lo = Traits::read(ctx, i.pc++);
        i.sp = static_cast<std::uint16_t>(lo | (Traits::read(ctx, i.pc++) << 8));
        cyc = 10; break;
    }
    case 0xC3: {
        const std::uint8_t lo = Traits::read(ctx, i.pc++);
        i.pc = static_cast<std::uint16_t>(lo | (Traits::read(ctx, i.pc++) << 8));
        cyc = 10; break;
    }
    case 0x7E: i.a = Traits::read(ctx, i8080Hl(i)); cyc = 7; break;
    case 0x77: Traits::write(ctx, i8080Hl(i), i.a); cyc = 7; break;
    default: break;
    }
    i.cycles += cyc;
    i.totalCycles += cyc;
    return cyc;
}

} // namespace FieldChips