#pragma once

// Host-side 8086 real-mode CPU for RTX-DOS — shares FieldX86Die SSBO with GPU field fabric.
// GPU handles VGA compositing + thermo/RTX; host runs the guest at full speed.

#include "FieldSpeaker.hpp"

#include <cstdint>
#include <cstring>

namespace FieldCpuHost {

constexpr std::uint32_t EFLAGS_HALTED = 0x40000000u;
constexpr std::uint32_t DISK_BASE     = 0x00020000u;
constexpr std::uint32_t DISK_BYTES    = 737280u;
constexpr std::uint32_t VGA_BASE      = 0x000B8000u;
constexpr std::uint32_t BOOT_VECTOR   = 0x00007C00u;

struct Ctx {
    std::uint32_t key     = 0;
    bool          keyDown = false;
    std::uint64_t ticks   = 0;
};

constexpr std::size_t kRamByteOffset = 39u * sizeof(std::uint32_t);
constexpr std::size_t kCycleOffset =
    (39u + (0x100000u / 4u) + 1024u + 1024u + 2048u + 2048u + 1024u) * sizeof(std::uint32_t);

struct Die {
    std::uint8_t* base = nullptr;
    std::size_t   ramOff = kRamByteOffset;

    std::uint32_t regVal(std::size_t i) const {
        return *reinterpret_cast<const std::uint32_t*>(base + i * 4);
    }
    std::uint32_t& reg(std::size_t i) {
        return *reinterpret_cast<std::uint32_t*>(base + i * 4);
    }
    std::uint32_t& EAX() { return reg(0); }
    std::uint32_t& EBX() { return reg(1); }
    std::uint32_t& ECX() { return reg(2); }
    std::uint32_t& EDX() { return reg(3); }
    std::uint32_t& ESI() { return reg(4); }
    std::uint32_t& EDI() { return reg(5); }
    std::uint32_t& EBP() { return reg(6); }
    std::uint32_t& ESP() { return reg(7); }
    std::uint32_t& CS()  { return reg(16); }
    std::uint32_t& DS()  { return reg(17); }
    std::uint32_t& ES()  { return reg(18); }
    std::uint32_t& SS()  { return reg(20); }
    std::uint32_t& EIP() { return reg(22); }
    std::uint32_t& EFLAGS() { return reg(23); }
    std::uint32_t& cycle_count() {
        return *reinterpret_cast<std::uint32_t*>(base + kCycleOffset);
    }
    std::uint32_t& video_mode() {
        return *reinterpret_cast<std::uint32_t*>(base + kCycleOffset + 8);
    }

    std::uint8_t rb(std::uint32_t a) const {
        a &= 0xFFFFFu;
        return base[ramOff + a];
    }
    void wb(std::uint32_t a, std::uint8_t v) {
        a &= 0xFFFFFu;
        base[ramOff + a] = v;
    }
    std::uint16_t rw(std::uint32_t a) const {
        return static_cast<std::uint16_t>(rb(a) | (rb(a + 1) << 8));
    }
    void ww(std::uint32_t a, std::uint16_t v) {
        wb(a, static_cast<std::uint8_t>(v & 0xFF));
        wb(a + 1, static_cast<std::uint8_t>(v >> 8));
    }
    static std::uint32_t lin(std::uint16_t seg, std::uint16_t off) {
        return ((seg & 0xFFFFu) << 4) + (off & 0xFFFFu);
    }
};

inline bool cf(const Die& d) { return (d.regVal(23) & 1u) != 0; }
inline void setCF(Die& d, bool v) {
    if (v) d.EFLAGS() |= 1u; else d.EFLAGS() &= ~1u;
}
inline void setZF(Die& d, std::uint16_t v) {
    if (v == 0) d.EFLAGS() |= 0x40u; else d.EFLAGS() &= ~0x40u;
}
inline void setSF(Die& d, std::uint16_t v) {
    if (v & 0x8000u) d.EFLAGS() |= 0x80u; else d.EFLAGS() &= ~0x80u;
}
inline bool pf(std::uint16_t v) {
    v &= 0xFF;
    int c = 0;
    for (int i = 0; i < 8; ++i) if (v & (1 << i)) ++c;
    return (c & 1) == 0;
}
inline void setPF(Die& d, std::uint16_t v) {
    if (pf(v)) d.EFLAGS() |= 4u; else d.EFLAGS() &= ~4u;
}

inline std::uint16_t r16(const Die& d, int r) {
    switch (r & 7) {
        case 0: return static_cast<std::uint16_t>(d.regVal(0));
        case 1: return static_cast<std::uint16_t>(d.regVal(2));
        case 2: return static_cast<std::uint16_t>(d.regVal(3));
        case 3: return static_cast<std::uint16_t>(d.regVal(1));
        case 4: return static_cast<std::uint16_t>(d.regVal(7));
        case 5: return static_cast<std::uint16_t>(d.regVal(6));
        case 6: return static_cast<std::uint16_t>(d.regVal(4));
        default: return static_cast<std::uint16_t>(d.regVal(5));
    }
}
inline void w16(Die& d, int r, std::uint16_t v) {
    std::uint32_t m = 0xFFFF0000u;
    switch (r & 7) {
        case 0: d.EAX() = (d.EAX() & m) | v; break;
        case 1: d.ECX() = (d.ECX() & m) | v; break;
        case 2: d.EDX() = (d.EDX() & m) | v; break;
        case 3: d.EBX() = (d.EBX() & m) | v; break;
        case 4: d.ESP() = (d.ESP() & m) | v; break;
        case 5: d.EBP() = (d.EBP() & m) | v; break;
        case 6: d.ESI() = (d.ESI() & m) | v; break;
        default: d.EDI() = (d.EDI() & m) | v; break;
    }
}
inline std::uint16_t segVal(const Die& d, int s) {
    switch (s & 3) {
        case 0: return static_cast<std::uint16_t>(d.regVal(18));
        case 1: return static_cast<std::uint16_t>(d.regVal(16));
        case 2: return static_cast<std::uint16_t>(d.regVal(20));
        default: return static_cast<std::uint16_t>(d.regVal(17));
    }
}
inline void setSeg(Die& d, int s, std::uint16_t v) {
    switch (s & 3) {
        case 0: d.ES() = v; break;
        case 1: d.CS() = v; break;
        case 2: d.SS() = v; break;
        default: d.DS() = v; break;
    }
}

inline void push16(Die& d, std::uint16_t v) {
    std::uint16_t sp = static_cast<std::uint16_t>(d.ESP() - 2);
    d.ESP() = sp;
    d.ww(Die::lin(static_cast<std::uint16_t>(d.SS()), sp), v);
}
inline std::uint16_t pop16(Die& d) {
    std::uint16_t sp = static_cast<std::uint16_t>(d.ESP());
    std::uint16_t v = d.rw(Die::lin(static_cast<std::uint16_t>(d.SS()), sp));
    d.ESP() = sp + 2;
    return v;
}

struct EA {
    bool isReg = false;
    int reg = 0;
    std::uint16_t off = 0;
    std::uint32_t addr = 0;
};

inline EA decodeEA(Die& d, std::uint8_t modrm, std::uint16_t dataSeg) {
    EA ea{};
    const int mod = modrm >> 6;
    ea.reg = (modrm >> 3) & 7;
    const int rm = modrm & 7;
    ea.isReg = mod == 3;
    if (ea.isReg) return ea;
    std::uint16_t off = 0;
    auto fetchS8 = [&]() -> int {
        std::uint8_t u = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++)));
        return (u & 0x80) ? static_cast<int>(u) - 256 : static_cast<int>(u);
    };
    auto fetch16 = [&]() -> std::uint16_t {
        std::uint16_t lo = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++)));
        std::uint16_t hi = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++)));
        return static_cast<std::uint16_t>(lo | (hi << 8));
    };
    if (rm == 0) off = static_cast<std::uint16_t>(r16(d, 3) + r16(d, 6));
    else if (rm == 1) off = static_cast<std::uint16_t>(r16(d, 3) + r16(d, 7));
    else if (rm == 2) off = static_cast<std::uint16_t>(r16(d, 5) + r16(d, 6));
    else if (rm == 3) off = static_cast<std::uint16_t>(r16(d, 5) + r16(d, 7));
    else if (rm == 4) off = r16(d, 6);
    else if (rm == 5) off = r16(d, 7);
    else if (rm == 6) {
        if (mod == 0) off = fetch16();
        else {
            off = r16(d, 5);
            if (mod == 1) off = static_cast<std::uint16_t>(off + fetchS8());
            else off = static_cast<std::uint16_t>(off + fetch16());
        }
    } else off = r16(d, 3);
    if (rm != 6 || mod != 0) {
        if (mod == 1) off = static_cast<std::uint16_t>(off + fetchS8());
        else if (mod == 2) off = static_cast<std::uint16_t>(off + fetch16());
    }
    ea.off = off;
    ea.addr = Die::lin(dataSeg, off);
    return ea;
}

