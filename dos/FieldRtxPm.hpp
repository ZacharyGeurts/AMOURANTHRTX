#pragma once

// RTX-PM — first-party protected-mode launcher (skip DOS/4GW extender stub).

#include "FieldDpmi.hpp"
#include "FieldGpuFiles.hpp"
#include "FieldGpuLaunch.hpp"
#include "FieldRtxLe.hpp"
#include "FieldVga.hpp"
#include "FieldX86Emu.hpp"

#include <cstdio>
#include <vector>

namespace FieldRtxPm {

inline std::uint16_t launchSeedCs = 0u;
inline std::uint16_t launchSeedIp = 0u;

inline void recordLaunchIp(x86emu_t* e) noexcept {
    if (!e) return;
    launchSeedCs = static_cast<std::uint16_t>(e->x86.R_CS & 0xFFFFu);
    launchSeedIp = static_cast<std::uint16_t>(e->x86.R_EIP & 0xFFFFu);
}

inline std::uint32_t linearIp(std::uint16_t cs, std::uint16_t ip) noexcept {
    return (static_cast<std::uint32_t>(cs) << 4) + ip;
}

// True when guest PC moved from GpuLaunch seed or across pump rounds.
inline bool ipProgressProbe(x86emu_t* e, std::uint16_t roundIp0, std::uint16_t roundIpLast,
                            std::uint16_t roundCs0, std::uint16_t roundCsLast) noexcept {
    if (!e) return false;
    const std::uint16_t cs = static_cast<std::uint16_t>(e->x86.R_CS & 0xFFFFu);
    const std::uint16_t ip = static_cast<std::uint16_t>(e->x86.R_EIP & 0xFFFFu);
    if (linearIp(cs, ip) != linearIp(launchSeedCs, launchSeedIp)) return true;
    if (roundIp0 != roundIpLast || roundCs0 != roundCsLast) return true;
    return linearIp(roundCsLast, roundIpLast) != linearIp(launchSeedCs, launchSeedIp);
}

// Keen P1 — CPU IP motion OR GpuLaunch title blit / EGA path / painted framebuffer = progress.
inline bool keenLaunchProgress(x86emu_t* e, std::uint16_t roundIp0, std::uint16_t roundIpLast,
                               std::uint16_t roundCs0, std::uint16_t roundCsLast,
                               bool titleBlitActive, int fbNonZero,
                               std::uint8_t vgaMode = 0u,
                               const std::uint8_t* guestRam = nullptr) noexcept {
    if (titleBlitActive || fbNonZero >= 500) return true;
    if (vgaMode == FieldVga::MODE_EGA_0D && fbNonZero > 0) return true;
    if (guestRam && FieldRtxLe::keenTitleBlitProbe(guestRam)) return true;
    return ipProgressProbe(e, roundIp0, roundIpLast, roundCs0, roundCsLast);
}

// True when LE/PM32 is live but VGA never reached mode 13h with a painted framebuffer.
inline bool pm32TitleStalled(x86emu_t* e, std::uint8_t vgaMode, int fbNonZero) noexcept {
    if (!e || fbNonZero >= 8000) return false;
    if (vgaMode == 0x13u) return false;
    return FieldDpmi::isProtected(e) || FieldDpmi::leBootPending;
}

inline bool launchMzPm(void* mapped, std::size_t offset, std::uint8_t* ram,
                       const std::vector<std::uint8_t>& image, const char* dosPath) noexcept {
    if (!mapped || !ram || image.size() < 32u) return false;

    std::vector<std::uint8_t> mz = image;
    if (FieldRtxLe::isLzexe91(image.data(), image.size())) {
        FieldX86Emu::ensure(mapped, offset);
        FieldX86Emu::Ctx ctx{};
        std::fprintf(stderr, "[RTX-PM] expanding LZEXE91 %s\n", dosPath ? dosPath : "");
        if (!FieldRtxLe::expandLzexe91(FieldX86Emu::emu, mapped, offset, image, mz, ctx))
            return false;
    }

    std::size_t leOff = 0;
    if (!FieldRtxLe::findLeOffset(mz.data(), mz.size(), leOff)) {
        std::fprintf(stderr, "[RTX-PM] real-mode MZ after LZEXE — GpuLaunch %s\n",
            dosPath ? dosPath : "(image)");
        if (!FieldGpuLaunch::seedKeenMzExec(mapped, offset, ram, mz, dosPath))
            return false;
        recordLaunchIp(FieldX86Emu::emu);
        return true;
    }

    FieldX86Emu::ensure(mapped, offset);
    if (!FieldBios::launchMzExec(FieldX86Emu::emu, mz, dosPath, 0x1000u))
        return false;

    const std::uint32_t staged = FieldGpuFiles::stageForLaunch(ram, dosPath);
    FieldX86Emu::syncToDie(mapped);

    auto* d = static_cast<FieldX86Emu::DieView*>(mapped);
    d->EFLAGS &= ~FieldGpuLaunch::EFLAGS_HALTED;
    d->EFLAGS |= 0x200u;
    if (!FieldDpmi::leBootPending)
        d->CR0 &= ~1u;

    std::fprintf(stderr, "[RTX-PM] LE bootstrap %s le@%zu files=%u pm=%d\n",
        dosPath ? dosPath : "(program)", leOff, staged, FieldDpmi::leBootPending ? 1 : 0);
    return FieldDpmi::leBootPending || FieldDpmi::inProtected;
}

} // namespace FieldRtxPm