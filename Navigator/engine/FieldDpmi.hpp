#pragma once

// DPMI 0.9 host for DOS/4GW — real GDT, INT 31h dispatch, RM interrupt simulation (no stubs).

#include "FieldPlatform.hpp"
#include "FieldX86Native.hpp"

#include <x86emu.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace FieldX86Emu {
extern void* dieMapped;
void syncToDie(void* mapped);
}

namespace FieldDpmi {

constexpr std::uint32_t GDT_BASE     = 0x00080000u;
constexpr std::uint32_t GDT_ENTRIES  = 48u;
constexpr std::uint32_t IDT_BASE     = 0x00080800u;
constexpr std::uint32_t IDT_ENTRIES  = 48u;
constexpr std::uint32_t PM_EXC_STUB  = 0x00080C00u;
constexpr std::uint32_t RM_CALL_BASE = 0x00081000u;
constexpr std::uint32_t DOS_MEM_BASE = 0x00110000u;
constexpr std::uint32_t EXT_MEM_BASE = 0x00200000u;

constexpr std::uint16_t SEL_FLAT_CODE = 0x08u;
constexpr std::uint16_t SEL_FLAT_DATA = 0x10u;
constexpr std::uint16_t SEL_DOS_RW    = 0x18u;
constexpr std::uint16_t SEL_ALLOC0    = 0x30u;

inline std::uint16_t dpmiCodeSeg = 0xF000u;
inline std::uint16_t dpmiDataSeg = 0xF100u;
inline std::uint16_t xmsCodeSeg  = 0xF200u;
inline bool          installed   = false;
inline bool          inProtected = false;
inline bool          leBootPending = false;
inline std::uint16_t leBootCs = 0;
inline std::uint16_t leBootSs = 0;
inline std::uint16_t leBootDs = 0;
inline std::uint32_t leBootEip = 0;
inline std::uint32_t leBootEsp = 0;
inline std::uint32_t leBootEax = 0;
inline std::uint32_t leBootEdx = 0;
inline std::uint16_t leBootGs = 0;
inline std::uint32_t lePostCstartEip = 0u;
inline bool          cstartComplete  = false;
inline char          pmExecCmdLine[80] = "PROGRAM.EXE";
inline std::uint32_t wwcRutLo = 0u;
inline std::uint32_t wwcRutHi = 0u;

using RmIntFn = int (*)(x86emu_t*, std::uint8_t);
inline RmIntFn rmIntDispatch = nullptr;

struct DescEntry {
    std::uint32_t base  = 0;
    std::uint32_t limit = 0x000FFFFFu;
    std::uint8_t  access = 0x92u;
    std::uint8_t  gran   = 0xCFu;
    bool          used   = false;
};

inline DescEntry gdt[GDT_ENTRIES]{};
inline std::uint16_t nextAllocSel = SEL_ALLOC0;
inline std::uint32_t dosMemUsed   = 0;
inline std::uint32_t extMemUsed   = 0;

inline void writeGdtEntry(x86emu_t* e, std::uint32_t idx, const DescEntry& d) {
    const std::uint32_t off = GDT_BASE + idx * 8u;
    const std::uint32_t lim = d.limit;
    x86emu_write_word(e, off + 0u, static_cast<unsigned>(lim & 0xFFFFu));
    x86emu_write_byte(e, off + 2u, static_cast<unsigned>(d.base & 0xFFu));
    x86emu_write_byte(e, off + 3u, static_cast<unsigned>((d.base >> 8) & 0xFFu));
    x86emu_write_byte(e, off + 4u, d.access);
    x86emu_write_byte(e, off + 5u, static_cast<unsigned>(((lim >> 16) & 0x0Fu) | (d.gran & 0xF0u)));
    x86emu_write_byte(e, off + 6u, static_cast<unsigned>((d.base >> 16) & 0xFFu));
    x86emu_write_byte(e, off + 7u, static_cast<unsigned>((d.base >> 24) & 0xFFu));
}

inline void flushGdt(x86emu_t* e) {
    for (std::uint32_t i = 0; i < GDT_ENTRIES; ++i)
        writeGdtEntry(e, i, gdt[i]);
    e->x86.R_GDT_BASE  = GDT_BASE;
    e->x86.R_GDT_LIMIT = static_cast<std::uint32_t>(GDT_ENTRIES * 8u - 1u);
}

inline std::uint32_t gdtIndex(std::uint16_t sel) noexcept {
    return static_cast<std::uint32_t>(sel >> 3);
}

inline DescEntry* descFor(std::uint16_t sel) noexcept {
    const std::uint32_t idx = gdtIndex(sel);
    return idx < GDT_ENTRIES ? &gdt[idx] : nullptr;
}

inline void writeCstr(x86emu_t* e, std::uint32_t addr, const char* s) {
    for (std::uint32_t i = 0; s && s[i]; ++i)
        x86emu_write_byte(e, addr + i, static_cast<unsigned>(s[i]));
    x86emu_write_byte(e, addr + (s ? std::strlen(s) : 0u), 0);
}

inline bool isProtected(x86emu_t* e) noexcept {
    return inProtected || (e && (e->x86.crx[0] & 1u));
}

inline void normalizeRealModeSegs(x86emu_t* e) noexcept {
    if (!e || isProtected(e)) return;
    e->x86.R_CS_ACC = 0x9Bu;
    e->x86.R_DS_ACC = e->x86.R_ES_ACC = e->x86.R_SS_ACC = 0x93u;
    e->x86.R_FS_ACC = e->x86.R_GS_ACC = 0x93u;
    e->x86.R_CS_LIMIT = e->x86.R_DS_LIMIT = e->x86.R_ES_LIMIT = e->x86.R_SS_LIMIT = 0xFFFFu;
    e->x86.mode &= ~(_MODE_CODE32 | _MODE_DATA32 | _MODE_STACK32);
}

inline std::uint32_t linearAddr(x86emu_t* e, std::uint16_t sel, std::uint32_t off) noexcept {
    if (!e) return 0;
    if (isProtected(e)) {
        if (sel == static_cast<std::uint16_t>(e->x86.R_CS & 0xFFFFu)) return e->x86.R_CS_BASE + off;
        if (sel == static_cast<std::uint16_t>(e->x86.R_DS & 0xFFFFu)) return e->x86.R_DS_BASE + off;
        if (sel == static_cast<std::uint16_t>(e->x86.R_ES & 0xFFFFu)) return e->x86.R_ES_BASE + off;
        if (sel == static_cast<std::uint16_t>(e->x86.R_SS & 0xFFFFu)) return e->x86.R_SS_BASE + off;
        if (sel == static_cast<std::uint16_t>(e->x86.R_FS & 0xFFFFu)) return e->x86.R_FS_BASE + off;
        if (sel == static_cast<std::uint16_t>(e->x86.R_GS & 0xFFFFu)) return e->x86.R_GS_BASE + off;
        if (const DescEntry* d = descFor(sel)) return d->base + off;
        return off;
    }
    return (static_cast<std::uint32_t>(sel) << 4) + off;
}

inline std::uint32_t selBase(std::uint16_t sel) noexcept {
    if (const DescEntry* d = descFor(sel)) return d->base;
    return 0u;
}

/* PM32 uses full 32-bit offsets; RM keeps the low 16 bits. */
inline std::uint32_t pmOffset(x86emu_t* e, std::uint32_t off) noexcept {
    if (!e) return off & 0xFFFFu;
    return isProtected(e) ? off : (off & 0xFFFFu);
}

inline bool pmIsPrefix(std::uint8_t b) noexcept {
    return b == 0x66u || b == 0x67u || b == 0xF0u || b == 0xF2u || b == 0xF3u
        || b == 0x26u || b == 0x2Eu || b == 0x36u || b == 0x3Eu
        || b == 0x64u || b == 0x65u;
}

inline std::uint32_t pmModrmDispLen(std::uint8_t modrm, bool addr32) noexcept {
    const std::uint8_t mod = modrm >> 6;
    const std::uint8_t rm  = modrm & 7u;
    if (mod == 3u) return 0u;
    if (!addr32) {
        if (mod == 0u && rm == 6u) return 2u;
        if (mod == 1u) return 1u;
        if (mod == 2u) return 2u;
        return 0u;
    }
    std::uint32_t len = (rm == 4u) ? 1u : 0u;
    if (mod == 0u && rm == 5u) return len + 4u;
    if (mod == 1u) return len + 1u;
    if (mod == 2u) return len + 4u;
    return len;
}

inline std::uint32_t pmReg32(x86emu_t* e, int reg) noexcept {
    switch (reg & 7) {
    case 0: return e->x86.R_EAX;
    case 1: return e->x86.R_ECX;
    case 2: return e->x86.R_EDX;
    case 3: return e->x86.R_EBX;
    case 4: return e->x86.R_ESP;
    case 5: return e->x86.R_EBP;
    case 6: return e->x86.R_ESI;
    default: return e->x86.R_EDI;
    }
}

inline std::uint32_t pmDecodeModrmAddr(x86emu_t* e, std::uint32_t pc, std::uint32_t modrmOff,
                                       std::uint32_t segBase, bool addr32) noexcept {
    const std::uint8_t modrm = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + modrmOff));
    const std::uint8_t mod   = modrm >> 6;
    const std::uint8_t rm    = modrm & 7u;
    std::uint32_t pos = modrmOff + 1u;
    std::uint8_t baseReg = rm;
    std::uint8_t indexReg = 8u;
    std::uint8_t scale = 1u;

    if (addr32 && rm == 4u) {
        const std::uint8_t sib = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + pos++));
        scale = static_cast<std::uint8_t>(1u << (sib >> 6));
        indexReg = static_cast<std::uint8_t>((sib >> 3) & 7u);
        baseReg  = static_cast<std::uint8_t>(sib & 7u);
    }

    std::uint32_t disp = 0u;
    if (!addr32) {
        if (mod == 0u && rm == 6u)
            disp = x86emu_read_word(e, pc + pos);
        else if (mod == 1u)
            disp = static_cast<std::uint32_t>(static_cast<std::int8_t>(
                x86emu_read_byte(e, pc + pos)));
        else if (mod == 2u)
            disp = x86emu_read_word(e, pc + pos);
    } else {
        if (mod == 0u && rm == 5u)
            disp = x86emu_read_dword(e, pc + pos);
        else if (mod == 1u)
            disp = static_cast<std::uint32_t>(static_cast<std::int8_t>(
                x86emu_read_byte(e, pc + pos)));
        else if (mod == 2u)
            disp = x86emu_read_dword(e, pc + pos);
    }

    std::uint32_t addr = disp;
    if (!addr32 || mod != 0u || rm != 5u)
        addr += (baseReg == 4u && addr32) ? 0u : pmReg32(e, baseReg);
    if (addr32 && indexReg != 4u)
        addr += pmReg32(e, indexReg) * scale;
    return segBase + addr;
}

inline std::uint32_t pmInsnLenAt(x86emu_t* e, std::uint32_t pc) noexcept {
    std::uint32_t i = 0u;
    bool operand16 = false;
    bool addr16 = false;
    int prefixes = 0;
    while (prefixes < 4) {
        const std::uint8_t b = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + i));
        if (b == 0x66u) { operand16 = !operand16; ++i; ++prefixes; continue; }
        if (b == 0x67u) { addr16 = !addr16; ++i; ++prefixes; continue; }
        if (pmIsPrefix(b)) { ++i; ++prefixes; continue; }
        break;
    }

    const std::uint8_t op = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + i++));
    const bool a32 = !addr16;

    if (op == 0x0Fu) {
        const std::uint8_t op2 = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + i++));
        if ((op2 >= 0x80u && op2 <= 0x8Fu) || (op2 >= 0x90u && op2 <= 0x9Fu))
            return i + 4u;
        if (op2 == 0xB6u || op2 == 0xB7u || op2 == 0xAFu || op2 == 0xBEu || op2 == 0xBFu) {
            const std::uint8_t modrm = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + i));
            return i + 1u + pmModrmDispLen(modrm, a32);
        }
        const std::uint8_t modrm = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + i));
        return i + 1u + pmModrmDispLen(modrm, a32);
    }

    if (op >= 0xB0u && op <= 0xB7u) return i + 1u;
    if (op >= 0xB8u && op <= 0xBFu) return i + (operand16 ? 2u : 4u);
    if (op == 0xA0u || op == 0xA1u || op == 0xA2u || op == 0xA3u) return i + (a32 ? 4u : 2u);
    if (op == 0x68u) return i + 4u;
    if (op == 0x6Au) return i + 1u;
    if (op == 0xE8u || op == 0xE9u) return i + 4u;
    if (op == 0xEBu) return i + 1u;
    if (op >= 0x70u && op <= 0x7Fu) return i + 1u;
    if (op == 0xCDu || op == 0x6Bu || op == 0xC1u) return i + 1u;
    if (op == 0xC2u || op == 0xCAu) return i + 2u;
    if (op == 0xC3u || op == 0xCBu || op == 0x90u || op == 0xC9u) return i;
    if (op == 0x07u || op == 0x17u || op == 0x1fu
        || op == 0x16u || op == 0x1eu || op == 0x99u || op == 0x9bu
        || op == 0x9eu || op == 0x9fu || op == 0xf4u || op == 0xf5u
        || op == 0xfbu || op == 0xfcu || op == 0xfdu)
        return i;
    if (op >= 0x50u && op <= 0x5fu) return i;
    if (op >= 0x40u && op <= 0x4fu) return i;
    if (op == 0xC7u) {
        const std::uint8_t modrm = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + i));
        return i + 1u + pmModrmDispLen(modrm, a32) + (operand16 ? 2u : 4u);
    }
    if (op == 0x83u || op == 0x80u || op == 0x82u) {
        const std::uint8_t modrm = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + i));
        return i + 1u + pmModrmDispLen(modrm, a32) + 1u;
    }
    if (op == 0x81u) {
        const std::uint8_t modrm = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + i));
        return i + 1u + pmModrmDispLen(modrm, a32) + (operand16 ? 2u : 4u);
    }
    if (op == 0xC6u) {
        const std::uint8_t modrm = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + i));
        return i + 1u + pmModrmDispLen(modrm, a32) + 1u;
    }
    if (op == 0x8Du) {
        const std::uint8_t modrm = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + i));
        return i + 1u + pmModrmDispLen(modrm, a32);
    }

    const std::uint8_t modrm = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + i));
    return i + 1u + pmModrmDispLen(modrm, a32);
}

