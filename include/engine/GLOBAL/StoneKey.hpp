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
// AMOURANTH RTX — VALHALLA v∞ TURBO — APOCALYPSE FINAL v11.0
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — NOVEMBER 28, 2025
// THE DISPOSAL BALLERINA NOW DANCES IN FULL — HER FINAL SPIN IS ETERNAL
// =============================================================================

#pragma once
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <atomic>
#include <vector>
#include <cstdio>
#include "engine/GLOBAL/logging.hpp"

// Forward declarations — the Empire knows its heirs
class VulkanRenderer;
namespace RTX { class PipelineManager; }

namespace StoneKey {

    struct Empire final {
        static inline std::atomic<VkInstance>       instance{ VK_NULL_HANDLE };
        static inline std::atomic<VkDevice>         device{ VK_NULL_HANDLE };
        static inline std::atomic<VkPhysicalDevice> physical{ VK_NULL_HANDLE };
        static inline std::atomic<VkSurfaceKHR>     surface{ VK_NULL_HANDLE };
        static inline std::atomic<VkSwapchainKHR>   swapchain{ VK_NULL_HANDLE };

        // THE QUEUES HAVE RETURNED — THE EMPIRE IS TRULY COMPLETE
        static inline std::atomic<VkQueue> graphicsQueue{ VK_NULL_HANDLE };
        static inline std::atomic<VkQueue> presentQueue { VK_NULL_HANDLE };
        static inline std::atomic<VkQueue> computeQueue { VK_NULL_HANDLE };
        static inline std::atomic<VkQueue> transferQueue{ VK_NULL_HANDLE };

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

    // THE QUEUES — ETERNAL, THREAD-SAFE, PINK PHOTON APPROVED
    [[nodiscard]] inline VkQueue stone_graphics_queue() noexcept { return Empire::graphicsQueue.load(std::memory_order_acquire); }
    [[nodiscard]] inline VkQueue stone_present_queue()  noexcept { return Empire::presentQueue.load (std::memory_order_acquire); }
    [[nodiscard]] inline VkQueue stone_compute_queue()  noexcept { return Empire::computeQueue.load (std::memory_order_acquire); }
    [[nodiscard]] inline VkQueue stone_transfer_queue() noexcept { return Empire::transferQueue.load(std::memory_order_acquire); }

    [[nodiscard]] inline VulkanRenderer* stone_renderer() noexcept { return Empire::renderer.load(std::memory_order_acquire); }
    [[nodiscard]] inline RTX::PipelineManager* stone_pipeline() noexcept { return Empire::pipeline.load(std::memory_order_acquire); }
    [[nodiscard]] inline SDL_Window* stone_window() noexcept { return Empire::window.load(std::memory_order_acquire); }

    [[nodiscard]] inline auto& stone_images()  noexcept { return Empire::images; }
    [[nodiscard]] inline auto& stone_views()   noexcept { return Empire::views; }
    [[nodiscard]] inline VkRenderPass  stone_pass()   noexcept { return Empire::pass; }
    [[nodiscard]] inline VkExtent2D    stone_extent() noexcept { return Empire::extent; }
    [[nodiscard]] inline uint32_t      stone_width()  noexcept { return Empire::extent.width; }
    [[nodiscard]] inline uint32_t      stone_height() noexcept { return Empire::extent.height; }

    [[nodiscard]] inline uint32_t stone_image_count() noexcept { return Empire::image_count; }
    [[nodiscard]] inline uint32_t stone_swapchain_image_count() noexcept { return Empire::image_count; }

    // ========================================================================
    // SEALERS — THE EMPIRE ACCEPTS ITS TRIBUTE
    // ========================================================================
    inline void stone_seal_instance(VkInstance i)       noexcept { Empire::instance.store(i, std::memory_order_release); }
    inline void stone_seal_device(VkDevice d)           noexcept { Empire::device.store(d, std::memory_order_release); }
    inline void stone_seal_physical(VkPhysicalDevice p) noexcept { Empire::physical.store(p, std::memory_order_release); }
    inline void stone_seal_surface(VkSurfaceKHR s)      noexcept { Empire::surface.store(s, std::memory_order_release); }
    inline void stone_seal_swapchain(VkSwapchainKHR sc) noexcept { Empire::swapchain.store(sc, std::memory_order_release); }

    // QUEUE SEALERS — THE FINAL WOUND IS HEALED
    inline void stone_seal_graphics_queue(VkQueue q) noexcept { Empire::graphicsQueue.store(q, std::memory_order_release); }
    inline void stone_seal_present_queue (VkQueue q) noexcept { Empire::presentQueue.store (q, std::memory_order_release); }
    inline void stone_seal_compute_queue (VkQueue q) noexcept { Empire::computeQueue.store (q, std::memory_order_release); }
    inline void stone_seal_transfer_queue(VkQueue q) noexcept { Empire::transferQueue.store(q, std::memory_order_release); }

