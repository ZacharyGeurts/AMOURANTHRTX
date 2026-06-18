#pragma once

// VESA BIOS extensions — mode info, bank switching, linear framebuffer.

#include "FieldVga.hpp"
#include "FieldVgaPlanes.hpp"

#include <cstdint>
#include <cstring>

namespace FieldVesa {

constexpr std::uint32_t SIGNATURE = 0x41534556u; /* 'VESA' little-endian */
constexpr std::uint32_t INFO_OFF  = 0x00020000u;

inline bool active = false;
inline std::uint16_t mode = 0;
inline std::uint16_t width = 640u;
inline std::uint16_t height = 480u;
inline std::uint16_t bpp = 8u;
inline std::uint16_t pitch = 640u;
inline std::uint16_t bank = 0u;
inline std::uint16_t bankSize = 64u;
inline std::uint32_t lfbAddr = FieldVgaPlanes::LFB_BASE;

inline void writeInfo(std::uint8_t* ram, std::uint16_t vesaMode) noexcept {
    if (!ram) return;
    const std::uint32_t base = INFO_OFF;
    std::memset(ram + base, 0, 256u);
    ram[base + 0u] = static_cast<std::uint8_t>(SIGNATURE);
    ram[base + 1u] = static_cast<std::uint8_t>(SIGNATURE >> 8);
    ram[base + 2u] = static_cast<std::uint8_t>(SIGNATURE >> 16);
    ram[base + 3u] = static_cast<std::uint8_t>(SIGNATURE >> 24);
    ram[base + 4u] = 0x02u;
    ram[base + 5u] = 0x00u;
    ram[base + 14u] = 0x01u;
    ram[base + 18u] = static_cast<std::uint8_t>(width & 0xFFu);
    ram[base + 19u] = static_cast<std::uint8_t>((width >> 8) & 0xFFu);
    ram[base + 20u] = static_cast<std::uint8_t>(height & 0xFFu);
    ram[base + 21u] = static_cast<std::uint8_t>((height >> 8) & 0xFFu);
    ram[base + 25u] = static_cast<std::uint8_t>(bpp);
    ram[base + 39u] = 0x01u;
    ram[base + 40u] = static_cast<std::uint8_t>(lfbAddr & 0xFFu);
    ram[base + 41u] = static_cast<std::uint8_t>((lfbAddr >> 8) & 0xFFu);
    ram[base + 42u] = static_cast<std::uint8_t>((lfbAddr >> 16) & 0xFFu);
    ram[base + 43u] = static_cast<std::uint8_t>((lfbAddr >> 24) & 0xFFu);
    ram[base + 44u] = static_cast<std::uint8_t>(vesaMode & 0xFFu);
    ram[base + 45u] = static_cast<std::uint8_t>((vesaMode >> 8) & 0xFFu);
}

inline void syncMeta(std::uint8_t* ram) noexcept {
    if (!ram) return;
    ram[FieldVgaPlanes::META_BYTE + 0u] = static_cast<std::uint8_t>(width & 0xFFu);
    ram[FieldVgaPlanes::META_BYTE + 1u] = static_cast<std::uint8_t>((width >> 8) & 0xFFu);
    ram[FieldVgaPlanes::META_BYTE + 2u] = static_cast<std::uint8_t>(height & 0xFFu);
    ram[FieldVgaPlanes::META_BYTE + 3u] = static_cast<std::uint8_t>((height >> 8) & 0xFFu);
    ram[FieldVgaPlanes::META_BYTE + 4u] = static_cast<std::uint8_t>(bpp);
    ram[FieldVgaPlanes::META_BYTE + 5u] = static_cast<std::uint8_t>(bank & 0xFFu);
    ram[FieldVgaPlanes::META_BYTE + 6u] = static_cast<std::uint8_t>((bank >> 8) & 0xFFu);
    ram[FieldVgaPlanes::META_BYTE + 8u] = static_cast<std::uint8_t>(lfbAddr & 0xFFu);
    ram[FieldVgaPlanes::META_BYTE + 9u] = static_cast<std::uint8_t>((lfbAddr >> 8) & 0xFFu);
    ram[FieldVgaPlanes::META_BYTE + 10u] = static_cast<std::uint8_t>((lfbAddr >> 16) & 0xFFu);
    ram[FieldVgaPlanes::META_BYTE + 11u] = static_cast<std::uint8_t>((lfbAddr >> 24) & 0xFFu);
}

inline bool setVesaMode(std::uint16_t vesaMode, std::uint8_t* ram) noexcept {
    std::uint16_t w = 640u, h = 480u, b = 8u, p = 640u;
    std::uint8_t vgaMode = FieldVga::MODE_VGA_13;
    switch (vesaMode) {
    case 0x100u: w = 640u; h = 400u; b = 8u; p = 640u; vgaMode = FieldVga::MODE_VGA_13; break;
    case 0x101u: w = 640u; h = 480u; b = 8u; p = 640u; vgaMode = FieldVga::MODE_VGA_13; break;
    case 0x102u: w = 800u; h = 600u; b = 4u; p = 800u; vgaMode = FieldVga::MODE_SVGA; break;
    case 0x103u: w = 800u; h = 600u; b = 8u; p = 800u; vgaMode = FieldVga::MODE_SVGA; break;
    case 0x111u: w = 640u; h = 480u; b = 4u; p = 640u; vgaMode = FieldVga::MODE_EGA_12; break;
    case 0x112u: w = 640u; h = 480u; b = 8u; p = 640u; vgaMode = FieldVga::MODE_VGA_13; break;
    default: return false;
    }
    active = true;
    mode = vesaMode;
    width = w;
    height = h;
    bpp = b;
    pitch = p;
    bank = 0u;
    FieldVga::setMode(vgaMode, ram);
    if (ram) {
        if (vgaMode == FieldVga::MODE_SVGA)
            std::memset(ram + lfbAddr, 0, static_cast<std::size_t>(p) * h);
        writeInfo(ram, vesaMode);
        syncMeta(ram);
    }
    return true;
}

inline bool setBank(std::uint16_t b, std::uint8_t* ram) noexcept {
    if (!active) return false;
    bank = b;
    syncMeta(ram);
    return true;
}

inline std::uint32_t sampleOffset(std::uint32_t x, std::uint32_t y) noexcept {
    if (!active || bpp <= 4u) return 0u;
    if (bpp == 8u)
        return lfbAddr + static_cast<std::uint32_t>(y) * pitch + x;
    return lfbAddr + static_cast<std::uint32_t>(y) * pitch + (x >> 1);
}

inline void reset() noexcept {
    active = false;
    mode = 0;
    width = 640u;
    height = 480u;
    bpp = 8u;
    pitch = 640u;
    bank = 0u;
}

} // namespace FieldVesa