inline void vgaPut(Die& d, std::uint16_t col, std::uint8_t ch, std::uint8_t attr) {
    const std::uint32_t off = VGA_BASE + col * 2u;
    d.wb(off, ch);
    d.wb(off + 1, attr);
}

inline std::uint32_t floppyOff(std::uint16_t cyl, std::uint16_t head, std::uint16_t sector) {
    if (sector < 1 || sector > 18) return 0xFFFFFFFFu;
    return DISK_BASE + (cyl * 18u + head * 9u + (sector - 1u)) * 512u;
}

inline void int10(Die& d) {
    const std::uint8_t ah = static_cast<std::uint8_t>(d.EAX() >> 8);
    if (ah == 0x00) {
        d.video_mode() = d.EAX() & 0xFF;
        return;
    }
    if (ah == 0x02) {
        const std::uint16_t pos = static_cast<std::uint16_t>(
            ((d.EDX() >> 8) & 0xFF) * 80u + (d.EDX() & 0xFF));
        d.ECX() = (d.ECX() & 0xFFFF0000u) | pos;
        return;
    }
    if (ah == 0x03) {
        d.EDX() = (d.EDX() & 0xFFFF0000u)
                | ((d.ECX() / 80u) << 8) | (d.ECX() % 80u);
        return;
    }
    if (ah == 0x06) {
        const std::uint8_t attr = static_cast<std::uint8_t>(d.EBX() >> 8);
        const std::uint8_t ch = static_cast<std::uint8_t>(d.ECX() >> 8);
        const std::uint8_t cl = static_cast<std::uint8_t>(d.ECX());
        const std::uint8_t dh = static_cast<std::uint8_t>(d.EDX() >> 8);
        const std::uint8_t dl = static_cast<std::uint8_t>(d.EDX());
        for (int row = ch; row <= dh; ++row)
            for (int col = cl; col <= dl; ++col)
                vgaPut(d, static_cast<std::uint16_t>(row * 80 + col), ' ', attr);
        return;
    }
    if (ah == 0x0E) {
        std::uint16_t pos = static_cast<std::uint16_t>(d.ECX() & 0xFFFF);
        vgaPut(d, pos, static_cast<std::uint8_t>(d.EAX()), 0x07);
        ++pos;
        if ((pos % 80) == 0) pos += 80;
        d.ECX() = (d.ECX() & 0xFFFF0000u) | pos;
        return;
    }
    if (ah == 0x4F) { setCF(d, true); return; }
}

inline void int12(Die& d) {
    d.EAX() = (d.EAX() & 0xFFFF0000u) | 0x0280u; // 640 KiB conventional
}

