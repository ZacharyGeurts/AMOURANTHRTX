#pragma once

// Field Die native x86 host engine — owned decode/exec wired to SSBO + thermo cycle_count.
// FieldX86Core (in-tree libx86emu) — see FieldX86Core.hpp for the public FieldX86 API.

#include "FieldPlatform.hpp"
#include "FieldX86Core.hpp"

#include <x86emu.h>

#include <cstdint>
#include <cstring>

namespace FieldX86Native {

constexpr std::size_t GUEST_BYTES = FieldPlatform::GUEST_RAM_BYTES;
constexpr std::size_t GUEST_PAGES = GUEST_BYTES / X86EMU_PAGE_SIZE;

inline x86emu_t*         cpu       = nullptr;
inline std::uint8_t*     ram       = nullptr;
inline std::size_t       ramOffset = 0;
inline bool              ramMapped = false;

inline std::uint32_t flatCsBase(x86emu_t* e) noexcept {
    return e ? e->x86.R_CS_BASE : 0u;
}

inline std::uint32_t flatSsBase(x86emu_t* e) noexcept {
    return e ? e->x86.R_SS_BASE : 0u;
}

inline std::uint32_t pmRead32(x86emu_t* e, std::uint32_t va) noexcept {
    return x86emu_read_dword(e, va);
}

inline void pmWrite32(x86emu_t* e, std::uint32_t va, std::uint32_t v) noexcept {
    x86emu_write_dword(e, va, v);
}

inline void pmPush32(x86emu_t* e, std::uint32_t v) noexcept {
    e->x86.R_ESP -= 4u;
    pmWrite32(e, flatSsBase(e) + e->x86.R_ESP, v);
}

inline std::uint32_t pmPop32(x86emu_t* e) noexcept {
    const std::uint32_t va = flatSsBase(e) + e->x86.R_ESP;
    const std::uint32_t v  = pmRead32(e, va);
    e->x86.R_ESP += 4u;
    return v;
}

constexpr std::uint32_t FLAT_VA_LO = 0x00010000u;
constexpr std::uint32_t FLAT_VA_HI = 0x00100000u;

inline bool flatVaOk(std::uint32_t va) noexcept {
    return va >= FLAT_VA_LO && va < FLAT_VA_HI;
}

/* Decode FF /r (call/jmp r/m) and near ret — fixes stack skew when faults skip calls. */
/* Pre-decode guard: skip register call when target lies outside LE flat window. */
inline bool skipBadRegCallAt(x86emu_t* e, std::uint32_t eip) noexcept {
    if (!e) return false;
    const std::uint32_t pc = flatCsBase(e) + eip;
    if (static_cast<std::uint8_t>(x86emu_read_byte(e, pc)) != 0xFFu) return false;
    const std::uint8_t modrm = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + 1u));
    if ((modrm >> 6) != 3u) return false;
    const int reg = (modrm >> 3) & 7;
    if (reg != 2 && reg != 3) return false;
    std::uint32_t target = 0u;
    switch (modrm & 7) {
    case 0: target = e->x86.R_EAX; break;
    case 1: target = e->x86.R_ECX; break;
    case 2: target = e->x86.R_EDX; break;
    case 3: target = e->x86.R_EBX; break;
    case 4: target = e->x86.R_ESP; break;
    case 5: target = e->x86.R_EBP; break;
    case 6: target = e->x86.R_ESI; break;
    default: target = e->x86.R_EDI; break;
    }
    if (flatVaOk(target)) return false;
    e->x86.R_EIP = eip + 2u;
    e->x86.saved_eip = e->x86.R_EIP;
    e->x86.mode &= ~_MODE_HALTED;
    return true;
}

