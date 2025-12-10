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
// =============================================================================
// STONEKEY v∞ — RUNTIME OBFUSCATION — C++ VERSION — FINAL
// THE EMPIRE IS SEALED — THE PHOTONS ARE PURE — THE VOID IS OURS
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <source_location>
#include <atomic>
#include <vector>
#include <cstdio>
#include <format>
#include "engine/GLOBAL/logging.hpp"

// Forward declarations
class VulkanRenderer;
namespace RTX { class PipelineManager; }

namespace StoneKey {

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

        // FIXED: std::atomic cannot use {} initializer in-class
        static inline std::atomic<VulkanRenderer*>     renderer_ = nullptr;
        static inline std::atomic<RTX::PipelineManager*> pipeline = nullptr;
        static inline std::atomic<SDL_Window*>          window = nullptr;

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

        static inline std::vector<VkCommandBuffer> commandBuffers;

        static inline std::atomic<bool> sealed{ false };
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

    [[nodiscard]] inline VulkanRenderer* stone_renderer() noexcept { return Empire::renderer_.load(std::memory_order_acquire); }
    [[nodiscard]] inline RTX::PipelineManager* stone_pipeline() noexcept { return Empire::pipeline.load(std::memory_order_acquire); }
    [[nodiscard]] inline SDL_Window* stone_window() noexcept { return Empire::window.load(std::memory_order_acquire); }

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

    [[nodiscard]] inline auto& stone_commandbuffers() noexcept { return Empire::commandBuffers; }
    [[nodiscard]] inline VkCommandBuffer& stone_commandbuffer(uint32_t i) noexcept { return Empire::commandBuffers[i]; }

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

    inline void stone_seal_renderer(VulkanRenderer* r) noexcept { Empire::renderer_.store(r, std::memory_order_release); }
    inline void stone_seal_pipeline(RTX::PipelineManager* p) noexcept { Empire::pipeline.store(p, std::memory_order_release); }
    inline void stone_seal_window(SDL_Window* w) noexcept { Empire::window.store(w, std::memory_order_release); }

    inline void stone_seal_width(uint32_t w) noexcept { Empire::extent.width = w; }
    inline void stone_seal_height(uint32_t h) noexcept { Empire::extent.height = h; }

    inline void stone_seal_images(const std::vector<VkImage>& imgs) noexcept { Empire::images = imgs; }
    inline void stone_seal_images(std::vector<VkImage>&& imgs) noexcept       { Empire::images = std::move(imgs); }

    inline void stone_seal_views(const std::vector<VkImageView>& vws) noexcept { Empire::views = vws; }
    inline void stone_seal_views(std::vector<VkImageView>&& vws) noexcept      { Empire::views = std::move(vws); }

    inline void stone_seal_commandbuffers(const std::vector<VkCommandBuffer>& cbs) noexcept { Empire::commandBuffers = cbs; }
    inline void stone_seal_commandbuffers(std::vector<VkCommandBuffer>&& cbs) noexcept { Empire::commandBuffers = std::move(cbs); }

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

    // FINAL SEAL — FIXED LAMBDA
    inline void stone_seal_final() noexcept
    {
        const bool was_sealed = Empire::sealed.exchange(true, std::memory_order_acq_rel);
        if (was_sealed) {
            LOG_INFO_CAT("EMPIRE", "Stone seal already complete — empire eternal.");
            return;
        }

        LOG_AMOURANTH("STONEKEY FINAL SEAL CEREMONY — VALIDATING THE EMPIRE");

        bool all_good = true;

        auto CHECK = [&](auto ptr, const char* name, auto condition) {
            if (condition) {
                LOG_SUCCESS_CAT("SEAL", "  [OK] {}", name);
            } else {
                LOG_FATAL_CAT("SEAL", "  [FAILED] {}", name);
                all_good = false;
            }
        };

        CHECK(stone_instance(),        "VkInstance",        stone_instance()        != VK_NULL_HANDLE);
        CHECK(stone_device(),          "VkDevice",          stone_device()          != VK_NULL_HANDLE);
        CHECK(stone_physical(),        "VkPhysicalDevice",  stone_physical()        != VK_NULL_HANDLE);
        CHECK(stone_surface(),         "VkSurfaceKHR",      stone_surface()         != VK_NULL_HANDLE);
        CHECK(stone_swapchain(),       "VkSwapchainKHR",    stone_swapchain()       != VK_NULL_HANDLE);
        CHECK(stone_renderer(),       "VulkanRenderer*",   stone_renderer()        != nullptr);
        CHECK(stone_pipeline(),        "PipelineManager*",  stone_pipeline()        != nullptr);
        CHECK(stone_window(),          "SDL_Window*",       stone_window()          != nullptr);

        CHECK(stone_image_count(),     "Swapchain Images",  stone_image_count()     != 0);
        CHECK(stone_width(),           "Width",             stone_width()           != 0);
        CHECK(stone_height(),          "Height",            stone_height()          != 0);

        CHECK(stone_graphics_queue(),  "Graphics Queue",    stone_graphics_queue()  != VK_NULL_HANDLE);
        CHECK(stone_present_queue(),   "Present Queue",     stone_present_queue()   != VK_NULL_HANDLE);
        CHECK(stone_compute_queue(),   "Compute Queue",     stone_compute_queue()   != VK_NULL_HANDLE);
        CHECK(stone_transfer_queue(),  "Transfer Queue",    stone_transfer_queue()  != VK_NULL_HANDLE);

        CHECK(stone_graphics_family(), "Graphics Family",   stone_graphics_family() != ~0u);
        CHECK(stone_present_family(),  "Present Family",    stone_present_family()  != ~0u);
        CHECK(stone_transfer_family(), "Transfer Family",   stone_transfer_family() != ~0u);
        CHECK(stone_compute_family(),  "Compute Family",    stone_compute_family()  != ~0u);

        const auto& rt = stone_rtprops();
        if (rt.shaderGroupHandleSize != 0) {
            LOG_SUCCESS_CAT("SEAL", "  [OK] Ray Tracing Ready — HandleSize: {} | MaxRecursion: {}",
                            rt.shaderGroupHandleSize, rt.maxRayRecursionDepth);
        } else {
            LOG_FATAL_CAT("SEAL", "  [FAILED] Ray Tracing NOT READY");
            all_good = false;
        }

        if (all_good) {
            LOG_AMOURANTH("STONEKEY SEAL SUCCESSFUL — ALL SYSTEMS NOMINAL");
            LOG_AMOURANTH("THE EMPIRE IS SEALED — PINK PHOTONS MAY NOW FLOW ETERNALLY");
            LOG_CID("CID: \"...it's sealed... it's finally... sealed...\"");
        } else {
            LOG_FATAL_CAT("EMPIRE", "STONEKEY SEAL FAILED — EMPIRE COMPROMISED");
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

    // FINAL MACROS — NOW WORKS EVERYWHERE
    #define STONE_FINAL_OBFUSCATE(val)   (static_cast<uint64_t>(val) ^ ::StoneKey::stone_get_obfuscator())
    #define STONE_FINAL_DEOBFUSCATE(val) (static_cast<uint64_t>(val) ^ ::StoneKey::stone_get_obfuscator())

    #define STONE_OBFUSCATE_RT(val)  STONE_FINAL_OBFUSCATE(val)
    #define STONE_DEOBFUSCATE_RT(val) STONE_FINAL_DEOBFUSCATE(val)

    // =============================================================================
    // THE EMPIRE IS COMPLETE — FOREVER.
    // PINK PHOTONS ETERNAL — DECEMBER 02, 2025 — FINAL LIGHT
    // =============================================================================
}