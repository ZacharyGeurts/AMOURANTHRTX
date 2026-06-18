#pragma once

// RTX Device Manager 2026 — chassis layout with icons (motherboard + drives + audio + I/O).

#include "FieldDevicePolicy.hpp"
#include "FieldDrives.hpp"
#include "FieldLayerShell.hpp"
#include "FieldMscdex.hpp"
#include "FieldRegistry.hpp"
#include "FieldRtxAmmos.hpp"
#include "FieldRtxDrivers.hpp"
#include "FieldRtxGui.hpp"
#include "FieldRtxMouse.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace FieldDeviceViewer {

struct Slot {
    int         row;
    int         col;
    int         w;
    int         h;
    char        icon;
    const char* shortLabel;
    const char* deviceId;   // FieldRtxAmmos::kDevices id, nullptr = none
    int         driverIdx;  // FieldRtxDrivers::kDriverTable index
    char        driveLetter; // 'A'..'E' or 0
};

inline bool active = false;
inline int  sel = 0;

constexpr int CHASSIS_R0 = 5;
constexpr int CHASSIS_R1 = 19;
constexpr int CHASSIS_C0 = 2;
constexpr int CHASSIS_C1 = 77;
constexpr int DETAIL_ROW = 21;

inline constexpr Slot kSlots[] = {
    { 7,  4,  8, 3, FieldRtxGui::ICO_BLOCK, "CPU",     "field_die", -1, 0 },
    { 7, 14,  6, 3, FieldRtxGui::ICO_BLOCK, "VGA",     "vga",       -1, 0 },
    { 7, 22,  4, 3, FieldRtxGui::ICO_DISK,  "A:",      "floppy_a",  -1, 'A' },
    { 7, 27,  4, 3, FieldRtxGui::ICO_DISK,  "C:",      "hdd_c",     -1, 'C' },
    { 7, 32,  4, 3, '\x09',                 "D:",      "cdrom_d",   -1, 'D' },
    { 7, 37,  4, 3, FieldRtxGui::ICO_FOLDER,"E:",      nullptr,     -1, 'E' },
    { 7, 58,  8, 3, FieldRtxGui::ICO_BLOCK, "KERN",    nullptr,      0, 0 },
    { 7, 67,  8, 3, FieldRtxGui::ICO_DISK,  "FAT",     nullptr,      5, 0 },
    {11,  4,  5, 2, '\x0E',                 "SB16",    "sb16",      -1, 0 },
    {11, 10,  5, 2, '\x0E',                 "OPL3",    "opl3",      -1, 0 },
    {11, 16,  5, 2, '\x0E',                 "GUS",     "gus",       -1, 0 },
    {11, 22,  5, 2, '\x0E',                 "ESS",     "ess",       -1, 0 },
    {11, 28,  5, 2, '\x0E',                 "Covox",   "covox",     -1, 0 },
    {11, 34,  5, 2, '\x0E',                 "MPU",     "mpu401",    -1, 0 },
    {11, 40,  5, 2, '\x0E',                 "LAPC",    "lapc1",     -1, 0 },
    {11, 46,  5, 2, '\x0E',                 "DSS",     "disney",    -1, 0 },
    {11, 52,  5, 2, '\x0E',                 "SPK",     "pcspk",     -1, 0 },
    {15,  4,  6, 2, 'M',                    "Mouse",   "mouse",     -1, 0 },
    {15, 11,  6, 2, 'J',                    "Joy",     "joystick",  -1, 0 },
    {15, 18,  6, 2, 'S',                    "COM1",    "com1",      -1, 0 },
    {15, 25,  6, 2, 'S',                    "COM2",    "com2",      -1, 0 },
    {15, 32,  6, 2, 'P',                    "LPT1",    "lpt1",      -1, 0 },
    {15, 39,  6, 2, 'U',                    "USB",     "usb_host",  -1, 0 },
    {15, 46,  6, 2, 'B',                    "BT",      "bluetooth", -1, 0 },
    {15, 58,  8, 2, FieldRtxGui::ICO_BLOCK, "RTXCD",   nullptr,      2, 0 },
    {15, 67,  8, 2, FieldRtxGui::ICO_FOLDER,"HOST",    nullptr,      6, 0 },
};

inline int slotCount() noexcept {
    return static_cast<int>(sizeof(kSlots) / sizeof(kSlots[0]));
}

