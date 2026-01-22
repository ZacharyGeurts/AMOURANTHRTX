// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v30.30 — JANUARY 21, 2026
// VULKAN RENDERER HEADER — PURE LIGHT | NO FRAMES | PEW PEW FOREVER
// DIRECT BEAMS TO SWAPCHAIN • SINGLE SET • TRANSIENT CMD PER PRESENT
// OWNS: LAS TLAS QUERY • PIPELINE • SBT • DESCRIPTOR UPDATE • UBO • HDR STORAGE • SUNLIGHT
// =============================================================================
// v30.30 changes:
// - Added hdrOutputImage_ and hdrOutputView_ members (fixes field errors)
// - Added HDR creation/copy helpers
// - Member order EXACTLY matches constructor initializer list (-Werror=reorder fixed)
// - Renderer owns lifetime clock (double totalTime_ + steady_clock)
// - Transient cmd per present — zero persistent state
// Empire complete — AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/camera.hpp"  // global CAM

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>

#include <vector>
#include <cstdint>
#include <chrono>

namespace RTX {

class VulkanRenderer {
public:
    VulkanRenderer(int width, int height, SDL_Window* window, bool overclock = false);
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

    // Materials & UBO — single, updated only on change
    uint64_t defaultMaterialsHandle_ = 0;
    uint64_t cameraUBO_ = 0;

    // Transient command pool — allocate per pew pew
    VkCommandPool transientCmdPool_ = VK_NULL_HANDLE;

    // HDR storage image for rtOutput (fixes format mismatch)
    VkImage     hdrOutputImage_ = VK_NULL_HANDLE;
    VkImageView hdrOutputView_ = VK_NULL_HANDLE;

    PipelineManager pipelineManager_;

    // Core private functions
    void createTransientCommandPool() noexcept;
    void createDefaultMaterials() noexcept;
    void createHDRStorageImage() noexcept;
    void copyHDRToSwapchain(VkCommandBuffer cmd, uint32_t imageIndex) noexcept;
    void forgeLivingWorld() noexcept;
    void updateUniformBuffer(const ::Camera& camera) noexcept;
    void transitionImageLayout(VkCommandBuffer cmd,
                               VkImage image,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout) noexcept;

    // One-time command buffer helpers for SBT creation
    [[nodiscard]] VkCommandBuffer getOneTimeCommandBuffer() noexcept;
    void submitAndWaitOneTime(VkCommandBuffer cmd) noexcept;

    // One-time global descriptor set update — called once at startup
    void updateGlobalDescriptorSet() noexcept;
};

} // namespace RTX

// =============================================================================
// FINAL HEADER — v30.30 — JANUARY 21, 2026
// - Added HDR members and helpers
// - Fixed field declaration errors
// - Member order matches constructor initializer list
// - Transient cmd per present — zero persistent state
// - Pure light — pew pew forever
// Empire complete — AMOURANTH FOREVER 💖
// =============================================================================