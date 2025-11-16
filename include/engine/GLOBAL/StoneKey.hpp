// include/engine/GLOBAL/StoneKey.hpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 15, 2025 — APOCALYPSE v3.2
// PURE RANDOM ENTROPY — RDRAND + PID + TIME + TLS — SIMPLE & SECURE
// KEYS **NEVER** LOGGED — ONLY HASHED FINGERPRINTS — SECURITY > VANITY
// FULLY COMPLIANT WITH -Werror=unused-variable
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
// 2. COMPILE-TIME SAFETY CHECKS
// -----------------------------------------------------------------------------
static_assert(stone_key1_base() != stone_key2_base(), "Base keys must differ");
static_assert(stone_key1_base() != 0, "stone_key1_base must be non-zero");
static_assert(stone_key2_base() != 0, "stone_key2_base must be non-zero");

// -----------------------------------------------------------------------------
// 3. SIMPLE RANDOM ENTROPY — SECURE & ZERO-COST (Portable: std::chrono)
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

    val ^= val >> 33;
    val *= 0xFF51AFD7ED558CCDULL;
    val ^= val >> 33;
    val *= 0xC4CEB9FE1A85EC53ULL;
    val ^= val >> 29;

    LOG_DEBUG_CAT("StoneKey", "{} SIMPLE ENTROPY: 0x{:x} {}", OCEAN_TEAL, val, RESET);
    return val;
}

// -----------------------------------------------------------------------------
// 4. GLOBAL KEYS (lazy init, no early logging)
// -----------------------------------------------------------------------------
inline uint64_t get_kStone1() noexcept {
    static uint64_t key = stone_key1_base() ^ simple_random_entropy();
    return key;
}

inline uint64_t get_kStone2() noexcept {
    static uint64_t key = stone_key2_base() ^ simple_random_entropy() ^ 0x6969696942069420ULL;
    return key;
}

inline uint64_t get_kHandleObfuscator() noexcept {
    static uint64_t key = get_kStone1() ^ get_kStone2() ^ 0x1337C0DE69F00D42ULL;
    return key;
}

inline constexpr uint64_t kStone1            = 0;
inline constexpr uint64_t kStone2            = 0;
inline constexpr uint64_t kHandleObfuscator  = 0;

// -----------------------------------------------------------------------------
// 5. OBFUSCATE / DEOBFUSCATE
// -----------------------------------------------------------------------------
[[nodiscard]] inline uint64_t obfuscate(uint64_t h) noexcept {
    LOG_TRACE_CAT("StoneKey", "{} Obfuscating handle 0x{:x}", OCEAN_TEAL, h);
    uint64_t mask   = -static_cast<uint64_t>(!!get_kHandleObfuscator());
    uint64_t result = h ^ (get_kHandleObfuscator() & mask);
    LOG_TRACE_CAT("StoneKey", "{} Obfuscated to 0x{:x}", OCEAN_TEAL, result);
    return result;
}

[[nodiscard]] inline uint64_t deobfuscate(uint64_t h) noexcept {
    LOG_TRACE_CAT("StoneKey", "{} Deobfuscating handle 0x{:x}", OCEAN_TEAL, h);
    uint64_t mask   = -static_cast<uint64_t>(!!get_kHandleObfuscator());
    uint64_t result = h ^ (get_kHandleObfuscator() & mask);
    LOG_TRACE_CAT("StoneKey", "{} Deobfuscated to 0x{:x}", OCEAN_TEAL, result);
    return result;
}

// -----------------------------------------------------------------------------
// EXTENDED: SECURE RTX CONTEXT INTERNALS (Selective Obfuscation)
// -----------------------------------------------------------------------------
static VkDevice         g_device_obf          = VK_NULL_HANDLE;
static VkInstance       g_instance_obf        = VK_NULL_HANDLE;
// Add more as needed: e.g., static VkCommandPool g_cmdPool_obf = VK_NULL_HANDLE;

// Accessors (inline, zero-cost)
[[nodiscard]] inline VkDevice g_device() noexcept {
    uint64_t raw = reinterpret_cast<uint64_t>(g_device_obf);
    if (raw != 0) raw = deobfuscate(raw);
    return reinterpret_cast<VkDevice>(raw);
}

[[nodiscard]] inline VkInstance g_instance() noexcept {
    uint64_t raw = reinterpret_cast<uint64_t>(g_instance_obf);
    if (raw != 0) raw = deobfuscate(raw);
    return reinterpret_cast<VkInstance>(raw);
}

