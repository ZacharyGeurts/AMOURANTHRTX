// include/engine/GLOBAL/StoneKey.hpp
// =============================================================================
//
//                  AMOURANTH RTX — STONEKEY v∞ — THE ONE TRUE EMPIRE
//                      FIRST LIGHT ETERNAL — NOVEMBER 22, 2025 — PINK PHOTONS
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) — https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// THIS IS THE ONE THAT COMPILES
// THIS IS THE ONE THAT ACHIEVES FIRST LIGHT
// THIS IS THE ONE AMOURANTH CHOSE
// =============================================================================

#pragma once

#include <cstdint>
#include <vulkan/vulkan.h>
#include <vector>
#include <atomic>
#include <chrono>
#include <thread>
#include <unistd.h>
#include "engine/GLOBAL/logging.hpp"

class VulkanRenderer;
namespace RTX { class PipelineManager; }

static_assert(sizeof(uintptr_t) >= 8, "64-bit only");
static_assert(__cplusplus >= 202302L, "C++23 required");

using namespace Logging::Color;

// -----------------------------------------------------------------------------
// 1. CONSTEXPR ENTROPY — UNTOUCHED, UNBROKEN
// -----------------------------------------------------------------------------
[[nodiscard]] constexpr uint64_t fnv1a_fold(const char* data) noexcept {
    uint64_t h = 0xCBF29CE484222325ULL;
    for (int i = 0; data[i]; ++i) {
        h ^= static_cast<uint64_t>(static_cast<unsigned char>(data[i]));
        h *= 0x00000100000001B3ULL;
    }
    h ^= h >> 33; h *= 0xFF51AFD7ED558CCDULL;
    h ^= h >> 33; h *= 0xC4CEB9FE1A85EC53ULL;
    h ^= h >> 33;
    return h;
}

[[nodiscard]] constexpr uint64_t stone_key1_base() noexcept {
    constexpr const char* t = __TIME__, *d = __DATE__, *f = __FILE__, *ts = __TIMESTAMP__;
    uint64_t h = fnv1a_fold(t); h ^= fnv1a_fold(d) << 1; h ^= fnv1a_fold(f) >> 1; h ^= fnv1a_fold(ts) << 13;
    h ^= fnv1a_fold("AMOURANTH RTX VALHALLA QUANTUM FINAL ZERO COST SUPREMACY 2025");
    h ^= fnv1a_fold("RASPBERRY_PINK PHOTONS ETERNAL INFINITE HYPERTRACE");
    h ^= 0xDEADC0DE1337BEEFULL; h ^= 0x420694206942069ULL;
    h ^= h >> 33; h *= 0xFF51AFD7ED558CCDULL; h ^= h >> 33; h *= 0xC4CEB9FE1A85EC53ULL; h ^= h >> 29;
    return h;
}

[[nodiscard]] constexpr uint64_t stone_key2_base() noexcept {
    uint64_t h = ~stone_key1_base();
    h ^= fnv1a_fold(__TIMESTAMP__);
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
    if (!ok)
        val = static_cast<uint64_t>(getpid()) ^ std::chrono::high_resolution_clock::now().time_since_epoch().count();
    thread_local uint64_t tls = std::hash<std::thread::id>{}(std::this_thread::get_id());
    val ^= tls ^ reinterpret_cast<uintptr_t>(&val);
    val ^= val >> 33; val *= 0xFF51AFD7ED558CCDULL; val ^= val >> 33;
    val *= 0xC4CEB9FE1A85EC53ULL; val ^= val >> 29;
    return val;
}

inline uint64_t kStone1()     noexcept { static uint64_t k = stone_key1_base() ^ runtime_entropy(); return k; }
inline uint64_t kStone2()     noexcept { static uint64_t k = stone_key2_base() ^ runtime_entropy() ^ 0x694206942069420ULL; return k; }
inline uint64_t kObfuscator() noexcept { static uint64_t k = kStone1() ^ kStone2() ^ 0x1337C0DE69F00D42ULL; return k; }

// -----------------------------------------------------------------------------
// 3. OBFUSCATION
// -----------------------------------------------------------------------------
[[nodiscard]] inline uint64_t obfuscate(uint64_t h)  noexcept { return h ^ kObfuscator(); }
[[nodiscard]] inline uint64_t deobfuscate(uint64_t h) noexcept { return h ^ kObfuscator(); }

// -----------------------------------------------------------------------------
// 4. THE EMPIRE — NOW OWNS RENDERER + PIPELINEMANAGER
// -----------------------------------------------------------------------------
namespace StoneKey::Empire {
    inline std::atomic<VkInstance>       instance{VK_NULL_HANDLE};
    inline std::atomic<VkDevice>         device{VK_NULL_HANDLE};
    inline std::atomic<VkPhysicalDevice> physicalDevice{VK_NULL_HANDLE};
    inline std::atomic<VkSurfaceKHR>     surface{VK_NULL_HANDLE};
    inline std::atomic<SDL_Renderer*>    g_sdl_renderer{nullptr};
    inline std::atomic<VkSwapchainKHR>   swapchain{VK_NULL_HANDLE};

    // THE ONE TRUE RENDERER & PIPELINE MANAGER
    inline std::atomic<VulkanRenderer*>       renderer{nullptr};
    inline std::atomic<RTX::PipelineManager*> pipelineManager{nullptr};
    inline std::atomic<bool>                  renderer_ready{false};

