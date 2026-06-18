#pragma once

// AMMOFAT path VFS — mkdir/rmdir/delete/copy/rename/list on C: subdirectories.

#include "FieldAmmoFat.hpp"
#include "FieldDos.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace FieldAmmoVfs {

constexpr std::uint8_t kDirAttr = 0x10u;
constexpr std::uint8_t kFileAttr = 0x20u;
constexpr std::uint8_t kDeleted = 0xE5u;

struct DirView {
    std::uint8_t* table = nullptr;
    std::uint32_t slots = 0;
    std::uint16_t ownCluster = 0;
    std::uint16_t parentCluster = 0;
    bool root = false;
};

inline void splitDosPath(const char* path, char drive, std::vector<std::string>& parts) {
    parts.clear();
    if (!path) return;
    const char* p = path;
    if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':')
        p += 2;
    while (*p == '\\' || *p == '/') ++p;
    std::string comp;
    for (; *p; ++p) {
        if (*p == '\\' || *p == '/') {
            if (!comp.empty()) {
                for (auto& c : comp) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                parts.push_back(comp);
                comp.clear();
            }
        } else {
            comp += *p;
        }
    }
    if (!comp.empty()) {
        for (auto& c : comp) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        parts.push_back(comp);
    }
    (void)drive;
}

inline bool nameTo83(const std::string& name, char out[11]) noexcept {
    FieldAmmoFat::toDosName(name.c_str(), out);
    return name[0] != '\0';
}

inline bool openRoot(DirView& view) noexcept {
    if (!FieldAmmoFat::mounted && !FieldAmmoFat::mount()) return false;
    auto* vol = FieldDos::hdImage.data() + FieldAmmoFat::geo.volOffset;
    const std::uint32_t rootOff =
        (FieldAmmoFat::geo.reserved + static_cast<std::uint32_t>(FieldAmmoFat::geo.fats) * FieldAmmoFat::geo.spf)
        * FieldAmmoFat::geo.bps;
    view.table = vol + rootOff;
    view.slots = FieldAmmoFat::geo.rootEnt;
    view.ownCluster = 0;
    view.parentCluster = 0;
    view.root = true;
    return true;
}

inline bool loadDirCluster(std::uint16_t cluster, std::uint16_t parentCluster, DirView& view) noexcept {
    auto* data = FieldAmmoFat::clusterData(cluster);
    if (!data) return false;
    view.table = data;
    view.slots = static_cast<std::uint32_t>(FieldAmmoFat::geo.spc) * FieldAmmoFat::geo.bps / 32u;
    view.ownCluster = cluster;
    view.parentCluster = parentCluster;
    view.root = false;
    return true;
}

inline bool findInView(const DirView& view, const char name83[11], std::uint32_t& index,
                       bool wantDir) noexcept {
    for (std::uint32_t i = 0; i < view.slots; ++i) {
        auto* e = view.table + i * 32u;
        if (e[0] == 0) break;
        if (e[0] == kDeleted) continue;
        const bool isDir = (e[11] & kDirAttr) != 0;
        if (wantDir != isDir) continue;
        if (std::memcmp(e, name83, 11) == 0) {
            index = i;
            return true;
        }
    }
    return false;
}

inline std::uint32_t freeSlot(const DirView& view) noexcept {
    std::uint32_t last = 0;
    for (std::uint32_t i = 0; i < view.slots; ++i) {
        auto* e = view.table + i * 32u;
        if (e[0] == 0) return i;
        if (e[0] == kDeleted) return i;
        last = i + 1u;
    }
    return last < view.slots ? last : 0xFFFFFFFFu;
}

inline void writeDotEntries(std::uint8_t* table, std::uint16_t own, std::uint16_t parent) noexcept {
    std::memset(table, 0, 64u);
    table[0] = '.';
    table[11] = kDirAttr;
    table[26] = static_cast<std::uint8_t>(own & 0xFFu);
    table[27] = static_cast<std::uint8_t>((own >> 8) & 0xFFu);
    table[32] = '.';
    table[33] = '.';
    table[43] = kDirAttr;
    table[58] = static_cast<std::uint8_t>(parent & 0xFFu);
    table[59] = static_cast<std::uint8_t>((parent >> 8) & 0xFFu);
    FieldAmmoFat::stampDirEntry(table);
    FieldAmmoFat::stampDirEntry(table + 32u);
}

inline void freeChain(std::uint16_t first) noexcept {
    std::uint16_t c = first;
    std::uint32_t guard = 0;
    while (c >= 2 && c < 0xFFF0u && guard++ < FieldAmmoFat::geo.totalClusters + 4) {
        const std::uint16_t next = FieldAmmoFat::fatGet(c);
        FieldAmmoFat::fatSet(c, 0);
        c = next;
    }
}

