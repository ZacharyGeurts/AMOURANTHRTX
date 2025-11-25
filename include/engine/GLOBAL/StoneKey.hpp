// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — NOVEMBER 21, 2025
// FULLY COMPILING — PURE EMPIRE
// =============================================================================

#pragma once
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>  // Added for SDL_Window
#include <atomic>
#include <vector>
#include <cstdio>
#include "engine/GLOBAL/logging.hpp"

class VulkanRenderer;
namespace RTX { class PipelineManager; }

namespace StoneKey {

    struct Empire final {
        // Vulkan core
        static inline std::atomic<VkInstance>       instance{ VK_NULL_HANDLE };
        static inline std::atomic<VkDevice>         device{ VK_NULL_HANDLE };
        static inline std::atomic<VkPhysicalDevice> physical{ VK_NULL_HANDLE };
        static inline std::atomic<VkSurfaceKHR>     surface{ VK_NULL_HANDLE };
        static inline std::atomic<VkSwapchainKHR>   swapchain{ VK_NULL_HANDLE };

        // Engine
        static inline std::atomic<VulkanRenderer*>       renderer{ nullptr };
        static inline std::atomic<RTX::PipelineManager*> pipeline{ nullptr };

        // SDL Window
        static inline std::atomic<SDL_Window*> window{ nullptr };

        // Swapchain state
        static inline std::vector<VkImage>     images;
        static inline std::vector<VkImageView> views;
        static inline VkRenderPass             pass{ VK_NULL_HANDLE };
        static inline VkExtent2D               extent{ 0, 0 };  // Fixed: Initialize to 0; set dynamically
        static inline uint32_t                 image_count{ 0 };

        // One-time seal
        static inline std::atomic<bool> sealed{ false };
    };

    // ========================================================================
    // GETTERS — clean, simple, fast
    // ========================================================================
    [[nodiscard]] inline VkInstance       stone_instance()    noexcept { return Empire::instance.load(std::memory_order_acquire); }
    [[nodiscard]] inline VkDevice         stone_device()      noexcept { return Empire::device.load(std::memory_order_acquire); }
    [[nodiscard]] inline VkPhysicalDevice stone_physical()    noexcept { return Empire::physical.load(std::memory_order_acquire); }
    [[nodiscard]] inline VkSurfaceKHR     stone_surface()     noexcept { return Empire::surface.load(std::memory_order_acquire); }
    [[nodiscard]] inline VkSwapchainKHR   stone_swapchain()   noexcept { return Empire::swapchain.load(std::memory_order_acquire); }

    [[nodiscard]] inline VulkanRenderer* stone_renderer() noexcept {
        auto* r = Empire::renderer.load(std::memory_order_acquire);
        if (!r) {
            LOG_ERROR("StoneKey", "Renderer is null - aborting");
            std::abort();  // Retained abort for speed; consider throwing in non-performance-critical builds
        }
        return r;
    }

    [[nodiscard]] inline RTX::PipelineManager* stone_pipeline() noexcept {
        auto* p = Empire::pipeline.load(std::memory_order_acquire);
        if (!p) {
            LOG_ERROR("StoneKey", "Pipeline manager is null - aborting");
            std::abort();
        }
        return p;
    }

    [[nodiscard]] inline SDL_Window* stone_window() noexcept {
        auto* w = Empire::window.load(std::memory_order_acquire);
        if (!w) {
            LOG_ERROR("StoneKey", "Window is null - aborting");
            std::abort();
        }
        return w;
    }

    [[nodiscard]] inline auto& stone_images()  noexcept { return Empire::images; }
    [[nodiscard]] inline auto& stone_views()   noexcept { return Empire::views; }
    [[nodiscard]] inline VkRenderPass  stone_pass()   noexcept { return Empire::pass; }
    [[nodiscard]] inline VkExtent2D    stone_extent() noexcept { return Empire::extent; }
    [[nodiscard]] inline uint32_t      stone_width()  noexcept { return Empire::extent.width; }
    [[nodiscard]] inline uint32_t      stone_height() noexcept { return Empire::extent.height; }
    [[nodiscard]] inline uint32_t      stone_image_count() noexcept { return Empire::image_count; }

