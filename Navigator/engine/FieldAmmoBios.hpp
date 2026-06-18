#pragma once

// AmmoBIOS — CMOS setup utility with mouse + keyboard navigation.

#include "FieldCmos.hpp"
#include "FieldDos.hpp"
#include "FieldPlatform.hpp"
#include "FieldRtxMouse.hpp"

#include <cstdio>
#include <cstring>

namespace FieldAmmoBios {

constexpr std::uint32_t VGA = 0x000B8000u;
constexpr int ROWS = 25;
constexpr int COLS = 80;

inline bool active = false;
inline int  hoverRow = -1;
inline int  focusRow = 0;

struct MenuItem {
    const char* label;
    int         cmosReg;
    int         minVal;
    int         maxVal;
    bool        bcd;
};

inline void putCell(std::uint8_t* ram, int row, int col, char ch, std::uint8_t attr) noexcept {
    if (!ram || row < 0 || row >= ROWS || col < 0 || col >= COLS) return;
    const std::uint32_t off = VGA + static_cast<std::uint32_t>((row * COLS + col) * 2);
    ram[off] = static_cast<std::uint8_t>(ch);
    ram[off + 1u] = attr;
}

inline void fill(std::uint8_t* ram, int row, char ch, std::uint8_t attr, int len = COLS) noexcept {
    for (int c = 0; c < len && c < COLS; ++c)
        putCell(ram, row, c, ch, attr);
}

inline void print(std::uint8_t* ram, int row, int col, const char* text, std::uint8_t attr) noexcept {
    if (!text) return;
    for (int i = 0; text[i] && col + i < COLS; ++i)
        putCell(ram, row, col + i, text[i], attr);
}

inline int regVal(int reg) noexcept {
    if (reg < 0 || reg >= static_cast<int>(sizeof FieldCmos::regs)) return 0;
    const std::uint8_t v = FieldCmos::regs[static_cast<std::size_t>(reg)];
    return FieldCmos::fromBcd(v);
}

inline void setReg(int reg, int val) noexcept {
    if (reg < 0 || reg >= static_cast<int>(sizeof FieldCmos::regs)) return;
    FieldCmos::regs[static_cast<std::size_t>(reg)] = FieldCmos::toBcd(val);
}

inline const MenuItem* menuItems(int& count) noexcept {
    static const MenuItem k[] = {
        { "Boot device (1=A 2=C)", FieldCmos::REG_BOOT, 1, 2, false },
        { "RTC hour",              FieldCmos::REG_HOUR, 0, 23, true },
        { "RTC minute",            FieldCmos::REG_MIN,  0, 59, true },
        { "RTC second",            FieldCmos::REG_SEC,  0, 59, true },
        { "RTC day",               FieldCmos::REG_DAY,  1, 31, true },
        { "RTC month",             FieldCmos::REG_MONTH,1, 12, true },
        { "RTC year (00-99)",      FieldCmos::REG_YEAR, 0, 99, true },
    };
    count = static_cast<int>(sizeof k / sizeof k[0]);
    return k;
}

inline void paint(std::uint8_t* ram) noexcept {
    if (!ram) return;
    for (int r = 0; r < ROWS; ++r)
        fill(ram, r, ' ', 0x07);

    print(ram, 0, 2, " AmmoBIOS Setup v1.0 - RTX-AMMOS CMOS / NVRAM", 0x1F);
    print(ram, 1, 2, " Arrow keys move | Enter edit | +/- adjust | Esc exit", 0x17);
    print(ram, 2, 2, " Mouse: click row to select, left click value to increment", 0x17);

    int n = 0;
    const MenuItem* items = menuItems(n);
    for (int i = 0; i < n; ++i) {
        const int row = 4 + i;
        const bool sel = (i == focusRow);
        const bool hov = (i == hoverRow);
        std::uint8_t attr = sel ? 0x1E : (hov ? 0x70 : 0x07);
        if (sel) attr = 0x1E;
        print(ram, row, 4, items[i].label, attr);
        char buf[16];
        std::snprintf(buf, sizeof buf, "%3d", regVal(items[i].cmosReg));
        print(ram, row, 48, buf, sel ? 0x9E : 0x0B);
    }

    char ext[64];
    const std::uint32_t extKb = FieldPlatform::EXTENDED_KB;
    std::snprintf(ext, sizeof ext, " Extended RAM: %u MiB | HD: %s | Floppy: A:",
        extKb / 1024u, FieldDos::hdReady ? "C: ready" : "none");
    print(ram, 14, 4, ext, 0x3B);

    char equip[64];
    std::snprintf(equip, sizeof equip, " CMOS equip=0x%02X floppy=0x%02X statD=0x%02X",
        FieldCmos::regs[FieldCmos::REG_EQUIP],
        FieldCmos::regs[FieldCmos::REG_FLOPPY],
        FieldCmos::regs[FieldCmos::REG_STAT_D]);
    print(ram, 15, 4, equip, 0x3B);

    print(ram, 22, 4, " F10 Save & exit | Esc cancel", 0x17);
}

inline void adjust(int delta) noexcept {
    int n = 0;
    const MenuItem* items = menuItems(n);
    if (focusRow < 0 || focusRow >= n) return;
    const MenuItem& it = items[focusRow];
    int v = regVal(it.cmosReg) + delta;
    if (v < it.minVal) v = it.maxVal;
    if (v > it.maxVal) v = it.minVal;
    setReg(it.cmosReg, v);
}

inline bool hitTest(int col, int row, int& outRow) noexcept {
    int n = 0;
    menuItems(n);
    if (row < 4 || row >= 4 + n || col < 4 || col >= 76) return false;
    outRow = row - 4;
    return true;
}

inline bool handleKey(std::uint16_t key, std::uint8_t* ram) noexcept {
    if (!active || !ram) return false;
    const std::uint16_t sc = static_cast<std::uint16_t>(key & 0xFFu);
    int n = 0;
    menuItems(n);

    if (sc == 0x01u) { active = false; return true; }
    if (sc == 0x48u) { focusRow = (focusRow + n - 1) % n; paint(ram); return true; }
    if (sc == 0x50u) { focusRow = (focusRow + 1) % n; paint(ram); return true; }
    if (sc == 0x4Bu || sc == 0x2Du) { adjust(-1); paint(ram); return true; }
    if (sc == 0x4Du || sc == 0x2Bu || sc == 0x0Du) { adjust(+1); paint(ram); return true; }
    if (sc == 0x44u) { active = false; return true; }
    return true;
}

inline bool handleMouse(std::uint8_t* ram) noexcept {
    if (!active || !ram) return false;
    const auto f = FieldRtxMouse::capture();
    if (!f.visible) return false;
    int row = -1;
    if (hitTest(f.col, f.row, row))
        hoverRow = row;
    else
        hoverRow = -1;
    if (f.leftClick && hoverRow >= 0) {
        focusRow = hoverRow;
        adjust(+1);
    }
    if (hoverRow >= 0 || f.leftClick)
        paint(ram);
    FieldRtxMouse::paintPointer(ram, f.col, f.row, 0x71);
    return true;
}

inline void open(std::uint8_t* ram) noexcept {
    FieldCmos::init(FieldDos::hdReady);
    if (FieldDos::hdReady)
        FieldCmos::regs[FieldCmos::REG_BOOT] = 0x02u;
    active = true;
    focusRow = 0;
    hoverRow = -1;
    paint(ram);
}

inline void close() noexcept {
    active = false;
    hoverRow = -1;
}

} // namespace FieldAmmoBios