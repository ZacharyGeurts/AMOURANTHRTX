#pragma once

// FieldLayer hook — pumps KILROY telemetry into data_bus each dispatch.

#include "FieldKilroyCompat.hpp"
#include "FieldKilroyKernel.hpp"

#include <algorithm>

namespace FieldKilroy {

inline void pump(std::uint8_t* /*ram*/, std::uint32_t* dataBus) noexcept {
    if (!dataBus) return;
    packDataBus(dataBus);
}

inline std::uint32_t milli(float v) noexcept {
    return static_cast<std::uint32_t>(std::clamp(v * 1000.0f, 0.0f, 4'000'000.0f));
}

inline void onDispatch(float entropyThisFrame, float boundaryThermo,
                       float prevMaintCost = 0.0f, float freeEnergyIncome = 0.0f,
                       const RtxFieldFcc* fcc = nullptr) noexcept {
    auto& st = global();
    if (!st.active) return;
    st.thermoBudget = boundaryThermo - entropyThisFrame * FieldRtxFieldAbs::TESLA_ENTROPY_K;
    if (st.thermoBudget < FieldRtxFieldAbs::ENTROPY_FLOOR)
        st.thermoBudget = FieldRtxFieldAbs::BOUNDARY_THERMO;

    if (!kernelNativeAvailable()) return;
    KilroyFieldThermo thermo{};
    thermo.entropy_micro = milli(entropyThisFrame) * 1000u;
    thermo.boundary_thermo_micro = milli(boundaryThermo) * 1000u;
    thermo.prev_maint_micro = milli(prevMaintCost) * 1000u;
    thermo.free_energy_micro = milli(freeEnergyIncome) * 1000u;
    thermo.phi_micro = FieldRtxFieldAbs::FIELD_PHI_MILLI * 1000u;
    thermo.entropy_floor_submicro = milli(FieldRtxFieldAbs::ENTROPY_FLOOR) * 1000000u;
    pushThermoToKernel(thermo);
    if (fcc)
        pushFccToKernel(*fcc);
}

} // namespace FieldKilroy