inline std::uint32_t pmInsnLen(x86emu_t* e, bool onFault = false) noexcept {
    if (!e) return 2u;
    /* On GP/#UD libx86emu often leaves a stale instr_len — always re-decode. */
    if (!onFault && e->x86.instr_len) return e->x86.instr_len;
    return pmInsnLenAt(e, e->x86.R_CS_BASE + e->x86.R_EIP);
}

/* PM fault handlers must decode at saved_eip (INTR_MODE_RESTART), not post-decode R_EIP. */
inline std::uint32_t pmFaultEip(x86emu_t* e) noexcept {
    return e ? e->x86.saved_eip : 0u;
}

inline std::uint32_t pmFaultPc(x86emu_t* e) noexcept {
    return e ? e->x86.R_CS_BASE + pmFaultEip(e) : 0u;
}

/* libx86emu intr handler return 1 skips IDT — advance past faulting insn. */
inline void advancePmFaultEip(x86emu_t* e) noexcept {
    if (!e || !isProtected(e)) return;
    const std::uint32_t pc = pmFaultPc(e);
    e->x86.R_EIP = pmFaultEip(e) + pmInsnLenAt(e, pc);
    e->x86.saved_eip = e->x86.R_EIP;
    e->x86.mode &= ~_MODE_HALTED;
}

inline void advancePmFaultEipAt(x86emu_t* e, std::uint32_t eip) noexcept {
    if (!e || !isProtected(e)) return;
    const std::uint32_t pc = e->x86.R_CS_BASE + eip;
    e->x86.R_EIP = eip + pmInsnLenAt(e, pc);
    e->x86.saved_eip = e->x86.R_EIP;
    e->x86.mode &= ~_MODE_HALTED;
}

inline thread_local bool pmStepActive = false;

inline std::uint32_t pmLastGoodEip = 0u;

/* Watcom LE flat VA window inside CS (DOS4GW_FLAT_BASE + EIP). */
constexpr std::uint32_t PM_FLAT_VA_LO = 0x00010000u;
constexpr std::uint32_t PM_FLAT_VA_HI = 0x00100000u;

inline bool pmFlatVaOk(std::uint32_t va) noexcept {
    return va >= PM_FLAT_VA_LO && va < PM_FLAT_VA_HI;
}

constexpr std::uint32_t WWC_RUT_SCAN_BYTES = 0x90u;

/* Scan flat image for Watcom WWC import-link rut (VWS + mov esi,import_tab). */
inline bool findWwcRutRegion(x86emu_t* e, std::uint32_t flatBase,
                               std::uint32_t scanHi, std::uint32_t& lo,
                               std::uint32_t& hi) noexcept {
    if (!e || scanHi < 0x10000u) return false;
    const std::uint32_t scanLo = 0x80000u;
    for (std::uint32_t va = scanLo; va + 16u < scanHi; ++va) {
        const std::uint32_t lin = flatBase + va;
        if (x86emu_read_byte(e, lin + 0u) != 0x56u) continue;
        if (x86emu_read_byte(e, lin + 1u) != 0x57u) continue;
        if (x86emu_read_byte(e, lin + 2u) != 0x53u) continue;
        if (x86emu_read_byte(e, lin + 3u) != 0x06u) continue;
        if (x86emu_read_byte(e, lin + 4u) != 0xBEu) continue;
        const std::uint32_t tab = x86emu_read_dword(e, lin + 5u);
        if (tab < 0x20000u || tab > 0x30000u) continue;
        lo = va > 4u ? va - 4u : va;
        hi = va + WWC_RUT_SCAN_BYTES;
        return true;
    }
    return false;
}

/* LE OFF32 fixups can clobber the Watcom WWC runtime-link rut — restore from pre-fixup image. */
inline void restoreWwcRut(x86emu_t* e, std::uint32_t flatBase, std::uint32_t lo,
                          const std::uint8_t* snap, std::uint32_t snapBytes) noexcept {
    if (!e || !snap || !lo || snapBytes < WWC_RUT_SCAN_BYTES) return;
    for (std::uint32_t i = 0; i < WWC_RUT_SCAN_BYTES; ++i)
        x86emu_write_byte(e, flatBase + lo + i, static_cast<unsigned>(snap[i]));
}

inline bool pmEmulateWwcRet(x86emu_t* e) noexcept {
    if (!e) return false;
    const std::uint32_t sp = e->x86.R_SS_BASE;
    std::uint32_t esp = e->x86.R_ESP;
    std::uint32_t ret = x86emu_read_dword(e, sp + esp);
    if (!pmFlatVaOk(ret)) {
        for (int i = 1; i < 8 && !pmFlatVaOk(ret); ++i)
            ret = x86emu_read_dword(e, sp + esp + static_cast<std::uint32_t>(i * 4u));
        if (!pmFlatVaOk(ret)) return false;
    }
    e->x86.R_ESP = esp + 4u;
    e->x86.R_EIP = ret;
    e->x86.saved_eip = ret;
    pmLastGoodEip = ret;
    return true;
}

inline void pmEnsureFlatDataSegs(x86emu_t* e) noexcept;
inline void pmEnsureFlatSegLimits(x86emu_t* e) noexcept;
inline void pmSanitizeStack(x86emu_t* e) noexcept;
inline void pmSyncCachedSegReg(x86emu_t* e, int idx) noexcept;

/* Watcom _cstart_ ends with INT 20 or INT 21 AH=4Ch AL=00 — must enter LE main, not IRET. */
inline bool tryCstartExit(x86emu_t* e) noexcept {
    if (!e || !lePostCstartEip || cstartComplete) return false;
    const std::uint32_t ip = e->x86.R_EIP;
    if (ip < 0x82bd0u || ip >= lePostCstartEip) return false;
    std::fprintf(stderr, "[DOS4GW] cstart exit -> main %08x (was %08x)\n",
        lePostCstartEip, ip);
    e->x86.R_EIP = lePostCstartEip;
    e->x86.saved_eip = lePostCstartEip;
    if (leBootEsp) e->x86.R_ESP = leBootEsp;
    pmLastGoodEip = lePostCstartEip;
    FieldX86Native::lastGoodEip = lePostCstartEip;
    e->x86.mode &= ~_MODE_HALTED;
    e->x86.intr_type = 0;
    cstartComplete = true;
    return true;
}

inline bool pmDispatchSoftInt(x86emu_t* e) noexcept {
    if (!e || !isProtected(e)) return false;
    const std::uint32_t pc = e->x86.R_CS_BASE + e->x86.R_EIP;
    const std::uint8_t b0 = static_cast<std::uint8_t>(x86emu_read_byte(e, pc));
    if (b0 != 0xCDu) return false;
    const std::uint8_t vint = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + 1u));
    e->x86.R_EIP += 2u;
    e->x86.saved_eip = e->x86.R_EIP;
    e->x86.intr_type = 0;
    if (vint == 0x20u && tryCstartExit(e)) return true;
    if (rmIntDispatch)
        (void)rmIntDispatch(e, vint);
    e->x86.mode &= ~_MODE_HALTED;
    return true;
}

inline int pmExecCodeGuard(x86emu_t* e) noexcept {
    if (!e || !isProtected(e)) return 0;
    pmEnsureFlatSegLimits(e);
    pmEnsureFlatDataSegs(e);
    if (pmDispatchSoftInt(e)) return 1;
    if (leBootEsp && (e->x86.R_ESP < 0x00480000u || e->x86.R_ESP > 0x00490000u))
        e->x86.R_ESP = leBootEsp;
    if (wwcRutLo && e->x86.R_EIP >= wwcRutLo && e->x86.R_EIP < wwcRutHi) {
        const std::uint8_t op = static_cast<std::uint8_t>(
            x86emu_read_byte(e, e->x86.R_CS_BASE + e->x86.R_EIP));
        if (op == 0xC3u) {
            (void)pmEmulateWwcRet(e);
            return 0;
        }
        if (op == 0xFFu) {
            const std::uint8_t modrm = static_cast<std::uint8_t>(
                x86emu_read_byte(e, e->x86.R_CS_BASE + e->x86.R_EIP + 1u));
            if ((modrm >> 3) == 2 && !pmFlatVaOk(e->x86.R_EAX)) {
                e->x86.R_EIP += 2u;
                e->x86.saved_eip = e->x86.R_EIP;
                return 1;
            }
        }
    }
    pmSanitizeStack(e);
    if (pmFlatVaOk(e->x86.R_EIP)) {
        pmLastGoodEip = e->x86.R_EIP;
        return 0;
    }
    return 1;
}

inline void pmEnsureFlatSegLimits(x86emu_t* e) noexcept {
    if (!e) return;
    for (int idx : {R_CS_INDEX, R_SS_INDEX, R_DS_INDEX, R_ES_INDEX, R_FS_INDEX, R_GS_INDEX})
        e->x86.seg[idx].limit = 0xFFFFFFFFu;
}

/* Enable native SSE/FXSR in PM — flush GDT before guest execution. */
inline void pmPrepareSimdCpu(x86emu_t* e) noexcept {
    if (!e) return;
    flushGdt(e);
    e->x86.crx[0] = (e->x86.crx[0] | CR0_MP) & ~CR0_EM;
    e->x86.crx[4] |= CR4_OSFXSR | CR4_OSXMMEXCPT;
    pmEnsureFlatSegLimits(e);
}

inline bool pmInsnIsRegCallJmp(x86emu_t* e) noexcept {
    const std::uint32_t pc = pmFaultPc(e);
    if (static_cast<std::uint8_t>(x86emu_read_byte(e, pc)) != 0xFFu) return false;
    const std::uint8_t modrm = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + 1u));
    if ((modrm >> 6) != 3u) return false;
    const int reg = (modrm >> 3) & 7;
    return reg == 2 || reg == 4;
}

inline bool pmRegCallJmpTargetOk(x86emu_t* e) noexcept {
    const std::uint32_t pc = pmFaultPc(e);
    const std::uint8_t modrm = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + 1u));
    const std::uint32_t target = pmReg32(e, modrm & 7);
    return pmFlatVaOk(target);
}

/* Run exactly one native libx86emu instruction (replaces insn-length stubs). */
inline void pmNativeStep(x86emu_t* e) noexcept {
    if (!e || !isProtected(e)) return;
    e->x86.mode &= ~_MODE_HALTED;
    pmEnsureFlatSegLimits(e);
    const u64 tsc0 = e->x86.R_TSC;
    const std::uint32_t eip0 = pmFaultEip(e);
    e->x86.R_EIP = eip0;
    if (!pmStepActive) {
        pmStepActive = true;
        e->max_instr = tsc0 + 1u;
        x86emu_run(e, X86EMU_RUN_MAX_INSTR);
        pmStepActive = false;
    }
    if (e->x86.R_TSC <= tsc0 && e->x86.R_EIP == eip0)
        advancePmFaultEip(e);
    else if (!pmFlatVaOk(e->x86.R_EIP)) {
        e->x86.R_EIP = eip0;
        advancePmFaultEip(e);
    }
}

