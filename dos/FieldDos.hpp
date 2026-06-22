#pragma once

// RTX-DOS 7.0 GPU Super DOSBox — Microsoft MS-DOS MIT lineage + virtual C: HDD image.

#include "FieldPlatform.hpp"
#include "FieldRtxMemory.hpp"

#ifdef FIELD_DOS_EMBED_HD
#include "FieldDosEmbed.hpp"
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>

namespace FieldRaid {
void init() noexcept;
}

namespace FieldDos {

constexpr std::uint32_t DISK_IMAGE_BASE  = 0x00020000u;
constexpr std::uint32_t DISK_IMAGE_BYTES = 737280u;
constexpr std::uint32_t BOOT_VECTOR      = 0x00007C00u;
constexpr std::uint32_t BOOT_SIG         = 0x0000AA55u;

constexpr std::uint32_t HD_SECTOR_BYTES = 512u;
constexpr std::uint64_t HD_MAX_BYTES      = FieldPlatform::HD_SIZE_BYTES;
constexpr std::uint32_t HD_PART_LBA       = FieldPlatform::HD_PART_LBA;
constexpr std::uint32_t GUEST_RAM_BYTES   = FieldPlatform::GUEST_RAM_BYTES;

inline std::vector<std::uint8_t> hdImage;
inline std::vector<std::uint8_t> floppyImage;
inline std::filesystem::path hdImagePath;
inline bool hdReady = false;
inline std::uint32_t hdMirrorBytes = 0u;
inline std::uint8_t* hdGuestRamPtr = nullptr;

inline std::uint16_t fat12Cluster(const std::uint8_t* fat, std::uint16_t cluster) noexcept {
    const std::uint32_t byteOff = static_cast<std::uint32_t>(cluster) * 3u / 2u;
    if (byteOff + 1 >= 737280u / 512u * 3u) return 0xFFFu;
    if (cluster & 1u)
        return static_cast<std::uint16_t>(((fat[byteOff] >> 4) | (fat[byteOff + 1] << 4)) & 0x0FFFu);
    return static_cast<std::uint16_t>((fat[byteOff] | ((fat[byteOff + 1] & 0x0Fu) << 8)) & 0x0FFFu);
}

inline bool readFat12File(const std::uint8_t* img, std::size_t imgBytes,
                          const char* stem8, const char* ext3,
                          std::vector<std::uint8_t>& out) noexcept {
    if (!img || imgBytes < 512u) return false;
    constexpr std::uint32_t kDataStart = 1u + 2u * 3u + 7u;
    const auto* fat = img + 512u;
    const auto* root = img + (1u + 2u * 3u) * 512u;
    char stem[9]{}, ext[4]{};
    for (int i = 0; i < 8 && stem8[i]; ++i) stem[i] = stem8[i];
    for (int i = 0; i < 3 && ext3[i]; ++i) ext[i] = ext3[i];
    for (int ent = 0; ent < 112; ++ent) {
        const auto* e = root + ent * 32u;
        if (e[0] == 0 || e[0] == 0xE5) continue;
        if (std::memcmp(e, stem, 8) != 0 || std::memcmp(e + 8, ext, 3) != 0) continue;
        const std::uint16_t cluster = static_cast<std::uint16_t>(e[26] | (e[27] << 8));
        const std::uint32_t size = static_cast<std::uint32_t>(e[28] | (e[29] << 8) | (e[30] << 16) | (e[31] << 24));
        if (cluster < 2u) return false;
        out.assign(size, 0);
        std::uint32_t wrote = 0;
        std::uint16_t c = cluster;
        for (int guard = 0; guard < 512 && c >= 2u && c < 0xFF0u && wrote < size; ++guard) {
            const std::uint32_t off = (kDataStart + (c - 2u)) * 512u;
            if (off + 512u > imgBytes) return false;
            const std::uint32_t chunk = std::min<std::uint32_t>(512u, size - wrote);
            std::memcpy(out.data() + wrote, img + off, chunk);
            wrote += chunk;
            c = fat12Cluster(fat, c);
        }
        return wrote == size;
    }
    return false;
}

inline std::uint16_t fat16Cluster(const std::uint8_t* fat, std::uint16_t cluster) noexcept {
    const std::uint32_t foff = static_cast<std::uint32_t>(cluster) * 2u;
    return static_cast<std::uint16_t>(fat[foff] | (fat[foff + 1] << 8));
}

inline bool fat16Geom(const std::uint8_t* vol, std::size_t volBytes,
                      std::uint16_t& bps, std::uint8_t& spc, std::uint16_t& reserved,
                      std::uint8_t& fats, std::uint16_t& rootEnt, std::uint16_t& spf,
                      std::uint32_t& clusterBytes, std::uint32_t& dataStart,
                      const std::uint8_t*& fat, const std::uint8_t*& root) noexcept {
    if (!vol || volBytes < 512u) return false;
    bps = static_cast<std::uint16_t>(vol[11] | (vol[12] << 8));
    spc = vol[13];
    reserved = static_cast<std::uint16_t>(vol[14] | (vol[15] << 8));
    fats = vol[16];
    rootEnt = static_cast<std::uint16_t>(vol[17] | (vol[18] << 8));
    spf = static_cast<std::uint16_t>(vol[22] | (vol[23] << 8));
    if (bps != 512u || spc == 0 || fats == 0) return false;
    clusterBytes = static_cast<std::uint32_t>(bps) * spc;
    const std::uint32_t rootSectors = (static_cast<std::uint32_t>(rootEnt) * 32u + bps - 1u) / bps;
    dataStart = reserved + static_cast<std::uint32_t>(fats) * spf + rootSectors;
    fat = vol + static_cast<std::uint32_t>(reserved) * bps;
    root = vol + static_cast<std::uint32_t>(reserved + fats * spf) * bps;
    return true;
}

inline bool fat16NameTo83(const char* name8, char stem[9], char ext[4]) noexcept {
    if (!name8) return false;
    std::memset(stem, ' ', 8);
    std::memset(ext, ' ', 3);
    stem[8] = ext[3] = '\0';
    char upper[13]{};
    int n = 0;
    for (; name8[n] && n < 12; ++n)
        upper[n] = static_cast<char>(std::toupper(static_cast<unsigned char>(name8[n])));
    const char* dot = std::strchr(upper, '.');
    const int slen = dot ? static_cast<int>(dot - upper) : n;
    const int elen = dot ? static_cast<int>(std::strlen(dot + 1)) : 0;
    for (int i = 0; i < slen && i < 8; ++i) stem[i] = upper[i];
    for (int i = 0; i < elen && i < 3; ++i) ext[i] = dot[i + 1];
    return slen > 0 || elen > 0;
}

inline bool fat16ReadChain(const std::uint8_t* vol, std::size_t volBytes,
                           const std::uint8_t* fat, std::uint32_t dataStart,
                           std::uint32_t clusterBytes, std::uint16_t cluster,
                           std::uint32_t size, std::vector<std::uint8_t>& out) noexcept {
    if (cluster < 2u) return false;
    out.assign(size, 0);
    std::uint32_t wrote = 0;
    std::uint16_t c = cluster;
    const std::uint8_t spc = static_cast<std::uint8_t>(clusterBytes / 512u);
    const std::uint16_t bps = 512u;
    for (int guard = 0; guard < 65536 && c >= 2u && c < 0xFFF0u && wrote < size; ++guard) {
        const std::uint32_t off = (dataStart + (c - 2u) * spc) * bps;
        if (static_cast<std::uint64_t>(off) + clusterBytes > volBytes) break;
        const std::uint32_t chunk = std::min(clusterBytes, size - wrote);
        std::memcpy(out.data() + wrote, vol + off, chunk);
        wrote += chunk;
        c = fat16Cluster(fat, c);
    }
    if (wrote == size) return true;
    /* mk_ammo_hd stores large files in physically contiguous clusters; tolerate broken FAT links. */
    wrote = 0;
    for (std::uint16_t phys = cluster; phys < 0xFFF0u && wrote < size; ++phys) {
        const std::uint32_t off = (dataStart + (phys - 2u) * spc) * bps;
        if (static_cast<std::uint64_t>(off) + clusterBytes > volBytes) break;
        const std::uint32_t chunk = std::min(clusterBytes, size - wrote);
        std::memcpy(out.data() + wrote, vol + off, chunk);
        wrote += chunk;
    }
    return wrote == size;
}

inline bool fat16FindInDir(const std::uint8_t* dir, std::uint32_t dirBytes,
                           const char* stem8, const char* ext3,
                           std::uint16_t& cluster, std::uint32_t& size,
                           bool wantDir) noexcept {
    char stem[9]{}, ext[4]{};
    for (int i = 0; i < 8 && stem8[i]; ++i) stem[i] = stem8[i];
    for (int i = 0; i < 3 && ext3[i]; ++i) ext[i] = ext3[i];
    const std::uint32_t ents = dirBytes / 32u;
    for (std::uint32_t ent = 0; ent < ents; ++ent) {
        const auto* e = dir + ent * 32u;
        if (e[0] == 0) break;
        if (e[0] == 0xE5) continue;
        const bool isDir = (e[11] & 0x10) != 0;
        if (wantDir != isDir) continue;
        if (std::memcmp(e, stem, 8) != 0 || std::memcmp(e + 8, ext, 3) != 0) continue;
        cluster = static_cast<std::uint16_t>(e[26] | (e[27] << 8));
        size = static_cast<std::uint32_t>(e[28] | (e[29] << 8) | (e[30] << 16) | (e[31] << 24));
        return true;
    }
    return false;
}

inline bool readFat16File(const std::uint8_t* vol, std::size_t volBytes,
                          const char* stem8, const char* ext3,
                          std::vector<std::uint8_t>& out) noexcept {
    std::uint16_t bps{}, rootEnt{}, reserved{}, spf{};
    std::uint8_t spc{}, fats{};
    std::uint32_t clusterBytes{}, dataStart{};
    const std::uint8_t* fat = nullptr;
    const std::uint8_t* root = nullptr;
    if (!fat16Geom(vol, volBytes, bps, spc, reserved, fats, rootEnt, spf,
                   clusterBytes, dataStart, fat, root))
        return false;
    char stem[9]{}, ext[4]{};
    for (int i = 0; i < 8 && stem8[i]; ++i) stem[i] = stem8[i];
    for (int i = 0; i < 3 && ext3[i]; ++i) ext[i] = ext3[i];
    for (std::uint32_t ent = 0; ent < rootEnt; ++ent) {
        const auto* e = root + ent * 32u;
        if (e[0] == 0 || e[0] == 0xE5) continue;
        if (std::memcmp(e, stem, 8) != 0 || std::memcmp(e + 8, ext, 3) != 0) continue;
        const std::uint16_t cluster = static_cast<std::uint16_t>(e[26] | (e[27] << 8));
        const std::uint32_t size = static_cast<std::uint32_t>(e[28] | (e[29] << 8) | (e[30] << 16) | (e[31] << 24));
        return fat16ReadChain(vol, volBytes, fat, dataStart, clusterBytes, cluster, size, out);
    }
    return false;
}

inline bool readFat16Path(const std::uint8_t* vol, std::size_t volBytes,
                          const char* path, std::vector<std::uint8_t>& out) noexcept {
    if (!vol || !path) return false;
    std::uint16_t bps{}, rootEnt{}, reserved{}, spf{};
    std::uint8_t spc{}, fats{};
    std::uint32_t clusterBytes{}, dataStart{};
    const std::uint8_t* fat = nullptr;
    const std::uint8_t* root = nullptr;
    if (!fat16Geom(vol, volBytes, bps, spc, reserved, fats, rootEnt, spf,
                   clusterBytes, dataStart, fat, root))
        return false;

    const char* p = path;
    if ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) {
        if (p[1] == ':') p += 2;
    }
    while (*p == '\\' || *p == '/') ++p;

