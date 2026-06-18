#pragma once

// AMMOZIP — PKZIP extractor (stored + deflate) → AMMOFAT subdirectories.

#include "FieldAmmoFat.hpp"
#include "FieldAmmoVfs.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <zlib.h>

namespace FieldAmmoZip {

struct Result {
    bool ok = false;
    std::string error;
    int files = 0;
    int dirs = 0;
};

inline std::uint32_t rd32le(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0])
        | (static_cast<std::uint32_t>(p[1]) << 8)
        | (static_cast<std::uint32_t>(p[2]) << 16)
        | (static_cast<std::uint32_t>(p[3]) << 24);
}

inline std::uint16_t rd16le(const std::uint8_t* p) noexcept {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

inline std::string dosStem(const char* path) noexcept {
    if (!path) return {};
    const char* base = path;
    for (const char* p = path; *p; ++p)
        if (*p == '\\' || *p == '/') base = p + 1;
    std::string s(base);
    const auto dot = s.find_last_of('.');
    if (dot != std::string::npos) s.erase(dot);
    for (auto& c : s)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

inline std::string joinDos(const std::string& base, const std::string& rel) {
    std::string out = base;
    if (!out.empty() && out.back() != '\\') out += '\\';
    std::string norm = rel;
    for (auto& c : norm)
        if (c == '/') c = '\\';
    out += norm;
    for (auto& c : out)
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return out;
}

inline bool ensureDirTree(const std::string& dosDir) noexcept {
    if (dosDir.empty()) return false;
    std::vector<std::string> parts;
    FieldAmmoVfs::splitDosPath(dosDir.c_str(), 'C', parts);
    if (parts.empty()) return false;
    std::string built = "C:\\";
    for (std::size_t i = 0; i < parts.size(); ++i) {
        built += parts[i];
        if (!FieldAmmoVfs::isDirectory(built.c_str())) {
            if (!FieldAmmoVfs::mkdirPath(built.c_str())) return false;
        }
        built += '\\';
    }
    return true;
}

inline bool inflateRaw(const std::uint8_t* in, std::size_t inLen,
                       std::vector<std::uint8_t>& out, std::size_t expect) {
    out.assign(expect, 0);
    z_stream strm{};
    strm.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(in));
    strm.avail_in = static_cast<uInt>(inLen);
    strm.next_out = out.data();
    strm.avail_out = static_cast<uInt>(expect);
    if (inflateInit2(&strm, -MAX_WBITS) != Z_OK) return false;
    const int rc = inflate(&strm, Z_FINISH);
    inflateEnd(&strm);
    return rc == Z_STREAM_END && strm.total_out == expect;
}

struct CdEntry {
    std::string name;
    std::uint16_t method = 0;
    std::uint32_t compSize = 0;
    std::uint32_t uncompSize = 0;
    std::uint32_t localOff = 0;
    bool isDir = false;
};

inline std::string stripZipPrefix(const std::vector<CdEntry>& entries) {
    if (entries.empty()) return {};
    std::string prefix = entries[0].name;
    for (const auto& ent : entries) {
        std::size_t i = 0;
        while (i < prefix.size() && i < ent.name.size() && prefix[i] == ent.name[i]) ++i;
        prefix.resize(i);
    }
    const auto slash = prefix.find_last_of("/\\");
    if (slash == std::string::npos) return {};
    prefix.resize(slash + 1);
    for (const auto& ent : entries) {
        if (ent.name.size() < prefix.size() || ent.name.compare(0, prefix.size(), prefix) != 0)
            return {};
    }
    return prefix;
}

inline std::string relEntryPath(const std::string& name, const std::string& prefix) {
    if (prefix.empty() || name.size() <= prefix.size()) return name;
    return name.substr(prefix.size());
}

inline bool parseCentralDir(const std::uint8_t* data, std::size_t size,
                            std::vector<CdEntry>& out) {
    out.clear();
    if (!data || size < 22u) return false;
    std::size_t eocd = size - 22u;
    for (std::size_t scan = 0; scan < 65536u && eocd >= scan; ++scan) {
        const std::size_t off = size - 22u - scan;
        if (rd32le(data + off) != 0x06054b50u) continue;
        eocd = off;
        break;
    }
    if (rd32le(data + eocd) != 0x06054b50u) return false;
    const std::uint32_t cdOff = rd32le(data + eocd + 16u);
    const std::uint16_t cdCount = rd16le(data + eocd + 10u);
    if (cdOff >= size) return false;
    std::size_t pos = cdOff;
    for (std::uint16_t i = 0; i < cdCount && pos + 46u <= size; ++i) {
        if (rd32le(data + pos) != 0x02014b50u) break;
        const std::uint16_t method = rd16le(data + pos + 10u);
        const std::uint32_t compSize = rd32le(data + pos + 20u);
        const std::uint32_t uncompSize = rd32le(data + pos + 24u);
        const std::uint16_t nameLen = rd16le(data + pos + 28u);
        const std::uint16_t extraLen = rd16le(data + pos + 30u);
        const std::uint16_t commentLen = rd16le(data + pos + 32u);
        const std::uint32_t localOff = rd32le(data + pos + 42u);
        if (pos + 46u + nameLen > size) break;
        CdEntry ent{};
        ent.name.assign(reinterpret_cast<const char*>(data + pos + 46u), nameLen);
        ent.method = method;
        ent.compSize = compSize;
        ent.uncompSize = uncompSize;
        ent.localOff = localOff;
        ent.isDir = !ent.name.empty()
            && (ent.name.back() == '/' || ent.name.back() == '\\');
        if (!ent.name.empty()) out.push_back(std::move(ent));
        pos += 46u + nameLen + extraLen + commentLen;
    }
    return !out.empty();
}

inline bool decompressEntry(const std::uint8_t* data, std::size_t size,
                            const CdEntry& ent, std::vector<std::uint8_t>& payload) {
    if (ent.localOff + 30u > size) return false;
    const std::uint16_t nameLen = rd16le(data + ent.localOff + 26u);
    const std::uint16_t extraLen = rd16le(data + ent.localOff + 28u);
    const std::size_t dataOff = ent.localOff + 30u + nameLen + extraLen;
    if (dataOff + ent.compSize > size) return false;
    const std::uint8_t* comp = data + dataOff;
    if (ent.method == 0u) {
        payload.assign(comp, comp + ent.compSize);
        return payload.size() == ent.uncompSize;
    }
    if (ent.method == 8u)
        return inflateRaw(comp, ent.compSize, payload, ent.uncompSize);
    return false;
}

inline Result extractArchive(const std::uint8_t* data, std::size_t size,
                             const char* destDir) noexcept {
    Result r{};
    if (!data || size < 64u) {
        r.error = "empty archive";
        return r;
    }
    if (!FieldAmmoFat::mounted && !FieldAmmoFat::mount()) {
        r.error = "AMMOFAT not mounted";
        return r;
    }
    std::vector<CdEntry> entries;
    if (!parseCentralDir(data, size, entries)) {
        r.error = "invalid ZIP (no central directory)";
        return r;
    }
    const std::string base = destDir && destDir[0] ? destDir : "C:\\";
    if (!ensureDirTree(base)) {
        r.error = "cannot create destination tree";
        return r;
    }
    const std::string prefix = stripZipPrefix(entries);
    for (const auto& ent : entries) {
        const std::string rel = relEntryPath(ent.name, prefix);
        if (rel.empty()) continue;
        const std::string outPath = joinDos(base, rel);
        if (ent.isDir) {
            if (ensureDirTree(outPath)) ++r.dirs;
            continue;
        }
        const auto slash = outPath.find_last_of('\\');
        if (slash != std::string::npos) {
            const std::string parent = outPath.substr(0, slash);
            if (!ensureDirTree(parent)) {
                r.error = "mkdir failed: " + parent;
                return r;
            }
        }
        std::vector<std::uint8_t> payload;
        if (!decompressEntry(data, size, ent, payload)) {
            r.error = "decompress failed: " + ent.name
                + " (method " + std::to_string(ent.method) + ")";
            return r;
        }
        if (!FieldAmmoVfs::writePath(outPath.c_str(), payload.data(), payload.size())) {
            r.error = "write failed: " + outPath;
            return r;
        }
        ++r.files;
    }
    r.ok = true;
    return r;
}

inline Result extractPath(const char* zipPath, const char* destDir = nullptr) noexcept {
    Result r{};
    if (!zipPath || !zipPath[0]) {
        r.error = "usage: AMMOZIP archive.zip [dest\\]";
        return r;
    }
    std::vector<std::uint8_t> data;
    if (!FieldAmmoVfs::readPath(zipPath, data) || data.empty()) {
        r.error = std::string("cannot read ") + zipPath;
        return r;
    }
    std::string dest;
    if (destDir && destDir[0]) {
        dest = destDir;
    } else {
        const auto slash = std::string(zipPath).find_last_of('\\');
        if (slash != std::string::npos)
            dest = std::string(zipPath, 0, slash);
        else
            dest = "C:\\" + dosStem(zipPath);
    }
    return extractArchive(data.data(), data.size(), dest.c_str());
}

inline Result extractArgs(const std::vector<std::string>& args) noexcept {
    std::string zip;
    std::string dest;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& a = args[i];
        if ((a == "-d" || a == "/D") && i + 1 < args.size()) {
            dest = args[++i];
            continue;
        }
        if (a.empty() || a[0] == '-' || a[0] == '/') continue;
        if (zip.empty()) zip = a;
        else if (dest.empty()) dest = a;
    }
    if (zip.empty()) {
        Result r{};
        r.error = "usage: AMMOZIP archive.zip [dest\\]  |  AMMOZIP -d dest archive.zip";
        return r;
    }
    if (zip.find(':') == std::string::npos && zip.find('\\') == std::string::npos)
        zip = "C:\\" + zip;
    if (!dest.empty()) {
        if (dest.find(':') == std::string::npos && dest.find('\\') == std::string::npos)
            dest = "C:\\" + dest;
        return extractPath(zip.c_str(), dest.c_str());
    }
    return extractPath(zip.c_str(), nullptr);
}

} // namespace FieldAmmoZip