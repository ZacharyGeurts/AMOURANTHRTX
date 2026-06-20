#pragma once

// RTX Registry 2026 — hierarchical HKRTX hive (INI on C:), journal, layer + assoc sync.

#include "FieldAmmoVfs.hpp"
#include "FieldJournal.hpp"
#include "FieldDosConfig.hpp"
#include "FieldExtensionMap.hpp"
#include "FieldPlatform.hpp"
#include "FieldRtxMemory.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace FieldRegistry {

using Section = std::map<std::string, std::string, std::less<>>;

inline std::map<std::string, Section, std::less<>> hive;
inline bool loaded = false;
inline bool dirty = false;

constexpr const char* kRegPath     = "C:\\TOOLS\\RTXREG.INI";
constexpr const char* kJournalPath = "C:\\TOOLS\\RTXREG.JRN";
constexpr const char* kRoot        = "HKRTX";

inline std::string normPath(std::string p) noexcept {
    for (auto& c : p)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    while (!p.empty() && (p.back() == '\\' || p.back() == '/')) p.pop_back();
    if (p.empty() || p == "HKRTX") return kRoot;
    if (p.rfind("HKRTX\\", 0) != 0) {
        if (p[0] == '\\') p = kRoot + p;
        else p = std::string(kRoot) + "\\" + p;
    }
    return p;
}

inline void journal(const char* action, const char* detail) noexcept {
    FieldJournal::append(kJournalPath, action, detail);
}

inline void setValue(const std::string& path, const std::string& key, const std::string& val) noexcept {
    const std::string sec = normPath(path);
    hive[sec][key] = val;
    dirty = true;
}

inline bool deleteValue(const std::string& path, const std::string& key) noexcept {
    const std::string sec = normPath(path);
    const auto sit = hive.find(sec);
    if (sit == hive.end()) return false;
    const auto kit = sit->second.find(key);
    if (kit == sit->second.end()) return false;
    sit->second.erase(kit);
    if (sit->second.empty()) hive.erase(sit);
    dirty = true;
    return true;
}

inline std::string getValue(const std::string& path, const std::string& key,
                            const std::string& def = {}) noexcept {
    const std::string sec = normPath(path);
    const auto sit = hive.find(sec);
    if (sit == hive.end()) return def;
    const auto kit = sit->second.find(key);
    return kit == sit->second.end() ? def : kit->second;
}

inline bool parseIni(const std::string& text) noexcept {
    hive.clear();
    std::string curSec;
    std::size_t pos = 0;
    while (pos < text.size()) {
        std::size_t end = text.find('\n', pos);
        std::string line = text.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        pos = (end == std::string::npos) ? text.size() : end + 1;
        while (!line.empty() && (line.back() == '\r' || line.back() == ' ')) line.pop_back();
        std::size_t i = 0;
        while (i < line.size() && line[i] == ' ') ++i;
        if (i >= line.size() || line[i] == ';' || line[i] == '#') continue;
        if (line[i] == '[') {
            const std::size_t close = line.find(']', i);
            if (close == std::string::npos) continue;
            curSec = normPath(line.substr(i + 1, close - i - 1));
            continue;
        }
        const std::size_t eq = line.find('=', i);
        if (eq == std::string::npos || curSec.empty()) continue;
        std::string key = line.substr(i, eq - i);
        std::string val = line.substr(eq + 1);
        while (!key.empty() && key.back() == ' ') key.pop_back();
        while (!val.empty() && val.front() == ' ') val.erase(0, 1);
        hive[curSec][key] = val;
    }
    return !hive.empty();
}

