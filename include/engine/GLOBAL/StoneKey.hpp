// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — STONEKEY v30.25
// UNBREAKABLE ZERO-COST HEADER-ONLY EMPIRE DEFENSE — FULL ACCESS GRANTED
// FULLY HEADER-ONLY | NO .CPP | ZERO OVERHEAD | ETERNAL INTEGRITY
// - Single global sealed flag + per-category tamper checks
// - Centralized breach detection & abort
// - Obfuscated only for handles/pointers — plain ints for families/counts
// - One seal function per category — idempotent, safe re-seal
// - Mandatory stone_seal_final() after startup — final lockdown
// - Added operator= to Obfuscated<T> for raw assignment
// - No descriptor sets/pools — descriptor buffer empire only
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <utility>
#include <vector>
#include <ctime>    // time
#include <unistd.h> // getpid
#include "engine/GLOBAL/logging.hpp"

namespace RTX { class PipelineManager; }

namespace StoneKey {

// -----------------------------------------------------------------------------
// Global empire lockdown — final tamper protection
// -----------------------------------------------------------------------------
inline bool empire_sealed = false;

// -----------------------------------------------------------------------------
// Private empire state — only accessors exposed
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

    template<typename T>
    struct Obfuscated {
        static_assert(sizeof(T) <= sizeof(uint64_t), "Obfuscated only for pointer-sized types");

        uint64_t slots[4]{};

        constexpr Obfuscated() noexcept = default;
        explicit constexpr Obfuscated(T v) noexcept { encrypt(v); }

        constexpr void encrypt(T v) noexcept {
            const auto& keys = get_keys();
            uint64_t raw = reinterpret_cast<uintptr_t>(v);
            slots[0] = raw ^ keys[0];
            slots[1] = raw ^ keys[1];
            slots[2] = raw ^ keys[2];
            slots[3] = raw ^ keys[3];
        }

        [[nodiscard]] T decrypt() const noexcept {
            const auto& keys = get_keys();
            uint64_t v0 = slots[0] ^ keys[0];
            uint64_t v1 = slots[1] ^ keys[1];
            uint64_t v2 = slots[2] ^ keys[2];
            uint64_t v3 = slots[3] ^ keys[3];

            if (v0 != v1 || v0 != v2 || v0 != v3) [[unlikely]] { // if decrypt all zeroes
                if (empire_sealed) {
                    LOG_FATAL_CAT("EMPIRE", "StoneKey breach — early assessment");
                }
                return nullptr;
            }
            return reinterpret_cast<T>(v0);
        }

        // Added: allow direct assignment from raw T
        Obfuscated& operator=(T v) noexcept {
            encrypt(v);
            return *this;
        }
    };

    struct Empire {
        Obfuscated<VkInstance>            instance;
        Obfuscated<VkDevice>              device;
        Obfuscated<VkPhysicalDevice>      physical;
        Obfuscated<VkSurfaceKHR>          surface;
        Obfuscated<VkSwapchainKHR>        swapchain;

        Obfuscated<VkQueue> graphics_queue;
        Obfuscated<VkQueue> present_queue;
        Obfuscated<VkQueue> compute_queue;
        Obfuscated<VkQueue> transfer_queue;

        uint32_t graphics_family = ~0u;
        uint32_t present_family  = ~0u;
        uint32_t transfer_family = ~0u;
        uint32_t compute_family  = ~0u;

        Obfuscated<RTX::PipelineManager*> pipeline;
        Obfuscated<SDL_Window*>           window;

        std::vector<VkImage>     images;
        std::vector<VkImageView> views;
        Obfuscated<VkRenderPass> pass;
        VkExtent2D               extent{0, 0};
        uint32_t                 image_count = 0;

        VkPhysicalDeviceRayTracingPipelinePropertiesKHR rt_props{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
        };

        Obfuscated<VkCommandPool> transient_pool;

        Obfuscated<VkPipeline> compute_pipeline;
        Obfuscated<VkPipeline> rt_pipeline;
        Obfuscated<VkPipelineLayout> pipeline_layout;

        // Mesh members
        Obfuscated<VkBuffer>       mesh_vertex_buffer;
        Obfuscated<VkDeviceMemory> mesh_vertex_memory;
        Obfuscated<VkBuffer>       mesh_index_buffer;
        Obfuscated<VkDeviceMemory> mesh_index_memory;
        uint32_t                   mesh_index_count = 0;
    };

    inline Empire& empire() noexcept {
        static Empire e;
        return e;
    }
}

