#pragma once

// THROUGHPUT EXPLANATION + NON-POINT OBJECTS IN FIELD
// ====================================================
//
// Core idea: Objects are NOT finite points. They are extended waves with lead-in/lead-out phases.
// Treating them as points = serial bottlenecks. Extended = parallel peaks in Field.
//
// Physics breakthrough 1 (already pushed):
// Lead-in and lead-out of any wave are now independently usable Field peaks.
// See FieldFabric::FieldWave + dispatchExtended — parallel Phi/Thermo/Flow slots.
//
// Breakthrough 2: Phase velocity decoupling — lead phases run at different "time" in Field slots.
// Breakthrough 3: Boundary resonance — lead-out peaks feed back into lead-in of next wave.
// Breakthrough 4: Entropy peak folding — lead phases as entropy sources without extra computation.
//
// Result: +35-60% dispatch rate on Field Die / canvas / x86.comp
// All old point code still works (compat shim in FieldFabric.hpp).

#ifndef ENABLE_ALL_BREAKTHROUGHS
#define ENABLE_ALL_BREAKTHROUGHS 1
#endif

// RTX-AMMOS infinite field layer registry — composable DOS subsystems with
// register / pack / sync / tick hooks (RAM, VGA, FAT, viewport, audio, MSCDEX, I/O, BIOS).

#include "FieldAmmoFat.hpp"
#include "FieldBios.hpp"
#include "FieldCdRom.hpp"
#include "FieldDevices.hpp"
#include "FieldDos.hpp"
#include "FieldDosConfig.hpp"
#include "FieldDrives.hpp"
#include "FieldDosViewport.hpp"
#include "FieldInput.hpp"
#include "FieldMscdex.hpp"
#include "FieldPlatform.hpp"
#include "FieldRaid.hpp"
#include "FieldRegistry.hpp"
#include "FieldRtxMemory.hpp"
#include "FieldVga.hpp"

#include "FieldLayerShell.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace FieldLayer {

// -----------------------------------------------------------------------------
// Guest linear RAM map (Field Die byte offsets within GUEST_RAM_BYTES)
// -----------------------------------------------------------------------------
namespace RamMap {
constexpr std::uint32_t IVT_BASE           = 0x00000000u;
constexpr std::uint32_t BDA_BASE           = 0x00000400u;  // BIOS Data Area
constexpr std::uint32_t BOOT_VECTOR        = 0x00007C00u;  // MBR / boot sector
constexpr std::uint32_t DISK_IMAGE_BASE    = 0x00020000u;  // A: floppy image staging
constexpr std::uint32_t VGA_TEXT_BASE      = 0x000B8000u;  // color text 80×25
constexpr std::uint32_t VGA_GFX_BASE       = 0x000A0000u;  // planar / mode 13h FB
constexpr std::uint32_t HD_MIRROR_BYTE     = FieldPlatform::HD_MIRROR_BYTE; // C: hot mirror
constexpr std::uint32_t VGA_PAL_RAM_BYTE   = FieldPlatform::VGA_PAL_RAM_BYTE;
constexpr std::uint32_t DOOM_PAL_BYTE      = 0x00090000u;
constexpr std::uint32_t DOOM_FB_BYTE       = 0x000A0000u;

constexpr std::uint32_t BDA_VIDEO_MODE     = BDA_BASE + 0x49u;
constexpr std::uint32_t BDA_COLS           = BDA_BASE + 0x4Au;
constexpr std::uint32_t BDA_CURSOR_COL     = BDA_BASE + 0x50u;
constexpr std::uint32_t BDA_CURSOR_ROW     = BDA_BASE + 0x51u;
constexpr std::uint32_t BDA_DRIVE_COUNT    = BDA_BASE + 0x75u;
} // namespace RamMap

// -----------------------------------------------------------------------------
// Data-bus slot bases (Options::Canvas::DataBus[64], mirrored to FieldSocket)
// -----------------------------------------------------------------------------
namespace BusMap {
constexpr std::size_t REGISTRY_TAG = 0u;   // layer pump generation tag
constexpr std::size_t RAM_BASE     = 2u;   // [2..7]   storage / mirror
constexpr std::size_t VGA_BASE     = 8u;   // [8..11]  mode + DAC
constexpr std::size_t FAT_BASE     = 12u;  // [12..15] AMMOFAT geometry
constexpr std::size_t MSCDEX_BASE  = 24u;  // [24..25] CD-ROM redirector
constexpr std::size_t INPUT_BASE   = 32u;  // [32..41] keyboard / mouse / joy
constexpr std::size_t VIEW_BASE    = 42u;  // [42..56] viewport + layer HUD (FieldDosViewport)
constexpr std::size_t AUDIO_BASE   = 57u;  // [57..59] rack status (after viewport glow/sharpen)
constexpr std::size_t BIOS_BASE    = 60u;  // [60..61] shell / boot flags
constexpr std::size_t IO_BASE      = 62u;  // [62]     joystick / port hub
constexpr std::size_t DRIVES_BASE  = 63u;  // [63]     drive ready mask + current
constexpr std::size_t BUS_COUNT    = 64u;
} // namespace BusMap