inline void seedDefaults() noexcept {
    hive.clear();
    setValue(std::string(kRoot) + "\\Machine\\Memory", "ReportedRamGB", "4");
    setValue(std::string(kRoot) + "\\Machine\\Memory", "GuestFastMB", "1");
    setValue(std::string(kRoot) + "\\Machine\\Memory", "BootConventionalKB", "512");
    setValue(std::string(kRoot) + "\\Machine\\Memory", "ConventionalKB", "512");
    setValue(std::string(kRoot) + "\\Machine\\Memory", "MaxConventionalKB", "640");
    setValue(std::string(kRoot) + "\\Machine\\Storage", "HdLogicalGB", "4");
    setValue(std::string(kRoot) + "\\Machine\\Storage", "RaidStripeKB", "64");
    setValue(std::string(kRoot) + "\\Machine\\Storage", "RaidTickBudgetKB", "256");
    setValue(std::string(kRoot) + "\\Software\\RTX-DOS", "Version", "7.0");
    setValue(std::string(kRoot) + "\\Software\\RTX-DOS", "Build", "2026");
    setValue(std::string(kRoot) + "\\Software\\RTX-DOS\\Shell", "Path",
              "C:\\;C:\\DOS;C:\\TOOLS;C:\\WINDOWS;C:\\WIN;C:\\AMMOCODE;C:\\QBASIC;C:\\SOUND");
    setValue(std::string(kRoot) + "\\Software\\RTX-DOS\\Shell", "Prompt", "$P$G");
    setValue(std::string(kRoot) + "\\Software\\RTX-DOS\\Audio", "SB16", "1");
    setValue(std::string(kRoot) + "\\Software\\RTX-DOS\\Audio", "GUS", "1");
    setValue(std::string(kRoot) + "\\Software\\RTX-DOS\\Audio", "Mouse", "1");
    setValue(std::string(kRoot) + "\\Software\\RTX-DOS\\Audio", "Joystick", "1");
    for (const auto& e : FieldExtensionMap::entries) {
        const std::string sec = std::string(kRoot) + "\\Associations\\" + e.ext;
        setValue(sec, "Handler", e.handler);
        setValue(sec, "Args", e.args);
        setValue(sec, "Desc", e.description);
    }
    if (FieldExtensionMap::entries.empty()) {
        FieldExtensionMap::seedDefaults();
        for (const auto& e : FieldExtensionMap::entries) {
            const std::string sec = std::string(kRoot) + "\\Associations\\" + e.ext;
            setValue(sec, "Handler", e.handler);
            setValue(sec, "Args", e.args);
            setValue(sec, "Desc", e.description);
        }
    }
    const char* layers[] = {"ram", "vga", "fat", "drives", "viewport", "audio", "mscdex", "input", "io", "bios"};
    for (const char* ly : layers)
        setValue(std::string(kRoot) + "\\Layers\\" + ly, "Enabled", "1");
    setValue(std::string(kRoot) + "\\User\\Desktop", "CommanderMouse", "1");
    setValue(std::string(kRoot) + "\\User\\Desktop", "ExtensionEditor", "F6");
    setValue(std::string(kRoot) + "\\User\\Desktop", "RestoreSession", "1");
    setValue(std::string(kRoot) + "\\User\\Desktop", "JournalCompactKB", "64");
    setValue(std::string(kRoot) + "\\User\\Desktop", "JournalMirror", "1");
    setValue(std::string(kRoot) + "\\User\\Desktop", "Theme", "Dark Chrome");
    setValue(std::string(kRoot) + "\\User\\Desktop", "ThemeIndex", "8");
    setValue(std::string(kRoot) + "\\User\\Desktop", "JournalFormats",
        "Folder,.EXE,.COM,.BAT,.NES,.ROM,.SFC,.MD,.SMS,.A26,.WAD,.ZIP,.ISO,.IMG,"
        ".TXT,.DOC,.ASM,.C,.FLD,.BAS,.PNG,.JPG,.HTML,.WAV,.MID,.JSON");
    dirty = true;
}