// -----------------------------------------------------------------------------
// Full, direct, zero-overhead access — unbreakable decrypt
// -----------------------------------------------------------------------------
[[nodiscard]] inline VkInstance       stone_instance()       noexcept { return detail::empire().instance.decrypt(); }
[[nodiscard]] inline VkDevice         stone_device()         noexcept { return detail::empire().device.decrypt(); }
[[nodiscard]] inline VkPhysicalDevice stone_physical()       noexcept { return detail::empire().physical.decrypt(); }
[[nodiscard]] inline VkSurfaceKHR     stone_surface()        noexcept { return detail::empire().surface.decrypt(); }
[[nodiscard]] inline VkSwapchainKHR   stone_swapchain()      noexcept { return detail::empire().swapchain.decrypt(); }

[[nodiscard]] inline VkQueue stone_graphics_queue() noexcept { return detail::empire().graphics_queue.decrypt(); }
[[nodiscard]] inline VkQueue stone_present_queue()  noexcept { return detail::empire().present_queue.decrypt(); }
[[nodiscard]] inline VkQueue stone_compute_queue()  noexcept { return detail::empire().compute_queue.decrypt(); }
[[nodiscard]] inline VkQueue stone_transfer_queue() noexcept { return detail::empire().transfer_queue.decrypt(); }

[[nodiscard]] inline uint32_t stone_graphics_family() noexcept { return detail::empire().graphics_family; }
[[nodiscard]] inline uint32_t stone_present_family()  noexcept { return detail::empire().present_family; }
[[nodiscard]] inline uint32_t stone_transfer_family() noexcept { return detail::empire().transfer_family; }
[[nodiscard]] inline uint32_t stone_compute_family()  noexcept { return detail::empire().compute_family; }

[[nodiscard]] inline RTX::PipelineManager* stone_pipeline() noexcept { return detail::empire().pipeline.decrypt(); }
[[nodiscard]] inline SDL_Window*           stone_window()   noexcept { return detail::empire().window.decrypt(); }

[[nodiscard]] inline VkRenderPass stone_pass() noexcept { return detail::empire().pass.decrypt(); }
[[nodiscard]] inline VkExtent2D&  stone_extent() noexcept { return detail::empire().extent; }
[[nodiscard]] inline uint32_t&    stone_image_count() noexcept { return detail::empire().image_count; }

[[nodiscard]] inline VkPhysicalDeviceRayTracingPipelinePropertiesKHR& stone_rtprops() noexcept { return detail::empire().rt_props; }
[[nodiscard]] inline VkCommandPool stone_transient_pool() noexcept { return detail::empire().transient_pool.decrypt(); }

[[nodiscard]] inline VkPipeline stone_compute_pipeline() noexcept { return detail::empire().compute_pipeline.decrypt(); }
[[nodiscard]] inline VkPipeline stone_rt_pipeline() noexcept { return detail::empire().rt_pipeline.decrypt(); }
[[nodiscard]] inline VkPipelineLayout stone_pipeline_layout() noexcept { return detail::empire().pipeline_layout.decrypt(); }

[[nodiscard]] inline VkBuffer       stone_mesh_vertex_buffer()   noexcept { return detail::empire().mesh_vertex_buffer.decrypt(); }
[[nodiscard]] inline VkDeviceMemory stone_mesh_vertex_memory()   noexcept { return detail::empire().mesh_vertex_memory.decrypt(); }
[[nodiscard]] inline VkBuffer       stone_mesh_index_buffer()    noexcept { return detail::empire().mesh_index_buffer.decrypt(); }
[[nodiscard]] inline VkDeviceMemory stone_mesh_index_memory()    noexcept { return detail::empire().mesh_index_memory.decrypt(); }
[[nodiscard]] inline uint32_t       stone_mesh_index_count()     noexcept { return detail::empire().mesh_index_count; }

// -----------------------------------------------------------------------------
// Centralized idempotent sealers — one per category, unbreakable
// -----------------------------------------------------------------------------
inline void stone_seal_device_resources(VkInstance i, VkDevice d, VkPhysicalDevice p,
                                        VkSurfaceKHR s, VkSwapchainKHR sc) noexcept {
    if (empire_sealed) {
        if (i != detail::empire().instance.decrypt() ||
            d != detail::empire().device.decrypt() ||
            p != detail::empire().physical.decrypt() ||
            s != detail::empire().surface.decrypt() ||
            sc != detail::empire().swapchain.decrypt()) {
            LOG_FATAL_CAT("EMPIRE", "Device resources tamper attempt — empire compromised");
            std::abort();
        }
        return;
    }

    detail::empire().instance = i;
    detail::empire().device = d;
    detail::empire().physical = p;
    detail::empire().surface = s;
    detail::empire().swapchain = sc;
}

