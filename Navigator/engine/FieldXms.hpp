#pragma once

// XMS 3.0 — control block, INT 2Fh AX=4310h install, INT 15h AH=87h/88h block move / size.

#include "FieldDos.hpp"
#include "FieldDpmi.hpp"
#include "FieldPlatform.hpp"
#include "FieldRaid.hpp"
#include "FieldRtxMemory.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>

#include <x86emu.h>

namespace FieldXms {

constexpr std::uint32_t POOL_BASE        = FieldPlatform::XMS_POOL_BYTE;
constexpr std::uint32_t POOL_BYTES     = FieldPlatform::XMS_POOL_BYTES;
constexpr std::uint32_t CTRL_OFF       = 0x0080u;
constexpr std::uint16_t SPEC_VER       = 0x0300u;
constexpr std::uint16_t MUX_INSTALL    = 0x4310u;
constexpr std::uint16_t MUX_DISPATCH   = 0x4311u;

struct ControlBlock {
    std::uint16_t specRev   = SPEC_VER;
    std::uint16_t driverOff = 0x0000u;
    std::uint32_t freeBytes = POOL_BYTES;
    std::uint16_t hmaAvail  = 0xFFFFu;
    std::uint16_t a20State  = 1u;
};

struct XmsHandle {
    std::uint32_t bytes = 0;
    std::uint32_t phys  = 0;
    bool          used  = false;
};

inline ControlBlock ctrl{};
inline XmsHandle    handles[256]{};
inline std::uint8_t nextHandle = 1u;
inline bool         installed  = false;
inline bool         a20Enabled = true;

inline std::uint32_t poolLimit() noexcept {
    const std::uint64_t end = static_cast<std::uint64_t>(POOL_BASE) + POOL_BYTES;
    return static_cast<std::uint32_t>(
        std::min(end, static_cast<std::uint64_t>(FieldPlatform::GUEST_RAM_BYTES)));
}

inline std::uint32_t xmsLinearBase() noexcept {
    return static_cast<std::uint32_t>(FieldDpmi::xmsCodeSeg) << 4;
}

inline void writeControlBlock(x86emu_t* e) noexcept {
    const std::uint32_t base = xmsLinearBase() + CTRL_OFF;
    x86emu_write_word(e, base + 0u, ctrl.specRev);
    x86emu_write_word(e, base + 2u, ctrl.driverOff);
    x86emu_write_dword(e, base + 4u, ctrl.freeBytes);
    x86emu_write_word(e, base + 8u, ctrl.hmaAvail);
    x86emu_write_word(e, base + 10u, ctrl.a20State);
}

inline void installDriverStub(x86emu_t* e) noexcept {
    const std::uint32_t base = xmsLinearBase();
    /* XMS entry: AH=05 -> AX=1 (A20 enabled); else INT 2F AX=4311h dispatch. */
    static const std::uint8_t stub[] = {
        0x80, 0xFC, 0x05, 0x75, 0x03, 0xB8, 0x01, 0x00, /* cmp ah,5; jne; mov ax,1 */
        0xC3,                                             /* ret */
        0xB8, 0x11, 0x43, 0xCD, 0x2F, 0xC3,               /* mov ax,4311h; int 2f; ret */
    };
    for (std::size_t i = 0; i < sizeof stub; ++i)
        x86emu_write_byte(e, base + static_cast<std::uint32_t>(i), stub[i]);
    ctrl.driverOff = 0x0000u;
    writeControlBlock(e);
    installed = true;
}

inline void deactivate() noexcept {
    installed = false;
    a20Enabled = true;
    ctrl = {};
    ctrl.specRev = SPEC_VER;
    ctrl.freeBytes = 0u;
    ctrl.hmaAvail = 0u;
    ctrl.a20State = 1u;
    nextHandle = 1u;
    for (auto& h : handles) h = {};
}

inline void activate(x86emu_t* e) noexcept {
    if (!e || !FieldRtxMemory::xmsLive()) return;
    FieldDpmi::install(e);
    installDriverStub(e);
    ctrl.freeBytes = POOL_BYTES;
    ctrl.hmaAvail = 0xFFFFu;
    writeControlBlock(e);
}

inline void install(x86emu_t* e) noexcept {
    if (!e) return;
    FieldRtxMemory::popXms(FieldDos::hdGuestRamPtr);
    activate(e);
}

inline bool copyGuestRange(x86emu_t* e, std::uint32_t dst, std::uint32_t src,
                           std::uint32_t len) noexcept {
    if (len == 0u) return true;
    auto* ram = FieldDos::hdGuestRamPtr;
    for (std::uint32_t i = 0; i < len; ++i) {
        const std::uint64_t s = src + i;
        const std::uint64_t d = dst + i;
        std::uint8_t v;
        if (s < FieldPlatform::GUEST_RAM_BYTES)
            v = x86emu_read_byte(e, static_cast<std::uint32_t>(s));
        else
            v = FieldRaid::readRam(s, ram);
        if (d < FieldPlatform::GUEST_RAM_BYTES)
            x86emu_write_byte(e, static_cast<std::uint32_t>(d), v);
        else if (!FieldRaid::ready)
            return false;
        else
            FieldRaid::writeRam(d, v, ram);
    }
    return true;
}

inline int moveExtBlock(x86emu_t* e) noexcept {
    const std::uint32_t si = (static_cast<std::uint32_t>(e->x86.R_DS & 0xFFFFu) << 4)
                           + (e->x86.R_ESI & 0xFFFFu);
    const std::uint32_t count = x86emu_read_dword(e, si) & 0xFFFFu;
    const std::uint32_t src   = x86emu_read_dword(e, si + 0x0Cu) & 0x00FFFFFFu;
    const std::uint32_t dst   = x86emu_read_dword(e, si + 0x14u) & 0x00FFFFFFu;
    if (count == 0u || (count & 1u) != 0u) {
        e->x86.R_AH = 0x86u;
        e->x86.R_FLG |= F_CF;
        return 1;
    }
    if (!copyGuestRange(e, dst, src, count)) {
        e->x86.R_AH = 0x86u;
        e->x86.R_FLG |= F_CF;
        return 1;
    }
    e->x86.R_AH = 0u;
    e->x86.R_FLG &= ~F_CF;
    return 1;
}

inline int dispatchDriver(x86emu_t* e) noexcept {
    const std::uint8_t ah = static_cast<std::uint8_t>(e->x86.R_EAX >> 8);
    switch (ah) {
    case 0x02:
    case 0x04:
        a20Enabled = true;
        ctrl.a20State = 1u;
        e->x86.R_AX = 0u;
        e->x86.R_BL = 0u;
        return 1;
    case 0x03:
    case 0x05:
        a20Enabled = false;
        ctrl.a20State = 0u;
        e->x86.R_AX = 0u;
        e->x86.R_BL = 0u;
        return 1;
    case 0x08:
        e->x86.R_AX = a20Enabled ? 1u : 0u;
        e->x86.R_BL = 0u;
        return 1;
    case 0x09: {
        const std::uint32_t want = ((e->x86.R_EDX & 0xFFFFu) << 16)
                                 | (e->x86.R_EAX & 0xFFFFu);
        if (want == 0u || want > ctrl.freeBytes || nextHandle == 0u) {
            e->x86.R_AX = 0u;
            e->x86.R_BL = 0xA0u;
            return 1;
        }
        const std::uint8_t hid = nextHandle++;
        XmsHandle& h = handles[hid];
        h.used  = true;
        h.bytes = want;
        h.phys  = POOL_BASE + (POOL_BYTES - ctrl.freeBytes);
        ctrl.freeBytes -= want;
        writeControlBlock(e);
        e->x86.R_DX = (e->x86.R_DX & 0xFFFF0000u) | hid;
        e->x86.R_AX = (e->x86.R_AX & 0xFFFF0000u) | static_cast<std::uint32_t>(want & 0xFFFFu);
        e->x86.R_BL = 0u;
        return 1;
    }
    case 0x0A: {
        const std::uint8_t hid = static_cast<std::uint8_t>(e->x86.R_DX & 0xFFu);
        if (hid == 0u || !handles[hid].used) {
            e->x86.R_AX = 0u;
            e->x86.R_BL = 0xA2u;
            return 1;
        }
        ctrl.freeBytes += handles[hid].bytes;
        handles[hid] = {};
        writeControlBlock(e);
        e->x86.R_AX = 0u;
        e->x86.R_BL = 0u;
        return 1;
    }
    case 0x0B: {
        const std::uint32_t ds = static_cast<std::uint32_t>(e->x86.R_DS & 0xFFFFu) << 4;
        const std::uint32_t si = e->x86.R_ESI & 0xFFFFu;
        const std::uint32_t srcHandle = x86emu_read_dword(e, ds + si) & 0xFFFFu;
        const std::uint32_t srcOff    = x86emu_read_dword(e, ds + si + 4u);
        const std::uint32_t dstHandle = x86emu_read_dword(e, ds + si + 8u) & 0xFFFFu;
        const std::uint32_t dstOff    = x86emu_read_dword(e, ds + si + 12u);
        const std::uint32_t len       = x86emu_read_dword(e, ds + si + 16u);
        std::uint32_t src = srcOff;
        std::uint32_t dst = dstOff;
        if (srcHandle != 0u) {
            if (!handles[srcHandle].used) {
                e->x86.R_AX = 0u;
                e->x86.R_BL = 0xA6u;
                return 1;
            }
            src = handles[srcHandle].phys + srcOff;
        }
        if (dstHandle != 0u) {
            if (!handles[dstHandle].used) {
                e->x86.R_AX = 0u;
                e->x86.R_BL = 0xA6u;
                return 1;
            }
            dst = handles[dstHandle].phys + dstOff;
        }
        if (!copyGuestRange(e, dst, src, len)) {
            e->x86.R_AX = 0u;
            e->x86.R_BL = 0xA7u;
            return 1;
        }
        e->x86.R_AX = 0u;
        e->x86.R_BL = 0u;
        return 1;
    }
    case 0x0E: {
        const std::uint8_t hid = static_cast<std::uint8_t>(e->x86.R_DX & 0xFFu);
        if (hid == 0u || !handles[hid].used) {
            e->x86.R_AX = 0u;
            e->x86.R_BL = 0xA2u;
            return 1;
        }
        e->x86.R_DX = (e->x86.R_DX & 0xFFFF0000u)
                    | static_cast<std::uint32_t>(handles[hid].bytes & 0xFFFFu);
        e->x86.R_AX = (e->x86.R_AX & 0xFFFF0000u)
                    | static_cast<std::uint32_t>((handles[hid].bytes >> 16) & 0xFFFFu);
        e->x86.R_BL = 0u;
        return 1;
    }
    default:
        e->x86.R_AX = 0u;
        e->x86.R_BL = 0x80u;
        return 1;
    }
}

inline int handleInt2fXms(x86emu_t* e) noexcept {
    if (!e) return 0;
    const std::uint16_t ax = static_cast<std::uint16_t>(e->x86.R_EAX & 0xFFFFu);
    if (ax == MUX_INSTALL) {
        if (!FieldRtxMemory::xmsLive()) return 0;
        activate(e);
        e->x86.R_AL = 0x80u;
        e->x86.R_ES = (e->x86.R_ES & 0xFFFF0000u) | FieldDpmi::xmsCodeSeg;
        e->x86.R_BX = (e->x86.R_BX & 0xFFFF0000u) | 0x0000u;
        return 1;
    }
    if (ax == MUX_DISPATCH) {
        if (!FieldRtxMemory::xmsLive()) return 0;
        return dispatchDriver(e);
    }
    return 0;
}

inline int handleInt15Xms(x86emu_t* e) noexcept {
    if (!e) return 0;
    const std::uint8_t ah = static_cast<std::uint8_t>(e->x86.R_EAX >> 8);
    if (ah == 0x88u) {
        e->x86.R_EAX = (e->x86.R_EAX & 0xFFFF0000u) | FieldPlatform::EXTENDED_KB;
        e->x86.R_FLG &= ~F_CF;
        return 1;
    }
    if (ah == 0x87u)
        return moveExtBlock(e);
    return 0;
}

} // namespace FieldXms