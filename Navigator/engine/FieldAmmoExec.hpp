#pragma once

// AmmoDOS — FieldX86Native host CPU runs DOS/4GW; GPU die composites VGA (CTRL_HOST_CPU skips shader execute_cycle).

#include "FieldDos.hpp"
#include "FieldDosConfig.hpp"
#include "FieldGpuFiles.hpp"
#include "FieldPlatform.hpp"
#include "FieldVga.hpp"
#include "FieldX86Emu.hpp"
#include "FieldX86Runtime.hpp"
#include "OptionsMenu.hpp"
#include "FieldBios.hpp"
#include "FieldGpuLaunch.hpp"
#include "FieldRtxPm.hpp"
#include "FieldKilroyLoader.hpp"
#include "FieldRtxAmmos.hpp"

#include <cstdio>
#include <cstring>
#include <vector>

namespace FieldAmmoExec {

enum class Format : std::uint8_t {
    Unknown = 0,
    DosMz,
    DosCom,
    Pe32,
    Elf,
    MachO,
};

inline bool active = false;
inline void* dieMapped = nullptr;
inline std::size_t ramByteOffset = 0;
inline char launchedPath[128]{};
inline Format launchedFormat = Format::Unknown;
inline int doomAutoKeys = 0;
inline int keenAutoKeys = 0;
inline int doomPumpRounds = 0;

inline bool isActive() noexcept { return active; }
inline const char* runningPath() noexcept { return launchedPath; }
inline Format runningFormat() noexcept { return launchedFormat; }

inline const char* formatName(Format f) noexcept {
    switch (f) {
    case Format::DosMz:  return "DOS-MZ";
    case Format::DosCom: return "DOS-COM";
    case Format::Pe32:   return "PE32";
    case Format::Elf:    return "ELF";
    case Format::MachO:  return "Mach-O";
    default:             return "unknown";
    }
}

inline Format sniff(const std::uint8_t* data, std::size_t size) noexcept {
    if (!data || size < 4u) return Format::Unknown;
    if (data[0] == 'M' && data[1] == 'Z') return Format::DosMz;
    if (size >= 4u && data[0] == 0x7Fu && data[1] == 'E' && data[2] == 'L' && data[3] == 'F')
        return Format::Elf;
    if (size >= 4u) {
        const std::uint32_t m = static_cast<std::uint32_t>(data[0])
            | (static_cast<std::uint32_t>(data[1]) << 8)
            | (static_cast<std::uint32_t>(data[2]) << 16)
            | (static_cast<std::uint32_t>(data[3]) << 24);
        if (m == 0xFEEDFACEu || m == 0xCEFAEDFEu || m == 0xFEEDFACFu || m == 0xCFFAEDFEu)
            return Format::MachO;
    }
    if (size >= 64u) {
        const std::uint32_t peOff = static_cast<std::uint32_t>(data[0x3C])
            | (static_cast<std::uint32_t>(data[0x3D]) << 8)
            | (static_cast<std::uint32_t>(data[0x3E]) << 16)
            | (static_cast<std::uint32_t>(data[0x3F]) << 24);
        if (peOff + 4u <= size && std::memcmp(data + peOff, "PE\0\0", 4) == 0)
            return Format::Pe32;
    }
    if (size <= 65536u) return Format::DosCom;
    return Format::Unknown;
}

inline void storePath(const char* dosPath) noexcept {
    if (!dosPath) {
        launchedPath[0] = '\0';
        return;
    }
    std::snprintf(launchedPath, sizeof launchedPath, "%s", dosPath);
}

inline bool launchDos(void* mapped, std::size_t offset, std::uint8_t* ram,
                      const char* dosPath, const std::vector<std::uint8_t>& image,
                      Format fmt) noexcept {
    dieMapped = mapped;
    ramByteOffset = offset;

    bool ok = false;
    if (fmt == Format::DosMz) {
        const bool isKeen = [&]() {
            if (!dosPath) return false;
            char upper[128]{};
            for (int i = 0; dosPath[i] && i < 127; ++i)
                upper[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(dosPath[i])));
            return std::strstr(upper, "KEEN") != nullptr;
        }();
        ok = isKeen
            ? FieldRtxPm::launchMzPm(mapped, offset, ram, image, dosPath)
            : FieldGpuLaunch::seedMzExec(mapped, offset, ram, image, dosPath);
    }
    else if (fmt == Format::DosCom)
        ok = FieldGpuLaunch::seedCom(mapped, offset, ram, image);
    if (!ok) return false;