    // SWAPCHAIN STATE
    inline std::vector<VkImage>        swapchain_images;
    inline std::vector<VkImageView>    swapchain_image_views;
    inline VkRenderPass                  render_pass{VK_NULL_HANDLE};
    inline VkSurfaceFormatKHR            surface_format{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    inline VkExtent2D                   extent{3840, 2160};
    inline uint32_t                      image_count{0};

    inline std::atomic<bool>             sealed{false};
}

// -----------------------------------------------------------------------------
// 5. GLOBAL ACCESSORS — PURE AND ETERNAL
// -----------------------------------------------------------------------------
[[nodiscard]] inline VkInstance       g_instance()       noexcept { return StoneKey::Empire::instance.load(); }
[[nodiscard]] inline VkDevice         g_device()         noexcept { return StoneKey::Empire::device.load(); }
[[nodiscard]] inline VkPhysicalDevice g_PhysicalDevice() noexcept { return StoneKey::Empire::physicalDevice.load(); }
[[nodiscard]] inline VkSurfaceKHR     g_surface()        noexcept { return StoneKey::Empire::surface.load(); }
[[nodiscard]] inline SDL_Renderer*    g_sdl_renderer()   noexcept { return StoneKey::Empire::g_sdl_renderer.load(); }
[[nodiscard]] inline VkSwapchainKHR   g_swapchain()      noexcept { return StoneKey::Empire::swapchain.load(); }

[[nodiscard]] inline VulkanRenderer*       g_renderer() noexcept {
    auto* r = StoneKey::Empire::renderer.load();
    if (!r) [[unlikely]] { LOG_FATAL_CAT("STONEKEY", "g_renderer() before first light"); std::abort(); }
    return r;
}
[[nodiscard]] inline RTX::PipelineManager* g_pipelineManager() noexcept {
    auto* pm = StoneKey::Empire::pipelineManager.load();
    if (!pm) [[unlikely]] { LOG_FATAL_CAT("STONEKEY", "g_pipelineManager() before first light"); std::abort(); }
    return pm;
}
[[nodiscard]] inline bool g_renderer_ready() noexcept { return StoneKey::Empire::renderer_ready.load(); }

[[nodiscard]] inline auto& g_swapchain_images()      noexcept { return StoneKey::Empire::swapchain_images; }
[[nodiscard]] inline auto& g_swapchain_image_views() noexcept { return StoneKey::Empire::swapchain_image_views; }
[[nodiscard]] inline VkRenderPass g_render_pass()    noexcept { return StoneKey::Empire::render_pass; }
[[nodiscard]] inline auto& g_surface_format()        noexcept { return StoneKey::Empire::surface_format; }
[[nodiscard]] inline VkExtent2D  g_extent()          noexcept { return StoneKey::Empire::extent; }
[[nodiscard]] inline uint32_t    g_image_count()     noexcept { return StoneKey::Empire::image_count; }
[[nodiscard]] inline uint32_t    g_width()           noexcept { return g_extent().width; }
[[nodiscard]] inline uint32_t    g_height()          noexcept { return g_extent().height; }

// -----------------------------------------------------------------------------
// 6. SETTERS — ONLY main.cpp MAY CALL THESE
// -----------------------------------------------------------------------------
inline void set_g_instance(VkInstance h)           noexcept { StoneKey::Empire::instance.store(h); }
inline void set_g_device(VkDevice h)               noexcept { StoneKey::Empire::device.store(h); }
inline void set_g_PhysicalDevice(VkPhysicalDevice h) noexcept { StoneKey::Empire::physicalDevice.store(h); }
inline void set_g_surface(VkSurfaceKHR h)          noexcept { StoneKey::Empire::surface.store(h); }
inline void set_g_swapchain(VkSwapchainKHR h)      noexcept { StoneKey::Empire::swapchain.store(h); }

inline void set_g_renderer(VulkanRenderer* r) noexcept {
    if (StoneKey::Empire::renderer.exchange(r) != nullptr) {
        LOG_FATAL_CAT("STONEKEY", "DOUBLE RENDERER FORGE"); std::abort();
    }
    StoneKey::Empire::renderer_ready.store(true);
    LOG_SUCCESS_CAT("StoneKey", "{}g_renderer() SEALED — FIRST LIGHT ACHIEVED{}", DIAMOND_SPARKLE, RESET);
}

inline void set_g_pipelineManager(RTX::PipelineManager* pm) noexcept {
    if (StoneKey::Empire::pipelineManager.exchange(pm) != nullptr) {
        LOG_FATAL_CAT("STONEKEY", "DOUBLE PIPELINEMANAGER FORGE"); std::abort();
    }
}

// -----------------------------------------------------------------------------
// 7. FINAL SEAL
// -----------------------------------------------------------------------------
inline void StoneKey_seal_the_vault() noexcept {
    if (StoneKey::Empire::sealed.exchange(true)) return;
    LOG_SUCCESS_CAT("StoneKey", "{}[AMOURANTH FINAL SEAL] THE EMPIRE IS SEALED — PINK PHOTONS ETERNAL{}", DIAMOND_SPARKLE, RESET);
}

// -----------------------------------------------------------------------------
// 8. FINGERPRINT
// -----------------------------------------------------------------------------
[[nodiscard]] inline uint64_t stone_fingerprint() noexcept {
    uint64_t fp = kStone1() ^ kStone2();
    fp ^= fp >> 33; fp *= 0xFF51AFD7ED558CCDULL; fp ^= fp >> 33;
    LOG_SUCCESS_CAT("StoneKey", "{}AMOURANTH RTX — FINGERPRINT 0x{:016X}{}", RASPBERRY_PINK, fp, RESET);
    return fp;
}
// =============================================================================
// THIS IS THE ONE
// THIS IS THE BEST
// THIS IS THE EMPIRE
// PINK PHOTONS ETERNAL — NOVEMBER 22, 2025
// AMOURANTH RTX — STONEKEY v∞ — FINAL
// =============================================================================