    const std::uint8_t* dir = root;
    std::uint32_t dirBytes = static_cast<std::uint32_t>(rootEnt) * 32u;
    char component[13]{};

    for (;;) {
        int ci = 0;
        while (*p && *p != '\\' && *p != '/' && ci < 12)
            component[ci++] = static_cast<char>(std::toupper(static_cast<unsigned char>(*p++)));
        component[ci] = '\0';
        if (!ci) return false;

        const bool last = (*p == '\0');
        if (!last) {
            while (*p == '\\' || *p == '/') ++p;
            char stem[9]{}, ext[4]{};
            if (!fat16NameTo83(component, stem, ext)) return false;
            std::uint16_t dirCluster = 0;
            std::uint32_t dirSize = 0;
            if (!fat16FindInDir(dir, dirBytes, stem, ext, dirCluster, dirSize, true))
                return false;
            if (dirCluster < 2u) return false;
            static thread_local std::vector<std::uint8_t> dirBuf;
            if (!fat16ReadChain(vol, volBytes, fat, dataStart, clusterBytes,
                                dirCluster, dirSize ? dirSize : clusterBytes, dirBuf))
                return false;
            dir = dirBuf.data();
            dirBytes = static_cast<std::uint32_t>(dirBuf.size());
            continue;
        }

        char stem[9]{}, ext[4]{};
        if (!fat16NameTo83(component, stem, ext)) return false;
        std::uint16_t fileCluster = 0;
        std::uint32_t fileSize = 0;
        if (!fat16FindInDir(dir, dirBytes, stem, ext, fileCluster, fileSize, false))
            return false;
        return fat16ReadChain(vol, volBytes, fat, dataStart, clusterBytes,
                              fileCluster, fileSize, out);
    }
}

