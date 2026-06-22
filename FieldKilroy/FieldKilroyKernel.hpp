#pragma once
/* KILROY FieldKilroy userspace bridge — canonical (layout v9, micro units) */

#include <cstdint>

namespace FieldKilroy {

constexpr const char* KILROY_FIELD_ABI = "kilroy-field-1.0";
constexpr const char* KILROY_DEV_PATH = "/dev/kilroy_field";
constexpr std::uint32_t KILROY_LAYOUT_VERSION = 9;
constexpr std::uint32_t KILROY_FIELD_MAGIC = 0x4B494C52u;

struct KilroyFieldThermo {
    std::uint32_t entropy_micro;
    std::uint32_t boundary_thermo_micro;
    std::uint32_t prev_maint_micro;
    std::uint32_t free_energy_micro;
    std::uint32_t steps;
    std::uint32_t host_heat_micro;
    std::uint32_t phi_micro;
    std::uint32_t entropy_floor_submicro;
};

struct KilroyFieldFcc {
    std::uint32_t time_scale_micro;
    std::uint32_t thermo_alpha_micro;
    std::uint32_t wave_speed_micro;
    std::uint32_t gate_fidelity_micro;
    std::uint32_t entropy_floor_submicro;
    std::uint32_t inject_strength_micro;
    std::uint32_t propalactic_micro;
    std::uint32_t field_coupling_micro;
    std::uint32_t tesla_bias_micro;
};

struct KilroyFieldCache {
    std::uint32_t shed_count;
    std::uint32_t bypass_ops;
    std::uint32_t slab_drops;
    std::uint32_t lru_drains;
    std::uint32_t retention_micro;
    std::uint32_t pressure_micro;
};

struct KilroyFieldDirect {
    std::uint64_t direct_maps;
    std::uint64_t direct_bytes;
    std::uint64_t cached_maps;
    std::uint64_t coherence_ops;
    std::uint32_t direct_ratio_micro;
    std::uint32_t pad;
};

bool kernelNativeAvailable() noexcept;
bool pushThermoToKernel(const KilroyFieldThermo& thermo) noexcept;
bool pushFccToKernel(const KilroyFieldFcc& fcc) noexcept;
bool pullCacheFromKernel(KilroyFieldCache& out) noexcept;
bool pullDirectFromKernel(KilroyFieldDirect& out) noexcept;

/* Legacy milli aliases — proc writes still accept milli when value < 10000 */
using RtxFieldThermo = KilroyFieldThermo;
using RtxFieldFcc = KilroyFieldFcc;
constexpr std::uint32_t RTX_LAYOUT_VERSION = KILROY_LAYOUT_VERSION;

} // namespace FieldKilroy