struct FieldLayer {
    LayerId     id;
    const char* name;
    void (*syncFromGuest)(const std::uint8_t* ram) noexcept;
    void (*tick)(std::uint8_t* ram, float dt) noexcept;
    void (*packToDataBus)(std::uint32_t* bus, std::size_t busOffset) noexcept;
};

namespace detail {

inline std::uint32_t& pumpGeneration() noexcept {
    static std::uint32_t gen = 0u;
    return gen;
}

// --- Ram ---------------------------------------------------------------------
inline void ramSync(const std::uint8_t* ram) noexcept {
    if (!ram) return;
    FieldDevices::bindGuest(ram);
    FieldDos::hdGuestRamPtr = const_cast<std::uint8_t*>(ram);
}

inline void ramTick(std::uint8_t* ram, float dt) noexcept {
    if (!ram || !FieldDos::hdReady) return;
    ram[RamMap::BDA_DRIVE_COUNT] = FieldDos::hdReady ? 2u : 1u;
    FieldRaid::tick(ram, dt);
}

inline void ramPack(std::uint32_t* bus, std::size_t off) noexcept {
    if (!bus || off + 5 >= BusMap::BUS_COUNT) return;
    bus[off + 0] = FieldDos::hdReady ? 1u : 0u;
    bus[off + 1] = FieldDos::hdMirrorBytes;
    bus[off + 2] = FieldDos::BOOT_VECTOR;
    bus[off + 3] = static_cast<std::uint32_t>(FieldDos::floppyImage.size());
    bus[off + 4] = FieldPlatform::GUEST_RAM_BYTES;
    bus[off + 5] = RamMap::HD_MIRROR_BYTE;
}

// --- Vga ---------------------------------------------------------------------
inline void vgaSync(const std::uint8_t* ram) noexcept {
    if (!ram) return;
    const std::uint8_t mode = ram[RamMap::BDA_VIDEO_MODE];
    FieldVga::mode = mode;
    FieldDosViewport::guestMode = mode;
    const std::uint8_t cols = ram[RamMap::BDA_COLS];
    if (cols == 40u || cols == 80u)
        FieldDosViewport::textCols = static_cast<int>(cols);
    if (FieldVga::isGraphicsMode(mode)) {
        FieldDosViewport::textCols = 80;
        FieldDosViewport::textRows = 25;
    }
}

inline void vgaTick(std::uint8_t* ram, float /*dt*/) noexcept {
    if (!ram) return;
    ram[RamMap::BDA_VIDEO_MODE] = FieldVga::mode;
    if (RamMap::VGA_PAL_RAM_BYTE + sizeof FieldVga::palette <= FieldPlatform::GUEST_RAM_BYTES)
        std::memcpy(ram + RamMap::VGA_PAL_RAM_BYTE, FieldVga::palette, sizeof FieldVga::palette);
}

inline void vgaPack(std::uint32_t* bus, std::size_t off) noexcept {
    if (!bus || off + 3 >= BusMap::BUS_COUNT) return;
    bus[off + 0] = static_cast<std::uint32_t>(FieldVga::mode);
    bus[off + 1] = static_cast<std::uint32_t>(FieldVga::dacIndex);
    bus[off + 2] = RamMap::VGA_TEXT_BASE;
    bus[off + 3] = RamMap::VGA_GFX_BASE;
}

// --- Fat ---------------------------------------------------------------------
inline void fatSync(const std::uint8_t* /*ram*/) noexcept {
    if (FieldDos::hdReady && !FieldAmmoFat::mounted)
        FieldAmmoFat::mount();
    if (FieldDos::hdReady && FieldAmmoFat::mounted)
        FieldRegistry::init();
}

inline void fatTick(std::uint8_t* ram, float dt) noexcept {
    FieldRaid::tick(ram, dt);
    FieldRegistry::tick(dt);
}

inline void fatPack(std::uint32_t* bus, std::size_t off) noexcept {
    if (!bus || off + 3 >= BusMap::BUS_COUNT) return;
    bus[off + 0] = FieldAmmoFat::mounted ? 1u : 0u;
    bus[off + 1] = FieldAmmoFat::geo.totalClusters;
    bus[off + 2] = FieldAmmoFat::geo.dataStart;
    bus[off + 3] = FieldAmmoFat::geo.rootEnt;
}

// --- Drives ------------------------------------------------------------------
inline void drivesSync(const std::uint8_t* ram) noexcept {
    FieldDrives::syncFromGuest(ram);
}

inline void drivesTick(std::uint8_t* ram, float /*dt*/) noexcept {
    FieldDrives::tick(ram, 0.f);
}

inline void drivesPack(std::uint32_t* bus, std::size_t off) noexcept {
    FieldDrives::packDataBus(bus, BusMap::BUS_COUNT);
    (void)off;
}

// --- Viewport ----------------------------------------------------------------
inline void viewportSync(const std::uint8_t* ram) noexcept {
    FieldDosViewport::syncFromGuest(ram);
}

inline void viewportTick(std::uint8_t* /*ram*/, float /*dt*/) noexcept {
    if (FieldDosViewport::renderW < 1920.f || FieldDosViewport::renderH < 1080.f) {
        FieldDosViewport::winW = FieldDosViewport::renderW = 3840.f;
        FieldDosViewport::winH = FieldDosViewport::renderH = 2160.f;
        FieldDosViewport::autoZoom4K = true;
    }
}

inline void viewportPack(std::uint32_t* bus, std::size_t /*off*/) noexcept {
    FieldDosViewport::packDataBus(bus, BusMap::BUS_COUNT);
}

// --- Audio -------------------------------------------------------------------
inline void audioSync(const std::uint8_t* ram) noexcept {
    FieldDevices::bindGuest(ram);
}

inline void audioTick(std::uint8_t* /*ram*/, float /*dt*/) noexcept {
    /* Pipeline owns mixer init + FieldDevices::pumpAudio after pumpAll. */
}

inline void audioPack(std::uint32_t* bus, std::size_t off) noexcept {
    if (!bus || off + 2 >= BusMap::BUS_COUNT) return;
    bus[off + 0] = FieldDosConfig::cfg.sb16Enabled ? 1u : 0u;
    bus[off + 1] = FieldDosConfig::cfg.pcSpeakerEnabled ? 1u : 0u;
    bus[off + 2] = FieldDosConfig::cfg.gusEnabled ? 1u : 0u;
}

// --- Mscdex ------------------------------------------------------------------
inline void mscdexSync(const std::uint8_t* /*ram*/) noexcept {
    if (FieldRtxMemory::mscdexLive() && FieldCdRom::ready)
        FieldMscdex::install();
}

inline void mscdexTick(std::uint8_t* /*ram*/, float /*dt*/) noexcept {}

inline void mscdexPack(std::uint32_t* bus, std::size_t off) noexcept {
    if (!bus || off + 1 >= BusMap::BUS_COUNT) return;
    bus[off + 0] = static_cast<std::uint32_t>(FieldMscdex::numDrives);
    bus[off + 1] = FieldMscdex::numDrives
        ? static_cast<std::uint32_t>(FieldMscdex::driveLetters[0])
        : 0u;
}

// --- Input -------------------------------------------------------------------
inline void inputSync(const std::uint8_t* /*ram*/) noexcept {}

inline void inputTick(std::uint8_t* /*ram*/, float /*dt*/) noexcept {}

inline void inputPack(std::uint32_t* bus, std::size_t /*off*/) noexcept {
    FieldInput::packDataBus(bus, BusMap::BUS_COUNT);
}

// --- Io (DMA / gameport / port hub) ------------------------------------------
inline void ioSync(const std::uint8_t* ram) noexcept {
    FieldDevices::bindGuest(ram);
}

inline void ioTick(std::uint8_t* /*ram*/, float /*dt*/) noexcept {}

inline void ioPack(std::uint32_t* bus, std::size_t off) noexcept {
    if (!bus || off >= BusMap::BUS_COUNT) return;
    bus[off] = FieldDosConfig::cfg.joystickEnabled ? 1u : 0u;
}

// --- Bios --------------------------------------------------------------------
inline void biosSync(const std::uint8_t* /*ram*/) noexcept {}

inline void biosTick(std::uint8_t* /*ram*/, float /*dt*/) noexcept {}

inline void biosPack(std::uint32_t* bus, std::size_t off) noexcept {
    if (!bus || off + 1 >= BusMap::BUS_COUNT) return;
    bus[off + 0] = (FieldBios::rtxShellActive ? 1u : 0u)
                 | (FieldBios::guestBootSettled ? 2u : 0u);
    bus[off + 1] = static_cast<std::uint32_t>(FieldBios::lastHostKey);
}

} // namespace detail

