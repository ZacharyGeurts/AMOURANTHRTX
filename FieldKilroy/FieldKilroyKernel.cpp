#include "FieldKilroyKernel.hpp"

#include <cstdio>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

extern "C" {
#include "../../include/uapi/kilroy/rtx_field.h"
}

namespace FieldKilroy {

namespace {
constexpr const char* kProcThermo = "/proc/kilroy_field/thermo";
constexpr const char* kProcFcc    = "/proc/kilroy_field/fcc";
constexpr const char* kProcStatus = "/proc/kilroy_field/status";
}

bool kernelNativeAvailable() noexcept
{
    return ::access(kProcStatus, R_OK) == 0;
}

bool pushThermoToKernel(const KilroyFieldThermo& thermo) noexcept
{
    const int fd = ::open(kProcThermo, O_WRONLY | O_CLOEXEC);
    if (fd < 0)
        return false;
    char line[192];
    const int n = std::snprintf(line, sizeof(line),
        "%u %u %u %u %u %u %u %u\n",
        thermo.entropy_micro, thermo.boundary_thermo_micro,
        thermo.prev_maint_micro, thermo.free_energy_micro,
        thermo.steps, thermo.host_heat_micro,
        thermo.phi_micro, thermo.entropy_floor_submicro);
    if (n <= 0) {
        ::close(fd);
        return false;
    }
    const ssize_t wrote = ::write(fd, line, static_cast<std::size_t>(n));
    ::close(fd);
    return wrote == n;
}

bool pushFccToKernel(const KilroyFieldFcc& fcc) noexcept
{
    const int fd = ::open(kProcFcc, O_WRONLY | O_CLOEXEC);
    if (fd < 0)
        return false;
    char line[224];
    const int n = std::snprintf(line, sizeof(line),
        "%u %u %u %u %u %u %u %u %u\n",
        fcc.time_scale_micro, fcc.thermo_alpha_micro,
        fcc.wave_speed_micro, fcc.gate_fidelity_micro,
        fcc.entropy_floor_submicro, fcc.inject_strength_micro,
        fcc.propalactic_micro, fcc.field_coupling_micro,
        fcc.tesla_bias_micro);
    if (n <= 0) {
        ::close(fd);
        return false;
    }
    const ssize_t wrote = ::write(fd, line, static_cast<std::size_t>(n));
    ::close(fd);
    return wrote == n;
}

bool pullCacheFromKernel(KilroyFieldCache& out) noexcept
{
    const int fd = ::open(KILROY_DEV_PATH, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return false;
    kilroy_field_cache cache{};
    const int rc = ::ioctl(fd, KILROY_IOC_GET_CACHE, &cache);
    ::close(fd);
    if (rc < 0)
        return false;
    out.shed_count = cache.shed_count;
    out.bypass_ops = cache.bypass_ops;
    out.slab_drops = cache.slab_drops;
    out.lru_drains = cache.lru_drains;
    out.retention_micro = cache.retention_micro;
    out.pressure_micro = cache.pressure_micro;
    return true;
}

bool pullDirectFromKernel(KilroyFieldDirect& out) noexcept
{
    const int fd = ::open(KILROY_DEV_PATH, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return false;
    kilroy_field_direct direct{};
    const int rc = ::ioctl(fd, KILROY_IOC_GET_DIRECT, &direct);
    ::close(fd);
    if (rc < 0)
        return false;
    out.direct_maps = direct.direct_maps;
    out.direct_bytes = direct.direct_bytes;
    out.cached_maps = direct.cached_maps;
    out.coherence_ops = direct.coherence_ops;
    out.direct_ratio_micro = direct.direct_ratio_micro;
    return true;
}

} // namespace FieldKilroy