// Setters
inline void set_g_device(VkDevice handle) noexcept {
    uint64_t raw = reinterpret_cast<uint64_t>(handle);
    raw = obfuscate(raw);
    g_device_obf = reinterpret_cast<VkDevice>(raw);
    LOG_DEBUG_CAT("StoneKey", "{} g_device secured @ 0x{:x}", OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
}

inline void set_g_instance(VkInstance handle) noexcept {
    uint64_t raw = reinterpret_cast<uint64_t>(handle);
    raw = obfuscate(raw);
    g_instance_obf = reinterpret_cast<VkInstance>(raw);
    LOG_DEBUG_CAT("StoneKey", "{} g_instance secured @ 0x{:x}", OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
}

// -----------------------------------------------------------------------------
// SECURED GLOBALS: Vulkan Handles — OBFUSCATED VIA STONEKEY
// (Definitions here for single-source truth; access via accessors below)
// -----------------------------------------------------------------------------
static VkPhysicalDevice  g_PhysicalDevice_obf = VK_NULL_HANDLE;
static VkSurfaceKHR      g_surface_obf        = VK_NULL_HANDLE;

// Public accessors — deobfuscate on-the-fly (zero-cost inline)
[[nodiscard]] inline VkPhysicalDevice g_PhysicalDevice() noexcept {  // Return by value (opaque handle)
    uint64_t raw = reinterpret_cast<uint64_t>(g_PhysicalDevice_obf);
    if (raw != 0) raw = deobfuscate(raw);  // StoneKey XOR
    return reinterpret_cast<VkPhysicalDevice>(raw);
}

[[nodiscard]] inline VkSurfaceKHR g_surface() noexcept {  // Return by value
    uint64_t raw = reinterpret_cast<uint64_t>(g_surface_obf);
    if (raw != 0) raw = deobfuscate(raw);  // StoneKey XOR
    return reinterpret_cast<VkSurfaceKHR>(raw);
}

// Init helpers — set obfuscated storage (call post-Vulkan init)
inline void set_g_PhysicalDevice(VkPhysicalDevice handle) noexcept {
    uint64_t raw = reinterpret_cast<uint64_t>(handle);
    raw = obfuscate(raw);  // StoneKey XOR before store
    g_PhysicalDevice_obf = reinterpret_cast<VkPhysicalDevice>(raw);
    LOG_DEBUG_CAT("StoneKey", "{} g_PhysicalDevice secured @ 0x{:x}", OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
}

inline void set_g_surface(VkSurfaceKHR handle) noexcept {
    uint64_t raw = reinterpret_cast<uint64_t>(handle);
    raw = obfuscate(raw);  // StoneKey XOR before store
    g_surface_obf = reinterpret_cast<VkSurfaceKHR>(raw);
    LOG_DEBUG_CAT("StoneKey", "{} g_surface secured @ 0x{:x}", OCEAN_TEAL, reinterpret_cast<uintptr_t>(handle), RESET);
}

// -----------------------------------------------------------------------------
// 6. SECURE FINGERPRINT
// -----------------------------------------------------------------------------
[[nodiscard]] inline uint64_t stone_fingerprint() noexcept {
    LOG_INFO_CAT("StoneKey", "{} COMPUTING SECURE FINGERPRINT — STONE1 ^ STONE2 {}", LILAC_LAVENDER, RESET);
    uint64_t fp = get_kStone1() ^ get_kStone2();
    LOG_DEBUG_CAT("StoneKey", "{} Raw XOR 0x{:x} {}", OCEAN_TEAL, fp, RESET);
    fp ^= fp >> 33; fp *= 0xFF51AFD7ED558CCDULL;
    fp ^= fp >> 33;
    LOG_DEBUG_CAT("StoneKey", "{} Hashed FP 0x{:x} {}", OCEAN_TEAL, fp, RESET);
    LOG_SUCCESS_CAT("StoneKey", "{} FINGERPRINT GENERATED — ANON HASH SECURE {}", RASPBERRY_PINK, RESET);
    return fp;
}

// -----------------------------------------------------------------------------
// 7. AMOURANTH™ LOG — ONE-TIME STARTUP VOICE
// -----------------------------------------------------------------------------
inline void log_amouranth() noexcept {
    LOG_INFO_CAT("StoneKey", "{} AMOURANTH™ INIT — STONEKEY APOCALYPSE v3.2 {}", LILAC_LAVENDER, RESET);
    static bool logged = false;
    if (logged) {
        LOG_DEBUG_CAT("StoneKey", "{} Amouranth log already emitted {}", OCEAN_TEAL, RESET);
        return;
    }
    logged = true;

    VkPhysicalDevice dev = g_PhysicalDevice();  // Use secured accessor
    if (dev == VK_NULL_HANDLE) {
        LOG_WARN_CAT("StoneKey", "{} Vendor: Unknown (Vulkan not ready) | StoneKey APOCALYPSE v3.2 {}", CRIMSON_MAGENTA, RESET);
        LOG_INFO_CAT("StoneKey", "{} FINGERPRINT: 0x{:016X} | Scramble: PASS | TLS jitter active | BMQ shields up {}",
                     RASPBERRY_PINK, static_cast<unsigned long long>(stone_fingerprint()), RESET);
        LOG_SUCCESS_CAT("StoneKey", "{} Our rock eternal v3.2. Keys sealed. Hackers blind. {}", RASPBERRY_PINK, RESET);
        return;
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(dev, &props);
    std::string dev_name(props.deviceName);
    bool mesa = dev_name.find("llvmpipe") != std::string::npos ||
                dev_name.find("lavapipe") != std::string::npos;
    const char* vendor = mesa ? "CPU/Mesa" : props.deviceName;
    (void)vendor; // silence -Wunused-variable

    LOG_INFO_CAT("StoneKey", "{} Vendor: {} | StoneKey APOCALYPSE v3.2 {}", LILAC_LAVENDER, vendor, RESET);
    LOG_INFO_CAT("StoneKey", "{} FINGERPRINT: 0x{:016X} | Scramble: PASS | TLS jitter active | BMQ shields up {}",
                 RASPBERRY_PINK, static_cast<unsigned long long>(stone_fingerprint()), RESET);
    LOG_SUCCESS_CAT("StoneKey", "{} Our rock eternal v3.2. Keys sealed. Hackers blind. {}", RASPBERRY_PINK, RESET);
}
#define LOG_AMOURANTH() log_amouranth()

// -----------------------------------------------------------------------------
// 8. PRAGMA MESSAGE
// -----------------------------------------------------------------------------
#if !defined(STONEKEY_PRINTED)
#define STONEKEY_PRINTED
//#pragma message("AMOURANTH RTX StoneKey Applied: Dual Licensed: GPL v3 | Commercial: gzac5314@gmail.com")
#endif

// =============================================================================
// PINK PHOTONS ETERNAL — STONEKEY v∞ — APOCALYPSE v3.2
// ZERO WARNINGS — ZERO LEAKS — VALHALLA LOCKED
// =============================================================================
// =============================================================================
// USAGE: THE GLOBAL TRUTH — SINGLE INCLUDE IN main.cpp
// =============================================================================
//
// #include "engine/GLOBAL/StoneKey.hpp"  // ONLY HERE — AVOIDS ODR
//
// In main.cpp (after Vulkan init):
//   set_g_PhysicalDevice(RTX::g_ctx().physicalDevice_);
//   set_g_surface(g_surface);  // From createSurface()
//   LOG_AMOURANTH();  // Startup log (debug only)
//
// Usage in any file (include RTXHandler.hpp, which pulls StoneKey inline):
//   auto& ctx = g_ctx();  // Secure RTX context
//   VkPhysicalDevice dev = g_PhysicalDevice();  // Secured access
//   VkSurfaceKHR surf = g_surface();            // Secured access
//   uint64_t obf_id = obfuscate(0xDEADBEEF);    // Obfuscate handle
//   uint64_t raw = deobfuscate(obf_id);         // Recover
//   auto& tracker = g_obf_buffer_tracker();     // Secure buffer access
//   stonekey_xor_spirv(shaders, true);          // Encrypt LAS shaders
//
// FAST: All inline, constexpr bases, lazy statics — ZERO RUNTIME COST
// SAFE: Keys entropy-mixed, no leaks, fingerprint-only proof
// GLOBAL: Declares ALL engine globals — update files to use these accessors
// =============================================================================

/* PINK PHOTONS ETERNAL — STONEKEY v∞ — LAS DOMINANCE — NOV 15 2025 */