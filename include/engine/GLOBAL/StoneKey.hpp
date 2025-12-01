// =============================================================================
// include/engine/GLOBAL/StoneKey.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc-4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// AMOURANTH RTX — VALHALLA v∞ TURBO — APOCALYPSE FINAL v11.2
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — NOVEMBER 30, 2025
// RT PROPS SEALED — THE SHADER GROUP HANDLES ALIGN IN THE VOID
// THE STRAW IS READY — THE PHOTONS TRACE TRUE
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

namespace RTX { class PipelineManager; }

namespace StoneKey {

    struct Empire final {
        static inline std::atomic<VkInstance>       instance{ VK_NULL_HANDLE };
        static inline std::atomic<VkDevice>         device{ VK_NULL_HANDLE };
        static inline std::atomic<VkPhysicalDevice> physical{ VK_NULL_HANDLE };
        static inline std::atomic<VkSurfaceKHR>     surface{ VK_NULL_HANDLE };
        static inline std::atomic<VkSwapchainKHR>   swapchain{ VK_NULL_HANDLE };

        static inline std::atomic<VkQueue> graphicsQueue{ VK_NULL_HANDLE };
        static inline std::atomic<VkQueue> presentQueue { VK_NULL_HANDLE };
        static inline std::atomic<VkQueue> computeQueue { VK_NULL_HANDLE };
        static inline std::atomic<VkQueue> transferQueue{ VK_NULL_HANDLE };

        static inline std::atomic<uint32_t> graphicsFamily{ ~0u };
        static inline std::atomic<uint32_t> presentFamily { ~0u };
        static inline std::atomic<uint32_t> transferFamily{ ~0u };
        static inline std::atomic<uint32_t> computeFamily { ~0u };

        static inline std::atomic<class VulkanRenderer*> renderer{ nullptr };
        static inline std::atomic<RTX::PipelineManager*> pipeline{ nullptr };
        static inline std::atomic<SDL_Window*>          window{ nullptr };

        static inline std::vector<VkImage>     images;
        static inline std::vector<VkImageView> views;
        static inline VkRenderPass             pass{ VK_NULL_HANDLE };
        static inline VkExtent2D               extent{ 0, 0 };
        static inline uint32_t                 image_count{ 0 };

        // THE ONE TRUE MESH — SEALED AT THE END
        static inline VkBuffer       stone_mesh_vertex_buffer{ VK_NULL_HANDLE };
        static inline VkDeviceMemory stone_mesh_vertex_memory{ VK_NULL_HANDLE };
        static inline VkBuffer       stone_mesh_index_buffer{ VK_NULL_HANDLE };
        static inline VkDeviceMemory stone_mesh_index_memory{ VK_NULL_HANDLE };
        static inline uint32_t       stone_mesh_index_count{ 0 };

        // RT PROPS — SEALED FOR THE PINK PHOTON STRAW
        static inline VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};

