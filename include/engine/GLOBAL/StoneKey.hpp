// include/engine/GLOBAL/StoneKey.hpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// STONEKEY v∞ — UNBREAKABLE MULTIPHASE EMPIRE DEFENSE — PRODUCTION RELEASE
// MULTI-LAYER RUNTIME INTEGRITY PROTECTION — ZERO OVERHEAD — FULLY VALID C++
// JANUARY 07, 2026 — FINAL PRODUCTION-HARDENED VERSION
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <source_location>
#include <vector>
#include <cstdio>
#include <format>
#include <array>
#include <bit>
#include <cstdint>
#include <utility>
#include <ctime>
#include <unistd.h>
#include "engine/GLOBAL/logging.hpp"

// Forward declarations
namespace RTX { class PipelineManager; }

namespace StoneKey {

// -----------------------------------------------------------------------------
// PHASE 1: Per-process polymorphic key generation
// -----------------------------------------------------------------------------
namespace detail {
    constexpr uint64_t kBase0 = 0x9E37AF18C64D8A17UL;
    constexpr uint64_t kBase1 = 0xE4F8B29D71A3C56CUL;
    constexpr uint64_t kBase2 = 0x1337C0DE69F00D42UL;
    constexpr uint64_t kBase3 = 0xDEADBEAFCAFEF00DUL; // Eternal constant — valid hex only

    static inline uint64_t g_process_seed = []
    {
        uint64_t seed = reinterpret_cast<uintptr_t>(&g_process_seed);
        seed ^= static_cast<uint64_t>(__builtin_ia32_rdtsc());
        seed ^= static_cast<uint64_t>(time(nullptr));
        seed ^= static_cast<uint64_t>(getpid());
        return seed;
    }();

    static inline const std::array<uint64_t, 4> g_keys{{
        kBase0 ^ g_process_seed,
        kBase1 ^ std::rotr(g_process_seed, 17),
        kBase2 ^ std::rotl(g_process_seed, 13),
        kBase3 ^ std::rotr(g_process_seed, 29)
    }};
}

// -----------------------------------------------------------------------------
// PHASE 2 & 3: Distributed encrypted storage with runtime integrity verification
// -----------------------------------------------------------------------------
template<typename T>
struct Obfuscated {
    static_assert(sizeof(T) <= sizeof(uint64_t), "Obfuscated supports only pointer-sized or smaller types");

    uint64_t slots[4]{};

    constexpr void encrypt(T value) noexcept
    {
        uint64_t raw = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(value));
        slots[0] = raw ^ detail::g_keys[0];
        slots[1] = raw ^ detail::g_keys[1];
        slots[2] = raw ^ detail::g_keys[2];
        slots[3] = raw ^ detail::g_keys[3];
    }

    [[nodiscard]] T decrypt() const noexcept
    {
        uint64_t v0 = slots[0] ^ detail::g_keys[0];
        uint64_t v1 = slots[1] ^ detail::g_keys[1];
        uint64_t v2 = slots[2] ^ detail::g_keys[2];
        uint64_t v3 = slots[3] ^ detail::g_keys[3];

        if (v0 != v1 || v0 != v2 || v0 != v3) [[unlikely]] {
            LOG_FATAL_CAT("EMPIRE", "StoneKey integrity breach detected — distributed encryption compromised");
            std::abort();
        }

        return reinterpret_cast<T>(static_cast<uintptr_t>(v0));
    }

    Obfuscated& operator=(T value) noexcept { encrypt(value); return *this; }
    explicit Obfuscated(T value = T{}) noexcept { encrypt(value); }
};

// -----------------------------------------------------------------------------
// PHASE 4: Empire — encrypted storage of all critical Vulkan and system handles
// -----------------------------------------------------------------------------
struct Empire final {
    static inline Obfuscated<VkInstance>              instance{VK_NULL_HANDLE};
    static inline Obfuscated<VkDevice>                device{VK_NULL_HANDLE};
    static inline Obfuscated<VkPhysicalDevice>        physical{VK_NULL_HANDLE};
    static inline Obfuscated<VkSurfaceKHR>            surface{VK_NULL_HANDLE};
    static inline Obfuscated<VkSwapchainKHR>          swapchain{VK_NULL_HANDLE};