    // ========================================================================
    // SEALERS — call as many times as you want
    // ========================================================================
    inline void stone_seal_instance(VkInstance i)       noexcept { Empire::instance.store(i, std::memory_order_release); }
    inline void stone_seal_device(VkDevice d)           noexcept { Empire::device.store(d, std::memory_order_release); }
    inline void stone_seal_physical(VkPhysicalDevice p) noexcept { Empire::physical.store(p, std::memory_order_release); }
    inline void stone_seal_surface(VkSurfaceKHR s)      noexcept { Empire::surface.store(s, std::memory_order_release); }
    inline void stone_seal_swapchain(VkSwapchainKHR sc) noexcept { Empire::swapchain.store(sc, std::memory_order_release); }
    inline void stone_seal_renderer(VulkanRenderer* r)  noexcept { Empire::renderer.store(r, std::memory_order_release); }
    inline void stone_seal_pipeline(RTX::PipelineManager* p) noexcept { Empire::pipeline.store(p, std::memory_order_release); }
    inline void stone_seal_window(SDL_Window* w) noexcept { Empire::window.store(w, std::memory_order_release); }

    // Additional sealers for non-atomic members (assuming single-threaded setup phase)
    inline void stone_seal_images(std::vector<VkImage>&& imgs) noexcept { Empire::images = std::move(imgs); }
    inline void stone_seal_views(std::vector<VkImageView>&& vws) noexcept { Empire::views = std::move(vws); }
    inline void stone_seal_pass(VkRenderPass p) noexcept { Empire::pass = p; }
    inline void stone_seal_extent(VkExtent2D ext) noexcept { Empire::extent = ext; }
    inline void stone_seal_image_count(uint32_t cnt) noexcept { Empire::image_count = cnt; }

    // ========================================================================
    // FINAL SEAL — safe to call 1 or 1000 times
    // ========================================================================
    inline void stone_seal_final() noexcept {
        const bool was_sealed = Empire::sealed.exchange(true, std::memory_order_acq_rel);
        if (was_sealed) return;  // already sealed — do nothing

        // Fixed: Add basic validation before sealing
        if (stone_instance() == VK_NULL_HANDLE || stone_device() == VK_NULL_HANDLE ||
            stone_physical() == VK_NULL_HANDLE || stone_surface() == VK_NULL_HANDLE ||
            stone_swapchain() == VK_NULL_HANDLE || stone_renderer() == nullptr ||
            stone_pipeline() == nullptr || stone_window() == nullptr ||
            stone_image_count() == 0 || stone_width() == 0 || stone_height() == 0) {
            LOG_ERROR("StoneKey", "Attempted to seal with incomplete state");
            std::abort();
        }

        LOG_SUCCESS_CAT("StoneKey",
            "THE EMPIRE IS SEALED — FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL");
    }

    // ========================================================================
    // LEGACY SUPPORT — g_*() still works (optional)
    // ========================================================================
    [[nodiscard]] inline VkDevice   g_device()   noexcept { return stone_device(); }
    [[nodiscard]] inline VkInstance g_instance() noexcept { return stone_instance(); }
    [[nodiscard]] inline auto&      g_swapchain_images() noexcept { return stone_images(); }
    inline void set_g_device(VkDevice d) noexcept { stone_seal_device(d); }
    // etc... (add more as needed)
};

// =============================================================================
// DONE. NO TEMPLATES. NO CONCEPTS. NO MACRO HELL.
// JUST WORKS. COMPILES. RUNS. FAST.
// PINK PHOTONS ETERNAL — NOVEMBER 25, 2025
// =============================================================================