    FieldX86Emu::ensure(mapped, offset);
    const bool isDoom = [&]() {
        if (!dosPath) return false;
        char upper[128]{};
        for (int i = 0; dosPath[i] && i < 127; ++i)
            upper[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(dosPath[i])));
        return std::strstr(upper, "DOOM") != nullptr;
    }();
    if (isDoom)
        FieldDosConfig::applyPreset(FieldDosConfig::Preset::Classic);

    FieldBios::pmExecActive = true;
    storePath(dosPath);
    launchedFormat = fmt;
    Options::Canvas::ControlFlags &= ~Options::Canvas::ControlGpuDoom;
    Options::Canvas::ControlFlags |= Options::Canvas::ControlAmmoExec
        | Options::Canvas::ControlHostCpu;
    active = true;
    std::fprintf(stderr, "[AmmoDOS] %s %s\n", formatName(fmt), dosPath);
    if (dosPath) {
        char upper[128]{};
        for (int i = 0; dosPath[i] && i < 127; ++i)
            upper[i] = static_cast<char>(std::toupper(static_cast<unsigned char>(dosPath[i])));
        if (std::strstr(upper, "DOOM"))
            FieldBios::enqueueHostKey(0x1C0Du);
    }
    return true;
}

inline bool launch(void* mapped, std::size_t offset, std::uint8_t* ram,
                   const char* dosPath) noexcept {
    if (!mapped || !ram || !dosPath || !dosPath[0]) return false;

    std::vector<std::uint8_t> image;
    if (!FieldDos::readHostFile(dosPath, image) || image.empty()) {
        std::fprintf(stderr, "[AmmoDOS] cannot read %s (AMMOFAT / C: / E: bridge)\n", dosPath);
        return false;
    }

    const Format fmt = sniff(image.data(), image.size());
    switch (fmt) {
    case Format::DosMz:
    case Format::DosCom:
        return launchDos(mapped, offset, ram, dosPath, image, fmt);
    case Format::Elf: {
        const auto loaded = FieldKilroy::loadFromVector(ram, image);
        if (!loaded.ok) {
            std::fprintf(stderr, "[FieldKilroy] ELF load failed: %s\n", loaded.error);
            return false;
        }
        FieldRtxAmmos::activeEra = FieldRtxAmmos::Era::Linux;
        Options::Canvas::ControlFlags |= Options::Canvas::ControlAmmoExec
            | Options::Canvas::ControlHostCpu;
        storePath(dosPath);
        launchedFormat = Format::Elf;
        std::fprintf(stderr,
            "[FieldKilroy] ELF launched entry=0x%llx — RTX kernel compat active (ABI %s)\n",
            static_cast<unsigned long long>(loaded.entry), FieldKilroy::KERNEL_ABI);
        return true;
    }
    case Format::MachO:
        std::fprintf(stderr,
            "[AmmoDOS] Mach-O detected — AmmoDOS Mac layer not wired yet (use DOS MZ/COM today)\n");
        return false;
    case Format::Pe32:
        std::fprintf(stderr,
            "[AmmoDOS] PE32 detected — run via DOS stub or Win31 ERA paint (native PE loader TBD)\n");
        return false;
    default:
        std::fprintf(stderr, "[AmmoDOS] unknown binary format: %s\n", dosPath);
        return false;
    }
}

