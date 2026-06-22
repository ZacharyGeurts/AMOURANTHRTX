#pragma once

#include "FieldAosAppJournal.hpp"
#include "FieldRegistry.hpp"
#include "FieldRtxThemes.hpp"

#include <cstdlib>

namespace FieldAosAppJournal {

inline bool flagOn(const std::string& v) noexcept {
    return v == "1" || v == "Y" || v == "YES" || v == "ON";
}

inline void loadConfigFromRegistry() noexcept {
    FieldRegistry::ensure();
    cfgRestoreSession = flagOn(
        FieldRegistry::getValue("HKRTX\\User\\Desktop", "RestoreSession", "1"));
    cfgMirrorHost = flagOn(
        FieldRegistry::getValue("HKRTX\\User\\Desktop", "JournalMirror", "1"));
}

inline int themeIndexFromRegistry() noexcept {
    FieldRegistry::ensure();
    const std::string idx = FieldRegistry::getValue(
        "HKRTX\\User\\Desktop", "ThemeIndex", "");
    if (!idx.empty()) {
        const int v = std::atoi(idx.c_str());
        if (v >= 0 && v < FieldRtxThemes::kPresetCount) return v;
    }
    const std::string name = FieldRegistry::getValue(
        "HKRTX\\User\\Desktop", "Theme", "Ammo Void");
    const int byName = FieldRtxThemes::indexByName(name.c_str());
    return byName >= 0 ? byName : FieldRtxThemes::indexByName("Ammo Void");
}

} // namespace FieldAosAppJournal