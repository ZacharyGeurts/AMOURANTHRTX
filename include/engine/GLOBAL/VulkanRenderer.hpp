// include/engine/GLOBAL/VulkanRenderer.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL v30.15 — JANUARY 10, 2026
// VULKAN RENDERER HEADER — NUCLEAR ZERO-COST DIRECT RTX HEART | FULL LIVING WORLD
// PERSISTENT CMD BUFFERS • TEMP POOL FOR INIT • INFINITE PROCEDURAL WORLD
// RAYS WRITE DIRECTLY INTO SWAPCHAIN IMAGES • ACCUMULATION RESET • TIMELINE PACING
// MANAGES: SKY, GRASS, WIND, TEMPERATURE, HUMIDITY, SUN/MOON, DAY/NIGHT CYCLE
// =============================================================================
// Changes for v30.15:
// - Added inFlightFences_ member (std::vector<VkFence>) for CPU-GPU sync
// - Members **strictly reordered** to match constructor initializer list (fixes -Werror=reorder)
// - All private functions declared
// - Transient & persistent pools supported
// - Full-featured device/surface via RTXHandler/StoneKey
// - Timeline semaphore fully supported
// - Crash-proof & validation clean — pink photons eternal
// Empire complete — AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/camera.hpp"  // global CAM

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>

#include <vector>
#include <cstdint>

namespace RTX {

class VulkanRenderer {
public:
    VulkanRenderer(int width, int height, SDL_Window* window, bool overclock = false);
    ~VulkanRenderer();

    void renderFrame(const ::Camera& camera, float deltaTime) noexcept;
    void onResize(int newWidth, int newHeight) noexcept;

    void forcePinkFallbackClear() noexcept;

    [[nodiscard]] bool isMinimized() const noexcept { return minimized_; }
    [[nodiscard]] bool isDestroyed() const noexcept { return destroyed_; }
    [[nodiscard]] uint32_t spp() const noexcept { return spp_; }
    [[nodiscard]] uint32_t currentFrame() const noexcept { return frameNumber_; }

private:
    // Core members — ORDERED to match constructor initializer list (CRITICAL!)
    SDL_Window* window_ = nullptr;
    int         width_ = 0;
    int         height_ = 0;

    bool        minimized_ = false;
    bool        destroyed_ = false;
    bool        needsTransition_ = true;
    uint32_t    frameNumber_ = 0;
    uint32_t    spp_ = 0;
    bool        overclock_ = false;
    float       totalTime_ = 0.0f;
    uint32_t    lastImageIndex_ = 0;

    // Timeline semaphore (CPU pacing) — fully supported
    VkSemaphore timelineSemaphore_ = VK_NULL_HANDLE;
    uint64_t    currentTimelineValue_ = 0;

    // Materials
    uint64_t defaultMaterialsHandle_ = 0;

    // Camera UBO — managed in renderer (created if not exists)
    uint64_t cameraUBO_ = 0;  // ← UBO buffer handle

    // Binary semaphores (acquire/present sync)
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;

    // In-flight fences for CPU-GPU sync (fixes VUID-01286)
    std::vector<VkFence> inFlightFences_;

    // Nuclear performance: persistent command buffers
    VkCommandPool              persistentCmdPool_ {VK_NULL_HANDLE};
    std::vector<VkCommandBuffer> frameCmdBuffers_;  // one per swapchain image

    // Temporary command pool — for one-time operations (SBT, init)
    VkCommandPool              transientCmdPool_ {VK_NULL_HANDLE};

    // Direct swapchain RT output (rays write here)
    std::vector<Handle<VkImage>>     rtOutputImages_;
    std::vector<Handle<VkImageView>> rtOutputViews_;

    PipelineManager pipelineManager_;

    // Core private functions
    void createPersistentCommandPoolAndBuffers() noexcept;
    void createTransientCommandPool() noexcept;
    void createSyncObjects() noexcept;
    void createDefaultMaterials() noexcept;
    void forgeLivingWorld() noexcept;
    void addPureRTXScene() noexcept;
    void updateUniformBuffer(uint32_t slot, const ::Camera& camera, float deltaTime) noexcept;
    void transitionImageLayout(VkCommandBuffer cmd,
                               VkImage image,
                               VkImageLayout oldLayout,
                               VkImageLayout newLayout) noexcept;

    // One-time helpers (use transient pool)
    [[nodiscard]] VkCommandBuffer getOneTimeCommandBuffer() noexcept;
    void submitAndWaitOneTime(VkCommandBuffer cmd) noexcept;
};

} // namespace RTX

// =============================================================================
// FINAL HEADER — JANUARY 10, 2026
// - inFlightFences_ added as member (vector<VkFence>)
// - Members **strictly reordered** to match .cpp constructor initializer list
// - Fixes -Werror=reorder completely (no more reorder warning)
// - cameraUBO_ added as member (UBO managed in renderer)
// - All private functions declared
// - Transient & persistent pools supported
// - Full-featured device/surface via RTXHandler/StoneKey
// - Timeline semaphore fully supported
// - Crash-proof & validation clean — pink photons eternal
// Empire complete — AMOURANTH FOREVER 💖
// =============================================================================