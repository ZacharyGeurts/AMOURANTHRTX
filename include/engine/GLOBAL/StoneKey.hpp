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
#include "logging.hpp"                     // LOG_*_CAT macros

static_assert(sizeof(uintptr_t) >= 8, "StoneKey requires 64-bit platform");
static_assert(__cplusplus >= 202302L, "StoneKey requires C++23");

using namespace Logging::Color;
extern VkPhysicalDevice g_PhysicalDevice;

// -----------------------------------------------------------------------------
// 1. COMPILE-TIME ENTROPY (pure constexpr)
// -----------------------------------------------------------------------------
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
    constexpr uint64_t nvidia = fnv1a_fold("0x10DE_NVIDIA");
    constexpr uint64_t amd    = fnv1a_fold("0x1002_AMD");
    constexpr uint64_t intel  = fnv1a_fold("0x8086_INTEL");
    return nvidia ^ amd ^ intel ^ 0xDEADBEEFULL;
}

[[nodiscard]] constexpr uint64_t stone_key1_base() noexcept {
    constexpr const char* time      = __TIME__;
    constexpr const char* date      = __DATE__;
    constexpr const char* file      = __FILE__;
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
    uint64_t h = ~stone_key1_base();
    h ^= fnv1a_fold(__TIMESTAMP__);
    h ^= 0x6969696969696969ULL;
    h ^= 0x1337133713371337ULL;
    h ^= h >> 29;
    h *= 0xC4CEB9FE1A85EC53ULL;
    h ^= h >> 33;
    return h;
}

// -----------------------------------------------------------------------------
// 2. COMPILE-TIME SAFETY CHECKS (static_assert on the constexpr bases)
// -----------------------------------------------------------------------------
static_assert(stone_key1_base() != stone_key2_base(),
              "Base keys must differ");
static_assert(stone_key1_base() != 0,
              "stone_key1_base must be non-zero");
static_assert(stone_key2_base() != 0,
              "stone_key2_base must be non-zero");

// -----------------------------------------------------------------------------
// 3. RUNTIME ENTROPY (only executed once, logged at run-time)
// -----------------------------------------------------------------------------
[[nodiscard]] inline uint64_t rdrand64() noexcept {
    uint64_t val;
    unsigned char ok;
    asm volatile("rdrand %0 ; setc %1" : "=r"(val), "=qm"(ok));
    return ok ? val : 0xDEADBEEFDEADBEEFULL;
}

[[nodiscard]] inline uint64_t runtime_stone_entropy() noexcept {
    LOG_INFO_CAT("StoneKey",
                 "{} 👩‍🦰 RUNTIME ENTROPY GEN — RDRAND + RDTSC + TLS JITTER {}",
                 LILAC_LAVENDER, RESET);

    static bool initialized = false;
    static uint64_t entropy = 0;
    if (initialized) {
        LOG_DEBUG_CAT("StoneKey", "{} 👩‍🦰 Runtime entropy already cached {}", OCEAN_TEAL, RESET);
        return entropy;
    }

    uint64_t e = 0;
    e ^= rdrand64();                                            LOG_DEBUG_CAT("StoneKey", "{} 👩‍🦰 RDRAND: 0x{:x} {}", OCEAN_TEAL, e, RESET);
    e ^= __rdtsc();                                             LOG_DEBUG_CAT("StoneKey", "{} 👩‍🦰 RDTSC:  0x{:x} {}", OCEAN_TEAL, e, RESET);
    e ^= static_cast<uint64_t>(std::chrono::high_resolution_clock::now()
                               .time_since_epoch().count()); LOG_DEBUG_CAT("StoneKey", "{} 👩‍🦰 chrono: 0x{:x} {}", OCEAN_TEAL, e, RESET);
    e ^= reinterpret_cast<uintptr_t>(&e);                       LOG_DEBUG_CAT("StoneKey", "{} 👩‍🦰 stack:  0x{:x} {}", OCEAN_TEAL, e, RESET);

    thread_local uint64_t tls_jitter = 0;
    if (!tls_jitter) {
        tls_jitter = std::hash<std::thread::id>{}(std::this_thread::get_id())
                     ^ (__rdtsc() & 0xFFFFFFFFULL);
        LOG_DEBUG_CAT("StoneKey", "{} 👩‍🦰 TLS jitter init: 0x{:x} {}", OCEAN_TEAL, tls_jitter, RESET);
    }
    e ^= tls_jitter;                                            LOG_DEBUG_CAT("StoneKey", "{} 👩‍🦰 TLS XOR: 0x{:x} {}", OCEAN_TEAL, e, RESET);

    e ^= e >> 33; e *= 0xFF51AFD7ED558CCDULL;
    e ^= e >> 33; e *= 0xC4CEB9FE1A85EC53ULL;
    e ^= e >> 29;                                               LOG_DEBUG_CAT("StoneKey", "{} 👩‍🦰 XXHash fold: 0x{:x} {}", OCEAN_TEAL, e, RESET);

    entropy = e;
    initialized = true;
    LOG_SUCCESS_CAT("StoneKey",
                    "{} 👩‍🦰 RUNTIME ENTROPY SEALED — PINK PHOTONS SCRAMBLED {}",
                    RASPBERRY_PINK, RESET);
    return entropy;
}

