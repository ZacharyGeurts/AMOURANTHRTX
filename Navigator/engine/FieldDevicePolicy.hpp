#pragma once

// Device live-state and registry-backed toggles (audio + input).

#include "FieldDosConfig.hpp"
#include "FieldDrives.hpp"
#include "FieldMscdex.hpp"
#include "FieldRegistry.hpp"
#include "FieldRtxAmmos.hpp"

#include <cstring>

namespace FieldDevicePolicy {

inline bool isLive(const FieldRtxAmmos::DeviceSlot& d) noexcept {
    FieldDrives::refresh();
    if (std::strcmp(d.id, "mouse") == 0) return FieldDosConfig::cfg.mouseEnabled;
    if (std::strcmp(d.id, "joystick") == 0) return FieldDosConfig::cfg.joystickEnabled;
    if (std::strcmp(d.id, "sb16") == 0) return FieldDosConfig::cfg.sb16Enabled;
    if (std::strcmp(d.id, "gus") == 0) return FieldDosConfig::cfg.gusEnabled;
    if (std::strcmp(d.id, "hdd_c") == 0) {
        const FieldDrives::Drive* c = FieldDrives::byLetter('C');
        return c && c->ready;
    }
    if (std::strcmp(d.id, "floppy_a") == 0) {
        const FieldDrives::Drive* a = FieldDrives::byLetter('A');
        return a && a->ready;
    }
    if (std::strcmp(d.id, "cdrom_d") == 0) return FieldMscdex::numDrives > 0;
    return d.enabled;
}

inline void toggleId(const char* id) noexcept {
    if (!id) return;
    const std::string audioSec = std::string(FieldRegistry::kRoot) + "\\Software\\RTX-DOS\\Audio";
    if (std::strcmp(id, "mouse") == 0) {
        FieldDosConfig::cfg.mouseEnabled = !FieldDosConfig::cfg.mouseEnabled;
        FieldRegistry::setValue(audioSec, "Mouse", FieldDosConfig::cfg.mouseEnabled ? "1" : "0");
    } else if (std::strcmp(id, "joystick") == 0) {
        FieldDosConfig::cfg.joystickEnabled = !FieldDosConfig::cfg.joystickEnabled;
        FieldRegistry::setValue(audioSec, "Joystick", FieldDosConfig::cfg.joystickEnabled ? "1" : "0");
    } else if (std::strcmp(id, "sb16") == 0) {
        FieldDosConfig::cfg.sb16Enabled = !FieldDosConfig::cfg.sb16Enabled;
        FieldRegistry::setValue(audioSec, "SB16", FieldDosConfig::cfg.sb16Enabled ? "1" : "0");
    } else if (std::strcmp(id, "gus") == 0) {
        FieldDosConfig::cfg.gusEnabled = !FieldDosConfig::cfg.gusEnabled;
        FieldRegistry::setValue(audioSec, "GUS", FieldDosConfig::cfg.gusEnabled ? "1" : "0");
    }
    FieldRegistry::journal("TOGGLE", id);
}

} // namespace FieldDevicePolicy