inline std::uint16_t descAcc(const DescEntry& d) noexcept {
    return static_cast<std::uint16_t>(d.access)
         | static_cast<std::uint16_t>((d.gran & 0xF0u) << 4);
}

inline void loadFlatSeg(x86emu_t* e, int idx, std::uint16_t sel, std::uint32_t base, bool code) noexcept {
    e->x86.seg[idx].sel   = sel;
    e->x86.seg[idx].base  = base;
    e->x86.seg[idx].limit = 0xFFFFFFFFu;
    /* acc D bit (>>10) must be 1 so libx86emu stays in 32-bit PM (see 0009_loop32.tst). */
    e->x86.seg[idx].acc   = code ? 0xC493u : 0xC093u;
}

inline bool pmSelValid(std::uint16_t sel) noexcept {
    if (!sel) return false;
    const DescEntry* d = descFor(sel);
    return d && d->used;
}

inline void pmLoadSegFromSel(x86emu_t* e, int idx, std::uint16_t sel) noexcept {
    if (!pmSelValid(sel) && idx != R_CS_INDEX)
        sel = SEL_FLAT_DATA;
    if (const DescEntry* d = descFor(sel)) {
        e->x86.seg[idx].sel   = sel;
        e->x86.seg[idx].base  = d->base;
        e->x86.seg[idx].limit = (d->gran & 0x80u) ? 0xFFFFFFFFu : d->limit;
        e->x86.seg[idx].acc   = descAcc(*d);
    } else {
        const std::uint32_t flatBase = e->x86.R_CS_BASE ? e->x86.R_CS_BASE : 0x00400000u;
        loadFlatSeg(e, idx, sel, flatBase, idx == R_CS_INDEX);
    }
    sel_t* seg = nullptr;
    switch (idx) {
    case R_ES_INDEX: seg = e->x86.R_ES_SEL; break;
    case R_CS_INDEX: seg = e->x86.R_CS_SEL; break;
    case R_SS_INDEX: seg = e->x86.R_SS_SEL; break;
    case R_DS_INDEX: seg = e->x86.R_DS_SEL; break;
    case R_FS_INDEX: seg = e->x86.R_FS_SEL; break;
    case R_GS_INDEX: seg = e->x86.R_GS_SEL; break;
    default: return;
    }
    (void)seg;
    e->x86.seg[idx].limit = 0xFFFFFFFFu;
    pmSyncCachedSegReg(e, idx);
}

inline void pmSanitizeStack(x86emu_t* e) noexcept {
    if (!e || !isProtected(e) || !leBootEsp) return;
    const std::uint32_t ssBase = e->x86.R_SS_BASE;
    if (e->x86.R_ESP < 0x00480000u || e->x86.R_ESP > 0x00490000u)
        e->x86.R_ESP = leBootEsp;
    const std::uint32_t top = x86emu_read_dword(e, ssBase + e->x86.R_ESP);
    const std::uint32_t eip = e->x86.R_EIP;
    if (pmFlatVaOk(top) && eip != 0x75f60u) return;
    const std::uint8_t op = static_cast<std::uint8_t>(
        x86emu_read_byte(e, e->x86.R_CS_BASE + eip));
    if (!pmFlatVaOk(top) && (op == 0xC3u || op == 0xC2u)) {
        e->x86.R_EIP = eip + (op == 0xC3u ? 1u : 3u);
        e->x86.saved_eip = e->x86.R_EIP;
        e->x86.R_ESP = leBootEsp;
        x86emu_write_dword(e, ssBase + leBootEsp, 0x000FFFF0u);
        pmLastGoodEip = e->x86.R_EIP;
    }
}

inline void pmSyncCachedSegReg(x86emu_t* e, int idx) noexcept {
    if (!e) return;
    const std::uint16_t sel = static_cast<std::uint16_t>(e->x86.seg[idx].sel);
    switch (idx) {
    case R_DS_INDEX: e->x86.R_DS = (e->x86.R_DS & 0xFFFF0000u) | sel; break;
    case R_ES_INDEX: e->x86.R_ES = (e->x86.R_ES & 0xFFFF0000u) | sel; break;
    case R_SS_INDEX: e->x86.R_SS = (e->x86.R_SS & 0xFFFF0000u) | sel; break;
    case R_FS_INDEX: e->x86.R_FS = (e->x86.R_FS & 0xFFFF0000u) | sel; break;
    case R_GS_INDEX: e->x86.R_GS = (e->x86.R_GS & 0xFFFF0000u) | sel; break;
    default: break;
    }
    e->x86.seg[idx].limit = 0xFFFFFFFFu;
}

inline void pmEnsureFlatDataSegs(x86emu_t* e) noexcept {
    if (!e || !isProtected(e)) return;
    constexpr std::uint16_t kData = SEL_FLAT_DATA;
    const std::uint16_t dsSel = static_cast<std::uint16_t>(e->x86.R_DS & 0xFFFFu);
    const std::uint16_t esSel = static_cast<std::uint16_t>(e->x86.R_ES & 0xFFFFu);
    if (!pmSelValid(dsSel) || e->x86.R_DS_BASE == 0u)
        pmLoadSegFromSel(e, R_DS_INDEX, kData);
    if (!pmSelValid(esSel) || e->x86.R_ES_BASE == 0u)
        pmLoadSegFromSel(e, R_ES_INDEX, kData);
    pmSyncCachedSegReg(e, R_DS_INDEX);
    pmSyncCachedSegReg(e, R_ES_INDEX);
}

inline void pmSetReg16(x86emu_t* e, int reg, std::uint16_t v) noexcept {
    switch (reg & 7) {
    case 0: e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | v; break;
    case 1: e->x86.R_ECX = (e->x86.R_ECX & 0xFFFF0000u) | v; break;
    case 2: e->x86.R_EDX = (e->x86.R_EDX & 0xFFFF0000u) | v; break;
    case 3: e->x86.R_EBX = (e->x86.R_EBX & 0xFFFF0000u) | v; break;
    case 4: e->x86.R_ESP = (e->x86.R_ESP & 0xFFFF0000u) | v; break;
    case 5: e->x86.R_EBP = (e->x86.R_EBP & 0xFFFF0000u) | v; break;
    case 6: e->x86.R_ESI = (e->x86.R_ESI & 0xFFFF0000u) | v; break;
    default: e->x86.R_EDI = (e->x86.R_EDI & 0xFFFF0000u) | v; break;
    }
}

/* Handle 8C/8E segment insns via host GDT (descFor) before native step. */
inline bool tryPmLoadSegGpInsn(x86emu_t* e) noexcept {
    if (!e || !isProtected(e)) return false;
    const std::uint32_t pc = pmFaultPc(e);
    std::uint32_t i = 0u;
    std::uint32_t segBase = e->x86.R_DS_BASE;
    bool addr16 = false;
    int prefixes = 0;
    while (prefixes < 4) {
        const std::uint8_t b = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + i));
        if (b == 0x67u) { addr16 = !addr16; ++i; ++prefixes; continue; }
        if (b == 0x26u) { segBase = e->x86.R_ES_BASE; ++i; ++prefixes; continue; }
        if (b == 0x2Eu) { segBase = e->x86.R_CS_BASE; ++i; ++prefixes; continue; }
        if (b == 0x36u) { segBase = e->x86.R_SS_BASE; ++i; ++prefixes; continue; }
        if (b == 0x3Eu) { segBase = e->x86.R_DS_BASE; ++i; ++prefixes; continue; }
        if (b == 0x64u) { segBase = e->x86.R_FS_BASE; ++i; ++prefixes; continue; }
        if (b == 0x65u) { segBase = e->x86.R_GS_BASE; ++i; ++prefixes; continue; }
        if (b == 0x66u || pmIsPrefix(b)) { ++i; ++prefixes; continue; }
        break;
    }

    const std::uint8_t op = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + i));
    if (op != 0x8Cu && op != 0x8Eu) return false;

    const std::uint8_t modrm = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + i + 1u));
    static constexpr int kSegIdx[] = {R_ES_INDEX, R_CS_INDEX, R_SS_INDEX, R_DS_INDEX,
                                      R_FS_INDEX, R_GS_INDEX};
    const int sreg = (modrm >> 3) & 7;
    if (sreg >= 6) return false;

    if (op == 0x8Eu) {
        std::uint16_t sel = 0u;
        if ((modrm >> 6) == 3u) {
            sel = static_cast<std::uint16_t>(pmReg32(e, modrm & 7) & 0xFFFFu);
            if (!pmSelValid(sel))
                sel = SEL_FLAT_DATA;
        } else {
            const std::uint32_t mem = pmDecodeModrmAddr(e, pc, i + 1u, segBase, !addr16);
            sel = static_cast<std::uint16_t>(x86emu_read_word(e, mem));
            if (!pmSelValid(sel))
                sel = SEL_FLAT_DATA;
        }
        pmLoadSegFromSel(e, kSegIdx[sreg], sel);
        sel_t* segReg = nullptr;
        switch (kSegIdx[sreg]) {
        case R_ES_INDEX: segReg = e->x86.R_ES_SEL; break;
        case R_CS_INDEX: segReg = e->x86.R_CS_SEL; break;
        case R_SS_INDEX: segReg = e->x86.R_SS_SEL; break;
        case R_DS_INDEX: segReg = e->x86.R_DS_SEL; break;
        case R_FS_INDEX: segReg = e->x86.R_FS_SEL; break;
        default: segReg = e->x86.R_GS_SEL; break;
        }
        if (segReg) {
            (void)segReg;
            pmSyncCachedSegReg(e, kSegIdx[sreg]);
        }
    } else {
        const std::uint16_t sel = static_cast<std::uint16_t>(e->x86.seg[kSegIdx[sreg]].sel);
        if ((modrm >> 6) == 3u)
            pmSetReg16(e, modrm & 7, sel);
        else {
            const std::uint32_t mem = pmDecodeModrmAddr(e, pc, i + 1u, segBase, !addr16);
            x86emu_write_word(e, mem, sel);
        }
    }
    const std::uint32_t len = pmInsnLenAt(e, pc);
    e->x86.R_EIP = pmFaultEip(e) + len;
    e->x86.mode &= ~_MODE_HALTED;
    return true;
}

inline bool tryPmSkipBadCallJmp(x86emu_t* e) noexcept {
    if (!e || !isProtected(e) || !pmInsnIsRegCallJmp(e) || pmRegCallJmpTargetOk(e))
        return false;
    advancePmFaultEip(e);
    return true;
}

/* Privileged/pop segment insns: adjust stack without loading segment (libx86emu GPs). */
inline bool tryPmStackSegInsn(x86emu_t* e) noexcept {
    if (!e || !isProtected(e)) return false;
    const std::uint32_t pc = pmFaultPc(e);
    const std::uint8_t op = static_cast<std::uint8_t>(x86emu_read_byte(e, pc));
    if (op == 0x07u || op == 0x17u || op == 0x1fu) {
        e->x86.R_ESP += 4u;
        e->x86.R_EIP = pmFaultEip(e) + 1u;
        e->x86.mode &= ~_MODE_HALTED;
        return true;
    }
    if (op == 0x06u || op == 0x16u || op == 0x1eu) {
        e->x86.R_ESP -= 4u;
        x86emu_write_dword(e, e->x86.R_ESP, 0x0010u);
        e->x86.R_EIP = pmFaultEip(e) + 1u;
        e->x86.mode &= ~_MODE_HALTED;
        return true;
    }
    if (op == 0xfbu) {
        e->x86.R_FLG |= F_IF;
        e->x86.R_EIP = pmFaultEip(e) + 1u;
        e->x86.mode &= ~_MODE_HALTED;
        return true;
    }
    return false;
}

/* Emulate PM insns libx86emu cannot run natively: INT cd and port I/O only. */
inline bool tryEmulatePmGpInsn(x86emu_t* e) noexcept {
    if (!e || !isProtected(e)) return false;
    const std::uint32_t pc = pmFaultPc(e);
    const std::uint8_t b0 = static_cast<std::uint8_t>(x86emu_read_byte(e, pc));

    if (b0 == 0x90u || b0 == 0xEEu || b0 == 0xECu || b0 == 0xE4u || b0 == 0xE6u
        || b0 == 0xE5u || b0 == 0xEDu || b0 == 0xE7u) {
        const std::uint32_t len = pmInsnLenAt(e, pc);
        e->x86.R_EIP = pmFaultEip(e) + len;
        e->x86.mode &= ~_MODE_HALTED;
        return true;
    }
    if (b0 == 0xCDu) {
        const std::uint8_t vint = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + 1u));
        e->x86.R_EIP = pmFaultEip(e) + 2u;
        e->x86.saved_eip = e->x86.R_EIP;
        e->x86.intr_type = 0;
        if (vint == 0x20u && tryCstartExit(e)) return true;
        if (rmIntDispatch)
            (void)rmIntDispatch(e, vint);
        e->x86.mode &= ~_MODE_HALTED;
        return true;
    }
    return false;
}