inline bool launchFromGuest(std::uint8_t* ram, const char* dosPath) noexcept {
    if (!ram || !dosPath) return false;
    if (!dieMapped)
        dieMapped = FieldX86Emu::dieMapped;
    if (!dieMapped) return false;
    const std::size_t off = ramByteOffset ? ramByteOffset
        : FieldPlatform::DIE_HEADER_UINTS * sizeof(std::uint32_t);
    return launch(dieMapped, off, ram, dosPath);
}

inline void close(std::uint8_t* ram) noexcept {
    if (!active) return;
    active = false;
    doomAutoKeys = 0;
    keenAutoKeys = 0;
    doomPumpRounds = 0;
    launchedFormat = Format::Unknown;
    FieldBios::pmExecActive = false;
    Options::Canvas::ControlFlags &= ~(Options::Canvas::ControlGpuDoom
        | Options::Canvas::ControlHostCpu | Options::Canvas::ControlAmmoExec);
    if (ram) {
        FieldGpuFiles::clear(ram);
        FieldVga::setMode(3u, ram);
    }
    std::fprintf(stderr, "[AmmoDOS] stopped %s\n", launchedPath[0] ? launchedPath : "(program)");
    launchedPath[0] = '\0';
}

inline std::uint32_t gpuCyclesPerFrame() noexcept {
    if (std::strstr(launchedPath, "DOOM") || std::strstr(launchedPath, "doom")
        || std::strstr(launchedPath, "KEEN") || std::strstr(launchedPath, "keen"))
        return FieldX86Runtime::turboCycles(16'000'000u);
    return FieldX86Runtime::turboCycles(4'000'000u);
}

inline void pump(std::uint8_t* ram, void* mapped, std::size_t offset,
                 std::uint32_t hostKey, bool keyDown, std::uint32_t cycles) noexcept {
    if (!active || !mapped) return;
    dieMapped = mapped;
    ramByteOffset = offset;

    if (std::strstr(launchedPath, "DOOM") || std::strstr(launchedPath, "doom")) {
        if (doomAutoKeys < 24) {
            if ((doomAutoKeys & 1) == 0)
                FieldBios::enqueueHostKey(0x1C0Du);
            else if (doomAutoKeys == 3)
                FieldBios::enqueueHostKey(0x3920u);
            ++doomAutoKeys;
        }
    }
    if (std::strstr(launchedPath, "KEEN") || std::strstr(launchedPath, "keen")) {
        if (keenAutoKeys < 32) {
            if (keenAutoKeys < 8)
                FieldBios::enqueueHostKey(0x1C0Du);
            else if (keenAutoKeys == 10)
                FieldBios::enqueueHostKey(0x3920u);
            ++keenAutoKeys;
        }
    }
    if (keyDown && hostKey != 0u) {
        const std::uint16_t key = static_cast<std::uint16_t>(hostKey & 0xFFFFu);
        if (key == 0x011Bu || (key & 0xFFu) == 27u) {
            close(ram);
            return;
        }
    }

    FieldX86Emu::guestAppExecute = true;
    FieldX86Emu::ensure(mapped, offset);
    FieldX86Emu::Ctx ctx{};
    ctx.key = hostKey;
    ctx.keyDown = keyDown;
    const std::uint32_t budget = cycles ? cycles : gpuCyclesPerFrame();
    FieldX86Runtime::pumpHost(mapped, offset, FieldPlatform::FIELD_X86_DIE_CYCLE_OFFSET,
        ctx, budget);
    FieldX86Emu::guestAppExecute = false;
    Options::Canvas::DataBus[30] = FieldX86Emu::hostCyclesLastFrame();

    auto* d = static_cast<FieldX86Emu::DieView*>(mapped);
    d->EFLAGS &= ~FieldGpuLaunch::EFLAGS_HALTED;
    FieldGpuFiles::wr32(ram, FieldGpuFiles::TRAP_OPCODE, 0u);
    FieldGpuLaunch::syncVideoFromGuest(mapped, ram);
}

} // namespace FieldAmmoExec