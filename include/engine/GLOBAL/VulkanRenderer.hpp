// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v30.49
// VULKAN RENDERER HEADER — PURE LIGHT | NO FRAMES | PEW FOREVER
// SINGLE ETERNAL DESCRIPTOR SET • FIXED CMD BUFFER RING (RESET + RE-RECORD)
// OWNS: TLAS QUERY • PIPELINE • SBT • DESCRIPTOR • UBO • HDR STORAGE • WORLD
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
    [[nodiscard]] double getLifetimeSeconds() const noexcept { return totalTime_; }

private:
    // Core members — declaration order MUST match constructor initializer list exactly
    SDL_Window*                     window_             = nullptr;
    int                             width_              = 0;
    int                             height_             = 0;

    bool                            minimized_          = false;
    bool                            destroyed_          = false;

    double                          totalTime_          = 0.0;
    std::chrono::steady_clock::time_point last_time_;

    VkSemaphore                     timelineSemaphore_  = VK_NULL_HANDLE;
    uint64_t                        currentTimelineValue_ = 0;

    std::array<VkSemaphore, 3>      acquireSemaphores_{};
    uint32_t                        currentFrame_       = 0;

    uint64_t                        defaultMaterialsHandle_ = 0;
    uint64_t                        cameraUBO_          = 0;
    VkBuffer                        cameraUBOBuffer_    = VK_NULL_HANDLE;
    VkDeviceMemory                  cameraUBOMemory_    = VK_NULL_HANDLE;

    VkCommandPool                   transientCmdPool_   = VK_NULL_HANDLE;

    VkImage                         hdrOutputImage_     = VK_NULL_HANDLE;
    VkImageView                     hdrOutputView_      = VK_NULL_HANDLE;
    VkDeviceMemory                  hdrOutputMemory_    = VK_NULL_HANDLE;

    // Fixed command buffer ring — self-disposing via reset on reuse
    static constexpr size_t         CMD_RING_SIZE       = 64;
    std::vector<VkCommandBuffer>    cmdRing_;
    size_t                          currentRingIndex_   = 0;

	PipelineManager                 pipelineManager_;

private:
    void createTransientCommandPool() noexcept;
    [[nodiscard]] VkCommandBuffer getOneTimeCommandBuffer() noexcept;
    [[nodiscard]] VkResult submitAndWaitOneTime(VkCommandBuffer cmd) noexcept;

    void transitionImageLayout(VkCommandBuffer cmd,
                               VkImage image,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout) noexcept;

    void updateGlobalDescriptorSet() noexcept;
};

} // namespace RTX

// =============================================================================
// VULKAN RENDERER HEADER — v30.49 — JANUARY 23, 2026
// Frame-free • single descriptor set • fixed cmd buffer ring (reset + re-record)
// Pure light — pew forever — empire stable — pink photons eternal
// =============================================================================