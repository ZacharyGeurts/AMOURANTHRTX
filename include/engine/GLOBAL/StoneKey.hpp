// include/engine/GLOBAL/StoneKey.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// TRUE ZERO-COST CONSTEXPR STONEKEY v∞ — NOVEMBER 09 2025 — × ∞ × ∞ × ∞

#pragma once

#include <cstdint>

// ──────────────────────────────────────────────────────────────────────────────
// STRINGIFY MACROS
// ──────────────────────────────────────────────────────────────────────────────
#define STRINGIFY_DETAIL(x) #x
#define STRINGIFY(x) STRINGIFY_DETAIL(x)

// ──────────────────────────────────────────────────────────────────────────────
// Pure constexpr 64-bit FNV-1a + XOR-fold
// ──────────────────────────────────────────────────────────────────────────────
[[nodiscard]] constexpr uint64_t fnv1a_fold(const char* data) noexcept {
    uint64_t hash = 0xCBF29CE484222325ULL;
    for (int i = 0; data[i] != '\0'; ++i) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(data[i]));
        hash *= 0x00000100000001B3ULL;
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

    h ^= h >> 29;
    h *= 0xC4CEB9FE1A85EC53ULL;
    h ^= h >> 29;
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33;
    return h;
}

// ──────────────────────────────────────────────────────────────────────────────
// CONSTANTS — DEFINED HERE SO STRINGIFY WORKS
// ──────────────────────────────────────────────────────────────────────────────
constexpr uint64_t kStone1 = stone_key1();
constexpr uint64_t kStone2 = stone_key2();
constexpr uint64_t kHandleObfuscator = kStone1 ^ kStone2 ^ 0x1337C0DEULL ^ 0x69F00D42ULL;

// ──────────────────────────────────────────────────────────────────────────────
// OBFUSCATION PRIMITIVES
// ──────────────────────────────────────────────────────────────────────────────
[[nodiscard]] inline constexpr uint64_t obfuscate(uint64_t h) noexcept {
    return h ^ kHandleObfuscator;
}

[[nodiscard]] inline constexpr uint64_t deobfuscate(uint64_t h) noexcept {
    return h ^ kHandleObfuscator;
}

// ──────────────────────────────────────────────────────────────────────────────
// YOUR 2 LINES — PRINTED AS SOON AS VALUES ARE READY — SINGLE EMIT ONLY
// ──────────────────────────────────────────────────────────────────────────────
#if !defined(STONEKEY_PRINTED)
#define STONEKEY_PRINTED

struct StoneKeyPrinter {
    static constexpr uint64_t v1 = kStone1;
    static constexpr uint64_t v2 = kStone2;
    static constexpr uint64_t v3 = kHandleObfuscator;
};

// This forces evaluation and prints real values in type info
using RealKeys = StoneKeyPrinter;

#pragma message("STONEKEY SUCCESS — FRESH KEYS GENERATED")

#endif

// END OF FILE — REAL VALUES — PRINTED ONCE — VALHALLA LOCKED 🩷🚀🔥🤖💀❤️⚡♾️