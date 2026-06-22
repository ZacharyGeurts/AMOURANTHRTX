#pragma once

// Field RTX CPU — DOSBox-class host engine on the Field Die.
// Hybrid: GPU canvas + analog fields (thermo/Tesla) + host x86 for DOS/4GW games.
// Backend: FieldX86Core today (full RM/PM32 + FPU); dosbox-staging core is the upgrade path.

#include "FieldDpmi.hpp"
#include "FieldPlatform.hpp"
#include "FieldX86Native.hpp"
#include "OptionsMenu.hpp"

#include <x86emu.h>

#include <algorithm>
#include <cstdint>

namespace FieldRtxCpu {

enum class Backend : std::uint8_t {
    X86Core = 0,   // FieldX86Core (in-tree libx86emu lineage)
    DosboxClass,   // reserved — dosbox-staging normal/dynrec swap-in
};

inline Backend activeBackend = Backend::X86Core;
inline std::uint32_t cyclesLastFrame = 0u;
inline std::uint32_t totalCycles = 0u;

inline const char* backendName(Backend b = activeBackend) noexcept {
    switch (b) {
    case Backend::X86Core:    return "FieldX86Core (DOSBox-class PM32)";
    case Backend::DosboxClass: return "DOSBox-class (pending)";
    default:                  return "unknown";
    }
}

struct ThermoField {
    float hostHeat = 0.f;
    float teslaEntropy = 0.f;
    float boundaryThermo = 0.f;
};

inline ThermoField thermoFromCycles(std::uint32_t cycles) noexcept {
    ThermoField t{};
    t.hostHeat = std::min(static_cast<float>(cycles) * 2.8e-7f, 0.42f);
    t.teslaEntropy = t.hostHeat * 0.15f * Options::AnalogFields::TeslaBiasStrength;
    t.boundaryThermo = std::clamp(
        Options::AnalogFields::EntropyFloor * 2.2f + t.hostHeat * 0.35f, 0.05f, 0.96f);
    return t;
}

inline void publishThermoToBus(std::uint32_t cycles) noexcept {
    cyclesLastFrame = cycles;
    totalCycles += cycles;
    // Host thermo floats are published on FieldSocket by Pipeline::dispatch_canvas
    // (data_bus[33]/[34]). Do not stomp AOS clock [29] or display scale [31].
    (void)thermoFromCycles(cycles);
}

inline std::uint64_t runPm(x86emu_t* e, std::uint64_t budget,
                           x86emu_code_handler_t preStep = nullptr) noexcept {
    if (!e || budget == 0u) return 0u;
    FieldX86Native::lastGoodEip = FieldDpmi::pmLastGoodEip;
    const std::uint64_t ran = FieldX86Native::runPmBudget(e, budget, preStep);
    FieldDpmi::pmLastGoodEip = FieldX86Native::lastGoodEip;
    publishThermoToBus(static_cast<std::uint32_t>(ran));
    return ran;
}

inline std::uint64_t runRm(x86emu_t* e, std::uint64_t budget) noexcept {
    if (!e || budget == 0u) return 0u;
    const std::uint64_t ran = FieldX86Native::runRmBudget(e, budget);
    publishThermoToBus(static_cast<std::uint32_t>(ran));
    return ran;
}

inline x86emu_code_handler_t pmGuardForDos4gw() noexcept {
    return FieldDpmi::pmExecCodeGuard;
}

} // namespace FieldRtxCpu