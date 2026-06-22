// Host POSIX passthrough — preserves KILROY userspace binary compatibility.

#include "FieldKilroyCompat.hpp"

#include <cerrno>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/ioctl.h>
#include <sched.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/random.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

namespace FieldKilroy {

namespace {

inline std::int64_t ok(std::int64_t v) noexcept { return v; }
inline std::int64_t err() noexcept { return -static_cast<std::int64_t>(errno); }

std::int64_t hostCall(std::int64_t nr, std::int64_t a0, std::int64_t a1,
                      std::int64_t a2, std::int64_t a3, std::int64_t a4,
                      std::int64_t a5) noexcept {
    errno = 0;
    return ::syscall(nr, a0, a1, a2, a3, a4, a5);
}

} // namespace

SyscallResult dispatchHost(const SyscallArgs& args) noexcept {
    SyscallResult out{};
    out.route = Route::HostPassthrough;

    const auto n = static_cast<Nr>(args.nr);
    switch (n) {
    case Nr::read:
        out.ret = ok(::read(static_cast<int>(args.a0),
                            reinterpret_cast<void*>(args.a1),
                            static_cast<std::size_t>(args.a2)));
        break;
    case Nr::write:
        out.ret = ok(::write(static_cast<int>(args.a0),
                             reinterpret_cast<const void*>(args.a1),
                             static_cast<std::size_t>(args.a2)));
        break;
    case Nr::openat:
        out.ret = ok(::openat(static_cast<int>(args.a0),
                              reinterpret_cast<const char*>(args.a1),
                              static_cast<int>(args.a2),
                              static_cast<mode_t>(args.a3)));
        break;
    case Nr::close:
        out.ret = ok(::close(static_cast<int>(args.a0)));
        break;
    case Nr::lseek:
        out.ret = ok(::lseek(static_cast<int>(args.a0),
                             static_cast<off_t>(args.a1),
                             static_cast<int>(args.a2)));
        break;
    case Nr::mmap:
        out.ret = reinterpret_cast<std::int64_t>(
            ::mmap(reinterpret_cast<void*>(args.a0),
                   static_cast<std::size_t>(args.a1),
                   static_cast<int>(args.a2),
                   static_cast<int>(args.a3),
                   static_cast<int>(args.a4),
                   static_cast<off_t>(args.a5)));
        if (out.ret == -1) out.ret = err();
        break;
    case Nr::munmap:
        out.ret = ok(::munmap(reinterpret_cast<void*>(args.a0),
                              static_cast<std::size_t>(args.a1)));
        break;
    case Nr::mprotect:
        out.ret = ok(::mprotect(reinterpret_cast<void*>(args.a0),
                                static_cast<std::size_t>(args.a1),
                                static_cast<int>(args.a2)));
        break;
    case Nr::brk:
        out.ret = reinterpret_cast<std::int64_t>(::sbrk(static_cast<std::intptr_t>(args.a0)));
        if (out.ret == -1) out.ret = err();
        break;
    case Nr::getpid:
        out.ret = ok(::getpid());
        break;
    case Nr::getppid:
        out.ret = ok(::getppid());
        break;
    case Nr::getuid:
        out.ret = ok(::getuid());
        break;
    case Nr::getgid:
        out.ret = ok(::getgid());
        break;
    case Nr::geteuid:
        out.ret = ok(::geteuid());
        break;
    case Nr::getegid:
        out.ret = ok(::getegid());
        break;
    case Nr::exit:
    case Nr::exit_group:
        ::_exit(static_cast<int>(args.a0));
        break;
    case Nr::clone:
        out.ret = hostCall(args.nr, args.a0, args.a1, args.a2, args.a3, args.a4, args.a5);
        break;
    case Nr::execve:
        out.ret = hostCall(args.nr, args.a0, args.a1, args.a2, 0, 0, 0);
        break;
    case Nr::wait4:
        out.ret = ok(::wait4(static_cast<pid_t>(args.a0),
                             reinterpret_cast<int*>(args.a1),
                             static_cast<int>(args.a2),
                             reinterpret_cast<struct rusage*>(args.a3)));
        break;
    case Nr::nanosleep: {
        const auto* req = reinterpret_cast<const struct timespec*>(args.a0);
        auto* rem = reinterpret_cast<struct timespec*>(args.a1);
        out.ret = ok(::nanosleep(req, rem));
        break;
    }
    case Nr::clock_gettime: {
        auto* tp = reinterpret_cast<struct timespec*>(args.a1);
        out.ret = ok(::clock_gettime(static_cast<clockid_t>(args.a0), tp));
        break;
    }
    case Nr::gettimeofday: {
        auto* tv = reinterpret_cast<struct timeval*>(args.a0);
        auto* tz = reinterpret_cast<struct timezone*>(args.a1);
        out.ret = ok(::gettimeofday(tv, tz));
        break;
    }
    case Nr::getrandom:
        out.ret = ok(::getrandom(reinterpret_cast<void*>(args.a0),
                                 static_cast<std::size_t>(args.a1),
                                 static_cast<unsigned>(args.a2)));
        break;
    case Nr::sched_yield:
        out.ret = ok(::sched_yield());
        break;
    case Nr::getrusage:
        out.ret = ok(::getrusage(static_cast<int>(args.a0),
                                 reinterpret_cast<struct rusage*>(args.a1)));
        break;
    case Nr::futex:
        out.ret = hostCall(SYS_futex,
                           args.a0, args.a1, args.a2, args.a3, args.a4, args.a5);
        break;
    case Nr::ioctl:
        out.ret = ok(::ioctl(static_cast<int>(args.a0),
                             static_cast<unsigned long>(args.a1),
                             reinterpret_cast<void*>(args.a2)));
        break;
    case Nr::fcntl:
        out.ret = ok(::fcntl(static_cast<int>(args.a0),
                             static_cast<int>(args.a1),
                             static_cast<int>(args.a2)));
        break;
    case Nr::pipe2:
        out.ret = ok(::pipe2(reinterpret_cast<int*>(args.a0),
                             static_cast<int>(args.a1)));
        break;
    case Nr::dup:
        out.ret = ok(::dup(static_cast<int>(args.a0)));
        break;
    case Nr::dup3:
        out.ret = ok(::dup3(static_cast<int>(args.a0),
                            static_cast<int>(args.a1),
                            static_cast<int>(args.a2)));
        break;
    case Nr::ppoll: {
        auto* fds = reinterpret_cast<struct pollfd*>(args.a0);
        auto* tsp = reinterpret_cast<const struct timespec*>(args.a3);
        auto* sigmask = reinterpret_cast<const sigset_t*>(args.a4);
        out.ret = ok(::ppoll(fds, static_cast<nfds_t>(args.a1), tsp, sigmask));
        break;
    }
    case Nr::socket:
        out.ret = ok(::socket(static_cast<int>(args.a0),
                              static_cast<int>(args.a1),
                              static_cast<int>(args.a2)));
        break;
    case Nr::connect:
        out.ret = ok(::connect(static_cast<int>(args.a0),
                               reinterpret_cast<const struct sockaddr*>(args.a1),
                               static_cast<socklen_t>(args.a2)));
        break;
    case Nr::bind:
        out.ret = ok(::bind(static_cast<int>(args.a0),
                            reinterpret_cast<const struct sockaddr*>(args.a1),
                            static_cast<socklen_t>(args.a2)));
        break;
    case Nr::listen:
        out.ret = ok(::listen(static_cast<int>(args.a0), static_cast<int>(args.a1)));
        break;
    case Nr::accept4:
        out.ret = ok(::accept4(static_cast<int>(args.a0),
                               reinterpret_cast<struct sockaddr*>(args.a1),
                               reinterpret_cast<socklen_t*>(args.a2),
                               static_cast<int>(args.a3)));
        break;
    case Nr::arch_prctl:
        out.ret = hostCall(SYS_arch_prctl, args.a0, args.a1, 0, 0, 0, 0);
        break;
    default:
        out.ret = hostCall(args.nr, args.a0, args.a1, args.a2, args.a3, args.a4, args.a5);
        break;
    }

    if (out.ret < 0 && out.ret > -4096)
        out.errnoVal = static_cast<std::int32_t>(-out.ret);
    else if (out.ret == -1)
        out.errnoVal = errno;
    return out;
}

} // namespace FieldKilroy