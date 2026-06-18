#pragma once

// AMMODBG v4 — interactive session: load, step, go, breakpoints, unasm, mem dump.

#include "FieldAmmoDisasm.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldAmmoRun.hpp"
#include "FieldDos.hpp"
#include "FieldPlatform.hpp"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace FieldAmmoDbg {

using EchoFn = std::function<void(std::uint8_t*, char)>;
using NewlineFn = std::function<void(std::uint8_t*)>;

struct Session {
    bool loaded = false;
    bool inRam = false;
    std::string path;
    std::vector<std::uint8_t> com;
    FieldAmmoRun::Cpu cpu{};
    std::vector<std::uint32_t> breakpoints;
};

inline Session session;

inline void printLine(std::uint8_t* ram, const EchoFn& echo, const NewlineFn& nl, const char* text) {
    for (const char* p = text; *p; ++p) {
        if (*p == '\r') continue;
        if (*p == '\n') nl(ram);
        else echo(ram, *p);
    }
}

inline void dumpRegs(const FieldAmmoRun::Cpu& cpu, std::uint8_t* ram,
                     const EchoFn& echo, const NewlineFn& nl) {
    char buf[256];
    std::snprintf(buf, sizeof buf,
        "\r\nAMMODBG v4 AX=%04X BX=%04X CX=%04X DX=%04X DI=%04X IP=%04X ZF=%u CF=%u\r\n",
        cpu.ax, cpu.bx, cpu.cx, cpu.dx, cpu.di,
        static_cast<unsigned>(cpu.ip & 0xFFFFu), cpu.zf ? 1u : 0u, cpu.cf ? 1u : 0u);
    printLine(ram, echo, nl, buf);
}

inline void unasmAt(std::uint8_t* ram, std::uint32_t ip, int lines,
                    const EchoFn& echo, const NewlineFn& nl) {
    printLine(ram, echo, nl, "\r\n");
    std::uint32_t cur = ip;
    for (int i = 0; i < lines && cur < FieldPlatform::GUEST_RAM_BYTES; ++i) {
        const auto ins = FieldAmmoDisasm::decodeAt(ram, cur, FieldPlatform::GUEST_RAM_BYTES);
        char buf[160];
        std::snprintf(buf, sizeof buf, "%04X  %s\r\n", cur & 0xFFFFu, ins.text);
        printLine(ram, echo, nl, buf);
        cur += ins.len ? ins.len : 1u;
    }
}

inline void dumpMem(std::uint8_t* ram, std::uint32_t addr, int bytes,
                    const EchoFn& echo, const NewlineFn& nl) {
    printLine(ram, echo, nl, "\r\n");
    for (int row = 0; row < bytes; row += 16) {
        char line[96];
        std::snprintf(line, sizeof line, "%04X:", (addr + static_cast<std::uint32_t>(row)) & 0xFFFFu);
        printLine(ram, echo, nl, line);
        for (int col = 0; col < 16 && row + col < bytes; ++col) {
            const std::uint32_t a = addr + static_cast<std::uint32_t>(row + col);
            char b[8];
            std::snprintf(b, sizeof b, " %02X", a < FieldPlatform::GUEST_RAM_BYTES ? ram[a] : 0u);
            printLine(ram, echo, nl, b);
        }
        nl(ram);
    }
}

inline void dumpVga(std::uint8_t* ram, const EchoFn& echo, const NewlineFn& nl, int rows = 8) {
    printLine(ram, echo, nl, "\r\nAMMODBG VGA peek:\r\n");
    char line[96];
    for (int row = 0; row < rows; ++row) {
        for (int col = 0; col < 40; ++col) {
            const char ch = static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>((row * 80 + col) * 2)]);
            line[col] = (ch >= 32 && ch < 127) ? ch : '.';
        }
        line[40] = '\0';
        printLine(ram, echo, nl, "\r\n");
        printLine(ram, echo, nl, line);
    }
}

inline void dumpGuest(std::uint8_t* ram, const EchoFn& echo, const NewlineFn& nl) {
    char buf[256];
    std::snprintf(buf, sizeof buf,
        "\r\nAMMODBG v4 guest: cursor=%u,%u mode=%u AMMOFAT=%s session=%s\r\n",
        ram[0x451], ram[0x450], ram[0x449],
        FieldAmmoFat::mounted ? "on" : "off",
        session.loaded ? session.path.c_str() : "(none)");
    printLine(ram, echo, nl, buf);
    dumpVga(ram, echo, nl, 6);
}