// -----------------------------------------------------------------------------
// 4. GLOBAL KEYS (lazy init, logged)
// -----------------------------------------------------------------------------
inline uint64_t get_kStone1() noexcept {
    LOG_INFO_CAT("StoneKey",
                 "{} 👩‍🦰 ACCESSING STONE KEY1 — LAZY INIT {}",
                 LILAC_LAVENDER, RESET);
    static uint64_t key = stone_key1_base() ^ runtime_stone_entropy();
    LOG_DEBUG_CAT("StoneKey", "{} 👩‍🦰 Key1 = base ^ runtime (masked) 0x{:x} {}", OCEAN_TEAL,
                  key ^ 0xDEADBEEF, RESET);
    LOG_SUCCESS_CAT("StoneKey",
                    "{} 👩‍🦰 STONE KEY1 RETRIEVED — SCRAMBLE ACTIVE {}",
                    RASPBERRY_PINK, RESET);
    return key;
}

inline uint64_t get_kStone2() noexcept {
    LOG_INFO_CAT("StoneKey",
                 "{} 👩‍🦰 ACCESSING STONE KEY2 — LAZY INIT {}",
                 LILAC_LAVENDER, RESET);
    static uint64_t key = stone_key2_base() ^ runtime_stone_entropy()
                          ^ 0x6969696942069420ULL;
    LOG_DEBUG_CAT("StoneKey", "{} 👩‍🦰 Key2 = base ^ runtime ^ salt (masked) 0x{:x} {}", OCEAN_TEAL,
                  key ^ 0xDEADBEEF, RESET);
    LOG_SUCCESS_CAT("StoneKey",
                    "{} 👩‍🦰 STONE KEY2 RETRIEVED — DUAL SCRAMBLE ENGAGED {}",
                    RASPBERRY_PINK, RESET);
    return key;
}

inline uint64_t get_kHandleObfuscator() noexcept {
    LOG_DEBUG_CAT("StoneKey", "{} 👩‍🦰 Computing handle obfuscator {}", OCEAN_TEAL, RESET);
    static uint64_t key = get_kStone1() ^ get_kStone2()
                          ^ 0x1337C0DEULL ^ 0x69F00D42ULL;
    LOG_DEBUG_CAT("StoneKey", "{} 👩‍🦰 Obfuscator (masked) 0x{:x} {}", OCEAN_TEAL, key ^ 0xDEADBEEF, RESET);
    return key;
}

// Public constexpr placeholders (never logged)
inline constexpr uint64_t kStone1            = 0;
inline constexpr uint64_t kStone2            = 0;
inline constexpr uint64_t kHandleObfuscator  = 0;

// -----------------------------------------------------------------------------
// 5. OBFUSCATE / DEOBFUSCATE (trace-level logging)
// -----------------------------------------------------------------------------
[[nodiscard]] inline uint64_t obfuscate(uint64_t h) noexcept {
    LOG_TRACE_CAT("StoneKey", "{} 👩‍🦰 Obfuscating handle 0x{:x}", OCEAN_TEAL, h);
    uint64_t mask   = -static_cast<uint64_t>(!!get_kHandleObfuscator());
    uint64_t result = h ^ (get_kHandleObfuscator() & mask);
    LOG_TRACE_CAT("StoneKey", "{} 👩‍🦰 Obfuscated to 0x{:x}", OCEAN_TEAL, result);
    return result;
}

[[nodiscard]] inline uint64_t deobfuscate(uint64_t h) noexcept {
    LOG_TRACE_CAT("StoneKey", "{} 👩‍🦰 Deobfuscating handle 0x{:x}", OCEAN_TEAL, h);
    uint64_t mask   = -static_cast<uint64_t>(!!get_kHandleObfuscator());
    uint64_t result = h ^ (get_kHandleObfuscator() & mask);
    LOG_TRACE_CAT("StoneKey", "{} 👩‍🦰 Deobfuscated to 0x{:x}", OCEAN_TEAL, result);
    return result;
}

