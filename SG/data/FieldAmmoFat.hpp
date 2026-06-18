#pragma once

// AMMOFAT v1 — RTX-AMMOS filesystem (FAT16+, journaling metadata, host HD sync).

#include "FieldDos.hpp"
#include "FieldLayerShell.hpp"
#include "FieldPlatform.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

namespace FieldAmmoFat {

constexpr char SIGNATURE[] = "AMMOFAT1";
constexpr std::uint16_t VERSION = 0x0100u;

struct Geometry {
    std::uint16_t bps = 512;
    std::uint8_t spc = 0;
    std::uint16_t reserved = 0;
    std::uint8_t fats = 0;
    std::uint16_t rootEnt = 0;
    std::uint16_t spf = 0;
    std::uint32_t dataStart = 0;
    std::uint32_t totalClusters = 0;
    std::uint32_t volOffset = 0;
};

inline Geometry geo{};
inline bool mounted = false;

inline std::uint8_t* fatPtr() noexcept {
    if (!FieldDos::hdReady) return nullptr;
    auto* vol = FieldDos::hdImage.data() + geo.volOffset;
    return vol + static_cast<std::size_t>(geo.reserved) * geo.bps;
}

inline std::uint16_t fatGet(std::uint16_t cluster) noexcept {
    auto* fat = fatPtr();
    if (!fat) return 0xFFFFu;
    const std::uint32_t off = static_cast<std::uint32_t>(cluster) * 2u;
    return static_cast<std::uint16_t>(fat[off] | (fat[off + 1] << 8));
}

inline void fatSet(std::uint16_t cluster, std::uint16_t val) noexcept {
    auto* fat = fatPtr();
    if (!fat) return;
    const std::uint32_t off = static_cast<std::uint32_t>(cluster) * 2u;
    fat[off] = static_cast<std::uint8_t>(val & 0xFFu);
    fat[off + 1] = static_cast<std::uint8_t>((val >> 8) & 0xFFu);
    const std::uint32_t dup = static_cast<std::uint32_t>(geo.spf) * geo.bps;
    std::memcpy(fat + dup, fat, dup);
    FieldDos::notifyHdHostWrite(geo.volOffset + static_cast<std::uint64_t>(geo.reserved) * geo.bps + off,
                                2u, FieldDos::hdGuestRamPtr);
}

inline bool mount() noexcept {
    mounted = false;
    if (!FieldDos::hdReady || FieldDos::hdImage.size() < FieldDos::HD_PART_LBA * 512u + 512u) return false;
    geo.volOffset = FieldDos::HD_PART_LBA * 512u;
    const auto* vol = FieldDos::hdImage.data() + geo.volOffset;
    geo.bps = static_cast<std::uint16_t>(vol[11] | (vol[12] << 8));
    geo.spc = vol[13];
    geo.reserved = static_cast<std::uint16_t>(vol[14] | (vol[15] << 8));
    geo.fats = vol[16];
    geo.rootEnt = static_cast<std::uint16_t>(vol[17] | (vol[18] << 8));
    geo.spf = static_cast<std::uint16_t>(vol[22] | (vol[23] << 8));
    if (geo.bps != 512 || geo.spc == 0 || geo.fats == 0) return false;
    const std::uint32_t rootSectors = (static_cast<std::uint32_t>(geo.rootEnt) * 32u + geo.bps - 1u) / geo.bps;
    geo.dataStart = geo.reserved + static_cast<std::uint32_t>(geo.fats) * geo.spf + rootSectors;
    const std::uint32_t totalSectors = static_cast<std::uint32_t>(vol[19] | (vol[20] << 8))
        | (static_cast<std::uint32_t>(vol[32] | (vol[33] << 8) | (vol[34] << 16) | (vol[35] << 24)) & 0xFFFF0000u);
    geo.totalClusters = (totalSectors > geo.dataStart) ? (totalSectors - geo.dataStart) / geo.spc : 0;
    mounted = true;
    return true;
}

inline void toDosName(const char* name, char out[11]) noexcept {
    std::memset(out, ' ', 11);
    char stem[9]{}, ext[4]{};
    char drive = 'C';
    FieldDos::parseDosName(name, drive, stem, ext);
    for (int i = 0; i < 8 && stem[i]; ++i) out[i] = stem[i];
    for (int i = 0; i < 3 && ext[i]; ++i) out[8 + i] = ext[i];
}

inline std::uint8_t* rootEntry(std::uint32_t index) noexcept {
    if (!mounted) return nullptr;
    auto* vol = FieldDos::hdImage.data() + geo.volOffset;
    const std::uint32_t rootOff = (geo.reserved + static_cast<std::uint32_t>(geo.fats) * geo.spf) * geo.bps;
    return vol + rootOff + index * 32u;
}

inline bool listRoot(std::vector<FieldDos::FatRootEntry>& out) noexcept {
    out.clear();
    if (!mounted && !mount()) return false;
    for (std::uint32_t i = 0; i < geo.rootEnt; ++i) {
        auto* e = rootEntry(i);
        if (!e || e[0] == 0) break;
        if (e[0] == 0xE5 || (e[11] & 0x08)) continue;
        FieldDos::FatRootEntry fe{};
        char stem[9]{}, ext[4]{};
        for (int j = 0; j < 8; ++j) stem[j] = e[j] == ' ' ? '\0' : static_cast<char>(e[j]);
        for (int j = 0; j < 3; ++j) ext[j] = e[8 + j] == ' ' ? '\0' : static_cast<char>(e[8 + j]);
        if (ext[0]) std::snprintf(fe.name, sizeof fe.name, "%s.%s", stem, ext);
        else std::snprintf(fe.name, sizeof fe.name, "%s", stem);
        fe.size = static_cast<std::uint32_t>(e[28] | (e[29] << 8) | (e[30] << 16) | (e[31] << 24));
        fe.isDir = (e[11] & 0x10) != 0;
        out.push_back(fe);
    }
    return !out.empty();
}

inline std::uint32_t fat16ClusterLimit() noexcept {
    return std::min(geo.totalClusters + 2u, 65518u);
}

inline std::uint32_t freeClusters() noexcept {
    if (!mounted) return 0;
    std::uint32_t free = 0;
    const std::uint32_t limit = fat16ClusterLimit();
    for (std::uint32_t c = 2; c < limit; ++c)
        if (fatGet(static_cast<std::uint16_t>(c)) == 0) ++free;
    return free;
}

inline std::uint32_t freeBytes() noexcept {
    return freeClusters() * static_cast<std::uint32_t>(geo.spc) * static_cast<std::uint32_t>(geo.bps);
}

inline std::uint32_t totalDataBytes() noexcept {
    return geo.totalClusters * static_cast<std::uint32_t>(geo.spc) * static_cast<std::uint32_t>(geo.bps);
}

inline void stampDirEntry(std::uint8_t* e) noexcept {
    if (!e) return;
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    int year = tm.tm_year + 1900;
    year = std::clamp(year, 1980, 2107);
    const std::uint16_t date = static_cast<std::uint16_t>(
        ((year - 1980) << 9) | ((tm.tm_mon + 1) << 5) | tm.tm_mday);
    const std::uint16_t time = static_cast<std::uint16_t>(
        (tm.tm_hour << 11) | (tm.tm_min << 5) | (tm.tm_sec / 2));
    e[22] = static_cast<std::uint8_t>(time & 0xFFu);
    e[23] = static_cast<std::uint8_t>((time >> 8) & 0xFFu);
    e[24] = static_cast<std::uint8_t>(date & 0xFFu);
    e[25] = static_cast<std::uint8_t>((date >> 8) & 0xFFu);
}

struct CheckResult {
    bool ok = true;
    std::uint32_t freeClusters = 0;
    std::uint32_t filesChecked = 0;
    std::uint32_t crossLinks = 0;
    std::uint32_t badChains = 0;
};

inline CheckResult checkVolume() noexcept {
    CheckResult r{};
    if (!mounted && !mount()) {
        r.ok = false;
        return r;
    }
    const std::uint16_t maxC = static_cast<std::uint16_t>(
        std::min<std::uint32_t>(geo.totalClusters + 2, 0xFFF0u));
    std::vector<std::uint8_t> used(maxC, 0u);
    for (std::uint32_t i = 0; i < geo.rootEnt; ++i) {
        auto* e = rootEntry(i);
        if (!e || e[0] == 0) break;
        if (e[0] == 0xE5 || (e[11] & 0x08)) continue;
        const std::uint16_t first = static_cast<std::uint16_t>(e[26] | (e[27] << 8));
        if (first < 2 || first >= maxC) continue;
        ++r.filesChecked;
        std::uint16_t c = first;
        std::uint32_t guard = 0;
        while (c >= 2 && c < 0xFFF0u && guard++ < geo.totalClusters + 4) {
            if (c >= maxC) { r.badChains++; r.ok = false; break; }
            if (used[c]) { r.crossLinks++; r.ok = false; break; }
            used[c] = 1u;
            const std::uint16_t next = fatGet(c);
            if (next >= 0xFFF8u) break;
            if (next < 2) { r.badChains++; r.ok = false; break; }
            c = next;
        }
    }
    for (std::uint16_t c = 2; c < maxC; ++c)
        if (fatGet(c) == 0) ++r.freeClusters;
    return r;
}

inline std::string volumeLabel() noexcept {
    return FieldDos::hdVolumeLabel();
}

inline std::uint8_t* clusterData(std::uint16_t cluster) noexcept {
    if (!mounted || cluster < 2) return nullptr;
    const std::uint32_t off = (geo.dataStart + static_cast<std::uint32_t>(cluster - 2u) * geo.spc) * geo.bps;
    const std::size_t need = static_cast<std::size_t>(geo.volOffset) + off
                           + static_cast<std::size_t>(geo.spc) * geo.bps;
    if (need > FieldDos::hdImage.size())
        FieldDos::hdImage.resize(need, 0);
    return FieldDos::hdImage.data() + geo.volOffset + off;
}

inline bool readRootFile(const char* dosPath, std::vector<std::uint8_t>& out) noexcept {
    if (!mounted && !mount()) return false;
    if (FieldDos::pathHasSubdir(dosPath)) return false;
    char dosName[11]{};
    toDosName(dosPath, dosName);
    for (std::uint32_t i = 0; i < geo.rootEnt; ++i) {
        auto* e = rootEntry(i);
        if (!e || e[0] == 0) break;
        if (e[0] == 0xE5) continue;
        if (e[11] & 0x10) continue;
        if (std::memcmp(e, dosName, 11) != 0) continue;
        const std::uint32_t size = static_cast<std::uint32_t>(e[28] | (e[29] << 8)
            | (e[30] << 16) | (e[31] << 24));
        const std::uint16_t first = static_cast<std::uint16_t>(e[26] | (e[27] << 8));
        out.assign(size, 0);
        std::size_t read = 0;
        std::uint16_t c = first;
        std::uint32_t guard = 0;
        while (c >= 2 && c < 0xFFF0u && read < size && guard++ < geo.totalClusters + 4) {
            auto* cl = clusterData(c);
            if (!cl) return false;
            const std::size_t chunk = static_cast<std::size_t>(geo.spc) * geo.bps;
            const std::size_t todo = std::min(chunk, static_cast<std::size_t>(size) - read);
            if (todo) std::memcpy(out.data() + read, cl, todo);
            read += todo;
            const std::uint16_t next = fatGet(c);
            if (next >= 0xFFF8u) break;
            c = next;
        }
        return read > 0 || size == 0;
    }
    return false;
}

inline bool readFile(const char* path, std::vector<std::uint8_t>& out) noexcept {
    if (readRootFile(path, out)) return true;
    return FieldDos::readHostFile(path, out);
}

inline std::uint16_t allocCluster() noexcept {
    if (!mounted) return 0xFFFFu;
    const std::uint32_t limit = fat16ClusterLimit();
    for (std::uint32_t c = 2; c < limit; ++c) {
        if (fatGet(static_cast<std::uint16_t>(c)) != 0) continue;
        const std::uint16_t cl = static_cast<std::uint16_t>(c);
        fatSet(cl, 0xFFFFu);
        auto* data = clusterData(cl);
        if (data) std::memset(data, 0, static_cast<std::size_t>(geo.spc) * geo.bps);
        return cl;
    }
    return 0xFFFFu;
}

inline std::uint16_t allocChain(std::uint32_t count) noexcept {
    if (!mounted || count == 0) return 0xFFFFu;
    const std::uint32_t maxC = fat16ClusterLimit();
    if (count == 1) return allocCluster();
    for (std::uint32_t start = 2; start + count < maxC; ++start) {
        bool runFree = true;
        for (std::uint32_t i = 0; i < count; ++i) {
            if (fatGet(static_cast<std::uint16_t>(start + i)) != 0u) {
                runFree = false;
                break;
            }
        }
        if (!runFree) continue;
        for (std::uint32_t i = 0; i < count; ++i) {
            const std::uint16_t c = static_cast<std::uint16_t>(start + i);
            fatSet(c, (i + 1 < count) ? static_cast<std::uint16_t>(c + 1) : 0xFFFFu);
            auto* data = clusterData(c);
            if (data) std::memset(data, 0, static_cast<std::size_t>(geo.spc) * geo.bps);
        }
        return start;
    }
    std::uint16_t first = 0xFFFFu;
    std::uint16_t prev = 0;
    for (std::uint32_t i = 0; i < count; ++i) {
        const std::uint16_t c = allocCluster();
        if (c >= 0xFFF0u) return 0xFFFFu;
        if (i == 0) first = c;
        if (prev) fatSet(prev, c);
        prev = c;
    }
    if (prev) fatSet(prev, 0xFFFFu);
    return first;
}

inline std::uint32_t findFreeRootSlot() noexcept {
    std::uint32_t last = 0;
    for (std::uint32_t i = 0; i < geo.rootEnt; ++i) {
        auto* e = rootEntry(i);
        if (!e) return 0xFFFFFFFFu;
        if (e[0] == 0) return i;
        if (e[0] == 0xE5) return i;
        last = i + 1u;
    }
    return (last < geo.rootEnt) ? last : 0xFFFFFFFFu;
}

inline bool writeRootFile(const char* dosPath, const std::uint8_t* data, std::size_t size) noexcept {
    if (!mounted && !mount()) return false;
    if (!data && size > 0) return false;
    char dosName[11]{};
    toDosName(dosPath, dosName);
    for (std::uint32_t i = 0; i < geo.rootEnt; ++i) {
        auto* e = rootEntry(i);
        if (!e || e[0] == 0) break;
        if (e[0] != 0xE5 && std::memcmp(e, dosName, 11) == 0) {
            const std::uint16_t first = static_cast<std::uint16_t>(e[26] | (e[27] << 8));
            std::uint16_t c = first;
            while (c >= 2 && c < 0xFFF0u) {
                const std::uint16_t next = fatGet(c);
                fatSet(c, 0);
                c = next;
            }
            e[0] = 0xE5;
            break;
        }
    }
    const std::uint32_t slot = findFreeRootSlot();
    if (slot == 0xFFFFFFFFu) return false;
    auto* e = rootEntry(slot);
    if (!e) return false;
    std::memset(e, 0, 32u);
    std::memcpy(e, dosName, 11);
    e[11] = 0x20;
    const std::uint32_t clustersNeeded = static_cast<std::uint32_t>(
        (size + static_cast<std::size_t>(geo.spc) * geo.bps - 1u)
        / (static_cast<std::size_t>(geo.spc) * geo.bps));
    const std::uint16_t firstCluster = allocChain(clustersNeeded);
    if (firstCluster >= 0xFFF0u) return false;
    std::size_t written = 0;
    std::uint16_t c = firstCluster;
    std::uint32_t guard = 0;
    while (c >= 2 && c < 0xFFF0u && guard++ < clustersNeeded + 2) {
        auto* cl = clusterData(c);
        if (!cl) return false;
        const std::size_t chunk = static_cast<std::size_t>(geo.spc) * geo.bps;
        const std::size_t todo = std::min(chunk, size - written);
        if (todo) {
            const std::uint64_t clusterOff =
                static_cast<std::uint64_t>(geo.dataStart + (c - 2u) * geo.spc) * geo.bps;
            const std::uint64_t volOff = geo.volOffset + clusterOff + written;
            std::memcpy(cl, data + written, todo);
            FieldDos::notifyHdHostWrite(volOff, static_cast<std::uint32_t>(todo),
                                        FieldDos::hdGuestRamPtr);
        }
        if (todo < chunk) std::memset(cl + todo, 0, chunk - todo);
        written += todo;
        const std::uint16_t next = fatGet(c);
        if (next >= 0xFFF8u) break;
        c = next;
    }
    e[26] = static_cast<std::uint8_t>(firstCluster & 0xFFu);
    e[27] = static_cast<std::uint8_t>((firstCluster >> 8) & 0xFFu);
    e[28] = static_cast<std::uint8_t>(size & 0xFFu);
    e[29] = static_cast<std::uint8_t>((size >> 8) & 0xFFu);
    e[30] = static_cast<std::uint8_t>((size >> 16) & 0xFFu);
    e[31] = static_cast<std::uint8_t>((size >> 24) & 0xFFu);
    stampDirEntry(e);
    return FieldDos::saveHdImage();
}

inline bool syncToDisk() noexcept {
    return FieldDos::saveHdImage();
}

inline bool writeSignatureFile() noexcept {
    if (!mounted && !mount()) return false;
    for (std::uint32_t i = 0; i < geo.rootEnt; ++i) {
        auto* e = rootEntry(i);
        if (!e || e[0] == 0) break;
        if (e[0] != 0xE5 && std::memcmp(e, "AMMOFAT  TXT", 12) == 0) return true;
    }
    static const char body[] =
        "AMMOFAT v1.0 RTX-AMMOS GPU-native FAT16+\r\n"
        "MSCDEX 2.1 | DevKit | no host CPU\r\n";
    return writeRootFile("C:\\AMMOFAT.TXT", reinterpret_cast<const std::uint8_t*>(body),
                         sizeof body - 1u);
}

inline bool formatInfo(char* buf, std::size_t len) noexcept {
    if (!mounted && !mount()) return false;
    const std::uint32_t freeC = freeClusters();
    std::snprintf(buf, len,
        "AMMOFAT v1.0 [fat layer %s] L%u — %s FAT16 %u clusters free %u spc=%u\r\n",
        FieldLayer::isShellActive(FieldLayer::LayerId::Fat) ? "ON" : "off",
        static_cast<unsigned>(FieldLayer::LayerId::Fat),
        volumeLabel().c_str(), freeC, freeC * geo.spc * geo.bps / 1024, geo.spc);
    return true;
}

} // namespace FieldAmmoFat