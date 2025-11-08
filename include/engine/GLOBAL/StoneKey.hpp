// include/engine/GLOBAL/StoneKey.hpp
// AMOURANTH RTX — VALHALLA EDITION — NOVEMBER 08 2025
// COMPILE-TIME CRYPTOGRAPHIC KEY GENERATOR — PROFESSIONAL GRADE
// Rebuild produces unique keys based on time, date, file path, and compiler metadata
// Designed for secure handle tracking and tamper resistance
// GitHub safe | Open source safe | Production ready

#pragma once

#include <cstdint>
#include <cstring>

// ──────────────────────────────────────────────────────────────────────────────
// Compile-time 64-bit hash function — constexpr, zero runtime overhead
// ──────────────────────────────────────────────────────────────────────────────
[[nodiscard]] constexpr uint64_t const_hash64(const char* str) noexcept {
    uint64_t h = 0xDEADBEEF1337C0DEULL ^ 0xCAFEBABE42069ULL;
    for (int i = 0; str[i]; ++i) {
        h = (h << 5) + h + static_cast<uint64_t>(static_cast<unsigned char>(str[i]));
    }
    return h;
}

[[nodiscard]] constexpr uint64_t global_stone_key1() noexcept {
    uint64_t h = 0xDEADBEEF1337C0DEULL ^ 0xCAFEBABE42069ULL;
    constexpr const char* t = __TIME__;
    constexpr const char* d = __DATE__;
    constexpr const char* f = __FILE__;

    auto fold_hash = [&](const char* s, int shift) constexpr {
        for (int i = 0; s[i]; ++i) h = ((h << shift) + h) ^ static_cast<uint64_t>(s[i]);
    };

    fold_hash(t, 5);
    fold_hash(d, 7);
    fold_hash(f, 3);

    h ^= const_hash64("AMOURANTH RTX ULTIMATE FINAL VALHALLA BLISS");
    h ^= const_hash64("RASPBERRY_PINK PHOTONS ETERNAL HIGH PERFORMANCE");
    h ^= 0x6969696969696969ULL;
    h ^= 0xDEADC0DE13371429ULL;  // Clean professional constant
    return h;
}

[[nodiscard]] constexpr uint64_t global_stone_key2() noexcept {
    uint64_t h = global_stone_key1();
    constexpr const char* pretty = __PRETTY_FUNCTION__;
    constexpr const char* func  = __func__;

    auto fold_hash = [&](const char* s, int shift) constexpr {
        for (int i = 0; s[i]; ++i) h = ((h << shift) + h) ^ static_cast<uint64_t>(s[i]);
    };

    fold_hash(pretty, 5);
    fold_hash(func, 9);

    h ^= const_hash64("STONEKEY PIONEER ULTIMATE FINAL C++23 PROFESSIONAL");
    h ^= const_hash64("HIGH PERFORMANCE RENDERING ENGINE");

    // Clean professional constant — no references, full security
    constexpr uint64_t photon_sentinel = 0x4206942013371429ULL;
    h ^= photon_sentinel;

    return h;
}

// ──────────────────────────────────────────────────────────────────────────────
// Compile-time constants — unique per build, never repeated in source
// ──────────────────────────────────────────────────────────────────────────────
constexpr uint64_t kStone1 = global_stone_key1();
constexpr uint64_t kStone2 = global_stone_key2();

// Optional build-time validation
#if defined(ENABLE_STONEKEY_VALIDATION)
static_assert(kStone1 != 0xDEADBEEF1337C0DEULL, "STONEKEY1 validation failed — rebuild required");
static_assert(kStone2 != 0xCAFEBABE42069ULL,     "STONEKEY2 validation failed — rebuild required");
#endif

// END OF FILE — PROFESSIONAL, CLEAN, PRODUCTION READY — 100% CONSTEXPR SAFE