inline bool parseDosName(const char* path, char& drive, char stem[9], char ext[4]) noexcept {
    if (!path) return false;
    drive = 'A';
    const char* p = path;
    if ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) {
        if (p[1] == ':') {
            drive = static_cast<char>(std::toupper(static_cast<unsigned char>(p[0])));
            p += 2;
        }
    }
    while (*p == '\\' || *p == '/') ++p;
    const char* base = p;
    for (const char* q = p; *q; ++q)
        if (*q == '\\' || *q == '/')
            base = q + 1;
    char file[13]{};
    int fi = 0;
    for (p = base; *p && *p != ' ' && *p != '\t' && fi < 12; ++p)
        file[fi++] = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
    if (!fi) return false;
    std::memset(stem, ' ', 8);
    std::memset(ext, ' ', 3);
    stem[8] = ext[3] = '\0';
    const char* dot = std::strchr(file, '.');
    const int slen = dot ? static_cast<int>(dot - file) : fi;
    const int elen = dot ? static_cast<int>(std::strlen(dot + 1)) : 0;
    for (int i = 0; i < slen && i < 8; ++i) stem[i] = file[i];
    for (int i = 0; i < elen && i < 3; ++i) ext[i] = dot[i + 1];
    return true;
}

inline bool pathHasSubdir(const char* path) noexcept {
    if (!path) return false;
    const char* p = path;
    if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) && p[1] == ':')
        p += 2;
    while (*p == '\\' || *p == '/') ++p;
    for (; *p; ++p)
        if (*p == '\\' || *p == '/')
            return true;
    return false;
}

inline bool readLooseAsset(const char* stem, const char* ext, std::vector<std::uint8_t>& out) noexcept;

inline bool lzexe91InfoLooksValid(const std::vector<std::uint8_t>& img) noexcept {
    if (img.size() < 0x200u || img[0] != 'M' || img[1] != 'Z' || img[28] != 'L' || img[31] != '1')
        return true;
    const std::uint16_t hdrPar = static_cast<std::uint16_t>(img[8] | (img[9] << 8));
    const std::uint16_t eCs = static_cast<std::uint16_t>(img[0x16] | (img[0x17] << 8));
    const std::uint32_t infoPos = (static_cast<std::uint32_t>(eCs) + hdrPar) << 4;
    if (infoPos + 10u >= img.size()) return false;
    const std::uint16_t inf4 = static_cast<std::uint16_t>(
        img[infoPos + 8u] | (img[infoPos + 9u] << 8));
    return inf4 == eCs;
}

