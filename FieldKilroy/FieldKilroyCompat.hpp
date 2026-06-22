#pragma once

// FieldKilroy — KILROY 7.1.1 ABI compatibility kernel routed through Field Die + RTX fabric.
// Replaces monolithic kernel services with Phi/Thermo/Flow scheduling while preserving
// full userspace binary compatibility via host POSIX passthrough.

#include "FieldKilroySyscalls.hpp"
#include "FieldKilroyKernel.hpp"

#include "FieldRtxFieldAbs.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>

namespace FieldKilroy {

constexpr std::uint32_t BUS_KILROY_BASE   = 29u;  // data_bus[29..31] KILROY layer telemetry
constexpr std::uint32_t BUS_KILROY_ACTIVE = 29u;
constexpr std::uint32_t BUS_KILROY_PID    = 30u;
constexpr std::uint32_t BUS_KILROY_SYSCALLS = 31u;

enum class Route : std::uint8_t {
    HostPassthrough,  // real POSIX — full hardware compat
    FieldDie,         // guest RAM / FieldLayer services
    Fabric,           // Phi/Thermo/Flow scheduler (ThermoAccountant)
    Deny,
};

struct SyscallArgs {
    std::int64_t nr;
    std::int64_t a0, a1, a2, a3, a4, a5;
};

struct SyscallResult {
    std::int64_t ret;
    std::int32_t errnoVal;
    Route route;
};

struct State {
    bool active = false;
    std::uint32_t pid = 0;
    std::uint64_t syscallCount = 0;
    std::uint64_t fabricScheduled = 0;
    std::uint64_t hostPassthrough = 0;
    float thermoBudget = 1.0f;
};

inline State& global() noexcept {
    static State s;
    return s;
}

inline Route classify(std::int64_t nr) noexcept {
    const auto n = static_cast<Nr>(nr);
    switch (n) {
    case Nr::read:
    case Nr::write:
    case Nr::openat:
    case Nr::close:
    case Nr::statx:
    case Nr::fstat:
    case Nr::newfstatat:
    case Nr::lseek:
    case Nr::mmap:
    case Nr::munmap:
    case Nr::mprotect:
    case Nr::brk:
    case Nr::rt_sigaction:
    case Nr::rt_sigprocmask:
    case Nr::ioctl:
    case Nr::fcntl:
    case Nr::getdents64:
    case Nr::getcwd:
    case Nr::chdir:
    case Nr::exit:
    case Nr::exit_group:
    case Nr::clone:
    case Nr::execve:
    case Nr::wait4:
    case Nr::getpid:
    case Nr::getppid:
    case Nr::getuid:
    case Nr::getgid:
    case Nr::geteuid:
    case Nr::getegid:
    case Nr::set_tid_address:
    case Nr::set_robust_list:
    case Nr::futex:
    case Nr::nanosleep:
    case Nr::clock_gettime:
    case Nr::clock_nanosleep:
    case Nr::gettimeofday:
    case Nr::pipe2:
    case Nr::dup:
    case Nr::dup3:
    case Nr::ppoll:
    case Nr::pselect6:
    case Nr::epoll_create1:
    case Nr::epoll_ctl:
    case Nr::epoll_pwait:
    case Nr::socket:
    case Nr::connect:
    case Nr::accept:
    case Nr::accept4:
    case Nr::bind:
    case Nr::listen:
    case Nr::sendto:
    case Nr::recvfrom:
    case Nr::sendmsg:
    case Nr::recvmsg:
    case Nr::shutdown:
    case Nr::setsockopt:
    case Nr::getsockopt:
    case Nr::getsockname:
    case Nr::getpeername:
        return Route::HostPassthrough;
    case Nr::sched_yield:
    case Nr::sched_getaffinity:
    case Nr::sched_setaffinity:
    case Nr::getrusage:
    case Nr::prlimit64:
    case Nr::getrandom:
        return Route::Fabric;
    case Nr::arch_prctl:
        return Route::FieldDie;
    default:
        return Route::HostPassthrough;
    }
}

SyscallResult dispatchHost(const SyscallArgs& args) noexcept;

inline SyscallResult dispatch(const SyscallArgs& args) noexcept {
    auto& st = global();
    ++st.syscallCount;
    const Route route = classify(args.nr);
    SyscallResult out{ -1, EINVAL, route };

    switch (route) {
    case Route::Fabric: {
        st.fabricScheduled++;
        const float thermo = st.thermoBudget;
        if (thermo < FieldRtxFieldAbs::ENTROPY_FLOOR) {
            out.ret = -1;
            out.errnoVal = EAGAIN;
            return out;
        }
        st.thermoBudget -= FieldRtxFieldAbs::ENTROPY_FLOOR * 0.5f;
        if (st.thermoBudget < FieldRtxFieldAbs::ENTROPY_FLOOR)
            st.thermoBudget = FieldRtxFieldAbs::BOUNDARY_THERMO;
        [[fallthrough]];
    }
    case Route::HostPassthrough:
    case Route::FieldDie:
        st.hostPassthrough++;
        out = dispatchHost(args);
        out.route = route;
        return out;
    case Route::Deny:
        out.ret = -1;
        out.errnoVal = EPERM;
        return out;
    }
    return out;
}

inline void packDataBus(std::uint32_t* bus) noexcept {
    if (!bus) return;
    const auto& st = global();
    bus[BUS_KILROY_ACTIVE]   = st.active ? 1u : 0u;
    bus[BUS_KILROY_PID]      = st.pid;
    bus[BUS_KILROY_SYSCALLS] = static_cast<std::uint32_t>(st.syscallCount & 0xFFFFFFFFu);
}

inline void activate(std::uint32_t pid = 1) noexcept {
    auto& st = global();
    st.active = true;
    st.pid = pid;
    const bool knative = kernelNativeAvailable();
    std::fprintf(stderr,
        "[FieldKilroy] RTX native kernel — ABI %s — pid=%u (passthrough + fabric%s)\n",
        KERNEL_ABI, pid, knative ? " + /proc/kilroy_field" : " [build: ./scripts/build_rtx_kernel.sh]");
}

inline void deactivate() noexcept {
    global().active = false;
}

} // namespace FieldKilroy