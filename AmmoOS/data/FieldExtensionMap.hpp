#pragma once

// RTX-DOS extension association journal — handler path, args template, launch routing.

#include "FieldAmmoVfs.hpp"
#include "FieldJournal.hpp"
#include "FieldAmouranthLaunch.hpp"
#include "FieldAosAppJournal.hpp"
#include "FieldDos.hpp"
#include "FieldPlatform.hpp"

namespace FieldRegistry {
void syncExtensionMapFromRegistry() noexcept;
void syncExtensionMapToRegistry() noexcept;
bool save() noexcept;
void ensure() noexcept;
} // namespace FieldRegistry

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace FieldExtensionMap {

struct Entry {
    std::string ext;         // ".ZIP" uppercase
    std::string handler;     // "C:\TOOLS\AMMOZIP.COM" or shell command
    std::string args;        // "%1" "%2" …
    std::string description;
};

inline std::vector<Entry> entries;
inline bool loaded = false;

constexpr const char* kMapPath = "C:\\TOOLS\\EXTMAP.TXT";
constexpr const char* kJournalPath = "C:\\TOOLS\\EXTJOURN.TXT";

inline std::string upperExt(const char* name) noexcept {
    if (!name) return {};
    const char* dot = std::strrchr(name, '.');
    if (!dot || !dot[1]) return {};
    std::string e = dot;
    for (auto& c : e) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return e;
}

inline void appendJournal(const char* action, const char* detail) noexcept {
    FieldJournal::append(kJournalPath, action, detail);
}

inline void seedDefaults() noexcept {
    entries = {
        {".COM",  "",                    "%1",     "DOS command file"},
        {".EXE",  "",                    "%1",     "DOS executable"},
        {".BAT",  "",                    "%1",     "Batch script"},
        {".SYS",  "TYPE",                "%1",     "System driver"},
        {".TXT",  "TYPE",                "%1",     "Text document"},
        {".DOC",  "TYPE",                "%1",     "Document"},
        {".INI",  "EDIT",                "%1",     "Configuration"},
        {".CFG",  "EDIT",                "%1",     "Configuration"},
        {".ZIP",  "C:\\TOOLS\\AMMOZIP.COM", "%1",  "ZIP archive — AMMOZIP extract"},
        {".NES",  "NES",                 "%1",     "iNES ROM — AmmoNES emulator"},
        {".ROM",  "NES",                 "%1",     "NES ROM — AmmoNES emulator"},
        {".SFC",  "SNES",                "%1",     "SNES ROM — AmmoSNES emulator"},
        {".SMC",  "SNES",                "%1",     "SNES ROM — AmmoSNES emulator"},
        {".MD",   "GENESIS",             "%1",     "Genesis ROM — AmmoGenesis"},
        {".GEN",  "GENESIS",             "%1",     "Genesis ROM — AmmoGenesis"},
        {".SMS",  "SMS",                 "%1",     "Master System ROM — AmmoSMS"},
        {".A26",  "A2600",               "%1",     "Atari 2600 ROM — AmmoA2600"},
        {".ISO",  "MOUNT",               "%1",     "CD-ROM image"},
        {".IMG",  "MOUNT",               "%1",     "Disk image"},
        {".WAD",  "C:\\GAMES\\DOOM\\DOOM.EXE", "%1", "Doom engine WAD"},
        {".WL6",  "C:\\GAMES\\WOLF3D\\WOLF3D.EXE", "%1", "Wolf3D data"},
        {".WL1",  "C:\\GAMES\\WOLF3D\\WOLF3D.EXE", "%1", "Wolf3D map/audio"},
        {".CK4",  "C:\\GAMES\\KEEN4\\KEEN4E.EXE", "%1", "Commander Keen 4"},
        {".CK3",  "C:\\GAMES\\KEEN3\\KEEN3.EXE", "%1", "Commander Keen 3"},
        {".ASM",  "AMMOCODE",            "%1",     "Assembly source"},
        {".C",    "AMMOCODE",            "%1",     "C source"},
        {".H",    "AMMOCODE",            "%1",     "Header source"},
        {".INC",  "AMMOCODE",            "%1",     "Include file"},
        {".FLD",  "FIELDC",              "%1",     "Field script"},
        {".BAS",  "QBASIC",              "%1",     "BASIC program"},
        {".WAV",  "SOUND",               "%1",     "Wave audio"},
        {".MID",  "SOUND",               "%1",     "MIDI music"},
        {".MP3",  "SOUND",               "%1",     "Compressed audio"},
        {".OBJ",  "AMMOLINK",            "%1",     "Object module"},
        {".MAP",  "TYPE",                "%1",     "Linker map"},
        {".LST",  "TYPE",                "%1",     "Listing file"},
        {".LOG",  "TYPE",                "%1",     "Log file"},
        {".JSON", "TYPE",                "%1",     "JSON data"},
        {".PY",   "TYPE",                "%1",     "Python script (host)"},
        {".HTML", "BROWSER",             "%1",     "Web page"},
        {".HTM",  "BROWSER",             "%1",     "Web page"},
        {".PIF",  "",                    "%1",     "Program info"},
        {".LNK",  "TYPE",                "%1",     "Shortcut metadata"},
    };
    loaded = true;
}

inline bool parseLine(const std::string& line, Entry& out) noexcept {
    if (line.empty() || line[0] == '#' || line[0] == ';') return false;
    std::string s = line;
    while (!s.empty() && (s.back() == '\r' || s.back() == ' ')) s.pop_back();
    std::vector<std::string> parts;
    std::string cur;
    for (char c : s) {
        if (c == ';') {
            if (!cur.empty()) { parts.push_back(cur); cur.clear(); }
            break;
        }
        if (c == '|') {
            if (!cur.empty()) { parts.push_back(cur); cur.clear(); }
        } else
            cur += c;
    }
    if (!cur.empty()) parts.push_back(cur);
    if (parts.size() < 2) return false;
    out.ext = parts[0];
    if (out.ext[0] != '.') out.ext = "." + out.ext;
    for (auto& c : out.ext) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    out.handler = parts.size() > 1 ? parts[1] : "";
    out.args    = parts.size() > 2 ? parts[2] : "%1";
    out.description = parts.size() > 3 ? parts[3] : "";
    return true;
}

inline std::string formatLine(const Entry& e) noexcept {
    return e.ext + " | " + e.handler + " | " + e.args + " | " + e.description;
}

inline void load() noexcept {
    entries.clear();
    std::vector<std::uint8_t> data;
    if (!FieldAmmoVfs::readPath(kMapPath, data) || data.empty()) {
        seedDefaults();
        return;
    }
    std::string text(reinterpret_cast<const char*>(data.data()), data.size());
    std::size_t pos = 0;
    while (pos < text.size()) {
        const std::size_t end = text.find('\n', pos);
        const std::string line = text.substr(pos, end == std::string::npos ? std::string::npos : end - pos);
        pos = (end == std::string::npos) ? text.size() : end + 1;
        Entry e;
        if (parseLine(line, e)) entries.push_back(std::move(e));
    }
    if (entries.empty()) seedDefaults();
    else loaded = true;
}

inline bool save() noexcept {
    FieldRegistry::syncExtensionMapToRegistry();
    appendJournal("SAVE", kMapPath);
    return FieldRegistry::save();
}

inline void ensure() noexcept {
    if (!loaded) {
        FieldRegistry::ensure();
        FieldRegistry::syncExtensionMapFromRegistry();
    }
}

inline const Entry* find(const std::string& ext) noexcept {
    ensure();
    for (const auto& e : entries)
        if (e.ext == ext) return &e;
    return nullptr;
}

inline const Entry* findForName(const char* filename) noexcept {
    return find(upperExt(filename));
}

inline std::string expandArgs(const std::string& tmpl, const std::string& filePath,
                              const std::vector<std::string>& extra) noexcept {
    std::string out = tmpl;
    auto repl = [&](const char* tok, const std::string& val) {
        std::size_t p = 0;
        while ((p = out.find(tok, p)) != std::string::npos) {
            out.replace(p, std::strlen(tok), val);
            p += val.size();
        }
    };
    repl("%1", filePath);
    repl("%F", filePath);
    repl("%2", extra.empty() ? "" : extra[0]);
    repl("%3", extra.size() > 1 ? extra[1] : "");
    return out;
}

inline std::string buildLaunchLine(const char* filePath,
                                   const std::vector<std::string>& extra = {}) noexcept {
    if (!filePath) return {};
    const Entry* ent = findForName(filePath);
    const std::string ext = upperExt(filePath);
    if (!ent) {
        if (ext == ".COM" || ext == ".EXE" || ext == ".BAT") return filePath;
        return "TYPE " + std::string(filePath);
    }
    if (ent->handler.empty()) {
        if (ext == ".COM" || ext == ".EXE" || ext == ".BAT") return filePath;
        return filePath;
    }
    const std::string args = expandArgs(ent->args.empty() ? "%1" : ent->args, filePath, extra);
    if (ent->handler.find('\\') != std::string::npos || ent->handler.find(':') != std::string::npos)
        return ent->handler + " " + args;
    return ent->handler + " " + args;
}

inline bool launchFile(std::uint8_t* ram, const char* filePath,
                       const std::vector<std::string>& extra = {}) noexcept {
    if (!filePath) return false;
    ensure();
    const std::string line = buildLaunchLine(filePath, extra);
    appendJournal("LAUNCH", line.c_str());
    char detail[FieldAosAppJournal::TEXT_LEN + 1];
    std::snprintf(detail, sizeof detail, ">%s", filePath);
    FieldAosAppJournal::recordLinkedAction(FieldAosAppJournal::journalProgId,
        FieldAosAppIdentity::AppId::FileCmd, "AmmoFiles", detail);
    FieldAmouranthLaunch::queue(line);
    (void)ram;
    return true;
}

inline void addEntry(const Entry& e) noexcept {
    ensure();
    for (auto& x : entries) {
        if (x.ext == e.ext) { x = e; save(); return; }
    }
    entries.push_back(e);
    save();
}

inline void removeAt(std::size_t idx) noexcept {
    ensure();
    if (idx < entries.size()) {
        entries.erase(entries.begin() + static_cast<std::ptrdiff_t>(idx));
        save();
    }
}

} // namespace FieldExtensionMap