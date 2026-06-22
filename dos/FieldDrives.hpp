#pragma once

// RTX-AMMOS field drive rack — A: floppy B: (reserved) C: AMMOFAT D: MSCDEX E: host bridge.

#include "FieldAmmoFat.hpp"
#include "FieldStorage.hpp"
#include "FieldCdRom.hpp"
#include "FieldDos.hpp"
#include "FieldLayerShell.hpp"
#include "FieldMscdex.hpp"
#include "FieldPlatform.hpp"
#include "FieldRtxMemory.hpp"

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace FieldDrives {

enum class Kind : std::uint8_t {
    None = 0,
    Floppy,
    Fixed,
    CdRom,
    HostBridge,
};

struct Drive {
    char letter = '?';
    Kind kind = Kind::None;
    FieldLayer::LayerId layer = FieldLayer::LayerId::Ram;
    const char* driver = "";
    const char* volume = "";
    const char* fieldTag = "";
    bool ready = false;
    bool enabled = true;
    std::uint32_t totalBytes = 0;
    std::uint32_t freeBytes = 0;
    std::uint32_t sectors = 0;
};

constexpr int kSlotCount = 5; /* A B C D E */
inline Drive slots[kSlotCount]{};
inline char currentLetter = 'C';
inline std::uint32_t readyMask = 0u;
inline std::uint32_t readyCount = 0u;

namespace detail {

struct Fingerprint {
    bool     floppyReady = false;
    std::uint32_t floppyBytes = 0u;
    bool     hdReady = false;
    bool     fatMounted = false;
    bool     cdReady = false;
    std::uint32_t cdBytes = 0u;
    bool     hostReady = false;
    bool     fatShell = true;
    bool     mscdexShell = true;
};

inline Fingerprint lastFp{};
inline bool cacheValid = false;
inline std::uint32_t hostPollTick = 0u;
inline bool hostReadyCached = false;
inline std::filesystem::path hostRootPath;

constexpr std::uint32_t kHostPollInterval = 90u; /* ~1.5 s @ 60 fps */

inline const std::filesystem::path& hostRoot() noexcept {
    if (hostRootPath.empty())
        hostRootPath = FieldDos::assetRoot() / "assets" / "dos" / "incoming" / "host";
    return hostRootPath;
}

inline bool pollHostBridge(std::uint32_t tick, bool force) noexcept {
    if (force || tick - hostPollTick >= kHostPollInterval) {
        hostPollTick = tick;
        hostReadyCached = std::filesystem::is_directory(hostRoot());
    }
    return hostReadyCached;
}

inline Fingerprint capture() noexcept {
    Fingerprint fp{};
    fp.floppyReady = !FieldDos::floppyImage.empty();
    fp.floppyBytes = static_cast<std::uint32_t>(FieldDos::floppyImage.size());
    fp.hdReady = FieldDos::hdReady;
    fp.fatMounted = FieldAmmoFat::mounted;
    fp.cdReady = FieldCdRom::ready;
    fp.cdBytes = FieldCdRom::ready
        ? static_cast<std::uint32_t>(FieldCdRom::sectorCount()) * 2048u : 0u;
    fp.hostReady = hostReadyCached;
    fp.fatShell = FieldLayer::isShellActive(FieldLayer::LayerId::Fat);
    fp.mscdexShell = FieldLayer::isShellActive(FieldLayer::LayerId::Mscdex);
    return fp;
}

inline bool changed(const Fingerprint& a, const Fingerprint& b) noexcept {
    return std::memcmp(&a, &b, sizeof a) != 0;
}

} // namespace detail

inline int letterIndex(char letter) noexcept {
    const char u = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
    if (u < 'A' || u > 'Z') return -1;
    return static_cast<int>(u - 'A');
}

inline Drive* byLetter(char letter) noexcept {
    const int i = letterIndex(letter);
    if (i < 0 || i >= kSlotCount) return nullptr;
    return &slots[i];
}

inline void setReadyMask() noexcept {
    readyMask = 0u;
    readyCount = 0u;
    for (const auto& s : slots) {
        if (!s.ready || !s.enabled) continue;
        ++readyCount;
        const int i = letterIndex(s.letter);
        if (i >= 0 && i < 26)
            readyMask |= 1u << static_cast<unsigned>(i);
    }
}

inline void invalidate() noexcept {
    detail::cacheValid = false;
}

inline void refresh(bool force = false) noexcept {
    static bool storageMounted = false;
    if (!storageMounted) {
        FieldStorage::mountMultiFS();
        storageMounted = true;
    }
    if (FieldDos::hdReady && !FieldAmmoFat::mounted)
        FieldAmmoFat::mount();

    ++detail::hostPollTick;
    detail::pollHostBridge(detail::hostPollTick, force);

    const detail::Fingerprint fp = detail::capture();
    if (!force && detail::cacheValid && !detail::changed(fp, detail::lastFp))
        return;

    detail::lastFp = fp;
    detail::cacheValid = true;

    static char volC[16] = "RTXDOS";
    static char volD[33] = "RTXCD001";
    if (fp.hdReady)
        std::snprintf(volC, sizeof volC, "%s", FieldDos::hdVolumeLabel().c_str());
    if (fp.cdReady)
        std::snprintf(volD, sizeof volD, "%s", FieldCdRom::volumeLabel.c_str());

    slots[0] = Drive{
        'A', Kind::Floppy, FieldLayer::LayerId::Ram, "RTXDRV.SYS", "RTXBOOT",
        "L0-FLOPPY", fp.floppyReady, true, fp.floppyBytes, fp.floppyBytes, 1440u};
    slots[1] = Drive{
        'B', Kind::None, FieldLayer::LayerId::Ram, "", "",
        "reserved", false, false, 0u, 0u, 0u};
    std::uint32_t cFree = 0u;
    if (fp.hdReady) {
        if (!FieldAmmoFat::mounted) FieldAmmoFat::mount();
        if (FieldAmmoFat::mounted) cFree = FieldAmmoFat::freeBytes();
    }
    slots[2] = Drive{
        'C', Kind::Fixed, FieldLayer::LayerId::Fat, "AMMOFAT.SYS", volC,
        "L2-FAT16", fp.hdReady && fp.fatMounted, fp.fatShell,
        FieldPlatform::HD_SIZE_BYTES32, cFree,
        FieldPlatform::HD_SIZE_BYTES32 / 512u};
    slots[3] = Drive{
        'D', Kind::CdRom, FieldLayer::LayerId::Mscdex, "RTXCD.SYS", volD,
        "L3-ISO9660", fp.cdReady, fp.mscdexShell,
        fp.cdBytes, 0u, FieldCdRom::sectorCount()};
    slots[4] = Drive{
        'E', Kind::HostBridge, FieldLayer::LayerId::Io, "RTXHOST.SYS",
        "RTXHOST", "L6-HOST", fp.hostReady, true, 0u, 0u, 0u};

    if (FieldRtxMemory::mscdexLive() && fp.cdReady && slots[3].ready)
        FieldMscdex::install();

    setReadyMask();
}

inline bool setCurrent(char letter) noexcept {
    refresh();
    Drive* d = byLetter(letter);
    if (!d || !d->ready || !d->enabled) return false;
    currentLetter = static_cast<char>(std::toupper(static_cast<unsigned char>(letter)));
    return true;
}

inline void syncFromGuest(const std::uint8_t* ram) noexcept {
    (void)ram;
    refresh();
    if (!byLetter(currentLetter) || !byLetter(currentLetter)->ready)
        currentLetter = slots[2].ready ? 'C' : 'A';
}

inline void tick(std::uint8_t* ram, float /*dt*/) noexcept {
    (void)ram;
    refresh();
}

inline void packDataBus(std::uint32_t* bus, std::size_t count) noexcept {
    if (!bus || count < 64u) return;
    refresh();
    const std::uint32_t cur = static_cast<std::uint32_t>(letterIndex(currentLetter));
    bus[63] = (readyMask & 0xFFu) | (cur << 8) | (readyCount << 16);
}

inline void formatLine(char* buf, std::size_t len, const Drive& d) noexcept {
    const char* kind = "????";
    switch (d.kind) {
    case Kind::Floppy: kind = "floppy"; break;
    case Kind::Fixed: kind = "fixed"; break;
    case Kind::CdRom: kind = "cdrom"; break;
    case Kind::HostBridge: kind = "host"; break;
    default: kind = "none"; break;
    }
    if (d.ready)
        std::snprintf(buf, len,
            "  %c:  %-8s %-6s %s — %s [%s] %u bytes\r\n",
            d.letter, d.volume, kind, d.driver, d.fieldTag,
            d.enabled ? "ON" : "off", d.totalBytes);
    else if (d.enabled && d.kind != Kind::None)
        std::snprintf(buf, len,
            "  %c:  — not ready (%s)\r\n", d.letter, d.driver);
}

inline void formatTable(char* buf, std::size_t len) noexcept {
    refresh();
    std::size_t off = 0;
    auto append = [&](const char* s) {
        const std::size_t n = std::strlen(s);
        if (off + n + 1 >= len) return;
        std::memcpy(buf + off, s, n);
        off += n;
    };
    char line[128];
    std::snprintf(line, sizeof line,
        "\r\nRTX-AMMOS field drives — %u mounted (current %c:)\r\n",
        readyCount, currentLetter);
    append(line);
    for (const auto& d : slots) {
        if (d.kind == Kind::None && !d.ready) continue;
        formatLine(line, sizeof line, d);
        append(line);
    }
    if (buf[len - 1] != '\0') buf[len - 1] = '\0';
}

inline const char* readyLetters() noexcept {
    refresh();
    static char buf[32];
    std::size_t n = 0;
    for (const auto& d : slots) {
        if (!d.ready) continue;
        if (n > 0 && n + 1 < sizeof buf) buf[n++] = '+';
        if (n + 1 < sizeof buf) buf[n++] = d.letter;
    }
    if (n == 0) {
        std::snprintf(buf, sizeof buf, "none");
    } else {
        buf[n] = '\0';
    }
    return buf;
}

inline bool mountCd() noexcept {
    if (!FieldCdRom::mountAny(FieldDos::assetRoot())) return false;
    FieldRtxMemory::growMscdexExtender();
    FieldMscdex::install();
    invalidate();
    refresh(true);
    return slots[3].ready;
}

inline bool listFloppyRoot(std::vector<FieldDos::FatRootEntry>& ents) noexcept {
    return FieldDos::listFat12Root(ents);
}

inline bool listHostBridge(std::vector<std::string>& names) noexcept {
    names.clear();
    const auto& root = detail::hostRoot();
    if (!std::filesystem::is_directory(root)) return false;
    for (const auto& ent : std::filesystem::directory_iterator(root)) {
        if (ent.is_regular_file())
            names.push_back(ent.path().filename().string());
    }
    return true;
}

inline bool readHostBridgeFile(const char* name, std::vector<std::uint8_t>& out) noexcept {
    out.clear();
    if (!name || !name[0]) return false;
    const std::filesystem::path path = detail::hostRoot() / name;
    if (!std::filesystem::is_regular_file(path)) return false;
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    in.seekg(0, std::ios::end);
    const auto sz = static_cast<std::size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    if (sz == 0 || sz > 1024u * 1024u) return false;
    out.resize(sz);
    in.read(reinterpret_cast<char*>(out.data()), static_cast<std::streamsize>(sz));
    return static_cast<bool>(in);
}

inline bool writeHostBridgeFile(const char* name, const std::uint8_t* data, std::size_t size) noexcept {
    if (!name || !name[0] || !data || size == 0 || size > 1024u * 1024u) return false;
    const std::filesystem::path root = detail::hostRoot();
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    const std::filesystem::path path = root / name;
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    invalidate();
    return static_cast<bool>(out);
}

} // namespace FieldDrives