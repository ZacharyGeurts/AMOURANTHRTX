#pragma once

// Field-parallel frame executor — batch CPU steps, tick devices once per frame.

#include "FieldChip6502.hpp"
#include "FieldChipZ80.hpp"
#include "FieldChipM68000.hpp"

namespace FieldChips {

constexpr int kMax6502StepsPerFrame = 200000;

template<typename Ctx, typename Traits, typename Devices>
inline int runFieldFrame(Ctx& ctx, int targetCycles,
                         void (*onScanline)(Ctx&) = nullptr) noexcept {
    int elapsed = 0;
    int guard = 0;
    while (elapsed < targetCycles && guard++ < kMax6502StepsPerFrame) {
        const int cyc = step6502<Ctx, Traits>(ctx);
        elapsed += (cyc > 0) ? cyc : 2;
        if (onScanline && elapsed > 0 && (elapsed % 114) == 0)
            onScanline(ctx);
    }
    Devices::endCpuFrame(ctx, targetCycles);
    return elapsed;
}

template<typename Ctx, typename Traits>
inline int run6502Cycles(Ctx& ctx, int targetCycles) noexcept {
    int elapsed = 0;
    int guard = 0;
    while (elapsed < targetCycles && guard++ < kMax6502StepsPerFrame) {
        const int cyc = step6502<Ctx, Traits>(ctx);
        elapsed += (cyc > 0) ? cyc : 2;
    }
    return elapsed;
}

template<typename Ctx, typename Traits, typename Devices>
inline int runZ80FieldFrame(Ctx& ctx, int targetCycles) noexcept {
    int elapsed = 0;
    while (elapsed < targetCycles)
        elapsed += stepZ80<Ctx, Traits>(ctx);
    Devices::endCpuFrame(ctx, targetCycles);
    return elapsed;
}

template<typename Ctx, typename Traits, typename Devices>
inline int runM68000FieldFrame(Ctx& ctx, int targetCycles) noexcept {
    int elapsed = 0;
    while (elapsed < targetCycles)
        elapsed += stepM68000<Ctx, Traits>(ctx);
    Devices::endCpuFrame(ctx, targetCycles);
    return elapsed;
}

} // namespace FieldChips