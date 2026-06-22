#pragma once

// Field theory + Tesla valve absolutes — single source for engine, shaders, DevKit headers.
// Matches CANVAS.comp / Pipeline.hpp / Options::AnalogFields.

#include <cstdint>

namespace FieldRtxFieldAbs {

constexpr const char* THEORY_REV = "2026.1";
constexpr const char* PRODUCT    = "RTX-DOS 7.0 Field Die";

// Guest RAM map (FieldLayer::RamMap)
constexpr std::uint32_t RAM_BYTES        = 0x04000000u;
constexpr std::uint32_t BOOT_VECTOR      = 0x00007C00u;
constexpr std::uint32_t VGA_TEXT         = 0x000B8000u;
constexpr std::uint32_t HD_MIRROR_BYTE   = 0x01000000u;
constexpr std::uint32_t HD_MIRROR_MAX    = 32u * 1024u * 1024u;

// Field layer IDs (L0-L9)
constexpr std::uint8_t LAYER_RAM       = 0;
constexpr std::uint8_t LAYER_VGA       = 1;
constexpr std::uint8_t LAYER_FAT       = 2;
constexpr std::uint8_t LAYER_DRIVES    = 3;
constexpr std::uint8_t LAYER_VIEWPORT  = 4;
constexpr std::uint8_t LAYER_AUDIO     = 5;
constexpr std::uint8_t LAYER_MSCDEX    = 6;
constexpr std::uint8_t LAYER_INPUT     = 7;
constexpr std::uint8_t LAYER_IO        = 8;
constexpr std::uint8_t LAYER_BIOS      = 9;
constexpr std::uint8_t LAYER_COUNT      = 10;

// Data bus slots (Options::Canvas::DataBus)
constexpr std::uint8_t BUS_REGISTRY    = 0;
constexpr std::uint8_t BUS_RAM         = 2;
constexpr std::uint8_t BUS_VGA         = 8;
constexpr std::uint8_t BUS_FAT         = 12;
constexpr std::uint8_t BUS_MSCDEX      = 24;
constexpr std::uint8_t BUS_CLOCK       = 29;  // AOS clock pack (hr<<16|mn<<8|sec); Kilroy overlays when active
constexpr std::uint8_t BUS_HOST_HEAT   = 33;  // FieldSocket hostHeat float (Pipeline thermo path)
constexpr std::uint8_t BUS_TESLA       = 34;  // FieldSocket TeslaBiasStrength float

// Tesla valve flow resistance (CANVAS.comp teslaR)
constexpr float TESLA_R_FORWARD   = 0.18f;
constexpr float TESLA_R_REVERSE   = 3.2f;
constexpr float TESLA_BIAS_NOM    = 1.0f;
constexpr float TESLA_ENTROPY_K   = 0.15f;
constexpr float TESLA_DAMP_REV    = 0.09f;

// Milli-scaled integers for AMMOCC / ASM (value * 1000)
constexpr int TESLA_R_FWD_MILLI   = 180;
constexpr int TESLA_R_REV_MILLI   = 3200;
constexpr int FIELD_PHI_MILLI     = 618;   // golden-ratio gate hint

// Thermo / entropy floor (Pipeline boundaryThermo baseline)
constexpr float THERMO_ALPHA_NOM  = 0.6f;
constexpr float ENTROPY_FLOOR     = 0.002f;
constexpr float BOUNDARY_THERMO     = 0.31f;

// RTX multiplex (INT 2Fh)
constexpr std::uint8_t RTX_MUX_AH     = 0x52;
constexpr std::uint16_t IOCTL_AMMOFAT = 0x2F10;
constexpr std::uint16_t IOCTL_RTXVGA  = 0x2F00;

} // namespace FieldRtxFieldAbs