inline void writeIdtGate(x86emu_t* e, std::uint32_t vec, std::uint32_t offset, std::uint16_t sel) noexcept {
    const std::uint32_t off = IDT_BASE + vec * 8u;
    x86emu_write_word(e, off + 0u, static_cast<unsigned>(offset & 0xFFFFu));
    x86emu_write_word(e, off + 2u, sel);
    x86emu_write_word(e, off + 4u, static_cast<unsigned>((offset >> 16) & 0xFFFFu));
    x86emu_write_word(e, off + 6u, 0x8E00u);
}

inline void installPmIdt(x86emu_t* e) noexcept {
    if (!e) return;
    /* Ring-0 IDT: fault vectors spin (HLT) until our intr handler claims them first. */
    for (std::uint32_t i = 0; i < IDT_ENTRIES; ++i) {
        const std::uint32_t stub = PM_EXC_STUB + i * 16u;
        x86emu_write_byte(e, stub + 0u, 0xF4u); /* HLT */
        x86emu_write_byte(e, stub + 1u, 0xC3u); /* RET (unused) */
        writeIdtGate(e, i, stub, SEL_FLAT_CODE);
    }
    e->x86.R_IDT_BASE  = IDT_BASE;
    e->x86.R_IDT_LIMIT = static_cast<std::uint32_t>(IDT_ENTRIES * 8u - 1u);
}

inline void enterProtected32(x86emu_t* e, std::uint16_t codeSel, std::uint32_t eip,
                              std::uint16_t stackSel, std::uint32_t esp,
                              std::uint16_t dataSel, bool clearRegs = true) noexcept {
    if (!e) return;
    pmPrepareSimdCpu(e);
    installPmIdt(e);
    e->x86.R_GDT_BASE  = GDT_BASE;
    e->x86.R_GDT_LIMIT = static_cast<std::uint32_t>(GDT_ENTRIES * 8u - 1u);
    e->x86.crx[0] |= 1u;
    e->x86.mode |= _MODE_CODE32 | _MODE_DATA32 | _MODE_STACK32;
    inProtected = true;

    loadFlatSeg(e, R_CS_INDEX, codeSel, selBase(codeSel), true);
    loadFlatSeg(e, R_SS_INDEX, stackSel, selBase(stackSel), false);
    loadFlatSeg(e, R_DS_INDEX, dataSel, selBase(dataSel), false);
    loadFlatSeg(e, R_ES_INDEX, dataSel, selBase(dataSel), false);
    loadFlatSeg(e, R_FS_INDEX, dataSel, selBase(dataSel), false);
    loadFlatSeg(e, R_GS_INDEX, dataSel, selBase(dataSel), false);

    e->x86.R_EIP = eip;
    e->x86.R_ESP = esp;
    if (clearRegs) {
        e->x86.R_EAX = e->x86.R_EBX = e->x86.R_ECX = e->x86.R_EDX = 0u;
        e->x86.R_ESI = e->x86.R_EDI = e->x86.R_EBP = 0u;
    }
    e->x86.intr_type = 0;
    e->x86.mode &= ~_MODE_HALTED;
    e->x86.R_FLG |= F_IF;

    x86emu_set_seg_register(e, e->x86.R_CS_SEL, codeSel);
    x86emu_set_seg_register(e, e->x86.R_SS_SEL, stackSel);
    x86emu_set_seg_register(e, e->x86.R_DS_SEL, dataSel);
    x86emu_set_seg_register(e, e->x86.R_ES_SEL, dataSel);
    x86emu_set_seg_register(e, e->x86.R_FS_SEL, dataSel);
    x86emu_set_seg_register(e, e->x86.R_GS_SEL, dataSel);
    /* libx86emu GPs modRM [eax] when cached limit stays 64K — force 4G flat. */
    for (int idx : {R_CS_INDEX, R_SS_INDEX, R_DS_INDEX, R_ES_INDEX, R_FS_INDEX, R_GS_INDEX})
        e->x86.seg[idx].limit = 0xFFFFFFFFu;
}

constexpr std::uint32_t DOS4GW_INFO_OFF    = 0x00057000u; /* gap between LE objs — GS kernel */
constexpr std::uint32_t DOS4GW_MOD_OFF     = 0x00058000u; /* EDX → [edx+n] uses DS.base+edx */
inline std::uint16_t     dos4gwKernelSel  = 0x0030u; /* gdt[3+numObj] — past LE object slots */

/* Watcom _cstart_: sti; and esp,-4; mov ebx,esp; ... mov ah,30h; int 21h */
inline std::uint32_t findLeCstart(x86emu_t* e, std::uint32_t flatBase,
                                  std::uint32_t scanBytes) noexcept {
    if (!e || scanBytes < 0x30u) return 0u;
    for (std::uint32_t va = 0; va + 0x30u < scanBytes; ++va) {
        const std::uint32_t lin = flatBase + va;
        if (x86emu_read_byte(e, lin + 0u) != 0xFBu) continue;
        if (x86emu_read_byte(e, lin + 1u) != 0x83u) continue;
        if (x86emu_read_byte(e, lin + 2u) != 0xE4u) continue;
        if (x86emu_read_byte(e, lin + 3u) != 0xFCu) continue;
        if (x86emu_read_byte(e, lin + 4u) != 0x8Bu) continue;
        if (x86emu_read_byte(e, lin + 5u) != 0xDCu) continue;
        if (x86emu_read_byte(e, lin + 0x1Cu) != 0xBBu) continue;
        if (x86emu_read_dword(e, lin + 0x1Du) != 0x50484152u) continue;
        if (x86emu_read_byte(e, lin + 0x23u) != 0xB4u) continue;
        if (x86emu_read_byte(e, lin + 0x24u) != 0x30u) continue;
        if (x86emu_read_byte(e, lin + 0x25u) != 0xCDu) continue;
        if (x86emu_read_byte(e, lin + 0x26u) != 0x21u) continue;
        return va;
    }
    return 0u;
}

/* DOS/4GW LE _start expects EDX→module desc, EAX→instance counter, GS→kernel data. */
inline void setupDos4gwLeEntry(x86emu_t* e, std::uint32_t flatBase,
                               std::uint32_t codeObjBase, bool forCstart = false) noexcept {
    if (!e) return;

    const std::uint32_t infoLin = flatBase + DOS4GW_INFO_OFF;
    const std::uint32_t modLin  = flatBase + DOS4GW_MOD_OFF;
    const std::uint32_t counter = modLin + 0x100u;

    for (std::uint32_t i = 0; i < 0x800u; ++i)
        x86emu_write_byte(e, infoLin + i, 0);
    for (std::uint32_t i = 0; i < 0x200u; ++i)
        x86emu_write_byte(e, modLin + i, 0);

    x86emu_write_dword(e, modLin + 0x00u, infoLin);
    x86emu_write_dword(e, modLin + 0x30u, 0u);
    x86emu_write_dword(e, modLin + 0x70u, 8u);
    x86emu_write_dword(e, modLin + 0x94u, 0u);
    /* _start@40b98: cmp [edx+eax*4+0x78],0 with eax=8 → [mod+0x98] must be non-zero. */
    x86emu_write_dword(e, modLin + 0x98u, 1u);
    x86emu_write_dword(e, modLin + 0x08u, flatBase);

    x86emu_write_dword(e, counter, 0u);

    x86emu_write_dword(e, flatBase + 0x2A3BCu, 0u);
    x86emu_write_dword(e, flatBase + 0x2A3ACu, 0u);
    if (forCstart) {
        x86emu_write_word(e, flatBase + 0x1F814u, 0x0020u);
        loadFlatSeg(e, R_ES_INDEX, 0x0020u, flatBase, false);
        loadFlatSeg(e, R_DS_INDEX, SEL_FLAT_DATA, flatBase, false);
        x86emu_set_seg_register(e, e->x86.R_ES_SEL, 0x0020u);
        x86emu_set_seg_register(e, e->x86.R_DS_SEL, SEL_FLAT_DATA);
        e->x86.R_EAX = (e->x86.R_EAX & 0x00FFFFFFu) | 0x4A0000u;
        e->x86.R_FLG &= ~F_CF;
    }

    const std::uint32_t pkgChain = infoLin + 0x200u;
    x86emu_write_dword(e, infoLin + 0x42u, pkgChain);

    gdt[5] = {infoLin, 0x000FFFFFu, 0x92u, 0xCFu, true};
    flushGdt(e);

    e->x86.R_EDX = DOS4GW_MOD_OFF;
    e->x86.R_EAX = DOS4GW_MOD_OFF + 0x100u;
    if (!forCstart)
        e->x86.R_EBX = codeObjBase;
    e->x86.R_ECX = 0u;
    e->x86.R_EBP = 0u;
    e->x86.R_ESI = 0u;
    e->x86.R_EDI = 0u;

    loadFlatSeg(e, R_GS_INDEX, dos4gwKernelSel, infoLin, false);
    x86emu_set_seg_register(e, e->x86.R_GS_SEL, dos4gwKernelSel);
}

/* Watcom _cstart_ (DOS/4GW path): ES=0x24 → gdt[4] base flatBase; reads ES:[0x2c]/[0x5c]. */
inline void seedDos4gwPspStub(x86emu_t* e, std::uint32_t flatBase) noexcept {
    if (!e) return;
    x86emu_write_word(e, flatBase + 0x2Cu, SEL_FLAT_DATA);
    x86emu_write_word(e, flatBase + 0x5Cu, 0x7F00u);
    /* Watcom _cstart_: ES: mov ds,[0x40bbc] reads flat data selector. */
    x86emu_write_word(e, flatBase + 0x40BBCu, SEL_FLAT_DATA);
    x86emu_write_word(e, flatBase + 0x1F814u, SEL_FLAT_DATA);
    /* PSP command tail — _cstart_ scans es:[81h] for argv workspace. */
    const char* cmd = pmExecCmdLine[0] ? pmExecCmdLine : "PROGRAM.EXE";
    std::uint8_t cmdLen = 0;
    for (; cmd[cmdLen] && cmdLen < 126u; ++cmdLen) {}
    x86emu_write_byte(e, flatBase + 0x80u, cmdLen);
    for (std::uint8_t i = 0; i < cmdLen; ++i)
        x86emu_write_byte(e, flatBase + 0x81u + i, static_cast<unsigned>(cmd[i]));
    x86emu_write_byte(e, flatBase + 0x81u + cmdLen, 0x0Du);
    x86emu_write_byte(e, flatBase + 0x1F836u, 0u);
}

inline void leaveProtected16(x86emu_t* e) noexcept {
    if (!e) return;
    e->x86.mode &= ~(_MODE_CODE32 | _MODE_DATA32 | _MODE_STACK32);
    e->x86.crx[0] &= ~1u;
    inProtected = false;
    normalizeRealModeSegs(e);
}

inline std::uint16_t allocSelectors(std::uint16_t count) noexcept {
    const std::uint16_t sel = nextAllocSel;
    for (std::uint16_t i = 0; i < count; ++i) {
        const std::uint32_t idx = gdtIndex(static_cast<std::uint16_t>(sel + i * 8u));
        if (idx >= GDT_ENTRIES) return 0;
        gdt[idx] = {};
        gdt[idx].used = true;
        gdt[idx].access = 0x92u;
        gdt[idx].gran = 0xCFu;
        gdt[idx].limit = 0x000FFFFFu;
    }
    nextAllocSel = static_cast<std::uint16_t>(nextAllocSel + count * 8u);
    return sel;
}

