#pragma once

// RTX-AMMOS x86 runtime — 2026 unified dispatch:
//   GPU path     : FieldRtxShell (COMMAND.COM semantics, zero host CPU)
//   Native turbo : FieldX86Native + FieldX86Core (MZ/COM/DOS4GW — real DOS games & tools)

#include "FieldDosConfig.hpp"
#include "FieldX86Emu.hpp"

#include <algorithm>
#include <cstdint>



namespace FieldX86Runtime {

enum class Path : std::uint8_t { GpuShell = 0, NativeTurbo = 1, HostTurbo = NativeTurbo };

inline Path activePath() noexcept {
    if (FieldX86Emu::guestAppExecute || FieldBios::pmExecActive)
        return Path::HostTurbo;
    if (FieldBios::rtxShellActive)
        return Path::GpuShell;
    return Path::GpuShell;
}

inline std::uint32_t turboCycles(std::uint32_t base) noexcept {
    const std::uint32_t floor = std::max(2'000'000u, FieldDosConfig::cfg.cyclesRun * 16u);
    const std::uint32_t scaled = std::max(base, floor);
    return std::min(scaled, std::min(FieldDosConfig::cfg.cyclesMax, 16'777'216u));
}

inline std::uint32_t pumpHost(void* mapped, std::size_t ramOff, std::size_t cycleOff,
                              FieldX86Emu::Ctx& ctx, std::uint32_t budget) noexcept {
    if (!mapped) return 0;
    return FieldX86Emu::runMapped(mapped, ramOff, cycleOff, ctx, turboCycles(budget));
}

} // namespace FieldX86Runtime