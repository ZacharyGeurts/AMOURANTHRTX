#pragma once

// LIM EMS 4.0 — page frame at segment 0xE000, 64 pool pages in guest RAM at 0x00E00000.

#include "FieldPlatform.hpp"
#include "FieldRtxMemory.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>

#include <x86emu.h>

namespace FieldEms {

constexpr std::uint16_t PAGE_FRAME_SEG     = 0xE000u;
constexpr std::uint32_t PAGE_FRAME_LINEAR  = 0x000E0000u;
constexpr std::uint32_t FRAME_PAGE_BYTES   = 0x4000u;   // 16 KiB per frame slot
constexpr std::uint32_t FRAME_SLOTS        = 4u;        // 64 KiB page frame
constexpr std::uint32_t POOL_BASE          = 0x00E00000u;
constexpr std::uint32_t POOL_PAGES         = 64u;
constexpr std::uint32_t PAGE_BYTES         = 0x4000u;
constexpr std::uint8_t  UNMAPPED           = 0xFFu;

struct EmsHandle {
    std::uint8_t  pages      = 0;
    std::uint8_t  firstPool  = 0;
    std::uint8_t  frameMap[FRAME_SLOTS]{};
    bool          active     = false;
};

inline std::uint16_t pageFrameSeg  = PAGE_FRAME_SEG;
inline std::uint16_t freePages     = static_cast<std::uint16_t>(POOL_PAGES);
inline std::uint8_t  nextHandle    = 1u;
inline bool          poolInit    = false;
inline EmsHandle     handles[256]{};
inline bool          poolUsed[POOL_PAGES]{};

inline void deactivate() noexcept {
    poolInit = false;
    freePages = static_cast<std::uint16_t>(POOL_PAGES);
    nextHandle = 1u;
    pageFrameSeg = PAGE_FRAME_SEG;
    for (auto& h : handles) h = {};
    for (auto& u : poolUsed) u = false;
}

inline void initPool(x86emu_t* e) noexcept {
    if (!e || poolInit || !FieldRtxMemory::emmLive()) return;
    for (std::uint32_t pg = 0; pg < POOL_PAGES; ++pg) {
        const std::uint32_t base = POOL_BASE + pg * PAGE_BYTES;
        for (std::uint32_t i = 0; i < PAGE_BYTES; ++i)
            x86emu_write_byte(e, base + i, 0);
        poolUsed[pg] = false;
    }
    poolInit = true;
}

inline void activate(x86emu_t* e) noexcept {
    if (!e || !FieldRtxMemory::emmLive()) return;
    initPool(e);
}

inline int allocPoolRun(std::uint8_t count, std::uint8_t& firstOut) noexcept {
    if (count == 0 || count > freePages) return 0;
    for (std::uint32_t start = 0; start + count <= POOL_PAGES; ++start) {
        bool ok = true;
        for (std::uint8_t i = 0; i < count; ++i) {
            if (poolUsed[start + i]) { ok = false; break; }
        }
        if (!ok) continue;
        for (std::uint8_t i = 0; i < count; ++i)
            poolUsed[start + i] = true;
        firstOut = static_cast<std::uint8_t>(start);
        freePages = static_cast<std::uint16_t>(freePages - count);
        return 1;
    }
    return 0;
}

inline void freePoolRun(std::uint8_t first, std::uint8_t count) noexcept {
    for (std::uint8_t i = 0; i < count && (first + i) < POOL_PAGES; ++i)
        poolUsed[first + i] = false;
    freePages = static_cast<std::uint16_t>(
        std::min<std::uint32_t>(POOL_PAGES, static_cast<std::uint32_t>(freePages) + count));
}

inline void copyPoolToFrame(x86emu_t* e, std::uint8_t poolPage, std::uint8_t frameSlot) noexcept {
    const std::uint32_t src = POOL_BASE + static_cast<std::uint32_t>(poolPage) * PAGE_BYTES;
    const std::uint32_t dst = PAGE_FRAME_LINEAR + static_cast<std::uint32_t>(frameSlot) * FRAME_PAGE_BYTES;
    for (std::uint32_t i = 0; i < PAGE_BYTES; ++i)
        x86emu_write_byte(e, dst + i, x86emu_read_byte(e, src + i));
}

inline void copyFrameToPool(x86emu_t* e, std::uint8_t frameSlot, std::uint8_t poolPage) noexcept {
    const std::uint32_t src = PAGE_FRAME_LINEAR + static_cast<std::uint32_t>(frameSlot) * FRAME_PAGE_BYTES;
    const std::uint32_t dst = POOL_BASE + static_cast<std::uint32_t>(poolPage) * PAGE_BYTES;
    for (std::uint32_t i = 0; i < PAGE_BYTES; ++i)
        x86emu_write_byte(e, dst + i, x86emu_read_byte(e, src + i));
}

inline EmsHandle* handlePtr(std::uint16_t handle) noexcept {
    if (handle == 0u || handle >= nextHandle) return nullptr;
    EmsHandle& h = handles[handle];
    return h.active ? &h : nullptr;
}

inline int handleInt34(x86emu_t* e) noexcept {
    if (!e) return 0;
    if (!FieldRtxMemory::emmLive()) return 0;
    initPool(e);
    const std::uint8_t ah = static_cast<std::uint8_t>(e->x86.R_EAX >> 8);
    switch (ah) {
    case 0x40:
        e->x86.R_AL = 0x80u;
        return 1;
    case 0x41:
        e->x86.R_BX = (e->x86.R_BX & 0xFFFF0000u) | pageFrameSeg;
        return 1;
    case 0x42:
        e->x86.R_BX = (e->x86.R_BX & 0xFFFF0000u) | freePages;
        e->x86.R_AX = 0u;
        return 1;
    case 0x43: {
        const std::uint16_t pages = static_cast<std::uint16_t>(e->x86.R_BX & 0xFFFFu);
        if (pages == 0u || pages > freePages || nextHandle == 0u) {
            e->x86.R_AX = (e->x86.R_AX & 0xFFFF0000u) | 0x0084u;
            return 1;
        }
        std::uint8_t first = 0;
        if (!allocPoolRun(static_cast<std::uint8_t>(pages), first)) {
            e->x86.R_AX = (e->x86.R_AX & 0xFFFF0000u) | 0x0084u;
            return 1;
        }
        const std::uint8_t hid = nextHandle++;
        EmsHandle& h = handles[hid];
        h = {};
        h.active = true;
        h.pages = static_cast<std::uint8_t>(pages);
        h.firstPool = first;
        for (std::uint32_t s = 0; s < FRAME_SLOTS; ++s)
            h.frameMap[s] = UNMAPPED;
        e->x86.R_DX = (e->x86.R_DX & 0xFFFF0000u) | hid;
        e->x86.R_AX = 0u;
        return 1;
    }
    case 0x44: {
        const std::uint8_t al = static_cast<std::uint8_t>(e->x86.R_EAX & 0xFFu);
        const std::uint16_t handle = static_cast<std::uint16_t>(e->x86.R_DX & 0xFFFFu);
        const std::uint8_t frameSlot = static_cast<std::uint8_t>(e->x86.R_CX & 0xFFu);
        EmsHandle* h = handlePtr(handle);
        if (!h || frameSlot >= FRAME_SLOTS) {
            e->x86.R_AX = (e->x86.R_AX & 0xFFFF0000u) | 0x0083u;
            return 1;
        }
        if (al == 0u) {
            const std::uint8_t logical = static_cast<std::uint8_t>(e->x86.R_BX & 0xFFu);
            if (logical >= h->pages) {
                e->x86.R_AX = (e->x86.R_AX & 0xFFFF0000u) | 0x0085u;
                return 1;
            }
            if (h->frameMap[frameSlot] != UNMAPPED)
                copyFrameToPool(e, frameSlot, static_cast<std::uint8_t>(h->firstPool + h->frameMap[frameSlot]));
            copyPoolToFrame(e, static_cast<std::uint8_t>(h->firstPool + logical), frameSlot);
            h->frameMap[frameSlot] = logical;
            e->x86.R_AX = 0u;
            return 1;
        }
        if (al == 1u) {
            if (h->frameMap[frameSlot] != UNMAPPED) {
                copyFrameToPool(e, frameSlot,
                    static_cast<std::uint8_t>(h->firstPool + h->frameMap[frameSlot]));
                h->frameMap[frameSlot] = UNMAPPED;
            }
            e->x86.R_AX = 0u;
            return 1;
        }
        e->x86.R_AX = (e->x86.R_AX & 0xFFFF0000u) | 0x0084u;
        return 1;
    }
    case 0x4D: {
        const std::uint8_t al = static_cast<std::uint8_t>(e->x86.R_EAX & 0xFFu);
        const std::uint16_t handle = static_cast<std::uint16_t>(e->x86.R_DX & 0xFFFFu);
        EmsHandle* h = handlePtr(handle);
        if (!h) {
            e->x86.R_AX = (e->x86.R_AX & 0xFFFF0000u) | 0x0083u;
            return 1;
        }
        const std::uint32_t es = static_cast<std::uint32_t>(e->x86.R_ES & 0xFFFFu) << 4;
        const std::uint32_t ds = static_cast<std::uint32_t>(e->x86.R_DS & 0xFFFFu) << 4;
        const std::uint32_t di = e->x86.R_EDI & 0xFFFFu;
        const std::uint32_t si = e->x86.R_ESI & 0xFFFFu;
        if (al == 0u) {
            for (std::uint32_t s = 0; s < FRAME_SLOTS; ++s) {
                const std::uint8_t v = (h->frameMap[s] == UNMAPPED)
                    ? 0u : static_cast<std::uint8_t>(h->frameMap[s] + 1u);
                x86emu_write_byte(e, es + di + s, v);
            }
            e->x86.R_AX = 0u;
            return 1;
        }
        if (al == 1u) {
            for (std::uint32_t s = 0; s < FRAME_SLOTS; ++s) {
                const std::uint8_t v = x86emu_read_byte(e, ds + si + s);
                if (v == 0u) {
                    if (h->frameMap[s] != UNMAPPED) {
                        copyFrameToPool(e, static_cast<std::uint8_t>(s),
                            static_cast<std::uint8_t>(h->firstPool + h->frameMap[s]));
                        h->frameMap[s] = UNMAPPED;
                    }
                    continue;
                }
                const std::uint8_t logical = static_cast<std::uint8_t>(v - 1u);
                if (logical >= h->pages) {
                    e->x86.R_AX = (e->x86.R_AX & 0xFFFF0000u) | 0x0085u;
                    return 1;
                }
                if (h->frameMap[s] != UNMAPPED)
                    copyFrameToPool(e, static_cast<std::uint8_t>(s),
                        static_cast<std::uint8_t>(h->firstPool + h->frameMap[s]));
                copyPoolToFrame(e, static_cast<std::uint8_t>(h->firstPool + logical),
                    static_cast<std::uint8_t>(s));
                h->frameMap[s] = logical;
            }
            e->x86.R_AX = 0u;
            return 1;
        }
        e->x86.R_AX = (e->x86.R_AX & 0xFFFF0000u) | 0x0084u;
        return 1;
    }
    case 0x59:
        e->x86.R_AL = 0x04u;
        return 1;
    default:
        e->x86.R_AH = 0x84u;
        return 1;
    }
}

} // namespace FieldEms