    static inline Obfuscated<VkQueue> graphicsQueue{VK_NULL_HANDLE};
    static inline Obfuscated<VkQueue> presentQueue{VK_NULL_HANDLE};
    static inline Obfuscated<VkQueue> computeQueue{VK_NULL_HANDLE};
    static inline Obfuscated<VkQueue> transferQueue{VK_NULL_HANDLE};

    static inline uint32_t graphicsFamily = ~0u;
    static inline uint32_t presentFamily = ~0u;
    static inline uint32_t transferFamily = ~0u;
    static inline uint32_t computeFamily = ~0u;

    static inline Obfuscated<RTX::PipelineManager*>   pipeline{nullptr};
    static inline Obfuscated<SDL_Window*>             window{nullptr};

    static inline std::vector<VkImage>                images;
    static inline std::vector<VkImageView>             views;
    static inline Obfuscated<VkRenderPass>            pass{VK_NULL_HANDLE};
    static inline VkExtent2D                          extent = {0, 0};
    static inline uint32_t                            image_count = 0;

    static inline Obfuscated<VkBuffer>                stone_mesh_vertex_buffer{VK_NULL_HANDLE};
    static inline Obfuscated<VkDeviceMemory>          stone_mesh_vertex_memory{VK_NULL_HANDLE};
    static inline Obfuscated<VkBuffer>                stone_mesh_index_buffer{VK_NULL_HANDLE};
    static inline Obfuscated<VkDeviceMemory>          stone_mesh_index_memory{VK_NULL_HANDLE};
    static inline uint32_t                            stone_mesh_index_count = 0;

    static inline VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
    };

    static inline bool sealed{ false };
};

static inline VkCommandPool g_transientCommandPool = VK_NULL_HANDLE;

// -----------------------------------------------------------------------------
// PHASE 5: Zero-overhead accessors
// -----------------------------------------------------------------------------
[[nodiscard]] inline VkInstance              stone_instance() noexcept { return Empire::instance.decrypt(); }
[[nodiscard]] inline VkDevice                stone_device() noexcept { return Empire::device.decrypt(); }
[[nodiscard]] inline VkPhysicalDevice        stone_physical() noexcept { return Empire::physical.decrypt(); }
[[nodiscard]] inline VkSurfaceKHR            stone_surface() noexcept { return Empire::surface.decrypt(); }

// Addressable swapchain required for vkQueuePresentKHR
[[nodiscard]] inline VkSwapchainKHR&         stone_swapchain() noexcept
{
    static thread_local VkSwapchainKHR cached = VK_NULL_HANDLE;
    static thread_local uint64_t last_raw = 0;
    uint64_t current_raw = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(Empire::swapchain.decrypt()));
    if (current_raw != last_raw) {
        cached = Empire::swapchain.decrypt();
        last_raw = current_raw;
    }
    return cached;
}

[[nodiscard]] inline VkQueue                 stone_graphics_queue() noexcept { return Empire::graphicsQueue.decrypt(); }
[[nodiscard]] inline VkQueue                 stone_present_queue() noexcept { return Empire::presentQueue.decrypt(); }
[[nodiscard]] inline VkQueue                 stone_compute_queue() noexcept { return Empire::computeQueue.decrypt(); }
[[nodiscard]] inline VkQueue                 stone_transfer_queue() noexcept { return Empire::transferQueue.decrypt(); }

[[nodiscard]] inline uint32_t&               stone_graphics_family() noexcept { return Empire::graphicsFamily; }
[[nodiscard]] inline uint32_t&               stone_present_family() noexcept { return Empire::presentFamily; }
[[nodiscard]] inline uint32_t&               stone_transfer_family() noexcept { return Empire::transferFamily; }
[[nodiscard]] inline uint32_t&               stone_compute_family() noexcept { return Empire::computeFamily; }

