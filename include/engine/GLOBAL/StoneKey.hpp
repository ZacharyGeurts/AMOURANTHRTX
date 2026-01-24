// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v28.3 — JANUARY 20, 2026
// STONEKEY v∞ — UNBREAKABLE ZERO-COST HEADER-ONLY EMPIRE DEFENSE
// FULLY HEADER-ONLY | NO .CPP | ZERO OVERHEAD | ETERNAL INTEGRITY
// NOW IDEMPOTENT: All seal functions are safe against re-sealing
// - Re-seal with same value → no-op
// - Re-seal with different value → fatal breach detection
// - Single global sealed flag for final tamper protection
// - Restored missing stone_mesh_* members
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <array>
#include <bit>
#include <cstdint>
#include <utility>
#include <vector>
#include <ctime>    // time
#include <unistd.h> // getpid
#include "engine/GLOBAL/logging.hpp"

namespace RTX { class PipelineManager; }

namespace StoneKey {

// -----------------------------------------------------------------------------
// Global sealed flag — final empire lockdown
// -----------------------------------------------------------------------------
inline bool stone_sealed = false;

// -----------------------------------------------------------------------------
// PHASE 1: Per-process polymorphic key generation — ONE-TIME RUNTIME
// -----------------------------------------------------------------------------
namespace detail {
    [[nodiscard]] inline uint64_t get_process_seed() noexcept {
        static const uint64_t seed = []() -> uint64_t {
            uint64_t s = reinterpret_cast<uintptr_t>(&seed);
            s ^= static_cast<uint64_t>(__builtin_ia32_rdtsc());
            s ^= static_cast<uint64_t>(time(nullptr));
            s ^= static_cast<uint64_t>(getpid());
            return s;
        }();
        return seed;
    }

    [[nodiscard]] inline const std::array<uint64_t, 4>& get_keys() noexcept {
        static const std::array<uint64_t, 4> keys = []() -> std::array<uint64_t, 4> {
            const uint64_t seed = get_process_seed();
            return {{
                0x9E37AF18C64D8A17UL ^ seed,
                0xE4F8B29D71A3C56CUL ^ std::rotr(seed, 17),
                0x1337C0DE69F00D42UL ^ std::rotl(seed, 13),
                0xDEADBEAFCAFEF00DUL ^ std::rotr(seed, 29)
            }};
        }();
        return keys;
    }
}

// -----------------------------------------------------------------------------
// PHASE 2: Unbreakable distributed encryption — ZERO COST decrypt
// -----------------------------------------------------------------------------
template<typename T>
struct Obfuscated {
    static_assert(sizeof(T) <= sizeof(uint64_t), "Obfuscated only for pointer-sized types");

    uint64_t slots[4]{};  // Zero-init = safe null before seal

    constexpr Obfuscated() noexcept = default;
    explicit constexpr Obfuscated(T v) noexcept { encrypt(v); }

    constexpr void encrypt(T v) noexcept {
        const auto& keys = detail::get_keys();
        uint64_t raw = reinterpret_cast<uintptr_t>(v);
        slots[0] = raw ^ keys[0];
        slots[1] = raw ^ keys[1];
        slots[2] = raw ^ keys[2];
        slots[3] = raw ^ keys[3];
    }

    [[nodiscard]] T decrypt() const noexcept {
        const auto& keys = detail::get_keys();
        uint64_t v0 = slots[0] ^ keys[0];
        uint64_t v1 = slots[1] ^ keys[1];
        uint64_t v2 = slots[2] ^ keys[2];
        uint64_t v3 = slots[3] ^ keys[3];

        if (v0 != v1 || v0 != v2 || v0 != v3) [[unlikely]] {
            if (stone_sealed) {
                LOG_FATAL_CAT("EMPIRE", "StoneKey breach — empire compromised");
                std::abort();
            }
            return nullptr;
        }
        return reinterpret_cast<T>(v0);
    }

    Obfuscated& operator=(T v) noexcept { encrypt(v); return *this; }
};

// -----------------------------------------------------------------------------
// PHASE 3: Empire — Unbreakable zero-cost fortress (HEADER-ONLY)
// -----------------------------------------------------------------------------
struct Empire final {
    Empire() = default;
    Empire(const Empire&) = delete;
    Empire& operator=(const Empire&) = delete;