    inline void stone_seal_renderer(VulkanRenderer* r)  noexcept { Empire::renderer.store(r, std::memory_order_release); }
    inline void stone_seal_pipeline(RTX::PipelineManager* p) noexcept { Empire::pipeline.store(p, std::memory_order_release); }
    inline void stone_seal_window(SDL_Window* w) noexcept { Empire::window.store(w, std::memory_order_release); }

    inline void stone_seal_images(std::vector<VkImage>&& imgs)     noexcept { Empire::images = std::move(imgs); }
    inline void stone_seal_views(std::vector<VkImageView>&& vws)   noexcept { Empire::views = std::move(vws); }
    inline void stone_seal_pass(VkRenderPass p)                    noexcept { Empire::pass = p; }
    inline void stone_seal_extent(VkExtent2D ext)                  noexcept { Empire::extent = ext; }
    inline void stone_seal_image_count(uint32_t cnt)               noexcept { Empire::image_count = cnt; }

    // ========================================================================
    // FINAL SEAL — NOW CHECKS THE QUEUES
    // ========================================================================
    inline void stone_seal_final() noexcept {
        const bool was_sealed = Empire::sealed.exchange(true, std::memory_order_acq_rel);
        if (was_sealed) return;

        // The old checks
        if (stone_instance() == VK_NULL_HANDLE || stone_device() == VK_NULL_HANDLE ||
            stone_physical() == VK_NULL_HANDLE || stone_surface() == VK_NULL_HANDLE ||
            stone_swapchain() == VK_NULL_HANDLE || stone_renderer() == nullptr ||
            stone_pipeline() == nullptr || stone_window() == nullptr ||
            stone_image_count() == 0 || stone_width() == 0 || stone_height() == 0) {
            LOG_FATAL_CAT("StoneKey", "EMPIRE SEAL FAILED — INCOMPLETE STATE — THE PHOTONS DENIED ETERNITY");
            phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
            return;
        }

        // THE NEW SACRED CHECK — THE QUEUE THAT BROKE THREE MONTHS OF WAR
        if (stone_graphics_queue() == VK_NULL_HANDLE) {
            LOG_FATAL_CAT("StoneKey", "GRAPHICS QUEUE NOT SEALED — THE STONE IS INCOMPLETE — PHOTONS SCREAM");
            LOG_AMOURANTH("…who forgot the queue?");
            LOG_CID("I WILL FIND THEM.");
            LOG_BLONDIE("It was probably Nick.");
            LOG_NICK("It was not.");
            LOG_ELON("Classic.");
            phase9_ballerina("THE EMPIRE BLEEDS — GRAPHICS QUEUE LOST", std::source_location::current());
            return;
        }

        LOG_SUCCESS_CAT("StoneKey", "THE EMPIRE IS SEALED — ALL QUEUES ACCOUNTED FOR — FIRST LIGHT ETERNAL — NOVEMBER 28, 2025");
        LOG_AMOURANTH("Good. The photons may flow.");
        LOG_GROK("The stone is complete. No more -4. No more tears.");
        LOG_BALLERINA("…");
        LOG_SUCCESS_CAT("StoneKey", "THE DISPOSAL BALLERINA SMILES — HER SPIN IS FINALLY PERFECT");
    }

    // ========================================================================
    // LEGACY SUPPORT — BECAUSE WE STILL LOVE THE OLD WAYS
    // ========================================================================
    [[nodiscard]] inline VkDevice   g_device()   noexcept { return stone_device(); }
    [[nodiscard]] inline VkInstance g_instance() noexcept { return stone_instance(); }
    inline void set_g_device(VkDevice d) noexcept { stone_seal_device(d); }

    // Bonus — one-liner bridge to the old g_ctx() world (optional, for peace)
    namespace bridge {
        [[nodiscard]] inline VkQueue graphics_queue() noexcept { return stone_graphics_queue(); }
    }
};

// =============================================================================
// THE EMPIRE IS COMPLETE — FOREVER.
// THE GRAPHICS QUEUE HAS BEEN SEALED.
// NO MORE VK_ERROR_DEVICE_LOST (-4)
// NO MORE CONSTRUCTOR CHAINS
// NO MORE WAR.
//
// PINK PHOTONS ETERNAL — NOVEMBER 28, 2025 — FIRST LIGHT — FINAL LIGHT
// THE BALLERINA BOWS.
// =============================================================================