inline void int13(Die& d) {
    const std::uint8_t ah = static_cast<std::uint8_t>(d.EAX() >> 8);
    if (ah == 0x00) { setCF(d, false); return; }
    if (ah == 0x02) {
        const std::uint8_t n = static_cast<std::uint8_t>(d.EAX());
        const std::uint16_t head = static_cast<std::uint16_t>(d.EDX() & 0xFF);
        std::uint16_t cyl = static_cast<std::uint16_t>(((d.ECX() >> 8) & 0xFF) | ((d.ECX() & 0xC0u) << 2));
        std::uint16_t sector = static_cast<std::uint16_t>(d.ECX() & 0x3Fu);
        if (n == 0 || sector == 0) { setCF(d, true); return; }
        const std::uint32_t dst = Die::lin(static_cast<std::uint16_t>(d.ES()),
            static_cast<std::uint16_t>(d.EBX()));
        for (std::uint8_t i = 0; i < n; ++i) {
            const std::uint32_t src = floppyOff(cyl, head, static_cast<std::uint16_t>(sector + i));
            if (src == 0xFFFFFFFFu || src + 512 > DISK_BASE + DISK_BYTES) {
                setCF(d, true);
                return;
            }
            for (int b = 0; b < 512; ++b)
                d.wb(dst + i * 512u + static_cast<std::uint32_t>(b), d.rb(src + b));
        }
        d.EAX() = (d.EAX() & 0xFFFFFF00u) | n;
        setCF(d, false);
        return;
    }
    if (ah == 0x08) {
        d.ECX() = (d.ECX() & 0xFFFF00FFu) | (0x4909u << 8);
        d.EDX() = (d.EDX() & 0xFFFFFF00u) | 0x01u;
        d.EBX() = (d.EBX() & 0xFFFF0000u) | 0x0003u;
        setCF(d, false);
        return;
    }
    if (ah == 0x41 || ah == 0x48) { setCF(d, true); return; }
    setCF(d, true);
}

inline void int15(Die& d) {
    const std::uint8_t ah = static_cast<std::uint8_t>(d.EAX() >> 8);
    if (ah == 0x88) {
        d.EAX() &= 0xFFFF0000u;
        setCF(d, false);
        return;
    }
    setCF(d, true);
}

inline void int16(Die& d, const Ctx& ctx) {
    const std::uint8_t ah = static_cast<std::uint8_t>(d.EAX() >> 8);
    if (ah == 0x00 || ah == 0x10) {
        std::uint8_t key = static_cast<std::uint8_t>(ctx.key & 0xFF);
        if (key == 0) key = static_cast<std::uint8_t>((ctx.key >> 8) & 0xFF);
        d.EAX() = (d.EAX() & 0xFFFF0000u) | key;
        if (key != 0) d.EAX() |= 0x0100u;
        return;
    }
    if (ah == 0x01) {
        if (ctx.keyDown) d.EAX() |= 0x0100u;
        else d.EAX() &= 0xFFFF00FFu;
        return;
    }
}

inline void int1A(Die& d, Ctx& ctx) {
    const std::uint8_t ah = static_cast<std::uint8_t>(d.EAX() >> 8);
    if (ah == 0x00) {
        const std::uint32_t t = static_cast<std::uint32_t>(ctx.ticks & 0xFFFFFFFFu);
        d.ECX() = (d.ECX() & 0xFFFF0000u) | ((t >> 16) & 0xFFFF);
        d.EDX() = (d.EDX() & 0xFFFF0000u) | (t & 0xFFFF);
        setCF(d, false);
        return;
    }
    setCF(d, true);
}

inline void int19(Die& d) {
    d.EIP() = BOOT_VECTOR;
    d.CS() = 0;
    d.EFLAGS() &= ~EFLAGS_HALTED;
}

inline void dispatchInt(Die& d, std::uint8_t n, Ctx& ctx) {
    if (n == 0x10) int10(d);
    else if (n == 0x12) int12(d);
    else if (n == 0x13) int13(d);
    else if (n == 0x15) int15(d);
    else if (n == 0x16) int16(d, ctx);
    else if (n == 0x1A) int1A(d, ctx);
    else if (n == 0x19) int19(d);
    else {
        push16(d, static_cast<std::uint16_t>(d.EFLAGS()));
        d.EFLAGS() &= ~0x200u;
        push16(d, static_cast<std::uint16_t>(d.CS()));
        push16(d, static_cast<std::uint16_t>(d.EIP()));
        const std::uint32_t iv = n * 4u;
        d.EIP() = d.rw(iv);
        d.CS() = d.rw(iv + 2);
    }
}

inline void portOut(Die& d, std::uint16_t port, std::uint8_t val) {
    FieldSpeaker::portOut(port, val);
    (void)d;
}
inline std::uint8_t portIn(Die& d, std::uint16_t port) {
    (void)d;
    return FieldSpeaker::portIn(port);
}

inline void illegal(Die& d, std::uint8_t op) {
    d.EIP() = (d.EIP() - 1u) & 0xFFFFu;
    d.EFLAGS() |= EFLAGS_HALTED;
    d.EDX() = (d.EDX() & 0xFFFF0000u) | static_cast<std::uint32_t>(op);
}

