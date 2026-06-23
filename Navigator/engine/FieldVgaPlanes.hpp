#pragma once

// EGA/VGA plane sequencer (0x3C4) + graphics controller (0x3CE) — planar modes 0x0D/0x12.

#include "FieldVga.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace FieldVgaPlanes {

constexpr std::uint16_t SEQ_INDEX = 0x3C4u;
constexpr std::uint16_t SEQ_DATA  = 0x3C5u;
constexpr std::uint16_t GC_INDEX  = 0x3CEu;
constexpr std::uint16_t GC_DATA   = 0x3CFu;
constexpr std::uint16_t ATTR_TOGGLE = 0x3C0u;
constexpr std::uint16_t ATTR_STATUS = 0x3DAu;

constexpr std::uint32_t PLANE_BYTES = 0x10000u;
constexpr std::uint32_t META_BYTE   = 0x00098FF0u;
constexpr std::uint32_t LFB_BASE    = 0x00E00000u;

inline std::uint8_t seqIndex = 0;
inline std::uint8_t seq[4]{};
inline std::uint8_t gcIndex = 0;
inline std::uint8_t gc[9]{};
inline std::uint8_t latch[4]{};
inline std::uint8_t readPlane = 0;
inline std::uint8_t attrFlip = 0;
inline std::uint8_t planes[4][PLANE_BYTES]{};
inline std::uint8_t* guestRamPtr = nullptr;

inline bool planarActive() noexcept {
    const std::uint8_t m = FieldVga::mode;
    return m == FieldVga::MODE_EGA_0D || m == FieldVga::MODE_EGA_12;
}

inline bool handlesMem(std::uint32_t addr) noexcept {
    return planarActive() && addr >= FieldVga::VGA_FB && addr < FieldVga::VGA_FB + 0x20000u;
}

inline std::uint32_t fbWidth() noexcept {
    return FieldVga::mode == FieldVga::MODE_EGA_12 ? 640u : 320u;
}

inline std::uint32_t fbHeight() noexcept {
    return FieldVga::mode == FieldVga::MODE_EGA_12 ? 480u : 200u;
}

inline std::uint32_t strideBytes() noexcept {
    return fbWidth() / 8u;
}

inline void bindGuest(std::uint8_t* ram) noexcept { guestRamPtr = ram; }

inline void writeMeta() noexcept {
    if (!guestRamPtr) return;
    const std::uint32_t w = fbWidth();
    const std::uint32_t h = fbHeight();
    guestRamPtr[META_BYTE + 0u] = static_cast<std::uint8_t>(w & 0xFFu);
    guestRamPtr[META_BYTE + 1u] = static_cast<std::uint8_t>((w >> 8) & 0xFFu);
    guestRamPtr[META_BYTE + 2u] = static_cast<std::uint8_t>(h & 0xFFu);
    guestRamPtr[META_BYTE + 3u] = static_cast<std::uint8_t>((h >> 8) & 0xFFu);
    guestRamPtr[META_BYTE + 4u] = 4u;
}

inline void compositePixel(std::uint32_t x, std::uint32_t y) noexcept {
    if (!guestRamPtr || x >= fbWidth() || y >= fbHeight()) return;
    const std::uint32_t byteOff = y * strideBytes() + (x >> 3);
    const std::uint8_t bit = static_cast<std::uint8_t>(0x80u >> (x & 7u));
    std::uint8_t color = 0;
    for (int p = 0; p < 4; ++p)
        if (planes[p][byteOff] & bit)
            color = static_cast<std::uint8_t>(color | (1u << p));
    const std::uint32_t lin = FieldVga::VGA_FB + y * fbWidth() + x;
    guestRamPtr[lin] = color;
}

inline void compositeAll() noexcept {
    const std::uint32_t w = fbWidth();
    const std::uint32_t h = fbHeight();
    for (std::uint32_t y = 0; y < h; ++y)
        for (std::uint32_t x = 0; x < w; ++x)
            compositePixel(x, y);
    writeMeta();
}

inline void compositeRange(std::uint32_t off, std::uint32_t len) noexcept {
    const std::uint32_t w = fbWidth();
    const std::uint32_t h = fbHeight();
    const std::uint32_t stride = strideBytes();
    const std::uint32_t y0 = off / stride;
    const std::uint32_t y1 = std::min(h, (off + len) / stride + 1u);
    for (std::uint32_t y = y0; y < y1; ++y) {
        const std::uint32_t x0 = (y == y0) ? ((off % stride) * 8u) : 0u;
        const std::uint32_t x1 = (y == y1 - 1 && y1 > y0) ? w : w;
        for (std::uint32_t x = x0; x < x1; ++x)
            compositePixel(x, y);
    }
    writeMeta();
}

