#pragma once

// GPU WM chrome — AmmoNES menu profile (Recent flyout RAM pack).

#include "FieldAmouranthHudRam.hpp"
#include "FieldWinFrame.hpp"

#include <cstdio>
#include <cstring>

namespace FieldWmNesMenu {

inline int dropdownRows = 1;
inline int dropdownActions[FieldAmouranthHudRam::WM_DROPDOWN_MAX]{};

inline void packDropdown(std::uint8_t* ram, const char* const* labels,
                         const int* actions, int count) noexcept {
    dropdownRows = count > 0 ? count : 1;
    for (int i = 0; i < FieldAmouranthHudRam::WM_DROPDOWN_MAX; ++i)
        dropdownActions[i] = (i < count && actions) ? actions[i] : 0;
    if (!ram) return;
    const std::uint32_t base = FieldAmouranthHudRam::WM_DROPDOWN_RAM;
    ram[base] = static_cast<std::uint8_t>(dropdownRows);
    for (int i = 0; i < FieldAmouranthHudRam::WM_DROPDOWN_MAX; ++i) {
        ram[base + 1u + static_cast<std::uint32_t>(i)] =
            static_cast<std::uint8_t>(dropdownActions[i] & 0xFF);
        char* dst = reinterpret_cast<char*>(ram + base + 0x10u
            + static_cast<std::uint32_t>(i * FieldAmouranthHudRam::WM_DROPDOWN_LABEL));
        if (i < count && labels && labels[i]) {
            std::snprintf(dst, FieldAmouranthHudRam::WM_DROPDOWN_LABEL, "%s", labels[i]);
        } else {
            dst[0] = '\0';
        }
    }
}

inline void packEmptyRecent(std::uint8_t* ram) noexcept {
    static const char* kLabels[] = { "(No recent ROMs)" };
    static const int kActions[] = { 0 };
    packDropdown(ram, kLabels, kActions, 1);
}

} // namespace FieldWmNesMenu