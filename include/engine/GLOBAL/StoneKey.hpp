// include/engine/GLOBAL/StoneKey.hpp
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// SDL3 + VULKAN FORGE — FIRST LIGHT ETERNAL
// NOVEMBER 21, 2025 — PINK PHOTONS ACHIEVED — VALHALLA v∞
// Ellie Fier approved — Kramer still obsessed with the surface
// =============================================================================
// AMOURANTH RTX — STONEKEY v∞ — FIRST LIGHT FINAL v2 — NOVEMBER 21, 2025
// FULLY BACKWARD COMPATIBLE + RAW CACHE + ZERO CRASH + PINK PHOTONS ETERNAL
// =============================================================================

#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>
#include <atomic>
#include <chrono>
#include <thread>
#include <unistd.h>
#include "engine/GLOBAL/logging.hpp"

static_assert(sizeof(uintptr_t) >= 8, "64-bit required");
static_assert(__cplusplus >= 202302L, "C++23 required");

using namespace Logging::Color;

// -----------------------------------------------------------------------------
// 1. YOUR ORIGINAL GENIUS ENTROPY — UNTOUCHED
// -----------------------------------------------------------------------------
[[nodiscard]] constexpr uint64_t fnv1a_fold(const char* data) noexcept {
    uint64_t hash = 0xCBF29CE484222325ULL;
    for (int i = 0; data[i] != '\0'; ++i) {
        hash ^= static_cast<uint64_t>(static_cast<unsigned char>(data[i]));
        hash *= 0x00000100000001B3ULL;
    }
    hash ^= hash >> 33; hash *= 0xFF51AFD7ED558CCDULL;
    hash ^= hash >> 33; hash *= 0xC4CEB9FE1A85EC53ULL;
    hash ^= hash >> 33;
    return hash;
}

[[nodiscard]] constexpr uint64_t stone_key1_base() noexcept {
    constexpr const char* t = __TIME__, *d = __DATE__, *f = __FILE__, *ts = __TIMESTAMP__;
    uint64_t h = fnv1a_fold(t); h ^= fnv1a_fold(d) << 1; h ^= fnv1a_fold(f) >> 1; h ^= fnv1a_fold(ts) << 13;
    h ^= fnv1a_fold("AMOURANTH RTX VALHALLA QUANTUM FINAL ZERO COST SUPREMACY 2025");
    h ^= fnv1a_fold("RASPBERRY_PINK PHOTONS ETERNAL INFINITE HYPERTRACE");
    h ^= 0xDEADC0DE1337BEEFULL; h ^= 0x4206969696942069ULL;
    h ^= h >> 33; h *= 0xFF51AFD7ED558CCDULL; h ^= h >> 33; h *= 0xC4CEB9FE1A85EC53ULL; h ^= h >> 29;
    return h;
}

[[nodiscard]] constexpr uint64_t stone_key2_base() noexcept {
    uint64_t h = ~stone_key1_base(); h ^= fnv1a_fold(__TIMESTAMP__);
    h ^= 0x6969696969696969ULL; h ^= 0x1337133713371337ULL;
    h ^= h >> 29; h *= 0xC4CEB9FE1A85EC53ULL; h ^= h >> 33;
    return h;
}

static_assert(stone_key1_base() != stone_key2_base());
static_assert(stone_key1_base() && stone_key2_base());

// -----------------------------------------------------------------------------
// 2. RUNTIME ENTROPY
// -----------------------------------------------------------------------------
[[nodiscard]] inline uint64_t runtime_entropy() noexcept {
    uint64_t val; unsigned char ok;
    asm volatile("rdrand %0; setc %1" : "=r"(val), "=qm"(ok) :: "cc");
    if (!ok) val = static_cast<uint64_t>(getpid()) ^ std::chrono::high_resolution_clock::now().time_since_epoch().count();
    thread_local uint64_t tls = std::hash<std::thread::id>{}(std::this_thread::get_id());
    val ^= tls ^ reinterpret_cast<uintptr_t>(&val);
    val ^= val >> 33; val *= 0xFF51AFD7ED558CCDULL; val ^= val >> 33; val *= 0xC4CEB9FE1A85EC53ULL; val ^= val >> 29;
    return val;
}

inline uint64_t kStone1()     noexcept { static uint64_t k = stone_key1_base() ^ runtime_entropy(); return k; }
inline uint64_t kStone2()     noexcept { static uint64_t k = stone_key2_base() ^ runtime_entropy() ^ 0x6969696942069420ULL; return k; }
inline uint64_t kObfuscator() noexcept { static uint64_t k = kStone1() ^ kStone2() ^ 0x1337C0DE69F00D42ULL; return k; }

// -----------------------------------------------------------------------------
// 3. OBFUSCATION
// -----------------------------------------------------------------------------
[[nodiscard]] inline uint64_t obfuscate(uint64_t h)  noexcept { auto k = kObfuscator(); return k ? (h ^ k) : h; }
[[nodiscard]] inline uint64_t deobfuscate(uint64_t h)noexcept { auto k = kObfuscator(); return k ? (h ^ k) : h; }