inline bool openDirPath(const char* path, DirView& view, std::string& leaf,
                        bool needLeaf) noexcept {
    if (!openRoot(view)) return false;
    std::vector<std::string> parts;
    splitDosPath(path, 'C', parts);
    if (parts.empty()) return !needLeaf;
    if (needLeaf) {
        leaf = parts.back();
        parts.pop_back();
    }
    std::uint16_t parentClus = 0;
    for (const auto& comp : parts) {
        char n83[11]{};
        if (!nameTo83(comp, n83)) return false;
        std::uint32_t idx = 0;
        if (!findInView(view, n83, idx, true)) return false;
        auto* e = view.table + idx * 32u;
        const std::uint16_t clus = static_cast<std::uint16_t>(e[26] | (e[27] << 8));
        if (clus < 2) return false;
        parentClus = view.ownCluster;
        if (!loadDirCluster(clus, view.root ? 0 : view.ownCluster, view)) return false;
        (void)parentClus;
    }
    return true;
}

inline bool listPath(const char* path, std::vector<FieldDos::FatRootEntry>& out) noexcept {
    out.clear();
    DirView view{};
    std::string leaf;
    if (!openDirPath(path, view, leaf, false)) return false;
    for (std::uint32_t i = 0; i < view.slots; ++i) {
        auto* e = view.table + i * 32u;
        if (e[0] == 0) break;
        if (e[0] == kDeleted) continue;
        if (e[0] == '.') continue;
        FieldDos::FatRootEntry fe{};
        char stem[9]{}, ext[4]{};
        for (int j = 0; j < 8; ++j) stem[j] = e[j] == ' ' ? '\0' : static_cast<char>(e[j]);
        for (int j = 0; j < 3; ++j) ext[j] = e[8 + j] == ' ' ? '\0' : static_cast<char>(e[8 + j]);
        if (ext[0]) std::snprintf(fe.name, sizeof fe.name, "%s.%s", stem, ext);
        else std::snprintf(fe.name, sizeof fe.name, "%s", stem);
        fe.size = static_cast<std::uint32_t>(e[28] | (e[29] << 8) | (e[30] << 16) | (e[31] << 24));
        fe.isDir = (e[11] & kDirAttr) != 0;
        out.push_back(fe);
    }
    return true;
}

inline bool mkdirPath(const char* path) noexcept {
    DirView view{};
    std::string leaf;
    if (!openDirPath(path, view, leaf, true) || leaf.empty()) return false;
    char n83[11]{};
    if (!nameTo83(leaf, n83)) return false;
    std::uint32_t idx = 0;
    if (findInView(view, n83, idx, false) || findInView(view, n83, idx, true))
        return false;
    const std::uint32_t slot = freeSlot(view);
    if (slot == 0xFFFFFFFFu) return false;
    const std::uint16_t dirClus = FieldAmmoFat::allocCluster();
    if (dirClus >= 0xFFF0u) return false;
    writeDotEntries(FieldAmmoFat::clusterData(dirClus), dirClus,
        view.root ? 0 : view.ownCluster);
    auto* e = view.table + slot * 32u;
    std::memset(e, 0, 32u);
    std::memcpy(e, n83, 11);
    e[11] = kDirAttr;
    e[26] = static_cast<std::uint8_t>(dirClus & 0xFFu);
    e[27] = static_cast<std::uint8_t>((dirClus >> 8) & 0xFFu);
    FieldAmmoFat::stampDirEntry(e);
    return FieldDos::saveHdImage();
}

inline bool dirEmpty(const DirView& view) noexcept {
    int count = 0;
    for (std::uint32_t i = 0; i < view.slots; ++i) {
        auto* e = view.table + i * 32u;
        if (e[0] == 0) break;
        if (e[0] == kDeleted) continue;
        if (e[0] == '.') continue;
        ++count;
    }
    return count == 0;
}

inline bool rmdirPath(const char* path) noexcept {
    DirView parent{};
    std::string leaf;
    if (!openDirPath(path, parent, leaf, true) || leaf.empty()) return false;
    char n83[11]{};
    if (!nameTo83(leaf, n83)) return false;
    std::uint32_t idx = 0;
    if (!findInView(parent, n83, idx, true)) return false;
    auto* e = parent.table + idx * 32u;
    const std::uint16_t clus = static_cast<std::uint16_t>(e[26] | (e[27] << 8));
    DirView sub{};
    if (!loadDirCluster(clus, parent.root ? 0 : parent.ownCluster, sub)) return false;
    if (!dirEmpty(sub)) return false;
    freeChain(clus);
    e[0] = kDeleted;
    return FieldDos::saveHdImage();
}

inline bool deletePath(const char* path) noexcept {
    DirView view{};
    std::string leaf;
    if (!openDirPath(path, view, leaf, true) || leaf.empty()) return false;
    char n83[11]{};
    if (!nameTo83(leaf, n83)) return false;
    std::uint32_t idx = 0;
    if (!findInView(view, n83, idx, false)) return false;
    auto* e = view.table + idx * 32u;
    const std::uint16_t clus = static_cast<std::uint16_t>(e[26] | (e[27] << 8));
    if (clus >= 2) freeChain(clus);
    e[0] = kDeleted;
    return FieldDos::saveHdImage();
}