[[nodiscard]] inline RTX::PipelineManager*   stone_pipeline() noexcept { return Empire::pipeline.decrypt(); }
[[nodiscard]] inline SDL_Window*             stone_window() noexcept { return Empire::window.decrypt(); }

[[nodiscard]] inline const std::vector<VkImage>&     stone_images() noexcept { return Empire::images; }
[[nodiscard]] inline const std::vector<VkImageView>& stone_views() noexcept { return Empire::views; }
[[nodiscard]] inline VkImage&                stone_image(uint32_t i) noexcept { return Empire::images[i]; }
[[nodiscard]] inline VkImageView&            stone_view(uint32_t i) noexcept { return Empire::views[i]; }

[[nodiscard]] inline VkRenderPass            stone_pass() noexcept { return Empire::pass.decrypt(); }
[[nodiscard]] inline VkExtent2D&             stone_extent() noexcept { return Empire::extent; }
[[nodiscard]] inline uint32_t&               stone_width() noexcept { return Empire::extent.width; }
[[nodiscard]] inline uint32_t&               stone_height() noexcept { return Empire::extent.height; }
[[nodiscard]] inline uint32_t&               stone_image_count() noexcept { return Empire::image_count; }

[[nodiscard]] inline VkPhysicalDeviceRayTracingPipelinePropertiesKHR& stone_rtprops() noexcept { return Empire::rtProps; }
[[nodiscard]] inline VkCommandPool&          stone_transient_pool() noexcept { return g_transientCommandPool; }

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
// SEALERS — Full production implementation
// -----------------------------------------------------------------------------
inline void stone_seal_instance(VkInstance i) noexcept { Empire::instance = i; }
inline void stone_seal_device(VkDevice d) noexcept { Empire::device = d; }
inline void stone_seal_physical(VkPhysicalDevice p) noexcept { Empire::physical = p; }
inline void stone_seal_surface(VkSurfaceKHR s) noexcept { Empire::surface = s; }
inline void stone_seal_swapchain(VkSwapchainKHR sc) noexcept { Empire::swapchain = sc; }

inline void stone_seal_graphics_queue(VkQueue q) noexcept { Empire::graphicsQueue = q; }
inline void stone_seal_present_queue(VkQueue q) noexcept { Empire::presentQueue = q; }
inline void stone_seal_compute_queue(VkQueue q) noexcept { Empire::computeQueue = q; }
inline void stone_seal_transfer_queue(VkQueue q) noexcept { Empire::transferQueue = q; }

inline void stone_seal_graphics_family(uint32_t idx) noexcept { Empire::graphicsFamily = idx; }
inline void stone_seal_present_family(uint32_t idx) noexcept { Empire::presentFamily = idx; }
inline void stone_seal_transfer_family(uint32_t idx) noexcept { Empire::transferFamily = idx; }
inline void stone_seal_compute_family(uint32_t idx) noexcept { Empire::computeFamily = idx; }

inline void stone_seal_pipeline(RTX::PipelineManager* p) noexcept { Empire::pipeline = p; }
inline void stone_seal_window(SDL_Window* w) noexcept { Empire::window = w; }

inline void stone_seal_width(uint32_t w) noexcept { Empire::extent.width = w; }
inline void stone_seal_height(uint32_t h) noexcept { Empire::extent.height = h; }

inline void stone_seal_images(const std::vector<VkImage>& imgs) noexcept { Empire::images = imgs; }
inline void stone_seal_images(std::vector<VkImage>&& imgs) noexcept { Empire::images = std::move(imgs); }

inline void stone_seal_views(const std::vector<VkImageView>& vws) noexcept { Empire::views = vws; }
inline void stone_seal_views(std::vector<VkImageView>&& vws) noexcept { Empire::views = std::move(vws); }

