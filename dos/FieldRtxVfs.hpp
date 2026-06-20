#pragma once

// RTX-AMMOS DOS 7 robust VFS — metadata, colorful listings, archive mounts.

#include "FieldAmmoFat.hpp"
#include "FieldAmmoVfs.hpp"
#include "FieldCdRom.hpp"
#include "FieldDos.hpp"
#include "FieldRtxMemory.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace FieldRtxVfs {

inline bool istrcmp(const char* a, const char* b) noexcept {
    if (!a || !b) return false;
    while (*a && *b) {
        if (std::toupper(static_cast<unsigned char>(*a))
            != std::toupper(static_cast<unsigned char>(*b)))
            return false;
        ++a; ++b;
    }
    return *a == *b;
}

struct FileMeta {
    std::string longDesc;
    std::string url;
};

struct RichEntry {
    char name[32]{};
    std::uint32_t size = 0;
    bool isDir = false;
    std::uint8_t vgaAttr = 0x07;
    const char* desc = nullptr;
    bool archive = false;
};

struct ZipItem {
    std::string fullPath;
    RichEntry entry;
};

struct ArchiveMount {
    std::string sourcePath;
    std::string mountPoint;
    std::vector<RichEntry> entries;
    std::vector<ZipItem> zipItems;
    bool isZip = false;
    bool isIso = false;
};

inline bool initialized = false;
inline std::unordered_map<std::string, FileMeta> metadata;
inline std::vector<ArchiveMount> mounts;