inline int simulateRmInterrupt(x86emu_t* e) {
    if (!e || !rmIntDispatch) return 0;
    const std::uint32_t buf = linearAddr(e,
        static_cast<std::uint16_t>(e->x86.R_ES & 0xFFFFu), pmOffset(e, e->x86.R_EDI));
    const std::uint8_t intr = static_cast<std::uint8_t>(e->x86.R_EBX & 0xFFu);

    const std::uint32_t saveMode = e->x86.mode;
    const std::uint32_t saveCr0  = e->x86.crx[0];
    const std::uint32_t saveCs   = e->x86.R_CS;
    const std::uint32_t saveSs   = e->x86.R_SS;
    const std::uint32_t saveDs   = e->x86.R_DS;
    const std::uint32_t saveEs   = e->x86.R_ES;
    const std::uint32_t saveFs   = e->x86.R_FS;
    const std::uint32_t saveGs   = e->x86.R_GS;
    const std::uint32_t saveEip  = e->x86.R_EIP;
    const std::uint32_t saveEsp  = e->x86.R_ESP;
    const std::uint32_t saveEax  = e->x86.R_EAX;
    const std::uint32_t saveEbx  = e->x86.R_EBX;
    const std::uint32_t saveEcx  = e->x86.R_ECX;
    const std::uint32_t saveEdx  = e->x86.R_EDX;
    const std::uint32_t saveEsi  = e->x86.R_ESI;
    const std::uint32_t saveEdi  = e->x86.R_EDI;
    const std::uint32_t saveEbp  = e->x86.R_EBP;
    const std::uint32_t saveFlg  = e->x86.R_FLG;

    leaveProtected16(e);
    /* DPMI 0.9 real-mode call structure (ES:DI). */
    e->x86.R_EDI = x86emu_read_dword(e, buf + 0u);
    e->x86.R_ESI = x86emu_read_dword(e, buf + 4u);
    e->x86.R_EBP = x86emu_read_dword(e, buf + 8u);
    e->x86.R_EBX = x86emu_read_dword(e, buf + 16u);
    e->x86.R_EDX = x86emu_read_dword(e, buf + 20u);
    e->x86.R_ECX = x86emu_read_dword(e, buf + 24u);
    e->x86.R_EAX = x86emu_read_dword(e, buf + 28u);
    e->x86.R_FLG = (e->x86.R_FLG & 0xFFFF0000u)
                 | (x86emu_read_word(e, buf + 32u) & 0xFFFFu);
    const std::uint16_t es = static_cast<std::uint16_t>(x86emu_read_word(e, buf + 34u));
    const std::uint16_t ds = static_cast<std::uint16_t>(x86emu_read_word(e, buf + 36u));
    const std::uint16_t fs = static_cast<std::uint16_t>(x86emu_read_word(e, buf + 38u));
    const std::uint16_t gs = static_cast<std::uint16_t>(x86emu_read_word(e, buf + 40u));
    e->x86.R_ESP = x86emu_read_dword(e, buf + 42u);
    const std::uint16_t ss = static_cast<std::uint16_t>(x86emu_read_dword(e, buf + 46u) & 0xFFFFu);
    const std::uint16_t cs = static_cast<std::uint16_t>(x86emu_read_dword(e, buf + 50u) & 0xFFFFu);
    e->x86.R_EIP = x86emu_read_dword(e, buf + 54u) & 0xFFFFu;

    x86emu_set_seg_register(e, e->x86.R_SS_SEL, ss);
    x86emu_set_seg_register(e, e->x86.R_CS_SEL, cs);
    x86emu_set_seg_register(e, e->x86.R_DS_SEL, ds);
    x86emu_set_seg_register(e, e->x86.R_ES_SEL, es);
    x86emu_set_seg_register(e, e->x86.R_FS_SEL, fs);
    x86emu_set_seg_register(e, e->x86.R_GS_SEL, gs);

    (void)rmIntDispatch(e, intr);

    x86emu_write_dword(e, buf + 0u, e->x86.R_EDI);
    x86emu_write_dword(e, buf + 4u, e->x86.R_ESI);
    x86emu_write_dword(e, buf + 8u, e->x86.R_EBP);
    x86emu_write_dword(e, buf + 12u, 0u);
    x86emu_write_dword(e, buf + 16u, e->x86.R_EBX);
    x86emu_write_dword(e, buf + 20u, e->x86.R_EDX);
    x86emu_write_dword(e, buf + 24u, e->x86.R_ECX);
    x86emu_write_dword(e, buf + 28u, e->x86.R_EAX);
    x86emu_write_word(e, buf + 32u, static_cast<unsigned>(e->x86.R_FLG & 0xFFFFu));
    x86emu_write_word(e, buf + 34u, static_cast<unsigned>(e->x86.R_ES & 0xFFFFu));
    x86emu_write_word(e, buf + 36u, static_cast<unsigned>(e->x86.R_DS & 0xFFFFu));
    x86emu_write_word(e, buf + 38u, static_cast<unsigned>(e->x86.R_FS & 0xFFFFu));
    x86emu_write_word(e, buf + 40u, static_cast<unsigned>(e->x86.R_GS & 0xFFFFu));
    x86emu_write_dword(e, buf + 42u, e->x86.R_ESP);
    x86emu_write_dword(e, buf + 46u, e->x86.R_SS & 0xFFFFu);
    x86emu_write_dword(e, buf + 50u, e->x86.R_CS & 0xFFFFu);
    x86emu_write_dword(e, buf + 54u, e->x86.R_EIP & 0xFFFFu);

    e->x86.mode = saveMode;
    e->x86.crx[0] = saveCr0;
    if (isProtected(e)) {
        flushGdt(e);
        x86emu_set_seg_register(e, e->x86.R_CS_SEL, static_cast<std::uint16_t>(saveCs & 0xFFFFu));
        x86emu_set_seg_register(e, e->x86.R_SS_SEL, static_cast<std::uint16_t>(saveSs & 0xFFFFu));
        x86emu_set_seg_register(e, e->x86.R_DS_SEL, static_cast<std::uint16_t>(saveDs & 0xFFFFu));
        x86emu_set_seg_register(e, e->x86.R_ES_SEL, static_cast<std::uint16_t>(saveEs & 0xFFFFu));
        x86emu_set_seg_register(e, e->x86.R_FS_SEL, static_cast<std::uint16_t>(saveFs & 0xFFFFu));
        x86emu_set_seg_register(e, e->x86.R_GS_SEL, static_cast<std::uint16_t>(saveGs & 0xFFFFu));
        e->x86.R_EIP = saveEip;
        e->x86.R_ESP = saveEsp;
    } else {
        e->x86.R_CS = saveCs;
        e->x86.R_SS = saveSs;
        e->x86.R_DS = saveDs;
        e->x86.R_ES = saveEs;
        e->x86.R_FS = saveFs;
        e->x86.R_GS = saveGs;
        e->x86.R_EIP = saveEip;
        e->x86.R_ESP = saveEsp;
    }
    /* DPMI 0300: post-INT register state lives in the call buffer; PM AX=0 on success. */
    e->x86.R_EAX = saveEax & 0xFFFF0000u;
    e->x86.R_EBX = saveEbx;
    e->x86.R_ECX = saveEcx;
    e->x86.R_EDX = saveEdx;
    e->x86.R_ESI = saveEsi;
    e->x86.R_EDI = saveEdi;
    e->x86.R_EBP = saveEbp;
    e->x86.R_FLG = saveFlg;
    e->x86.R_FLG &= ~F_CF;
    normalizeRealModeSegs(e);
    return 1;
}

inline int dispatch(x86emu_t* e, std::uint16_t ax) {
    if (!e) return 0;
    switch (ax) {
    case 0x0000u:
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | 0x0005u;
        e->x86.R_EDX = (e->x86.R_EDX & 0xFFFF0000u) | 0x0001u;
        return 1;
    case 0x0006u: {
        const std::uint16_t sel = static_cast<std::uint16_t>(e->x86.R_BX & 0xFFFFu);
        const std::uint32_t base = selBase(sel);
        e->x86.R_CX = (e->x86.R_CX & 0xFFFF0000u) | ((base >> 16) & 0xFFFFu);
        e->x86.R_EDX = (e->x86.R_EDX & 0xFFFF0000u) | (base & 0xFFFFu);
        e->x86.R_EAX &= 0xFFFF0000u;
        return 1;
    }
    case 0x0001u: {
        const std::uint8_t intr = static_cast<std::uint8_t>(e->x86.R_BL & 0xFFu);
        const std::uint32_t info = linearAddr(e,
            static_cast<std::uint16_t>(e->x86.R_ES & 0xFFFFu), pmOffset(e, e->x86.R_EDI));
        x86emu_write_dword(e, info + 0u, 0x00008E00u);
        x86emu_write_dword(e, info + 4u, static_cast<std::uint32_t>(intr) * 8u + 0x40u);
        e->x86.R_EAX &= 0xFFFF0000u;
        return 1;
    }
    case 0x0100u: {
        const std::uint16_t count = static_cast<std::uint16_t>(e->x86.R_ECX & 0xFFFFu);
        const std::uint16_t sel = allocSelectors(count ? count : 1u);
        if (!sel) {
            e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | 0x8011u;
            return 1;
        }
        flushGdt(e);
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | sel;
        e->x86.R_ECX = (e->x86.R_ECX & 0xFFFF0000u) | count;
        return 1;
    }
    case 0x0101u: {
        const std::uint16_t sel = static_cast<std::uint16_t>(e->x86.R_BX & 0xFFFFu);
        if (DescEntry* d = descFor(sel)) {
            d->access = static_cast<std::uint8_t>(e->x86.R_ECX & 0xFFu);
            flushGdt(e);
        }
        e->x86.R_EAX &= 0xFFFF0000u;
        return 1;
    }
    case 0x0102u: {
        const std::uint16_t sel = static_cast<std::uint16_t>(e->x86.R_BX & 0xFFFFu);
        if (DescEntry* d = descFor(sel)) {
            d->access = static_cast<std::uint8_t>(e->x86.R_ECX & 0xFFu);
            flushGdt(e);
        }
        e->x86.R_EAX &= 0xFFFF0000u;
        return 1;
    }
    case 0x0103u: {
        const std::uint16_t sel = static_cast<std::uint16_t>(e->x86.R_BX & 0xFFFFu);
        const DescEntry* d = descFor(sel);
        const std::uint32_t base = d ? d->base : 0u;
        e->x86.R_ECX = (e->x86.R_ECX & 0xFFFF0000u) | ((base >> 16) & 0xFFFFu);
        e->x86.R_EDX = (e->x86.R_EDX & 0xFFFF0000u) | (base & 0xFFFFu);
        e->x86.R_EAX &= 0xFFFF0000u;
        return 1;
    }
    case 0x0104u: {
        const std::uint16_t sel = static_cast<std::uint16_t>(e->x86.R_BX & 0xFFFFu);
        if (DescEntry* d = descFor(sel)) {
            d->base = ((e->x86.R_ECX & 0xFFFFu) << 16) | (e->x86.R_EDX & 0xFFFFu);
            flushGdt(e);
        }
        e->x86.R_EAX &= 0xFFFF0000u;
        return 1;
    }
    case 0x0105u: {
        const std::uint16_t sel = static_cast<std::uint16_t>(e->x86.R_BX & 0xFFFFu);
        if (DescEntry* d = descFor(sel)) {
            d->limit = ((e->x86.R_ECX & 0xFFFFu) << 16) | (e->x86.R_EDX & 0xFFFFu);
            flushGdt(e);
        }
        e->x86.R_EAX &= 0xFFFF0000u;
        return 1;
    }
    case 0x0106u: {
        const std::uint16_t sel = static_cast<std::uint16_t>(e->x86.R_BX & 0xFFFFu);
        if (DescEntry* d = descFor(sel)) {
            d->access = static_cast<std::uint8_t>(e->x86.R_ECX & 0xFFu);
            if (e->x86.R_ECX & 0x8000u) d->gran = 0xCFu;
            flushGdt(e);
        }
        e->x86.R_EAX &= 0xFFFF0000u;
        return 1;
    }
    case 0x0200u: {
        const std::uint32_t paras = static_cast<std::uint32_t>(e->x86.R_BX & 0xFFFFu);
        const std::uint32_t bytes = paras * 16u;
        const std::uint32_t base  = DOS_MEM_BASE + dosMemUsed;
        dosMemUsed += bytes;
        const std::uint16_t rmSeg = static_cast<std::uint16_t>(base >> 4);
        if (DescEntry* d = descFor(SEL_DOS_RW)) {
            d->base = base;
            d->limit = std::max<std::uint32_t>(bytes - 1u, 0xFFFFu);
            d->access = 0x92u;
            d->gran = (bytes > 0x100000u) ? 0xCFu : 0xC0u;
            flushGdt(e);
        }
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | SEL_DOS_RW;
        e->x86.R_EDX = (e->x86.R_EDX & 0xFFFF0000u) | rmSeg;
        e->x86.R_EAX &= 0xFFFF0000u;
        return 1;
    }
    case 0x0201u:
    case 0x0202u:
        e->x86.R_EAX &= 0xFFFF0000u;
        return 1;
    case 0x0300u:
        return simulateRmInterrupt(e);
    case 0x0301u: {
        const std::uint16_t sel = allocSelectors(1u);
        if (!sel) {
            e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | 0x8011u;
            return 1;
        }
        flushGdt(e);
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | sel;
        e->x86.R_ECX = (e->x86.R_ECX & 0xFFFF0000u) | RM_CALL_BASE;
        e->x86.R_EDX = (e->x86.R_EDX & 0xFFFF0000u) | 0x0000u;
        return 1;
    }
    case 0x0303u: {
        const std::uint16_t sel = static_cast<std::uint16_t>(e->x86.R_ECX & 0xFFFFu);
        if (DescEntry* d = descFor(sel)) {
            d->used = false;
            flushGdt(e);
        }
        e->x86.R_EAX &= 0xFFFF0000u;
        return 1;
    }
    case 0x0400u:
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | 0x0001u;
        return 1;
    case 0x0500u: {
        const std::uint32_t info = linearAddr(e,
            static_cast<std::uint16_t>(e->x86.R_ES & 0xFFFFu), pmOffset(e, e->x86.R_EDI));
        const std::uint32_t freeExt = 32u * 1024u * 1024u;
        x86emu_write_dword(e, info + 0u, 0u);
        x86emu_write_dword(e, info + 4u, freeExt);
        x86emu_write_dword(e, info + 8u, freeExt);
        x86emu_write_dword(e, info + 12u, 0u);
        x86emu_write_dword(e, info + 16u, 0u);
        e->x86.R_EAX &= 0xFFFF0000u;
        return 1;
    }
    case 0x0501u: {
        const std::uint32_t bytes = (e->x86.R_EBX & 0xFFFFu) * 16u + (e->x86.R_ECX & 0xFFFFu);
        const std::uint32_t base  = EXT_MEM_BASE + extMemUsed;
        extMemUsed += (bytes + 15u) & ~15u;
        e->x86.R_EBX = (e->x86.R_EBX & 0xFFFF0000u) | ((base >> 16) & 0xFFFFu);
        e->x86.R_ECX = (e->x86.R_ECX & 0xFFFF0000u) | (base & 0xFFFFu);
        e->x86.R_EDX = (e->x86.R_EDX & 0xFFFF0000u) | 0x0000u;
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | 0x0000u;
        e->x86.R_ESI = (e->x86.R_ESI & 0xFFFF0000u) | ((base >> 16) & 0xFFFFu);
        e->x86.R_EDI = (e->x86.R_EDI & 0xFFFF0000u) | (base & 0xFFFFu);
        return 1;
    }
    case 0x0502u:
    case 0x0503u:
    case 0x0600u:
    case 0x0601u:
        e->x86.R_EAX &= 0xFFFF0000u;
        return 1;
    case 0x0B00u: {
        const std::uint16_t codeSel  = static_cast<std::uint16_t>(e->x86.R_ES & 0xFFFFu);
        const std::uint32_t eip      = e->x86.R_EDI;
        const std::uint16_t stackSel = static_cast<std::uint16_t>(e->x86.R_SS & 0xFFFFu);
        const std::uint32_t esp      = e->x86.R_ESP;
        const std::uint16_t dataSel  = static_cast<std::uint16_t>(e->x86.R_DS & 0xFFFFu);
        enterProtected32(e, codeSel, eip, stackSel, esp, dataSel);
        e->x86.R_EAX &= 0xFFFF0000u;
        return 1;
    }
    case 0x0B01u:
        leaveProtected16(e);
        e->x86.R_EAX &= 0xFFFF0000u;
        return 1;
    case 0x0B02u:
        e->x86.R_ECX = (e->x86.R_ECX & 0xFFFF0000u) | 0x0000u;
        e->x86.R_EDX = (e->x86.R_EDX & 0xFFFF0000u) | 0x0000u;
        e->x86.R_EAX &= 0xFFFF0000u;
        return 1;
    default:
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | 0x8000u;
        return 1;
    }
}

