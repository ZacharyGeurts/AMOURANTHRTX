#pragma once

// RTX-PM — first-party protected-mode launcher (skip DOS/4GW extender stub).

#include "FieldBios.hpp"
#include "FieldDpmi.hpp"
#include "FieldGpuFiles.hpp"
#include "FieldGpuLaunch.hpp"
#include "FieldRtxLe.hpp"
#include "FieldX86Emu.hpp"

#include <cstdio>
#include <vector>

namespace FieldRtxPm {

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
        std::fprintf(stderr, "[RTX-PM] no Watcom LE in %s\n", dosPath ? dosPath : "(image)");
        return false;
    }

    FieldX86Emu::ensure(mapped, offset);
    if (!FieldBios::launchMzExec(FieldX86Emu::emu, mz, dosPath))
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