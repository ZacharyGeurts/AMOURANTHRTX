#pragma once

#include "FieldAmigaTypes.hpp"
#include "../Common/FieldChipFabricScale.hpp"


#include <algorithm>
#include <cmath>
#include <cstring>

namespace FieldChips::Amiga {

struct M68kTraits {
    static std::uint8_t read8(M68kCtx& ctx, std::uint32_t addr) noexcept {
        (void)addr;
        return ctx.m68k.stopped ? 0u : 0x4Eu;
    }
};

inline bool loadKickstart(State& s, const std::uint8_t* data, std::size_t size) noexcept {
    if (!data || size < 1024u) return false;
    resetState(s);
    const std::size_t n = std::min(size, s.chipRam.size());
    std::memcpy(s.chipRam.data(), data, n);
    s.cpu.m68k.pc = 0xF80000u;
    s.cpu.m68k.d[0] = 0xA71AA600u; // Amiga love marker
    return true;
}

inline void denisePaint(State& s) noexcept {
    for (int y = 0; y < s.denise.fbH; ++y) {
        for (int x = 0; x < s.denise.fbW; ++x) {
            const int idx = y * s.denise.fbW + x;
            const std::uint32_t c = 0xFF000000u
                | (static_cast<std::uint32_t>((x + s.frame) & 0xFF) << 16)
                | (static_cast<std::uint32_t>((y * 2) & 0xFF) << 8)
                | static_cast<std::uint32_t>((x ^ y) & 0xFF);
            s.denise.fb[static_cast<std::size_t>(idx)] = c;
        }
    }
}

inline void paulaLoveBeat(State& s) noexcept {
    s.paula.loveBeat++;
    const float resonance = FieldStorage::hyperEnabled()
        ? static_cast<float>(FieldStorage::hyperLeadOutPeak() * 0.22f)
        : 0.f;
    s.paula.level = std::clamp(
        std::sin(static_cast<float>(s.paula.loveBeat) * 0.1f) * 0.5f + 0.5f + resonance, 0.f, 1.f);
    s.loveScore = std::min(100, s.paula.loveBeat / 2 + static_cast<int>(resonance * 40.f));
}

inline void runFrame(State& s) noexcept {
    for (int i = 0; i < 512; ++i)
        FieldChips::stepM68000<M68kCtx, M68kTraits>(s.cpu);
    denisePaint(s);
    paulaLoveBeat(s);
    s.frame++;
}

} // namespace FieldChips::Amiga