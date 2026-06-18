#pragma once

// AMMORUN v3 — GPU COM runner + step debugger hooks.

#include "FieldAmmoFat.hpp"
#include "FieldDos.hpp"
#include "FieldPlatform.hpp"

#include <cstdint>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace FieldAmmoRun {

constexpr std::uint32_t PSP_SEG = 0x0800u;
constexpr std::uint32_t PSP_LINEAR = PSP_SEG << 4;
constexpr std::uint32_t VGA_BASE = 0x000B8000u;
constexpr std::uint32_t BDA_CUR_COL = 0x450u;
constexpr std::uint32_t BDA_CUR_ROW = 0x451u;

struct Cpu {
    std::uint16_t ax = 0, bx = 0, cx = 0, dx = 0, di = 0, sp = 0xFFFEu;
    std::uint16_t ds = PSP_SEG, es = PSP_SEG, ss = PSP_SEG;
    std::uint32_t ip = PSP_LINEAR + 0x100u;
    bool halted = false;
    bool zf = false;
    bool cf = false;
    bool sf = false;
};

using EchoFn = std::function<void(std::uint8_t*, char)>;
using NewlineFn = std::function<void(std::uint8_t*)>;

struct Result {
    bool ok = false;
    std::string error;
    std::uint32_t steps = 0;
    Cpu cpu{};
};

inline std::uint8_t fetch8(std::uint8_t* ram, std::uint32_t& ip) {
    return ram[ip++];
}

inline std::uint16_t fetch16(std::uint8_t* ram, std::uint32_t& ip) {
    const std::uint16_t lo = fetch8(ram, ip);
    const std::uint16_t hi = fetch8(ram, ip);
    return static_cast<std::uint16_t>(lo | (hi << 8));
}

inline std::uint32_t linear(std::uint16_t seg, std::uint16_t off) {
    return (static_cast<std::uint32_t>(seg) << 4) + off;
}

inline void push16(std::uint8_t* ram, Cpu& cpu, std::uint16_t v) {
    cpu.sp -= 2u;
    const std::uint32_t addr = linear(cpu.ss, cpu.sp);
    ram[addr] = static_cast<std::uint8_t>(v & 0xFFu);
    ram[addr + 1u] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
}

inline std::uint16_t pop16(std::uint8_t* ram, Cpu& cpu) {
    const std::uint32_t addr = linear(cpu.ss, cpu.sp);
    const std::uint16_t v = static_cast<std::uint16_t>(ram[addr])
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(ram[addr + 1u]) << 8);
    cpu.sp += 2u;
    return v;
}

inline void setReg8(Cpu& cpu, std::uint8_t reg, std::uint8_t v) {
    switch (reg) {
    case 0: cpu.ax = (cpu.ax & 0xFF00u) | v; break;
    case 1: cpu.cx = (cpu.cx & 0xFF00u) | v; break;
    case 2: cpu.dx = (cpu.dx & 0xFF00u) | v; break;
    case 3: cpu.bx = (cpu.bx & 0xFF00u) | v; break;
    case 4: cpu.ax = (cpu.ax & 0x00FFu) | (static_cast<std::uint16_t>(v) << 8); break;
    case 5: cpu.cx = (cpu.cx & 0x00FFu) | (static_cast<std::uint16_t>(v) << 8); break;
    case 6: cpu.dx = (cpu.dx & 0x00FFu) | (static_cast<std::uint16_t>(v) << 8); break;
    case 7: cpu.bx = (cpu.bx & 0x00FFu) | (static_cast<std::uint16_t>(v) << 8); break;
    default: break;
    }
}

inline std::uint16_t cursorPos(std::uint8_t* ram) noexcept {
    return static_cast<std::uint16_t>(ram[BDA_CUR_ROW] * 80u + ram[BDA_CUR_COL]);
}

inline void setCursor(std::uint8_t* ram, std::uint16_t cell) noexcept {
    ram[BDA_CUR_COL] = static_cast<std::uint8_t>(cell % 80u);
    ram[BDA_CUR_ROW] = static_cast<std::uint8_t>(cell / 80u);
}

inline void vgaPut(std::uint8_t* ram, std::uint16_t cell, char ch, std::uint8_t attr = 0x07u) noexcept {
    const std::uint32_t off = VGA_BASE + static_cast<std::uint32_t>(cell) * 2u;
    if (off + 1u >= FieldPlatform::GUEST_RAM_BYTES) return;
    ram[off] = static_cast<std::uint8_t>(ch);
    ram[off + 1u] = attr;
}

