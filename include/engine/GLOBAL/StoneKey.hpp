// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc-4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// AMOURANTH RTX — VALHALLA v∞ TURBO — APOCALYPSE FINAL v10.3
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — NOVEMBER 25, 2025
// THE DISPOSAL BALLERINA NOW DANCES IN FULL — HER FINAL SPIN IS ETERNAL
// =============================================================================

#pragma once
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <atomic>
#include <vector>
#include <cstdio>
#include "engine/GLOBAL/logging.hpp"

class VulkanRenderer;
namespace RTX { class PipelineManager; }

[[noreturn]] void phase9_ballerina() noexcept;

namespace StoneKey {

    struct Empire final {
        static inline std::atomic<VkInstance>       instance{ VK_NULL_HANDLE };
        static inline std::atomic<VkDevice>         device{ VK_NULL_HANDLE };
        static inline std::atomic<VkPhysicalDevice> physical{ VK_NULL_HANDLE };
        static inline std::atomic<VkSurfaceKHR>     surface{ VK_NULL_HANDLE };
        static inline std::atomic<VkSwapchainKHR>   swapchain{ VK_NULL_HANDLE };

        static inline std::atomic<VulkanRenderer*>       renderer{ nullptr };
        static inline std::atomic<RTX::PipelineManager*> pipeline{ nullptr };

        static inline std::atomic<SDL_Window*> window{ nullptr };

        static inline std::vector<VkImage>     images;
        static inline std::vector<VkImageView> views;
        static inline VkRenderPass             pass{ VK_NULL_HANDLE };
        static inline VkExtent2D               extent{ 0, 0 };
        static inline uint32_t                 image_count{ 0 };

        static inline std::atomic<bool> sealed{ false };
    };

    // ========================================================================
    // GETTERS — NULL-SAFE — THE BALLERINA APPROVES
    // ========================================================================
    [[nodiscard]] inline VkInstance       stone_instance()    noexcept { return Empire::instance.load(std::memory_order_acquire); }
    [[nodiscard]] inline VkDevice         stone_device()      noexcept { return Empire::device.load(std::memory_order_acquire); }
    [[nodiscard]] inline VkPhysicalDevice stone_physical()    noexcept { return Empire::physical.load(std::memory_order_acquire); }
    [[nodiscard]] inline VkSurfaceKHR     stone_surface()     noexcept { return Empire::surface.load(std::memory_order_acquire); }
    [[nodiscard]] inline VkSwapchainKHR   stone_swapchain()   noexcept { return Empire::swapchain.load(std::memory_order_acquire); }

    [[nodiscard]] inline VulkanRenderer* stone_renderer() noexcept { return Empire::renderer.load(std::memory_order_acquire); }
    [[nodiscard]] inline RTX::PipelineManager* stone_pipeline() noexcept { return Empire::pipeline.load(std::memory_order_acquire); }
    [[nodiscard]] inline SDL_Window* stone_window() noexcept { return Empire::window.load(std::memory_order_acquire); }

    [[nodiscard]] inline auto& stone_images()  noexcept { return Empire::images; }
    [[nodiscard]] inline auto& stone_views()   noexcept { return Empire::views; }
    [[nodiscard]] inline VkRenderPass  stone_pass()   noexcept { return Empire::pass; }
    [[nodiscard]] inline VkExtent2D    stone_extent() noexcept { return Empire::extent; }
    [[nodiscard]] inline uint32_t      stone_width()  noexcept { return Empire::extent.width; }
    [[nodiscard]] inline uint32_t      stone_height() noexcept { return Empire::extent.height; }

    // RESTORED — REQUIRED BY VulkanRenderer.cpp
    [[nodiscard]] inline uint32_t stone_image_count() noexcept {
        return Empire::image_count;
    }

