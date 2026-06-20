#pragma once

// RTX-AMMOS CD-ROM — ISO9660 image file or host USB/DVD block device (INT 13h / MSCDEX).

#include "FieldCdRomHost.hpp"
#include "FieldDos.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <unistd.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace FieldCdRom {

constexpr std::uint8_t DRIVE_LETTER = 'D';
constexpr std::uint32_t SECTOR_BYTES = 512u;

enum class SourceKind : std::uint8_t { None = 0, FileImage, HostDevice };

inline std::vector<std::uint8_t> isoImage;
inline bool ready = false;
inline std::string volumeLabel = "RTXCD001";
inline std::string isoPath;
inline SourceKind sourceKind = SourceKind::None;
inline int hostFd = -1;
inline std::uint32_t hostSectorTotal = 0u;

inline void unload() noexcept {
    isoImage.clear();
    if (hostFd >= 0) {
        close(hostFd);
        hostFd = -1;
    }
    ready = false;
    isoPath.clear();
    sourceKind = SourceKind::None;
    hostSectorTotal = 0u;
    volumeLabel = "RTXCD001";
}

inline bool parseIsoLabel(const std::uint8_t* data, std::size_t sz) noexcept {
    if (sz < 2048u * 17u) return false;
    constexpr std::size_t pvdOff = 16u * 2048u;
    if (data[pvdOff] != 1u) return false;
    if (std::memcmp(data + pvdOff + 1, "CD001", 5) != 0) return false;
    char lbl[33]{};
    std::memcpy(lbl, data + pvdOff + 40, 32);
    for (int i = 31; i >= 0; --i) {
        if (lbl[i] == ' ') lbl[i] = '\0';
        else break;
    }
    if (lbl[0]) volumeLabel = lbl;
    return true;
}

inline std::filesystem::path defaultIncomingDir(const std::filesystem::path& root) {
    return root / "assets" / "dos" / "incoming" / "cd";
}

inline bool loadIso(const std::filesystem::path& path) noexcept {
    unload();
    if (!std::filesystem::exists(path)) return false;
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    in.seekg(0, std::ios::end);
    const auto sz = static_cast<std::size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    if (sz < 2048u) return false;
    isoImage.resize(sz);
    in.read(reinterpret_cast<char*>(isoImage.data()), static_cast<std::streamsize>(sz));
    if (!in) {
        unload();
        return false;
    }
    parseIsoLabel(isoImage.data(), isoImage.size());
    isoPath = path.string();
    sourceKind = SourceKind::FileImage;
    ready = true;
    return true;
}

inline bool mountHostDevice(const char* devPath) noexcept {
    if (!devPath || !devPath[0]) return false;
    unload();
    char label[33]{};
    std::uint32_t sectors = 0u;
    int fd = -1;
    if (!FieldCdRomHost::mountDevice(devPath, fd, sectors, label, sizeof label))
        return false;
    hostFd = fd;
    hostSectorTotal = sectors;
    volumeLabel = label[0] ? label : "HOST_CD";
    isoPath = devPath;
    sourceKind = SourceKind::HostDevice;
    ready = true;
    return true;
}

inline bool autoMountHost() noexcept {
    char dev[128]{};
    if (!FieldCdRomHost::detectOptical(dev, sizeof dev))
        return false;
    return mountHostDevice(dev);
}

// Boot-safe: incoming .iso only. Host USB/DVD can block on open/pread — use mountCd().
inline bool autoMount(const std::filesystem::path& projectRoot) noexcept {
    const auto dir = defaultIncomingDir(projectRoot);
    if (std::filesystem::is_directory(dir)) {
        for (const auto& ent : std::filesystem::directory_iterator(dir)) {
            if (!ent.is_regular_file()) continue;
            const auto ext = ent.path().extension().string();
            if (ext == ".iso" || ext == ".ISO" || ext == ".bin" || ext == ".BIN") {
                if (loadIso(ent.path()))
                    return true;
            }
        }
    }
    return false;
}

inline bool mountAny(const std::filesystem::path& projectRoot) noexcept {
    if (autoMount(projectRoot)) return true;
    return autoMountHost();
}

inline std::uint32_t sectorCount() noexcept {
    if (!ready) return 0u;
    if (sourceKind == SourceKind::HostDevice)
        return hostSectorTotal;
    if (isoImage.empty()) return 0u;
    return static_cast<std::uint32_t>((isoImage.size() + 2047u) / 2048u);
}

inline bool readSector2048(std::uint32_t lba, std::uint8_t* out2048) noexcept {
    if (!ready || !out2048) return false;
    if (sourceKind == SourceKind::HostDevice)
        return FieldCdRomHost::readSector2048(hostFd, lba, out2048);
    const std::size_t off = static_cast<std::size_t>(lba) * 2048u;
    if (off + 2048u > isoImage.size()) return false;
    std::memcpy(out2048, isoImage.data() + off, 2048u);
    return true;
}

inline bool readSector512(std::uint32_t lba, std::uint8_t* out512) noexcept {
    if (!ready || !out512) return false;
    std::uint8_t sec[2048]{};
    const std::uint32_t isoLba = lba / 4u;
    const std::uint32_t sub = lba % 4u;
    if (!readSector2048(isoLba, sec)) return false;
    std::memcpy(out512, sec + sub * 512u, 512u);
    return true;
}

inline bool listRoot(std::vector<std::string>& names) noexcept {
    names.clear();
    if (!ready) return false;
    std::uint8_t root[2048]{};
    if (!readSector2048(16u + 2u, root)) return false;
    for (std::size_t off = 0; off + 33u <= 2048u; ) {
        const std::uint8_t len = root[off];
        if (len == 0) break;
        if (len < 33u) break;
        const std::uint8_t flags = root[off + 25];
        if (!(flags & 0x02u)) { off += len; continue; }
        char nm[13]{};
        const std::uint8_t nlen = root[off + 32];
        const std::size_t copy = std::min<std::size_t>(nlen, 12u);
        std::memcpy(nm, root + off + 33, copy);
        if (nm[0]) names.emplace_back(nm);
        off += len;
    }
    return true;
}

inline bool isHostDevice() noexcept {
    return ready && sourceKind == SourceKind::HostDevice;
}

} // namespace FieldCdRom