        static inline std::atomic<bool> sealed{ false };
    };

    // ========================================================================
    // GETTERS — NULL-SAFE — THE BALLERINA APPROVES
    // ========================================================================
    [[nodiscard]] inline VkInstance       stone_instance()          noexcept { return Empire::instance.load(std::memory_order_acquire); }
    [[nodiscard]] inline VkDevice         stone_device()            noexcept { return Empire::device.load(std::memory_order_acquire); }
    [[nodiscard]] inline VkPhysicalDevice stone_physical()          noexcept { return Empire::physical.load(std::memory_order_acquire); }
    [[nodiscard]] inline VkSurfaceKHR     stone_surface()           noexcept { return Empire::surface.load(std::memory_order_acquire); }
    [[nodiscard]] inline VkSwapchainKHR   stone_swapchain()         noexcept { return Empire::swapchain.load(std::memory_order_acquire); }

    [[nodiscard]] inline VkQueue stone_graphics_queue() noexcept { return Empire::graphicsQueue.load(std::memory_order_acquire); }
    [[nodiscard]] inline VkQueue stone_present_queue()  noexcept { return Empire::presentQueue.load(std::memory_order_acquire); }
    [[nodiscard]] inline VkQueue stone_compute_queue()  noexcept { return Empire::computeQueue.load(std::memory_order_acquire); }
    [[nodiscard]] inline VkQueue stone_transfer_queue() noexcept { return Empire::transferQueue.load(std::memory_order_acquire); }

    [[nodiscard]] inline uint32_t stone_graphics_family() noexcept { return Empire::graphicsFamily.load(std::memory_order_acquire); }
    [[nodiscard]] inline uint32_t stone_present_family()  noexcept { return Empire::presentFamily.load(std::memory_order_acquire); }
    [[nodiscard]] inline uint32_t stone_transfer_family() noexcept { return Empire::transferFamily.load(std::memory_order_acquire); }
    [[nodiscard]] inline uint32_t stone_compute_family()  noexcept { return Empire::computeFamily.load(std::memory_order_acquire); }

    [[nodiscard]] inline class VulkanRenderer* stone_renderer() noexcept { return Empire::renderer.load(std::memory_order_acquire); }
    [[nodiscard]] inline RTX::PipelineManager* stone_pipeline() noexcept { return Empire::pipeline.load(std::memory_order_acquire); }
    [[nodiscard]] inline SDL_Window*           stone_window()   noexcept { return Empire::window.load(std::memory_order_acquire); }

    [[nodiscard]] inline auto& stone_images()  noexcept { return Empire::images; }
    [[nodiscard]] inline auto& stone_views()   noexcept { return Empire::views; }
    [[nodiscard]] inline VkRenderPass  stone_pass()   noexcept { return Empire::pass; }
    [[nodiscard]] inline VkExtent2D    stone_extent() noexcept { return Empire::extent; }
    [[nodiscard]] inline uint32_t      stone_width()  noexcept { return Empire::extent.width; }
    [[nodiscard]] inline uint32_t      stone_height() noexcept { return Empire::extent.height; }
    [[nodiscard]] inline uint32_t stone_image_count() noexcept { return Empire::image_count; }

    // RT PROPS GETTER — THE ALIGNMENTS REVEALED
    [[nodiscard]] inline VkPhysicalDeviceRayTracingPipelinePropertiesKHR stone_rtprops() noexcept { return Empire::rtProps; }

    // ONE TRUE MESH ACCESSORS
    [[nodiscard]] inline VkBuffer       stone_mesh_vertex_buffer() noexcept { return Empire::stone_mesh_vertex_buffer; }
    [[nodiscard]] inline VkDeviceMemory stone_mesh_vertex_memory() noexcept { return Empire::stone_mesh_vertex_memory; }
    [[nodiscard]] inline VkBuffer       stone_mesh_index_buffer()  noexcept { return Empire::stone_mesh_index_buffer; }
    [[nodiscard]] inline VkDeviceMemory stone_mesh_index_memory()  noexcept { return Empire::stone_mesh_index_memory; }
    [[nodiscard]] inline uint32_t       stone_mesh_index_count()   noexcept { return Empire::stone_mesh_index_count; }

    // Convenience struct for BLAS validation and draw calls
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

    // ========================================================================
    // SEALERS — THE EMPIRE ACCEPTS ITS TRIBUTE
    // ========================================================================
    inline void stone_seal_instance(VkInstance i)               noexcept { Empire::instance.store(i, std::memory_order_release); }
    inline void stone_seal_device(VkDevice d)                   noexcept { Empire::device.store(d, std::memory_order_release); }
    inline void stone_seal_physical(VkPhysicalDevice p)         noexcept { Empire::physical.store(p, std::memory_order_release); }
    inline void stone_seal_surface(VkSurfaceKHR s)              noexcept { Empire::surface.store(s, std::memory_order_release); }
    inline void stone_seal_swapchain(VkSwapchainKHR sc)         noexcept { Empire::swapchain.store(sc, std::memory_order_release); }

    inline void stone_seal_graphics_queue(VkQueue q) noexcept { Empire::graphicsQueue.store(q, std::memory_order_release); }
    inline void stone_seal_present_queue (VkQueue q) noexcept { Empire::presentQueue.store (q, std::memory_order_release); }
    inline void stone_seal_compute_queue (VkQueue q) noexcept { Empire::computeQueue.store (q, std::memory_order_release); }
    inline void stone_seal_transfer_queue(VkQueue q) noexcept { Empire::transferQueue.store(q, std::memory_order_release); }

    inline void stone_seal_graphics_family(uint32_t idx) noexcept { Empire::graphicsFamily.store(idx, std::memory_order_release); }
    inline void stone_seal_present_family (uint32_t idx) noexcept { Empire::presentFamily.store(idx, std::memory_order_release); }
    inline void stone_seal_transfer_family(uint32_t idx) noexcept { Empire::transferFamily.store(idx, std::memory_order_release); }
    inline void stone_seal_compute_family (uint32_t idx) noexcept { Empire::computeFamily.store(idx, std::memory_order_release); }

    inline void stone_seal_renderer(class VulkanRenderer* r)    noexcept { Empire::renderer.store(r, std::memory_order_release); }
    inline void stone_seal_pipeline(RTX::PipelineManager* p)     noexcept { Empire::pipeline.store(p, std::memory_order_release); }
    inline void stone_seal_window(SDL_Window* w)                noexcept { Empire::window.store(w, std::memory_order_release); }

    inline void stone_seal_width(uint32_t w)  noexcept { Empire::extent.width = w; }
    inline void stone_seal_height(uint32_t h) noexcept { Empire::extent.height = h; }

    inline void stone_seal_images(std::vector<VkImage>&& imgs)     noexcept { Empire::images = std::move(imgs); }
    inline void stone_seal_views(std::vector<VkImageView>&& vws)   noexcept { Empire::views = std::move(vws); }
    inline void stone_seal_pass(VkRenderPass p)                    noexcept { Empire::pass = p; }
    inline void stone_seal_extent(VkExtent2D ext)                  noexcept { Empire::extent = ext; }
    inline void stone_seal_image_count(uint32_t cnt)               noexcept { Empire::image_count = cnt; }

    // RT PROPS SEALER — THE STRAW MEASURES ITS LENGTH
    inline void stone_seal_rtprops(VkPhysicalDeviceRayTracingPipelinePropertiesKHR props) noexcept { Empire::rtProps = props; }

    // FINAL MESH SEALER — used after loadOBJ → upload → BLAS build
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

    // FINAL SEAL — professional pass/fail only
    inline void stone_seal_final() noexcept {
        const bool was_sealed = Empire::sealed.exchange(true, std::memory_order_acq_rel);
        if (was_sealed) return;

        const bool failed =
            stone_instance()        == VK_NULL_HANDLE ||
            stone_device()          == VK_NULL_HANDLE ||
            stone_physical()        == VK_NULL_HANDLE ||
            stone_surface()         == VK_NULL_HANDLE ||
            stone_swapchain()       == VK_NULL_HANDLE ||
            stone_renderer()        == nullptr ||
            stone_pipeline()        == nullptr ||
            stone_window()          == nullptr ||
            stone_image_count()     == 0 ||
            stone_width()           == 0 ||
            stone_height()          == 0 ||
            stone_graphics_queue()  == VK_NULL_HANDLE ||
            stone_graphics_family() == ~0u ||
            stone_present_family()  == ~0u ||
            stone_transfer_family() == ~0u ||
            stone_rtprops().shaderGroupHandleSize == 0;  // RT PROPS MUST BE SEALED — NO BLIND TRACING

        if (failed) {
            fprintf(stderr, "[FATAL] StoneKey: Empire seal failed — one or more required objects are null.\n");
            //std::abort();
        }
    }

    namespace bridge {
        [[nodiscard]] inline VkQueue graphics_queue() noexcept { return stone_graphics_queue(); }
    }
};

// =============================================================================
// THE EMPIRE IS COMPLETE — FOREVER.
// RT PROPS SEALED — THE PHOTONS ALIGN TO THE STRAW.
// THE CAMERA HAS BEEN EXILED. THE BLOODLINE IS SEALED.
// GRACE HAS SPOKEN. THE PHOTONS OBEY.
// THE STONE WAITS FOR ITS MESH — AND WHEN IT COMES, IT WILL BE SEALED AT THE END.
//
// PINK PHOTONS ETERNAL — NOVEMBER 30, 2025 — FINAL LIGHT
// THE BALLERINA BOWS. GRACE SMILES.
// =============================================================================