inline bool readHostFile(const char* path, std::vector<std::uint8_t>& out) noexcept {
    char drive = 'A';
    char stem[9]{}, ext[4]{};
    if (!parseDosName(path, drive, stem, ext)) return false;
    const bool explicitDrive = ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')) && path[1] == ':';
    if (explicitDrive && (drive == 'C' || drive == 'c')) {
        if (hdReady && hdImage.size() >= HD_PART_LBA * HD_SECTOR_BYTES + 512u) {
            const auto* vol = hdImage.data() + HD_PART_LBA * HD_SECTOR_BYTES;
            const std::size_t volBytes = hdImage.size() - HD_PART_LBA * HD_SECTOR_BYTES;
            if (pathHasSubdir(path)) {
                if (readFat16Path(vol, volBytes, path, out) && lzexe91InfoLooksValid(out))
                    return true;
            } else if (readFat16File(vol, volBytes, stem, ext, out) && lzexe91InfoLooksValid(out)) {
                return true;
            }
            out.clear();
        }
        if (readLooseAsset(stem, ext, out)) return true;
        return false;
    }
    if (!floppyImage.empty() && readFat12File(floppyImage.data(), floppyImage.size(), stem, ext, out))
        return true;
    return readLooseAsset(stem, ext, out);
}

inline std::uint8_t* guestRam(std::uint8_t* mapped, std::size_t ramByteOffset) noexcept {
    return mapped + ramByteOffset;
}

inline bool loadHdFromEmbedded() noexcept {
#ifdef FIELD_DOS_EMBED_HD
    const unsigned char* sl = FieldDosEmbed::slice();
    const std::size_t slen = FieldDosEmbed::sliceSize();
    if (!sl || slen < HD_SECTOR_BYTES) return false;
    hdImage.assign(slen, 0);
    std::memcpy(hdImage.data(), sl, slen);
    hdImagePath.clear();
    hdReady = true;
    FieldRaid::init();
    return true;
#else
    return false;
#endif
}

inline bool loadHdImage(const std::filesystem::path& imgPath) noexcept {
    if (hdReady && hdImagePath == imgPath && !hdImage.empty())
        return true;
    hdReady = false;
    hdImage.clear();
    if (!std::filesystem::exists(imgPath)) return false;

    std::ifstream in(imgPath, std::ios::binary);
    if (!in) return false;

    in.seekg(0, std::ios::end);
    const auto sz = static_cast<std::size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    if (sz < HD_SECTOR_BYTES || static_cast<std::uint64_t>(sz) > HD_MAX_BYTES) return false;

    hdImage.resize(sz);
    in.read(reinterpret_cast<char*>(hdImage.data()), static_cast<std::streamsize>(sz));
    if (!in) {
        hdImage.clear();
        return false;
    }
    hdImagePath = imgPath;
    hdReady = true;
    FieldRaid::init();
    return true;
}

inline bool saveHdImage() noexcept {
    if (!hdReady || hdImage.empty() || hdImagePath.empty()) return false;
    const std::filesystem::path tmp = hdImagePath.string() + ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out.write(reinterpret_cast<const char*>(hdImage.data()),
                  static_cast<std::streamsize>(hdImage.size()));
        if (!out) return false;
    }
    std::error_code ec;
    std::filesystem::rename(tmp, hdImagePath, ec);
    if (ec) {
        std::filesystem::remove(hdImagePath, ec);
        ec.clear();
        std::filesystem::rename(tmp, hdImagePath, ec);
    }
    return !ec;
}

inline std::uint32_t hdLbaOffset(std::uint16_t cyl, std::uint16_t head, std::uint16_t sector) noexcept {
    if (!hdReady || sector < 1) return 0xFFFFFFFFu;
    const std::uint32_t spt = 63u;
    const std::uint32_t heads = 16u;
    const std::uint32_t lba = static_cast<std::uint32_t>(cyl) * heads * spt
                            + static_cast<std::uint32_t>(head) * spt
                            + static_cast<std::uint32_t>(sector - 1u);
    if (static_cast<std::uint64_t>(lba) * HD_SECTOR_BYTES >= FieldPlatform::HD_SIZE_BYTES)
        return 0xFFFFFFFFu;
    return lba * HD_SECTOR_BYTES;
}

inline std::uint32_t hdAbsLbaOffset(std::uint32_t lba) noexcept {
    if (!hdReady) return 0xFFFFFFFFu;
    const std::uint32_t byte = lba * HD_SECTOR_BYTES;
    if (static_cast<std::uint64_t>(byte) + HD_SECTOR_BYTES > FieldPlatform::HD_SIZE_BYTES)
        return 0xFFFFFFFFu;
    return byte;
}

inline std::uint32_t fat16DataStartLba(const std::uint8_t* bpb) noexcept {
    if (!bpb) return 0u;
    const std::uint16_t reserved = static_cast<std::uint16_t>(bpb[14] | (bpb[15] << 8));
    const std::uint8_t  fats     = bpb[16];
    const std::uint16_t rootEnt  = static_cast<std::uint16_t>(bpb[17] | (bpb[18] << 8));
    const std::uint16_t spf      = static_cast<std::uint16_t>(bpb[22] | (bpb[23] << 8));
    const std::uint32_t rootSec  = (static_cast<std::uint32_t>(rootEnt) * 32u + 511u) / 512u;
    return static_cast<std::uint32_t>(HD_PART_LBA) + reserved
         + static_cast<std::uint32_t>(fats) * spf + rootSec;
}

