#pragma once

// AmouranthOS GPU HUD staging — MUST live inside x86.comp die.RAM (8 MiB).
// Placed above VGA (0xB8000) so C++ pack and GPU ramByte() agree.

#include "FieldRtxMemory.hpp"

#include <cstdint>
#include <cstring>

namespace FieldAmouranthHudRam {

constexpr std::uint32_t SURFACE_RAM      = 0x000B9000u;
constexpr std::uint32_t MENU_RAM         = 0x000B9100u;
constexpr int           MENU_MAX_ROWS    = 24;
constexpr int           MENU_ROW_STRIDE  = 112;
constexpr std::uint32_t MENU_FLYOUT_OFF  = 0x10u
    + static_cast<std::uint32_t>(MENU_MAX_ROWS * MENU_ROW_STRIDE);

constexpr std::uint32_t FILES_MENU_RAM   = 0x000BA010u;
constexpr int           FILES_MENU_MAX   = 24;
constexpr int           FILES_ROW_STRIDE = 24;

constexpr std::uint32_t TASKBAR_RAM      = 0x000BA400u;
constexpr std::uint32_t START_LABEL_OFF  = 0x80u;
constexpr std::uint32_t FOOTER_RAM       = 0x000BA600u;
constexpr std::uint32_t STATUS_RAM       = 0x000BA680u;
// WM panel dropdown rows (AmmoNES Recent labels + action ids for GPU chrome).
constexpr std::uint32_t WM_DROPDOWN_RAM  = 0x000BA780u;
constexpr int           WM_DROPDOWN_MAX  = 8;
constexpr int           WM_DROPDOWN_LABEL = 40;
constexpr std::uint32_t CLOCK_DATE_RAM   = 0x000BA700u;
constexpr std::uint32_t WIDGET_RAM       = 0x000BB000u;
constexpr std::uint32_t IDENTITY_RAM     = 0x000BB800u;
constexpr std::uint32_t JOURNAL_RAM      = 0x000BC000u;

// File search flyout — query + top-10 results for Start menu (GPU HUD).
constexpr std::uint32_t FILE_SEARCH_RAM  = 0x000BD000u;
constexpr int           SEARCH_MAX       = 10;
constexpr int           SEARCH_QUERY_LEN = 32;
constexpr int           SEARCH_NAME_LEN  = 28;
constexpr int           SEARCH_PATH_LEN  = 48;
constexpr int           SEARCH_ROW_STRIDE = 80;

// Drag-and-drop scaffold — source path + ghost position (not wired to UI yet).
constexpr std::uint32_t DND_RAM          = 0x000BD500u;
constexpr int           DND_PATH_LEN     = 64;

// Zero GPU HUD staging (taskbar/menu/journal) — bootGuest clears 0..0xC0000 and leaves
// garbage tab slots visible until packChromeRam runs again.
inline void clearRegion(std::uint8_t* ram) noexcept {
    if (!ram) return;
    const std::uint32_t span = FieldRtxMemory::hudClearBytes();
    if (!span) return;
    std::memset(ram + SURFACE_RAM, 0, span);
}

} // namespace FieldAmouranthHudRam