inline void install(x86emu_t* e) noexcept {
    if (!e) return;

    gdt[0] = {};
    gdt[1] = {0, 0xFFFFF, 0x9Au, 0xCFu, true};
    gdt[2] = {0, 0xFFFFF, 0x92u, 0xCFu, true};
    gdt[3] = {0, 0xFFFFF, 0x92u, 0xCFu, true};
    gdt[3] = {DOS_MEM_BASE, 0xFFFFF, 0x92u, 0xCFu, true};
    flushGdt(e);

    const std::uint32_t codeBase = static_cast<std::uint32_t>(dpmiCodeSeg) << 4;
    const std::uint32_t dataBase = static_cast<std::uint32_t>(dpmiDataSeg) << 4;
    const std::uint32_t xmsBase  = static_cast<std::uint32_t>(xmsCodeSeg) << 4;

    /* Real-mode DPMI entry: NOP sled — codeThunk at F000:0 performs mode switch. */
    for (std::uint32_t i = 0; i < 8u; ++i)
        x86emu_write_byte(e, codeBase + i, 0x90u);
    x86emu_write_byte(e, codeBase + 8u, 0xCBu); /* RETF */

    const std::uint8_t xmsStub[] = {
        0x80, 0xFC, 0x05, 0x75, 0x04, 0xB8, 0x01, 0x00, 0xC3, 0xB4, 0x00, 0xC3,
    };
    for (std::size_t i = 0; i < sizeof xmsStub; ++i)
        x86emu_write_byte(e, xmsBase + static_cast<std::uint32_t>(i), xmsStub[i]);

    x86emu_write_word(e, dataBase + 0u, 0x0000u);
    x86emu_write_word(e, dataBase + 2u, dpmiCodeSeg);

    /* DOS/4GW extender host-id probe (stub ~092898, ES=0). */
    x86emu_write_byte(e, 0x000Au, 0x45u);  /* 'E' */
    x86emu_write_word(e, 0x000Bu, 0x4D4Du); /* 'MM' */
    x86emu_write_byte(e, 0x000Du, 0x58u);  /* 'X' */
    x86emu_write_word(e, 0x000Eu, 0x5858u); /* 'XX' */
    x86emu_write_byte(e, 0x0011u, 0x30u);  /* '0' */
    x86emu_write_byte(e, 0x0047u, 0x80u);

    installed = true;
}

inline int handleInt2f(x86emu_t* e) {
    const std::uint16_t ax = static_cast<std::uint16_t>(e->x86.R_EAX & 0xFFFFu);
    if (ax == 0x1687u || ax == 0x1684u) {
        install(e);
        const std::uint32_t dataBase = static_cast<std::uint32_t>(dpmiDataSeg) << 4;
        writeCstr(e, dataBase + 4u, "RWDOS");
        /* DOS/4GW: OR AX,AX / JNZ after INT 2F 1687 — AX must stay non-zero. */
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | 0x1687u;
        e->x86.R_EBX = (e->x86.R_EBX & 0xFFFF0000u) | 0x0000u; /* PM entry offset */
        e->x86.R_ECX = (e->x86.R_ECX & 0xFFFF0000u) | dpmiCodeSeg; /* PM entry seg F000h */
        e->x86.R_EDX = (e->x86.R_EDX & 0xFFFF0000u) | 0x0005u; /* DPMI 0.9 */
        e->x86.R_ESI = (e->x86.R_ESI & 0xFFFF0000u) | 0x0000u;
        e->x86.R_ES  = (e->x86.R_ES & 0xFFFF0000u) | dpmiDataSeg;
        e->x86.R_EDI = (e->x86.R_EDI & 0xFFFF0000u) | 0x0004u;
        return 1;
    }
    if (ax == 0x4A33u) {
        e->x86.R_AL = 0x00u;
        e->x86.R_BX = (e->x86.R_BX & 0xFFFF0000u) | 0x0005u;
        return 1;
    }
    return 0;
}

inline int handleInt31(x86emu_t* e) {
    install(e);
    return dispatch(e, static_cast<std::uint16_t>(e->x86.R_EAX & 0xFFFFu));
}

constexpr std::uint32_t DOS4GW_FLAT_BASE = 0x00400000u;
inline std::uint32_t dos4gwMzHdrBytes = 0u;
inline std::uint32_t dos4gwLeOff = 0u;

constexpr std::uint32_t LE_OFF_START_OBJ    = 0x18u;
constexpr std::uint32_t LE_OFF_EIP          = 0x1Cu;
constexpr std::uint32_t LE_OFF_STACK_OBJ    = 0x20u;
constexpr std::uint32_t LE_OFF_ESP          = 0x24u;
constexpr std::uint32_t LE_OFF_PAGE_SIZE    = 0x28u;
constexpr std::uint32_t LE_OFF_LAST_PAGE    = 0x2Cu;
constexpr std::uint32_t LE_OFF_NUM_PAGES    = 0x14u;
constexpr std::uint32_t LE_OFF_FIXUP_SIZE     = 0x30u;
constexpr std::uint32_t LE_OFF_OBJMAP         = 0x48u;
constexpr std::uint32_t LE_OFF_OBJTAB         = 0x40u;
constexpr std::uint32_t LE_OFF_NUM_OBJECTS    = 0x44u;
constexpr std::uint32_t LE_OFF_PAGE_DATA      = 0x80u;
constexpr std::uint32_t LE_OFF_FIXPAGE        = 0x68u;
constexpr std::uint32_t LE_OFF_FIXREC         = 0x6Cu;
constexpr std::uint32_t LE_OBJ_REC_SIZE       = 24u;
constexpr std::uint32_t LE_OBJ_FILL_MASK      = 0x0300u;
constexpr std::uint32_t LE_OBJ_ZERO_FILL      = 0x0100u;
constexpr std::uint16_t LE_SEL_FLAT_CODE      = 0x0008u;
constexpr std::uint16_t LE_SEL_FLAT_DATA      = 0x0010u;

constexpr std::uint8_t LE_SRC_MASK            = 0x0Fu;
constexpr std::uint8_t LE_SFLAG_LIST          = 0x20u;
constexpr std::uint8_t LE_SRC_BYTE            = 0x00u;
constexpr std::uint8_t LE_SRC_SEG             = 0x02u;
constexpr std::uint8_t LE_SRC_PTR32           = 0x03u;
constexpr std::uint8_t LE_SRC_OFF16           = 0x05u;
constexpr std::uint8_t LE_SRC_PTR48           = 0x06u;
constexpr std::uint8_t LE_SRC_OFF32           = 0x07u;
constexpr std::uint8_t LE_SRC_OFF32_REL       = 0x08u;
constexpr std::uint8_t LE_TGT_INTERNAL        = 0x00u;
constexpr std::uint8_t LE_TFLAG_ADDITIVE      = 0x04u;
constexpr std::uint8_t LE_TFLAG_OFF32         = 0x10u;
constexpr std::uint8_t LE_TFLAG_OBJ16          = 0x40u;

inline std::uint16_t leObjSelector(std::uint32_t objIdx) noexcept {
    return static_cast<std::uint16_t>(LE_SEL_FLAT_DATA + objIdx * 8u);
}

inline void lePatchFixup(x86emu_t* e, std::uint32_t fixLin, std::uint32_t fixVirt,
                         std::uint8_t srcType, std::uint32_t value, std::uint32_t selOrZero,
                         std::uint32_t additive, bool additiveOp) noexcept {
    switch (srcType) {
    case LE_SRC_OFF32: {
        const std::uint32_t cur = x86emu_read_dword(e, fixLin);
        const std::uint32_t out = additiveOp ? cur + value + additive : value + additive;
        x86emu_write_dword(e, fixLin, out);
        break;
    }
    case LE_SRC_OFF32_REL: {
        const std::uint32_t rel = value - (fixVirt + 4u) + additive;
        x86emu_write_dword(e, fixLin, rel);
        break;
    }
    case LE_SRC_OFF16: {
        const std::uint32_t cur = x86emu_read_word(e, fixLin);
        const std::uint32_t out = additiveOp ? cur + value + additive : value + additive;
        x86emu_write_word(e, fixLin, static_cast<unsigned>(out & 0xFFFFu));
        break;
    }
    case LE_SRC_SEG:
        x86emu_write_word(e, fixLin, static_cast<unsigned>(selOrZero & 0xFFFFu));
        break;
    case LE_SRC_PTR48: {
        const std::uint32_t cur = x86emu_read_dword(e, fixLin);
        const std::uint32_t out = additiveOp ? cur + value + additive : value + additive;
        x86emu_write_dword(e, fixLin, out);
        x86emu_write_word(e, fixLin + 4u, static_cast<unsigned>(selOrZero & 0xFFFFu));
        break;
    }
    case LE_SRC_PTR32: {
        const std::uint32_t cur = x86emu_read_dword(e, fixLin);
        const std::uint32_t out = additiveOp ? cur + value + additive : value + additive;
        x86emu_write_dword(e, fixLin, out);
        break;
    }
    case LE_SRC_BYTE: {
        const std::uint32_t cur = x86emu_read_byte(e, fixLin);
        const std::uint32_t out = additiveOp ? cur + value + additive : value + additive;
        x86emu_write_byte(e, fixLin, static_cast<unsigned>(out & 0xFFu));
        break;
    }
    default:
        break;
    }
}

inline std::uint32_t leMapFilePage(std::uint32_t mapEnt) noexcept {
    return ((mapEnt & 0xFFu) << 16) | (mapEnt & 0xFF00u) | ((mapEnt >> 16) & 0xFFu);
}

