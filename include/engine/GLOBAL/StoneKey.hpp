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
// STONEKEY v∞ — RUNTIME OBFUSCATION — C++ VERSION — FINAL
// THE EMPIRE IS SEALED — EVERY VALUE VALIDATED WITH SIZE & HANDLE DISPLAY
// COMMAND BUFFERS REMOVED FROM SEAL (transient, recreated on resize)
// ADDED: GLOBAL TRANSIENT COMMAND POOL — SHARED ACROSS ALL MODULES (LAS, Renderer, main)
// FULLY COMPILABLE — CLEAN & ETERNAL — PINK PHOTONS ETERNAL
// DECEMBER 20, 2025 — THE FINAL LIGHT IS COMPLETE
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <source_location>
#include <vector>
#include <cstdio>
#include <format>
#include "engine/GLOBAL/logging.hpp"

// Forward declarations
namespace RTX { class PipelineManager; }

namespace StoneKey {

    // Global transient command pool — shared across all modules
    static inline VkCommandPool g_transientCommandPool = VK_NULL_HANDLE;

    struct Empire final {
        static inline VkInstance       instance = VK_NULL_HANDLE;
        static inline VkDevice         device = VK_NULL_HANDLE;
        static inline VkPhysicalDevice physical = VK_NULL_HANDLE;
        static inline VkSurfaceKHR     surface = VK_NULL_HANDLE;
        static inline VkSwapchainKHR   swapchain = VK_NULL_HANDLE;

        static inline VkQueue graphicsQueue = VK_NULL_HANDLE;
        static inline VkQueue presentQueue = VK_NULL_HANDLE;
        static inline VkQueue computeQueue = VK_NULL_HANDLE;
        static inline VkQueue transferQueue = VK_NULL_HANDLE;

        static inline uint32_t graphicsFamily = ~0u;
        static inline uint32_t presentFamily = ~0u;
        static inline uint32_t transferFamily = ~0u;
        static inline uint32_t computeFamily = ~0u;

        static inline RTX::PipelineManager* pipeline = nullptr;
        static inline SDL_Window* window = nullptr;

        static inline std::vector<VkImage>     images;
        static inline std::vector<VkImageView> views;
        static inline VkRenderPass             pass = VK_NULL_HANDLE;
        static inline VkExtent2D               extent = { 0, 0 };
        static inline uint32_t                 image_count = 0;

        static inline VkBuffer       stone_mesh_vertex_buffer = VK_NULL_HANDLE;
        static inline VkDeviceMemory stone_mesh_vertex_memory = VK_NULL_HANDLE;
        static inline VkBuffer       stone_mesh_index_buffer = VK_NULL_HANDLE;
        static inline VkDeviceMemory stone_mesh_index_memory = VK_NULL_HANDLE;
        static inline uint32_t       stone_mesh_index_count = 0;

        static inline VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{
            .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR
        };

        static inline bool sealed{ false };
    };

    // GETTERS
    [[nodiscard]] inline VkInstance&       stone_instance() noexcept { return Empire::instance; }
    [[nodiscard]] inline VkDevice&         stone_device() noexcept { return Empire::device; }
    [[nodiscard]] inline VkPhysicalDevice& stone_physical() noexcept { return Empire::physical; }
    [[nodiscard]] inline VkSurfaceKHR&     stone_surface() noexcept { return Empire::surface; }
    [[nodiscard]] inline VkSwapchainKHR&   stone_swapchain() noexcept { return Empire::swapchain; }

    [[nodiscard]] inline VkQueue& stone_graphics_queue() noexcept { return Empire::graphicsQueue; }
    [[nodiscard]] inline VkQueue& stone_present_queue() noexcept { return Empire::presentQueue; }
    [[nodiscard]] inline VkQueue& stone_compute_queue() noexcept { return Empire::computeQueue; }
    [[nodiscard]] inline VkQueue& stone_transfer_queue() noexcept { return Empire::transferQueue; }

    [[nodiscard]] inline uint32_t& stone_graphics_family() noexcept { return Empire::graphicsFamily; }
    [[nodiscard]] inline uint32_t& stone_present_family() noexcept { return Empire::presentFamily; }
    [[nodiscard]] inline uint32_t& stone_transfer_family() noexcept { return Empire::transferFamily; }
    [[nodiscard]] inline uint32_t& stone_compute_family() noexcept { return Empire::computeFamily; }

