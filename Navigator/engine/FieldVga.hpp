#pragma once

// VGA / CGA / EGA / Voodoo mode tracking + DAC palette for guest DOS.

#include "FieldPlatform.hpp"

#include <cstdint>
#include <cstring>

namespace FieldVgaCrtc { inline void onVgaModeChange(std::uint8_t mode) noexcept; }

namespace FieldVga {

constexpr std::uint8_t MODE_TEXT_80  = 3u;
constexpr std::uint8_t MODE_CGA_4    = 4u;
constexpr std::uint8_t MODE_CGA_5    = 5u;
constexpr std::uint8_t MODE_CGA_BW   = 6u;
constexpr std::uint8_t MODE_EGA_0D   = 0x0Du;
constexpr std::uint8_t MODE_EGA_12   = 0x12u;
constexpr std::uint8_t MODE_VGA_13   = 0x13u;
constexpr std::uint8_t MODE_MCGA_13  = 0x13u;
constexpr std::uint8_t MODE_VOODOO   = 0xA0u;
constexpr std::uint8_t MODE_SVGA     = 0xFEu;

constexpr std::uint32_t VGA_PAL_RAM  = 0x00098000u;
constexpr std::uint32_t VGA_FB       = 0x000A0000u;

inline std::uint8_t mode = MODE_TEXT_80;
inline std::uint8_t dacIndex = 0u;
inline std::uint8_t dacComp = 0u;
inline std::uint8_t palette[256u * 3u]{};

inline void resetPalette() noexcept {
    static const std::uint8_t kClassic[256u * 3u] = {
        0,0,0, 31,0,0, 0,31,0, 31,31,0, 0,0,31, 31,0,31, 0,31,31, 31,31,31,
        63,0,0, 95,0,0, 127,0,0, 159,0,0, 191,0,0, 223,0,0, 255,0,0, 63,31,0,
        95,31,0, 127,31,0, 159,31,0, 191,31,0, 223,31,0, 255,31,0, 63,63,0,
        95,63,0, 127,63,0, 159,63,0, 191,63,0, 223,63,0, 255,63,0, 0,95,0,
        31,95,0, 63,95,0, 95,95,0, 127,95,0, 159,95,0, 191,95,0, 223,95,0,
        255,95,0, 0,127,0, 31,127,0, 63,127,0, 95,127,0, 127,127,0, 159,127,0,
        191,127,0, 223,127,0, 255,127,0, 0,159,0, 31,159,0, 63,159,0, 95,159,0,
        127,159,0, 159,159,0, 191,159,0, 223,159,0, 255,159,0, 0,191,0, 31,191,0,
        63,191,0, 95,191,0, 127,191,0, 159,191,0, 191,191,0, 223,191,0, 255,191,0,
        0,223,0, 31,223,0, 63,223,0, 95,223,0, 127,223,0, 159,223,0, 191,223,0,
        223,223,0, 255,223,0, 0,255,0, 31,255,0, 63,255,0, 95,255,0, 127,255,0,
        159,255,0, 191,255,0, 223,255,0, 255,255,0, 63,0,31, 95,0,31, 127,0,31,
        159,0,31, 191,0,31, 223,0,31, 255,0,31, 63,31,31, 95,31,31, 127,31,31,
        159,31,31, 191,31,31, 223,31,31, 255,31,31, 63,63,31, 95,63,31, 127,63,31,
        159,63,31, 191,63,31, 223,63,31, 255,63,31, 0,95,31, 31,95,31, 63,95,31,
        95,95,31, 127,95,31, 159,95,31, 191,95,31, 223,95,31, 255,95,31, 0,127,31,
        31,127,31, 63,127,31, 95,127,31, 127,127,31, 159,127,31, 191,127,31, 223,127,31,
        255,127,31, 0,159,31, 31,159,31, 63,159,31, 95,159,31, 127,159,31, 159,159,31,
        191,159,31, 223,159,31, 255,159,31, 0,191,31, 31,191,31, 63,191,31, 95,191,31,
        127,191,31, 159,191,31, 191,191,31, 223,191,31, 255,191,31, 0,223,31, 31,223,31,
        63,223,31, 95,223,31, 127,223,31, 159,223,31, 191,223,31, 223,223,31, 255,223,31,
        0,255,31, 31,255,31, 63,255,31, 95,255,31, 127,255,31, 159,255,31, 191,255,31,
        223,255,31, 255,255,31, 0,0,63, 31,0,63, 63,0,63, 95,0,63, 127,0,63, 159,0,63,
        191,0,63, 223,0,63, 255,0,63, 0,31,63, 31,31,63, 63,31,63, 95,31,63, 127,31,63,
        159,31,63, 191,31,63, 223,31,63, 255,31,63, 0,63,63, 31,63,63, 63,63,63, 95,63,63,
        127,63,63, 159,63,63, 191,63,63, 223,63,63, 255,63,63, 127,127,127, 191,191,191,
        223,223,223, 255,255,255
    };
    std::memcpy(palette, kClassic, sizeof palette);
}

inline void syncPaletteToGuest(std::uint8_t* guestRam) noexcept {
    if (!guestRam) return;
    std::memcpy(guestRam + VGA_PAL_RAM, palette, sizeof palette);
}

inline bool isGraphicsMode(std::uint8_t m) noexcept {
    return m == MODE_CGA_4 || m == MODE_CGA_5 || m == MODE_CGA_BW
        || m == MODE_EGA_0D || m == MODE_EGA_12
        || m == MODE_VGA_13 || m == MODE_VOODOO || m == MODE_SVGA;
}

inline bool isPlanarMode(std::uint8_t m) noexcept {
    return m == MODE_EGA_0D || m == MODE_EGA_12;
}

inline void paintModeDemo(std::uint8_t* ram, std::uint8_t m) noexcept {
    if (!ram) return;
    if (m == MODE_CGA_4 || m == MODE_CGA_5) {
        for (std::uint32_t y = 0; y < 200u; ++y)
            for (std::uint32_t x = 0; x < 80u; ++x) {
                const std::uint8_t c0 = static_cast<std::uint8_t>((x / 10u + y / 25u) & 3u);
                const std::uint8_t c1 = static_cast<std::uint8_t>((c0 + 1u) & 3u);
                ram[VGA_FB + y * 80u + x] = static_cast<std::uint8_t>((c0 << 4) | c1);
            }
        return;
    }
    if (m == MODE_CGA_BW) {
        for (std::uint32_t y = 0; y < 200u; ++y)
            for (std::uint32_t x = 0; x < 80u; ++x)
                ram[VGA_FB + y * 80u + x] = static_cast<std::uint8_t>(
                    ((x + y) & 4u) ? 0xFFu : 0x00u);
        return;
    }
    if (m == MODE_EGA_0D) {
        for (std::uint32_t y = 0; y < 200u; ++y)
            for (std::uint32_t x = 0; x < 320u; ++x)
                ram[VGA_FB + y * 320u + x] = static_cast<std::uint8_t>(
                    ((x / 20u) + (y / 25u)) & 15u);
        return;
    }
    if (m == MODE_EGA_12) {
        for (std::uint32_t y = 0; y < 480u; ++y)
            for (std::uint32_t x = 0; x < 640u; ++x)
                ram[VGA_FB + y * 640u + x] = static_cast<std::uint8_t>(
                    ((x / 40u) + (y / 30u)) & 15u);
        return;
    }
    if (m == MODE_VGA_13 || m == MODE_VOODOO) {
        std::memset(ram + VGA_FB, 0, 320u * 200u);
        for (std::uint32_t y = 0; y < 200u; ++y)
            for (std::uint32_t x = 0; x < 320u; ++x) {
                const std::uint8_t idx = static_cast<std::uint8_t>(
                    ((x ^ y) >> 2) + (x >> 4) + 32u);
                ram[VGA_FB + y * 320u + x] = idx;
            }
        return;
    }
}

inline void setMode(std::uint8_t m, std::uint8_t* guestRam) noexcept {
    mode = m;
    FieldVgaCrtc::onVgaModeChange(m);
    if (!guestRam) return;
    guestRam[0x449u] = m;
    if (m == MODE_TEXT_80) {
        guestRam[0x44Au] = 80u;
        return;
    }
    if (isGraphicsMode(m)) {
        guestRam[0x44Au] = 80u;
        resetPalette();
        syncPaletteToGuest(guestRam);
        if (m == MODE_EGA_12)
            std::memset(guestRam + VGA_FB, 0, 640u * 480u);
        else if (m == MODE_EGA_0D)
            std::memset(guestRam + VGA_FB, 0, 320u * 200u);
        else if (m == MODE_CGA_4 || m == MODE_CGA_5 || m == MODE_CGA_BW)
            std::memset(guestRam + VGA_FB, 0, 80u * 200u);
        else if (m == MODE_SVGA) { /* LFB cleared by FieldVesa */ }
        else
            std::memset(guestRam + VGA_FB, 0, 320u * 200u);
    }
}

inline void portOut(std::uint16_t port, std::uint8_t val) noexcept {
    if (port == 0x3C8u) {
        dacIndex = val;
        dacComp = 0u;
        return;
    }
    if (port == 0x3C9u) {
        const std::size_t off = static_cast<std::size_t>(dacIndex) * 3u + dacComp;
        if (off + 1u < sizeof palette)
            palette[off] = static_cast<std::uint8_t>((val & 63u) << 2);
        dacComp = static_cast<std::uint8_t>((dacComp + 1u) % 3u);
        if (dacComp == 0u) ++dacIndex;
        return;
    }
    if (port == 0x3C6u && val == 0xFFu)
        resetPalette();
}

inline void init(std::uint8_t* guestRam) noexcept {
    resetPalette();
    setMode(MODE_TEXT_80, guestRam);
}

} // namespace FieldVga

#include "FieldVgaCrtc.hpp"
#include "FieldVgaPlanes.hpp"