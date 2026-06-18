// Benchmark: FieldDrives refresh / pack / pump — reports ns/op metrics.
#include "FieldAmmoFat.hpp"
#include "FieldDos.hpp"
#include "FieldDrives.hpp"
#include "FieldPlatform.hpp"

#include <chrono>
#include <cstdio>
#include <vector>

using Clock = std::chrono::steady_clock;

static double nsPerOp(const Clock::time_point& t0, const Clock::time_point& t1, std::uint64_t n) {
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
    return static_cast<double>(ns) / static_cast<double>(n);
}

static std::vector<std::uint8_t> gBuf;

static void seedDosState() {
    gBuf.assign(FieldPlatform::FIELD_X86_DIE_UINTS * 4, 0);
    FieldDos::loadHdImage(FieldDos::defaultHdPath("."));
    FieldDos::loadFloppyIntoGuest(gBuf.data(), FieldPlatform::DIE_HEADER_UINTS * 4,
        FieldDos::defaultImagePath("."));
    FieldAmmoFat::mount();
    FieldDrives::invalidate();
    FieldDrives::refresh(true);
}

int main() {
    constexpr std::uint64_t kIters = 200'000u;
    std::vector<std::uint8_t> ram(FieldPlatform::FIELD_X86_DIE_UINTS * 4, 0);
    std::uint32_t bus[64]{};

    seedDosState();

    FieldDrives::invalidate();
    auto t0 = Clock::now();
    for (std::uint64_t i = 0; i < kIters; ++i)
        FieldDrives::refresh(true);
    auto t1 = Clock::now();
    const double refreshColdNs = nsPerOp(t0, t1, kIters);

    auto t2 = Clock::now();
    for (std::uint64_t i = 0; i < kIters; ++i)
        FieldDrives::refresh();
    auto t3 = Clock::now();
    const double refreshCachedNs = nsPerOp(t2, t3, kIters);

    auto t4 = Clock::now();
    for (std::uint64_t i = 0; i < kIters; ++i)
        FieldDrives::packDataBus(bus, 64u);
    auto t5 = Clock::now();
    const double packNs = nsPerOp(t4, t5, kIters);

    auto t6 = Clock::now();
    for (std::uint64_t i = 0; i < kIters; ++i) {
        FieldDrives::tick(ram.data(), 0.f);
        FieldDrives::packDataBus(bus, 64u);
    }
    auto t7 = Clock::now();
    const double frameNs = nsPerOp(t6, t7, kIters);

    auto t8 = Clock::now();
    for (std::uint64_t i = 0; i < kIters; ++i)
        (void)FieldDrives::readyLetters();
    auto t9 = Clock::now();
    const double lettersNs = nsPerOp(t8, t9, kIters);

    std::printf("METRIC refresh_cold_ns=%.1f\n", refreshColdNs);
    std::printf("METRIC refresh_cached_ns=%.1f\n", refreshCachedNs);
    std::printf("METRIC pack_bus_ns=%.1f\n", packNs);
    std::printf("METRIC tick_pack_ns=%.1f\n", frameNs);
    std::printf("METRIC ready_letters_ns=%.1f\n", lettersNs);
    std::printf("METRIC cache_speedup=%.1fx\n", refreshColdNs / refreshCachedNs);

    if (refreshCachedNs > 500.0) {
        std::fprintf(stderr, "FAIL cached refresh %.1f ns/op exceeds 500 ns budget\n", refreshCachedNs);
        return 1;
    }
    if (frameNs > 2'000.0) {
        std::fprintf(stderr, "FAIL tick+pack %.1f ns/frame exceeds 2 us budget\n", frameNs);
        return 1;
    }
    std::printf("OK drives bench\n");
    return 0;
}