    [[nodiscard]] inline RTX::PipelineManager* stone_pipeline() noexcept { return Empire::pipeline; }
    [[nodiscard]] inline SDL_Window* stone_window() noexcept { return Empire::window; }

    [[nodiscard]] inline auto& stone_images() noexcept { return Empire::images; }
    [[nodiscard]] inline auto& stone_views() noexcept { return Empire::views; }
    [[nodiscard]] inline VkImage& stone_image(uint32_t i) noexcept { return Empire::images[i]; }
    [[nodiscard]] inline VkImageView& stone_view(uint32_t i) noexcept { return Empire::views[i]; }

    [[nodiscard]] inline VkRenderPass& stone_pass() noexcept { return Empire::pass; }
    [[nodiscard]] inline VkExtent2D& stone_extent() noexcept { return Empire::extent; }
    [[nodiscard]] inline uint32_t& stone_width() noexcept { return Empire::extent.width; }
    [[nodiscard]] inline uint32_t& stone_height() noexcept { return Empire::extent.height; }
    [[nodiscard]] inline uint32_t& stone_image_count() noexcept { return Empire::image_count; }

    [[nodiscard]] inline VkPhysicalDeviceRayTracingPipelinePropertiesKHR& stone_rtprops() noexcept { return Empire::rtProps; }

    [[nodiscard]] inline VkCommandPool& stone_transient_pool() noexcept { return g_transientCommandPool; }

    struct StoneMesh {
        VkBuffer       vertexBuffer;
        VkDeviceMemory vertexMemory;
        VkBuffer       indexBuffer;
        VkDeviceMemory indexMemory;
        uint32_t       indexCount;
    };

    [[nodiscard]] inline StoneMesh stone_mesh() noexcept {
        return {
            Empire::stone_mesh_vertex_buffer,
            Empire::stone_mesh_vertex_memory,
            Empire::stone_mesh_index_buffer,
            Empire::stone_mesh_index_memory,
            Empire::stone_mesh_index_count
        };
    }

    // SEALERS
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