inline void stone_seal_queues(VkQueue graphics, VkQueue present, VkQueue compute, VkQueue transfer) noexcept {
    if (empire_sealed) {
        if (graphics != detail::empire().graphics_queue.decrypt() ||
            present != detail::empire().present_queue.decrypt() ||
            compute != detail::empire().compute_queue.decrypt() ||
            transfer != detail::empire().transfer_queue.decrypt()) {
            LOG_FATAL_CAT("EMPIRE", "Queues tamper attempt — empire compromised");
            std::abort();
        }
        return;
    }

    detail::empire().graphics_queue = graphics;
    detail::empire().present_queue = present;
    detail::empire().compute_queue = compute;
    detail::empire().transfer_queue = transfer;
}

inline void stone_seal_families(uint32_t graphics, uint32_t present, uint32_t transfer, uint32_t compute) noexcept {
    if (empire_sealed) {
        if (graphics != detail::empire().graphics_family ||
            present != detail::empire().present_family ||
            transfer != detail::empire().transfer_family ||
            compute != detail::empire().compute_family) {
            LOG_FATAL_CAT("EMPIRE", "Families tamper attempt — empire compromised");
            std::abort();
        }
        return;
    }

    detail::empire().graphics_family = graphics;
    detail::empire().present_family = present;
    detail::empire().transfer_family = transfer;
    detail::empire().compute_family = compute;
}

inline void stone_seal_pipelines(RTX::PipelineManager* pipeline, VkPipeline compute, VkPipeline rt, VkPipelineLayout layout) noexcept {
    if (empire_sealed) {
        if (pipeline != detail::empire().pipeline.decrypt() ||
            compute != detail::empire().compute_pipeline.decrypt() ||
            rt != detail::empire().rt_pipeline.decrypt() ||
            layout != detail::empire().pipeline_layout.decrypt()) {
            LOG_FATAL_CAT("EMPIRE", "Pipelines tamper attempt — empire compromised");
            std::abort();
        }
        return;
    }

    detail::empire().pipeline = pipeline;
    detail::empire().compute_pipeline = compute;
    detail::empire().rt_pipeline = rt;
    detail::empire().pipeline_layout = layout;
}

inline void stone_seal_window_and_pass(SDL_Window* window, VkRenderPass pass) noexcept {
    if (empire_sealed) {
        if (window != detail::empire().window.decrypt() ||
            pass != detail::empire().pass.decrypt()) {
            LOG_FATAL_CAT("EMPIRE", "Window/pass tamper attempt — empire compromised");
            std::abort();
        }
        return;
    }

    detail::empire().window = window;
    detail::empire().pass = pass;
}

inline void stone_seal_swapchain_resources(const std::vector<VkImage>& images,
                                           const std::vector<VkImageView>& views,
                                           VkExtent2D extent,
                                           uint32_t image_count) noexcept {
    detail::empire().images = images;
    detail::empire().views = views;
    detail::empire().extent = extent;
    detail::empire().image_count = image_count;
}

inline void stone_seal_rtprops(const VkPhysicalDeviceRayTracingPipelinePropertiesKHR& props) noexcept {
    detail::empire().rt_props = props;
}

inline void stone_seal_mesh_buffers(VkBuffer vb, VkDeviceMemory vm, VkBuffer ib, VkDeviceMemory im, uint32_t ic) noexcept {
    detail::empire().mesh_vertex_buffer = vb;
    detail::empire().mesh_vertex_memory = vm;
    detail::empire().mesh_index_buffer = ib;
    detail::empire().mesh_index_memory = im;
    detail::empire().mesh_index_count = ic;
}

inline void stone_seal_transient_pool(VkCommandPool pool) noexcept {
    if (empire_sealed && pool != detail::empire().transient_pool.decrypt()) {
        LOG_FATAL_CAT("EMPIRE", "Transient pool tamper attempt — empire compromised");
        std::abort();
    }
    detail::empire().transient_pool = pool;
}

// -----------------------------------------------------------------------------
// FINAL UNBREAKABLE SEAL — call once after all resources are set
// -----------------------------------------------------------------------------
inline void stone_seal_final() noexcept {
    if (empire_sealed) return;

    empire_sealed = true;
    LOG_AMOURANTH("STONEKEY v30.25 — FINAL EMPIRE SEAL FORGED — FULL ACCESS GRANTED — ALL RESOURCES LOCKED");
}

} // namespace StoneKey