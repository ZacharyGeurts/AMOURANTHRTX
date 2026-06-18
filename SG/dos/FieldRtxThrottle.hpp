#pragma once

// RTX-AMMOS automagic throttle — deep MZ/PE header analysis for perfect era speeds.

#include "FieldAmmoFat.hpp"
#include "FieldAmmoVfs.hpp"
#include "FieldDosConfig.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace FieldRtxThrottle {

enum class Profile : std::uint8_t {
    FullSpeed  = 0,
    ClassicMz  = 1,
    LegacyMz   = 2,
};

struct Analysis {
    Profile profile = Profile::FullSpeed;
    std::uint32_t cycles = 0;
    const char* label = "default";
    bool isMz = false;
    bool isPe = false;
    std::uint16_t pages = 0;
    std::uint16_t reloc = 0;
};

inline constexpr std::uint32_t kDefaultCycles   = 131'072u;
inline constexpr std::uint32_t kClassicCycles   = 65'536u;
inline constexpr std::uint32_t kLegacyCycles    = 22'000u;

inline Analysis lastAnalysis{};
inline bool autoEnabled = true;

inline Analysis analyzeBytes(const std::uint8_t* data, std::size_t size) noexcept {
    Analysis a{};
    if (!data || size < 64u) {
        a.cycles = 0;
        a.label = "unknown";
        return a;
    }

    const std::uint16_t mz = static_cast<std::uint16_t>(data[0] | (data[1] << 8));
    if (mz != 0x5A4Du && mz != 0x4D5Au) {
        a.label = "not MZ";
        return a;
    }
    a.isMz = true;

    a.pages = static_cast<std::uint16_t>(data[4] | (data[5] << 8));
    a.reloc = static_cast<std::uint16_t>(data[0x18] | (data[0x19] << 8));
    const std::uint32_t peOff = static_cast<std::uint32_t>(data[0x3C] | (data[0x3D] << 8)
        | (data[0x3E] << 16) | (data[0x3F] << 24));

    if (peOff + 4u <= size) {
        if (std::memcmp(data + peOff, "PE\0\0", 4) == 0) {
            a.isPe = true;
            a.profile = Profile::FullSpeed;
            a.cycles = 0;
            a.label = "Modern PE — full speed";
            return a;
        }
    }

    if (a.reloc < 0x40u || a.pages < 64u) {
        a.profile = Profile::LegacyMz;
        a.cycles = kLegacyCycles;
        a.label = "Very old DOS MZ — legacy speed restored";
    } else {
        a.profile = Profile::ClassicMz;
        a.cycles = kClassicCycles;
        a.label = "Classic 16-bit MZ — balanced throttle";
    }
    return a;
}

inline Analysis analyzePath(const char* path) noexcept {
    std::vector<std::uint8_t> data;
    if (!path || !FieldAmmoVfs::readPath(path, data) || data.empty())
        return Analysis{};
    return analyzeBytes(data.data(), data.size());
}

inline void apply(const Analysis& a) noexcept {
    lastAnalysis = a;
    if (!autoEnabled) return;
    if (a.cycles == 0u)
        FieldDosConfig::cfg.cyclesRun = kDefaultCycles;
    else
        FieldDosConfig::cfg.cyclesRun = a.cycles;
}

inline void fieldSetCycles(std::uint32_t cycles) noexcept {
    if (cycles == 0u)
        FieldDosConfig::cfg.cyclesRun = kDefaultCycles;
    else
        FieldDosConfig::cfg.cyclesRun = cycles;
}

inline void autoThrottleDeep(const char* filename) noexcept {
    if (!filename || !autoEnabled) {
        fieldSetCycles(0);
        return;
    }
    char path[160]{};
    if (std::strchr(filename, ':') || std::strchr(filename, '\\') || std::strchr(filename, '/'))
        std::snprintf(path, sizeof path, "%s", filename);
    else
        std::snprintf(path, sizeof path, "C:\\%s", filename);
    const Analysis a = analyzePath(path);
    apply(a);
}

inline std::uint32_t activeCycles() noexcept {
    return FieldDosConfig::cfg.cyclesRun;
}

inline const char* activeLabel() noexcept {
    if (!lastAnalysis.isMz) return "full speed (default)";
    return lastAnalysis.label;
}

} // namespace FieldRtxThrottle