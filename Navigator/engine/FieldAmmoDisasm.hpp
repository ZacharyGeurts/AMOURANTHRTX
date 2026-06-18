#pragma once

// AMMO disassembler — DevKit COM/OBJ listings (8086 subset matching AMMOASM/AMMORUN).

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace FieldAmmoDisasm {

struct Instr {
    std::uint32_t len = 0;
    char text[128]{};
};

inline const char* reg8Name(std::uint8_t r) {
    static const char* k[] = {"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"};
    return r < 8u ? k[r] : "r8?";
}

inline const char* reg16Name(std::uint8_t r) {
    static const char* k[] = {"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"};
    return r < 8u ? k[r] : "r16?";
}

inline std::uint16_t u16le(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0]) | static_cast<std::uint16_t>(p[1] << 8);
}

inline std::int16_t s16le(const std::uint8_t* p) {
    return static_cast<std::int16_t>(u16le(p));
}

inline Instr decodeAt(const std::uint8_t* ram, std::uint32_t ip, std::uint32_t ramBytes) {
    Instr out{};
    if (!ram || ip >= ramBytes) {
        std::snprintf(out.text, sizeof out.text, "???");
        out.len = 1;
        return out;
    }
    const std::uint8_t op = ram[ip];
    const std::uint32_t relIp = ip & 0xFFFFu;

    if (op >= 0xB0u && op <= 0xB7u && ip + 1u < ramBytes) {
        out.len = 2;
        std::snprintf(out.text, sizeof out.text, "mov %s, %02Xh",
            reg8Name(static_cast<std::uint8_t>(op - 0xB0u)), ram[ip + 1u]);
        return out;
    }
    if (op >= 0xB8u && op <= 0xBFu && ip + 2u < ramBytes) {
        out.len = 3;
        std::snprintf(out.text, sizeof out.text, "mov %s, %04Xh",
            reg16Name(static_cast<std::uint8_t>(op - 0xB8u)), u16le(ram + ip + 1u));
        return out;
    }
    if (op == 0xB4u && ip + 1u < ramBytes) {
        out.len = 2;
        std::snprintf(out.text, sizeof out.text, "mov ah, %02Xh", ram[ip + 1u]);
        return out;
    }
    if (op == 0xBAu && ip + 2u < ramBytes) {
        out.len = 3;
        std::snprintf(out.text, sizeof out.text, "mov dx, %04Xh", u16le(ram + ip + 1u));
        return out;
    }
    if (op == 0x04u && ip + 1u < ramBytes) {
        out.len = 2;
        std::snprintf(out.text, sizeof out.text, "add al, %02Xh", ram[ip + 1u]);
        return out;
    }
    if (op == 0x05u && ip + 2u < ramBytes) {
        out.len = 3;
        std::snprintf(out.text, sizeof out.text, "add ax, %04Xh", u16le(ram + ip + 1u));
        return out;
    }
    if (op == 0x2Du && ip + 2u < ramBytes) {
        out.len = 3;
        std::snprintf(out.text, sizeof out.text, "sub ax, %04Xh", u16le(ram + ip + 1u));
        return out;
    }
    if (op == 0x3Du && ip + 2u < ramBytes) {
        out.len = 3;
        std::snprintf(out.text, sizeof out.text, "cmp ax, %04Xh", u16le(ram + ip + 1u));
        return out;
    }
    if (op >= 0x40u && op <= 0x47u) {
        out.len = 1;
        std::snprintf(out.text, sizeof out.text, "inc %s",
            reg16Name(static_cast<std::uint8_t>(op - 0x40u)));
        return out;
    }
    if (op >= 0x48u && op <= 0x4Fu) {
        out.len = 1;
        std::snprintf(out.text, sizeof out.text, "dec %s",
            reg16Name(static_cast<std::uint8_t>(op - 0x48u)));
        return out;
    }
    if (op >= 0x50u && op <= 0x57u) {
        out.len = 1;
        std::snprintf(out.text, sizeof out.text, "push %s",
            reg16Name(static_cast<std::uint8_t>(op - 0x50u)));
        return out;
    }
    if (op >= 0x58u && op <= 0x5Fu) {
        out.len = 1;
        std::snprintf(out.text, sizeof out.text, "pop %s",
            reg16Name(static_cast<std::uint8_t>(op - 0x58u)));
        return out;
    }
    if (op == 0x33u && ip + 1u < ramBytes && ram[ip + 1u] == 0xC0u) {
        out.len = 2;
        std::snprintf(out.text, sizeof out.text, "xor ax, ax");
        return out;
    }
    if (op == 0xCDu && ip + 1u < ramBytes) {
        out.len = 2;
        std::snprintf(out.text, sizeof out.text, "int %02Xh", ram[ip + 1u]);
        return out;
    }
    if (op == 0xC3u) {
        out.len = 1;
        std::snprintf(out.text, sizeof out.text, "ret");
        return out;
    }
    if (op == 0x90u) {
        out.len = 1;
        std::snprintf(out.text, sizeof out.text, "nop");
        return out;
    }
    if (op == 0xEBu && ip + 1u < ramBytes) {
        out.len = 2;
        const std::int8_t rel = static_cast<std::int8_t>(ram[ip + 1u]);
        std::snprintf(out.text, sizeof out.text, "jmp short %04Xh",
            static_cast<unsigned>((static_cast<std::int32_t>(relIp) + 2 + rel) & 0xFFFF));
        return out;
    }
    if (op == 0xE8u && ip + 2u < ramBytes) {
        out.len = 3;
        const std::int16_t rel = s16le(ram + ip + 1u);
        std::snprintf(out.text, sizeof out.text, "call %04Xh",
            static_cast<unsigned>((static_cast<std::int32_t>(relIp) + 3 + rel) & 0xFFFF));
        return out;
    }
    if (op == 0xE9u && ip + 2u < ramBytes) {
        out.len = 3;
        const std::int16_t rel = s16le(ram + ip + 1u);
        std::snprintf(out.text, sizeof out.text, "jmp %04Xh",
            static_cast<unsigned>((static_cast<std::int32_t>(relIp) + 3 + rel) & 0xFFFF));
        return out;
    }
    if (op == 0x74u && ip + 1u < ramBytes) {
        out.len = 2;
        const std::int8_t rel = static_cast<std::int8_t>(ram[ip + 1u]);
        std::snprintf(out.text, sizeof out.text, "je %04Xh",
            static_cast<unsigned>((static_cast<std::int32_t>(relIp) + 2 + rel) & 0xFFFF));
        return out;
    }
    if (op == 0x75u && ip + 1u < ramBytes) {
        out.len = 2;
        const std::int8_t rel = static_cast<std::int8_t>(ram[ip + 1u]);
        std::snprintf(out.text, sizeof out.text, "jne %04Xh",
            static_cast<unsigned>((static_cast<std::int32_t>(relIp) + 2 + rel) & 0xFFFF));
        return out;
    }
    if (op == 0x8Bu && ip + 1u < ramBytes) {
        const std::uint8_t modrm = ram[ip + 1u];
        if ((modrm & 0xC0u) == 0xC0u) {
            out.len = 2;
            std::snprintf(out.text, sizeof out.text, "mov ax, %s",
                reg16Name(static_cast<std::uint8_t>((modrm >> 3) & 7u)));
            return out;
        }
    }
    if (op == 0x89u && ip + 1u < ramBytes) {
        const std::uint8_t modrm = ram[ip + 1u];
        if ((modrm & 0xC0u) == 0xC0u) {
            out.len = 2;
            std::snprintf(out.text, sizeof out.text, "mov %s, ax",
                reg16Name(static_cast<std::uint8_t>((modrm >> 3) & 7u)));
            return out;
        }
    }

    out.len = 1;
    std::snprintf(out.text, sizeof out.text, "db %02Xh", op);
    return out;
}

inline std::string formatListing(const std::uint8_t* ram, std::uint32_t ip, std::uint32_t count,
                                 std::uint32_t ramBytes) {
    std::string out;
    for (std::uint32_t i = 0; i < count && ip < ramBytes; ++i) {
        const Instr ins = decodeAt(ram, ip, ramBytes);
        char line[160];
        std::snprintf(line, sizeof line, "%04X  %s\r\n", ip & 0xFFFFu, ins.text);
        out += line;
        ip += ins.len ? ins.len : 1u;
    }
    return out;
}

} // namespace FieldAmmoDisasm