inline void scrollVga(std::uint8_t* ram) noexcept {
    for (int row = 0; row < 24; ++row) {
        for (int col = 0; col < 80; ++col) {
            const std::uint16_t dst = static_cast<std::uint16_t>(row * 80 + col);
            const std::uint16_t src = static_cast<std::uint16_t>((row + 1) * 80 + col);
            ram[VGA_BASE + dst * 2u] = ram[VGA_BASE + src * 2u];
            ram[VGA_BASE + dst * 2u + 1u] = ram[VGA_BASE + src * 2u + 1u];
        }
    }
    for (int col = 0; col < 80; ++col)
        vgaPut(ram, static_cast<std::uint16_t>(24 * 80 + col), ' ');
}

inline void vgaEcho(std::uint8_t* ram, char ch) noexcept {
    if (ch == '\r') return;
    if (ch == '\n') {
        std::uint16_t pos = cursorPos(ram);
        const std::uint16_t row = static_cast<std::uint16_t>(pos / 80u + 1u);
        if (row >= 25u) {
            scrollVga(ram);
            setCursor(ram, static_cast<std::uint16_t>(24 * 80u));
        } else {
            setCursor(ram, static_cast<std::uint16_t>(row * 80u));
        }
        return;
    }
    std::uint16_t pos = cursorPos(ram);
    if (pos / 80u >= 25u) {
        scrollVga(ram);
        pos = static_cast<std::uint16_t>(24 * 80u);
    }
    vgaPut(ram, pos, ch);
    setCursor(ram, static_cast<std::uint16_t>(pos + 1u));
}

inline void guestEcho(std::uint8_t* ram, char ch, const EchoFn& echo) {
    vgaEcho(ram, ch);
    if (echo) echo(ram, ch);
}

inline void guestNewline(std::uint8_t* ram, const NewlineFn& nl) {
    vgaEcho(ram, '\n');
    if (nl) nl(ram);
}

inline void printDosString(std::uint8_t* ram, std::uint16_t seg, std::uint16_t off,
                            const EchoFn& echo, const NewlineFn& nl) {
    std::uint32_t addr = linear(seg, off);
    for (int guard = 0; guard < 512; ++guard) {
        const char ch = static_cast<char>(ram[addr++]);
        if (ch == '$') break;
        if (ch == '\r') continue;
        if (ch == '\n') guestNewline(ram, nl);
        else guestEcho(ram, ch, echo);
    }
}