inline bool tryEmulateFfCallJmp(x86emu_t* e, std::uint32_t faultEip) noexcept {
    if (!e) return false;
    const std::uint32_t pc = flatCsBase(e) + faultEip;
    const std::uint8_t b0  = static_cast<std::uint8_t>(x86emu_read_byte(e, pc));
    if (b0 != 0xFFu) return false;

    const std::uint8_t modrm = static_cast<std::uint8_t>(x86emu_read_byte(e, pc + 1u));
    const int reg            = (modrm >> 3) & 7;
    const int mod            = modrm >> 6;
    const std::uint32_t len  = 2u;

    std::uint32_t target = 0u;
    if (mod == 3u) {
        target = [&]() -> std::uint32_t {
            switch (modrm & 7) {
            case 0: return e->x86.R_EAX;
            case 1: return e->x86.R_ECX;
            case 2: return e->x86.R_EDX;
            case 3: return e->x86.R_EBX;
            case 4: return e->x86.R_ESP;
            case 5: return e->x86.R_EBP;
            case 6: return e->x86.R_ESI;
            default: return e->x86.R_EDI;
            }
        }();
    } else {
        return false; /* memory-indirect: let native core handle */
    }

    if (reg == 2 || reg == 3) { /* call near */
        if (!flatVaOk(target)) {
            e->x86.R_EIP = faultEip + len;
            e->x86.saved_eip = e->x86.R_EIP;
            e->x86.mode &= ~_MODE_HALTED;
            return true;
        }
        pmPush32(e, faultEip + len);
        e->x86.R_EIP = target;
        e->x86.saved_eip = target;
        e->x86.mode &= ~_MODE_HALTED;
        return true;
    }
    if (reg == 4 || reg == 5) { /* jmp near */
        e->x86.R_EIP = target;
        e->x86.saved_eip = target;
        e->x86.mode &= ~_MODE_HALTED;
        return true;
    }
    return false;
}

inline bool tryEmulateNearRet(x86emu_t* e, std::uint32_t faultEip) noexcept {
    if (!e) return false;
    const std::uint32_t pc = flatCsBase(e) + faultEip;
    const std::uint8_t b0  = static_cast<std::uint8_t>(x86emu_read_byte(e, pc));
    if (b0 == 0xC3u) {
        e->x86.R_EIP = pmPop32(e);
        e->x86.saved_eip = e->x86.R_EIP;
        e->x86.mode &= ~_MODE_HALTED;
        return true;
    }
    if (b0 == 0xC2u) {
        const std::uint16_t imm = static_cast<std::uint16_t>(x86emu_read_word(e, pc + 1u));
        e->x86.R_EIP = pmPop32(e);
        e->x86.R_ESP += imm;
        e->x86.saved_eip = e->x86.R_EIP;
        e->x86.mode &= ~_MODE_HALTED;
        return true;
    }
    return false;
}

inline bool tryEmulateNearCallRel(x86emu_t* e, std::uint32_t faultEip) noexcept {
    if (!e) return false;
    const std::uint32_t pc = flatCsBase(e) + faultEip;
    if (static_cast<std::uint8_t>(x86emu_read_byte(e, pc)) != 0xE8u) return false;
    const std::int32_t rel = static_cast<std::int32_t>(x86emu_read_dword(e, pc + 1u));
    const std::uint32_t ret = faultEip + 5u;
    pmPush32(e, ret);
    e->x86.R_EIP = faultEip + 5u + static_cast<std::uint32_t>(rel);
    e->x86.saved_eip = e->x86.R_EIP;
    e->x86.mode &= ~_MODE_HALTED;
    return true;
}

inline std::uint32_t lastGoodEip = 0u;

inline int flatGuard(x86emu_t* e) noexcept {
    if (!e) return 0;
    if (flatVaOk(e->x86.R_EIP)) {
        lastGoodEip = e->x86.R_EIP;
        return 0;
    }
    if (lastGoodEip && flatVaOk(lastGoodEip)) {
        e->x86.R_EIP = lastGoodEip;
        return 0;
    }
    e->x86.mode |= _MODE_HALTED;
    return 1;
}

