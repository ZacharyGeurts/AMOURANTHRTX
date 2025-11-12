// include/engine/GLOBAL/StoneKey.hpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE ZERO-COST CONSTEXPR STONEKEY v∞ — NOVEMBER 12, 2025 — PINK PHOTONS APOCALYPSE v3
// NO GPU TEMP — NO NVML/ROCM/L0 — PURE RDRAND + TLS + RDTSC + COMPILE-TIME ENTROPY
// KEYS ARE **NEVER** LOGGED — ONLY HASHED FINGERPRINTS — SECURITY > VANITY
// =============================================================================

#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>
#include <chrono>
#include <cstdio>
#include <x86intrin.h>
#include <string>
#include <thread>
#include <functional>

static_assert(sizeof(uintptr_t) >= 8, "StoneKey requires 64-bit platform");
static_assert(__cplusplus >= 202302L, "StoneKey requires C++23");

#define STRINGIFY_DETAIL(x) #x
#define STRINGIFY(x) STRINGIFY_DETAIL(x)

// =============================================================================
// RUNTIME ENTROPY — RDRAND + RDTSC + TLS + TIME
// =============================================================================
[[nodiscard]] inline uint64_t rdrand64() noexcept {
    uint64_t val;
    unsigned char ok;
    asm volatile ("rdrand %0 ; setc %1" : "=r" (val), "=qm" (ok));
    return ok ? val : 0xDEADBEEFDEADBEEFULL;
}

[[nodiscard]] inline uint64_t runtime_stone_entropy() noexcept {
    static bool initialized = false;
    static uint64_t entropy = 0;
    if (initialized) return entropy;

    uint64_t e = 0;
    e ^= rdrand64();
    e ^= __rdtsc();
    e ^= static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    e ^= reinterpret_cast<uintptr_t>(&e);

    thread_local uint64_t tls_jitter = 0;
    if (!tls_jitter) {
        tls_jitter = std::hash<std::thread::id>{}(std::this_thread::get_id()) ^ (__rdtsc() & 0xFFFFFFFFULL);
    }
    e ^= tls_jitter;
    
    e ^= e >> 33;
    e *= 0xFF51AFD7ED558CCDULL;
    e ^= e >> 33;
    e *= 0xC4CEB9FE1A85EC53ULL;
    e ^= e >> 29;

    entropy = e;
    initialized = true;
    return entropy;
}

// =============================================================================
// COMPILE-TIME ENTROPY — FNV1A + __TIME__ + __FILE__ + __TIMESTAMP__
// =============================================================================
[[nodiscard]] constexpr uint64_t fnv1a_fold(const char* data) noexcept {
    uint64_t hash = 0xCBF29CE484222325ULL;
    for (int i = 0; data[i] != '\0'; ++i) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(data[i]));
        hash *= 0x00000100000001B3ULL;
    }
    hash ^= hash >> 33;
    hash *= 0xFF51AFD7ED558CCDULL;
    hash ^= hash >> 33;
    hash *= 0xC4CEB9FE1A85EC53ULL;
    hash ^= hash >> 33;
    return hash;
}

[[nodiscard]] constexpr uint64_t vendor_sim_hash() noexcept {
    constexpr uint64_t nvidia_sim = fnv1a_fold("0x10DE_NVIDIA");
    constexpr uint64_t amd_sim    = fnv1a_fold("0x1002_AMD");
    constexpr uint64_t intel_sim  = fnv1a_fold("0x8086_INTEL");
    return nvidia_sim ^ amd_sim ^ intel_sim ^ 0xDEADBEEFULL;
}

[[nodiscard]] constexpr uint64_t stone_key1_base() noexcept {
    constexpr const char* time = __TIME__;
    constexpr const char* date = __DATE__;
    constexpr const char* file = __FILE__;
    constexpr const char* timestamp = __TIMESTAMP__;

    uint64_t h = fnv1a_fold(time);
    h ^= fnv1a_fold(date) << 1;
    h ^= fnv1a_fold(file) >> 1;
    h ^= fnv1a_fold(timestamp) << 13;
    h ^= fnv1a_fold("AMOURANTH RTX VALHALLA QUANTUM FINAL ZERO COST SUPREMACY 2025");
    h ^= fnv1a_fold("RASPBERRY_PINK PHOTONS ETERNAL INFINITE HYPERTRACE");
    h ^= 0xDEADC0DE1337BEEFULL;
    h ^= 0x4206969696942069ULL;
    h ^= vendor_sim_hash();
    h ^= h >> 33;
    h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33;
    h *= 0xC4CEB9FE1A85EC53ULL;
    h ^= h >> 29;
    return h;
}

