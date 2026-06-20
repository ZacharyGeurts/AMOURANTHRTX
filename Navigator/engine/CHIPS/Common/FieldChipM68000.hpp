#pragma once

// Motorola 68000 — Genesis main CPU subset.

#include "FieldChipTypes.hpp"

namespace FieldChips {

template<typename Ctx, typename Traits>
inline int stepM68000(Ctx& ctx) noexcept {
    auto& m = ctx.m68k;
    if (m.stopped) {
        m.cycles += 4;
        m.totalCycles += 4;
        return 4;
    }
    const std::uint16_t op = static_cast<std::uint16_t>(
        (Traits::read8(ctx, m.pc) << 8) | Traits::read8(ctx, m.pc + 1));
    int cyc = 4;
    switch (op >> 12) {
    case 0x0:
        if ((op & 0xFF00) == 0x0000) { m.pc += 4; cyc = 16; }
        break;
    case 0x4:
        if (op == 0x4E71) { m.pc += 2; cyc = 4; }
        else if (op == 0x4E75) { m.pc += 2; cyc = 16; }
        else if ((op & 0xFFC0) == 0x4E80) { m.pc += 2; cyc = 20; }
        else m.pc += 2;
        break;
    case 0x6: {
        const std::int8_t disp = static_cast<std::int8_t>(op & 0xFF);
        m.pc += 2;
        if (disp != 0)
            m.pc = static_cast<std::uint32_t>(static_cast<std::int32_t>(m.pc) + disp);
        else {
            const std::int16_t wd = static_cast<std::int16_t>(
                (Traits::read8(ctx, m.pc) << 8) | Traits::read8(ctx, m.pc + 1));
            m.pc += 2;
            m.pc = static_cast<std::uint32_t>(static_cast<std::int32_t>(m.pc) + wd);
        }
        cyc = 10;
        break;
    }
    default:
        if ((op & 0xF000) == 0x1000 || (op & 0xF000) == 0x3000) { m.pc += 2; cyc = 8; }
        else if ((op & 0xF000) == 0x2000) { m.pc += 2; cyc = 12; }
        else { m.pc += 2; cyc = 4; }
        break;
    }
    m.cycles += cyc;
    m.totalCycles += cyc;
    return cyc;
}

} // namespace FieldChips