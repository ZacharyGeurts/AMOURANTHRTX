#include "FieldStorage.hpp"

#include "FieldAmmoFat.hpp"
#include "FieldDos.hpp"
#include "FieldRtxVfs.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace FieldStorage {
namespace {

constexpr std::uint32_t kResidentCapMiB = 256u;
constexpr std::uint64_t kBlock = 4096u;

std::filesystem::path storageRoot() {
    return FieldDos::assetRoot() / "cache" / "fieldstorage";
}

void appendMount(FsKind fs, const std::string& path, const std::string& hostPath, bool live) {
    mounts.push_back(MountPoint{fs, path, hostPath, live, 0});
}

} // namespace

bool mountMultiFS(const char* projectRoot) noexcept {
    dismissAll();
    const auto root = projectRoot && projectRoot[0] ? std::filesystem::path(projectRoot)
                                                      : FieldDos::assetRoot();
    std::error_code ec;
    std::filesystem::create_directories(storageRoot(), ec);

    if (!FieldAmmoFat::mounted) FieldAmmoFat::mount();
    appendMount(FsKind::AmmoFat, "C:\\", "assets/dos/ammo", FieldAmmoFat::mounted);

    FieldRtxVfs::vfsReload();
    appendMount(FsKind::Vfs, "VFS:\\", root.string(), FieldRtxVfs::initialized);

    const auto games = root / "assets" / "dos" / "games";
    appendMount(FsKind::Fat16, "C:\\GAMES\\", games.string(),
        std::filesystem::is_directory(games));

    const auto teamImg = storageRoot() / "team_drive.img";
    if (!std::filesystem::exists(teamImg)) {
        std::ofstream mk(teamImg, std::ios::binary);
        if (mk) {
            std::vector<char> zero(kBlock, 0);
            for (int i = 0; i < 256 * 1024; ++i) mk.write(zero.data(), static_cast<std::streamsize>(zero.size()));
        }
    }
    appendMount(FsKind::Team, "T:\\", teamImg.string(), std::filesystem::exists(teamImg));

    bo.phi = 1.0;
    bo.harmonic = 1.6180339887;
    const std::size_t liveCount = std::count_if(mounts.begin(), mounts.end(),
        [](const MountPoint& m) { return m.live; });
    std::fprintf(stderr, "[FieldStorage] mountMultiFS %zu mounts %zu live (resident cap %u MiB)\n",
        mounts.size(), liveCount, kResidentCapMiB);
    return liveCount >= 2u;
}

bool mountTeamDrive(const char* devPath, bool allowInit) noexcept {
    if (!devPath || !devPath[0]) devPath = teamDriveDev.c_str();
    teamDriveDev = devPath;

    if (devPath[0] != '/' || std::strstr(devPath, "nvme0") != nullptr
        || std::strstr(devPath, "sda") != nullptr || std::strstr(devPath, "sdb") != nullptr) {
        std::fprintf(stderr, "[FieldStorage] TEAM drive rejected (protected device %s)\n", devPath);
        return false;
    }

    std::error_code ec;
    if (!std::filesystem::exists(devPath, ec)) {
        std::fprintf(stderr, "[FieldStorage] TEAM device missing %s\n", devPath);
        return false;
    }

    const auto staging = storageRoot() / "team_staging";
    std::filesystem::create_directories(staging, ec);
    appendMount(FsKind::Team, "T:\\", staging.string(), true);
    teamDriveLive = true;
    if (allowInit)
        std::fprintf(stderr, "[FieldStorage] TEAM drive %s staged at %s (no destructive format)\n",
            devPath, staging.string().c_str());
    return teamDriveLive;
}

void dismissAll() noexcept {
    mounts.clear();
    teamDriveLive = false;
    linuxCtx = {};
    windowsCtx = {};
}

bool readPath(const char* path, std::vector<std::uint8_t>& out) noexcept {
    if (!path) return false;
    boLeadIn(static_cast<std::uint32_t>(std::strlen(path) & 0xFFu));
    if (FieldDos::readHostFile(path, out)) {
        bo.prefetchHits++;
        return true;
    }
    const auto p = storageRoot() / std::filesystem::path(path).filename();
    if (!std::filesystem::exists(p)) return false;
    std::ifstream in(p, std::ios::binary);
    if (!in) return false;
    out.assign(std::istreambuf_iterator<char>(in), {});
    bo.prefetchHits++;
    return !out.empty();
}

bool writePath(const char* path, const std::uint8_t* data, std::size_t size) noexcept {
    if (!path || !data || !size) return false;
    const auto p = storageRoot() / std::filesystem::path(path).filename();
    std::ofstream out(p, std::ios::binary);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    boWriteback(static_cast<std::uint32_t>(size / kBlock));
    return static_cast<bool>(out);
}

std::uint64_t benchWrite(const char* path, std::size_t bytes, std::size_t rounds) noexcept {
    if (!path || !bytes || !rounds) return 0;
    std::vector<std::uint8_t> buf(bytes, 0xA5u);
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < rounds; ++r) {
        char name[128];
        std::snprintf(name, sizeof name, "bench_%zu.bin", r);
        if (!writePath(name, buf.data(), buf.size())) return 0;
    }
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    if (us <= 0) return bytes * rounds;
    return (bytes * rounds * 1'000'000u) / static_cast<std::uint64_t>(us);
}

std::uint64_t benchRead(const char* path, std::size_t bytes, std::size_t rounds) noexcept {
    (void)path;
    if (!bytes || !rounds) return 0;
    std::vector<std::uint8_t> tmp;
    const auto t0 = std::chrono::steady_clock::now();
    for (std::size_t r = 0; r < rounds; ++r) {
        char name[128];
        std::snprintf(name, sizeof name, "bench_%zu.bin", r);
        if (!readPath(name, tmp)) return 0;
    }
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - t0).count();
    if (us <= 0) return bytes * rounds;
    return (bytes * rounds * 1'000'000u) / static_cast<std::uint64_t>(us);
}

double boGain() noexcept {
    const double fold = 1.0 + static_cast<double>(bo.entropyFold) * 0.01;
    return bo.phi * bo.harmonic * fold;
}

void boLeadIn(std::uint32_t blockIndex) noexcept {
    bo.entropyFold += (blockIndex ^ 0xA5A5u) & 0xFFu;
    bo.phi = std::min(10.0, bo.phi + 0.01);
}

void boWriteback(std::uint32_t blockIndex) noexcept {
    bo.writebacks++;
    bo.harmonic = std::min(10.0, bo.harmonic + static_cast<double>(blockIndex) * 1e-6);
}

bool dualHostReady() noexcept {
    return linuxCtx.kind == HostKind::Linux && windowsCtx.kind == HostKind::Windows;
}

bool activateLinux(std::uint8_t* guestRam, std::size_t ramBytes) noexcept {
    (void)guestRam;
    linuxCtx = {HostKind::Linux, 0u, 0u, std::min(kResidentCapMiB / 2u, 128u)};
    std::fprintf(stderr, "[FieldStorage] Linux host ctx slot=0 resident=%u MiB ram=%zu\n",
        linuxCtx.residentMiB, ramBytes);
    return true;
}

bool activateWindows(std::uint8_t* guestRam, std::size_t ramBytes) noexcept {
    (void)guestRam;
    windowsCtx = {HostKind::Windows, 1u, 1u, std::min(kResidentCapMiB / 2u, 128u)};
    std::fprintf(stderr, "[FieldStorage] Windows host ctx slot=1 resident=%u MiB ram=%zu\n",
        windowsCtx.residentMiB, ramBytes);
    return true;
}

} // namespace FieldStorage