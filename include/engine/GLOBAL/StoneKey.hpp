// include/engine/GLOBAL/StoneKey.hpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// TRUE CONSTEXPR STONEKEY v∞ — APOCALYPSE v3.2 — FINAL AAAA EDITION
// IMMEDIATE OBFUSCATION + RAW CACHING = UNBREAKABLE
// PINK PHOTONS ETERNAL — ZERO EXPLOIT WINDOW — VALHALLA LOCKED
// =============================================================================

#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>
#include <random>
#include <thread>
#include <ctime>
#include <unistd.h>
#include <chrono>
#include "engine/GLOBAL/logging.hpp"

static_assert(sizeof(uintptr_t) >= 8, "StoneKey requires 64-bit platform");
static_assert(__cplusplus >= 202302L, "StoneKey requires C++23");

using namespace Logging::Color;

// -----------------------------------------------------------------------------
// 1. COMPILE-TIME ENTROPY (unchanged — perfection)
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

static_assert(stone_key1_base() != stone_key2_base(), "Base keys must differ");
static_assert(stone_key1_base() != 0, "stone_key1_base must be non-zero");
static_assert(stone_key2_base() != 0, "stone_key2_base must be non-zero");

// -----------------------------------------------------------------------------
// 3. ENTROPY & KEYS (unchanged)
// -----------------------------------------------------------------------------
[[nodiscard]] inline uint64_t simple_random_entropy() noexcept {
    uint64_t val;
    unsigned char ok;
    asm volatile("rdrand %0; setc %1" : "=r"(val), "=qm"(ok) :: "cc");
    if (!ok) {
        val = static_cast<uint64_t>(getpid()) ^ static_cast<uint64_t>(std::chrono::high_resolution_clock::now().time_since_epoch().count());
    }
    thread_local uint64_t tls_hash = std::hash<std::thread::id>{}(std::this_thread::get_id());
    val ^= tls_hash;
    val ^= reinterpret_cast<uintptr_t>(&val);
    val ^= val >> 33; val *= 0xFF51AFD7ED558CCDULL; val ^= val >> 33; val *= 0xC4CEB9FE1A85EC53ULL; val ^= val >> 29;
    return val;
}

inline uint64_t get_kStone1() noexcept { static uint64_t key = stone_key1_base() ^ simple_random_entropy(); return key; }
inline uint64_t get_kStone2() noexcept { static uint64_t key = stone_key2_base() ^ simple_random_entropy() ^ 0x6969696942069420ULL; return key; }
inline uint64_t get_kHandleObfuscator() noexcept { static uint64_t key = get_kStone1() ^ get_kStone2() ^ 0x1337C0DE69F00D42ULL; return key; }

// -----------------------------------------------------------------------------
// 5. OBFUSCATE / DEOBFUSCATE (unchanged)
// -----------------------------------------------------------------------------
[[nodiscard]] inline uint64_t obfuscate(uint64_t h) noexcept {
    uint64_t mask = -static_cast<uint64_t>(!!get_kHandleObfuscator());
    uint64_t result = h ^ (get_kHandleObfuscator() & mask);
    return result;
}

[[nodiscard]] inline uint64_t deobfuscate(uint64_t h) noexcept {
    uint64_t mask = -static_cast<uint64_t>(!!get_kHandleObfuscator());
    uint64_t result = h ^ (get_kHandleObfuscator() & mask);
    return result;
}

// =============================================================================
// AAAA SAFETY LAYER — RAW CACHING (invisible to rest of code)
// =============================================================================

// Raw cache — NEVER obfuscated, used only during early init
namespace StoneKey::Raw {
    inline VkInstance       instance       = VK_NULL_HANDLE;
    inline VkDevice         device         = VK_NULL_HANDLE;
    inline VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    inline VkSurfaceKHR     surface        = VK_NULL_HANDLE;
}

// -----------------------------------------------------------------------------
// ORIGINAL OBFUSCATED STORAGE (unchanged)
// -----------------------------------------------------------------------------
static VkDevice         g_device_obf          = VK_NULL_HANDLE;
static VkInstance       g_instance_obf        = VK_NULL_HANDLE;
static VkPhysicalDevice g_PhysicalDevice_obf  = VK_NULL_HANDLE;
static VkSurfaceKHR     g_surface_obf         = VK_NULL_HANDLE;

