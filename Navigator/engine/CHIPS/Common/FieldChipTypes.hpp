#pragma once

// Shared silicon types — Golden Era CPUs and field-parallel execution hooks.

#include <cstdint>

namespace FieldChips {

enum class ChipId : std::uint8_t {
    Mos6502 = 0,
    Mos6507,
    Z80,
    Intel8080,
    Ricoh2A03,
    Ricoh2C02,
    Tia6532,
    Mos6532,
    Sn76489,
    Ym2612,
    M68000,
    Wdc65816,
    Sm83,
    Mos6581,
    Tms9918,
    VdpGenesis,
};

struct Cpu6502 {
    std::uint16_t pc = 0;
    std::uint8_t a = 0;
    std::uint8_t x = 0;
    std::uint8_t y = 0;
    std::uint8_t sp = 0xFD;
    std::uint8_t status = 0x24;
    int cycles = 0;
    int totalCycles = 0;
    bool stall = false;
    int stallCycles = 0;
};

struct Cpu6502Config {
    bool decimalMode = false;
    std::uint16_t addrMask = 0xFFFF;
    std::uint16_t irqVector = 0xFFFE;
    std::uint16_t nmiVector = 0xFFFA;
    std::uint16_t resetVector = 0xFFFC;
};

struct CpuZ80 {
    std::uint16_t pc = 0;
    std::uint16_t sp = 0;
    std::uint8_t a = 0, f = 0;
    std::uint8_t b = 0, c = 0, d = 0, e = 0, h = 0, l = 0;
    std::uint8_t a_ = 0, f_ = 0, b_ = 0, c_ = 0, d_ = 0, e_ = 0, h_ = 0, l_ = 0;
    std::uint8_t i = 0, r = 0;
    int iff1 = 0, iff2 = 0;
    int im = 0;
    bool halted = false;
    int cycles = 0;
    int totalCycles = 0;
};

struct Cpu8080 {
    std::uint16_t pc = 0;
    std::uint16_t sp = 0;
    std::uint8_t a = 0, b = 0, c = 0, d = 0, e = 0, h = 0, l = 0;
    std::uint8_t flags = 0x02;
    bool inte = false;
    int cycles = 0;
    int totalCycles = 0;
};

struct CpuM68000 {
    std::uint32_t d[8]{};
    std::uint32_t a[8]{};
    std::uint32_t pc = 0;
    std::uint16_t sr = 0x2700;
    int cycles = 0;
    int totalCycles = 0;
    bool stopped = false;
};

inline std::uint16_t z80Bc(const CpuZ80& z) noexcept {
    return static_cast<std::uint16_t>((z.b << 8) | z.c);
}
inline void z80SetBc(CpuZ80& z, std::uint16_t v) noexcept {
    z.b = static_cast<std::uint8_t>(v >> 8);
    z.c = static_cast<std::uint8_t>(v);
}
inline std::uint16_t z80De(const CpuZ80& z) noexcept {
    return static_cast<std::uint16_t>((z.d << 8) | z.e);
}
inline void z80SetDe(CpuZ80& z, std::uint16_t v) noexcept {
    z.d = static_cast<std::uint8_t>(v >> 8);
    z.e = static_cast<std::uint8_t>(v);
}
inline std::uint16_t z80Hl(const CpuZ80& z) noexcept {
    return static_cast<std::uint16_t>((z.h << 8) | z.l);
}
inline void z80SetHl(CpuZ80& z, std::uint16_t v) noexcept {
    z.h = static_cast<std::uint8_t>(v >> 8);
    z.l = static_cast<std::uint8_t>(v);
}
inline std::uint16_t z80Af(const CpuZ80& z) noexcept {
    return static_cast<std::uint16_t>((z.a << 8) | z.f);
}
inline void z80SetAf(CpuZ80& z, std::uint16_t v) noexcept {
    z.a = static_cast<std::uint8_t>(v >> 8);
    z.f = static_cast<std::uint8_t>(v & 0xFF);
}

inline std::uint16_t i8080Bc(const Cpu8080& i) noexcept {
    return static_cast<std::uint16_t>((i.b << 8) | i.c);
}
inline void i8080SetBc(Cpu8080& i, std::uint16_t v) noexcept {
    i.b = static_cast<std::uint8_t>(v >> 8);
    i.c = static_cast<std::uint8_t>(v);
}
inline std::uint16_t i8080De(const Cpu8080& i) noexcept {
    return static_cast<std::uint16_t>((i.d << 8) | i.e);
}
inline void i8080SetDe(Cpu8080& i, std::uint16_t v) noexcept {
    i.d = static_cast<std::uint8_t>(v >> 8);
    i.e = static_cast<std::uint8_t>(v);
}
inline std::uint16_t i8080Hl(const Cpu8080& i) noexcept {
    return static_cast<std::uint16_t>((i.h << 8) | i.l);
}
inline void i8080SetHl(Cpu8080& i, std::uint16_t v) noexcept {
    i.h = static_cast<std::uint8_t>(v >> 8);
    i.l = static_cast<std::uint8_t>(v);
}

inline void reset6502(Cpu6502& c, std::uint8_t sp = 0xFD, std::uint8_t status = 0x24) noexcept {
    c = Cpu6502{};
    c.sp = sp;
    c.status = status;
}

inline void resetZ80(CpuZ80& c) noexcept { c = CpuZ80{}; }

inline void reset8080(Cpu8080& c) noexcept {
    c = Cpu8080{};
    c.flags = 0x02;
}

inline void resetM68000(CpuM68000& c) noexcept { c = CpuM68000{}; }

} // namespace FieldChips