inline void mapGuestRam(x86emu_t* e, std::uint8_t* guest, std::size_t offset) noexcept {
    if (!e || !guest) return;
    if (ram == guest && ramMapped && ramOffset == offset) return;
    ram       = guest;
    ramOffset = offset;
    for (std::size_t p = 0; p < GUEST_PAGES; ++p)
        x86emu_set_page(e, static_cast<unsigned>(p * X86EMU_PAGE_SIZE),
            guest + p * X86EMU_PAGE_SIZE);
    x86emu_set_perm(e, 0, static_cast<unsigned>(GUEST_BYTES - 1),
        X86EMU_PERM_R | X86EMU_PERM_W | X86EMU_PERM_X);
    ramMapped = true;
}

inline void ensureFlatSegLimits(x86emu_t* e) noexcept {
    if (!e) return;
    for (int idx : {R_CS_INDEX, R_SS_INDEX, R_DS_INDEX, R_ES_INDEX, R_FS_INDEX, R_GS_INDEX})
        e->x86.seg[idx].limit = 0xFFFFFFFFu;
}

/* Native PM run loop — one insn at a time with flat guard, no fault ping-pong stalls. */
inline std::uint64_t runPmBudget(x86emu_t* e, std::uint64_t budget,
                                 x86emu_code_handler_t preStep) noexcept {
    if (!e || budget == 0u) return 0u;
    ensureFlatSegLimits(e);
    const u64 t0 = e->x86.R_TSC;
    const u64 goal = t0 + budget;
    int stalls = 0;
    while (e->x86.R_TSC < goal) {
        e->x86.mode &= ~_MODE_HALTED;
        if (preStep)
            (void)preStep(e);
        if (flatGuard(e))
            break;
        const u64 tBefore = e->x86.R_TSC;
        const std::uint32_t eip = e->x86.R_EIP;
        const bool pmHot = (eip >= 0x82bd6u && eip < 0x83000u);
        const u64 chunk = pmHot ? 256u : 2048u;
        e->max_instr = tBefore + std::min<u64>(goal - tBefore, chunk);
        const unsigned rs = x86emu_run(e, X86EMU_RUN_MAX_INSTR);
        if (!flatVaOk(e->x86.R_EIP)) {
            if (lastGoodEip && flatVaOk(lastGoodEip))
                e->x86.R_EIP = lastGoodEip;
            else if (rs & X86EMU_RUN_NO_CODE)
                break;
        } else {
            lastGoodEip = e->x86.R_EIP;
        }
        if (e->x86.R_TSC <= tBefore) {
            e->x86.R_TSC = tBefore + 1u;
            if (++stalls > 4096) break;
        } else {
            stalls = 0;
        }
    }
    return e->x86.R_TSC - t0;
}

inline std::uint64_t runRmBudget(x86emu_t* e, std::uint64_t budget) noexcept {
    if (!e || budget == 0u) return 0u;
    const u64 t0 = e->x86.R_TSC;
    const u64 goal = t0 + budget;
    while (e->x86.R_TSC < goal) {
        e->x86.mode &= ~_MODE_HALTED;
        const u64 tBefore = e->x86.R_TSC;
        const u64 chunk = std::min<u64>(goal - tBefore, 1'000'000u);
        e->max_instr = tBefore + chunk;
        (void)x86emu_run(e, X86EMU_RUN_MAX_INSTR);
        if (e->x86.R_TSC <= tBefore)
            e->x86.R_TSC = tBefore + 1u;
    }
    return e->x86.R_TSC - t0;
}

inline x86emu_t* create(unsigned memPerm = X86EMU_PERM_RWX) noexcept {
    auto* e = x86emu_new(memPerm, X86EMU_PERM_R | X86EMU_PERM_W);
    if (e) {
        cpu = e;
        lastGoodEip = 0u;
    }
    return e;
}

inline void destroy(x86emu_t* e) noexcept {
    if (e) {
        x86emu_done(e);
        if (cpu == e) cpu = nullptr;
    }
    ram       = nullptr;
    ramOffset = 0;
    ramMapped = false;
}

} // namespace FieldX86Native