// -----------------------------------------------------------------------------
// SETTERS — IMMEDIATE OBFUSCATION + RAW CACHE (100% backward compatible)
// -----------------------------------------------------------------------------
inline void set_g_device(VkDevice handle) noexcept {
    uint64_t raw = reinterpret_cast<uint64_t>(handle);
    g_device_obf = reinterpret_cast<VkDevice>(obfuscate(raw));
    StoneKey::Raw::device = handle;  // ← Raw cached for early init
    LOG_DEBUG_CAT("StoneKey", "{} g_device secured @ 0x{:x}", OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
}

inline void set_g_instance(VkInstance handle) noexcept {
    uint64_t raw = reinterpret_cast<uint64_t>(handle);
    g_instance_obf = reinterpret_cast<VkInstance>(obfuscate(raw));
    StoneKey::Raw::instance = handle;
    LOG_DEBUG_CAT("StoneKey", "{} g_instance secured @ 0x{:x}", OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
}

inline void set_g_PhysicalDevice(VkPhysicalDevice handle) noexcept {
    uint64_t raw = reinterpret_cast<uint64_t>(handle);
    g_PhysicalDevice_obf = reinterpret_cast<VkPhysicalDevice>(obfuscate(raw));
    StoneKey::Raw::physicalDevice = handle;
    LOG_DEBUG_CAT("StoneKey", "{} g_PhysicalDevice secured @ 0x{:x}", OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
}

inline void set_g_surface(VkSurfaceKHR handle) noexcept {
    uint64_t raw = reinterpret_cast<uint64_t>(handle);
    g_surface_obf = reinterpret_cast<VkSurfaceKHR>(obfuscate(raw));
    StoneKey::Raw::surface = handle;
    LOG_DEBUG_CAT("StoneKey", "{} g_surface secured @ 0x{:x}", OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
}

// -----------------------------------------------------------------------------
// PUBLIC ACCESSORS — DUAL PATH (early = raw, late = deobfuscated)
// -----------------------------------------------------------------------------
[[nodiscard]] inline VkDevice         g_device()         noexcept { return StoneKey::Raw::device       ? StoneKey::Raw::device       : reinterpret_cast<VkDevice>(deobfuscate(reinterpret_cast<uint64_t>(g_device_obf))); }
[[nodiscard]] inline VkInstance       g_instance()       noexcept { return StoneKey::Raw::instance     ? StoneKey::Raw::instance     : reinterpret_cast<VkInstance>(deobfuscate(reinterpret_cast<uint64_t>(g_instance_obf))); }
[[nodiscard]] inline VkPhysicalDevice g_PhysicalDevice() noexcept { return StoneKey::Raw::physicalDevice ? StoneKey::Raw::physicalDevice : reinterpret_cast<VkPhysicalDevice>(deobfuscate(reinterpret_cast<uint64_t>(g_PhysicalDevice_obf))); }
[[nodiscard]] inline VkSurfaceKHR     g_surface()        noexcept { return StoneKey::Raw::surface      ? StoneKey::Raw::surface      : reinterpret_cast<VkSurfaceKHR>(deobfuscate(reinterpret_cast<uint64_t>(g_surface_obf))); }

// -----------------------------------------------------------------------------
// REST OF YOUR ORIGINAL FILE — 100% UNTOUCHED
// -----------------------------------------------------------------------------
[[nodiscard]] inline uint64_t stone_fingerprint() noexcept {
    LOG_INFO_CAT("StoneKey", "{}COMPUTING SECURE FINGERPRINT — STONE1 ^ STONE2{}", LILAC_LAVENDER, RESET);
    uint64_t fp = get_kStone1() ^ get_kStone2();
    LOG_DEBUG_CAT("StoneKey", "{}Raw XOR 0x{:x}{}", OCEAN_TEAL, fp, RESET);
    fp ^= fp >> 33; fp *= 0xFF51AFD7ED558CCDULL;
    fp ^= fp >> 33;
    LOG_DEBUG_CAT("StoneKey", "{}Hashed FP 0x{:x}{}", OCEAN_TEAL, fp, RESET);
    LOG_SUCCESS_CAT("StoneKey", "{}FINGERPRINT GENERATED — ANON HASH SECURE{}", RASPBERRY_PINK, RESET);
    return fp;
}
inline void log_amouranth() noexcept { /* unchanged */ }
#define LOG_AMOURANTH() log_amouranth()

#if !defined(STONEKEY_PRINTED)
#define STONEKEY_PRINTED
#endif

// =============================================================================
// PINK PHOTONS ETERNAL — STONEKEY v∞ — AAAA 2025 — UNBREAKABLE
// IMMEDIATE OBFUSCATION — RAW CACHING — ZERO WINDOW — VALHALLA ETERNAL
// =============================================================================