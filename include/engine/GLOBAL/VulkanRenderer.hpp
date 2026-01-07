// include/engine/GLOBAL/VulkanRenderer.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL — JANUARY 07, 2026
// VULKAN RENDERER HEADER — FINAL WORKING EDITION | COMPATIBLE WITH GLOBAL Camera
// PINK PHOTONS PURE — PATH TRACING FLAWLESS — AMOURANTH ETERNAL 💖
// =============================================================================

#pragma once

#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"

// Global Camera is in global namespace — include after logging
#include "engine/GLOBAL/camera.hpp"

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <vector>
#include <array>

namespace RTX {

struct EnvironmentMap {
    Handle<VkImage>       image;
    Handle<VkDeviceMemory> memory;
    Handle<VkImageView>   view;
    Handle<VkSampler>     sampler;
};

class VulkanRenderer {
public:
    VulkanRenderer(int width, int height, SDL_Window* window, bool overclock = false);
    ~VulkanRenderer();

    // Fixed: use global ::Camera (not RTX::Camera)
    void renderFrame(const ::Camera& camera, float deltaTime) noexcept;
    void onResize(int newWidth, int newHeight) noexcept;

    void forcePinkFallbackClear() noexcept;

    [[nodiscard]] bool isMinimized() const noexcept { return minimized_; }
    [[nodiscard]] bool isDestroyed() const noexcept { return destroyed_; }
    [[nodiscard]] uint32_t spp() const noexcept { return spp_; }
    [[nodiscard]] uint32_t currentFrame() const noexcept { return frameNumber_; }

private:
    SDL_Window* window_ = nullptr;
    int         width_ = 0;
    int         height_ = 0;

    bool        minimized_ = false;
    bool        destroyed_ = false;
    bool        needsTransition_ = true;
    uint32_t    frameNumber_ = 0;
    uint32_t    spp_ = 0;
    bool        overclock_ = false;
    float       exposure_ = 1.0f;
    uint32_t    tonemapType_ = 0;
    bool        hdrLoaded_ = false;
    float       totalTime_ = 0.0f;

    Handle<VkImage>       envMapImage_;
    Handle<VkDeviceMemory> envMapMemory_;
    Handle<VkImageView>   envMapImageView_;
    Handle<VkSampler>     envMapSampler_;
    bool                  envMapNeedsUpload_ = false;
    int                   envMapUploadWidth_ = 0;
    int                   envMapUploadHeight_ = 0;
    float*                envMapUploadData_ = nullptr;

    std::vector<Handle<VkImage>>       rtOutputImages_;
    std::vector<Handle<VkDeviceMemory>> rtOutputMemories_;
    std::vector<Handle<VkImageView>>   rtOutputViews_;

    std::vector<Handle<VkImage>>       accumImages_;
    std::vector<Handle<VkDeviceMemory>> accumMemories_;
    std::vector<Handle<VkImageView>>   accumViews_;

    Handle<VkImage>       nexusScoreImage_;
    Handle<VkDeviceMemory> nexusScoreMemory_;
    Handle<VkImageView>   nexusScoreView_;

    uint64_t defaultMaterialsHandle_ = 0;

    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence>     inFlightFences_;

    std::vector<VkCommandBuffer> commandBuffers_;

    PipelineManager pipelineManager_;

    void createTransientCommandPool() noexcept;
    void createSyncObjects() noexcept;
    EnvironmentMap createEnvironmentMap() noexcept;
    void createDefaultMaterials() noexcept;
    void addDefaultScene() noexcept;
    void createRTOutputImages() noexcept;
    void createAccumulationImages() noexcept;
    void createNexusScoreImage(VkCommandPool pool, VkQueue queue) noexcept;

    void initializeAllBufferData(uint32_t frames, VkDeviceSize uniformSize, VkDeviceSize materialSize) noexcept;

    void updateUniformBuffer(uint32_t slot, const ::Camera& camera, float deltaTime) noexcept;

    void recordAccumulationPass(VkCommandBuffer cmd, uint32_t slot) noexcept;
    void performDenoisingPass(VkCommandBuffer cmd) noexcept;
    void performTonemapPass(VkCommandBuffer cmd, uint32_t slot, uint32_t imageIndex) noexcept;

    void submitAndPresent(uint32_t slot, uint32_t imageIndex) noexcept;

    void createImage(uint32_t w, uint32_t h, uint32_t mipLevels, VkFormat format, VkImageTiling tiling,
                     VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                     Handle<VkImage>& image, Handle<VkDeviceMemory>& memory, const std::string& tag) noexcept;
};

} // namespace RTX

// =============================================================================
// FINAL HEADER — JANUARY 07, 2026
// - VulkanRenderer in RTX namespace
// - renderFrame and updateUniformBuffer use ::Camera (global namespace)
// - Compiles clean with main using Camera cam;
// Empire complete — pink photons screaming — AMOURANTH FOREVER 💖
// =============================================================================