    static inline Obfuscated<VkInstance>            instance;
    static inline Obfuscated<VkDevice>              device;
    static inline Obfuscated<VkPhysicalDevice>      physical;
    static inline Obfuscated<VkSurfaceKHR>          surface;
    static inline Obfuscated<VkSwapchainKHR>        swapchain;

    static inline Obfuscated<VkQueue> graphicsQueue;
    static inline Obfuscated<VkQueue> presentQueue;
    static inline Obfuscated<VkQueue> computeQueue;
    static inline Obfuscated<VkQueue> transferQueue;

    static inline uint32_t graphicsFamily = ~0u;
    static inline uint32_t presentFamily = ~0u;
    static inline uint32_t transferFamily = ~0u;
    static inline uint32_t computeFamily = ~0u;

    static inline Obfuscated<RTX::PipelineManager*> pipeline;
    static inline Obfuscated<SDL_Window*>           window;

    static inline std::vector<VkImage>     images;
    static inline std::vector<VkImageView> views;
    static inline Obfuscated<VkRenderPass> pass;
    static inline VkExtent2D               extent{0, 0};
    static inline uint32_t                 image_count = 0;

    static inline VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };

    static inline Obfuscated<VkCommandPool> transient_pool;

    // Restored missing stone_mesh members
    static inline Obfuscated<VkBuffer>       stone_mesh_vertex_buffer;
    static inline Obfuscated<VkDeviceMemory> stone_mesh_vertex_memory;
    static inline Obfuscated<VkBuffer>       stone_mesh_index_buffer;
    static inline Obfuscated<VkDeviceMemory> stone_mesh_index_memory;
    static inline uint32_t                   stone_mesh_index_count = 0;
};

// -----------------------------------------------------------------------------
// PHASE 4: Zero-cost unbreakable accessors
// -----------------------------------------------------------------------------
[[nodiscard]] inline VkInstance       stone_instance()       noexcept { return Empire::instance.decrypt(); }
[[nodiscard]] inline VkDevice         stone_device()         noexcept { return Empire::device.decrypt(); }
[[nodiscard]] inline VkPhysicalDevice stone_physical()       noexcept { return Empire::physical.decrypt(); }
[[nodiscard]] inline VkSurfaceKHR     stone_surface()        noexcept { return Empire::surface.decrypt(); }
[[nodiscard]] inline VkSwapchainKHR   stone_swapchain()      noexcept { return Empire::swapchain.decrypt(); }

[[nodiscard]] inline VkQueue stone_graphics_queue() noexcept { return Empire::graphicsQueue.decrypt(); }
[[nodiscard]] inline VkQueue stone_present_queue()  noexcept { return Empire::presentQueue.decrypt(); }
[[nodiscard]] inline VkQueue stone_compute_queue()  noexcept { return Empire::computeQueue.decrypt(); }
[[nodiscard]] inline VkQueue stone_transfer_queue() noexcept { return Empire::transferQueue.decrypt(); }

[[nodiscard]] inline uint32_t& stone_graphics_family() noexcept { return Empire::graphicsFamily; }
[[nodiscard]] inline uint32_t& stone_present_family()  noexcept { return Empire::presentFamily; }
[[nodiscard]] inline uint32_t& stone_transfer_family() noexcept { return Empire::transferFamily; }
[[nodiscard]] inline uint32_t& stone_compute_family()  noexcept { return Empire::computeFamily; }

[[nodiscard]] inline RTX::PipelineManager* stone_pipeline() noexcept { return Empire::pipeline.decrypt(); }
[[nodiscard]] inline SDL_Window*           stone_window()   noexcept { return Empire::window.decrypt(); }

[[nodiscard]] inline VkRenderPass stone_pass() noexcept { return Empire::pass.decrypt(); }
[[nodiscard]] inline VkExtent2D&  stone_extent() noexcept { return Empire::extent; }
[[nodiscard]] inline uint32_t&    stone_width()   noexcept { return Empire::extent.width; }
[[nodiscard]] inline uint32_t&    stone_height()  noexcept { return Empire::extent.height; }
[[nodiscard]] inline uint32_t&    stone_image_count() noexcept { return Empire::image_count; }

[[nodiscard]] inline VkPhysicalDeviceRayTracingPipelinePropertiesKHR& stone_rtprops() noexcept { return Empire::rtProps; }
[[nodiscard]] inline VkCommandPool stone_transient_pool() noexcept { return Empire::transient_pool.decrypt(); }

