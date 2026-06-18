#pragma once

// RTX-AMMOS CD-ROM — ISO9660 image as drive D: (INT 13h / MSCDEX-ready).

#include "FieldDos.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace FieldCdRom {

constexpr std::uint8_t DRIVE_LETTER = 'D';
constexpr std::uint32_t SECTOR_BYTES = 512u;

inline std::vector<std::uint8_t> isoImage;
inline bool ready = false;
inline std::string volumeLabel = "RTXCD001";
inline std::string isoPath;

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
    isoImage.clear();
    ready = false;
    isoPath.clear();
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
        isoImage.clear();
        return false;
    }
    parseIsoLabel(isoImage.data(), isoImage.size());
    isoPath = path.string();
    ready = true;
    return true;
}

inline bool autoMount(const std::filesystem::path& projectRoot) noexcept {
    const auto dir = defaultIncomingDir(projectRoot);
    if (!std::filesystem::is_directory(dir)) return false;
    for (const auto& ent : std::filesystem::directory_iterator(dir)) {
        if (!ent.is_regular_file()) continue;
        const auto ext = ent.path().extension().string();
        if (ext == ".iso" || ext == ".ISO" || ext == ".bin" || ext == ".BIN")
            return loadIso(ent.path());
    }
    return false;
}

inline std::uint32_t sectorCount() noexcept {
    if (!ready || isoImage.empty()) return 0u;
    return static_cast<std::uint32_t>((isoImage.size() + 2047u) / 2048u);
}

inline bool readSector2048(std::uint32_t lba, std::uint8_t* out2048) noexcept {
    if (!ready || !out2048) return false;
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

} // namespace FieldCdRom