inline std::uint32_t leGuestFileAddr(std::uint32_t mzLinear, std::uint32_t mzHdrBytes,
                                     std::uint32_t leFileBase, std::uint32_t rel) noexcept {
    return mzLinear + (leFileBase + rel - mzHdrBytes);
}

/* MZ-embedded LE: pageData offsets are relative to the MZ exe base, not the LE signature. */
inline std::uint32_t leGuestPageAddr(std::uint32_t mzLinear, std::uint32_t mzHdrBytes,
                                     std::uint32_t leFileBase, std::uint32_t rel) noexcept {
    return leGuestFileAddr(mzLinear, mzHdrBytes, leFileBase, rel) - mzHdrBytes;
}

inline void leZeroRange(x86emu_t* e, std::uint32_t dst, std::uint32_t bytes) noexcept {
    for (std::uint32_t i = 0; i < bytes; ++i)
        x86emu_write_byte(e, dst + i, 0);
}

inline void leCopyRange(x86emu_t* e, std::uint32_t src, std::uint32_t dst, std::uint32_t bytes) noexcept {
    for (std::uint32_t i = 0; i < bytes; ++i)
        x86emu_write_byte(e, dst + i, x86emu_read_byte(e, src + i));
}

inline std::uint32_t leObjBase(x86emu_t* e, std::uint32_t objTab, std::uint32_t objIdx) noexcept {
    return x86emu_read_dword(e, objTab + objIdx * LE_OBJ_REC_SIZE + 4u);
}

inline std::uint32_t leTargetVirtual(x86emu_t* e, std::uint32_t objTab,
                                     std::uint32_t objOneBased, std::uint32_t tgtOff) noexcept {
    if (!objOneBased) return tgtOff;
    return leObjBase(e, objTab, objOneBased - 1u) + tgtOff;
}

inline bool leApplyFixups(x86emu_t* e, std::uint32_t le, std::uint32_t mzLinear,
                          std::uint32_t mzHdrBytes, std::uint32_t leFileBase,
                          std::uint32_t flatBase, std::uint32_t objTab,
                          std::uint32_t numObj, std::uint32_t numPages,
                          std::uint32_t pageSize) noexcept {
    const std::uint32_t fixpageOff = x86emu_read_dword(e, le + LE_OFF_FIXPAGE);
    const std::uint32_t fixrecOff  = x86emu_read_dword(e, le + LE_OFF_FIXREC);
    const std::uint32_t fixupSize  = x86emu_read_dword(e, le + LE_OFF_FIXUP_SIZE);
    if (!fixupSize) return true;

    const std::uint32_t fixMem = leGuestFileAddr(mzLinear, mzHdrBytes, leFileBase, fixrecOff);
    const std::uint32_t fixPageTab = le + fixpageOff;
    std::uint32_t applied = 0u;
    std::uint32_t skipped = 0u;

    for (std::uint32_t o = 0; o < numObj; ++o) {
        const std::uint32_t rec = objTab + o * LE_OBJ_REC_SIZE;
        const std::uint32_t objPages = x86emu_read_dword(e, rec + 16u);
        if (!objPages) continue;

        const std::uint32_t objBaseRel = x86emu_read_dword(e, rec + 4u);
        std::uint32_t mapIdx = x86emu_read_dword(e, rec + 12u);
        if (mapIdx) --mapIdx;

        for (std::uint32_t pg = 0; pg < objPages; ++pg, ++mapIdx) {
            const std::uint32_t fixStart = x86emu_read_dword(e, fixPageTab + mapIdx * 4u);
            const std::uint32_t fixEnd   = x86emu_read_dword(e, fixPageTab + (mapIdx + 1u) * 4u);
            if (fixEnd <= fixStart) continue;

            std::uint32_t fixPos = fixMem + fixStart;
            const std::uint32_t fixLim = fixMem + fixEnd;
            while (fixPos < fixLim) {
                const std::uint32_t recStart = fixPos;
                const std::uint8_t srcType = x86emu_read_byte(e, fixPos++);
                const std::uint8_t flags   = x86emu_read_byte(e, fixPos++);
                const bool isList = (srcType & LE_SFLAG_LIST) != 0u;
                const std::uint8_t srcKind = srcType & LE_SRC_MASK;

                std::int32_t srcOff = 0;
                std::uint16_t listCount = 0;
                if (isList) {
                    listCount = x86emu_read_byte(e, fixPos++);
                } else {
                    srcOff = static_cast<std::int16_t>(x86emu_read_word(e, fixPos));
                    fixPos += 2u;
                }

                std::uint32_t objNum = 0u;
                if (flags & LE_TFLAG_OBJ16) {
                    objNum = x86emu_read_word(e, fixPos);
                    fixPos += 2u;
                } else {
                    objNum = x86emu_read_byte(e, fixPos++);
                }

                const std::uint8_t tgtKind = flags & 0x03u;
                std::uint32_t tgtOff = 0u;
                std::uint32_t additive = 0u;

                if (tgtKind == LE_TGT_INTERNAL) {
                    if (srcKind != LE_SRC_SEG) {
                        if (flags & LE_TFLAG_OFF32) {
                            tgtOff = x86emu_read_dword(e, fixPos);
                            fixPos += 4u;
                        } else {
                            tgtOff = x86emu_read_word(e, fixPos);
                            fixPos += 2u;
                        }
                    }
                } else if (tgtKind == 3u) {
                    if (flags & LE_TFLAG_ADDITIVE) {
                        additive = x86emu_read_dword(e, fixPos);
                        fixPos += 4u;
                    }
                    tgtOff = objNum;
                    objNum = 1u;
                } else {
                    if (flags & LE_TFLAG_OBJ16) {
                        fixPos += 2u;
                    } else {
                        fixPos += 1u;
                    }
                    if (flags & 0x80u) {
                        fixPos += 1u;
                    } else if (flags & LE_TFLAG_OFF32) {
                        fixPos += 4u;
                    } else {
                        fixPos += 2u;
                    }
                    if (flags & LE_TFLAG_ADDITIVE) {
                        fixPos += 4u;
                    }
                    skipped++;
                    if (isList && listCount)
                        fixPos += static_cast<std::uint32_t>(listCount) * 2u;
                    continue;
                }

                if (flags & LE_TFLAG_ADDITIVE) {
                    additive = x86emu_read_dword(e, fixPos);
                    fixPos += 4u;
                }

                std::uint16_t listOffsets[64]{};
                std::uint16_t listN = 0;
                if (isList) {
                    const std::uint32_t tail = fixPos + static_cast<std::uint32_t>(listCount) * 2u;
                    if (tail > fixLim) return false;
                    for (std::uint16_t i = 0; i < listCount && i < 64u; ++i)
                        listOffsets[i] = x86emu_read_word(e, fixPos + static_cast<std::uint32_t>(i) * 2u);
                    listN = listCount;
                    fixPos = tail;
                }

                std::int32_t fixOff = srcOff;
                if (!isList && fixOff < 0)
                    fixOff = static_cast<std::int32_t>(pageSize) + fixOff;

                const std::uint32_t tgtVirt = leTargetVirtual(e, objTab, objNum, tgtOff);
                const bool additiveOp = (flags & LE_TFLAG_ADDITIVE) != 0u
                    || (tgtKind == LE_TGT_INTERNAL && srcKind != LE_SRC_SEG
                        && srcKind != LE_SRC_OFF32_REL);
                const std::uint32_t value = additiveOp && objNum
                    ? leObjBase(e, objTab, objNum - 1u)
                    : tgtVirt;
                const std::uint32_t sel = objNum ? leObjSelector(objNum - 1u) : 0u;

                auto applyAt = [&](std::int32_t off) {
                    if (off < 0) return;
                    const std::uint32_t fixVirt = objBaseRel + pg * pageSize
                                                + static_cast<std::uint32_t>(off);
                    const std::uint32_t fixLin  = flatBase + fixVirt;
                    const std::uint32_t patchVal = srcKind == LE_SRC_OFF32_REL ? tgtVirt : value;
                    lePatchFixup(e, fixLin, fixVirt, srcKind, patchVal, sel, additive, additiveOp);
                    ++applied;
                };

                if (isList) {
                    for (std::uint16_t i = 0; i < listN; ++i)
                        applyAt(static_cast<std::int32_t>(listOffsets[i]));
                } else {
                    applyAt(fixOff);
                }

                if (fixPos <= recStart) return false;
            }
        }
    }
    (void)numObj;
    (void)numPages;
    (void)pageSize;
    std::fprintf(stderr, "[DPMI] LE fixups applied=%u skipped=%u\n", applied, skipped);
    return true;
}