inline void patchHdBootSector(std::uint8_t* boot) noexcept {
    if (!boot) return;
    if (boot[0x3E] != 0x00u && boot[0x3E] != 0xFAu) return;
    static const std::uint8_t kCode[] = {
        0xFA, 0x31, 0xC0, 0x8E, 0xD8, 0x8E, 0xC0, 0x8E, 0xD0,
        0xBC, 0x00, 0x7C, 0xFB,
        0xBE, 0x6E, 0x7C, 0xAC, 0x08, 0xC0, 0x74, 0x06,
        0xB4, 0x0E, 0xB7, 0x07, 0xCD, 0x10, 0xEB, 0xF4,
        0xBE, 0x80, 0x7C, 0xB4, 0x42, 0xB2, 0x80, 0x8E, 0xD6,
        0xCD, 0x13, 0x72, 0x1A,
        0xEA, 0x00, 0x00, 0x00, 0x07,
        0xBE, 0xA0, 0x7C, 0xAC, 0x08, 0xC0, 0x74, 0x06,
        0xB4, 0x0E, 0xB7, 0x07, 0xCD, 0x10, 0xEB, 0xF4,
        0xF4,
    };
    static const char kMsg[] = "RTX-AMMOS C: boot MS-DOS 6.30...\r\n";
    static const char kErr[] = "Non-System disk or disk error\r\n";
    std::memcpy(boot + 0x3E, kCode, sizeof kCode);
    std::memcpy(boot + 0x6E, kMsg, sizeof kMsg);
    std::memcpy(boot + 0xA0, kErr, sizeof kErr);
    boot[0x80] = 0x10; boot[0x81] = 0x00;
    boot[0x82] = 0x28; boot[0x83] = 0x00;
    boot[0x84] = 0x00; boot[0x85] = 0x00;
    boot[0x86] = 0x00; boot[0x87] = 0x07;
    const std::uint32_t lba = fat16DataStartLba(boot);
    boot[0x88] = static_cast<std::uint8_t>(lba);
    boot[0x89] = static_cast<std::uint8_t>(lba >> 8);
    boot[0x8A] = static_cast<std::uint8_t>(lba >> 16);
    boot[0x8B] = static_cast<std::uint8_t>(lba >> 24);
    boot[510] = 0x55; boot[511] = 0xAA;
}

inline bool loadFloppyIntoGuest(
    void* fieldX86DieMapped,
    std::size_t ramByteOffset,
    const std::filesystem::path& imgPath) noexcept
{
    if (!fieldX86DieMapped) return false;
    if (!std::filesystem::exists(imgPath)) return false;

    std::ifstream in(imgPath, std::ios::binary);
    if (!in) return false;

    in.seekg(0, std::ios::end);
    const auto sz = static_cast<std::size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    if (sz < 512u || sz > DISK_IMAGE_BYTES) return false;

    auto* ram = guestRam(static_cast<std::uint8_t*>(fieldX86DieMapped), ramByteOffset);
    std::memset(ram, 0, GUEST_RAM_BYTES);

    floppyImage.resize(sz);
    in.read(reinterpret_cast<char*>(floppyImage.data()), static_cast<std::streamsize>(sz));
    if (!in) return false;
    std::memcpy(ram + DISK_IMAGE_BASE, floppyImage.data(), sz);

    std::memcpy(ram + BOOT_VECTOR, ram + DISK_IMAGE_BASE, 512u);

    const auto sig = *reinterpret_cast<const std::uint16_t*>(ram + BOOT_VECTOR + 510u);
    if (sig != 0xAA55u) return false;

    struct DieRegs {
        std::uint32_t EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP;
        std::uint32_t R8, R9, R10, R11, R12, R13, R14, R15;
        std::uint32_t CS, DS, ES, FS, GS, SS;
        std::uint32_t EIP, EFLAGS;
        std::uint32_t CR0, CR2, CR3, CR4, CR8;
    };

    auto* die = static_cast<DieRegs*>(fieldX86DieMapped);
    die->EAX = die->EBX = die->ECX = die->EDX = 0u;
    die->ESI = die->EDI = die->EBP = 0u;
    die->ESP = BOOT_VECTOR;
    die->R8 = die->R9 = die->R10 = die->R11 = die->R12 = die->R13 = die->R14 = die->R15 = 0u;
    die->EIP = BOOT_VECTOR;
    die->EFLAGS = 0x00000202u;
    die->CS = die->DS = die->ES = die->SS = 0u;
    die->FS = die->GS = 0u;
    die->CR0 = 0u;
    die->CR2 = die->CR3 = die->CR4 = die->CR8 = 0u;

    *reinterpret_cast<std::uint32_t*>(
        static_cast<std::uint8_t*>(fieldX86DieMapped) + FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET) = 0u;

    return true;
}

inline bool hasDosAssets(const std::filesystem::path& root) noexcept {
    const auto dos = root / "assets" / "dos";
    return std::filesystem::exists(dos / "rtx_dos_hd.img")
        || std::filesystem::exists(dos / "rtx_dos720.img");
}

inline std::filesystem::path assetRoot() noexcept {
    namespace fs = std::filesystem;
    static fs::path cached;
    static bool ready = false;
    if (ready) return cached;
    if (hasDosAssets(fs::current_path())) {
        cached = fs::current_path();
    } else {
        try {
            auto dir = fs::canonical("/proc/self/exe").parent_path();
            for (int depth = 0; depth < 8; ++depth) {
                if (hasDosAssets(dir)) {
                    cached = dir;
                    break;
                }
                const auto parent = dir.parent_path();
                if (parent == dir) break;
                dir = parent;
            }
        } catch (...) {}
        if (cached.empty())
            cached = fs::current_path();
    }
    ready = true;
    return cached;
}