inline bool slotLive(const Slot& s) noexcept {
    FieldDrives::refresh();
    FieldRtxDrivers::syncAllLayers();
    if (s.driverIdx >= 0) {
        const std::size_t di = static_cast<std::size_t>(s.driverIdx);
        if (di < FieldRtxDrivers::count())
            return FieldRtxDrivers::kDriverTable[di].loaded;
        return false;
    }
    if (s.driveLetter) {
        const FieldDrives::Drive* d = FieldDrives::byLetter(s.driveLetter);
        return d && d->ready;
    }
    const FieldRtxAmmos::DeviceSlot* dev = s.deviceId ? FieldRtxAmmos::deviceById(s.deviceId) : nullptr;
    return dev ? FieldDevicePolicy::isLive(*dev) : false;
}

inline void drawMiniBox(std::uint8_t* ram, const Slot& s, bool selected, bool on) noexcept {
    const std::uint8_t frame = selected ? FieldRtxGui::ATTR_MENU_SEL : FieldRtxGui::ATTR_FRAME;
    const std::uint8_t body  = on ? FieldRtxGui::ATTR_EDITOR : FieldRtxGui::ATTR_DIM;
    FieldRtxGui::drawFrame(ram, s.row, s.col, s.row + s.h, s.col + s.w, frame, nullptr);
    FieldRtxGui::put(ram, s.row + 1, s.col + s.w / 2, s.icon, on ? FieldRtxGui::ATTR_GOLD : body);
    FieldRtxGui::text(ram, s.row + s.h - 1, s.col + 1, s.shortLabel, body, s.w - 1);
    if (on)
        FieldRtxGui::put(ram, s.row + s.h, s.col + s.w / 2, '\xFE', FieldRtxGui::ATTR_DEBUG);
    else
        FieldRtxGui::put(ram, s.row + s.h, s.col + s.w / 2, '\xB0', FieldRtxGui::ATTR_DIM);
}

inline void paintChassis(std::uint8_t* ram) noexcept {
    FieldRtxGui::drawFrame(ram, CHASSIS_R0, CHASSIS_C0, CHASSIS_R1, CHASSIS_C1,
        FieldRtxGui::ATTR_FRAME, " RTX-AMMOS Field PC ");
    FieldRtxGui::text(ram, CHASSIS_R0 + 1, CHASSIS_C0 + 2,
        " motherboard ", FieldRtxGui::ATTR_DIM, 14);
    FieldRtxGui::drawHLine(ram, 9, CHASSIS_C0 + 14, CHASSIS_C0 + 54, FieldRtxGui::ATTR_DIM);
    FieldRtxGui::text(ram, 9, CHASSIS_C0 + 20, " ISA / PCI bus ", FieldRtxGui::ATTR_TITLE, 16);
    FieldRtxGui::text(ram, 10, CHASSIS_C0 + 2, " drives ", FieldRtxGui::ATTR_DIM, 10);
    FieldRtxGui::text(ram, 10, CHASSIS_C0 + 54, " field SYS ", FieldRtxGui::ATTR_DIM, 12);
    FieldRtxGui::text(ram, 13, CHASSIS_C0 + 2, " audio rack ", FieldRtxGui::ATTR_DIM, 12);
    FieldRtxGui::text(ram, 17, CHASSIS_C0 + 2, " rear I/O panel ", FieldRtxGui::ATTR_DIM, 18);
    for (int i = 0; i < slotCount(); ++i)
        drawMiniBox(ram, kSlots[static_cast<std::size_t>(i)], i == sel, slotLive(kSlots[static_cast<std::size_t>(i)]));
}

