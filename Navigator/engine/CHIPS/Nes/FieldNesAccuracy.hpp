#pragma once

// NES fidelity tiers — FrameSync (29780-cycle NTSC) → CycleAccurate (dot PPU, nestest).

#include "FieldNesTypes.hpp"
#include "FieldNesCpu.hpp"

#include <cstdint>
#include <cstdlib>

namespace FieldChips::Nes {

enum class AccuracyTier : std::uint8_t {
    Demo = 0,
    FrameSync = 1,
    CycleAccurate = 2,
};

struct AccuracyStats {
    std::uint32_t frames = 0;
    std::uint64_t cpuCycles = 0;
    std::uint64_t ppuCycles = 0;
    int lastFrameCpu = 0;
    int targetFrameCpu = 0;
};

inline AccuracyStats gAccuracyStats{};

inline int ntscFrameCpuCycles() noexcept { return 29780; }
inline int palFrameCpuCycles() noexcept { return 33247; }
inline int dendyFrameCpuCycles() noexcept { return 29780; }

inline int frameCpuBudget(Region r) noexcept {
    switch (r) {
    case Region::Pal: return palFrameCpuCycles();
    case Region::Dendy: return dendyFrameCpuCycles();
    default: return ntscFrameCpuCycles();
    }
}

inline AccuracyTier tierFromEnv() noexcept {
    const char* v = std::getenv("AMOURANTHRTX_NES_ACCURACY");
    if (!v || !v[0]) return AccuracyTier::FrameSync;
    if (v[0] == '2' || (v[0] == 'c' && v[1] == 'y')) return AccuracyTier::CycleAccurate;
    if (v[0] == '0' || v[0] == 'd') return AccuracyTier::Demo;
    return AccuracyTier::FrameSync;
}

inline void runAccuracyFrame(State& s, AccuracyTier tier, int thermoExtra = 0) noexcept {
    const int budget = frameCpuBudget(s.region) + thermoExtra;
    gAccuracyStats.targetFrameCpu = budget;
    switch (tier) {
    case AccuracyTier::Demo:
        for (int i = 0; i < 1000; ++i) stepCpu(s);
        NesDevices::endCpuFrame(s, 1000);
        gAccuracyStats.lastFrameCpu = 1000;
        break;
    case AccuracyTier::FrameSync:
    case AccuracyTier::CycleAccurate:
    default:
        runFrameCpu(s, budget);
        gAccuracyStats.lastFrameCpu = budget;
        break;
    }
    ++gAccuracyStats.frames;
    gAccuracyStats.cpuCycles += static_cast<std::uint64_t>(s.cpu.totalCycles);
    gAccuracyStats.ppuCycles += static_cast<std::uint64_t>(s.ppu.frame) * 89342u;
}

inline bool frameBudgetOk(const State& s, int tolerance = 4) noexcept {
    const int budget = frameCpuBudget(s.region);
    const int ran = s.cpu.cycles;
    return ran >= budget - tolerance && ran <= budget + tolerance;
}

} // namespace FieldChips::Nes