inline bool writePath(const char* path, const std::uint8_t* data, std::size_t size) noexcept {
    DirView view{};
    std::string leaf;
    if (!openDirPath(path, view, leaf, true) || leaf.empty()) return false;
    char n83[11]{};
    if (!nameTo83(leaf, n83)) return false;
    std::uint32_t idx = 0;
    if (findInView(view, n83, idx, false)) {
        auto* e = view.table + idx * 32u;
        const std::uint16_t old = static_cast<std::uint16_t>(e[26] | (e[27] << 8));
        if (old >= 2) freeChain(old);
        e[0] = kDeleted;
    }
    const std::uint32_t slot = freeSlot(view);
    if (slot == 0xFFFFFFFFu) return false;
    const std::uint32_t clustersNeeded = static_cast<std::uint32_t>(
        (size + static_cast<std::size_t>(FieldAmmoFat::geo.spc) * FieldAmmoFat::geo.bps - 1u)
        / (static_cast<std::size_t>(FieldAmmoFat::geo.spc) * FieldAmmoFat::geo.bps));
    const std::uint16_t first = FieldAmmoFat::allocChain(clustersNeeded);
    if (first >= 0xFFF0u) return false;
    std::size_t written = 0;
    std::uint16_t c = first;
    std::uint32_t guard = 0;
    while (c >= 2 && c < 0xFFF0u && guard++ < clustersNeeded + 2) {
        auto* cl = FieldAmmoFat::clusterData(c);
        if (!cl) return false;
        const std::size_t chunk = static_cast<std::size_t>(FieldAmmoFat::geo.spc) * FieldAmmoFat::geo.bps;
        const std::size_t todo = std::min(chunk, size - written);
        if (todo) {
            std::memcpy(cl, data + written, todo);
            const std::uint64_t volOff = FieldAmmoFat::geo.volOffset
                + static_cast<std::uint64_t>(
                       FieldAmmoFat::geo.dataStart + (c - 2u) * FieldAmmoFat::geo.spc)
                   * FieldAmmoFat::geo.bps
                + written;
            FieldDos::notifyHdHostWrite(volOff, static_cast<std::uint32_t>(todo),
                                        FieldDos::hdGuestRamPtr);
        }
        if (todo < chunk) std::memset(cl + todo, 0, chunk - todo);
        written += todo;
        const std::uint16_t next = FieldAmmoFat::fatGet(c);
        if (next >= 0xFFF8u) break;
        c = next;
    }
    auto* e = view.table + slot * 32u;
    std::memset(e, 0, 32u);
    std::memcpy(e, n83, 11);
    e[11] = kFileAttr;
    e[26] = static_cast<std::uint8_t>(first & 0xFFu);
    e[27] = static_cast<std::uint8_t>((first >> 8) & 0xFFu);
    e[28] = static_cast<std::uint8_t>(size & 0xFFu);
    e[29] = static_cast<std::uint8_t>((size >> 8) & 0xFFu);
    e[30] = static_cast<std::uint8_t>((size >> 16) & 0xFFu);
    e[31] = static_cast<std::uint8_t>((size >> 24) & 0xFFu);
    FieldAmmoFat::stampDirEntry(e);
    return FieldDos::saveHdImage();
}

inline bool readPath(const char* path, std::vector<std::uint8_t>& out) noexcept {
    char full[256];
    std::snprintf(full, sizeof full, "%.255s", path);
    return FieldDos::readHostFile(full, out);
}

inline bool isDirectory(const char* path) noexcept {
    DirView view{};
    std::string leaf;
    return openDirPath(path, view, leaf, false);
}

inline bool pathExists(const char* path) noexcept {
    if (isDirectory(path)) return true;
    std::vector<std::uint8_t> data;
    return readPath(path, data);
}

inline bool copyPath(const char* src, const char* dst) noexcept {
    std::vector<std::uint8_t> data;
    if (!readPath(src, data)) return false;
    return writePath(dst, data.data(), data.size());
}

inline bool renamePath(const char* src, const char* dst) noexcept {
    if (!copyPath(src, dst)) return false;
    return deletePath(src);
}

inline void joinPath(const std::string& base, const std::string& name, char out[128]) noexcept {
    if (base.empty() || base.back() == '\\')
        std::snprintf(out, 128, "C:\\%s", name.c_str());
    else
        std::snprintf(out, 128, "C:\\%s\\%s", base.c_str(), name.c_str());
}

inline int copyTree(const char* srcDir, const char* dstDir) noexcept {
    std::vector<FieldDos::FatRootEntry> entries;
    if (!listPath(srcDir, entries)) return -1;
    int copied = 0;
    for (const auto& fe : entries) {
        char src[160], dst[160];
        std::snprintf(src, sizeof src, "%s%s%s", srcDir,
            (srcDir[std::strlen(srcDir) - 1] == '\\') ? "" : "\\", fe.name);
        std::snprintf(dst, sizeof dst, "%s%s%s", dstDir,
            (dstDir[std::strlen(dstDir) - 1] == '\\') ? "" : "\\", fe.name);
        if (fe.isDir) {
            mkdirPath(dst);
            copied += copyTree(src, dst);
        } else if (copyPath(src, dst)) {
            ++copied;
        }
    }
    return copied;
}

} // namespace FieldAmmoVfs