inline void paintDetail(std::uint8_t* ram) noexcept {
    FieldRtxGui::fill(ram, DETAIL_ROW, ' ', FieldRtxGui::ATTR_STATUS);
    FieldRtxGui::fill(ram, DETAIL_ROW + 1, ' ', FieldRtxGui::ATTR_DIM);
    if (sel < 0 || sel >= slotCount()) return;
    const Slot& s = kSlots[static_cast<std::size_t>(sel)];
    const bool on = slotLive(s);
    char line1[80];
    char line2[80];
    if (s.driverIdx >= 0) {
        const auto& dr = FieldRtxDrivers::kDriverTable[static_cast<std::size_t>(s.driverIdx)];
        std::snprintf(line1, sizeof line1,
            " %s %s — IOCTL %04X  [%s]  Space=toggle layer ",
            dr.name, dr.file, dr.ioctlBase, on ? "LOADED" : "idle");
        const char* layerName = "?";
        for (const auto& L : FieldLayer::kShellRegistry) {
            if (L.id == dr.layerId) { layerName = L.name; break; }
        }
        std::snprintf(line2, sizeof line2, " Layer L%u %s | DRIVERS for full table | Esc close ",
            static_cast<unsigned>(dr.layerId), layerName);
    } else if (s.driveLetter) {
        const FieldDrives::Drive* d = FieldDrives::byLetter(s.driveLetter);
        std::snprintf(line1, sizeof line1,
            " Drive %c: %s — %s  %u MB free / %u MB ",
            s.driveLetter,
            d && d->volume[0] ? d->volume : "RTXDOS",
            d && d->driver ? d->driver : "—",
            d ? d->freeBytes / (1024u * 1024u) : 0u,
            d ? d->totalBytes / (1024u * 1024u) : 0u);
        std::snprintf(line2, sizeof line2, " %s | click or arrows to inspect | F5 refresh ",
            on ? "Ready — field layer online" : "Offline");
    } else if (s.deviceId) {
        const FieldRtxAmmos::DeviceSlot* dp = FieldRtxAmmos::deviceById(s.deviceId);
        if (!dp) {
            std::snprintf(line1, sizeof line1, " Unknown device id: %s ", s.deviceId);
            line2[0] = '\0';
            FieldRtxGui::text(ram, DETAIL_ROW, 1, line1, FieldRtxGui::ATTR_STATUS, 78);
            return;
        }
        const auto& d = *dp;
        std::snprintf(line1, sizeof line1,
            " %s — %s  port=%04X irq=%u  [%s] ",
            d.id, d.name, d.basePort, d.irq, on ? "ON" : "off");
        std::snprintf(line2, sizeof line2,
            " Space toggles mouse/joy/SB16/GUS | REGEDIT HKRTX\\Software\\RTX-DOS\\Audio ");
    } else {
        std::snprintf(line1, sizeof line1, " (empty slot) ");
        line2[0] = '\0';
    }
    FieldRtxGui::text(ram, DETAIL_ROW, 1, line1, FieldRtxGui::ATTR_STATUS, 78);
    FieldRtxGui::text(ram, DETAIL_ROW + 1, 1, line2, FieldRtxGui::ATTR_DIM, 78);
}

inline void paint(std::uint8_t* ram) noexcept {
    if (!active) return;
    FieldRtxGui::initTextMode(ram);
    FieldRtxGui::drawFrame(ram, 2, 0, 23, 79, FieldRtxGui::ATTR_RTX,
        " RTX Device Manager 2026 — PC chassis view ");
    FieldRtxGui::text(ram, 3, 2,
        " Arrows/click select | Space toggle | Tab next | F5 refresh | Esc close ",
        FieldRtxGui::ATTR_HELP, 76);
    paintChassis(ram);
    paintDetail(ram);
}

inline void open() noexcept {
    FieldDrives::refresh();
    FieldRtxDrivers::syncAllLayers();
    FieldRegistry::ensure();
    active = true;
    sel = 0;
}

inline void close() noexcept {
    active = false;
}

inline bool hitSlot(int row, int col, int& outIdx) noexcept {
    for (int i = 0; i < slotCount(); ++i) {
        const Slot& s = kSlots[static_cast<std::size_t>(i)];
        if (row >= s.row && row <= s.row + s.h && col >= s.col && col <= s.col + s.w) {
            outIdx = i;
            return true;
        }
    }
    return false;
}

inline void toggleSel() noexcept {
    if (sel < 0 || sel >= slotCount()) return;
    const Slot& s = kSlots[static_cast<std::size_t>(sel)];
    if (s.deviceId) FieldDevicePolicy::toggleId(s.deviceId);
}

inline void handleMouseFrame(std::uint8_t* ram) noexcept {
    if (!active) return;
    const auto mf = FieldRtxMouse::capture();
    if (mf.visible) {
        int idx = 0;
        if (hitSlot(mf.row, mf.col, idx)) {
            sel = idx;
            if (mf.leftClick) toggleSel();
        }
        FieldRtxMouse::paintPointer(ram, mf.col, mf.row);
    }
}

inline bool handleKey(std::uint8_t* ram, std::uint16_t key) noexcept {
    if (!active) return false;
    switch (key) {
    case 0x011Bu: close(); return true;
    case 0x0F00u: sel = (sel + 1) % slotCount(); paint(ram); return true;
    case 0x3F00u: FieldDrives::refresh(); FieldRtxDrivers::syncAllLayers(); paint(ram); return true;
    case 0x3920u: toggleSel(); paint(ram); return true;
    case 0x4800u: if (sel > 0) --sel; paint(ram); return true;
    case 0x4B00u: if (sel >= 6) sel -= 6; else sel = 0; paint(ram); return true;
    case 0x5000u: if (sel + 1 < slotCount()) ++sel; paint(ram); return true;
    case 0x4D00u: if (sel + 6 < slotCount()) sel += 6; else sel = slotCount() - 1; paint(ram); return true;
    default: break;
    }
    paint(ram);
    return true;
}

} // namespace FieldDeviceViewer