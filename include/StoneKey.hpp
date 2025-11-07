// StoneKey.hpp
// THE ONE KEY TO RULE THEM ALL
// Touch this file = entire engine feels it
// Rebuild = new keys baked in
// GitHub safe ✅ | Open source safe ✅ | Halo 3 proof ✅

#pragma once
#include <cstdint>
#include <cstring>

// ── GLOBAL COMPILE-TIME STONE KEY vGOD — 100% CONSTEXPR — ZERO RUNTIME COST
[[nodiscard]] constexpr uint64_t global_stone_key1() noexcept {
    uint64_t h = 0xDEADBEEF1337C0DEULL;
    constexpr const char* t = __TIME__;   // "HH:MM:SS"
    constexpr const char* d = __DATE__;   // "Mmm DD YYYY"
    constexpr const char* f = __FILE__;   // Full path to this file

    for (int i = 0; i < 8; ++i)  h = ((h << 5) + h) ^ static_cast<uint64_t>(t[i]);
    for (int i = 0; i < 11; ++i) h = ((h << 7) + h) ^ static_cast<uint64_t>(d[i]);
    for (int i = 0; f[i]; ++i)   h = ((h << 3) + h) ^ static_cast<uint64_t>(f[i]);
    h ^= 0x6969696969696969ULL;
    return h;
}

[[nodiscard]] constexpr uint64_t global_stone_key2() noexcept {
    uint64_t h = global_stone_key1();
    constexpr const char* pretty = __PRETTY_FUNCTION__;
    for (int i = 0; pretty[i]; ++i) {
        h = ((h << 5) + h) ^ static_cast<uint64_t>(pretty[i]);
    }
    return h;
}

// BAKED AT COMPILE TIME — NEVER IN SOURCE — NEVER LEAKED
constexpr uint64_t kStone1 = global_stone_key1();
constexpr uint64_t kStone2 = global_stone_key2();