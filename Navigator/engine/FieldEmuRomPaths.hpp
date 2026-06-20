#pragma once

// Lightweight ROM path table — safe for OS shell includes without SDL_mixer / WinApp.

#include <cstdint>

namespace FieldAmmoEmu {

enum class Kind : std::uint8_t { Nes, Snes, Genesis, Sms, A2600 };

struct RomProfile {
    const char* displayName;
    const char* romDir;
    const char* const* exts;
    int extCount;
    const char* openTitle;
};

inline const char* kSnesExts[] = { ".sfc", ".smc" };
inline const char* kGenesisExts[] = { ".md", ".bin", ".gen" };
inline const char* kSmsExts[] = { ".sms", ".bin" };
inline const char* kA2600Exts[] = { ".a26", ".bin" };
inline const char* kNesExts[] = { ".nes", ".rom" };

inline const RomProfile& profile(Kind k) noexcept {
    static const RomProfile kProfiles[] = {
        { "AmmoNES",     "C:\\NES\\",     kNesExts,     2, "Open NES ROM" },
        { "AmmoSNES",    "C:\\SNES\\",    kSnesExts,    2, "Open SNES ROM" },
        { "AmmoGenesis", "C:\\GENESIS\\", kGenesisExts, 3, "Open Genesis ROM" },
        { "AmmoSMS",     "C:\\SMS\\",     kSmsExts,     2, "Open SMS ROM" },
        { "AmmoA2600",   "C:\\A2600\\",   kA2600Exts,   2, "Open Atari ROM" },
    };
    const auto idx = static_cast<std::size_t>(k);
    return kProfiles[idx < 5 ? idx : 0];
}

} // namespace FieldAmmoEmu