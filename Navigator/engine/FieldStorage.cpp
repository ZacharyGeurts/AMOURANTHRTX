#include "FieldStorage.hpp"
#include "FieldFabric.hpp"
#include "FieldStorageHyper.hpp"

#include "FieldAmmoFat.hpp"
#include "FieldAmmoVfs.hpp"
#include "FieldDos.hpp"
#include "FieldRtxVfs.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>

namespace FieldStorage {

HyperState hyper{};

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

void enableAllBreakthroughs(bool on) noexcept {
    hyper.enabled = on;
    if (!on) return;
    FieldFabric::updatePeaks(0.5f, 1.2f, 0.77f, 0.22f, 1.f / 60.f);
    const FieldFabric::FieldWave w = FieldFabric::waveFromPeaks(FieldFabric::gPeaks);
    FieldFabric::dispatchExtended(w);
    hyper.leadInPeak = FieldFabric::gPeaks.leadInPeak.w;
    hyper.leadOutPeak = FieldFabric::gPeaks.leadOutPeak.w;
    hyper.entropyFold = FieldFabric::gEntropyFold;
    hyper.phaseVelocityIn = 1.15 + hyper.leadInPeak * 0.08;
    hyper.phaseVelocityOut = 0.87 + hyper.leadOutPeak * 0.06;
    hyper.resonanceCoupling = 0.22 + hyper.entropyFold * 0.05;
    hyper.phiSuperposition = static_cast<double>(FieldFabric::gDispatch.phi)
        + static_cast<double>(FieldFabric::gDispatch.thermo) * 0.618
        + static_cast<double>(FieldFabric::gDispatch.flow) * 0.382;
    hyper.fabricScale = 1.0 + hyper.leadInPeak * hyper.phaseVelocityIn * 0.31
        + hyper.leadOutPeak * hyper.phaseVelocityOut * 0.18
        + hyper.entropyFold * 0.12 + hyper.resonanceCoupling * 0.08
        + hyper.phiSuperposition * 0.15;
    bo.phi = std::min(10.0, bo.phi + hyper.leadInPeak * 0.5 + hyper.phiSuperposition * 0.1);
    bo.harmonic = std::min(10.0, bo.harmonic + hyper.leadOutPeak * 0.3);
    bo.entropyFold += static_cast<std::uint64_t>(hyper.entropyFold * 512.0);
    sdf.amplitude = std::min(4.0, sdf.amplitude + hyper.leadOutPeak * 0.25);
    sdf.logicalBytes = sdfLogicalCapacity();
    std::fprintf(stderr,
        "[FieldStorage] ALL_BREAKTHROUGHS leadIn=%.3f leadOut=%.3f entropy=%.3f scale=%.3f\n",
        hyper.leadInPeak, hyper.leadOutPeak, hyper.entropyFold, hyper.fabricScale);
}

double chipsFabricScale() noexcept {
    return hyper.enabled ? hyper.fabricScale : 1.0;
}

bool hyperEnabled() noexcept { return hyper.enabled; }
double hyperLeadInPeak() noexcept { return hyper.leadInPeak; }
double hyperLeadOutPeak() noexcept { return hyper.leadOutPeak; }
double hyperEntropyFold() noexcept { return hyper.entropyFold; }
double hyperPhiSuperposition() noexcept { return hyper.phiSuperposition; }

void hyperTick(std::uint32_t blockIndex) noexcept {
    if (!hyper.enabled) return;
    const float t = static_cast<float>(blockIndex) * 0.001f;
    FieldFabric::updatePeaks(t, 1.2f, 0.77f, 0.22f, 1.f / 60.f);
    const FieldFabric::FieldWave w = FieldFabric::waveFromPeaks(FieldFabric::gPeaks);
    FieldFabric::dispatchExtended(w);
    hyper.leadInPeak = FieldFabric::gPeaks.leadInPeak.w;
    hyper.leadOutPeak = FieldFabric::gPeaks.leadOutPeak.w;
    hyper.entropyFold = FieldFabric::gEntropyFold;
    hyper.phaseVelocityIn = 1.15 + hyper.leadInPeak * 0.08;
    hyper.phaseVelocityOut = 0.87 + hyper.leadOutPeak * 0.06;
    hyper.resonanceCoupling = 0.22 + hyper.entropyFold * 0.05;
    hyper.phiSuperposition = static_cast<double>(FieldFabric::gDispatch.phi)
        + static_cast<double>(FieldFabric::gDispatch.thermo) * 0.618
        + static_cast<double>(FieldFabric::gDispatch.flow) * 0.382;
    hyper.fabricScale = 1.0 + hyper.leadInPeak * hyper.phaseVelocityIn * 0.31
        + hyper.leadOutPeak * hyper.phaseVelocityOut * 0.18
        + hyper.entropyFold * 0.12 + hyper.resonanceCoupling * 0.08
        + hyper.phiSuperposition * 0.15;
    bo.entropyFold += static_cast<std::uint64_t>(FieldFabric::gEntropyFold * 4.0);
}

void enableInfiniteMode(bool on) noexcept {
    infiniteMode = on;
    if (on) {
        sdf.logicalBase = 8ull * 1024u * 1024u * 1024u;
        sdf.amplitude = 2.5;
        sdf.logicalBytes = sdfLogicalCapacity();
        std::fprintf(stderr, "[FieldStorage] infinite SDF wave ON logical=%.2f GiB\n",
            static_cast<double>(sdf.logicalBytes) / (1024.0 * 1024.0 * 1024.0));
    }
}

void enableEndGameMode(bool on) noexcept {
    endGameActive = on;
    if (!on) return;
    enableInfiniteMode(true);
    enableAllBreakthroughs(true);
    sdf.logicalBase = 16ull * 1024u * 1024u * 1024u;
    sdf.amplitude = 4.0;
    bo.phi = std::min(10.0, bo.phi + 1.618);
    bo.harmonic = std::min(10.0, bo.harmonic * 1.618);
    sdf.logicalBytes = sdfLogicalCapacity();
    std::fprintf(stderr,
        "[FieldStorage] END GAME MODE — infinite SDF + hyper physics logical=%.2f GiB scale=%.3f\n",
        static_cast<double>(sdf.logicalBytes) / (1024.0 * 1024.0 * 1024.0), hyper.fabricScale);
}

bool endGameMode() noexcept { return endGameActive; }

bool mountMultiFS(const char* projectRoot) noexcept {
    dismissAll();
    if (std::getenv("AMOURANTHRTX_END_GAME"))
        enableEndGameMode(true);
    else {
        if (std::getenv("AMOURANTHRTX_INFINITE"))
            enableInfiniteMode(true);
        if (std::getenv("AMOURANTHRTX_ALL_BREAKTHROUGHS")
                || std::getenv("AMOURANTHRTX_EXTENDED_FIELD"))
            enableAllBreakthroughs(true);
    }
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

    const auto chips = root / "assets" / "chips";
    appendMount(FsKind::Vfs, "P:\\", chips.string(), std::filesystem::is_directory(chips));

    bo.phi = 1.0;
    bo.harmonic = 1.6180339887;
    sdf.phase = 0.0;
    sdf.amplitude = 1.0;
    sdf.logicalBytes = sdfLogicalCapacity();
    const std::size_t liveCount = std::count_if(mounts.begin(), mounts.end(),
        [](const MountPoint& m) { return m.live; });
    if (std::getenv("AMOURANTHRTX_FIELD_PERSIST")
            || std::getenv("AMOURANTHRTX_EVERYTHING_EVERYWHERE")
            || std::getenv("AMOURANTHRTX_END_GAME"))
        restoreFieldState();

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

namespace {

constexpr std::uint32_t kPersistMagic = 0x504C4446u; // 'FLDP'
constexpr std::uint32_t kPersistVersion = 2u;

struct PersistBlob {
    std::uint32_t magic = kPersistMagic;
    std::uint32_t version = kPersistVersion;
    double sdfPhase = 0.0;
    double sdfAmplitude = 1.0;
    std::uint64_t sdfLogicalBase = 0u;
    std::uint64_t foldedBlocks = 0u;
    double boPhi = 1.0;
    double boHarmonic = 1.0;
    std::uint64_t boEntropyFold = 0u;
    double hyperLeadIn = 0.0;
    double hyperLeadOut = 0.0;
    double hyperEntropy = 0.0;
    double hyperScale = 1.0;
    double resonanceCoupling = 0.22;
    std::uint32_t everythingTicks = 0u;
    std::uint8_t everythingActive = 0u;
    std::uint32_t mountLive = 0u;
    std::uint64_t fabricChecksum = 0u;
};

std::filesystem::path persistPath() { return storageRoot() / "field_wave.persist"; }

} // namespace

bool persistFieldState() noexcept {
    std::error_code ec;
    std::filesystem::create_directories(storageRoot(), ec);
    PersistBlob b{};
    b.sdfPhase = sdf.phase;
    b.sdfAmplitude = sdf.amplitude;
    b.sdfLogicalBase = sdf.logicalBase;
    b.foldedBlocks = sdf.foldedBlocks;
    b.boPhi = bo.phi;
    b.boHarmonic = bo.harmonic;
    b.boEntropyFold = bo.entropyFold;
    b.hyperLeadIn = hyper.leadInPeak;
    b.hyperLeadOut = hyper.leadOutPeak;
    b.hyperEntropy = hyper.entropyFold;
    b.hyperScale = hyper.fabricScale;
    b.resonanceCoupling = hyper.resonanceCoupling;
    b.everythingTicks = fabricPersist.everythingTicks;
    b.everythingActive = fabricPersist.everythingActive ? 1u : 0u;
    b.mountLive = static_cast<std::uint32_t>(std::count_if(mounts.begin(), mounts.end(),
        [](const MountPoint& m) { return m.live; }));
    b.fabricChecksum = b.foldedBlocks ^ static_cast<std::uint64_t>(b.everythingTicks)
        ^ (static_cast<std::uint64_t>(b.mountLive) << 32);
    const auto tmp = persistPath().string() + ".tmp";
    std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out.write(reinterpret_cast<const char*>(&b), sizeof b);
    out.close();
    std::filesystem::rename(tmp, persistPath(), ec);
    std::fprintf(stderr, "[FieldStorage] persist resonance hold phase=%.4f folded=%llu fabric=%u\n",
        b.sdfPhase, static_cast<unsigned long long>(b.foldedBlocks), b.everythingTicks);
    return !ec;
}

bool restoreFieldState() noexcept {
    std::ifstream in(persistPath(), std::ios::binary);
    if (!in) return false;
    PersistBlob b{};
    in.read(reinterpret_cast<char*>(&b), sizeof b);
    if (!in || b.magic != kPersistMagic
            || (b.version != kPersistVersion && b.version != 1u)) return false;
    sdf.phase = b.sdfPhase;
    sdf.amplitude = b.sdfAmplitude;
    sdf.logicalBase = b.sdfLogicalBase ? b.sdfLogicalBase : sdf.logicalBase;
    sdf.foldedBlocks = b.foldedBlocks;
    bo.phi = b.boPhi;
    bo.harmonic = b.boHarmonic;
    bo.entropyFold = b.boEntropyFold;
    hyper.leadInPeak = b.hyperLeadIn;
    hyper.leadOutPeak = b.hyperLeadOut;
    hyper.entropyFold = b.hyperEntropy;
    hyper.fabricScale = b.hyperScale;
    hyper.resonanceCoupling = b.resonanceCoupling;
    hyper.enabled = true;
    if (b.version >= 2u) {
        fabricPersist.everythingTicks = b.everythingTicks;
        fabricPersist.everythingActive = b.everythingActive != 0u;
    }
    sdf.logicalBytes = sdfLogicalCapacity();
    persistLoaded = true;
    std::fprintf(stderr, "[FieldStorage] restore resonance hold phase=%.4f logical=%.2f GiB\n",
        sdf.phase, static_cast<double>(sdf.logicalBytes) / (1024.0 * 1024.0 * 1024.0));
    return true;
}

bool fieldStatePersisted() noexcept { return persistLoaded; }

void dismissAll() noexcept {
    if (hyper.enabled || infiniteMode || endGameActive
            || std::getenv("AMOURANTHRTX_FIELD_PERSIST")
            || std::getenv("AMOURANTHRTX_EVERYTHING_EVERYWHERE"))
        persistFieldState();
    mounts.clear();
    teamDriveLive = false;
    linuxCtx = {};
    windowsCtx = {};
}

bool readPath(const char* path, std::vector<std::uint8_t>& out) noexcept {
    if (!path) return false;
    const auto block = static_cast<std::uint32_t>(std::strlen(path) & 0xFFu);
    boLeadIn(block);
    sdfFoldBlock(block);
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
    sdfFoldBlock(static_cast<std::uint32_t>(size / kBlock));
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

std::uint64_t sdfLogicalCapacity() noexcept {
    const double resonance = 1.0 + std::sin(sdf.phase) * sdf.amplitude;
    const double fold = boGain() * resonance;
    const long double logical = static_cast<long double>(sdf.logicalBase) * fold;
    if (logical > static_cast<long double>(std::numeric_limits<std::uint64_t>::max()))
        return std::numeric_limits<std::uint64_t>::max();
    return static_cast<std::uint64_t>(logical);
}

double sdfWavePhase(std::uint32_t blockIndex) noexcept {
    return sdf.phase + static_cast<double>(blockIndex) * 0.0001 * bo.harmonic;
}

void sdfFoldBlock(std::uint32_t blockIndex) noexcept {
    hyperTick(blockIndex);
    sdf.phase = sdfWavePhase(blockIndex);
    sdf.foldedBlocks++;
    sdf.logicalBytes = sdfLogicalCapacity();
    boLeadIn(blockIndex);
    if (hyper.enabled) {
        boWriteback(blockIndex);
        sdf.amplitude = std::min(4.0, sdf.amplitude + hyper.leadOutPeak * 0.01);
        sdf.logicalBytes = sdfLogicalCapacity();
    }
}

bool ammoVfsBridgeRead(const char* dosPath, std::vector<std::uint8_t>& out) noexcept {
    if (!dosPath || !dosPath[0]) return false;
    if (!FieldAmmoFat::mounted && !FieldAmmoFat::mount()) return false;
    sdfFoldBlock(static_cast<std::uint32_t>(std::strlen(dosPath) & 0xFFu));
    if (FieldAmmoVfs::readPath(dosPath, out) && !out.empty()) {
        bo.prefetchHits++;
        return true;
    }
    return false;
}

bool ammoVfsBridgeWrite(const char* dosPath, const std::uint8_t* data, std::size_t size) noexcept {
    if (!dosPath || !data || !size) return false;
    if (!FieldAmmoFat::mounted && !FieldAmmoFat::mount()) return false;
    sdfFoldBlock(static_cast<std::uint32_t>(size / kBlock));
    return FieldAmmoVfs::writePath(dosPath, data, size);
}

bool vfsBridgeRead(const char* vfsPath, std::vector<std::uint8_t>& out) noexcept {
    if (!vfsPath || !vfsPath[0]) return false;
    if ((vfsPath[0] == 'C' || vfsPath[0] == 'c') && vfsPath[1] == ':') {
        if (ammoVfsBridgeRead(vfsPath, out)) return true;
    }
    if (!FieldRtxVfs::initialized) FieldRtxVfs::vfsReload();
    const std::string key = FieldRtxVfs::upperKey(vfsPath);
    const auto it = FieldRtxVfs::metadata.find(key);
    if (it != FieldRtxVfs::metadata.end()) {
        out.assign(it->second.longDesc.begin(), it->second.longDesc.end());
        bo.prefetchHits++;
        return !out.empty();
    }
    return readPath(vfsPath, out);
}

bool vfsBridgeWrite(const char* vfsPath, const std::uint8_t* data, std::size_t size) noexcept {
    if (!vfsPath || !data || !size) return false;
    if ((vfsPath[0] == 'C' || vfsPath[0] == 'c') && vfsPath[1] == ':') {
        if (ammoVfsBridgeWrite(vfsPath, data, size)) return true;
    }
    if (!FieldRtxVfs::initialized) FieldRtxVfs::vfsReload();
    FieldRtxVfs::metadata[FieldRtxVfs::upperKey(vfsPath)] = FieldRtxVfs::FileMeta{
        std::string(reinterpret_cast<const char*>(data), size), ""};
    return writePath(vfsPath, data, size);
}

} // namespace FieldStorage