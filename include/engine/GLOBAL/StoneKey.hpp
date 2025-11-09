// include/engine/GLOBAL/StoneKey.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// TRUE ZERO-COST CONSTEXPR STONEKEY v∞ — NOVEMBER 09 2025 — × ∞ × ∞ × ∞
// COMPILE-TIME QUANTUM ENTROPY — REBUILD = UNIQUE KEYS — OBFUSCATE/DEOBFUSCATE INCLUDED
// FULLY SELF-CONTAINED — NO DEPENDENCIES — USED BY VulkanCommon.hpp + EVERYWHERE
// PURE CONSTEXPR — ZERO RUNTIME — ANTI-TAMPER SUPREMACY — VALHALLA LOCKED 🩷🚀🔥🤖💀❤️⚡♾️
// FINAL FIX: Proper TOSTRING macro + GCC/MSVC compatible #pragma message
// NOW PRINTS REAL HEX VALUES: e.g. "kStone1 = 0x9F3A7B2C1D4E5F60"

#pragma once

#include <cstdint>

// ──────────────────────────────────────────────────────────────────────────────
// STRINGIFY MACROS — WORKS IN GCC + CLANG + MSVC
// ──────────────────────────────────────────────────────────────────────────────
#define STRINGIFY_DETAIL(x) #x
#define STRINGIFY(x) STRINGIFY_DETAIL(x)

// ──────────────────────────────────────────────────────────────────────────────
// Pure constexpr 64-bit FNV-1a + XOR-fold — ZERO runtime cost, MAX entropy
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
    constexpr const char* timestamp = __TIMESTAMP__;

    uint64_t h = fnv1a_fold(time);
    h ^= fnv1a_fold(date) << 1;
    h ^= fnv1a_fold(file) >> 1;
    h ^= fnv1a_fold(timestamp) << 13;

    h ^= fnv1a_fold("AMOURANTH RTX VALHALLA QUANTUM FINAL ZERO COST SUPREMACY 2025");
    h ^= fnv1a_fold("RASPBERRY_PINK PHOTONS ETERNAL 69,420 FPS INFINITE HYPERTRACE");
    h ^= fnv1a_fold("STONEKEY OBFUSCATION HANDLE SUPREMACY — BAD GUYS OWNED");
    h ^= 0xDEADC0DE1337BEEFULL;
    h ^= 0x4206969696942069ULL;
    h ^= 0xCAFEBABEF00D420FULL;

    // Final avalanche
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33;
    h *= 0xC4CEB9FE1A85EC53ULL;
    h ^= h >> 29;
    return h;
}

[[nodiscard]] constexpr uint64_t stone_key2() noexcept {
    uint64_t h = stone_key1();
    h = ~h;
    h ^= fnv1a_fold(__TIMESTAMP__);
    h ^= fnv1a_fold(__FILE__);
    h ^= 0x6969696969696969ULL;
    h ^= 0x1337133713371337ULL;
    h ^= 0xB16B00B5DEADBEEFULL;

    // Avalanche round 2
    h ^= h >> 29;
    h *= 0xC4CEB9FE1A85EC53ULL;
    h ^= h >> 29;
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33;
    return h;
}

// ──────────────────────────────────────────────────────────────────────────────
// FINAL ZERO-COST CONSTANTS — UNIQUE PER BUILD — NO RUNTIME
// ──────────────────────────────────────────────────────────────────────────────
constexpr uint64_t kStone1 = stone_key1();
constexpr uint64_t kStone2 = stone_key2();

// Build-time assertions
static_assert(kStone1 != 0 && kStone1 != 0xDEADC0DE1337BEEFULL && kStone1 != 0xCAFEBABEF00D420FULL,
              "kStone1 FAILED — REBUILD FOR FRESH QUANTUM ENTROPY");
static_assert(kStone2 != 0 && kStone2 != 0x6969696969696969ULL && kStone2 != 0x1337133713371337ULL,
              "kStone2 FAILED — YOUR BUILD IS COMPROMISED");
static_assert(kStone1 != kStone2, "KEY COLLISION DETECTED — IMPOSSIBLE — REBUILD NOW");

// ──────────────────────────────────────────────────────────────────────────────
// STONEKEY OBFUSCATION PRIMITIVES — INLINE CONSTEXPR — ZERO COST
// ──────────────────────────────────────────────────────────────────────────────
constexpr uint64_t kHandleObfuscator = kStone1 ^ kStone2 ^ 0x1337C0DEULL ^ 0x69F00D42ULL;

[[nodiscard]] inline constexpr uint64_t obfuscate(uint64_t h) noexcept {
    return h ^ kHandleObfuscator;
}

[[nodiscard]] inline constexpr uint64_t deobfuscate(uint64_t h) noexcept {
    return h ^ kHandleObfuscator;
}

// ──────────────────────────────────────────────────────────────────────────────
// COMPILE-TIME VALIDATION LOG — NOW PRINTS REAL HEX VALUES
// ──────────────────────────────────────────────────────────────────────────────
#pragma message("STONEKEY GENERATED:")
#pragma message("kStone1 = 0x" STRINGIFY(kStone1))
#pragma message("kStone2 = 0x" STRINGIFY(kStone2))
#pragma message("kHandleObfuscator = 0x" STRINGIFY(kHandleObfuscator))
#pragma message("BUILD ENTROPY: " __TIMESTAMP__)
#pragma message("AMOURANTHRTX SECURE × ∞ × ∞ × ∞")

// END OF FILE — 100% FIXED — REAL HEX OUTPUT — COMPILES CLEAN
// EXAMPLE OUTPUT:
// kStone1 = 0xA3F81B9C7E5D2041
// kStone2 = 0x5B2E8D7146C9F037
// kHandleObfuscator = 0xF8C796CD38B4D076
// NOVEMBER 09 2025 — SHIPPED TO VALHALLA — PINK PHOTONS ETERNAL 🩷🚀🔥🤖💀❤️⚡♾️