inline bool stepOne(std::uint8_t* ram, Cpu& cpu, const EchoFn& echo, const NewlineFn& nl) {
    if (cpu.halted) return false;
    const std::uint8_t op = fetch8(ram, cpu.ip);
    if (op >= 0xB0u && op <= 0xB7u) {
        setReg8(cpu, static_cast<std::uint8_t>(op - 0xB0u), fetch8(ram, cpu.ip));
    } else if (op >= 0xB8u && op <= 0xBFu) {
        const std::uint16_t imm = fetch16(ram, cpu.ip);
        const std::uint8_t reg = op - 0xB8u;
        if (reg == 0) cpu.ax = imm;
        else if (reg == 3) cpu.bx = imm;
        else if (reg == 2) cpu.dx = imm;
        else if (reg == 1) cpu.cx = imm;
        else if (reg == 7) cpu.di = imm;
    } else if (op == 0xBAu) {
        cpu.dx = fetch16(ram, cpu.ip);
    } else if (op == 0xB4u) {
        cpu.ax = (cpu.ax & 0x00FFu) | (static_cast<std::uint16_t>(fetch8(ram, cpu.ip)) << 8);
    } else if (op == 0x04u) {
        cpu.ax = static_cast<std::uint16_t>((cpu.ax & 0xFF00u) + fetch8(ram, cpu.ip));
    } else if (op == 0x05u) {
        cpu.ax = static_cast<std::uint16_t>(cpu.ax + fetch16(ram, cpu.ip));
    } else if (op == 0x3Du) {
        const std::uint16_t imm = fetch16(ram, cpu.ip);
        cpu.zf = (cpu.ax == imm);
        cpu.cf = (cpu.ax < imm);
        cpu.sf = (static_cast<std::int16_t>(cpu.ax) < 0);
    } else if (op == 0x39u && fetch8(ram, cpu.ip) == 0xC3u) {
        cpu.zf = (cpu.bx == cpu.ax);
        cpu.cf = (cpu.bx < cpu.ax);
        cpu.sf = (static_cast<std::int16_t>(cpu.bx) < static_cast<std::int16_t>(cpu.ax));
    } else if (op == 0x3Bu && fetch8(ram, cpu.ip) == 0xC3u) {
        cpu.zf = (cpu.ax == cpu.bx);
        cpu.cf = (cpu.ax < cpu.bx);
        cpu.sf = (static_cast<std::int16_t>(cpu.ax) < static_cast<std::int16_t>(cpu.bx));
    } else if (op == 0x2Bu && fetch8(ram, cpu.ip) == 0xC3u) {
        cpu.ax = static_cast<std::uint16_t>(cpu.ax - cpu.bx);
        cpu.zf = (cpu.ax == 0);
        cpu.cf = (cpu.ax > cpu.bx);
    } else if (op == 0x01u && fetch8(ram, cpu.ip) == 0xD8u) {
        cpu.ax = static_cast<std::uint16_t>(cpu.ax + cpu.bx);
        cpu.zf = (cpu.ax == 0);
    } else if (op == 0x33u && fetch8(ram, cpu.ip) == 0xC0u) {
        cpu.ax = 0;
        cpu.zf = true;
        cpu.cf = false;
    } else if (op >= 0x50u && op <= 0x57u) {
        const std::uint8_t reg = op - 0x50u;
        std::uint16_t v = cpu.ax;
        if (reg == 1) v = cpu.cx;
        else if (reg == 2) v = cpu.dx;
        else if (reg == 3) v = cpu.bx;
        else if (reg == 7) v = cpu.di;
        push16(ram, cpu, v);
    } else if (op >= 0x58u && op <= 0x5Fu) {
        const std::uint16_t v = pop16(ram, cpu);
        const std::uint8_t reg = op - 0x58u;
        if (reg == 0) cpu.ax = v;
        else if (reg == 1) cpu.cx = v;
        else if (reg == 2) cpu.dx = v;
        else if (reg == 3) cpu.bx = v;
        else if (reg == 7) cpu.di = v;
    } else if (op >= 0x48u && op <= 0x4Fu) {
        const std::uint8_t reg = op - 0x48u;
        std::uint16_t* p = &cpu.ax;
        if (reg == 1) p = &cpu.cx;
        else if (reg == 2) p = &cpu.dx;
        else if (reg == 3) p = &cpu.bx;
        else if (reg == 7) p = &cpu.di;
        --*p;
        cpu.zf = (*p == 0);
    } else if (op >= 0x40u && op <= 0x47u) {
        const std::uint8_t reg = op - 0x40u;
        std::uint16_t* p = &cpu.ax;
        if (reg == 1) p = &cpu.cx;
        else if (reg == 2) p = &cpu.dx;
        else if (reg == 3) p = &cpu.bx;
        else if (reg == 7) p = &cpu.di;
        ++*p;
    } else if (op == 0xCDu) {
        const std::uint8_t vec = fetch8(ram, cpu.ip);
        if (vec == 0x21u) {
            const std::uint8_t ah = static_cast<std::uint8_t>(cpu.ax >> 8);
            if (ah == 0x09u) printDosString(ram, cpu.ds, cpu.dx, echo, nl);
            else if (ah == 0x02u) guestEcho(ram, static_cast<char>(cpu.dx & 0xFFu), echo);
            else if (ah == 0x4Cu || cpu.ax == 0x4C00u) cpu.halted = true;
        }
    } else if (op == 0xC3u) {
        cpu.ip = PSP_LINEAR + pop16(ram, cpu);
    } else if (op == 0xEBu) {
        const std::int8_t rel = static_cast<std::int8_t>(fetch8(ram, cpu.ip));
        cpu.ip = static_cast<std::uint32_t>(static_cast<std::int32_t>(cpu.ip) + rel);
    } else if (op == 0xE8u) {
        const std::int16_t rel = static_cast<std::int16_t>(fetch16(ram, cpu.ip));
        const std::uint32_t retIp = cpu.ip;
        push16(ram, cpu, static_cast<std::uint16_t>(retIp - PSP_LINEAR));
        cpu.ip = static_cast<std::uint32_t>(static_cast<std::int32_t>(retIp) + rel);
    } else if (op == 0x74u) {
        const std::int8_t rel = static_cast<std::int8_t>(fetch8(ram, cpu.ip));
        if (cpu.zf) cpu.ip = static_cast<std::uint32_t>(static_cast<std::int32_t>(cpu.ip) + rel);
    } else if (op == 0x75u) {
        const std::int8_t rel = static_cast<std::int8_t>(fetch8(ram, cpu.ip));
        if (!cpu.zf) cpu.ip = static_cast<std::uint32_t>(static_cast<std::int32_t>(cpu.ip) + rel);
    } else if (op == 0x72u) {
        const std::int8_t rel = static_cast<std::int8_t>(fetch8(ram, cpu.ip));
        if (cpu.cf) cpu.ip = static_cast<std::uint32_t>(static_cast<std::int32_t>(cpu.ip) + rel);
    } else if (op == 0x77u) {
        const std::int8_t rel = static_cast<std::int8_t>(fetch8(ram, cpu.ip));
        if (!cpu.cf && !cpu.zf) cpu.ip = static_cast<std::uint32_t>(static_cast<std::int32_t>(cpu.ip) + rel);
    } else if (op == 0x76u) {
        const std::int8_t rel = static_cast<std::int8_t>(fetch8(ram, cpu.ip));
        if (cpu.cf || cpu.zf) cpu.ip = static_cast<std::uint32_t>(static_cast<std::int32_t>(cpu.ip) + rel);
    } else if (op == 0x73u) {
        const std::int8_t rel = static_cast<std::int8_t>(fetch8(ram, cpu.ip));
        if (!cpu.cf) cpu.ip = static_cast<std::uint32_t>(static_cast<std::int32_t>(cpu.ip) + rel);
    } else if (op == 0x89u || op == 0x8Bu) {
        const std::uint8_t modrm = fetch8(ram, cpu.ip);
        if ((modrm & 0xC0u) == 0xC0u) {
            const std::uint8_t reg = (modrm >> 3) & 7u;
            if (op == 0x8Bu) {
                if (reg == 0) cpu.ax = cpu.ax;
                else if (reg == 3) cpu.ax = cpu.bx;
                else if (reg == 1) cpu.ax = cpu.cx;
                else if (reg == 2) cpu.ax = cpu.dx;
                else if (reg == 7) cpu.ax = cpu.di;
            } else {
                if (reg == 0) cpu.ax = cpu.ax;
                else if (reg == 3) cpu.bx = cpu.ax;
                else if (reg == 1) cpu.cx = cpu.ax;
                else if (reg == 2) cpu.dx = cpu.ax;
                else if (reg == 7) cpu.di = cpu.ax;
            }
        } else return false;
    } else {
        return false;
    }
    return true;
}