inline std::string serialize() noexcept {
    std::string out =
        "; RTX Registry 2026 v1 — HKRTX hierarchical configuration\r\n"
        "; Sections map to registry paths. Journal: RTXREG.JRN\r\n\r\n";
    std::vector<std::string> secs;
    for (const auto& [sec, _] : hive) secs.push_back(sec);
    std::sort(secs.begin(), secs.end());
    for (const auto& sec : secs) {
        out += "[" + sec + "]\r\n";
        const auto& kv = hive[sec];
        for (const auto& [k, v] : kv)
            out += k + "=" + v + "\r\n";
        out += "\r\n";
    }
    return out;
}

inline bool exportLegacyExtmap() noexcept {
    std::string body =
        "; RTX-DOS Extension Map — legacy export from HKRTX\\Associations\r\n"
        "; Primary store: C:\\TOOLS\\RTXREG.INI\r\n";
    syncExtensionMapFromRegistry();
    for (const auto& e : FieldExtensionMap::entries)
        body += FieldExtensionMap::formatLine(e) + "\r\n";
    return FieldAmmoVfs::writePath(FieldExtensionMap::kMapPath,
        reinterpret_cast<const std::uint8_t*>(body.data()), body.size());
}

inline bool save() noexcept {
    const std::string body = serialize();
    journal("SAVE", kRegPath);
    dirty = false;
    const bool ok = FieldAmmoVfs::writePath(kRegPath,
        reinterpret_cast<const std::uint8_t*>(body.data()), body.size());
    exportLegacyExtmap();
    return ok;
}

inline bool load() noexcept {
    hive.clear();
    std::vector<std::uint8_t> data;
    if (FieldAmmoVfs::readPath(kRegPath, data) && !data.empty()) {
        if (parseIni(std::string(reinterpret_cast<const char*>(data.data()), data.size()))) {
            loaded = true;
            dirty = false;
            return true;
        }
    }
    FieldExtensionMap::entries.clear();
    FieldExtensionMap::loaded = false;
    FieldExtensionMap::load();
    if (!FieldExtensionMap::entries.empty()) {
        for (const auto& e : FieldExtensionMap::entries) {
            const std::string sec = std::string(kRoot) + "\\Associations\\" + e.ext;
            setValue(sec, "Handler", e.handler);
            setValue(sec, "Args", e.args);
            setValue(sec, "Desc", e.description);
        }
        setValue(std::string(kRoot) + "\\Machine\\Memory", "ReportedRamGB", "4");
        setValue(std::string(kRoot) + "\\Software\\RTX-DOS", "Version", "7.0");
        setValue(std::string(kRoot) + "\\Software\\RTX-DOS", "Build", "2026");
        journal("MIGRATE", "EXTMAP.TXT -> RTXREG.INI");
    } else {
        seedDefaults();
    }
    loaded = true;
    save();
    return true;
}

inline void ensure() noexcept {
    if (!loaded) load();
}

inline void syncExtensionMapFromRegistry() noexcept {
    ensure();
    FieldExtensionMap::entries.clear();
    const std::string prefix = normPath(std::string(kRoot) + "\\Associations") + "\\";
    for (const auto& [sec, kv] : hive) {
        if (sec.rfind(prefix, 0) != 0) continue;
        std::string ext = sec.substr(prefix.size());
        if (ext.empty() || ext[0] != '.') continue;
        FieldExtensionMap::Entry e;
        e.ext = ext;
        const auto h = kv.find("Handler");
        const auto a = kv.find("Args");
        const auto d = kv.find("Desc");
        e.handler = h != kv.end() ? h->second : "";
        e.args    = a != kv.end() ? a->second : "%1";
        e.description = d != kv.end() ? d->second : "";
        FieldExtensionMap::entries.push_back(std::move(e));
    }
    if (FieldExtensionMap::entries.empty()) FieldExtensionMap::seedDefaults();
    FieldExtensionMap::loaded = true;
}

inline void syncExtensionMapToRegistry() noexcept {
    ensure();
    for (const auto& e : FieldExtensionMap::entries) {
        const std::string sec = std::string(kRoot) + "\\Associations\\" + e.ext;
        setValue(sec, "Handler", e.handler);
        setValue(sec, "Args", e.args);
        setValue(sec, "Desc", e.description);
    }
}

