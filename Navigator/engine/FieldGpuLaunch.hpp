#pragma once

// Seed the Field Die SSBO for GPU execute_cycle() — one-shot host BIOS, then GPU owns the CPU.

#include "FieldDos.hpp"
#include "FieldGpuFiles.hpp"
#include "FieldPlatform.hpp"
#include "FieldVga.hpp"
#include "FieldX86Emu.hpp"

#include <cstdio>
#include <vector>

#include <x86emu.h>

namespace FieldBios {
bool launchMzExec(x86emu_t* e, const std::vector<std::uint8_t>& img, const char* dosPath,
    std::uint16_t pspSeg);
bool launchComImage(x86emu_t* e, const std::vector<std::uint8_t>& com, std::uint16_t pspSeg);
}

namespace FieldGpuLaunch {

inline constexpr std::uint32_t EFLAGS_HALTED = 0x40000000u;

inline bool seedMzExec(void* mapped, std::size_t offset, std::uint8_t* ram,
                       const std::vector<std::uint8_t>& image, const char* dosPath) noexcept {
    if (!mapped || !ram || image.size() < 32u) return false;

    FieldX86Emu::ensure(mapped, offset);
    if (!FieldBios::launchMzExec(FieldX86Emu::emu, image, dosPath, 0x1000u))
        return false;

    const std::uint32_t staged = FieldGpuFiles::stageForLaunch(ram, dosPath);
    FieldX86Emu::syncToDie(mapped);

    auto* d = static_cast<FieldX86Emu::DieView*>(mapped);
    d->EFLAGS &= ~EFLAGS_HALTED;
    d->EFLAGS |= 0x200u;
    /* LE bootstrap leaves PM32 pending — do not drop CR0.PE before first runMapped. */
    if (!FieldDpmi::leBootPending)
        d->CR0 &= ~1u;

    std::fprintf(stderr, "[GpuLaunch] MZ seeded cs=%04x ip=%04x files=%u\n",
        static_cast<unsigned>(d->CS & 0xFFFFu),
        static_cast<unsigned>(d->EIP & 0xFFFFu),
        staged);
    return true;
}

inline bool seedCom(void* mapped, std::size_t offset, std::uint8_t* ram,
                    const std::vector<std::uint8_t>& image) noexcept {
    if (!mapped || !ram || image.empty()) return false;

    FieldX86Emu::ensure(mapped, offset);
    if (!FieldBios::launchComImage(FieldX86Emu::emu, image, 0x0800u))
        return false;

    FieldX86Emu::syncToDie(mapped);
    auto* d = static_cast<FieldX86Emu::DieView*>(mapped);
    d->EFLAGS &= ~EFLAGS_HALTED;
    d->EFLAGS |= 0x200u;
    return true;
}

inline void syncVideoFromGuest(void* mapped, std::uint8_t* ram) noexcept {
    if (!mapped || !ram) return;
    FieldVga::mode = ram[0x449u];
    FieldX86Emu::syncVideoMode(mapped);
}

} // namespace FieldGpuLaunch