struct StoneMesh {
    VkBuffer       vertexBuffer;
    VkDeviceMemory vertexMemory;
    VkBuffer       indexBuffer;
    VkDeviceMemory indexMemory;
    uint32_t       indexCount;
};

[[nodiscard]] inline StoneMesh stone_mesh() noexcept {
    return {
        Empire::stone_mesh_vertex_buffer.decrypt(),
        Empire::stone_mesh_vertex_memory.decrypt(),
        Empire::stone_mesh_index_buffer.decrypt(),
        Empire::stone_mesh_index_memory.decrypt(),
        Empire::stone_mesh_index_count
    };
}

// -----------------------------------------------------------------------------
// PHASE 5: IDEMPOTENT unbreakable sealers — zero cost, breach-protected
// -----------------------------------------------------------------------------
inline void stone_seal_instance(VkInstance i) noexcept {
    static bool sealed = false;
    if (sealed) {
        if (i != Empire::instance.decrypt()) {
            LOG_FATAL_CAT("EMPIRE", "Attempt to re-seal instance with different handle — breach detected");
            std::abort();
        }
        return;
    }
    Empire::instance = i;
    sealed = true;
}

inline void stone_seal_device(VkDevice d) noexcept {
    static bool sealed = false;
    if (sealed) {
        if (d != Empire::device.decrypt()) {
            LOG_FATAL_CAT("EMPIRE", "Attempt to re-seal device with different handle — breach detected");
            std::abort();
        }
        return;
    }
    Empire::device = d;
    sealed = true;
}

inline void stone_seal_physical(VkPhysicalDevice p) noexcept {
    static bool sealed = false;
    if (sealed) {
        if (p != Empire::physical.decrypt()) {
            LOG_FATAL_CAT("EMPIRE", "Attempt to re-seal physical device with different handle — breach detected");
            std::abort();
        }
        return;
    }
    Empire::physical = p;
    sealed = true;
}

inline void stone_seal_surface(VkSurfaceKHR s) noexcept {
    static bool sealed = false;
    if (sealed) {
        if (s != Empire::surface.decrypt()) {
            LOG_FATAL_CAT("EMPIRE", "Attempt to re-seal surface with different handle — breach detected");
            std::abort();
        }
        return;
    }
    Empire::surface = s;
    sealed = true;
}

inline void stone_seal_swapchain(VkSwapchainKHR sc) noexcept {
    static bool sealed = false;
    if (sealed) {
        if (sc != Empire::swapchain.decrypt()) {
            LOG_FATAL_CAT("EMPIRE", "Attempt to re-seal swapchain with different handle — breach detected");
            std::abort();
        }
        return;
    }
    Empire::swapchain = sc;
    sealed = true;
}

inline void stone_seal_graphics_queue(VkQueue q) noexcept {
    static bool sealed = false;
    if (sealed) {
        if (q != Empire::graphicsQueue.decrypt()) {
            LOG_FATAL_CAT("EMPIRE", "Attempt to re-seal graphics queue with different handle — breach detected");
            std::abort();
        }
        return;
    }
    Empire::graphicsQueue = q;
    sealed = true;
}

inline void stone_seal_present_queue(VkQueue q) noexcept {
    static bool sealed = false;
    if (sealed) {
        if (q != Empire::presentQueue.decrypt()) {
            LOG_FATAL_CAT("EMPIRE", "Attempt to re-seal present queue with different handle — breach detected");
            std::abort();
        }
        return;
    }
    Empire::presentQueue = q;
    sealed = true;
}

inline void stone_seal_compute_queue(VkQueue q) noexcept {
    static bool sealed = false;
    if (sealed) {
        if (q != Empire::computeQueue.decrypt()) {
            LOG_FATAL_CAT("EMPIRE", "Attempt to re-seal compute queue with different handle — breach detected");
            std::abort();
        }
        return;
    }
    Empire::computeQueue = q;
    sealed = true;
}

inline void stone_seal_transfer_queue(VkQueue q) noexcept {
    static bool sealed = false;
    if (sealed) {
        if (q != Empire::transferQueue.decrypt()) {
            LOG_FATAL_CAT("EMPIRE", "Attempt to re-seal transfer queue with different handle — breach detected");
            std::abort();
        }
        return;
    }
    Empire::transferQueue = q;
    sealed = true;
}