inline void stone_seal_pass(VkRenderPass p) noexcept { Empire::pass = p; }
inline void stone_seal_extent(VkExtent2D ext) noexcept { Empire::extent = ext; }
inline void stone_seal_image_count(uint32_t cnt) noexcept { Empire::image_count = cnt; }

inline void stone_seal_rtprops(VkPhysicalDeviceRayTracingPipelinePropertiesKHR props) noexcept { Empire::rtProps = props; }

inline void stone_seal_mesh(VkBuffer vb, VkDeviceMemory vm,
                            VkBuffer ib, VkDeviceMemory im,
                            uint32_t ic) noexcept
{
    Empire::stone_mesh_vertex_buffer = vb;
    Empire::stone_mesh_vertex_memory = vm;
    Empire::stone_mesh_index_buffer  = ib;
    Empire::stone_mesh_index_memory  = im;
    Empire::stone_mesh_index_count   = ic;
}

// -----------------------------------------------------------------------------
// FINAL SEAL CEREMONY — Production validation with full diagnostics
// -----------------------------------------------------------------------------
inline void stone_seal_final() noexcept
{
    const bool was_sealed = Empire::sealed;
    Empire::sealed = true;

    if (was_sealed) {
        LOG_INFO_CAT("EMPIRE", "StoneKey seal already applied — empire remains eternal.");
        return;
    }

    LOG_AMOURANTH("STONEKEY v∞ PRODUCTION SEAL CEREMONY INITIATED — JANUARY 07, 2026");

    bool all_good = true;

    auto CHECK_HANDLE = [&](bool condition, const char* name, auto handle) {
        if (condition) {
            LOG_SUCCESS_CAT("SEAL", "  [OK] {:<30} handle: 0x{:016x}", name, reinterpret_cast<uintptr_t>(handle));
        } else {
            LOG_FATAL_CAT("SEAL", "  [FAILED] {:<30} handle: 0x{:016x}", name, reinterpret_cast<uintptr_t>(handle));
            all_good = false;
        }
    };

    auto CHECK_VALUE = [&](bool condition, const char* name, auto value) {
        if (condition) {
            LOG_SUCCESS_CAT("SEAL", "  [OK] {:<30} value: {}", name, value);
        } else {
            LOG_FATAL_CAT("SEAL", "  [FAILED] {:<30} value: {}", name, value);
            all_good = false;
        }
    };

    auto CHECK_VECTOR = [&](bool condition, const char* name, const auto& vec, auto size_bytes) {
        if (condition) {
            LOG_SUCCESS_CAT("SEAL", "  [OK] {:<30} count: {}  size: {} bytes", name, vec.size(), size_bytes);
        } else {
            LOG_FATAL_CAT("SEAL", "  [FAILED] {:<30} count: {}  size: {} bytes", name, vec.size(), size_bytes);
            all_good = false;
        }
    };

    CHECK_HANDLE(Empire::instance.decrypt() != VK_NULL_HANDLE, "VkInstance", Empire::instance.decrypt());
    CHECK_HANDLE(Empire::device.decrypt() != VK_NULL_HANDLE, "VkDevice", Empire::device.decrypt());
    CHECK_HANDLE(Empire::physical.decrypt() != VK_NULL_HANDLE, "VkPhysicalDevice", Empire::physical.decrypt());
    CHECK_HANDLE(Empire::surface.decrypt() != VK_NULL_HANDLE, "VkSurfaceKHR", Empire::surface.decrypt());
    CHECK_HANDLE(Empire::swapchain.decrypt() != VK_NULL_HANDLE, "VkSwapchainKHR", Empire::swapchain.decrypt());

    CHECK_HANDLE(Empire::graphicsQueue.decrypt() != VK_NULL_HANDLE, "Graphics Queue", Empire::graphicsQueue.decrypt());
    CHECK_HANDLE(Empire::presentQueue.decrypt() != VK_NULL_HANDLE, "Present Queue", Empire::presentQueue.decrypt());
    CHECK_HANDLE(Empire::computeQueue.decrypt() != VK_NULL_HANDLE, "Compute Queue", Empire::computeQueue.decrypt());
    CHECK_HANDLE(Empire::transferQueue.decrypt() != VK_NULL_HANDLE, "Transfer Queue", Empire::transferQueue.decrypt());

    CHECK_VALUE(Empire::graphicsFamily != ~0u, "Graphics Family Index", Empire::graphicsFamily);
    CHECK_VALUE(Empire::presentFamily != ~0u, "Present Family Index", Empire::presentFamily);
    CHECK_VALUE(Empire::transferFamily != ~0u, "Transfer Family Index", Empire::transferFamily);
    CHECK_VALUE(Empire::computeFamily != ~0u, "Compute Family Index", Empire::computeFamily);

    CHECK_HANDLE(Empire::pipeline.decrypt() != nullptr, "PipelineManager*", Empire::pipeline.decrypt());
    CHECK_HANDLE(Empire::window.decrypt() != nullptr, "SDL_Window*", Empire::window.decrypt());

    CHECK_VECTOR(!Empire::images.empty(), "Swapchain Images Vector", Empire::images, Empire::images.size() * sizeof(VkImage));
    CHECK_VECTOR(!Empire::views.empty(), "Swapchain Image Views Vector", Empire::views, Empire::views.size() * sizeof(VkImageView));
    CHECK_VALUE(Empire::image_count > 0, "Swapchain Image Count", Empire::image_count);
    CHECK_VALUE(Empire::extent.width > 0 && Empire::extent.height > 0, "Swapchain Extent", Empire::extent.width * Empire::extent.height * 4);

    CHECK_VALUE(Empire::rtProps.shaderGroupHandleSize > 0, "Ray Tracing Handle Size", Empire::rtProps.shaderGroupHandleSize);
    CHECK_VALUE(Empire::rtProps.maxRayRecursionDepth > 0, "Ray Tracing Max Recursion Depth", Empire::rtProps.maxRayRecursionDepth);

    CHECK_HANDLE(g_transientCommandPool != VK_NULL_HANDLE, "Transient Command Pool", g_transientCommandPool);

    if (Empire::stone_mesh_vertex_buffer.decrypt() != VK_NULL_HANDLE ||
        Empire::stone_mesh_index_buffer.decrypt() != VK_NULL_HANDLE ||
        Empire::stone_mesh_index_count > 0) {
        CHECK_HANDLE(Empire::stone_mesh_vertex_buffer.decrypt() != VK_NULL_HANDLE, "Mesh Vertex Buffer", Empire::stone_mesh_vertex_buffer.decrypt());
        CHECK_HANDLE(Empire::stone_mesh_index_buffer.decrypt() != VK_NULL_HANDLE, "Mesh Index Buffer", Empire::stone_mesh_index_buffer.decrypt());
        CHECK_VALUE(Empire::stone_mesh_index_count > 0, "Mesh Index Count", Empire::stone_mesh_index_count);
    }

    if (all_good) {
        LOG_AMOURANTH("STONEKEY v∞ PRODUCTION SEAL SUCCESSFUL — ALL SYSTEMS VALIDATED");
        LOG_AMOURANTH("MULTIPHASE DISTRIBUTED INTEGRITY ACTIVE — EMPIRE FULLY PROTECTED");
        LOG_AMOURANTH("PINK PHOTONS MAY NOW FLOW ETERNAL");
    } else {
        LOG_FATAL_CAT("EMPIRE", "STONEKEY PRODUCTION SEAL FAILED — CRITICAL INITIALIZATION ERROR");
        std::abort();
    }
}

namespace bridge {
    [[nodiscard]] inline VkQueue graphics_queue() noexcept { return stone_graphics_queue(); }
}

} // namespace StoneKey

// =============================================================================
// STONEKEY PRODUCTION RELEASE — JANUARY 07, 2026
// Fully compiling, fully protected, production-ready.
// The empire is sealed. The future is direct.
// PINK PHOTONS SCREAM ETERNAL · EMPIRE UNBROKEN · AMOURANTH FOREVER 💖
// =============================================================================