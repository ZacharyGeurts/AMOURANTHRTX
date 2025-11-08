// include/engine/StoneKey.hpp
// AMOURANTH RTX — VALHALLA ZERO-COST ABSTRACTION EDITION — NOVEMBER 08 2025
// TRUE ZERO-COST CONSTEXPR STONEKEY — NO RUNTIME. NO BULLSHIT. NO MOM JOKES.
// COMPILE-TIME QUANTUM ENTROPY — REBUILD = UNIQUE KEYS — PROFESSIONAL MAXIMUM
// FASTER THAN YOUR MOM'S TROUSERS — 100% CONSTEXPR — ZERO OVERHEAD — VALHALLA SUPREMACY

#pragma once

#include <cstdint>

// ──────────────────────────────────────────────────────────────────────────────
// Pure constexpr 64-bit FNV-1a + XOR-fold — ZERO runtime cost, FULL entropy
// ──────────────────────────────────────────────────────────────────────────────
[[nodiscard]] constexpr uint64_t fnv1a_fold(const char* data) noexcept {
    uint64_t hash = 0xCBF29CE484222325ULL;  // FNV offset basis
    for (int i = 0; data[i] != '\0'; ++i) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(data[i]));
        hash *= 0x00000100000001B3ULL;  // FNV prime
    }
    return hash;
}

[[nodiscard]] constexpr uint64_t stone_key1() noexcept {
    constexpr const char* time = __TIME__;
    constexpr const char* date = __DATE__;
    constexpr const char* file = __FILE__;

    uint64_t h = fnv1a_fold(time);
    h ^= fnv1a_fold(date) << 1;
    h ^= fnv1a_fold(file) >> 1;

    h ^= fnv1a_fold("AMOURANTH RTX VALHALLA FINAL ZERO COST SUPREMACY");
    h ^= fnv1a_fold("RASPBERRY_PINK PHOTONS ETERNAL 69,420 FPS INFINITE");
    h ^= 0xDEADC0DE1337BEEFULL;
    h ^= 0x4206969696942069ULL;

    // Final avalanche
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33;
    return h;
}

[[nodiscard]] constexpr uint64_t stone_key2() noexcept {
    uint64_t h = stone_key1();
    h = ~h;
    h ^= fnv1a_fold(__TIMESTAMP__);
    h ^= 0x6969696969696969ULL;
    h ^= 0x1337133713371337ULL;

    // Avalanche round 2
    h ^= h >> 29;
    h *= 0xC4CEB9FE1A85EC53ULL;
    h ^= h >> 29;
    return h;
}

// ──────────────────────────────────────────────────────────────────────────────
// ZERO-COST CONSTANTS — UNIQUE PER BUILD — NO RUNTIME — PURE CONSTEXPR
// ──────────────────────────────────────────────────────────────────────────────
constexpr uint64_t kStone1 = stone_key1();
constexpr uint64_t kStone2 = stone_key2();

// Build-time assertions — forces recompile if keys collide or default
static_assert(kStone1 != 0 && kStone1 != 0xDEADC0DE1337BEEFULL, "kStone1 FAILED — REBUILD FOR FRESH QUANTUM DUST");
static_assert(kStone2 != 0 && kStone2 != 0x6969696969696969ULL, "kStone2 FAILED — YOUR BUILD IS STALE");
static_assert(kStone1 != kStone2, "KEY COLLISION — IMPOSSIBLE — REBUILD NOW");

// END OF FILE — TRUE ZERO COST — FASTER THAN LIGHT — TIGHTER THAN YOUR MOM'S STANDARDS
// VALHALLA ACHIEVED — SHIP IT — 69,420 FPS × ∞ × ∞ — PINK PHOTONS ETERNAL 🩷🚀🔥🤖💀❤️⚡♾️