inline void planeWrite(std::uint32_t off, std::uint8_t val) noexcept {
    const std::uint8_t mapMask = seq[2];
    const std::uint8_t writeMode = gc[5] & 3u;
    const std::uint8_t bitMask = gc[8];
    const std::uint8_t setReset = gc[0];
    const std::uint8_t enableSR = gc[1];

    if (writeMode == 2u) {
        const std::uint8_t color = gc[5] & 0x0Fu;
        for (int p = 0; p < 4; ++p) {
            if (!(mapMask & (1u << p))) continue;
            const std::uint8_t bit = static_cast<std::uint8_t>((color >> p) & 1u) ? 0xFFu : 0x00u;
            planes[p][off] = static_cast<std::uint8_t>((planes[p][off] & ~bitMask) | (bit & bitMask));
        }
    } else if (writeMode == 0u) {
        for (int p = 0; p < 4; ++p) {
            if (!(mapMask & (1u << p))) continue;
            const std::uint8_t sr = (enableSR & (1u << p)) ? setReset : val;
            planes[p][off] = static_cast<std::uint8_t>((planes[p][off] & ~bitMask) | (sr & bitMask));
        }
    } else if (writeMode == 3u) {
        const std::uint8_t rotate = gc[3] & 7u;
        std::uint8_t r = val;
        if (rotate) r = static_cast<std::uint8_t>((r >> rotate) | (r << (8 - rotate)));
        for (int p = 0; p < 4; ++p) {
            if (!(mapMask & (1u << p))) continue;
            const std::uint8_t sr = (enableSR & (1u << p)) ? setReset : r;
            planes[p][off] = static_cast<std::uint8_t>((planes[p][off] & ~bitMask) | (sr & bitMask));
        }
    } else {
        for (int p = 0; p < 4; ++p) {
            if (!(mapMask & (1u << p))) continue;
            planes[p][off] = val;
        }
    }
    compositeRange(off, 1u);
}

inline std::uint8_t planeRead(std::uint32_t off) noexcept {
    const std::uint8_t readMode = gc[5] & 3u;
    if (readMode == 0u)
        return planes[readPlane & 3u][off];
    if (readMode == 1u) {
        std::uint8_t out = 0;
        for (int p = 0; p < 4; ++p)
            if (planes[p][off] & latch[p])
                out = static_cast<std::uint8_t>(out | (1u << p));
        return out;
    }
    return planes[readPlane & 3u][off];
}

inline void memWrite(std::uint32_t addr, std::uint8_t val) noexcept {
    const std::uint32_t off = (addr - FieldVga::VGA_FB) & (PLANE_BYTES - 1u);
    planeWrite(off, val);
}

inline std::uint8_t memRead(std::uint32_t addr) noexcept {
    const std::uint32_t off = (addr - FieldVga::VGA_FB) & (PLANE_BYTES - 1u);
    for (int p = 0; p < 4; ++p)
        latch[p] = planes[p][off];
    return planeRead(off);
}

inline void applyModeDefaults(std::uint8_t mode) noexcept {
    std::memset(seq, 0, sizeof seq);
    std::memset(gc, 0, sizeof gc);
    seq[0] = 0x03u;
    seq[1] = 0x01u;
    seq[2] = 0x0Fu;
    seq[3] = 0x00u;
    gc[4] = 0x00u;
    gc[5] = 0x00u;
    gc[6] = 0x05u;
    gc[8] = 0xFFu;
    readPlane = 0;
    if (mode == FieldVga::MODE_EGA_0D || mode == FieldVga::MODE_EGA_12) {
        std::memset(planes, 0, sizeof planes);
        compositeAll();
    }
}

inline void reset() noexcept {
    seqIndex = 0;
    gcIndex = 0;
    attrFlip = 0;
    std::memset(seq, 0, sizeof seq);
    std::memset(gc, 0, sizeof gc);
    std::memset(planes, 0, sizeof planes);
    applyModeDefaults(FieldVga::mode);
}

inline void portOut(std::uint16_t port, std::uint8_t val) noexcept {
    if (port == SEQ_INDEX) {
        seqIndex = static_cast<std::uint8_t>(val & 7u);
        return;
    }
    if (port == SEQ_DATA && seqIndex < 4u) {
        seq[seqIndex] = val;
        return;
    }
    if (port == GC_INDEX) {
        gcIndex = static_cast<std::uint8_t>(val & 0x0Fu);
        return;
    }
    if (port == GC_DATA && gcIndex < 9u) {
        gc[gcIndex] = val;
        if (gcIndex == 4u)
            readPlane = static_cast<std::uint8_t>(val & 3u);
        return;
    }
    if (port == ATTR_TOGGLE) {
        attrFlip ^= 1u;
        (void)val;
        return;
    }
}

inline std::uint8_t portIn(std::uint16_t port) noexcept {
    if (port == SEQ_INDEX) return seqIndex;
    if (port == SEQ_DATA && seqIndex < 4u) return seq[seqIndex];
    if (port == GC_INDEX) return gcIndex;
    if (port == GC_DATA && gcIndex < 9u) return gc[gcIndex];
    if (port == ATTR_STATUS) return 0x09u;
    return 0xFFu;
}

inline bool handlesPort(std::uint16_t port) noexcept {
    return port == SEQ_INDEX || port == SEQ_DATA
        || port == GC_INDEX || port == GC_DATA
        || port == ATTR_TOGGLE || port == ATTR_STATUS;
}

} // namespace FieldVgaPlanes