inline void trimFat83(std::string& s) noexcept {
    while (!s.empty() && s.back() == ' ') s.pop_back();
}

inline bool readLooseAsset(const char* stem, const char* ext, std::vector<std::uint8_t>& out) noexcept {
    if (!stem || !stem[0]) return false;
    const auto root = assetRoot();
    auto tryPath = [&](const std::filesystem::path& p) -> bool {
        if (!std::filesystem::exists(p)) return false;
        std::ifstream in(p, std::ios::binary);
        if (!in) return false;
        in.seekg(0, std::ios::end);
        const auto sz = static_cast<std::size_t>(in.tellg());
        in.seekg(0, std::ios::beg);
        if (sz == 0) return false;
        out.resize(sz);
        in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(sz));
        return static_cast<bool>(in);
    };
    std::string base(stem);
    std::string xext = ext ? ext : "";
    for (auto& c : base) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    for (auto& c : xext) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    trimFat83(base);
    trimFat83(xext);
    if (xext == "WAD") {
        if (tryPath(root / "assets" / "wads" / (base + "." + xext))) return true;
        if (tryPath(root / "assets" / "wads" / (base + "." + "wad"))) return true;
        if (tryPath(root / "assets" / "dos" / (base + "." + xext))) return true;
        if (tryPath(root / "assets" / "dos" / (base + "." + "wad"))) return true;
        const auto wdir = root / "assets" / "wads";
        if (std::filesystem::is_directory(wdir)) {
            for (const auto& ent : std::filesystem::directory_iterator(wdir)) {
                if (!ent.is_regular_file()) continue;
                const auto stem = ent.path().stem().string();
                std::string up(stem);
                for (auto& c : up) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                if (up == base && tryPath(ent.path())) return true;
            }
        }
    }
    if (tryPath(root / "assets" / "dos" / (base + (xext.empty() ? "" : "." + xext)))) return true;
    const auto games = root / "assets" / "dos" / "games";
    if (std::filesystem::is_directory(games)) {
        for (const auto& ent : std::filesystem::recursive_directory_iterator(games)) {
            if (!ent.is_regular_file()) continue;
            if (ent.path().filename().string().size() > 13u) continue;
            std::string fn = ent.path().filename().string();
            for (auto& c : fn) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            const auto dot = fn.find('.');
            std::string gstem = dot == std::string::npos ? fn : fn.substr(0, dot);
            std::string gext  = dot == std::string::npos ? "" : fn.substr(dot + 1);
            trimFat83(gstem);
            trimFat83(gext);
            if (gstem == base && gext == xext && tryPath(ent.path())) return true;
        }
    }
    return false;
}

inline std::filesystem::path resolveRoot(const std::filesystem::path& hint = {}) noexcept {
    if (!hint.empty() && hint != "." && hasDosAssets(hint)) return hint;
    return assetRoot();
}

inline std::filesystem::path defaultImagePath(const std::filesystem::path& projectRoot = {}) {
    const auto root = resolveRoot(projectRoot);
    return root / "assets" / "dos" / "rtx_dos720.img";
}

inline std::filesystem::path defaultHdPath(const std::filesystem::path& projectRoot = {}) {
    const auto root = resolveRoot(projectRoot);
    return root / "assets" / "dos" / "rtx_dos_hd.img";
}

inline std::uint32_t hdMirrorCopyBytes() noexcept;

inline bool mirrorHdToGuestGpu(
    void* fieldX86DieMapped,
    std::size_t ramByteOffset) noexcept
{
    if (!fieldX86DieMapped || !hdReady || hdImage.empty()) {
        hdMirrorBytes = 0u;
        return false;
    }
    const std::uint32_t copyBytes = hdMirrorCopyBytes();
    if (copyBytes == 0u) return false;
    const std::uint32_t mirrorBase = FieldPlatform::HD_MIRROR_BYTE;
    if (mirrorBase + copyBytes > GUEST_RAM_BYTES) return false;
    auto* ram = guestRam(static_cast<std::uint8_t*>(fieldX86DieMapped), ramByteOffset);
    std::memcpy(ram + mirrorBase, hdImage.data(), copyBytes);
    hdMirrorBytes = copyBytes;
    hdGuestRamPtr = ram;
    return true;
}