/* DOS/4GW: load LE image to flat base (OpenWatcom os2_flat_header) and enter PM32. */
inline bool tryBootstrapDos4gw(x86emu_t* e, std::uint32_t mzLinear, std::uint32_t imageBytes) noexcept {
    if (!e || imageBytes < 0x400u) return false;
    install(e);

    const std::uint32_t mzHdrBytes = dos4gwMzHdrBytes ? dos4gwMzHdrBytes : 0xA40u;
    std::uint32_t leOff = dos4gwLeOff;
    const std::uint32_t eLfanew = x86emu_read_dword(e, mzLinear + 0x3Cu);
    if (!leOff) {
        if (eLfanew >= mzHdrBytes && eLfanew + 0x84u < imageBytes
            && x86emu_read_byte(e, mzLinear + eLfanew) == 'L'
            && x86emu_read_byte(e, mzLinear + eLfanew + 1u) == 'E'
            && x86emu_read_byte(e, mzLinear + eLfanew + 2u) == 0u
            && x86emu_read_byte(e, mzLinear + eLfanew + 3u) == 0u)
            leOff = eLfanew;
    }
    if (!leOff) return false;

    const std::uint32_t le         = mzLinear + leOff;
    const std::uint32_t leFileBase = mzHdrBytes + leOff;
    const std::uint32_t flatBase   = DOS4GW_FLAT_BASE;
    leZeroRange(e, flatBase, 0x00100000u);
    const std::uint32_t entryEip   = x86emu_read_dword(e, le + LE_OFF_EIP);
    const std::uint32_t entryEsp   = x86emu_read_dword(e, le + LE_OFF_ESP);
    const std::uint32_t stackObj   = x86emu_read_dword(e, le + LE_OFF_STACK_OBJ);
    const std::uint32_t pageSize   = x86emu_read_dword(e, le + LE_OFF_PAGE_SIZE);
    const std::uint32_t lastPage   = x86emu_read_dword(e, le + LE_OFF_LAST_PAGE);
    const std::uint32_t numPages   = x86emu_read_dword(e, le + LE_OFF_NUM_PAGES);
    const std::uint32_t pageData   = x86emu_read_dword(e, le + LE_OFF_PAGE_DATA);
    const std::uint32_t objMap     = le + x86emu_read_dword(e, le + LE_OFF_OBJMAP);
    const std::uint32_t objTab     = le + x86emu_read_dword(e, le + LE_OFF_OBJTAB);
    const std::uint32_t numObj     = x86emu_read_dword(e, le + LE_OFF_NUM_OBJECTS);
    const std::uint32_t psz        = pageSize ? pageSize : 4096u;

    {
        const std::uint32_t mapEnt48 = x86emu_read_dword(e, objMap + 48u * 4u);
        const std::uint32_t filePg48 = leMapFilePage(mapEnt48);
        const std::uint32_t srcProbe = mzLinear + leOff + pageData + (filePg48 - 1u) * psz + 0xb48u;
        std::fprintf(stderr, "[DPMI] LE hdr @%08x objs=%u pg48=%u src@%08x=%02x%02x%02x\n",
            leOff,
            static_cast<unsigned>(numObj),
            filePg48,
            srcProbe,
            static_cast<unsigned>(x86emu_read_byte(e, srcProbe)),
            static_cast<unsigned>(x86emu_read_byte(e, srcProbe + 1u)),
            static_cast<unsigned>(x86emu_read_byte(e, srcProbe + 2u)));
    }

    for (std::uint32_t o = 0; o < numObj; ++o) {
        const std::uint32_t rec   = objTab + o * LE_OBJ_REC_SIZE;
        const std::uint32_t size  = x86emu_read_dword(e, rec + 0u);
        const std::uint32_t addr  = x86emu_read_dword(e, rec + 4u);
        const std::uint32_t flags = x86emu_read_dword(e, rec + 8u);
        std::uint32_t mapIdx = x86emu_read_dword(e, rec + 12u);
        if (mapIdx) --mapIdx;
        const std::uint32_t mapCnt = x86emu_read_dword(e, rec + 16u);
        const std::uint32_t dst    = flatBase + addr;

        if ((flags & LE_OBJ_FILL_MASK) == LE_OBJ_ZERO_FILL) {
            leZeroRange(e, dst, size);
            continue;
        }
        if (!mapCnt) continue;

        std::uint32_t loaded = 0u;
        for (std::uint32_t pg = 0; pg < mapCnt; ++pg, ++mapIdx) {
            std::uint32_t pageBytes = psz;
            if (mapIdx + 1u == numPages && lastPage)
                pageBytes = lastPage;
            if (loaded + pageBytes > size)
                pageBytes = size - loaded;

            const std::uint32_t mapEnt = x86emu_read_dword(e, objMap + mapIdx * 4u);
            const std::uint32_t filePage = leMapFilePage(mapEnt);
            if (!filePage) {
                leZeroRange(e, dst + loaded, pageBytes);
            } else {
                const std::uint32_t src = leGuestFileAddr(mzLinear, mzHdrBytes, leFileBase,
                    pageData + (filePage - 1u) * psz);
                leCopyRange(e, src, dst + loaded, pageBytes);
            }
            loaded += pageBytes;
            if (loaded >= size) break;
        }
        if (loaded < size)
            leZeroRange(e, dst + loaded, size - loaded);
    }

    /* Watcom absolute VAs (0x10000..0x90000): sparse LE pages + MZ runtime; skip DPMI @0x80000. */
    constexpr std::uint32_t kMzMirrorLo = 0x00010000u;
    constexpr std::uint32_t kMzMirrorHi = 0x00090000u;
    constexpr std::uint32_t kDpmiLo     = 0x00080000u;
    constexpr std::uint32_t kDpmiHi     = RM_CALL_BASE + 0x1000u; /* 0x82000 */
    if (kMzMirrorHi > kMzMirrorLo) {
        if (kMzMirrorLo < kDpmiLo) {
            const std::uint32_t n = std::min(kDpmiLo, kMzMirrorHi) - kMzMirrorLo;
            leCopyRange(e, mzLinear + kMzMirrorLo - mzHdrBytes, flatBase + kMzMirrorLo, n);
        }
        if (kMzMirrorHi > kDpmiHi) {
            const std::uint32_t n = kMzMirrorHi - std::max(kMzMirrorLo, kDpmiHi);
            leCopyRange(e, mzLinear + std::max(kMzMirrorLo, kDpmiHi) - mzHdrBytes,
                flatBase + std::max(kMzMirrorLo, kDpmiHi), n);
        }
        std::fprintf(stderr, "[DPMI] MZ absolute mirror %08x-%08x (skip %08x-%08x)\n",
            kMzMirrorLo, kMzMirrorHi, kDpmiLo, kDpmiHi);
    }

    /* Watcom LE _cstart_ — often outside sparse LE page map; scan if absent from entry page. */
    constexpr std::uint32_t kDoomCstartVa  = 0x82BD6u;
    constexpr std::uint32_t kDoomCstartOff = 0x82BD6u;
    const std::uint32_t cstartSrc = mzLinear + kDoomCstartOff - mzHdrBytes;
    leCopyRange(e, cstartSrc, flatBase + kDoomCstartVa, 0x800u);

    std::uint32_t cstartEip = kDoomCstartVa;
    if (x86emu_read_byte(e, flatBase + cstartEip) != 0xFBu
        || x86emu_read_byte(e, flatBase + cstartEip + 0x26u) != 0x21u) {
        cstartEip = findLeCstart(e, flatBase, 0x00100000u);
        if (cstartEip >= entryEip && cstartEip < entryEip + 0x100u)
            cstartEip = 0u;
        if (!cstartEip)
            cstartEip = kDoomCstartVa;
    }

    std::uint8_t cstartPrologue[0x80u]{};
    for (std::uint32_t i = 0; i < sizeof cstartPrologue; ++i)
        cstartPrologue[i] = static_cast<std::uint8_t>(
            x86emu_read_byte(e, flatBase + cstartEip + i));

    wwcRutLo = 0u;
    wwcRutHi = 0u;
    (void)findWwcRutRegion(e, flatBase, 0x00100000u, wwcRutLo, wwcRutHi);

    std::uint8_t wwcRutSnap[WWC_RUT_SCAN_BYTES]{};
    if (wwcRutLo) {
        for (std::uint32_t i = 0; i < sizeof wwcRutSnap; ++i)
            wwcRutSnap[i] = static_cast<std::uint8_t>(
                x86emu_read_byte(e, flatBase + wwcRutLo + i));
    }

    if (!leApplyFixups(e, le, mzLinear, mzHdrBytes, leFileBase, flatBase,
            objTab, numObj, numPages, psz))
        std::fprintf(stderr, "[DPMI] LE fixup warning — continuing\n");

    if (wwcRutLo)
        restoreWwcRut(e, flatBase, wwcRutLo, wwcRutSnap,
            static_cast<std::uint32_t>(sizeof wwcRutSnap));

    /* LE OFF32 fixups can clobber the Watcom _cstart_ opcode bytes — restore prologue. */
    if (x86emu_read_byte(e, flatBase + cstartEip) != 0xFBu
        || x86emu_read_byte(e, flatBase + cstartEip + 1u) != 0x83u) {
        for (std::uint32_t i = 0; i < sizeof cstartPrologue; ++i)
            x86emu_write_byte(e, flatBase + cstartEip + i,
                static_cast<unsigned>(cstartPrologue[i]));
        std::fprintf(stderr, "[DPMI] LE _cstart_%08x prologue restored after fixups\n", cstartEip);
    }
    /* Watcom stores 0x24 (RM seg / LDT-TI selector) — use obj1 sel 0x20 in PM. */
    for (std::uint32_t i = 0; i + 2u < 0x120u; ++i) {
        const std::uint8_t op = static_cast<std::uint8_t>(
            x86emu_read_byte(e, flatBase + cstartEip + i));
        if ((op == 0xB8u || op == 0xB9u || op == 0xBAu || op == 0xBBu)
            && x86emu_read_byte(e, flatBase + cstartEip + i + 1u) == 0x24u
            && x86emu_read_byte(e, flatBase + cstartEip + i + 2u) == 0x00u)
            x86emu_write_byte(e, flatBase + cstartEip + i + 1u, 0x20u);
    }
    /* NOP Watcom _cstart_ segment loads — host presets DS/ES (libx86emu GPs ES: overrides). */
    auto nopCstartPattern = [&](std::uint32_t off, std::uint32_t n) {
        if (off + n > 0x800u) return;
        for (std::uint32_t i = 0; i < n; ++i)
            x86emu_write_byte(e, flatBase + cstartEip + off + i, 0x90u);
    };
    if (x86emu_read_byte(e, flatBase + cstartEip + 0x108u) == 0x26u
        && x86emu_read_byte(e, flatBase + cstartEip + 0x109u) == 0x8Cu)
        nopCstartPattern(0x108u, 7u); /* ES: mov ds,[0x40bbc] */
    if (x86emu_read_byte(e, flatBase + cstartEip + 0x117u) == 0x8Eu
        && x86emu_read_byte(e, flatBase + cstartEip + 0x118u) == 0x05u)
        nopCstartPattern(0x117u, 6u); /* mov es,[0x1f814] */
    if (x86emu_read_byte(e, flatBase + cstartEip + 0x4Eu) == 0x8Eu
        && x86emu_read_byte(e, flatBase + cstartEip + 0x4Fu) == 0x05u)
        nopCstartPattern(0x4Eu, 6u);
    auto nopResizeInt21 = [&](std::uint32_t base) {
        for (std::uint32_t i = 0; i + 3u < 0x200u; ++i) {
            if (x86emu_read_byte(e, base + i) == 0xB4u
                && x86emu_read_byte(e, base + i + 1u) == 0x4Au
                && x86emu_read_byte(e, base + i + 2u) == 0xCDu
                && x86emu_read_byte(e, base + i + 3u) == 0x21u) {
                for (std::uint32_t j = 0; j < 4u; ++j)
                    x86emu_write_byte(e, base + i + j, 0x90u);
            }
        }
    };
    nopResizeInt21(flatBase + cstartEip);
    const std::uint32_t entryLin = flatBase + entryEip;
    std::fprintf(stderr, "[DPMI] LE loaded entry=%02x%02x%02x\n",
        static_cast<unsigned>(x86emu_read_byte(e, entryLin)),
        static_cast<unsigned>(x86emu_read_byte(e, entryLin + 1u)),
        static_cast<unsigned>(x86emu_read_byte(e, entryLin + 2u)));

    /* Flat shared mapping — LE virtual addresses are absolute offsets from flatBase. */
    /* CS=gdt[1] code; DS=gdt[2] must stay data-writable even when obj0 is execute. */
    gdt[1] = {flatBase, 0x000FFFFFu, 0x9Au, 0xCFu, true};
    gdt[2] = {flatBase, 0x000FFFFFu, 0x92u, 0xCFu, true};
    /* Per-object selectors must be R/W data — _cstart_ uses ES=0x24 (obj1) for PSP dwords. */
    for (std::uint32_t o = 0; o < numObj && o + 3u < GDT_ENTRIES; ++o) {
        gdt[o + 3u] = {flatBase, 0x000FFFFFu, 0x92u, 0xCFu, true};
    }
    const std::uint32_t kernIdx = numObj + 3u;
    if (kernIdx < GDT_ENTRIES) {
        dos4gwKernelSel = static_cast<std::uint16_t>(kernIdx * 8u);
        gdt[kernIdx] = {flatBase + DOS4GW_INFO_OFF, 0x000FFFFFu, 0x92u, 0xCFu, true};
    }
    pmPrepareSimdCpu(e);
    seedDos4gwPspStub(e, flatBase);

    const std::uint32_t startObj = x86emu_read_dword(e, le + LE_OFF_START_OBJ);
    (void)startObj;
    const std::uint16_t codeSel  = LE_SEL_FLAT_CODE;
    const std::uint16_t dataSel  = LE_SEL_FLAT_DATA;
    std::uint16_t stackSel = dataSel;
    if (stackObj > 0u && stackObj <= numObj)
        stackSel = static_cast<std::uint16_t>(LE_SEL_FLAT_DATA + stackObj * 8u);
    (void)startObj;

    std::uint32_t eip = entryEip;
    std::uint32_t esp = entryEsp;

    const std::uint32_t startObjIdx = startObj > 0u ? startObj - 1u : 0u;
    const std::uint32_t codeObjBase = leObjBase(e, objTab, startObjIdx);

    constexpr std::uint32_t kLeStartVa = 0x00040B48u;
    const bool useCstart = true;
    if (cstartEip && useCstart) {
        std::fprintf(stderr,
            "[DPMI] LE _start@%08x simulated; entering _cstart_%08x\n",
            kLeStartVa, cstartEip);
        eip = cstartEip;
    } else {
        eip = kLeStartVa;
        std::fprintf(stderr, "[DPMI] LE bootstrap via _start@%08x (cstart@%08x)\n",
            eip, cstartEip);
    }

    enterProtected32(e, codeSel, eip, stackSel, esp, dataSel, false);
    setupDos4gwLeEntry(e, flatBase, codeObjBase, useCstart && cstartEip != 0u && eip == cstartEip);
    leBootPending = true;
    leBootCs = codeSel;
    leBootSs = stackSel;
    leBootDs = dataSel;
    leBootEip = eip;
    leBootEsp = esp;
    lePostCstartEip = entryEip;
    cstartComplete = false;
    leBootEax = e->x86.R_EAX;
    leBootEdx = e->x86.R_EDX;
    leBootGs = static_cast<std::uint16_t>(e->x86.R_GS & 0xFFFFu);
    if (FieldX86Emu::dieMapped)
        FieldX86Emu::syncToDie(FieldX86Emu::dieMapped);
    std::fprintf(stderr,
        "[DPMI] DOS/4GW LE bootstrap base=%08x eip=%08x esp=%08x cs=%04x ss=%04x ds=%04x gs=%04x eax=%08x edx=%08x acc=%04x objs=%u pages=%u entry=%02x%02x%02x\n",
        flatBase, flatBase + eip, flatBase + esp,
        static_cast<unsigned>(codeSel), static_cast<unsigned>(stackSel),
        static_cast<unsigned>(dataSel),
        static_cast<unsigned>(e->x86.R_GS & 0xFFFFu),
        static_cast<unsigned>(e->x86.R_EAX),
        static_cast<unsigned>(e->x86.R_EDX),
        static_cast<unsigned>(e->x86.R_CS_ACC),
        numObj, numPages,
        static_cast<unsigned>(x86emu_read_byte(e, flatBase + eip)),
        static_cast<unsigned>(x86emu_read_byte(e, flatBase + eip + 1u)),
        static_cast<unsigned>(x86emu_read_byte(e, flatBase + eip + 2u)));
    return true;
}

} // namespace FieldDpmi