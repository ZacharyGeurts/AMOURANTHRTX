#pragma once

// RTX Drive Indexer — production flat sorted index for instant Start-menu file search.
// Sorted by lowercase full path; bucket-accelerated prefix scan (256-way split on basename[0]).
// Rebuilds on FAT mount / VFS reload; packs top matches to FILE_SEARCH_RAM for GPU flyout.

#include "FieldAmouranthHudRam.hpp"
#include "FieldAmouranthTextures.hpp"
#include "FieldAmmoFat.hpp"
#include "FieldAmmoVfs.hpp"
#include "FieldExtensionMap.hpp"
#include "FieldRtxVfs.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace FieldAmouranthFileIndex {

constexpr std::uint32_t FILE_SEARCH_RAM = FieldAmouranthHudRam::FILE_SEARCH_RAM;
constexpr int           SEARCH_MAX      = FieldAmouranthHudRam::SEARCH_MAX;
constexpr int           SEARCH_QUERY_LEN = FieldAmouranthHudRam::SEARCH_QUERY_LEN;
constexpr int           SEARCH_NAME_LEN  = FieldAmouranthHudRam::SEARCH_NAME_LEN;
constexpr int           SEARCH_PATH_LEN  = FieldAmouranthHudRam::SEARCH_PATH_LEN;
constexpr int           SEARCH_ROW_STRIDE = FieldAmouranthHudRam::SEARCH_ROW_STRIDE;

struct Entry {
    std::string path;
    std::string sortKey;
    std::string baseKey;
    std::uint32_t size = 0u;
    std::uint8_t  icon = 6u;
    bool          isDir = false;
};

inline std::vector<Entry> entries;
inline std::vector<Entry> buckets[256];
inline bool built = false;
inline std::uint64_t buildGen = 0u;

