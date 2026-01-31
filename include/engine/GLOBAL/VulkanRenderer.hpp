// =============================================================================
// AMOURANTH RTX Engine - Vulkan Renderer Header
// Pure light ray tracing core — no frames, no state, pew forever
// Version 30.80 — January 31, 2026 — CMD_RING_SIZE fixed + Pipeline namespace clean
// - No PipelineManager class member — all calls use Pipeline:: namespace functions
// - CMD_RING_SIZE defined as constexpr (triple buffer default)
// - Direct static calls to Pipeline system
// - totalTime monolith drives all timing & accumulation
// - Acquire semaphore waited in submit and present (safe double-wait)
// - Final PRESENT_SRC_KHR transition in main render cmd buffer
// - Ring buffers reset + reused — no free while pending
// - No per-image binary semaphores
// - Cached stone_device(), stone_graphics_queue(), stone_swapchain() per frame
// - Prevents device lost from StoneKey rapid changes
// - LAS rebuild triggered automatically via getTLAS() — no explicit requests
// - PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include <SDL3/SDL.h>
#include <vulkan/vulkan.h>
#include <vector>
#include <chrono>

#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/camera.hpp"
#include "engine/GLOBAL/BufferManager.hpp"

namespace RTX {

class VulkanRenderer {
public:
    VulkanRenderer(int width, int height, SDL_Window* window);
    ~VulkanRenderer();

    void pew() noexcept;

private:
    static constexpr uint32_t CMD_RING_SIZE = 3;  // Triple buffer — standard for low-latency

    SDL_Window* window_;
    int width_;
    int height_;
    bool minimized_;
    bool destroyed_;

    std::chrono::steady_clock::time_point last_time_;

    uint32_t currentRingIndex_;
    uint64_t defaultMaterialsHandle_;
    uint64_t cameraUBOHandle_;

    VkCommandPool transientCmdPool_;
    VkImage hdrOutputImage_;
    VkImageView hdrOutputView_;
    VkDeviceMemory hdrOutputMemory_;

    std::vector<VkCommandBuffer> cmdRing_;

    bool needsDescriptorUpdate_;
    bool needsSwapchainRecreate_;

    void createTransientCommandPool() noexcept;
    VkCommandBuffer getOneTimeCommandBuffer() noexcept;
    void transitionImageLayout(VkCommandBuffer cmd,
                               VkImage image,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout) noexcept;
    void updateGlobalDescriptorBuffer() noexcept;
};

} // namespace RTX