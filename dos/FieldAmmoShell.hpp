#pragma once

// Legacy alias — RTX-AMMOS shell lives in FieldRtxShell.hpp.

#include "FieldRtxAmmos.hpp"
#include "FieldRtxShell.hpp"

#include <x86emu.h>

namespace FieldAmmoShell {

using Era = FieldRtxAmmos::Era;
inline bool& graphicsActive = FieldRtxShell::graphicsActive;

inline void shellPrint(x86emu_t* e,
    void (*echo)(x86emu_t*, char),
    void (*newline)(x86emu_t*),
    const char* text) {
    (void)e;
    (void)echo;
    (void)newline;
    (void)text;
}

inline void paintWelcome(x86emu_t* e,
    void (*vgaPut)(x86emu_t*, std::uint16_t, std::uint8_t, std::uint8_t),
    void (*setCursor)(x86emu_t*, std::uint16_t)) {
    constexpr std::uint8_t attr = 0x02u;
    for (int row = 0; row < 25; ++row)
        for (int col = 0; col < 80; ++col)
            vgaPut(e, static_cast<std::uint16_t>(row * 80 + col), ' ', attr);
    static const char prompt[] = "C:\\>";
    const std::uint16_t base = static_cast<std::uint16_t>(24 * 80);
    for (int i = 0; i < 4; ++i)
        vgaPut(e, static_cast<std::uint16_t>(base + i),
            static_cast<std::uint8_t>(prompt[i]), attr);
    setCursor(e, static_cast<std::uint16_t>(base + 4));
}

inline void setTextMode(x86emu_t* e, std::uint8_t* guestRam) {
    (void)e;
    FieldRtxShell::setTextMode(guestRam);
}

inline void execLine(x86emu_t* e, const char* line, std::uint8_t* guestRam,
    void (*echo)(x86emu_t*, char),
    void (*newline)(x86emu_t*),
    void (*prompt)(x86emu_t*)) {
    (void)e;
    auto echoRam = [echo, e](std::uint8_t*, char c) { echo(e, c); };
    auto nlRam = [newline, e](std::uint8_t*) { newline(e); };
    auto promptRam = [prompt, e](std::uint8_t*) { prompt(e); };
    FieldRtxShell::execLine(line, guestRam, echoRam, nlRam, promptRam);
}

} // namespace FieldAmmoShell