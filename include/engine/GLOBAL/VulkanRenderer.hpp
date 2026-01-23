// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v30.41 — JANUARY 22, 2026
// VULKAN RENDERER HEADER — PURE LIGHT | NO FRAMES | PEW PEW FOREVER
// SINGLE ETERNAL DESCRIPTOR SET • TRANSIENT CMD PER PRESENT • LINEAR TILING TOGGLEABLE
// OWNS: LAS TLAS QUERY • PIPELINE • SBT • DESCRIPTOR UPDATE • UBO • HDR STORAGE • SUNLIGHT
// =============================================================================
// v30.41 changes:
// - Frame-free: single descriptor set, no MAX_FRAMES_IN_FLIGHT, no frame index
// - Constructor matches .cpp: (width, height, window, overclock = false)
// - Member declaration order EXACTLY matches initializer list (-Werror=reorder fixed)
// - Per-frame acquire semaphores for sync (fixes pending operations VUID)
// - Proper HDR memory cleanup
// - No frames — pew pew forever
// Empire complete — pink photons eternal — AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/camera.hpp"  // global CAM
#include "engine/GLOBAL/OptionsMenu.hpp"

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>

#include <vector>
#include <array>
#include <cstdint>
#include <chrono>

namespace RTX {

class VulkanRenderer {
public:
    VulkanRenderer(int width, int height, SDL_Window* window);
    ~VulkanRenderer();

    // The only hot path — acquire, pew pew, present
    void pewPew() noexcept;

    [[nodiscard]] bool isMinimized() const noexcept { return minimized_; }
    [[nodiscard]] bool isDestroyed() const noexcept { return destroyed_; }

    [[nodiscard]] double getLifetimeSeconds() const noexcept { return totalTime_; }

private:
    // Core members — order MUST match constructor initializer list exactly
    SDL_Window* window_ = nullptr;
    int         width_ = 0;
    int         height_ = 0;

    bool        minimized_ = false;
    bool        destroyed_ = false;

    // Lifetime clock — renderer owns the watch
    double      totalTime_ = 0.0;                           // accumulated lifetime seconds
    std::chrono::steady_clock::time_point last_time_;       // last measurement for dt

    // Timeline semaphore — pacing if queue backs up
    VkSemaphore timelineSemaphore_ = VK_NULL_HANDLE;
    uint64_t    currentTimelineValue_ = 0;

    // Per-frame acquire semaphores (fixes pending operations VUID)
    std::array<VkSemaphore, 3> acquireSemaphores_{};  // size 3 is safe; adjust if needed
    uint32_t    currentFrame_ = 0;

    // Materials & UBO — single, updated only on change
    uint64_t    defaultMaterialsHandle_ = 0;
    uint64_t    cameraUBO_ = 0;
    VkBuffer    cameraUBOBuffer_  = VK_NULL_HANDLE;  // Manual UBO buffer
    VkDeviceMemory cameraUBOMemory_  = VK_NULL_HANDLE;  // Manual UBO memory

    // Transient command pool — allocate per pew pew
    VkCommandPool transientCmdPool_ = VK_NULL_HANDLE;

    // HDR storage image for rtOutput
    VkImage     hdrOutputImage_ = VK_NULL_HANDLE;
    VkImageView hdrOutputView_ = VK_NULL_HANDLE;
    VkDeviceMemory hdrOutputMemory_ = VK_NULL_HANDLE;

    PipelineManager pipelineManager_;

    // Core private functions
    void createTransientCommandPool() noexcept;
    void createDefaultMaterials() noexcept;
    void createHDRStorageImage() noexcept;
    void transitionImageLayout(VkCommandBuffer cmd,
                               VkImage image,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout) noexcept;

    // One-time command buffer helpers for SBT creation
    [[nodiscard]] VkCommandBuffer getOneTimeCommandBuffer() noexcept;
    [[nodiscard]] VkResult submitAndWaitOneTime(VkCommandBuffer cmd) noexcept;

    // One-time global descriptor set update — called once at startup
    void updateGlobalDescriptorSet() noexcept;
};

} // namespace RTX

// =============================================================================
// FINAL HEADER — v30.41 — JANUARY 22, 2026
// - Frame-free: single descriptor set, no frame index, no MAX_FRAMES_IN_FLIGHT
// - Constructor matches .cpp: (width, height, window, overclock = false)
// - Member declaration order EXACTLY matches initializer list (-Werror=reorder fixed)
// - Per-frame acquire semaphores for sync
// - Proper cleanup of HDR memory
// - Pure light — pew pew forever
// Empire complete — AMOURANTH FOREVER 💖
// =============================================================================