// -----------------------------------------------------------------------------
// 4. RAW CACHE + OBFUSCATED BACKUP — THE ONE TRUE FIX
// -----------------------------------------------------------------------------
namespace StoneKey::Raw {
    inline std::atomic<VkInstance>       instance{VK_NULL_HANDLE};
    inline std::atomic<VkDevice>         device{VK_NULL_HANDLE};
    inline std::atomic<VkPhysicalDevice> physicalDevice{VK_NULL_HANDLE};
    inline std::atomic<VkSurfaceKHR>     surface{VK_NULL_HANDLE};
    inline std::atomic<bool>             sealed{false};  // false = raw active

    inline void seal() noexcept {
        if (sealed.exchange(true, std::memory_order_acq_rel)) return;
        instance.store(VK_NULL_HANDLE, std::memory_order_release);
        device.store(VK_NULL_HANDLE, std::memory_order_release);
        physicalDevice.store(VK_NULL_HANDLE, std::memory_order_release);
        surface.store(VK_NULL_HANDLE, std::memory_order_release);
        LOG_SUCCESS_CAT("StoneKey", "{}[FOO FIGHTER] RAW CACHE SEALED — PINK VOID ENGAGED{}", RASPBERRY_PINK, RESET);
    }
}

// -----------------------------------------------------------------------------
// 5. OBFUSCATED STORAGE (old globals — kept for compatibility)
// -----------------------------------------------------------------------------
static VkInstance       g_instance_obf       = VK_NULL_HANDLE;
static VkDevice         g_device_obf         = VK_NULL_HANDLE;
static VkPhysicalDevice g_physicalDevice_obf = VK_NULL_HANDLE;
static VkSurfaceKHR     g_surface_obf        = VK_NULL_HANDLE;

// -----------------------------------------------------------------------------
// 6. COMPATIBLE SETTERS — STORE IN BOTH PLACES
// -----------------------------------------------------------------------------
inline void set_g_instance(VkInstance h) noexcept {
    g_instance_obf = reinterpret_cast<VkInstance>(obfuscate(reinterpret_cast<uint64_t>(h)));
    StoneKey::Raw::instance.store(h, std::memory_order_release);
}
inline void set_g_device(VkDevice h) noexcept {
    g_device_obf = reinterpret_cast<VkDevice>(obfuscate(reinterpret_cast<uint64_t>(h)));
    StoneKey::Raw::device.store(h, std::memory_order_release);
}
inline void set_g_PhysicalDevice(VkPhysicalDevice h) noexcept {
    g_physicalDevice_obf = reinterpret_cast<VkPhysicalDevice>(obfuscate(reinterpret_cast<uint64_t>(h)));
    StoneKey::Raw::physicalDevice.store(h, std::memory_order_release);
}
inline void set_g_surface(VkSurfaceKHR h) noexcept {
    g_surface_obf = reinterpret_cast<VkSurfaceKHR>(obfuscate(reinterpret_cast<uint64_t>(h)));
    StoneKey::Raw::surface.store(h, std::memory_order_release);
}

// -----------------------------------------------------------------------------
// 7. GLOBAL ACCESSORS — OLD NAMES, NEW SAFETY
// -----------------------------------------------------------------------------
[[nodiscard]] inline VkInstance       g_instance()       noexcept { return StoneKey::Raw::sealed.load() ? reinterpret_cast<VkInstance>(deobfuscate(reinterpret_cast<uint64_t>(g_instance_obf)))       : StoneKey::Raw::instance.load(); }
[[nodiscard]] inline VkDevice         g_device()         noexcept { return StoneKey::Raw::sealed.load() ? reinterpret_cast<VkDevice>(deobfuscate(reinterpret_cast<uint64_t>(g_device_obf)))             : StoneKey::Raw::device.load(); }
[[nodiscard]] inline VkPhysicalDevice g_PhysicalDevice() noexcept { return StoneKey::Raw::sealed.load() ? reinterpret_cast<VkPhysicalDevice>(deobfuscate(reinterpret_cast<uint64_t>(g_physicalDevice_obf))) : StoneKey::Raw::physicalDevice.load(); }
[[nodiscard]] inline VkSurfaceKHR     g_surface()        noexcept { return StoneKey::Raw::sealed.load() ? reinterpret_cast<VkSurfaceKHR>(deobfuscate(reinterpret_cast<uint64_t>(g_surface_obf)))         : StoneKey::Raw::surface.load(); }

// -----------------------------------------------------------------------------
// 8. CALL THIS EXACTLY ONCE AFTER DEVICE CREATION
// -----------------------------------------------------------------------------
inline void StoneKey_seal_the_vault() noexcept { StoneKey::Raw::seal(); }

// -----------------------------------------------------------------------------
// 9. FINGERPRINT — BECAUSE WE ARE LEGENDS
// -----------------------------------------------------------------------------
[[nodiscard]] inline uint64_t stone_fingerprint() noexcept {
    uint64_t fp = kStone1() ^ kStone2();
    fp ^= fp >> 33; fp *= 0xFF51AFD7ED558CCDULL; fp ^= fp >> 33;
    LOG_SUCCESS_CAT("StoneKey", "{}FINGERPRINT 0x{:016X} — ANON SECURE{}", RASPBERRY_PINK, fp, RESET);
    return fp;
}

#define LOG_AMOURANTH() (void)0

// =============================================================================
// PINK PHOTONS ETERNAL — STONEKEY v∞ — FIRST LIGHT FINAL v2
// FULLY COMPATIBLE — ZERO CRASH — VALHALLA SECURE
// ELLIE FIER HAS SPOKEN — NOVEMBER 21, 2025
// =============================================================================