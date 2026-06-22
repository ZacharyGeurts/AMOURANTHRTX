// FieldKilroy acceptance — syscall router + ELF sniff + data bus packing.

#include "FieldKilroyCompat.hpp"
#include "FieldKilroyLayer.hpp"
#include "FieldKilroyLoader.hpp"
#include "FieldPlatform.hpp"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <vector>

static void test_syscall_router() {
    FieldKilroy::activate(42);
    FieldKilroy::SyscallArgs args{};
    args.nr = static_cast<std::int64_t>(FieldKilroy::Nr::getpid);
    auto r = FieldKilroy::dispatch(args);
    assert(r.ret > 0);
    assert(r.route == FieldKilroy::Route::HostPassthrough);
    std::printf("[OK] getpid -> %lld (host passthrough)\n", static_cast<long long>(r.ret));

    args.nr = static_cast<std::int64_t>(FieldKilroy::Nr::sched_yield);
    r = FieldKilroy::dispatch(args);
    assert(r.ret == 0);
    assert(r.route == FieldKilroy::Route::Fabric);
    std::printf("[OK] sched_yield -> fabric route\n");

    args.nr = static_cast<std::int64_t>(FieldKilroy::Nr::read);
    args.a0 = -1;
    args.a1 = 0;
    args.a2 = 0;
    r = FieldKilroy::dispatch(args);
    assert(r.ret < 0);
    std::printf("[OK] read invalid fd -> %lld errno=%d\n",
        static_cast<long long>(r.ret), r.errnoVal);
}

static void test_data_bus() {
    std::uint32_t bus[64]{};
    FieldKilroy::packDataBus(bus);
    assert(bus[FieldKilroy::BUS_KILROY_ACTIVE] == 1u);
    assert(bus[FieldKilroy::BUS_KILROY_PID] == 42u);
    assert(bus[FieldKilroy::BUS_KILROY_SYSCALLS] >= 3u);
    std::printf("[OK] data_bus slots 29-31 packed\n");
}

static void test_elf_sniff() {
    static const std::uint8_t hdr[] = {
        0x7F, 'E', 'L', 'F', 2, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0x02, 0x00, 0x3E, 0x00, 0x01, 0x00, 0x00, 0x00,
    };
    assert(FieldKilroy::isElf64(hdr, sizeof(hdr)));
    std::printf("[OK] ELF64 magic recognized\n");
}

static void test_guest_load_stub() {
    std::vector<std::uint8_t> ram(FieldPlatform::GUEST_RAM_BYTES, 0);
    static const std::uint8_t bad[] = {0x4D, 0x5A};
    auto r = FieldKilroy::loadIntoGuest(ram.data(), bad, sizeof(bad));
    assert(!r.ok);
    std::printf("[OK] non-ELF rejected: %s\n", r.error);
}

int main() {
    std::printf("=== FieldKilroy QA (ABI %s) ===\n", FieldKilroy::KERNEL_ABI);
    test_syscall_router();
    test_data_bus();
    test_elf_sniff();
    test_guest_load_stub();
    std::printf("=== FieldKilroy QA PASS ===\n");
    return 0;
}