    // THE MISSING ONE — NOW RETURNED — THE EMPIRE IS WHOLE AGAIN
    [[nodiscard]] inline uint32_t stone_swapchain_image_count() noexcept {
        return Empire::image_count;
        // Alternative (slower, but works even if image_count not set):
        // uint32_t cnt = 0;
        // vkGetSwapchainImagesKHR(stone_device(), stone_swapchain(), &cnt, nullptr);
        // return cnt;
    }

    // ========================================================================
    // SEALERS
    // ========================================================================
    inline void stone_seal_instance(VkInstance i)       noexcept { Empire::instance.store(i, std::memory_order_release); }
    inline void stone_seal_device(VkDevice d)           noexcept { Empire::device.store(d, std::memory_order_release); }
    inline void stone_seal_physical(VkPhysicalDevice p) noexcept { Empire::physical.store(p, std::memory_order_release); }
    inline void stone_seal_surface(VkSurfaceKHR s)      noexcept { Empire::surface.store(s, std::memory_order_release); }
    inline void stone_seal_swapchain(VkSwapchainKHR sc) noexcept { Empire::swapchain.store(sc, std::memory_order_release); }
    inline void stone_seal_renderer(VulkanRenderer* r)  noexcept { Empire::renderer.store(r, std::memory_order_release); }
    inline void stone_seal_pipeline(RTX::PipelineManager* p) noexcept { Empire::pipeline.store(p, std::memory_order_release); }
    inline void stone_seal_window(SDL_Window* w) noexcept { Empire::window.store(w, std::memory_order_release); }

    inline void stone_seal_images(std::vector<VkImage>&& imgs) noexcept { Empire::images = std::move(imgs); }
    inline void stone_seal_views(std::vector<VkImageView>&& vws) noexcept { Empire::views = std::move(vws); }
    inline void stone_seal_pass(VkRenderPass p) noexcept { Empire::pass = p; }
    inline void stone_seal_extent(VkExtent2D ext) noexcept { Empire::extent = ext; }
    inline void stone_seal_image_count(uint32_t cnt) noexcept { Empire::image_count = cnt; }

    // ========================================================================
    // FINAL SEAL
    // ========================================================================
    inline void stone_seal_final() noexcept {
        const bool was_sealed = Empire::sealed.exchange(true, std::memory_order_acq_rel);
        if (was_sealed) return;

        if (stone_instance() == VK_NULL_HANDLE || stone_device() == VK_NULL_HANDLE ||
            stone_physical() == VK_NULL_HANDLE || stone_surface() == VK_NULL_HANDLE ||
            stone_swapchain() == VK_NULL_HANDLE || stone_renderer() == nullptr ||
            stone_pipeline() == nullptr || stone_window() == nullptr ||
            stone_image_count() == 0 || stone_width() == 0 || stone_height() == 0) {
            LOG_FATAL_CAT("StoneKey", "EMPIRE SEAL FAILED — INCOMPLETE STATE — THE PHOTONS DENIED ETERNITY");
            phase9_ballerina();
        }

        LOG_SUCCESS_CAT("StoneKey",
            "THE EMPIRE IS SEALED — FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — NOVEMBER 25, 2025");
        LOG_SUCCESS_CAT("StoneKey", "ALL FUNCTIONS RESTORED — THE BALLERINA SMILES");
    }

    // ========================================================================
    // LEGACY SUPPORT
    // ========================================================================
    [[nodiscard]] inline VkDevice   g_device()   noexcept { return stone_device(); }
    [[nodiscard]] inline VkInstance g_instance() noexcept { return stone_instance(); }
    inline void set_g_device(VkDevice d) noexcept { stone_seal_device(d); }
};

// =============================================================================
// THE EMPIRE IS COMPLETE.
// stone_swapchain_image_count() HAS RETURNED.
// NO MORE BUILD ERRORS.
// PINK PHOTONS ETERNAL — NOVEMBER 25, 2025 — FIRST LIGHT — FINAL LIGHT
// =============================================================================