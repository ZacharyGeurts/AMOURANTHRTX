#pragma once

// RTX-AMMOS native driver registry — IOCTL table keyed by FieldLayer::LayerId.

#include "FieldDrives.hpp"
#include "FieldLayerShell.hpp"
#include "FieldMscdex.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>

namespace FieldRtxDrivers {

enum class DriverType : std::uint8_t {
    Block,
    Char,
    Fs,
    Audio,
    Display,
};

struct Driver {
    const char* name;
    const char* file;
    DriverType type;
    FieldLayer::LayerId layerId;
    std::uint16_t ioctlBase;
    bool loaded;
};

inline Driver kDriverTable[] = {
    {"RTXKERNEL", "RTXKERNEL.SYS", DriverType::Block, FieldLayer::LayerId::Ram,    0x2F50, false},
    {"RTXDRV",  "RTXDRV.SYS",  DriverType::Block,   FieldLayer::LayerId::Ram,      0x2F05, false},
    {"RTXCD",   "RTXCD.SYS",   DriverType::Block,   FieldLayer::LayerId::Mscdex,   0x1500, true},
    {"RTXSB",   "RTXSB.SYS",   DriverType::Audio,   FieldLayer::LayerId::Audio,    0x2200, false},
    {"RTXVGA",  "RTXVGA.SYS",  DriverType::Display, FieldLayer::LayerId::Vga,      0x2F00, false},
    {"AMMOFAT", "AMMOFAT.SYS", DriverType::Fs,      FieldLayer::LayerId::Fat,      0x2F10, true},
    {"RTXHOST", "RTXHOST.SYS", DriverType::Char,    FieldLayer::LayerId::Io,       0x2F40, false},
    {"RTXAPI",  "RTXAPI.SYS",  DriverType::Char,    FieldLayer::LayerId::Bios,     0x2F20, false},
};

inline std::size_t count() noexcept { return sizeof(kDriverTable) / sizeof(kDriverTable[0]); }

inline Driver* byLayer(FieldLayer::LayerId id) noexcept {
    for (std::size_t i = 0; i < count(); ++i)
        if (kDriverTable[i].layerId == id) return &kDriverTable[i];
    return nullptr;
}

inline void syncLayerToDriver(FieldLayer::LayerId id) noexcept {
    FieldDrives::refresh();
    for (std::size_t i = 0; i < count(); ++i) {
        if (kDriverTable[i].layerId != id) continue;
        if (std::strcmp(kDriverTable[i].file, "RTXDRV.SYS") == 0) {
            const FieldDrives::Drive* s = FieldDrives::byLetter('A');
            kDriverTable[i].loaded = s && s->ready;
            return;
        }
        if (std::strcmp(kDriverTable[i].file, "RTXHOST.SYS") == 0) {
            const FieldDrives::Drive* s = FieldDrives::byLetter('E');
            kDriverTable[i].loaded = s && s->ready;
            return;
        }
        kDriverTable[i].loaded = FieldLayer::isShellActive(id);
        return;
    }
}

inline void syncAllLayers() noexcept {
    for (const auto& layer : FieldLayer::kShellRegistry)
        syncLayerToDriver(layer.id);
}

inline bool ioctl(std::uint16_t code, std::uint32_t& eax, std::uint16_t& ebx,
                  std::uint16_t& ecx, std::uint16_t& edx) noexcept {
    if ((code & 0xFF00u) == 0x1500u) {
        if (!FieldLayer::isShellActive(FieldLayer::LayerId::Mscdex)) return false;
        std::uint32_t outEax = 0;
        const std::uint16_t hit = FieldMscdex::handle(code, ebx, ecx, ebx, ecx, edx, outEax);
        eax = outEax;
        return hit != 0;
    }
    if (code == 0x2F50u) {
        syncAllLayers();
        for (std::size_t i = 0; i < count(); ++i)
            if (std::strcmp(kDriverTable[i].file, "RTXKERNEL.SYS") == 0)
                kDriverTable[i].loaded = true;
        eax = 0x0100u;
        return true;
    }
    if (code == 0x2F05u) {
        syncLayerToDriver(FieldLayer::LayerId::Ram);
        eax = 0x0100u;
        return true;
    }
    if (code == 0x2F10u) {
        if (!FieldLayer::isShellActive(FieldLayer::LayerId::Fat)) return false;
        eax = 0x0100u;
        return true;
    }
    if (code == 0x2F40u) {
        syncLayerToDriver(FieldLayer::LayerId::Io);
        eax = 0x0100u;
        return true;
    }
    if (code == 0x2F20u) {
        if (!FieldLayer::isShellActive(FieldLayer::LayerId::Bios)) return false;
        eax = 0x0100u;
        return true;
    }
    if (code == 0x2F30u) {
        eax = 0x0630u;
        return true;
    }
    if (code == 0x2F00u) {
        if (!FieldLayer::isShellActive(FieldLayer::LayerId::Vga)) return false;
        eax = 0x2026u;
        return true;
    }
    if (code == 0x2200u) {
        syncLayerToDriver(FieldLayer::LayerId::Audio);
        if (!FieldLayer::isShellActive(FieldLayer::LayerId::Audio)) return false;
        eax = 0x0100u;
        return true;
    }
    (void)ecx;
    (void)edx;
    (void)ebx;
    return false;
}

inline const char* listLoaded() noexcept {
    static char buf[256];
    buf[0] = '\0';
    for (std::size_t i = 0; i < count(); ++i) {
        if (!kDriverTable[i].loaded) continue;
        std::strncat(buf, kDriverTable[i].name, sizeof buf - std::strlen(buf) - 1);
        std::strncat(buf, " ", sizeof buf - std::strlen(buf) - 1);
    }
    return buf;
}

inline void formatTableLine(char* buf, std::size_t len, std::size_t index) noexcept {
    if (index >= count()) {
        buf[0] = '\0';
        return;
    }
    const Driver& d = kDriverTable[index];
    const char* layerName = "?";
    for (const auto& L : FieldLayer::kShellRegistry) {
        if (L.id == d.layerId) {
            layerName = L.name;
            break;
        }
    }
    std::snprintf(buf, len, "  %-8s %-12s L%u %-7s %s\r\n",
        d.name, d.file, static_cast<unsigned>(d.layerId),
        d.loaded ? "loaded" : "idle", layerName);
}

} // namespace FieldRtxDrivers