inline bool loadCom(std::uint8_t* ram, const std::vector<std::uint8_t>& com) {
    if (!ram || com.empty() || com.size() > 65280u) return false;
    const std::uint32_t base = PSP_LINEAR;
    std::memset(ram + base, 0, 0x100u);
    std::memcpy(ram + base + 0x100u, com.data(), com.size());
    ram[BDA_CUR_COL] = 0;
    ram[BDA_CUR_ROW] = 0;
    return true;
}

inline bool loadMz(std::uint8_t* ram, const std::vector<std::uint8_t>& img,
                   std::uint16_t loadSeg, std::uint16_t& entryIp,
                   std::uint16_t& entryCs, std::uint16_t& entrySs, std::uint16_t& entrySp) {
    if (!ram || img.size() < 32u || img[0] != 'M' || img[1] != 'Z') return false;
    const std::uint16_t hdrPar = static_cast<std::uint16_t>(img[8] | (img[9] << 8));
    const std::uint32_t hdrBytes = static_cast<std::uint32_t>(hdrPar) * 16u;
    if (hdrBytes >= img.size()) return false;
    entryIp = static_cast<std::uint16_t>(img[0x14] | (img[0x15] << 8));
    entryCs = static_cast<std::uint16_t>(img[0x16] | (img[0x17] << 8));
    entrySs = static_cast<std::uint16_t>(img[0x0E] | (img[0x0F] << 8));
    entrySp = static_cast<std::uint16_t>(img[0x10] | (img[0x11] << 8));
    const std::uint32_t base = static_cast<std::uint32_t>(loadSeg) << 4;
    const std::uint32_t loadSize = static_cast<std::uint32_t>(img.size()) - hdrBytes;
    if (base + loadSize > FieldPlatform::GUEST_RAM_BYTES) return false;
    const std::uint16_t pspSeg = static_cast<std::uint16_t>(loadSeg - 0x10u);
    const std::uint32_t pspBase = static_cast<std::uint32_t>(pspSeg) << 4;
    std::memset(ram + pspBase, 0, 0x100u);
    ram[pspBase] = 0xCDu;
    ram[pspBase + 1u] = 0x20u;
    std::memset(ram + pspBase + 0x20u, 0xFFu, 20u);
    std::memcpy(ram + base, img.data() + hdrBytes, loadSize);
    const std::uint16_t relCount = static_cast<std::uint16_t>(img[6] | (img[7] << 8));
    const std::uint16_t relOff  = static_cast<std::uint16_t>(img[0x18] | (img[0x19] << 8));
    for (std::uint16_t ri = 0; ri < relCount; ++ri) {
        const std::uint32_t ro = static_cast<std::uint32_t>(relOff) + ri * 4u;
        if (ro + 4u > img.size()) break;
        const std::uint16_t roff = static_cast<std::uint16_t>(img[ro] | (img[ro + 1] << 8));
        const std::uint16_t rseg = static_cast<std::uint16_t>(img[ro + 2] | (img[ro + 3] << 8));
        const std::uint32_t addr = base + (static_cast<std::uint32_t>(rseg) << 4) + roff;
        if (addr + 2u > FieldPlatform::GUEST_RAM_BYTES) continue;
        const std::uint16_t word = static_cast<std::uint16_t>(ram[addr])
            | static_cast<std::uint16_t>(static_cast<std::uint16_t>(ram[addr + 1u]) << 8);
        const std::uint16_t fixed = static_cast<std::uint16_t>(word + loadSeg);
        ram[addr] = static_cast<std::uint8_t>(fixed & 0xFFu);
        ram[addr + 1u] = static_cast<std::uint8_t>(fixed >> 8);
    }
    return true;
}