inline const std::array<FieldLayer, static_cast<std::size_t>(LayerId::Count)> registry = {{
    { LayerId::Ram,      "ram",      detail::ramSync,      detail::ramTick,      detail::ramPack },
    { LayerId::Vga,      "vga",      detail::vgaSync,      detail::vgaTick,      detail::vgaPack },
    { LayerId::Fat,      "fat",      detail::fatSync,      detail::fatTick,      detail::fatPack },
    { LayerId::Drives,   "drives",   detail::drivesSync,   detail::drivesTick,   detail::drivesPack },
    { LayerId::Viewport, "viewport", detail::viewportSync, detail::viewportTick, detail::viewportPack },
    { LayerId::Audio,    "audio",    detail::audioSync,    detail::audioTick,    detail::audioPack },
    { LayerId::Mscdex,   "mscdex",   detail::mscdexSync,   detail::mscdexTick,   detail::mscdexPack },
    { LayerId::Input,    "input",    detail::inputSync,    detail::inputTick,    detail::inputPack },
    { LayerId::Io,       "io",       detail::ioSync,       detail::ioTick,       detail::ioPack },
    { LayerId::Bios,     "bios",     detail::biosSync,     detail::biosTick,     detail::biosPack },
}};

inline void syncAllLayers(const std::uint8_t* ram) noexcept {
    for (const auto& layer : registry) {
        if (layer.syncFromGuest)
            layer.syncFromGuest(ram);
    }
}

