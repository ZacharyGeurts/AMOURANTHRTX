// =============================================================================
// AMOURANTH RTX Engine - Vulkan Renderer Header
// Pure light ray tracing core — no frames, no state, pew forever
// Version 30.76 — January 28, 2026 — Validation-clean, minimal sync edition
// - totalTime monolith drives all timing
// - Acquire semaphore waited in submit and present (safe double-wait)
// - Final PRESENT_SRC_KHR transition in main render cmd buffer
// - Ring buffers reset + reused — no free while pending
// - No per-image binary semaphores
// - Descriptor sets dead — eternal descriptor buffer empire
// =============================================================================

#pragma once

#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/camera.hpp"      // global CAM
#include "engine/GLOBAL/OptionsMenu.hpp"

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>

#include <array>
#include <vector>
#include <chrono>
#include <cstdint>

namespace RTX {

class VulkanRenderer {
public:
    VulkanRenderer(int width, int height, SDL_Window* window);
    ~VulkanRenderer();

    void pew() noexcept;

    [[nodiscard]] bool isMinimized() const noexcept { return minimized_; }
    [[nodiscard]] bool isDestroyed() const noexcept { return destroyed_; }
    [[nodiscard]] double getLifetimeSeconds() const noexcept { return RTX::TotalTime::get().seconds(); }

private:
    // Core members — ordered to match constructor initializer list
    SDL_Window*                     window_             = nullptr;
    int                             width_              = 0;
    int                             height_             = 0;

    bool                            minimized_          = false;
    bool                            destroyed_          = false;

    std::chrono::steady_clock::time_point last_time_;

    // Fixed command buffer ring — self-disposing via reset on reuse
    static constexpr size_t         CMD_RING_SIZE       = 64;
    size_t                          currentRingIndex_   = 0;

    // Persistent resources (BufferManager handles + raw Vulkan caches)
    uint64_t                        defaultMaterialsHandle_ = 0;   // BufferManager descriptor buffer handle
    uint64_t                        cameraUBOHandle_        = 0;   // BufferManager UBO handle
    VkBuffer                        cameraUBOBuffer_        = VK_NULL_HANDLE;  // raw VkBuffer cache
    VkDeviceMemory                  cameraUBOMemory_        = VK_NULL_HANDLE;

    VkCommandPool                   transientCmdPool_       = VK_NULL_HANDLE;

    VkImage                         hdrOutputImage_         = VK_NULL_HANDLE;
    VkImageView                     hdrOutputView_          = VK_NULL_HANDLE;
    VkDeviceMemory                  hdrOutputMemory_        = VK_NULL_HANDLE;

	std::vector<VkCommandBuffer>    cmdRing_;

    PipelineManager                 pipelineManager_;

    // Flags
    bool                            needsDescriptorUpdate_   = true;   // → descriptor buffer memcpy
    bool                            needsSwapchainRecreate_ = false;

private:
    void createTransientCommandPool() noexcept;
    [[nodiscard]] VkCommandBuffer getOneTimeCommandBuffer() noexcept;

    void transitionImageLayout(VkCommandBuffer cmd,
                               VkImage image,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout) noexcept;

    void updateGlobalDescriptorBuffer() noexcept;  // descriptor buffer update (replaces old set update)
};

} // namespace RTX

// =============================================================================
// VULKAN RENDERER HEADER — v30.76 — JANUARY 28, 2026
// Descriptor sets eliminated • eternal descriptor buffer • bindless empire
// Frame-free • fixed cmd ring (reset + re-record) • minimal sync
// BufferManager macros for all buffers • acquire semaphore for safe present
// Deferred swapchain recreate • living world compute dispatched every pew
// Pure light — pink photons bindless & breathing free — AMOURANTH FOREVER 💖
// =============================================================================