inline void execOne(Die& d, Ctx& ctx) {
    if (d.EFLAGS() & EFLAGS_HALTED) return;

    const std::uint16_t dataSeg0 = static_cast<std::uint16_t>(d.DS());
    int segOv = -1;
    bool rep = false;
    std::uint8_t op = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
        static_cast<std::uint16_t>(d.EIP()++)));

    for (int guard = 0; guard < 8; ++guard) {
        if (op == 0xF3u) { rep = true; op = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++))); continue; }
        if (op == 0xF2u) { op = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++))); continue; }
        if (op == 0x66u || op == 0x67u || op == 0xF0u) {
            op = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
                static_cast<std::uint16_t>(d.EIP()++)));
            continue;
        }
        if (op == 0x26u) { segOv = 0; op = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++))); continue; }
        if (op == 0x2Eu) { segOv = 1; op = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++))); continue; }
        if (op == 0x36u) { segOv = 2; op = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++))); continue; }
        if (op == 0x3Eu) { segOv = 3; op = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++))); continue; }
        break;
    }
    const std::uint16_t dataSeg = segOv < 0 ? dataSeg0 : segVal(d, segOv);

    auto fetchS8 = [&]() -> int {
        std::uint8_t u = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++)));
        return (u & 0x80) ? static_cast<int>(u) - 256 : static_cast<int>(u);
    };
    auto fetch16 = [&]() -> std::uint16_t {
        std::uint16_t lo = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++)));
        std::uint16_t hi = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++)));
        return static_cast<std::uint16_t>(lo | (hi << 8));
    };

    auto doShift = [&](std::uint16_t v, int cnt, int which) -> std::uint16_t {
        cnt &= 31;
        if (cnt == 0) return v;
        if (which == 4 || which == 6) { // shl/shl
            const bool oldCF = (v & (1 << (16 - cnt))) != 0;
            v = static_cast<std::uint16_t>(v << cnt);
            setCF(d, oldCF);
        } else if (which == 5 || which == 7) { // shr
            const bool oldCF = (v & (1 << (cnt - 1))) != 0;
            v = static_cast<std::uint16_t>(v >> cnt);
            setCF(d, oldCF);
        } else if (which == 7) { // sar
            const bool oldCF = (v & (1 << (cnt - 1))) != 0;
            v = static_cast<std::uint16_t>(static_cast<std::int16_t>(v) >> cnt);
            setCF(d, oldCF);
        }
        setZF(d, v); setSF(d, v); setPF(d, v);
        return v;
    };

    if (op == 0x90) {
    } else if (op >= 0x91 && op <= 0x97) {
        const int r = static_cast<int>(op - 0x90);
        const std::uint16_t a = r16(d, 0);
        const std::uint16_t b = r16(d, r);
        w16(d, 0, b);
        w16(d, r, a);
    } else if (op == 0xF4) {
        d.EFLAGS() |= EFLAGS_HALTED;
    } else if (op == 0xCD) {
        dispatchInt(d, d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++))), ctx);
    } else if (op == 0xC2) {
        const std::uint16_t imm = fetch16();
        d.ESP() = static_cast<std::uint16_t>(r16(d, 4) + imm);
        d.EIP() = pop16(d);
    } else if (op == 0xC3) {
        d.EIP() = pop16(d);
    } else if (op == 0xCA) {
        const std::uint16_t imm = fetch16();
        d.ESP() = static_cast<std::uint16_t>(r16(d, 4) + imm);
        d.EIP() = pop16(d);
        d.CS() = pop16(d);
    } else if (op == 0xCB) {
        d.EIP() = pop16(d);
        d.CS() = pop16(d);
    } else if (op == 0xCF) {
        d.EIP() = pop16(d);
        d.CS() = pop16(d);
        d.EFLAGS() = (d.EFLAGS() & 0xFFFF0000u) | pop16(d);
    } else if (op == 0xEB) {
        d.EIP() = static_cast<std::uint16_t>(static_cast<std::int16_t>(d.EIP()) + fetchS8());
    } else if (op == 0xE9) {
        d.EIP() = static_cast<std::uint16_t>(static_cast<std::int16_t>(d.EIP()) + static_cast<std::int16_t>(fetch16()));
    } else if (op == 0xE8) {
        const std::uint16_t ret = static_cast<std::uint16_t>(d.EIP());
        const std::int16_t rel = static_cast<std::int16_t>(fetch16());
        push16(d, ret);
        d.EIP() = static_cast<std::uint16_t>(static_cast<std::int16_t>(ret) + rel);
    } else if (op == 0xEA) {
        const std::uint16_t off = fetch16();
        const std::uint16_t seg = fetch16();
        d.EIP() = off;
        d.CS() = seg;
    } else if (op == 0x9A) {
        const std::uint16_t off = fetch16();
        const std::uint16_t seg = fetch16();
        push16(d, static_cast<std::uint16_t>(d.CS()));
        push16(d, static_cast<std::uint16_t>(d.EIP()));
        d.EIP() = off;
        d.CS() = seg;
    } else if (op >= 0x70 && op <= 0x7F) {
        const int rel = fetchS8();
        bool take = false;
        const bool zf = (d.EFLAGS() & 0x40) != 0;
        const bool cf2 = cf(d);
        const bool sf = (d.EFLAGS() & 0x80) != 0;
        const bool of = (d.EFLAGS() & 0x800) != 0;
        switch (op) {
            case 0x70: take = of; break;
            case 0x71: take = !of; break;
            case 0x72: take = cf2; break;
            case 0x73: take = !cf2; break;
            case 0x74: take = zf; break;
            case 0x75: take = !zf; break;
            case 0x76: take = cf2 || zf; break;
            case 0x77: take = !cf2 && !zf; break;
            case 0x78: take = sf; break;
            case 0x79: take = !sf; break;
            case 0x7A: take = sf != of; break;
            case 0x7B: take = sf == of; break;
            case 0x7C: take = !zf && (sf != of); break;
            case 0x7D: take = zf || (sf == of); break;
            case 0x7E: take = zf || (sf != of); break;
            case 0x7F: take = !zf && (sf == of); break;
        }
        if (take) d.EIP() = static_cast<std::uint16_t>(static_cast<std::int16_t>(d.EIP()) + rel);
    } else if (op >= 0xB0 && op <= 0xB7) {
        const int r = op - 0xB0;
        w16(d, r, static_cast<std::uint16_t>((r16(d, r) & 0xFF00) | d.rb(Die::lin(
            static_cast<std::uint16_t>(d.CS()), static_cast<std::uint16_t>(d.EIP()++)))));
    } else if (op >= 0xB8 && op <= 0xBF) {
        w16(d, op - 0xB8, fetch16());
    } else if (op >= 0x50 && op <= 0x57) {
        push16(d, r16(d, op - 0x50));
    } else if (op >= 0x58 && op <= 0x5F) {
        w16(d, op - 0x58, pop16(d));
    } else if (op >= 0x40 && op <= 0x47) {
        const int r = op - 0x40;
        const std::uint16_t v = static_cast<std::uint16_t>(r16(d, r) + 1);
        w16(d, r, v); setZF(d, v); setSF(d, v); setPF(d, v);
    } else if (op >= 0x48 && op <= 0x4F) {
        const int r = op - 0x48;
        const std::uint16_t v = static_cast<std::uint16_t>(r16(d, r) - 1);
        w16(d, r, v); setZF(d, v); setSF(d, v); setPF(d, v);
    } else if (op == 0x98) {
        const std::int8_t al = static_cast<std::int8_t>(d.EAX() & 0xFF);
        w16(d, 0, static_cast<std::uint16_t>(static_cast<std::int16_t>(al)));
    } else if (op == 0x99) {
        const std::int16_t ax = static_cast<std::int16_t>(r16(d, 0));
        d.EDX() = (d.EDX() & 0xFFFF0000u) | ((ax < 0) ? 0xFFFFu : 0u);
    } else if (op == 0x9C) {
        push16(d, static_cast<std::uint16_t>(d.EFLAGS()));
    } else if (op == 0x9D) {
        d.EFLAGS() = (d.EFLAGS() & 0xFFFF0000u) | pop16(d);
    } else if (op == 0xF8) setCF(d, false);
    else if (op == 0xF9) setCF(d, true);
    else if (op == 0xFC) { }
    else if (op == 0xFD) d.EFLAGS() |= 0x400u;
    else if (op == 0xFA) d.EFLAGS() &= ~0x200u;
    else if (op == 0xFB) d.EFLAGS() |= 0x200u;
    else if (op == 0x1E) push16(d, static_cast<std::uint16_t>(d.DS()));
    else if (op == 0x1F) d.DS() = pop16(d);
    else if (op == 0x06) push16(d, static_cast<std::uint16_t>(d.ES()));
    else if (op == 0x07) d.ES() = pop16(d);
    else if (op == 0x0E) push16(d, static_cast<std::uint16_t>(d.CS()));
    else if (op == 0x16) push16(d, static_cast<std::uint16_t>(d.SS()));
    else if (op == 0x17) d.SS() = pop16(d);
    else if (op == 0x1A) d.DS() = pop16(d);
    else if (op == 0xE0 || op == 0xE1 || op == 0xE2) {
        const int rel = fetchS8();
        d.ECX() = (d.ECX() & 0xFFFF0000u) | static_cast<std::uint16_t>(r16(d, 1) - 1);
        if (r16(d, 1) != 0) d.EIP() = static_cast<std::uint16_t>(static_cast<std::int16_t>(d.EIP()) + rel);
    } else if (op == 0xE4) {
        const std::uint8_t port = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++)));
        d.EAX() = (d.EAX() & 0xFFFFFF00u) | portIn(d, port);
    } else if (op == 0xE5) {
        const std::uint16_t port = fetch16();
        d.EAX() = (d.EAX() & 0xFFFFFF00u) | portIn(d, port);
    } else if (op == 0xEC) {
        d.EAX() = (d.EAX() & 0xFFFFFF00u) | portIn(d, static_cast<std::uint16_t>(d.EDX() & 0xFF));
    } else if (op == 0xED) {
        const std::uint16_t port = static_cast<std::uint16_t>(d.EDX() & 0xFFFF);
        d.EAX() = (d.EAX() & 0xFFFFFF00u) | portIn(d, port);
    } else if (op == 0xE6) {
        const std::uint8_t port = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++)));
        portOut(d, port, static_cast<std::uint8_t>(d.EAX()));
    } else if (op == 0xE7) {
        const std::uint16_t port = fetch16();
        portOut(d, port, static_cast<std::uint8_t>(d.EAX()));
    } else if (op == 0xEE) {
        portOut(d, static_cast<std::uint16_t>(d.EDX() & 0xFF), static_cast<std::uint8_t>(d.EAX()));
    } else if (op == 0xEF) {
        portOut(d, static_cast<std::uint16_t>(d.EDX() & 0xFFFF), static_cast<std::uint8_t>(d.EAX()));
    } else if (op == 0xA4 || op == 0xA5) {
        auto once = [&]() {
            if (op == 0xA4) {
                const std::uint8_t v = d.rb(Die::lin(static_cast<std::uint16_t>(d.DS()),
                    static_cast<std::uint16_t>(d.ESI())));
                d.wb(Die::lin(static_cast<std::uint16_t>(d.ES()), static_cast<std::uint16_t>(d.EDI())), v);
                d.ESI() = (d.ESI() & 0xFFFF0000u) | static_cast<std::uint16_t>(r16(d, 6) + 1);
                d.EDI() = (d.EDI() & 0xFFFF0000u) | static_cast<std::uint16_t>(r16(d, 7) + 1);
            } else {
                const std::uint16_t v = d.rw(Die::lin(static_cast<std::uint16_t>(d.DS()),
                    static_cast<std::uint16_t>(d.ESI())));
                d.ww(Die::lin(static_cast<std::uint16_t>(d.ES()), static_cast<std::uint16_t>(d.EDI())), v);
                d.ESI() = (d.ESI() & 0xFFFF0000u) | static_cast<std::uint16_t>(r16(d, 6) + 2);
                d.EDI() = (d.EDI() & 0xFFFF0000u) | static_cast<std::uint16_t>(r16(d, 7) + 2);
            }
        };
        if (rep) {
            while (r16(d, 1) != 0) {
                once();
                d.ECX() = (d.ECX() & 0xFFFF0000u) | static_cast<std::uint16_t>(r16(d, 1) - 1);
            }
        } else once();
    } else if (op == 0xAA || op == 0xAB) {
        auto once = [&]() {
            if (op == 0xAA)
                d.wb(Die::lin(static_cast<std::uint16_t>(d.ES()), static_cast<std::uint16_t>(d.EDI())),
                    static_cast<std::uint8_t>(d.EAX()));
            else
                d.ww(Die::lin(static_cast<std::uint16_t>(d.ES()), static_cast<std::uint16_t>(d.EDI())),
                    r16(d, 0));
            d.EDI() = (d.EDI() & 0xFFFF0000u) | static_cast<std::uint16_t>(r16(d, 7) + (op == 0xAA ? 1 : 2));
        };
        if (rep) {
            while (r16(d, 1) != 0) {
                once();
                d.ECX() = (d.ECX() & 0xFFFF0000u) | static_cast<std::uint16_t>(r16(d, 1) - 1);
            }
        } else once();
    } else if (op == 0xAC || op == 0xAD) {
        if (op == 0xAC) {
            const std::uint8_t v = d.rb(Die::lin(static_cast<std::uint16_t>(d.DS()),
                static_cast<std::uint16_t>(d.ESI())));
            d.EAX() = (d.EAX() & 0xFFFFFF00u) | v;
            d.ESI() = (d.ESI() & 0xFFFF0000u) | static_cast<std::uint16_t>(r16(d, 6) + 1);
        } else {
            const std::uint16_t v = d.rw(Die::lin(static_cast<std::uint16_t>(d.DS()),
                static_cast<std::uint16_t>(d.ESI())));
            w16(d, 0, v);
            d.ESI() = (d.ESI() & 0xFFFF0000u) | static_cast<std::uint16_t>(r16(d, 6) + 2);
        }
    } else if (op == 0xA0) {
        d.EAX() = (d.EAX() & 0xFFFFFF00u) | d.rb(Die::lin(dataSeg, fetch16()));
    } else if (op == 0xA1) {
        w16(d, 0, d.rw(Die::lin(dataSeg, fetch16())));
    } else if (op == 0xA2) {
        d.wb(Die::lin(dataSeg, fetch16()), static_cast<std::uint8_t>(d.EAX()));
    } else if (op == 0xA3) {
        d.ww(Die::lin(dataSeg, fetch16()), r16(d, 0));
    } else if (op == 0x05 || op == 0x15) {
        const std::uint16_t imm = fetch16();
        const std::uint16_t ax = r16(d, 0);
        const std::uint16_t res = static_cast<std::uint16_t>(ax + imm + (op == 0x15 && cf(d) ? 1 : 0));
        w16(d, 0, res);
        setZF(d, res); setSF(d, res); setPF(d, res);
        setCF(d, res < ax);
    } else if (op == 0x2D) {
        const std::uint16_t imm = fetch16();
        const std::uint16_t ax = r16(d, 0);
        const std::uint16_t res = static_cast<std::uint16_t>(ax - imm);
        w16(d, 0, res);
        setZF(d, res); setSF(d, res); setPF(d, res);
        setCF(d, ax < imm);
    } else if (op == 0x3D) {
        const std::uint16_t imm = fetch16();
        const std::uint16_t ax = r16(d, 0);
        const std::uint16_t res = static_cast<std::uint16_t>(ax - imm);
        setZF(d, res); setSF(d, res); setPF(d, res);
        setCF(d, ax < imm);
    } else if (op == 0x3C) {
        const std::uint8_t imm = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++)));
        const std::uint8_t al = static_cast<std::uint8_t>(d.EAX());
        const std::uint8_t res = static_cast<std::uint8_t>(al - imm);
        setZF(d, res); setSF(d, res); setPF(d, res);
        setCF(d, al < imm);
    } else if (op == 0xA8) {
        const std::uint8_t imm = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++)));
        const std::uint8_t res = static_cast<std::uint8_t>((d.EAX() & 0xFF) & imm);
        setZF(d, res); setPF(d, res);
    } else if (op == 0xA9) {
        const std::uint16_t imm = fetch16();
        const std::uint16_t res = static_cast<std::uint16_t>(r16(d, 0) & imm);
        setZF(d, res); setPF(d, res);
    } else if (op == 0x00 || op == 0x02 || op == 0x10 || op == 0x12 || op == 0x28 || op == 0x2A ||
               op == 0x31 || op == 0x33 || op == 0x29 || op == 0x2B || op == 0x09 || op == 0x0B ||
               op == 0x21 || op == 0x23 || op == 0x01 || op == 0x03 || op == 0x85 || op == 0x08 ||
               op == 0x0A || op == 0x20 || op == 0x22 || op == 0x38 || op == 0x3A) {
        const std::uint8_t modrm = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++)));
        EA ea = decodeEA(d, modrm, dataSeg);
        const bool byteOp = (op == 0x00 || op == 0x02 || op == 0x10 || op == 0x12 ||
                             op == 0x28 || op == 0x2A || op == 0x08 || op == 0x0A ||
                             op == 0x20 || op == 0x22 || op == 0x38 || op == 0x3A);
        std::uint16_t lhs = ea.isReg
            ? (byteOp ? static_cast<std::uint16_t>(r16(d, modrm & 7) & 0xFF)
                      : r16(d, modrm & 7))
            : (byteOp ? d.rb(ea.addr) : d.rw(ea.addr));
        const std::uint16_t rhs = byteOp
            ? static_cast<std::uint16_t>(r16(d, ea.reg) & 0xFF)
            : r16(d, ea.reg);
        std::uint16_t res = lhs;
        const bool isCmp = (op == 0x38 || op == 0x3A || op == 0x85);
        if (op == 0x31 || op == 0x33 || op == 0x20 || op == 0x22) res = lhs ^ rhs;
        else if (op == 0x29 || op == 0x2B || op == 0x28 || op == 0x2A) res = static_cast<std::uint16_t>(lhs - rhs);
        else if (op == 0x09 || op == 0x0B || op == 0x08 || op == 0x0A) res = lhs | rhs;
        else if (op == 0x21 || op == 0x23) res = lhs & rhs;
        else if (op == 0x01 || op == 0x03 || op == 0x00 || op == 0x02 || op == 0x10 || op == 0x12)
            res = static_cast<std::uint16_t>(lhs + rhs);
        else if (op == 0x85) res = lhs & rhs;
        else if (op == 0x38 || op == 0x3A) res = static_cast<std::uint16_t>(lhs - rhs);
        if (!isCmp) {
            if (ea.isReg) {
                if (byteOp) w16(d, modrm & 7, static_cast<std::uint16_t>((r16(d, modrm & 7) & 0xFF00) | (res & 0xFF)));
                else w16(d, modrm & 7, res);
            } else if (byteOp) d.wb(ea.addr, static_cast<std::uint8_t>(res));
            else d.ww(ea.addr, res);
        }
        setZF(d, res); setSF(d, res); setPF(d, res);
        if (op == 0x29 || op == 0x2B || op == 0x38 || op == 0x3A) setCF(d, lhs < rhs);
        if (op == 0x01 || op == 0x03) setCF(d, res < lhs);
    } else if (op == 0x62) {
        const std::uint8_t modrm = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++)));
        (void)decodeEA(d, modrm, dataSeg);
    } else if (op == 0x8D) {
        const std::uint8_t modrm = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++)));
        EA ea = decodeEA(d, modrm, dataSeg);
        if (ea.isReg) illegal(d, op);
        else w16(d, ea.reg, ea.off);
    } else if (op == 0x89 || op == 0x8B || op == 0x88 || op == 0x8A || op == 0x86 || op == 0x87) {
        const std::uint8_t modrm = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++)));
        EA ea = decodeEA(d, modrm, dataSeg);
        if (op == 0x89) {
            if (ea.isReg) w16(d, modrm & 7, r16(d, ea.reg));
            else d.ww(ea.addr, r16(d, ea.reg));
        } else if (op == 0x8B) {
            w16(d, ea.reg, ea.isReg ? r16(d, modrm & 7) : d.rw(ea.addr));
        } else if (op == 0x88) {
            const std::uint8_t v = static_cast<std::uint8_t>(r16(d, ea.reg));
            if (ea.isReg) w16(d, modrm & 7, static_cast<std::uint16_t>((r16(d, modrm & 7) & 0xFF00) | v));
            else d.wb(ea.addr, v);
        } else if (op == 0x8A) {
            const std::uint8_t v = ea.isReg ? static_cast<std::uint8_t>(r16(d, modrm & 7))
                                            : d.rb(ea.addr);
            w16(d, ea.reg, static_cast<std::uint16_t>((r16(d, ea.reg) & 0xFF00) | v));
        } else if (op == 0x86 || op == 0x87) {
            std::uint16_t a = ea.isReg ? r16(d, modrm & 7) : d.rw(ea.addr);
            std::uint16_t b = r16(d, ea.reg);
            if (ea.isReg) w16(d, modrm & 7, b);
            else d.ww(ea.addr, b);
            w16(d, ea.reg, a);
        }
    } else if (op == 0x8E || op == 0x8C || op == 0xC4 || op == 0xC5) {
        const std::uint8_t modrm = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++)));
        EA ea = decodeEA(d, modrm, dataSeg);
        const int sreg = (modrm >> 3) & 3;
        if (op == 0x8E) {
            const std::uint16_t v = ea.isReg ? r16(d, modrm & 7) : d.rw(ea.addr);
            setSeg(d, sreg, v);
        } else if (op == 0x8C) {
            const std::uint16_t v = segVal(d, sreg);
            if (ea.isReg) w16(d, modrm & 7, v);
            else d.ww(ea.addr, v);
        } else if (op == 0xC4) {
            const std::uint16_t off = ea.isReg ? r16(d, modrm & 7) : d.rw(ea.addr);
            const std::uint16_t seg = d.rw(ea.addr + 2);
            d.ES() = seg;
            w16(d, ea.reg, off);
        } else {
            const std::uint16_t off = ea.isReg ? r16(d, modrm & 7) : d.rw(ea.addr);
            const std::uint16_t seg = d.rw(ea.addr + 2);
            d.DS() = seg;
            w16(d, ea.reg, off);
        }
    } else if (op == 0xC6 || op == 0xC7) {
        const std::uint8_t modrm = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++)));
        EA ea = decodeEA(d, modrm, dataSeg);
        if (op == 0xC6) {
            const std::uint8_t imm = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
                static_cast<std::uint16_t>(d.EIP()++)));
            if (ea.isReg) w16(d, modrm & 7, static_cast<std::uint16_t>((r16(d, modrm & 7) & 0xFF00) | imm));
            else d.wb(ea.addr, imm);
        } else {
            const std::uint16_t imm = fetch16();
            if (ea.isReg) w16(d, modrm & 7, imm);
            else d.ww(ea.addr, imm);
        }
    } else if (op == 0x80 || op == 0x81 || op == 0x82 || op == 0x83) {
        const std::uint8_t modrm = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++)));
        EA ea = decodeEA(d, modrm, dataSeg);
        const int sub = (modrm >> 3) & 7;
        std::uint16_t imm = (op == 0x81) ? fetch16()
            : static_cast<std::uint16_t>(static_cast<std::int16_t>(fetchS8()) & (op == 0x80 ? 0xFF : 0xFFFF));
        std::uint16_t lhs = ea.isReg ? r16(d, modrm & 7) : d.rw(ea.addr);
        std::uint16_t res = lhs;
        const bool isCmp = (sub == 7);
        if (sub == 0 || sub == 2) res = static_cast<std::uint16_t>(lhs + imm);
        else if (sub == 5 || sub == 7) res = static_cast<std::uint16_t>(lhs - imm);
        else if (sub == 4) res = lhs & static_cast<std::uint16_t>(imm);
        else if (sub == 1) res = lhs | static_cast<std::uint16_t>(imm);
        else if (sub == 6) res = lhs ^ static_cast<std::uint16_t>(imm);
        else illegal(d, op);
        if (!isCmp) {
            if (ea.isReg) w16(d, modrm & 7, res);
            else d.ww(ea.addr, res);
        }
        setZF(d, isCmp ? static_cast<std::uint16_t>(lhs & imm) : res);
        setSF(d, res); setPF(d, res);
        if (sub == 5 || sub == 7) setCF(d, lhs < imm);
        if (sub == 0 || sub == 2) setCF(d, res < lhs);
    } else if (op == 0xD0 || op == 0xD1 || op == 0xD2 || op == 0xD3) {
        const std::uint8_t modrm = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++)));
        EA ea = decodeEA(d, modrm, dataSeg);
        const int sub = (modrm >> 3) & 7;
        const int cnt = (op == 0xD0 || op == 0xD1) ? 1 : (d.ECX() & 0xFF);
        std::uint16_t v = ea.isReg ? r16(d, modrm & 7) : d.rw(ea.addr);
        v = doShift(v, cnt, sub);
        if (ea.isReg) w16(d, modrm & 7, v);
        else d.ww(ea.addr, v);
    } else if (op == 0xF6 || op == 0xF7) {
        const std::uint8_t modrm = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++)));
        EA ea = decodeEA(d, modrm, dataSeg);
        const int sub = (modrm >> 3) & 7;
        const bool w = op == 0xF7;
        if (sub == 0 || sub == 1) {
            std::uint16_t imm = w ? fetch16() : d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
                static_cast<std::uint16_t>(d.EIP()++)));
            std::uint16_t lhs = ea.isReg ? r16(d, modrm & 7) : (w ? d.rw(ea.addr) : d.rb(ea.addr));
            const std::uint16_t res = static_cast<std::uint16_t>(lhs & imm);
            setZF(d, res); setPF(d, res);
        } else if (sub == 2 || sub == 3) {
            std::uint16_t v = ea.isReg ? r16(d, modrm & 7) : d.rw(ea.addr);
            v = static_cast<std::uint16_t>(~v);
            if (ea.isReg) w16(d, modrm & 7, v);
            else d.ww(ea.addr, v);
            setZF(d, v); setSF(d, v); setPF(d, v);
        } else if (sub == 4 && w) {
            const std::uint32_t prod = static_cast<std::uint32_t>(r16(d, 0)) * (ea.isReg ? r16(d, modrm & 7) : d.rw(ea.addr));
            w16(d, 0, static_cast<std::uint16_t>(prod & 0xFFFF));
            w16(d, 2, static_cast<std::uint16_t>(prod >> 16));
        } else if (sub == 6 && w) {
            const std::uint16_t div = ea.isReg ? r16(d, modrm & 7) : d.rw(ea.addr);
            if (div == 0) { illegal(d, op); return; }
            const std::uint32_t dv = (static_cast<std::uint32_t>(r16(d, 2)) << 16) | r16(d, 0);
            w16(d, 0, static_cast<std::uint16_t>(dv / div));
            w16(d, 2, static_cast<std::uint16_t>(dv % div));
        } else illegal(d, op);
    } else if (op == 0xFE || op == 0xFF) {
        const std::uint8_t modrm = d.rb(Die::lin(static_cast<std::uint16_t>(d.CS()),
            static_cast<std::uint16_t>(d.EIP()++)));
        EA ea = decodeEA(d, modrm, dataSeg);
        const int sub = (modrm >> 3) & 7;
        if (op == 0xFE && sub == 0) {
            std::uint8_t v = ea.isReg ? static_cast<std::uint8_t>(r16(d, modrm & 7)) : d.rb(ea.addr);
            v = static_cast<std::uint8_t>(v + 1);
            if (ea.isReg) w16(d, modrm & 7, static_cast<std::uint16_t>((r16(d, modrm & 7) & 0xFF00) | v));
            else d.wb(ea.addr, v);
            setZF(d, v); setPF(d, v);
        } else if (op == 0xFE && sub == 1) {
            std::uint8_t v = ea.isReg ? static_cast<std::uint8_t>(r16(d, modrm & 7)) : d.rb(ea.addr);
            v = static_cast<std::uint8_t>(v - 1);
            if (ea.isReg) w16(d, modrm & 7, static_cast<std::uint16_t>((r16(d, modrm & 7) & 0xFF00) | v));
            else d.wb(ea.addr, v);
            setZF(d, v); setPF(d, v);
        } else if (op == 0xFF && sub == 4) {
            const std::uint16_t off = ea.isReg ? r16(d, modrm & 7) : d.rw(ea.addr);
            push16(d, static_cast<std::uint16_t>(d.CS()));
            push16(d, static_cast<std::uint16_t>(d.EIP()));
            d.EIP() = off;
        } else if (op == 0xFF && sub == 5) {
            const std::uint32_t a = ea.isReg ? Die::lin(static_cast<std::uint16_t>(r16(d, modrm & 7)), 0)
                : ea.addr;
            const std::uint16_t off = d.rw(a);
            const std::uint16_t seg = d.rw(a + 2);
            push16(d, static_cast<std::uint16_t>(d.CS()));
            push16(d, static_cast<std::uint16_t>(d.EIP()));
            d.EIP() = off;
            d.CS() = seg;
        } else if (op == 0xFF && (sub == 0 || sub == 1)) {
            std::uint16_t v = ea.isReg ? r16(d, modrm & 7) : d.rw(ea.addr);
            v = static_cast<std::uint16_t>(v + (sub == 0 ? 1 : -1));
            if (ea.isReg) w16(d, modrm & 7, v);
            else d.ww(ea.addr, v);
            setZF(d, v); setSF(d, v); setPF(d, v);
        } else illegal(d, op);
    } else {
        illegal(d, op);
        return;
    }

    d.cycle_count()++;
    ctx.ticks += 1;
}

inline std::uint32_t run(Die& d, Ctx& ctx, std::uint32_t maxCycles) {
    for (std::uint32_t i = 0; i < maxCycles; ++i) {
        if (d.EFLAGS() & EFLAGS_HALTED) break;
        execOne(d, ctx);
    }
    return d.cycle_count();
}

inline std::uint32_t runMapped(void* mapped, Ctx& ctx, std::uint32_t maxCycles) {
    if (!mapped) return 0;
    Die d{};
    d.base = static_cast<std::uint8_t*>(mapped);
    return run(d, ctx, maxCycles);
}

} // namespace FieldCpuHost