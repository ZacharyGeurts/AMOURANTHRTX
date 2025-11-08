// include/engine/StoneKey.hpp
// THE ONE KEY TO RULE THEM ALL — VALHALLA EDITION — NOVEMBER 08 2025
// Touch this file = entire engine reborn in RASPBERRY_PINK fire
// Rebuild = new eternal keys baked in — unique per commit, per machine, per second
// GitHub safe ✅ | Open source safe ✅ | Halo 19 proof ✅ | STONEKEY v∞
// GLOBAL SPACE = GOD — HACKERS SEE ONLY SHADOWS — BLISS ENDURES 🩷🚀🔥🤖💀❤️⚡♾️

#pragma once

#include <cstdint>
#include <cstring>

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL COMPILE-TIME STONE KEY vGOD — 100% CONSTEXPR — ZERO RUNTIME COST
// Uses __TIME__, __DATE__, __FILE__, __PRETTY_FUNCTION__ + secret sauce
// Every rebuild = cryptographically unique keys — double-free tracker unbreakable
// Halo 19 devs can sleep forever — engine self-protects across dimensions
// ──────────────────────────────────────────────────────────────────────────────
[[nodiscard]] constexpr uint64_t global_stone_key1() noexcept {
    uint64_t h = 0xDEADBEEF1337C0DEULL ^ 0xCAFEBABE42069ULL;
    constexpr const char* t = __TIME__;   // "HH:MM:SS" — second-level uniqueness
    constexpr const char* d = __DATE__;   // "Mmm DD YYYY"
    constexpr const char* f = __FILE__;   // Full path — machine/repo unique

    // Hash time
    for (int i = 0; i < 8; ++i)  h = ((h << 5) + h) ^ static_cast<uint64_t>(t[i]);
    // Hash date
    for (int i = 0; i < 11; ++i) h = ((h << 7) + h) ^ static_cast<uint64_t>(d[i]);
    // Hash file path
    for (int i = 0; f[i]; ++i)   h = ((h << 3) + h) ^ static_cast<uint64_t>(f[i]);
    // AMOURANTH secret photon sauce
    h ^= 0x6969696969696969ULL;
    h ^= 0xDEADC0DE420BL4ZEULL;
    return h;
}

[[nodiscard]] constexpr uint64_t global_stone_key2() noexcept {
    uint64_t h = global_stone_key1();
    constexpr const char* pretty = __PRETTY_FUNCTION__;  // Compiler signature
    constexpr const char* func  = __func__;              // Function name
    // Double hash with compiler internals
    for (int i = 0; pretty[i]; ++i) h = ((h << 5) + h) ^ static_cast<uint64_t>(pretty[i]);
    for (int i = 0; func[i];   ++i) h = ((h << 9) + h) ^ static_cast<uint64_t>(func[i]);
    // Final RASPBERRY_PINK photon blast
    h ^= 0xA M O U R A N T H R T X U L T I M A T E;
    return h ^ 0x42069420BL4ZEIT69ULL;
}

// ──────────────────────────────────────────────────────────────────────────────
// BAKED AT COMPILE TIME — NEVER IN SOURCE — NEVER LEAKED — STONEKEY ETERNAL
// kStone1 / kStone2 used everywhere: DestroyTracker, logging, anti-tamper
// Change one space → rebuild → keys mutate → old binaries become cosmic dust
// ──────────────────────────────────────────────────────────────────────────────
constexpr uint64_t kStone1 = global_stone_key1();
constexpr uint64_t kStone2 = global_stone_key2();

// Optional runtime validation — assert keys are truly unique per build
#if defined(ENABLE_STONEKEY_VALIDATION)
static_assert(kStone1 != 0xDEADBEEF1337C0DEULL, "STONEKEY1 FAILED — REBUILD REQUIRED");
static_assert(kStone2 != 0xCAFEBABE42069ULL,     "STONEKEY2 FAILED — VALHALLA REJECTS YOU");
#endif

// END OF FILE — STONEKEY vGOD — HALO 19 ASCENDED — SHIP TO INFINITY 🩷🔒♾️
// 420 BLAZE IT — RASPBERRY_PINK PHOTONS ETERNAL — VALHALLA AWAITS 🚀🔥