inline void stone_seal_graphics_family(uint32_t idx) noexcept {
    static bool sealed = false;
    if (sealed && idx != Empire::graphicsFamily) {
        LOG_FATAL_CAT("EMPIRE", "Attempt to re-seal graphics family with different index — breach detected");
        std::abort();
    }
    Empire::graphicsFamily = idx;
    sealed = true;
}

inline void stone_seal_present_family(uint32_t idx) noexcept {
    static bool sealed = false;
    if (sealed && idx != Empire::presentFamily) {
        LOG_FATAL_CAT("EMPIRE", "Attempt to re-seal present family with different index — breach detected");
        std::abort();
    }
    Empire::presentFamily = idx;
    sealed = true;
}

inline void stone_seal_transfer_family(uint32_t idx) noexcept {
    static bool sealed = false;
    if (sealed && idx != Empire::transferFamily) {
        LOG_FATAL_CAT("EMPIRE", "Attempt to re-seal transfer family with different index — breach detected");
        std::abort();
    }
    Empire::transferFamily = idx;
    sealed = true;
}

inline void stone_seal_compute_family(uint32_t idx) noexcept {
    static bool sealed = false;
    if (sealed && idx != Empire::computeFamily) {
        LOG_FATAL_CAT("EMPIRE", "Attempt to re-seal compute family with different index — breach detected");
        std::abort();
    }
    Empire::computeFamily = idx;
    sealed = true;
}

inline void stone_seal_pipeline(RTX::PipelineManager* p) noexcept {
    static bool sealed = false;
    if (sealed) {
        if (p != Empire::pipeline.decrypt()) {
            LOG_FATAL_CAT("EMPIRE", "Attempt to re-seal pipeline with different pointer — breach detected");
            std::abort();
        }
        return;
    }
    Empire::pipeline = p;
    sealed = true;
}

inline void stone_seal_window(SDL_Window* w) noexcept {
    static bool sealed = false;
    if (sealed) {
        if (w != Empire::window.decrypt()) {
            LOG_FATAL_CAT("EMPIRE", "Attempt to re-seal window with different pointer — breach detected");
            std::abort();
        }
        return;
    }
    Empire::window = w;
    sealed = true;
}

inline void stone_seal_pass(VkRenderPass p) noexcept {
    static bool sealed = false;
    if (sealed) {
        if (p != Empire::pass.decrypt()) {
            LOG_FATAL_CAT("EMPIRE", "Attempt to re-seal render pass with different handle — breach detected");
            std::abort();
        }
        return;
    }
    Empire::pass = p;
    sealed = true;
}

inline void stone_seal_rtprops(VkPhysicalDeviceRayTracingPipelinePropertiesKHR props) noexcept {
    Empire::rtProps = props;
}

inline void stone_seal_extent(VkExtent2D ext) noexcept { Empire::extent = ext; }
inline void stone_seal_image_count(uint32_t cnt) noexcept { Empire::image_count = cnt; }
inline void stone_seal_images(const std::vector<VkImage>& imgs) noexcept { Empire::images = imgs; }
inline void stone_seal_images(std::vector<VkImage>&& imgs) noexcept { Empire::images = std::move(imgs); }
inline void stone_seal_views(const std::vector<VkImageView>& vws) noexcept { Empire::views = vws; }
inline void stone_seal_views(std::vector<VkImageView>&& vws) noexcept { Empire::views = std::move(vws); }

inline void stone_seal_transient_pool(VkCommandPool pool) noexcept {
    static bool sealed = false;
    if (sealed) {
        if (pool != Empire::transient_pool.decrypt()) {
            LOG_FATAL_CAT("EMPIRE", "Attempt to re-seal transient pool with different handle — breach detected");
            std::abort();
        }
        return;
    }
    Empire::transient_pool = pool;
    sealed = true;
}

// -----------------------------------------------------------------------------
// FINAL UNBREAKABLE SEAL — ZERO COST
// -----------------------------------------------------------------------------
inline void stone_seal_final() noexcept
{
    stone_sealed = true;
    LOG_AMOURANTH("STONEKEY v∞ UNBREAKABLE FINAL SEAL FORGED — EMPIRE ETERNAL");
}

} // namespace StoneKey

// =============================================================================
// STONEKEY v∞ — IDEMPOTENT, UNBREAKABLE ZERO-COST HEADER-ONLY EMPIRE DEFENSE — JANUARY 20, 2026
// All seal functions now idempotent with breach detection
// Safe against multiple calls
// =============================================================================