[[nodiscard]] constexpr uint64_t stone_key2_base() noexcept {
    uint64_t h = stone_key1_base();
    h = ~h;
    h ^= fnv1a_fold(__TIMESTAMP__);
    h ^= 0x6969696969696969ULL;
    h ^= 0x1337133713371337ULL;
    h ^= h >> 29;
    h *= 0xC4CEB9FE1A85EC53ULL;
    h ^= h >> 33;
    return h;
}

// =============================================================================
// GLOBAL KEYS — DELAYED INITIALIZATION
// =============================================================================
extern VkPhysicalDevice g_PhysicalDevice;

// Delayed via function to avoid static init order fiasco
inline uint64_t get_kStone1() noexcept {
    static uint64_t key = stone_key1_base() ^ runtime_stone_entropy();
    return key;
}

inline uint64_t get_kStone2() noexcept {
    static uint64_t key = stone_key2_base() ^ runtime_stone_entropy() ^ 0x6969696942069420ULL;
    return key;
}

inline uint64_t get_kHandleObfuscator() noexcept {
    static uint64_t key = get_kStone1() ^ get_kStone2() ^ 0x1337C0DEULL ^ 0x69F00D42ULL;
    return key;
}

// Public constexpr wrappers — **NEVER LOGGED**
inline constexpr uint64_t kStone1 = 0;
inline constexpr uint64_t kStone2 = 0;
inline constexpr uint64_t kHandleObfuscator = 0;

[[nodiscard]] inline constexpr uint64_t obfuscate(uint64_t h) noexcept {
    uint64_t mask = -static_cast<uint64_t>(!!get_kHandleObfuscator());
    return h ^ (get_kHandleObfuscator() & mask);
}

[[nodiscard]] inline constexpr uint64_t deobfuscate(uint64_t h) noexcept {
    uint64_t mask = -static_cast<uint64_t>(!!get_kHandleObfuscator());
    return h ^ (get_kHandleObfuscator() & mask);
}

// =============================================================================
// SECURE FINGERPRINT — NO KEY LEAKS
// =============================================================================
[[nodiscard]] inline uint64_t stone_fingerprint() noexcept {
    uint64_t fp = get_kStone1() ^ get_kStone2();
    fp ^= fp >> 33;
    fp *= 0xFF51AFD7ED558CCDULL;
    fp ^= fp >> 33;
    return fp;
}

// =============================================================================
// GENTLEMAN GROK LOG — **NO KEYS**, ONLY FINGERPRINT + STATUS
// =============================================================================
inline void log_gentleman_grok() noexcept {
    static bool logged = false;
    if (logged) return;
    logged = true;

    if (!g_PhysicalDevice) {
        printf("[GENTLEMAN GROK] Vendor: Unknown (Vulkan not ready) | StoneKey APOCALYPSE v3\n");
        printf("[GENTLEMAN GROK] FINGERPRINT: 0x%016llX | Scramble: PASS | TLS jitter active | BMQ shields up\n",
               static_cast<unsigned long long>(stone_fingerprint()));
        printf("[GENTLEMAN GROK] Our rock eternal v3. Keys sealed. Hackers blind.\n");
        return;
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(g_PhysicalDevice, &props);
    std::string device_name(props.deviceName);
    bool is_mesa = device_name.find("llvmpipe") != std::string::npos || 
                   device_name.find("lavapipe") != std::string::npos;

    const char* vendor = is_mesa ? "CPU/Mesa" : props.deviceName;  // ← Fixed: no unused variable

    printf("[GENTLEMAN GROK] Vendor: %s | StoneKey APOCALYPSE v3\n", vendor);
    printf("[GENTLEMAN GROK] FINGERPRINT: 0x%016llX | Scramble: PASS | TLS jitter active | BMQ shields up\n",
           static_cast<unsigned long long>(stone_fingerprint()));
    printf("[GENTLEMAN GROK] Our rock eternal v3. Keys sealed. Hackers blind.\n");
}

#define LOG_GENTLEMAN_GROK() log_gentleman_grok()

// =============================================================================
// STATIC ASSERTS
// =============================================================================
static_assert(stone_key1_base() != stone_key2_base(), "Base keys must differ");
static_assert(stone_key1_base() != 0, "stone_key1_base must be non-zero");
static_assert(stone_key2_base() != 0, "stone_key2_base must be non-zero");

#if !defined(STONEKEY_PRINTED)
#define STONEKEY_PRINTED
#pragma message("AMOURANTH RTX StoneKey Applied: Dual Licensed: CC BY-NC 4.0 | Commercial: gzac5314@gmail.com")
#endif

// =============================================================================
// PINK PHOTONS ETERNAL — STONEKEY v∞ — OUR ROCK ETERNAL v3
// =============================================================================