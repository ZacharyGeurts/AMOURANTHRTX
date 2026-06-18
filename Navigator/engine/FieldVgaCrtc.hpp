#pragma once

// VGA CRTC — color ports 0x3D4/0x3D5, mode 13h framebuffer at 0xA0000.

#include "FieldVga.hpp"

#include <cstdint>
#include <cstring>

namespace FieldVgaCrtc {

constexpr std::uint16_t PORT_INDEX = 0x3D4u;
constexpr std::uint16_t PORT_DATA  = 0x3D5u;

constexpr std::uint8_t REG_HTOTAL        = 0x00u;
constexpr std::uint8_t REG_HDISP_END    = 0x01u;
constexpr std::uint8_t REG_MAX_SCAN     = 0x09u;
constexpr std::uint8_t REG_CURSOR_START = 0x0Au;
constexpr std::uint8_t REG_CURSOR_END   = 0x0Bu;
constexpr std::uint8_t REG_START_HI     = 0x0Cu;
constexpr std::uint8_t REG_START_LO     = 0x0Du;
constexpr std::uint8_t REG_CURSOR_HI    = 0x0Eu;
constexpr std::uint8_t REG_CURSOR_LO    = 0x0Fu;
constexpr std::uint8_t REG_OFFSET_HI    = 0x10u;
constexpr std::uint8_t REG_OFFSET_LO    = 0x13u;
constexpr std::uint8_t REG_UNDERLINE    = 0x14u;
constexpr std::uint8_t REG_MODE_CTRL    = 0x17u;
constexpr std::uint8_t REG_LINE_COMPARE = 0x18u;

inline std::uint8_t index = 0;
inline std::uint8_t regs[64]{};
inline std::uint16_t cursorPos = 0;
inline std::uint32_t startAddr = FieldVga::VGA_FB;
inline std::uint8_t* guestRamPtr = nullptr;

inline void bindGuest(std::uint8_t* ram) noexcept {
    guestRamPtr = ram;
}

inline void syncCursorToBda() noexcept {
    if (!guestRamPtr) return;
    guestRamPtr[0x450u] = static_cast<std::uint8_t>(cursorPos & 0xFFu);
    guestRamPtr[0x451u] = static_cast<std::uint8_t>((cursorPos >> 8) & 0xFFu);
}

inline void updateStartFromRegs() noexcept {
    const std::uint32_t off = static_cast<std::uint32_t>(regs[REG_START_LO])
                            | (static_cast<std::uint32_t>(regs[REG_START_HI] & 0x3Fu) << 8);
    startAddr = FieldVga::VGA_FB + off;
}

inline void updateCursorFromRegs() noexcept {
    cursorPos = static_cast<std::uint16_t>(regs[REG_CURSOR_LO])
              | (static_cast<std::uint16_t>(regs[REG_CURSOR_HI] & 0x3Fu) << 8);
    syncCursorToBda();
}

inline void applyText80() noexcept {
    regs[REG_HTOTAL]     = 0x5Fu;
    regs[REG_HDISP_END]  = 0x4Fu;
    regs[REG_MAX_SCAN]   = 0x0Fu;
    regs[REG_CURSOR_START] = 0x0Eu;
    regs[REG_CURSOR_END] = 0x0Fu;
    regs[REG_START_HI]   = 0x00u;
    regs[REG_START_LO]   = 0x00u;
    regs[REG_CURSOR_HI]  = 0x00u;
    regs[REG_CURSOR_LO]  = 0x00u;
    regs[REG_OFFSET_LO]  = 0x50u;
    regs[REG_OFFSET_HI]  = 0x00u;
    regs[REG_UNDERLINE]  = 0x00u;
    regs[REG_MODE_CTRL]  = 0xE3u;
    regs[REG_LINE_COMPARE] = 0xFFu;
    startAddr = FieldVga::VGA_FB;
    cursorPos = 0;
    syncCursorToBda();
}

inline void applyMode0Dh() noexcept {
    regs[REG_HTOTAL]     = 0x2Du;
    regs[REG_HDISP_END]  = 0x27u;
    regs[REG_MAX_SCAN]   = 0x0Bu;
    regs[REG_CURSOR_START] = 0x20u;
    regs[REG_CURSOR_END] = 0x00u;
    regs[REG_START_HI]   = 0x00u;
    regs[REG_START_LO]   = 0x00u;
    regs[REG_CURSOR_HI]  = 0x00u;
    regs[REG_CURSOR_LO]  = 0x00u;
    regs[REG_OFFSET_LO]  = 0x14u;
    regs[REG_OFFSET_HI]  = 0x00u;
    regs[REG_UNDERLINE]  = 0x00u;
    regs[REG_MODE_CTRL]  = 0xE3u;
    regs[REG_LINE_COMPARE] = 0xFFu;
    startAddr = FieldVga::VGA_FB;
    cursorPos = 0;
    syncCursorToBda();
}

inline void applyMode12h() noexcept {
    regs[REG_HTOTAL]     = 0x5Fu;
    regs[REG_HDISP_END]  = 0x4Fu;
    regs[REG_MAX_SCAN]   = 0x40u;
    regs[REG_CURSOR_START] = 0x20u;
    regs[REG_CURSOR_END] = 0x00u;
    regs[REG_START_HI]   = 0x00u;
    regs[REG_START_LO]   = 0x00u;
    regs[REG_CURSOR_HI]  = 0x00u;
    regs[REG_CURSOR_LO]  = 0x00u;
    regs[REG_OFFSET_LO]  = 0x50u;
    regs[REG_OFFSET_HI]  = 0x00u;
    regs[REG_UNDERLINE]  = 0x00u;
    regs[REG_MODE_CTRL]  = 0xE3u;
    regs[REG_LINE_COMPARE] = 0xFFu;
    startAddr = FieldVga::VGA_FB;
    cursorPos = 0;
    syncCursorToBda();
}

inline void applyMode13h() noexcept {
    regs[REG_HTOTAL]     = 0x5Fu;
    regs[REG_HDISP_END]  = 0x4Fu;
    regs[REG_MAX_SCAN]   = 0x41u;
    regs[REG_CURSOR_START] = 0x20u; /* cursor disabled in graphics */
    regs[REG_CURSOR_END] = 0x00u;
    regs[REG_START_HI]   = 0x00u;
    regs[REG_START_LO]   = 0x00u;
    regs[REG_CURSOR_HI]  = 0x00u;
    regs[REG_CURSOR_LO]  = 0x00u;
    regs[REG_OFFSET_LO]  = 0x28u;
    regs[REG_OFFSET_HI]  = 0x00u;
    regs[REG_UNDERLINE]  = 0x00u;
    regs[REG_MODE_CTRL]  = 0xE3u;
    regs[REG_LINE_COMPARE] = 0xFFu;
    startAddr = FieldVga::VGA_FB;
    cursorPos = 0;
    syncCursorToBda();
}

inline void onVgaModeChange(std::uint8_t mode) noexcept {
    if (mode == FieldVga::MODE_VGA_13 || mode == FieldVga::MODE_VOODOO || mode == FieldVga::MODE_SVGA)
        applyMode13h();
    else if (mode == FieldVga::MODE_EGA_0D)
        applyMode0Dh();
    else if (mode == FieldVga::MODE_EGA_12)
        applyMode12h();
    else if (mode == FieldVga::MODE_CGA_4 || mode == FieldVga::MODE_CGA_5 || mode == FieldVga::MODE_CGA_BW)
        applyMode13h();
    else if (mode == FieldVga::MODE_TEXT_80)
        applyText80();
    else
        applyText80();
}

inline void reset() noexcept {
    index = 0;
    std::memset(regs, 0, sizeof regs);
    applyText80();
    onVgaModeChange(FieldVga::mode);
}

inline void portOut(std::uint16_t port, std::uint8_t val) noexcept {
    if (port == PORT_INDEX) {
        index = static_cast<std::uint8_t>(val & 0x1Fu);
        return;
    }
    if (port == PORT_DATA) {
        if (index >= sizeof regs) return;
        regs[index] = val;
        if (index == REG_START_HI || index == REG_START_LO)
            updateStartFromRegs();
        if (index == REG_CURSOR_HI || index == REG_CURSOR_LO)
            updateCursorFromRegs();
        return;
    }
}

inline std::uint8_t portIn(std::uint16_t port) noexcept {
    if (port == PORT_INDEX)
        return index;
    if (port == PORT_DATA) {
        if (index >= sizeof regs) return 0x00u;
        return regs[index];
    }
    return 0xFFu;
}

inline bool handlesPort(std::uint16_t port) noexcept {
    return port == PORT_INDEX || port == PORT_DATA;
}

} // namespace FieldVgaCrtc