#pragma once

// Field Die native host CPU for MS-DOS 6.30 — guest RAM + regs live in FieldX86Die SSBO.

#include "FieldRtxCpu.hpp"
#include "FieldX86Native.hpp"
#include "FieldDpmi.hpp"

namespace FieldDpmi { extern bool inProtected; }
#include "FieldDevices.hpp"
#include "FieldPc.hpp"
#include "FieldInput.hpp"
#include "FieldDos.hpp"
#include "FieldDosConfig.hpp"
#include "FieldPlatform.hpp"
#include "FieldSb16.hpp"
#include "FieldVga.hpp"
#include "FieldVgaPlanes.hpp"
#include "FieldVesa.hpp"

#include <x86emu.h>

extern "C" {
#include <mem.h>
}

#include <cstdint>
#include <cstring>

namespace FieldBios {
void init(x86emu_t* emu);
void patchConventionalKb(x86emu_t* e, std::uint8_t* ram) noexcept;
int handleInt(x86emu_t* e, u8 num, std::uint32_t key, bool keyDown, std::uint64_t ticks);
void enqueueHostKey(std::uint16_t key) noexcept;
void pumpHostShell(x86emu_t* emu, std::uint32_t key, bool keyDown, std::uint8_t* ram);
void maintainBoot(x86emu_t* emu, u64 tsc, std::uint8_t* ram);
void recoverFromHalt(x86emu_t* emu);
inline bool pmExecActive = false;
inline bool guestBootSettled = false;
inline bool rtxShellActive = false;
}