inline char lowerChar(char c) noexcept {
    return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

inline std::string lowerCopy(const char* s) noexcept {
    if (!s) return {};
    std::string out;
    for (; *s; ++s) out += lowerChar(*s);
    return out;
}

inline std::string basenameKey(const std::string& path) noexcept {
    const std::size_t slash = path.find_last_of('\\');
    const std::string base = slash == std::string::npos ? path : path.substr(slash + 1);
    return lowerCopy(base.c_str());
}

inline std::uint8_t iconForName(const char* name, bool isDir) noexcept {
    using IS = FieldAmouranthTextures::IconSlot;
    if (isDir) return static_cast<std::uint8_t>(IS::Desktop);
    const std::string ext = FieldExtensionMap::upperExt(name);
    if (ext == ".EXE" || ext == ".COM" || ext == ".BAT") return static_cast<std::uint8_t>(IS::Shell);
    if (ext == ".NES" || ext == ".ROM") return static_cast<std::uint8_t>(IS::Nes);
    if (ext == ".WAD") return static_cast<std::uint8_t>(IS::Doom);
    if (ext == ".ASM" || ext == ".C" || ext == ".H" || ext == ".INC")
        return static_cast<std::uint8_t>(IS::AmmoCode);
    if (ext == ".BAS") return static_cast<std::uint8_t>(IS::QBasic);
    if (ext == ".FLD") return static_cast<std::uint8_t>(IS::FieldC);
    if (ext == ".TXT" || ext == ".DOC" || ext == ".LOG")
        return static_cast<std::uint8_t>(IS::AmmoText);
    if (ext == ".HTM" || ext == ".HTML") return static_cast<std::uint8_t>(IS::Display);
    if (ext == ".ZIP" || ext == ".ISO" || ext == ".IMG") return static_cast<std::uint8_t>(IS::Settings);
    return static_cast<std::uint8_t>(IS::FileCmd);
}

inline void indexDir(const char* dir, std::vector<Entry>& out) noexcept {
    if (!dir || !dir[0]) return;
    std::vector<FieldDos::FatRootEntry> fat;
    if (!FieldAmmoVfs::listPath(dir, fat)) return;
    for (const auto& fe : fat) {
        char full[160]{};
        const std::size_t dlen = std::strlen(dir);
        const bool slash = dlen > 0 && dir[dlen - 1] == '\\';
        std::snprintf(full, sizeof full, "%s%s%s", dir, slash ? "" : "\\", fe.name);
        if (fe.isDir) {
            if (fe.name[0] == '.' && (fe.name[1] == '\0'
                    || (fe.name[1] == '.' && fe.name[2] == '\0')))
                continue;
            indexDir(full, out);
            continue;
        }
        Entry e{};
        e.path = full;
        e.sortKey = lowerCopy(full);
        e.baseKey = basenameKey(e.path);
        e.size = fe.size;
        e.isDir = false;
        e.icon = iconForName(fe.name, false);
        out.push_back(std::move(e));
    }
}

inline void rebuildBuckets() noexcept {
    for (auto& b : buckets) b.clear();
    for (const auto& e : entries) {
        const unsigned char b0 = e.baseKey.empty()
            ? 0u : static_cast<unsigned char>(e.baseKey[0]);
        buckets[b0].push_back(e);
    }
    for (auto& b : buckets) {
        std::sort(b.begin(), b.end(),
            [](const Entry& a, const Entry& b) { return a.sortKey < b.sortKey; });
    }
}

inline void build() noexcept {
    entries.clear();
    built = false;
    if (!FieldAmmoFat::mounted && !FieldAmmoFat::mount()) {
        std::fprintf(stderr, "[FILEINDEX] FAT not mounted — empty index\n");
        return;
    }
    FieldRtxVfs::vfsInit();
    indexDir("C:\\", entries);
    std::sort(entries.begin(), entries.end(),
        [](const Entry& a, const Entry& b) { return a.sortKey < b.sortKey; });
    rebuildBuckets();
    built = true;
    ++buildGen;
    std::fprintf(stderr, "[FILEINDEX] Drive index ready — %zu files (gen %llu)\n",
        entries.size(), static_cast<unsigned long long>(buildGen));
}

inline void ensureBuilt() noexcept {
    if (!built) build();
}

inline void invalidate() noexcept {
    built = false;
}

inline bool matchesQuery(const Entry& e, const std::string& q) noexcept {
    if (q.empty()) return false;
    if (e.sortKey.rfind(q, 0) == 0) return true;
    if (e.baseKey.rfind(q, 0) == 0) return true;
    return e.baseKey.find(q) != std::string::npos;
}

inline void collectMatches(const std::string& q, std::vector<const Entry*>& out) noexcept {
    out.clear();
    if (q.empty() || entries.empty()) return;

    auto scanBucket = [&](const std::vector<Entry>& bucket) {
        for (const auto& e : bucket) {
            if (!matchesQuery(e, q)) continue;
            out.push_back(&e);
            if (out.size() >= static_cast<std::size_t>(SEARCH_MAX)) return;
        }
    };

    if (q.size() == 1u) {
        const unsigned char b0 = static_cast<unsigned char>(q[0]);
        scanBucket(buckets[b0]);
        if (out.size() < static_cast<std::size_t>(SEARCH_MAX)) {
            for (int bi = 0; bi < 256; ++bi) {
                if (static_cast<unsigned char>(bi) == b0) continue;
                scanBucket(buckets[static_cast<std::size_t>(bi)]);
                if (out.size() >= static_cast<std::size_t>(SEARCH_MAX)) break;
            }
        }
    } else {
        auto it = std::lower_bound(entries.begin(), entries.end(), q,
            [](const Entry& e, const std::string& needle) { return e.sortKey < needle; });
        for (; it != entries.end() && out.size() < static_cast<std::size_t>(SEARCH_MAX); ++it) {
            if (!matchesQuery(*it, q)) {
                if (it->sortKey > q && it->baseKey.rfind(q, 0) != 0
                        && it->baseKey.find(q) == std::string::npos)
                    continue;
                if (!matchesQuery(*it, q)) continue;
            }
            out.push_back(&(*it));
        }
        if (out.size() < static_cast<std::size_t>(SEARCH_MAX)) {
            for (const auto& e : entries) {
                if (out.size() >= static_cast<std::size_t>(SEARCH_MAX)) break;
                if (e.sortKey.rfind(q, 0) == 0) continue;
                if (!matchesQuery(e, q)) continue;
                bool dup = false;
                for (const Entry* p : out) if (p->path == e.path) { dup = true; break; }
                if (!dup) out.push_back(&e);
            }
        }
    }

    std::sort(out.begin(), out.end(),
        [](const Entry* a, const Entry* b) { return a->sortKey < b->sortKey; });
    if (out.size() > static_cast<std::size_t>(SEARCH_MAX))
        out.resize(static_cast<std::size_t>(SEARCH_MAX));
}

inline void writeRamStr(std::uint8_t* ram, std::uint32_t off, const char* s, int maxLen) noexcept {
    if (!ram) return;
    for (int i = 0; i < maxLen; ++i) {
        const char ch = (s && s[i]) ? s[i] : ' ';
        ram[off + static_cast<std::uint32_t>(i)] = static_cast<std::uint8_t>(ch);
    }
}

inline void packResultsRam(std::uint8_t* ram, const char* query, int hover,
        const std::vector<const Entry*>& hits) noexcept {
    if (!ram) return;
    const int qLen = query ? static_cast<int>(std::strlen(query)) : 0;
    const int n = static_cast<int>(std::min(hits.size(), static_cast<std::size_t>(SEARCH_MAX)));
    ram[FILE_SEARCH_RAM] = static_cast<std::uint8_t>(n);
    ram[FILE_SEARCH_RAM + 1u] = static_cast<std::uint8_t>(std::min(qLen, SEARCH_QUERY_LEN));
    ram[FILE_SEARCH_RAM + 2u] = hover >= 0 ? static_cast<std::uint8_t>(hover) : 0xFFu;
    ram[FILE_SEARCH_RAM + 3u] = (qLen > 0 && n > 0) ? 1u : 0u;
    writeRamStr(ram, FILE_SEARCH_RAM + 4u, query, SEARCH_QUERY_LEN);

    for (int i = 0; i < SEARCH_MAX; ++i) {
        const std::uint32_t base = FILE_SEARCH_RAM + 0x40u
            + static_cast<std::uint32_t>(i * SEARCH_ROW_STRIDE);
        if (i >= n || !hits[static_cast<std::size_t>(i)]) {
            ram[base] = 0xFFu;
            continue;
        }
        const Entry& e = *hits[static_cast<std::size_t>(i)];
        ram[base] = e.icon;
        ram[base + 1u] = e.isDir ? 1u : 0u;
        ram[base + 2u] = static_cast<std::uint8_t>(e.size & 0xFFu);
        ram[base + 3u] = static_cast<std::uint8_t>((e.size >> 8) & 0xFFu);
        ram[base + 4u] = static_cast<std::uint8_t>((e.size >> 16) & 0xFFu);
        ram[base + 5u] = static_cast<std::uint8_t>((e.size >> 24) & 0xFFu);
        const std::size_t slash = e.path.find_last_of('\\');
        const char* name = slash == std::string::npos ? e.path.c_str() : e.path.c_str() + slash + 1;
        writeRamStr(ram, base + 8u, name, SEARCH_NAME_LEN);
        writeRamStr(ram, base + 8u + SEARCH_NAME_LEN, e.path.c_str(), SEARCH_PATH_LEN);
    }
}

} // namespace FieldAmouranthFileIndex