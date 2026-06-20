#pragma once

// MSCDEX 2.x — INT 2Fh CD-ROM redirector (real API for RTX-AMMOS drive D:).

#include "FieldCdRom.hpp"
#include "FieldLayerShell.hpp"
#include "FieldRtxMemory.hpp"

#include <cstdint>
#include <cstring>
#include <string>

namespace FieldMscdex {

constexpr char DRIVER_NAME[] = "RTXCD001";
constexpr std::uint8_t FIRST_DRIVE = 3; /* D: = 0=A 1=B 2=C 3=D */

inline bool installed = false;
inline std::uint8_t numDrives = 0;
inline std::uint8_t driveLetters[4]{};
inline std::uint8_t driverSubunit = 0;

inline void dismiss() noexcept {
    installed = false;
    numDrives = 0;
    driverSubunit = 0;
    for (auto& l : driveLetters) l = 0;
}

inline void install() noexcept {
    if (!FieldRtxMemory::mscdexLive()) {
        dismiss();
        return;
    }
    installed = true;
    numDrives = FieldCdRom::ready ? 1u : 0u;
    if (numDrives > 0) {
        driveLetters[0] = static_cast<std::uint8_t>('D');
        FieldCdRom::volumeLabel = FieldCdRom::volumeLabel.empty() ? "RTXCD001" : FieldCdRom::volumeLabel;
    }
}

inline std::uint16_t handle(std::uint16_t ax, std::uint16_t bx, std::uint16_t /*cx*/,
                            std::uint16_t& outBx, std::uint16_t& outCx, std::uint16_t& outDx,
                            std::uint32_t& outEax) noexcept {
    if (!FieldRtxMemory::mscdexLive() || !installed) return 0;
    const std::uint8_t fn = static_cast<std::uint8_t>((ax >> 8) & 0xFFu);
    if (fn != 0x15u) return 0; /* MSCDEX multiplex */
    const std::uint8_t sub = static_cast<std::uint8_t>(ax & 0xFFu);

    switch (sub) {
    case 0x00: /* Get number of CD-ROM drives */
        outCx = numDrives;
        outEax = 0;
        return 1;
    case 0x01: /* Get drive list — ES:BX buffer, caller fills */
        outEax = numDrives;
        return 1;
    case 0x02: /* Get driver header */
        outEax = 0;
        return 1;
    case 0x03: /* Get drive letter for subunit BX */
        if (bx < numDrives) {
            outDx = driveLetters[bx];
            outEax = 0;
            return 1;
        }
        outEax = 0x0F; /* invalid */
        return 1;
    case 0x04: /* Get subunit for drive letter BL */
        if (numDrives && (bx & 0xFFu) == driveLetters[0]) {
            outBx = 0;
            outEax = 0;
            return 1;
        }
        outEax = 0x0F;
        return 1;
    case 0x06: /* Get driver version */
        outBx = 0x0201u; /* MSCDEX 2.1 */
        outEax = 0;
        return 1;
    case 0x08: /* IOCTL read long — media change */
        outEax = FieldCdRom::ready ? 0u : 0x0Fu;
        return 1;
    case 0x0C: /* IOCTL read */
        outEax = 0;
        return 1;
    case 0x0E: /* Get driver name */
        outEax = 0;
        return 1;
    case 0x0F: /* Get UPC / volume */
        outEax = 0;
        return 1;
    default:
        outEax = 0x0F;
        return 1;
    }
}

inline bool ioctlRead(std::uint8_t drive, std::uint8_t* out, std::size_t max) noexcept {
    if (!FieldCdRom::ready || drive != 'D') return false;
    const char* info = "RTX-AMMOS MSCDEX 2.1\r\n";
    const std::size_t n = std::min(max, std::strlen(info));
    std::memcpy(out, info, n);
    return true;
}

inline std::string statusLine() noexcept {
    const char* layer = FieldLayer::isShellActive(FieldLayer::LayerId::Mscdex) ? "ON" : "off";
    if (!FieldCdRom::ready)
        return std::string("MSCDEX [mscdex layer ") + layer
            + "]: no CD-ROM (USB/DVD /dev/sr0 or .iso in assets/dos/incoming/cd/)";
    char buf[160];
    std::snprintf(buf, sizeof buf,
        "MSCDEX 2.1 [mscdex layer %s] L%u — %u drive(s) D: %s (%u sectors)",
        layer, static_cast<unsigned>(FieldLayer::LayerId::Mscdex),
        numDrives, FieldCdRom::volumeLabel.c_str(), FieldCdRom::sectorCount());
    return buf;
}

} // namespace FieldMscdex