inline Result runCpu(std::uint8_t* ram, Cpu& cpu, const EchoFn& echo, const NewlineFn& nl,
                     std::uint32_t maxSteps = 65536u) {
    Result r;
    if (!ram) { r.error = "no guest RAM"; return r; }
    for (std::uint32_t step = 0; step < maxSteps && !cpu.halted; ++step) {
        if (!stepOne(ram, cpu, echo, nl)) {
            r.error = "unsupported opcode during AMMORUN";
            r.cpu = cpu;
            return r;
        }
        r.steps = step + 1u;
    }
    r.ok = true;
    r.cpu = cpu;
    return r;
}

inline Result runLoaded(std::uint8_t* ram, const EchoFn& echo, const NewlineFn& nl,
                        std::uint32_t entryOffset = 0u, std::uint32_t maxSteps = 65536u) {
    Cpu cpu;
    cpu.sp = 0xFFFEu;
    cpu.ip = PSP_LINEAR + 0x100u + entryOffset;
    return runCpu(ram, cpu, echo, nl, maxSteps);
}

inline Result runCom(std::uint8_t* ram, const std::vector<std::uint8_t>& com,
                     const EchoFn& echo, const NewlineFn& nl,
                     std::uint32_t entryOffset = 0u) {
    if (!loadCom(ram, com))
        return Result{false, "COM load failed", 0, {}};
    return runLoaded(ram, echo, nl, entryOffset);
}

inline Result runExe(std::uint8_t* ram, const std::vector<std::uint8_t>& exe,
                     const EchoFn& echo, const NewlineFn& nl,
                     std::uint16_t loadSeg = 0x1000u) {
    std::uint16_t eIp = 0, eCs = 0, eSs = 0, eSp = 0;
    if (!loadMz(ram, exe, loadSeg, eIp, eCs, eSs, eSp))
        return Result{false, "EXE load failed", 0, {}};
    Cpu cpu;
    cpu.ds = cpu.es = loadSeg;
    cpu.ss = static_cast<std::uint16_t>(loadSeg + eSs);
    cpu.sp = eSp;
    cpu.ip = (static_cast<std::uint32_t>(loadSeg + eCs) << 4) + eIp;
    return runCpu(ram, cpu, echo, nl);
}

inline Result runPath(std::uint8_t* ram, const char* comPath,
                      const EchoFn& echo, const NewlineFn& nl,
                      std::uint32_t entryOffset = 0u) {
    std::vector<std::uint8_t> image;
    if (!FieldAmmoFat::readFile(comPath, image) || image.empty())
        return Result{false, "cannot read file", 0, {}};
    if (image.size() >= 2u && image[0] == 'M' && image[1] == 'Z')
        return runExe(ram, image, echo, nl);
    return runCom(ram, image, echo, nl, entryOffset);
}

} // namespace FieldAmmoRun