namespace FieldX86Emu {

constexpr std::size_t GUEST_BYTES = FieldPlatform::GUEST_RAM_BYTES;
constexpr std::size_t GUEST_PAGES = GUEST_BYTES / X86EMU_PAGE_SIZE;

struct Ctx {
    std::uint32_t key     = 0;
    bool          keyDown = false;
    std::uint64_t ticks   = 0;
};

struct DieView {
    std::uint32_t EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP;
    std::uint32_t R8, R9, R10, R11, R12, R13, R14, R15;
    std::uint32_t CS, DS, ES, FS, GS, SS;
    std::uint32_t EIP, EFLAGS;
    std::uint32_t CR0, CR2, CR3, CR4, CR8;
};

inline x86emu_t* emu         = nullptr;
inline void*     dieMapped   = nullptr;
inline std::uint8_t* ramHost = nullptr;

// Chrome HUD writes guest RAM before the x86 emulator binds — keep ramHost in sync.
inline void bindChromeGuest(std::uint8_t* guestRam) noexcept {
    if (guestRam) ramHost = guestRam;
}
inline std::size_t ramByteOffset = 0;
inline bool      pagesMapped = false;
inline bool      biosInited  = false;
inline bool      benchForceExecute = false;
inline bool      guestAppExecute   = false;
inline Ctx       hostCtx{};
inline std::uint32_t lastFrameCycles = 0u;

inline std::uint32_t hostCyclesLastFrame() noexcept { return lastFrameCycles; }

inline unsigned memio(x86emu_t* e, u32 addr, u32* val, unsigned type) {
    const unsigned op = (type >> 8) & 0xFu;
    const unsigned w  = type & 3u;

    if (op == X86EMU_MEMIO_O) {
        if (w == X86EMU_MEMIO_8)
            FieldDevices::portOut(static_cast<std::uint16_t>(addr & 0xFFFFu),
                static_cast<std::uint8_t>(*val & 0xFFu));
        return 0;
    }
    if (op == X86EMU_MEMIO_I) {
        if (w == X86EMU_MEMIO_8)
            *val = FieldDevices::portIn(static_cast<std::uint16_t>(addr & 0xFFFFu));
        else if (w == X86EMU_MEMIO_16)
            *val = FieldDevices::portIn(static_cast<std::uint16_t>(addr & 0xFFFFu))
                 | (static_cast<std::uint32_t>(
                        FieldDevices::portIn(static_cast<std::uint16_t>((addr + 1u) & 0xFFFFu))) << 8);
        else
            *val = 0xFFFFFFFFu;
        return 0;
    }
    if (FieldVesa::active && addr >= FieldVesa::lfbAddr
        && addr < FieldVesa::lfbAddr + static_cast<std::uint32_t>(FieldVesa::pitch) * FieldVesa::height) {
        return vm_memio(e, addr, val, type);
    }
    if (FieldVgaPlanes::handlesMem(addr)) {
        if (op == X86EMU_MEMIO_W && w == X86EMU_MEMIO_8) {
            FieldVgaPlanes::memWrite(addr, static_cast<std::uint8_t>(*val & 0xFFu));
            return 0;
        }
        if (op == X86EMU_MEMIO_R && w == X86EMU_MEMIO_8) {
            *val = FieldVgaPlanes::memRead(addr);
            return 0;
        }
    }
    return vm_memio(e, addr, val, type);
}

inline int handleInt(x86emu_t* e, u8 num, unsigned type) {
    (void)type;
    return FieldBios::handleInt(e, num, hostCtx.key, hostCtx.keyDown, hostCtx.ticks);
}

inline int intrThunk(x86emu_t* e, u8 num, unsigned type) {
    return handleInt(e, num, type);
}

inline void cpuidThunk(x86emu_t* e) {
    const std::uint32_t leaf = static_cast<std::uint32_t>(e->x86.R_EAX);
    switch (leaf) {
    case 0u:
        e->x86.R_EAX = 1u;
        e->x86.R_EBX = 0x756E6547u;
        e->x86.R_ECX = 0x6C65746Eu;
        e->x86.R_EDX = 0x49656E69u;
        break;
    case 1u:
        /* Family in bits 8-11 (AH&0x0F) — DOS/4GW uses MOV AL,AH / AND AX,0Fh. */
        e->x86.R_EAX = 0x00000420u;
        e->x86.R_EBX = (e->x86.R_EBX & 0xFFFF0000u) | 0x0010u;
        e->x86.R_ECX = 0u;
        e->x86.R_EDX = (1u << 4) | (1u << 8) | (1u << 15) | (1u << 23);
        break;
    default:
        e->x86.R_EAX = e->x86.R_EBX = e->x86.R_ECX = e->x86.R_EDX = 0u;
        break;
    }
}

inline int codeThunk(x86emu_t* e) {
    if (FieldBios::pmExecActive) {
        FieldDpmi::normalizeRealModeSegs(e);
        const std::uint16_t cs = static_cast<std::uint16_t>(e->x86.R_CS & 0xFFFFu);
        const std::uint16_t ip = static_cast<std::uint16_t>(e->x86.R_EIP & 0xFFFFu);
        if (cs == FieldDpmi::dpmiCodeSeg && ip == 0u) {
            if (FieldDpmi::tryBootstrapDos4gw(e, 0x10000u, 720u * 1024u))
                return 1;
        }
    }
    return 0;
}

inline unsigned memioThunk(x86emu_t* e, u32 addr, u32* val, unsigned type) {
    return memio(e, addr, val, type);
}

inline void mapGuestRam(void* mapped, std::size_t offset) {
    if (!emu || !mapped) return;
    dieMapped = mapped;
    auto* guestRam = FieldDos::guestRam(static_cast<std::uint8_t*>(mapped), offset);
    if (ramHost == guestRam && pagesMapped && ramByteOffset == offset) return;
    ramHost = guestRam;
    ramByteOffset = offset;
    FieldX86Native::mapGuestRam(emu, guestRam, offset);
    pagesMapped = true;
    biosInited = false;
}

inline void syncVideoMode(void* mapped) {
    if (!emu || !mapped || !ramHost) return;
    const std::uint8_t mode = static_cast<std::uint8_t>(x86emu_read_byte(emu, 0x449u));
    FieldVga::mode = mode;
    auto* video = reinterpret_cast<std::uint32_t*>(
        static_cast<std::uint8_t*>(mapped) + FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET + 8u);
    *video = mode;
    std::memcpy(ramHost + FieldPlatform::VGA_PAL_RAM_BYTE, FieldVga::palette, sizeof FieldVga::palette);
}

inline void ensure(void* mapped, std::size_t offset) {
    if (!mapped) return;
    if (!emu) {
        emu = FieldX86Native::create(X86EMU_PERM_R | X86EMU_PERM_W | X86EMU_PERM_X);
        x86emu_set_memio_handler(emu, memioThunk);
        x86emu_set_intr_handler(emu, intrThunk);
        x86emu_set_code_handler(emu, codeThunk);
        x86emu_set_cpuid_handler(emu, cpuidThunk);
    }
    mapGuestRam(mapped, offset);
    FieldDevices::bindGuest(ramHost);
    if (!biosInited) {
        FieldDosConfig::applyPreset(FieldDosConfig::cfg.preset);
        FieldBios::init(emu);
        FieldVga::init(ramHost);
        FieldDpmi::rmIntDispatch = [](x86emu_t* e, std::uint8_t n) {
            return FieldBios::handleInt(e, n, hostCtx.key, hostCtx.keyDown, hostCtx.ticks);
        };
        biosInited = true;
    }
}

inline void syncFromDie(void* mapped) {
    if (!emu || !mapped) return;
    const auto* d = static_cast<const DieView*>(mapped);
    emu->x86.R_EAX = d->EAX;
    emu->x86.R_EBX = d->EBX;
    emu->x86.R_ECX = d->ECX;
    emu->x86.R_EDX = d->EDX;
    emu->x86.R_ESI = d->ESI;
    emu->x86.R_EDI = d->EDI;
    emu->x86.R_EBP = d->EBP;
    emu->x86.R_ESP = d->ESP;
    emu->x86.R_EIP = d->EIP;
    emu->x86.R_FLG = d->EFLAGS;
    x86emu_set_seg_register(emu, emu->x86.R_CS_SEL, static_cast<u16>(d->CS & 0xFFFFu));
    x86emu_set_seg_register(emu, emu->x86.R_DS_SEL, static_cast<u16>(d->DS & 0xFFFFu));
    x86emu_set_seg_register(emu, emu->x86.R_ES_SEL, static_cast<u16>(d->ES & 0xFFFFu));
    x86emu_set_seg_register(emu, emu->x86.R_SS_SEL, static_cast<u16>(d->SS & 0xFFFFu));
    x86emu_set_seg_register(emu, emu->x86.R_FS_SEL, static_cast<u16>(d->FS & 0xFFFFu));
    x86emu_set_seg_register(emu, emu->x86.R_GS_SEL, static_cast<u16>(d->GS & 0xFFFFu));
    emu->x86.crx[0] = d->CR0;
    FieldDpmi::inProtected = (d->CR0 & 1u) != 0u;
}

inline void syncToDie(void* mapped) {
    if (!emu || !mapped) return;
    auto* d = static_cast<DieView*>(mapped);
    d->EAX    = emu->x86.R_EAX;
    d->EBX    = emu->x86.R_EBX;
    d->ECX    = emu->x86.R_ECX;
    d->EDX    = emu->x86.R_EDX;
    d->ESI    = emu->x86.R_ESI;
    d->EDI    = emu->x86.R_EDI;
    d->EBP    = emu->x86.R_EBP;
    d->ESP    = emu->x86.R_ESP;
    d->EIP    = emu->x86.R_EIP;
    d->EFLAGS = emu->x86.R_FLG;
    d->CS     = emu->x86.R_CS;
    d->DS     = emu->x86.R_DS;
    d->ES     = emu->x86.R_ES;
    d->SS     = emu->x86.R_SS;
    d->FS     = emu->x86.R_FS;
    d->GS     = emu->x86.R_GS;
    d->CR0    = emu->x86.crx[0];
    syncVideoMode(mapped);
}

inline std::uint32_t runMapped(void* mapped, std::size_t offset, std::size_t cycleOffset,
                               Ctx& ctx, std::uint32_t maxInstr)
{
    if (!mapped) return 0;
    static std::uint32_t prevEnqueueKey = 0;
    static std::uint16_t prevBiosKey = 0;
    const std::uint16_t biosKey = FieldInput::state.keyboard.biosKey;
    if (biosKey != 0u && biosKey != prevBiosKey)
        FieldBios::enqueueHostKey(biosKey);
    if (biosKey == 0u)
        prevBiosKey = 0;
    else
        prevBiosKey = biosKey;
    if (ctx.keyDown && ctx.key != 0u && ctx.key != prevEnqueueKey)
        FieldBios::enqueueHostKey(static_cast<std::uint16_t>(ctx.key & 0xFFFFu));
    if (!ctx.keyDown || ctx.key == 0u)
        prevEnqueueKey = 0;
    else
        prevEnqueueKey = ctx.key;
    hostCtx = ctx;
    ensure(mapped, offset);
    dieMapped = mapped;
    /* Host libx86emu owns PM DOS/4GW state — never clobber from stale GPU die regs. */
    const bool hostPmDos4gw = FieldBios::pmExecActive && FieldDpmi::isProtected(emu);
    if (!hostPmDos4gw)
        syncFromDie(mapped);
    if (FieldBios::pmExecActive && FieldDpmi::leBootPending && emu) {
        FieldDpmi::leBootPending = false;
        FieldDpmi::inProtected = true;
        emu->x86.crx[0] |= 1u;
        emu->x86.mode |= _MODE_CODE32 | _MODE_DATA32 | _MODE_STACK32;
        emu->x86.mode &= ~_MODE_HALTED;
        emu->x86.R_EIP = FieldDpmi::leBootEip;
        emu->x86.R_ESP = FieldDpmi::leBootEsp;
        emu->x86.R_EAX = FieldDpmi::leBootEax;
        emu->x86.R_EDX = FieldDpmi::leBootEdx;
        x86emu_set_seg_register(emu, emu->x86.R_CS_SEL, FieldDpmi::leBootCs);
        x86emu_set_seg_register(emu, emu->x86.R_SS_SEL, FieldDpmi::leBootSs);
        x86emu_set_seg_register(emu, emu->x86.R_DS_SEL, FieldDpmi::leBootDs);
        x86emu_set_seg_register(emu, emu->x86.R_ES_SEL, FieldDpmi::leBootDs);
        x86emu_set_seg_register(emu, emu->x86.R_GS_SEL, FieldDpmi::leBootGs);
        for (int idx : {R_CS_INDEX, R_SS_INDEX, R_DS_INDEX, R_ES_INDEX, R_FS_INDEX, R_GS_INDEX})
            emu->x86.seg[idx].limit = 0xFFFFFFFFu;
        emu->x86.R_FLG |= F_IF;
    }

    std::uint32_t budget = maxInstr;
    u64 ran = 0;
    const bool forceExec = benchForceExecute || guestAppExecute;
    const bool settledIdle = FieldBios::guestBootSettled && !forceExec;
    if (settledIdle) {
        if (!FieldBios::rtxShellActive)
            FieldBios::pumpHostShell(emu, ctx.key, ctx.keyDown, ramHost);
        ran = static_cast<u64>(budget);
    } else {
        if (FieldBios::guestBootSettled && !forceExec)
            budget = std::min(budget, FieldDosConfig::cfg.cyclesRun);

        if (!forceExec)
            FieldBios::maintainBoot(emu, emu->x86.R_TSC, ramHost);
        FieldBios::recoverFromHalt(emu);
        const bool hostPm = FieldBios::pmExecActive && FieldDpmi::isProtected(emu);
        if (forceExec && !hostPm)
            emu->x86.mode &= ~_MODE_HALTED;

        const u64 t0 = emu->x86.R_TSC;
        const u64 goal = t0 + static_cast<u64>(budget);
        if (hostPm)
            ran = FieldRtxCpu::runPm(emu, goal - t0, FieldRtxCpu::pmGuardForDos4gw());
        else
            ran = FieldRtxCpu::runRm(emu, goal - t0);
        FieldPc::tick(emu, ran);
        if (!FieldBios::guestBootSettled && !forceExec)
            FieldBios::maintainBoot(emu, emu->x86.R_TSC, ramHost);
        FieldBios::recoverFromHalt(emu);
    }

    if (FieldSb16::isIrqPending() && emu) {
        x86emu_intr_raise(emu, FieldDosConfig::sbIrqVector(), 1u, 0);
        FieldSb16::consumeIrq();
    }

    syncToDie(mapped);
    auto* cycles = reinterpret_cast<std::uint32_t*>(static_cast<std::uint8_t*>(mapped) + cycleOffset);
    *cycles = static_cast<std::uint32_t>(*cycles + ran);
    lastFrameCycles = FieldRtxCpu::cyclesLastFrame;
    ctx.ticks += ran;
    hostCtx.ticks = ctx.ticks;
    return static_cast<std::uint32_t>(*cycles);
}

inline void invalidateBios() noexcept {
    biosInited = false;
    FieldBios::guestBootSettled = false;
}

inline void shutdown() noexcept {
    if (emu) {
        FieldX86Native::destroy(emu);
        emu = nullptr;
    }
    ramHost = nullptr;
    ramByteOffset = 0;
    pagesMapped = false;
    biosInited = false;
}

} // namespace FieldX86Emu