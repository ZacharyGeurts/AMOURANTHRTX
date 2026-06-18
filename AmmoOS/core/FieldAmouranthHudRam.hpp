#pragma once

// AmouranthOS GPU HUD staging — MUST live inside x86.comp die.RAM (8 MiB).
// Placed above VGA (0xB8000) so C++ pack and GPU ramByte() agree.

#include <cstdint>

namespace FieldAmouranthHudRam {

constexpr std::uint32_t SURFACE_RAM      = 0x000B9000u;
constexpr std::uint32_t MENU_RAM         = 0x000B9100u;
constexpr int           MENU_MAX_ROWS    = 24;
constexpr int           MENU_ROW_STRIDE  = 96;
constexpr std::uint32_t MENU_FLYOUT_OFF  = 0x10u
    + static_cast<std::uint32_t>(MENU_MAX_ROWS * MENU_ROW_STRIDE);

constexpr std::uint32_t TASKBAR_RAM      = 0x000BA400u;
constexpr std::uint32_t START_LABEL_OFF  = 0x80u;
constexpr std::uint32_t FOOTER_RAM       = 0x000BA600u;
constexpr std::uint32_t STATUS_RAM       = 0x000BA680u;
constexpr std::uint32_t CLOCK_DATE_RAM   = 0x000BA700u;

} // namespace FieldAmouranthHudRam