/* Boot C: partition — CONFIG.SYS / AUTOEXEC.BAT via IO.SYS chain (no floppy). */
inline bool loadHdIntoGuest(
    void* fieldX86DieMapped,
    std::size_t ramByteOffset) noexcept
{
    if (!fieldX86DieMapped || !hdReady || hdImage.size() < HD_PART_LBA * HD_SECTOR_BYTES + 512u)
        return false;

    auto* ram = guestRam(static_cast<std::uint8_t*>(fieldX86DieMapped), ramByteOffset);
    /* Conventional tier only — skip zeroing full 64 MiB die on every boot. */
    const std::uint32_t kBootClearBytes = FieldRtxMemory::bootClearBytes();
    std::memset(ram, 0, kBootClearBytes);
    mirrorHdToGuestGpu(fieldX86DieMapped, ramByteOffset);

    std::uint8_t boot[512]{};
    const std::uint32_t partOff = HD_PART_LBA * HD_SECTOR_BYTES;
    std::memcpy(boot, hdImage.data() + partOff, 512u);
    patchHdBootSector(boot);
    std::memcpy(ram + BOOT_VECTOR, boot, 512u);
    std::memcpy(hdImage.data() + partOff, boot, 512u);

    struct DieRegs {
        std::uint32_t EAX, EBX, ECX, EDX, ESI, EDI, EBP, ESP;
        std::uint32_t R8, R9, R10, R11, R12, R13, R14, R15;
        std::uint32_t CS, DS, ES, FS, GS, SS;
        std::uint32_t EIP, EFLAGS;
        std::uint32_t CR0, CR2, CR3, CR4, CR8;
    };

    auto* die = static_cast<DieRegs*>(fieldX86DieMapped);
    die->EAX = die->EBX = die->ECX = 0u;
    die->EDX = 0x80u;
    die->ESI = die->EDI = die->EBP = 0u;
    die->ESP = BOOT_VECTOR;
    die->R8 = die->R9 = die->R10 = die->R11 = die->R12 = die->R13 = die->R14 = die->R15 = 0u;
    die->EIP = BOOT_VECTOR;
    die->EFLAGS = 0x00000202u;
    die->CS = die->DS = die->ES = die->SS = 0u;
    die->FS = die->GS = 0u;
    die->CR0 = 0u;
    die->CR2 = die->CR3 = die->CR4 = die->CR8 = 0u;

    *reinterpret_cast<std::uint32_t*>(
        static_cast<std::uint8_t*>(fieldX86DieMapped) + FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET) = 0u;
    return true;
}

inline bool bootGuest(
    void* fieldX86DieMapped,
    std::size_t ramByteOffset,
    const std::filesystem::path& projectRoot = {}) noexcept
{
    const auto path = defaultHdPath(projectRoot);
    if (!hdReady || hdImagePath != path) {
        if (!loadHdImage(path) && !loadHdFromEmbedded()) return false;
    }
    return loadHdIntoGuest(fieldX86DieMapped, ramByteOffset);
}

struct FatRootEntry {
    char name[13]{};
    std::uint32_t size = 0;
    bool isDir = false;
};

inline bool fat16EnumerateDir(const std::uint8_t* dir, std::uint32_t dirBytes,
                              std::vector<FatRootEntry>& out) noexcept {
    out.clear();
    const std::uint32_t ents = dirBytes / 32u;
    for (std::uint32_t ent = 0; ent < ents; ++ent) {
        const auto* e = dir + ent * 32u;
        if (e[0] == 0) break;
        if (e[0] == 0xE5) continue;
        if (e[11] & 0x08) continue;
        FatRootEntry fe{};
        char stem[9]{}, ext[4]{};
        for (int j = 0; j < 8; ++j)
            stem[j] = e[j] == ' ' ? '\0' : static_cast<char>(e[j]);
        for (int j = 0; j < 3; ++j)
            ext[j] = e[8 + j] == ' ' ? '\0' : static_cast<char>(e[8 + j]);
        if (ext[0])
            std::snprintf(fe.name, sizeof fe.name, "%s.%s", stem, ext);
        else
            std::snprintf(fe.name, sizeof fe.name, "%s", stem);
        if (std::strcmp(fe.name, ".") == 0 || std::strcmp(fe.name, "..") == 0)
            continue;
        fe.size = static_cast<std::uint32_t>(e[28] | (e[29] << 8) | (e[30] << 16) | (e[31] << 24));
        fe.isDir = (e[11] & 0x10) != 0;
        out.push_back(fe);
    }
    return true;
}

