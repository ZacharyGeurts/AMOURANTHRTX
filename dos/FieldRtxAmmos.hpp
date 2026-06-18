#pragma once

// RTX-AMMOS — unified low-level x86 platform (DOS + Linux bridge + Vulkan Field Die).
// Prepared for custom GUI, drivers, and toolchain extensions.

#include "FieldPlatform.hpp"

#include <cstdint>
#include <cstring>

namespace FieldRtxAmmos {

constexpr const char* PRODUCT      = "RTX-AMMOS";
constexpr const char* DOS_LAYER    = "RTX-DOS 7.0";
constexpr const char* TAGLINE      = "Golden Era Man+Machine — GPU Super DOSBox forever";
constexpr const char* ARCHITECTURE = "GPU-primary Field Die; host shell + device rack; HD/CD persistence";

enum class Era : std::uint8_t {
    PrePc   = 0,  // 1970s terminals (reserved)
    Dos     = 1,  // RTX-DOS 7.0
    Win31   = 2,
    Win95   = 3,
    Win98   = 4,
    Linux   = 5,  // host bridge (future native layer)
    RtxGui  = 6,  // custom GUI slot
};

enum class DeviceClass : std::uint8_t {
    Cpu,
    Vga,
    Storage,
    CdRom,
    Audio,
    Input,
    Network,
    Serial,
    Custom,
};

struct DeviceSlot {
    const char* id;
    const char* name;
    DeviceClass cls;
    bool enabled;
    std::uint16_t basePort;
    std::uint8_t irq;
};

inline Era activeEra = Era::Dos;

// Catalog — extend for custom drivers / compile toolchain hooks.
inline constexpr DeviceSlot kDevices[] = {
    {"field_die",   "Vulkan x86 Field Die",     DeviceClass::Cpu,     true,  0,    0},
    {"vga",         "IBM VGA + VESA",           DeviceClass::Vga,     true,  0x3C0, 0},
    {"floppy_a",    "720K RTX boot",            DeviceClass::Storage, true,  0,    6},
    {"hdd_c",       "2GB RTXDOS FAT16",         DeviceClass::Storage, true,  0x1F0, 14},
    {"cdrom_d",     "ISO9660 CD-ROM",           DeviceClass::CdRom,   true,  0x1F0, 15},
    {"sb16",        "Sound Blaster 16",         DeviceClass::Audio,   true,  0x220, 5},
    {"opl3",        "Yamaha OPL3 FM",           DeviceClass::Audio,   true,  0x388, 0},
    {"gus",         "Gravis UltraSound",        DeviceClass::Audio,   true,  0x240, 7},
    {"ess",         "ESS AudioDrive",           DeviceClass::Audio,   true,  0x220, 7},
    {"covox",       "Covox Speech Thing",       DeviceClass::Audio,   true,  0x378, 0},
    {"mpu401",      "Roland MPU-401 MIDI",      DeviceClass::Audio,   true,  0x330, 0},
    {"lapc1",       "Roland LAPC-1 / MT-32",    DeviceClass::Audio,   true,  0x330, 0},
    {"disney",      "Disney Sound Source",      DeviceClass::Audio,   true,  0x378, 0},
    {"pcspk",       "PC Speaker / PIT",           DeviceClass::Audio,   true,  0x61,  0},
    {"mouse",       "Microsoft mouse INT33",    DeviceClass::Input,   true,  0,    0},
    {"joystick",    "Gameport",                 DeviceClass::Input,   true,  0x201, 0},
    {"com1",        "8250 UART COM1",           DeviceClass::Serial,  true,  0x3F8, 4},
    {"com2",        "8250 UART COM2",           DeviceClass::Serial,  true,  0x2F8, 3},
    {"com3",        "8250 UART COM3",           DeviceClass::Serial,  true,  0x3E8, 4},
    {"com4",        "8250 UART COM4",           DeviceClass::Serial,  true,  0x2E8, 3},
    {"lpt1",        "Parallel LPT1",            DeviceClass::Serial,  true,  0x378, 7},
    {"lpt2",        "Parallel LPT2",            DeviceClass::Serial,  true,  0x278, 5},
    {"lpt3",        "Parallel LPT3",            DeviceClass::Serial,  true,  0x3BC, 7},
    {"usb_host",    "USB 2.0 host bridge",      DeviceClass::Custom,  true,  0,    0},
    {"bluetooth",   "Bluetooth adapter",        DeviceClass::Custom,  true,  0,    0},
    {"ne2000",      "NE2000 (reserved)",        DeviceClass::Network, false, 0x300, 3},
    {"rtx_toolchain","RTX DevKit AMMOASM/CC/LINK",DeviceClass::Custom,  true,  0x2F30, 0},
};

inline std::size_t deviceCount() noexcept {
    return sizeof(kDevices) / sizeof(kDevices[0]);
}

inline int deviceIndexById(const char* id) noexcept {
    if (!id) return -1;
    for (std::size_t i = 0; i < deviceCount(); ++i)
        if (std::strcmp(kDevices[i].id, id) == 0)
            return static_cast<int>(i);
    return -1;
}

inline const DeviceSlot* deviceById(const char* id) noexcept {
    const int idx = deviceIndexById(id);
    return idx >= 0 ? &kDevices[static_cast<std::size_t>(idx)] : nullptr;
}

} // namespace FieldRtxAmmos