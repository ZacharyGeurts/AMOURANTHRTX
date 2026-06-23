#pragma once

// FieldStorage v2 — infinite SDF wave storage, dual-host shim, FieldAmmoVfs bridge, Bo physics.

#include "FieldPlatform.hpp"

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace FieldStorage {

enum class FsKind : std::uint8_t {
    Fat12, Fat16, Fat32, Ntfs, Ext4, Zip, AmmoFat, Vfs, Team
};

enum class HostKind : std::uint8_t {
    None, Linux, Windows, Dos
};

struct HostCtx {
    HostKind kind = HostKind::None;
    std::uint32_t dieSlot = 0u;
    std::uint32_t pm32 = 0u;
    std::uint32_t residentMiB = 0u;
};

struct MountPoint {
    FsKind fs = FsKind::Vfs;
    std::string path;
    std::string hostPath;
    bool live = false;
    std::uint64_t bytesUsed = 0u;
};

struct BoState {
    double phi = 1.0;
    double harmonic = 1.0;
    std::uint64_t entropyFold = 0u;
    std::uint64_t prefetchHits = 0u;
    std::uint64_t writebacks = 0u;
};

// SDF wave fold — 2 GiB physical anchor → practical infinite logical via phase resonance.
struct SdfWave {
    double phase = 0.0;
    double amplitude = 1.0;
    std::uint64_t logicalBase = 2ull * 1024u * 1024u * 1024u;
    std::uint64_t logicalBytes = 0u;
    std::uint64_t foldedBlocks = 0u;
};

inline HostCtx linuxCtx{};
inline HostCtx windowsCtx{};
inline std::vector<MountPoint> mounts;
inline BoState bo{};
inline SdfWave sdf{};
inline bool teamDriveLive = false;
inline bool infiniteMode = false;
inline bool endGameActive = false;
inline bool persistLoaded = false;
inline std::string teamDriveDev = "/dev/nvme2n1";

struct FabricPersist {
    std::uint32_t everythingTicks = 0u;
    bool everythingActive = false;
};
inline FabricPersist fabricPersist{};

void enableInfiniteMode(bool on = true) noexcept;
void enableAllBreakthroughs(bool on = true) noexcept;
void enableEndGameMode(bool on = true) noexcept;
bool endGameMode() noexcept;
double chipsFabricScale() noexcept;
bool hyperEnabled() noexcept;
double hyperLeadInPeak() noexcept;
double hyperLeadOutPeak() noexcept;
double hyperEntropyFold() noexcept;
double hyperPhiSuperposition() noexcept;
bool mountMultiFS(const char* projectRoot = ".") noexcept;
bool mountTeamDrive(const char* devPath, bool allowInit = false) noexcept;
void dismissAll() noexcept;

bool readPath(const char* path, std::vector<std::uint8_t>& out) noexcept;
bool writePath(const char* path, const std::uint8_t* data, std::size_t size) noexcept;

std::uint64_t benchWrite(const char* path, std::size_t bytes, std::size_t rounds) noexcept;
std::uint64_t benchRead(const char* path, std::size_t bytes, std::size_t rounds) noexcept;

double boGain() noexcept;
void boLeadIn(std::uint32_t blockIndex) noexcept;
void boWriteback(std::uint32_t blockIndex) noexcept;

bool dualHostReady() noexcept;
bool activateLinux(std::uint8_t* guestRam, std::size_t ramBytes) noexcept;
bool activateWindows(std::uint8_t* guestRam, std::size_t ramBytes) noexcept;

std::uint64_t sdfLogicalCapacity() noexcept;
double sdfWavePhase(std::uint32_t blockIndex) noexcept;
void sdfFoldBlock(std::uint32_t blockIndex) noexcept;
bool ammoVfsBridgeRead(const char* dosPath, std::vector<std::uint8_t>& out) noexcept;
bool ammoVfsBridgeWrite(const char* dosPath, const std::uint8_t* data, std::size_t size) noexcept;
bool vfsBridgeRead(const char* vfsPath, std::vector<std::uint8_t>& out) noexcept;
bool vfsBridgeWrite(const char* vfsPath, const std::uint8_t* data, std::size_t size) noexcept;

bool persistFieldState() noexcept;
bool restoreFieldState() noexcept;
bool fieldStatePersisted() noexcept;

} // namespace FieldStorage