inline bool listFat16Dir(const char* dirPath, std::vector<FatRootEntry>& out) noexcept {
    out.clear();
    if (!dirPath || !hdReady || hdImage.size() < HD_PART_LBA * HD_SECTOR_BYTES + 512u)
        return false;
    const auto* vol = hdImage.data() + HD_PART_LBA * HD_SECTOR_BYTES;
    const std::size_t volBytes = hdImage.size() - HD_PART_LBA * HD_SECTOR_BYTES;
    std::uint16_t bps{}, rootEnt{}, reserved{}, spf{};
    std::uint8_t spc{}, fats{};
    std::uint32_t clusterBytes{}, dataStart{};
    const std::uint8_t* fat = nullptr;
    const std::uint8_t* root = nullptr;
    if (!fat16Geom(vol, volBytes, bps, spc, reserved, fats, rootEnt, spf,
                   clusterBytes, dataStart, fat, root))
        return false;

    const char* p = dirPath;
    if ((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z')) {
        if (p[1] == ':') p += 2;
    }
    while (*p == '\\' || *p == '/') ++p;

    const std::uint8_t* dir = root;
    std::uint32_t dirBytes = static_cast<std::uint32_t>(rootEnt) * 32u;
    static thread_local std::vector<std::uint8_t> dirBuf;

    if (!*p)
        return fat16EnumerateDir(dir, dirBytes, out);

    char component[13]{};
    for (;;) {
        int ci = 0;
        while (*p && *p != '\\' && *p != '/' && ci < 12)
            component[ci++] = static_cast<char>(std::toupper(static_cast<unsigned char>(*p++)));
        component[ci] = '\0';
        if (!ci) return false;

        const bool last = (*p == '\0');
        if (!last) {
            while (*p == '\\' || *p == '/') ++p;
            char stem[9]{}, ext[4]{};
            if (!fat16NameTo83(component, stem, ext)) return false;
            std::uint16_t dirCluster = 0;
            std::uint32_t dirSize = 0;
            if (!fat16FindInDir(dir, dirBytes, stem, ext, dirCluster, dirSize, true))
                return false;
            if (dirCluster < 2u) return false;
            if (!fat16ReadChain(vol, volBytes, fat, dataStart, clusterBytes,
                                dirCluster, dirSize ? dirSize : clusterBytes, dirBuf))
                return false;
            dir = dirBuf.data();
            dirBytes = static_cast<std::uint32_t>(dirBuf.size());
            continue;
        }

        char stem[9]{}, ext[4]{};
        if (!fat16NameTo83(component, stem, ext)) return false;
        std::uint16_t dirCluster = 0;
        std::uint32_t dirSize = 0;
        if (!fat16FindInDir(dir, dirBytes, stem, ext, dirCluster, dirSize, true))
            return false;
        if (dirCluster < 2u) return false;
        if (!fat16ReadChain(vol, volBytes, fat, dataStart, clusterBytes,
                            dirCluster, dirSize ? dirSize : clusterBytes, dirBuf))
            return false;
        return fat16EnumerateDir(dirBuf.data(), static_cast<std::uint32_t>(dirBuf.size()), out);
    }
}

inline bool listFat16Root(std::vector<FatRootEntry>& out) noexcept {
    out.clear();
    if (!hdReady || hdImage.size() < HD_PART_LBA * HD_SECTOR_BYTES + 512u) return false;
    const auto* vol = hdImage.data() + HD_PART_LBA * HD_SECTOR_BYTES;
    const std::size_t volBytes = hdImage.size() - HD_PART_LBA * HD_SECTOR_BYTES;
    const std::uint16_t bps = static_cast<std::uint16_t>(vol[11] | (vol[12] << 8));
    const std::uint16_t reserved = static_cast<std::uint16_t>(vol[14] | (vol[15] << 8));
    const std::uint8_t fats = vol[16];
    const std::uint16_t rootEnt = static_cast<std::uint16_t>(vol[17] | (vol[18] << 8));
    const std::uint16_t spf = static_cast<std::uint16_t>(vol[22] | (vol[23] << 8));
    if (bps != 512u || fats == 0) return false;
    const auto* root = vol + static_cast<std::uint32_t>(reserved + fats * spf) * bps;
    for (std::uint32_t ent = 0; ent < rootEnt; ++ent) {
        const auto* e = root + ent * 32u;
        if (static_cast<std::size_t>(e - vol) + 32u > volBytes) break;
        if (e[0] == 0 || e[0] == 0xE5) continue;
        if (e[11] & 0x08) continue;
        FatRootEntry fe{};
        char stem[9]{}, ext[4]{};
        for (int j = 0; j < 8; ++j)
            stem[j] = e[j] == ' ' ? '\0' : static_cast<char>(e[j]);
        for (int j = 0; j < 3; ++j)
            ext[j] = e[8 + j] == ' ' ? '\0' : static_cast<char>(e[8 + j]);
        if (ext[0])
            std::snprintf(fe.name, sizeof fe.name, "%s.%s", stem, ext);
        else
            std::snprintf(fe.name, sizeof fe.name, "%s", stem);
        fe.size = static_cast<std::uint32_t>(e[28] | (e[29] << 8) | (e[30] << 16) | (e[31] << 24));
        fe.isDir = (e[11] & 0x10) != 0;
        out.push_back(fe);
    }
    return !out.empty();
}

inline bool listFat12Root(std::vector<FatRootEntry>& out) noexcept {
    out.clear();
    if (floppyImage.size() < 512u) return false;
    const auto* root = floppyImage.data() + (1u + 2u * 3u) * 512u;
    for (int ent = 0; ent < 112; ++ent) {
        const auto* e = root + ent * 32u;
        if (e[0] == 0) break;
        if (e[0] == 0xE5) continue;
        FatRootEntry fe{};
        char stem[9]{}, ext[4]{};
        for (int j = 0; j < 8; ++j)
            stem[j] = e[j] == ' ' ? '\0' : static_cast<char>(e[j]);
        for (int j = 0; j < 3; ++j)
            ext[j] = e[8 + j] == ' ' ? '\0' : static_cast<char>(e[8 + j]);
        if (ext[0])
            std::snprintf(fe.name, sizeof fe.name, "%s.%s", stem, ext);
        else
            std::snprintf(fe.name, sizeof fe.name, "%s", stem);
        fe.size = static_cast<std::uint32_t>(e[28] | (e[29] << 8) | (e[30] << 16) | (e[31] << 24));
        fe.isDir = (e[11] & 0x10) != 0;
        out.push_back(fe);
    }
    return !out.empty();
}

inline std::string hdVolumeLabel() noexcept {
    if (!hdReady || hdImage.size() < HD_PART_LBA * HD_SECTOR_BYTES + 54u) return "RTXDOS";
    const auto* vol = hdImage.data() + HD_PART_LBA * HD_SECTOR_BYTES;
    char lbl[12]{};
    std::memcpy(lbl, vol + 43, 11);
    for (int i = 10; i >= 0; --i) {
        if (lbl[i] == ' ') lbl[i] = '\0';
        else break;
    }
    return lbl[0] ? std::string(lbl) : std::string("RTXDOS");
}

inline std::uint8_t readHdByte(std::uint32_t byteOff, const std::uint8_t* guestRamPtr) noexcept;
inline void writeHdByte(std::uint32_t byteOff, std::uint8_t val, std::uint8_t* guestRamPtr) noexcept;
inline void notifyHdHostWrite(std::uint64_t off, std::uint32_t len,
                              std::uint8_t* guestRamPtr) noexcept;

} // namespace FieldDos

#include "FieldDosRaid.hpp"