    // FINAL SEAL — VALIDATES EVERY VALUE WITH SIZE & HANDLE DISPLAY
    inline void stone_seal_final() noexcept
    {
        const bool was_sealed = Empire::sealed;
        Empire::sealed = true;

        if (was_sealed) {
            LOG_INFO_CAT("EMPIRE", "Stone seal already complete — empire eternal.");
            return;
        }

        LOG_AMOURANTH("STONEKEY FINAL SEAL CEREMONY — VALIDATING EVERY VALUE WITH SIZE & HANDLE");

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

        CHECK_HANDLE(Empire::instance != VK_NULL_HANDLE, "VkInstance", Empire::instance);
        CHECK_HANDLE(Empire::device != VK_NULL_HANDLE, "VkDevice", Empire::device);
        CHECK_HANDLE(Empire::physical != VK_NULL_HANDLE, "VkPhysicalDevice", Empire::physical);
        CHECK_HANDLE(Empire::surface != VK_NULL_HANDLE, "VkSurfaceKHR", Empire::surface);
        CHECK_HANDLE(Empire::swapchain != VK_NULL_HANDLE, "VkSwapchainKHR", Empire::swapchain);

        CHECK_HANDLE(Empire::graphicsQueue != VK_NULL_HANDLE, "Graphics Queue", Empire::graphicsQueue);
        CHECK_HANDLE(Empire::presentQueue != VK_NULL_HANDLE, "Present Queue", Empire::presentQueue);
        CHECK_HANDLE(Empire::computeQueue != VK_NULL_HANDLE, "Compute Queue", Empire::computeQueue);
        CHECK_HANDLE(Empire::transferQueue != VK_NULL_HANDLE, "Transfer Queue", Empire::transferQueue);

        CHECK_VALUE(Empire::graphicsFamily != ~0u, "Graphics Family Index", Empire::graphicsFamily);
        CHECK_VALUE(Empire::presentFamily != ~0u, "Present Family Index", Empire::presentFamily);
        CHECK_VALUE(Empire::transferFamily != ~0u, "Transfer Family Index", Empire::transferFamily);
        CHECK_VALUE(Empire::computeFamily != ~0u, "Compute Family Index", Empire::computeFamily);

        CHECK_HANDLE(Empire::pipeline != nullptr, "PipelineManager*", Empire::pipeline);
        CHECK_HANDLE(Empire::window != nullptr, "SDL_Window*", Empire::window);

        CHECK_VECTOR(!Empire::images.empty(), "Swapchain Images Vector", Empire::images, Empire::images.size() * sizeof(VkImage));
        CHECK_VECTOR(!Empire::views.empty(), "Swapchain Image Views Vector", Empire::views, Empire::views.size() * sizeof(VkImageView));
        CHECK_VALUE(Empire::image_count > 0, "Swapchain Image Count", Empire::image_count);
        CHECK_VALUE(Empire::extent.width > 0 && Empire::extent.height > 0, "Swapchain Extent", Empire::extent.width * Empire::extent.height * 4);

        CHECK_VALUE(Empire::rtProps.shaderGroupHandleSize > 0, "Ray Tracing Handle Size", Empire::rtProps.shaderGroupHandleSize);
        CHECK_VALUE(Empire::rtProps.maxRayRecursionDepth > 0, "Ray Tracing Max Recursion Depth", Empire::rtProps.maxRayRecursionDepth);

        CHECK_HANDLE(g_transientCommandPool != VK_NULL_HANDLE, "Transient Command Pool", g_transientCommandPool);

        // Mesh data (optional but validated if present)
        if (Empire::stone_mesh_vertex_buffer != VK_NULL_HANDLE ||
            Empire::stone_mesh_index_buffer != VK_NULL_HANDLE ||
            Empire::stone_mesh_index_count > 0) {
            CHECK_HANDLE(Empire::stone_mesh_vertex_buffer != VK_NULL_HANDLE, "Mesh Vertex Buffer", Empire::stone_mesh_vertex_buffer);
            CHECK_HANDLE(Empire::stone_mesh_index_buffer != VK_NULL_HANDLE, "Mesh Index Buffer", Empire::stone_mesh_index_buffer);
            CHECK_VALUE(Empire::stone_mesh_index_count > 0, "Mesh Index Count", Empire::stone_mesh_index_count);
        }

        if (all_good) {
            LOG_AMOURANTH("STONEKEY SEAL SUCCESSFUL — EVERY VALUE VALIDATED WITH SIZE & HANDLE — EMPIRE ETERNAL");
            LOG_AMOURANTH("THE EMPIRE IS SEALED — PINK PHOTONS MAY NOW FLOW FOREVER");
            LOG_CID("CID: \"...it's sealed... it's finally... sealed...\"");
        } else {
            LOG_FATAL_CAT("EMPIRE", "STONEKEY SEAL FAILED — EMPIRE COMPROMISED — ONE OR MORE VALUES INVALID");
            std::abort();
        }
    }

    namespace bridge {
        [[nodiscard]] inline VkQueue graphics_queue() noexcept { return stone_graphics_queue(); }
    }

    // =============================================================================
    // STONEKEY v∞ — RUNTIME OBFUSCATION — C++ VERSION — FINAL
    // =============================================================================
    namespace detail {
        constexpr uint64_t kStoneObfuscatorBase = 
            0x9E37AF18C64D8A17UL ^ 0xE4F8B29D71A3C56CUL ^ 0x1337C0DE69F00D42UL;

        inline uint64_t g_runtimeObfuscator = kStoneObfuscatorBase;
    }

    [[nodiscard]] inline uint64_t stone_get_obfuscator() noexcept {
        return detail::g_runtimeObfuscator;
    }

    inline void stone_set_obfuscator(uint64_t key) noexcept {
        detail::g_runtimeObfuscator = key ? key : detail::kStoneObfuscatorBase;
    }

    #define STONE_FINAL_OBFUSCATE(val)   (static_cast<uint64_t>(val) ^ ::StoneKey::stone_get_obfuscator())
    #define STONE_FINAL_DEOBFUSCATE(val) (static_cast<uint64_t>(val) ^ ::StoneKey::stone_get_obfuscator())

    #define STONE_OBFUSCATE_RT(val)  STONE_FINAL_OBFUSCATE(val)
    #define STONE_DEOBFUSCATE_RT(val) STONE_FINAL_DEOBFUSCATE(val)

} // namespace StoneKey

// =============================================================================
// FINAL STONEKEY — JANUARY 07, 2026
// - No VulkanRenderer forward decl