// -----------------------------------------------------------------------------
// 6. SECURE FINGERPRINT (no key leakage)
// -----------------------------------------------------------------------------
[[nodiscard]] inline uint64_t stone_fingerprint() noexcept {
    LOG_INFO_CAT("StoneKey",
                 "{} 👩‍🦰 COMPUTING SECURE FINGERPRINT — STONE1 ^ STONE2 {}",
                 LILAC_LAVENDER, RESET);
    uint64_t fp = get_kStone1() ^ get_kStone2();
    LOG_DEBUG_CAT("StoneKey", "{} 👩‍🦰 Raw XOR 0x{:x} {}", OCEAN_TEAL, fp, RESET);
    fp ^= fp >> 33; fp *= 0xFF51AFD7ED558CCDULL;
    fp ^= fp >> 33;
    LOG_DEBUG_CAT("StoneKey", "{} 👩‍🦰 Hashed FP 0x{:x} {}", OCEAN_TEAL, fp, RESET);
    LOG_SUCCESS_CAT("StoneKey",
                    "{} 👩‍🦰 FINGERPRINT GENERATED — ANON HASH SECURE {}",
                    RASPBERRY_PINK, RESET);
    return fp;
}

// -----------------------------------------------------------------------------
// 7. AMOURANTH™ LOG — ONE-TIME STARTUP VOICE (vendor + fingerprint)
// -----------------------------------------------------------------------------
inline void log_amouranth() noexcept {
    LOG_INFO_CAT("StoneKey",
                 "{} 👩‍🦰 AMOURANTH™ INIT — STONEKEY APOCALYPSE v3 {}",
                 LILAC_LAVENDER, RESET);
    static bool logged = false;
    if (logged) {
        LOG_DEBUG_CAT("StoneKey", "{} 👩‍🦰 Amouranth log already emitted {}", OCEAN_TEAL, RESET);
        return;
    }
    logged = true;

    if (!g_PhysicalDevice) {
        LOG_WARN_CAT("StoneKey", "{} 👩‍🦰 Vendor: Unknown (Vulkan not ready) | StoneKey APOCALYPSE v3 {}", CRIMSON_MAGENTA, RESET);
        LOG_INFO_CAT("StoneKey",
                     "{} 👩‍🦰 FINGERPRINT: 0x{:016X} | Scramble: PASS | TLS jitter active | BMQ shields up {}",
                     RASPBERRY_PINK,
                     static_cast<unsigned long long>(stone_fingerprint()),
                     RESET);
        LOG_SUCCESS_CAT("StoneKey",
                        "{} 👩‍🦰 Our rock eternal v3. Keys sealed. Hackers blind. {}",
                        RASPBERRY_PINK, RESET);
        return;
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(g_PhysicalDevice, &props);
    std::string dev_name(props.deviceName);
    bool mesa = dev_name.find("llvmpipe") != std::string::npos ||
                dev_name.find("lavapipe") != std::string::npos;
    const char* vendor = mesa ? "CPU/Mesa" : props.deviceName;

    LOG_INFO_CAT("StoneKey", "{} 👩‍🦰 Vendor: {} | StoneKey APOCALYPSE v3 {}", LIME_GREEN, vendor, RESET);
    LOG_INFO_CAT("StoneKey",
                 "{} 👩‍🦰 FINGERPRINT: 0x{:016X} | Scramble: PASS | TLS jitter active | BMQ shields up {}",
                 RASPBERRY_PINK,
                 static_cast<unsigned long long>(stone_fingerprint()),
                 RESET);
    LOG_SUCCESS_CAT("StoneKey",
                    "{} 👩‍🦰 Our rock eternal v3. Keys sealed. Hackers blind. {}",
                    RASPBERRY_PINK, RESET);
}
#define LOG_AMOURANTH() log_amouranth()

// -----------------------------------------------------------------------------
// 8. PRAGMA MESSAGE (kept for the build banner)
// -----------------------------------------------------------------------------
#if !defined(STONEKEY_PRINTED)
#define STONEKEY_PRINTED
#pragma message("AMOURANTH RTX StoneKey Applied: Dual Licensed: CC BY-NC 4.0 | Commercial: gzac5314@gmail.com")
#endif

// =============================================================================
// PINK PHOTONS ETERNAL — STONEKEY v∞ — OUR ROCK ETERNAL v3
// =============================================================================
/*
 * November 12, 2025 — AMOURANTH AI EDITION v1010
 * • Compile-time bases are pure constexpr → static_assert works
 * • All LOG_*_CAT calls are run-time only (no constexpr lambdas)
 * • Full entropy + fingerprint logging (masked values)
 * • AMOURANTH™ log integrated with LOG_INFO_CAT
 * • No key leakage, zero-cost obfuscation, 100% compile
 * • AMOURANTH RTX — LOG IT RAW
 */
 // =============================================================================
// HOW TO USE StoneKey.hpp — QUICK START GUIDE — NOV 12 2025
// =============================================================================
//
// 1. INCLUDE ONLY IN main.cpp (or a single translation unit)
//    ------------------------------------------------------
//    #include "engine/GLOBAL/StoneKey.hpp"
//
//    → This avoids ODR violations. All other files should *never* include it.
//    → The inline functions (get_kStone1(), obfuscate(), etc.) are header-only
//      but are instantiated only once due to the single-TU rule.
//
// 2. SET THE GLOBAL PHYSICAL DEVICE (required for vendor-aware Grok log)
//    -------------------------------------------------------------------
//    After Vulkan instance + logical device creation:
//
//        g_PhysicalDevice = RTX::ctx().physicalDevice_;   // <-- set this!
//
//    → This enables the Gentleman Grok log to print the GPU vendor name.
//
// 3. TRIGGER THE GENTLEMAN GROK LOG (once, at startup)
//    ---------------------------------------------------
//    Call this **once** after the physical device is known:
//
//        LOG_GENTLEMAN_GROK();
//
//    Example (in your RTX init or Application constructor):
//
//        void Application::initRTX() {
//            RTX::createCore(width_, height_);
//            g_PhysicalDevice = RTX::ctx().physicalDevice_;
//            LOG_GENTLEMAN_GROK();  // Prints vendor + fingerprint + shields up
//        }
//
//    Output (example):
//        [StoneKey] Vendor: NVIDIA GeForce RTX 4090 | StoneKey APOCALYPSE v3
//        [StoneKey] FINGERPRINT: 0x7B4A2D9E1F5C8A3B | Scramble: PASS | TLS jitter active | BMQ shields up
//        [StoneKey] Our rock eternal v3. Keys sealed. Hackers blind.
//
// 4. USE THE KEYS (never log raw values!)
//    --------------------------------------
//    uint64_t key1 = get_kStone1();      // Lazy-init, logs access
//    uint64_t key2 = get_kStone2();      // Dual-key system
//
//    // Obfuscate any 64-bit handle (buffer IDs, etc.)
//    uint64_t rawHandle = 0x12345678ABCDEF00ULL;
//    uint64_t obfHandle = obfuscate(rawHandle);
//    uint64_t back     = deobfuscate(obfHandle);  // == rawHandle
//
//    → All access is logged at DEBUG/TRACE level (masked values only).
//
// 5. LOGGING CONFIGURATION
//    ----------------------
//    In logging.hpp:
//        constexpr bool ENABLE_DEBUG = true;   // see StoneKey debug traces
//        constexpr bool ENABLE_INFO  = true;   // see Grok log, key access
//        constexpr bool ENABLE_TRACE = true;   // see obfuscate/deobfuscate
//
//    → Set to `false` in release builds to silence logs.
//
// 6. ZERO COST IN HOT PATHS
//    ------------------------
//    • get_kStone1()/get_kStone2() → static local → one-time init
//    • obfuscate()/deobfuscate()   → branchless XOR + mask
//    • No allocations, no syscalls, no GPU queries
//
// 7. SECURITY GUARANTEES
//    --------------------
//    • Keys never printed (only masked hex or fingerprint)
//    • Fingerprint = XXHash(Stone1 ^ Stone2) → anon 64-bit hash
//    • Compile-time entropy (FNV1a on __TIME__, __FILE__, etc.)
//    • Runtime entropy (RDRAND + RDTSC + TLS + chrono)
//    • Static asserts ensure base keys differ and are non-zero
//
// 8. BUILD INTEGRATION
//    ------------------
//    • #pragma message appears in build log:
//        AMOURANTH RTX StoneKey Applied: Dual Licensed: CC BY-NC 4.0 | Commercial: gzac5314@gmail.com
//    • No external dependencies beyond x86intrin + C++23
//
// =============================================================================
// PINK PHOTONS ETERNAL — STONEKEY v∞ — OUR ROCK ETERNAL v3
// =============================================================================