inline std::string upperKey(std::string s) {
    for (auto& c : s)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

inline std::string baseName(const char* path) noexcept {
    if (!path) return {};
    const char* p = path;
    const char* last = path;
    for (; *p; ++p)
        if (*p == '\\' || *p == '/') last = p + 1;
    return upperKey(last);
}

inline void setMeta(const std::string& key, const char* desc, const char* url = nullptr) {
    auto& m = metadata[upperKey(key)];
    if (desc) m.longDesc = desc;
    if (url) m.url = url;
}

inline bool lookupMeta(const std::string& name, FileMeta& out) noexcept {
    const auto it = metadata.find(upperKey(name));
    if (it == metadata.end()) return false;
    out = it->second;
    return true;
}

inline void seedBuiltinMeta() noexcept {
    setMeta("COMMAND.COM", "RTX-AMMOS DOS 7 shell — GPU Field Die interactive",
        "https://github.com/amouranth/rtx-ammos");
    setMeta("RTXKERNEL.SYS", "Supercore INT 2Fh field mux L0-L9");
    setMeta("AMMOFAT.SYS", "GPU-native FAT16 + HD mirror hot set");
    setMeta("FIELDLAY.TXT", "Field layer map — viewport, FAT, MSCDEX");
    setMeta("GOLDEN.TXT", "Golden Era Man+Machine manifesto");
    setMeta("IO.SYS", "RTX-DOS 7.0 boot IO — MS-DOS MIT lineage");
    setMeta("MSDOS.SYS", "RTX-DOS 7.0 kernel shell — version 7.0");
    setMeta("HIMEM.SYS", "XMS extended memory manager");
    setMeta("CONFIG.SYS", "Boot menu: RTX-DOS / Win31 / Win95 / Win98");
    setMeta("AUTOEXEC.BAT", "PATH + RTX welcome + AMMOFAT layer");
}

inline void parseDescriptIonLine(const char* line) noexcept {
    if (!line || !line[0] || line[0] == ';') return;
    std::string fname;
    std::string desc;
    const char* p = line;
    while (*p && *p != ' ' && *p != '\t') fname += *p++;
    while (*p == ' ' || *p == '\t') ++p;
    while (*p && *p != '\r' && *p != '\n') desc += *p++;
    while (!desc.empty() && (desc.back() == ' ' || desc.back() == '\t')) desc.pop_back();
    if (!fname.empty() && !desc.empty())
        setMeta(fname, desc.c_str());
}

inline void loadDescriptIon(const char* path = "C:\\DESCRIPT.ION") noexcept {
    std::vector<std::uint8_t> data;
    if (!FieldAmmoVfs::readPath(path, data) || data.empty()) return;
    std::string line;
    for (std::uint8_t b : data) {
        if (b == '\r') continue;
        if (b == '\n') {
            parseDescriptIonLine(line.c_str());
            line.clear();
        } else if (b >= 32 && b < 127)
            line += static_cast<char>(b);
    }
    if (!line.empty()) parseDescriptIonLine(line.c_str());
}

inline void loadDescriptIonForDir(const char* dirPath) noexcept {
    if (!dirPath) return;
    char ion[160];
    std::snprintf(ion, sizeof ion, "%s%sDESCRIPT.ION",
        dirPath, (dirPath[std::strlen(dirPath) - 1] == '\\') ? "" : "\\");
    loadDescriptIon(ion);
}

inline void loadManifestMeta() noexcept {
    const auto host = FieldDos::assetRoot() / "assets" / "dos" / "ammo" / "ammo_manifest.json";
    std::ifstream in(host);
    if (!in) return;
    std::string json((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::size_t pos = 0;
    while ((pos = json.find("\"desc\"", pos)) != std::string::npos) {
        const std::size_t keyEnd = json.rfind('"', pos > 0 ? pos - 1 : 0);
        const std::size_t keyStart = json.rfind('"', keyEnd > 0 ? keyEnd - 1 : 0);
        if (keyStart == std::string::npos || keyEnd <= keyStart) { ++pos; continue; }
        std::string key = json.substr(keyStart + 1, keyEnd - keyStart - 1);
        const std::size_t colon = json.find(':', pos);
        const std::size_t q0 = json.find('"', colon);
        const std::size_t q1 = json.find('"', q0 + 1);
        if (q0 == std::string::npos || q1 == std::string::npos) { ++pos; continue; }
        const std::string desc = json.substr(q0 + 1, q1 - q0 - 1);
        const auto slash = key.find_last_of('/');
        const std::string leaf = slash != std::string::npos ? key.substr(slash + 1) : key;
        if (!leaf.empty() && !desc.empty()) setMeta(leaf, desc.c_str());
        pos = q1 + 1;
    }
}

inline void vfsReload() noexcept {
    const auto savedMounts = mounts;
    metadata.clear();
    seedBuiltinMeta();
    loadDescriptIon();
    loadDescriptIonForDir("C:\\TOOLS");
    loadDescriptIonForDir("C:\\SOUND");
    loadDescriptIonForDir("C:\\DOS");
    loadManifestMeta();
    mounts = savedMounts;
    initialized = true;
}

inline std::uint8_t colorForName(const char* name, bool isDir) noexcept {
    if (isDir) return 0x1Bu;
    if (!name) return 0x07u;
    const char* dot = std::strrchr(name, '.');
    if (!dot) return 0x0Fu;
    const char* ext = dot + 1;
    if (istrcmp(ext, "COM") || istrcmp(ext, "EXE") || istrcmp(ext, "BAT")) return 0x0Au;
    if (istrcmp(ext, "SYS")) return 0x0Eu;
    if (istrcmp(ext, "TXT") || istrcmp(ext, "DOC")) return 0x0Fu;
    if (istrcmp(ext, "ZIP") || istrcmp(ext, "ISO") || istrcmp(ext, "IMG")) return 0x0Du;
    if (istrcmp(ext, "ASM") || istrcmp(ext, "C") || istrcmp(ext, "H")) return 0x0Bu;
    if (istrcmp(ext, "NES") || istrcmp(ext, "ROM")) return 0x05u;
    return 0x07u;
}

inline bool parseZipLocal(const std::uint8_t* data, std::size_t size,
                          std::vector<ZipItem>& items) noexcept {
    items.clear();
    if (!data || size < 30u) return false;
    std::size_t off = 0;
    int guard = 0;
    while (off + 30u <= size && guard++ < 4096) {
        const std::uint32_t sig = static_cast<std::uint32_t>(data[off])
            | (static_cast<std::uint32_t>(data[off + 1]) << 8)
            | (static_cast<std::uint32_t>(data[off + 2]) << 16)
            | (static_cast<std::uint32_t>(data[off + 3]) << 24);
        if (sig != 0x04034b50u) break;
        const std::uint16_t nameLen = static_cast<std::uint16_t>(data[off + 26] | (data[off + 27] << 8));
        const std::uint16_t extraLen = static_cast<std::uint16_t>(data[off + 28] | (data[off + 29] << 8));
        const std::uint32_t compSize = static_cast<std::uint32_t>(data[off + 18])
            | (static_cast<std::uint32_t>(data[off + 19]) << 8)
            | (static_cast<std::uint32_t>(data[off + 20]) << 16)
            | (static_cast<std::uint32_t>(data[off + 21]) << 24);
        if (off + 30u + nameLen > size) break;
        ZipItem zi{};
        zi.fullPath.assign(reinterpret_cast<const char*>(data + off + 30u), nameLen);
        for (auto& c : zi.fullPath)
            if (c == '/') c = '\\';
        if (zi.fullPath.empty()) {
            off += 30u + nameLen + extraLen + compSize;
            continue;
        }
        const bool isDir = !zi.fullPath.empty() && zi.fullPath.back() == '\\';
        const char* leaf = std::strrchr(zi.fullPath.c_str(), '\\');
        const char* disp = leaf ? leaf + 1 : zi.fullPath.c_str();
        std::snprintf(zi.entry.name, sizeof zi.entry.name, "%s", disp);
        if (isDir && zi.entry.name[0])
            zi.entry.name[std::strlen(zi.entry.name) - 1] = '\0';
        zi.entry.isDir = isDir;
        zi.entry.size = compSize;
        zi.entry.vgaAttr = colorForName(zi.entry.name, isDir);
        zi.entry.archive = true;
        items.push_back(std::move(zi));
        off += 30u + nameLen + extraLen + compSize;
    }
    return !items.empty();
}

inline std::string mountLabelFromPath(const char* path) noexcept {
    std::string base = baseName(path);
    const auto dot = base.find('.');
    if (dot != std::string::npos) base.erase(dot);
    return base;
}

inline bool mountZip(const char* zipPath, const char* mountName = nullptr) noexcept {
    std::vector<std::uint8_t> data;
    if (!zipPath || !FieldAmmoVfs::readPath(zipPath, data) || data.empty())
        return false;
    ArchiveMount m{};
    m.sourcePath = zipPath;
    m.isZip = true;
    const std::string label = mountName ? upperKey(mountName) : mountLabelFromPath(zipPath);
    m.mountPoint = "C:\\" + label;
    if (!parseZipLocal(data.data(), data.size(), m.zipItems)) return false;
    for (const auto& zi : m.zipItems)
        m.entries.push_back(zi.entry);
    mounts.erase(std::remove_if(mounts.begin(), mounts.end(),
        [&](const ArchiveMount& x) { return x.mountPoint == m.mountPoint; }), mounts.end());
    mounts.push_back(std::move(m));
    return true;
}

inline bool unmountZip(const char* label) noexcept {
    if (!label) return false;
    std::string target = upperKey(label);
    if (target.find(':') == std::string::npos)
        target = "C:\\" + target;
    const std::size_t before = mounts.size();
    mounts.erase(std::remove_if(mounts.begin(), mounts.end(),
        [&](const ArchiveMount& x) {
            return x.isZip && istrcmp(x.mountPoint.c_str(), target.c_str());
        }), mounts.end());
    return mounts.size() < before;
}

inline bool unmountIso() noexcept {
    const std::size_t before = mounts.size();
    mounts.erase(std::remove_if(mounts.begin(), mounts.end(),
        [](const ArchiveMount& x) { return x.isIso; }), mounts.end());
    FieldCdRom::unload();
    const auto tmp = FieldDos::assetRoot() / ".rtx_mount_tmp.iso";
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
    return mounts.size() < before || !FieldCdRom::ready;
}

inline void unmountAll() noexcept {
    mounts.clear();
    FieldCdRom::unload();
    const auto tmp = FieldDos::assetRoot() / ".rtx_mount_tmp.iso";
    std::error_code ec;
    std::filesystem::remove(tmp, ec);
}

inline void formatMountList(char* buf, std::size_t cap) noexcept {
    if (!buf || cap == 0) return;
    buf[0] = '\0';
    std::size_t n = 0;
    for (const auto& m : mounts) {
        char line[128];
        std::snprintf(line, sizeof line, "  %s <- %s (%zu entries)\r\n",
            m.mountPoint.c_str(), m.sourcePath.c_str(),
            m.isZip ? m.zipItems.size() : m.entries.size());
        const std::size_t len = std::strlen(line);
        if (n + len + 1 >= cap) break;
        std::memcpy(buf + n, line, len);
        n += len;
    }
    if (n == 0)
        std::snprintf(buf, cap, "  (no archive mounts)\r\n");
}

inline bool mountIsoFromC(const char* isoPath) noexcept {
    std::vector<std::uint8_t> data;
    if (!isoPath || !FieldAmmoVfs::readPath(isoPath, data) || data.size() < 2048u)
        return false;
    const auto tmp = FieldDos::assetRoot() / ".rtx_mount_tmp.iso";
    {
        std::ofstream out(tmp, std::ios::binary);
        if (!out) return false;
        out.write(reinterpret_cast<const char*>(data.data()),
            static_cast<std::streamsize>(data.size()));
    }
    if (!FieldCdRom::loadIso(tmp)) return false;
    ArchiveMount m{};
    m.sourcePath = isoPath;
    m.isIso = true;
    m.mountPoint = "D:\\";
    RichEntry e{};
    std::snprintf(e.name, sizeof e.name, "<ISO>");
    e.isDir = true;
    e.vgaAttr = 0x0Du;
    e.desc = FieldCdRom::volumeLabel.c_str();
    m.entries.push_back(e);
    mounts.push_back(std::move(m));
    return true;
}

inline void vfsInit() noexcept {
    if (initialized) return;
    vfsReload();
}

inline std::uint32_t getFreeUmbMem() noexcept {
    return static_cast<std::uint32_t>(FieldPlatform::UMB_FREE_KB) * 1024u;
}

inline std::uint32_t getFreeConvMem() noexcept {
    const std::uint32_t total = static_cast<std::uint32_t>(FieldRtxMemory::conventionalKb) * 1024u;
    constexpr std::uint32_t shellTax = 28u * 1024u;
    return total > shellTax ? total - shellTax : total / 2u;
}

inline bool wcMatch(const char* pat, const char* str) noexcept {
    if (!pat || !str) return false;
    if (!*pat) return !*str;
    if (*pat == '*')
        return wcMatch(pat + 1, str) || (*str && wcMatch(pat, str + 1));
    if (*pat == '?')
        return *str && wcMatch(pat + 1, str + 1);
    if (std::toupper(static_cast<unsigned char>(*pat))
        == std::toupper(static_cast<unsigned char>(*str)))
        return wcMatch(pat + 1, str + 1);
    return false;
}

inline bool matchWildcard(const char* pattern, const char* name) noexcept {
    if (!pattern || !name) return false;
    const char* bracket = std::strchr(pattern, '[');
    if (bracket) {
        const char* end = std::strchr(bracket, ']');
        if (end) {
            const char* dot = std::strrchr(name, '.');
            if (!dot) return false;
            const char* ext = dot + 1;
            for (const char* p = bracket + 1; p < end; ++p) {
                if (std::toupper(static_cast<unsigned char>(*p))
                    == std::toupper(static_cast<unsigned char>(ext[0])))
                    return true;
            }
            return false;
        }
    }
    if (std::strchr(pattern, '*') || std::strchr(pattern, '?'))
        return wcMatch(pattern, name);
    return istrcmp(pattern, name);
}

inline bool hasWildcard(const char* s) noexcept {
    return s && (std::strchr(s, '*') || std::strchr(s, '?') || std::strchr(s, '['));
}

inline void filterByWildcard(std::vector<RichEntry>& entries, const char* pattern) noexcept {
    if (!pattern || !hasWildcard(pattern)) return;
    entries.erase(std::remove_if(entries.begin(), entries.end(),
        [&](const RichEntry& e) { return !matchWildcard(pattern, e.name); }), entries.end());
}

inline bool listPathRich(const char* path, bool wide, bool color,
                         std::vector<RichEntry>& out) noexcept {
    out.clear();
    vfsInit();

    if (path) {
        for (const auto& m : mounts) {
            if (!m.isZip) continue;
            const bool exact = istrcmp(path, m.mountPoint.c_str());
            std::string prefix = m.mountPoint + "\\";
            const bool nested = std::strncmp(path, prefix.c_str(), prefix.size()) == 0;
            if (!exact && !nested) continue;
            std::string sub;
            if (nested && !exact)
                sub = std::string(path + prefix.size());
            for (const auto& zi : m.zipItems) {
                if (!sub.empty()) {
                    if (zi.fullPath.rfind(sub, 0) != 0) continue;
                }
                RichEntry copy = zi.entry;
                FileMeta meta;
                if (lookupMeta(copy.name, meta) && !meta.longDesc.empty())
                    copy.desc = meta.longDesc.c_str();
                if (!color) copy.vgaAttr = 0x07;
                out.push_back(copy);
            }
            (void)wide;
            return true;
        }
    }

    const char* fatPath = path ? path : "C:\\";
    loadDescriptIonForDir(fatPath);

    std::vector<FieldDos::FatRootEntry> fat;
    if (!FieldAmmoVfs::listPath(fatPath, fat)) return false;
    for (const auto& fe : fat) {
        RichEntry e{};
        std::snprintf(e.name, sizeof e.name, "%s", fe.name);
        e.size = fe.size;
        e.isDir = fe.isDir;
        e.vgaAttr = color ? colorForName(fe.name, fe.isDir) : 0x07u;
        FileMeta meta;
        if (lookupMeta(fe.name, meta)) {
            if (!meta.longDesc.empty()) e.desc = meta.longDesc.c_str();
        }
        out.push_back(e);
    }
    (void)wide;
    return true;
}

inline int vfsListDir(const char* path, bool wide, bool color) noexcept {
    std::vector<RichEntry> entries;
    if (!listPathRich(path, wide, color, entries)) return -1;
    return static_cast<int>(entries.size());
}

inline bool expandWildcardInDir(const char* dirPath, const char* pattern,
                                std::vector<std::string>& out) noexcept {
    out.clear();
    std::vector<RichEntry> entries;
    if (!listPathRich(dirPath, false, false, entries)) return false;
    for (const auto& e : entries) {
        if (e.isDir) continue;
        if (matchWildcard(pattern, e.name)) {
            char full[160];
            std::snprintf(full, sizeof full, "%s%s%s", dirPath,
                (dirPath[std::strlen(dirPath) - 1] == '\\') ? "" : "\\", e.name);
            out.emplace_back(full);
        }
    }
    return !out.empty();
}

inline RichEntry richFromName(const char* name, std::uint32_t size, bool isDir) noexcept {
    RichEntry e{};
    std::snprintf(e.name, sizeof e.name, "%s", name ? name : "");
    e.size = size;
    e.isDir = isDir;
    e.vgaAttr = colorForName(name, isDir);
    FileMeta meta;
    if (name && lookupMeta(name, meta) && !meta.longDesc.empty())
        e.desc = meta.longDesc.c_str();
    return e;
}

} // namespace FieldRtxVfs