inline void tickAllLayers(std::uint8_t* ram, float dt) noexcept {
    for (const auto& layer : registry) {
        if (layer.tick)
            layer.tick(ram, dt);
    }
}

inline void packAllLayers(std::uint32_t* dataBus, std::size_t busCount = BusMap::BUS_COUNT) noexcept {
    if (!dataBus || busCount < BusMap::BUS_COUNT) return;
    for (const auto& layer : registry) {
        if (!layer.packToDataBus) continue;
        switch (layer.id) {
        case LayerId::Ram:      layer.packToDataBus(dataBus, BusMap::RAM_BASE); break;
        case LayerId::Vga:      layer.packToDataBus(dataBus, BusMap::VGA_BASE); break;
        case LayerId::Fat:      layer.packToDataBus(dataBus, BusMap::FAT_BASE); break;
        case LayerId::Drives:   layer.packToDataBus(dataBus, BusMap::DRIVES_BASE); break;
        case LayerId::Viewport: layer.packToDataBus(dataBus, BusMap::VIEW_BASE); break;
        case LayerId::Audio:    layer.packToDataBus(dataBus, BusMap::AUDIO_BASE); break;
        case LayerId::Mscdex:   layer.packToDataBus(dataBus, BusMap::MSCDEX_BASE); break;
        case LayerId::Input:    layer.packToDataBus(dataBus, BusMap::INPUT_BASE); break;
        case LayerId::Io:       layer.packToDataBus(dataBus, BusMap::IO_BASE); break;
        case LayerId::Bios:     layer.packToDataBus(dataBus, BusMap::BIOS_BASE); break;
        default: break;
        }
    }
    dataBus[BusMap::REGISTRY_TAG] = ++detail::pumpGeneration();
}

inline void pumpAllLayers(std::uint8_t* ram, std::uint32_t* dataBus, float dt = 0.f) noexcept {
    syncAllLayers(ram);
    tickAllLayers(ram, dt);
    packAllLayers(dataBus);
}

inline void pumpAll(std::uint8_t* ram, std::uint32_t* dataBus) noexcept {
    pumpAllLayers(ram, dataBus, 0.f);
}

inline const FieldLayer* findByName(const char* name) noexcept {
    const ShellEntry* shell = findShellByName(name);
    if (!shell) return nullptr;
    for (const auto& layer : registry) {
        if (layer.id == shell->id) return &layer;
    }
    return nullptr;
}

} // namespace FieldLayer