inline void applyMemoryConfig() noexcept {
    ensure();
    const std::string sec = std::string(kRoot) + "\\Machine\\Memory";
    FieldRtxMemory::bootConventionalKb = FieldRtxMemory::parseKb(
        getValue(sec, "BootConventionalKB", ""),
        FieldRtxMemory::parseKb(getValue(sec, "ConventionalKB", ""),
                                FieldRtxMemory::kBootConventionalKb));
    FieldRtxMemory::bootConventionalKb = std::clamp(
        FieldRtxMemory::bootConventionalKb,
        FieldRtxMemory::kMinConventionalKb, FieldRtxMemory::kMaxConventionalKb);
    FieldRtxMemory::maxConventionalKb = FieldRtxMemory::parseKb(
        getValue(sec, "MaxConventionalKB", ""), FieldRtxMemory::kMaxConventionalKb);
    FieldRtxMemory::maxConventionalKb = std::clamp(
        FieldRtxMemory::maxConventionalKb,
        FieldRtxMemory::bootConventionalKb, FieldRtxMemory::kMaxConventionalKb);
    FieldRtxMemory::conventionalKb = FieldRtxMemory::bootConventionalKb;
    FieldRtxMemory::guestFastMb = FieldRtxMemory::parseMb(
        getValue(sec, "GuestFastMB", ""), FieldRtxMemory::kBootGuestFastMb);
    FieldRtxMemory::guestFastMb = std::clamp(FieldRtxMemory::guestFastMb, 1u, 64u);
    FieldRtxMemory::reportedRamGb = FieldRtxMemory::parseMb(
        getValue(sec, "ReportedRamGB", ""), 4u);
    FieldRtxMemory::reportedRamGb = std::clamp(FieldRtxMemory::reportedRamGb, 1u, 64u);
    FieldRtxMemory::grown = false;
    FieldRtxMemory::resetTree();
}

inline void applyDosConfig() noexcept {
    ensure();
    applyMemoryConfig();
    auto flag = [&](const char* key, bool& out) {
        const std::string v = getValue(std::string(kRoot) + "\\Software\\RTX-DOS\\Audio",
                                       key, out ? "1" : "0");
        out = (v == "1" || v == "Y" || v == "YES" || v == "ON");
    };
    flag("SB16", FieldDosConfig::cfg.sb16Enabled);
    flag("GUS", FieldDosConfig::cfg.gusEnabled);
    flag("Mouse", FieldDosConfig::cfg.mouseEnabled);
    flag("Joystick", FieldDosConfig::cfg.joystickEnabled);
}

inline std::vector<std::string> listSections(const std::string& prefix = kRoot) noexcept {
    ensure();
    const std::string pre = normPath(prefix);
    std::vector<std::string> out;
    for (const auto& [sec, _] : hive) {
        if (sec.rfind(pre, 0) == 0) out.push_back(sec);
    }
    std::sort(out.begin(), out.end());
    return out;
}

inline std::vector<std::string> listSectionNames(const std::string& prefix = kRoot) noexcept {
    const auto all = listSections(prefix);
    std::vector<std::string> names;
    const std::string pre = normPath(prefix);
    const std::string preBack = pre + "\\";
    for (const auto& sec : all) {
        if (sec == pre) { names.push_back("(root)"); continue; }
        if (sec.rfind(preBack, 0) == 0) names.push_back(sec.substr(preBack.size()));
        else names.push_back(sec);
    }
    return names;
}

inline void tick(float /*dt*/) noexcept {
    if (dirty) save();
}

inline void init() noexcept {
    const bool first = !loaded;
    if (!loaded) load();
    syncExtensionMapFromRegistry();
    applyDosConfig();
    if (first) journal("BOOT", "RTX Registry loaded");
}

} // namespace FieldRegistry