inline bool loadSession(std::uint8_t* ram, const char* comPath) {
    session.com.clear();
    if (!FieldAmmoFat::readFile(comPath, session.com) || session.com.empty())
        return false;
    session.path = comPath;
    session.loaded = true;
    session.inRam = ram && FieldAmmoRun::loadCom(ram, session.com);
    session.cpu = FieldAmmoRun::Cpu{};
    session.cpu.ip = FieldAmmoRun::PSP_LINEAR + 0x100u;
    session.breakpoints.clear();
    return session.inRam;
}

inline bool hitBreakpoint(const FieldAmmoRun::Cpu& cpu) {
    for (std::uint32_t bp : session.breakpoints)
        if (bp == cpu.ip) return true;
    return false;
}

inline FieldAmmoRun::Result stepSession(std::uint8_t* ram, const EchoFn& echo, const NewlineFn& nl,
                                          std::uint32_t count = 1u) {
    FieldAmmoRun::Result r;
    if (!session.loaded || !session.inRam) { r.error = "no program loaded (AMMODBG LOAD file)"; return r; }
    for (std::uint32_t i = 0; i < count && !session.cpu.halted; ++i) {
        if (!FieldAmmoRun::stepOne(ram, session.cpu, echo, nl)) {
            r.error = "step opcode fail";
            r.cpu = session.cpu;
            return r;
        }
        r.steps = i + 1u;
        if (hitBreakpoint(session.cpu)) break;
    }
    r.ok = true;
    r.cpu = session.cpu;
    return r;
}

inline FieldAmmoRun::Result goSession(std::uint8_t* ram, const EchoFn& echo, const NewlineFn& nl,
                                      std::uint32_t maxSteps = 65536u) {
    FieldAmmoRun::Result r;
    if (!session.loaded || !session.inRam) { r.error = "no program loaded"; return r; }
    for (std::uint32_t i = 0; i < maxSteps && !session.cpu.halted; ++i) {
        if (!FieldAmmoRun::stepOne(ram, session.cpu, echo, nl)) {
            r.error = "go opcode fail";
            r.cpu = session.cpu;
            return r;
        }
        r.steps = i + 1u;
        if (hitBreakpoint(session.cpu)) break;
    }
    r.ok = true;
    r.cpu = session.cpu;
    return r;
}

inline bool addBreakpoint(std::uint32_t linearIp) {
    if (std::find(session.breakpoints.begin(), session.breakpoints.end(), linearIp)
        == session.breakpoints.end())
        session.breakpoints.push_back(linearIp);
    return true;
}

inline void clearBreakpoints() { session.breakpoints.clear(); }

inline FieldAmmoRun::Result stepCom(std::uint8_t* ram, const char* comPath,
                                    const EchoFn& echo, const NewlineFn& nl,
                                    std::uint32_t maxSteps = 32u) {
    if (!loadSession(ram, comPath)) return FieldAmmoRun::Result{false, "cannot read COM", 0, {}};
    auto r = stepSession(ram, echo, nl, maxSteps);
    if (r.ok) {
        dumpRegs(session.cpu, ram, echo, nl);
        unasmAt(ram, session.cpu.ip, 4, echo, nl);
    }
    return r;
}

inline void usage(std::uint8_t* ram, const EchoFn& echo, const NewlineFn& nl) {
    printLine(ram, echo, nl,
        "\r\nAMMODBG v4 — RTX Field Die debugger\r\n"
        "  LOAD file.com    load into session\r\n"
        "  STEP [n]         step n instructions (default 1)\r\n"
        "  GO               run until BP/halt\r\n"
        "  U [addr] [n]     unassemble n lines\r\n"
        "  D addr [bytes]   hex memory dump\r\n"
        "  R                registers\r\n"
        "  BP addr          breakpoint (linear IP)\r\n"
        "  BC               clear breakpoints\r\n"
        "  VGA / GUEST      peek guest state\r\n"
        "  RUN file         AMMORUN shortcut\r\n");
}

} // namespace FieldAmmoDbg