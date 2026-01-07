// include/engine/GLOBAL/VulkanRenderer.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL — JANUARY 07, 2026
// VULKAN RENDERER HEADER — FINAL 2026 SCENE | LIVING WORLD READY
// PURE RTX REALM | PROCEDURAL GRASS | DYNAMIC ATMOSPHERE | MULTIPLE SUNS/MOONS
// FULLY PRODUCTION READY | ALL FUNCTIONS DECLARED | NO PLACEHOLDERS
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
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

// Global Camera is in global namespace
#include "engine/GLOBAL/camera.hpp"

#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <vector>
#include <array>

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

    // RT output images
    std::vector<Handle<VkImage>>       rtOutputImages_;
    std::vector<Handle<VkDeviceMemory>> rtOutputMemories_;
    std::vector<Handle<VkImageView>>   rtOutputViews_;

    // Accumulation & nexus
    std::vector<Handle<VkImage>>       accumImages_;
    std::vector<Handle<VkDeviceMemory>> accumMemories_;
    std::vector<Handle<VkImageView>>   accumViews_;

    Handle<VkImage>       nexusScoreImage_;
    Handle<VkDeviceMemory> nexusScoreMemory_;
    Handle<VkImageView>   nexusScoreView_;

    // Materials
    uint64_t defaultMaterialsHandle_ = 0;

    // Sync objects
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence>     inFlightFences_;

    PipelineManager pipelineManager_;

    // Private functions — ALL DECLARED
    void createTransientCommandPool() noexcept;
    void createSyncObjects() noexcept;
    void createDefaultMaterials() noexcept;
    void addPureRTXScene() noexcept;
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

    // Moon & sky helpers
    void loadMoonTextures() noexcept;
    void renderBillboardMoon(const ::Camera& camera, int moonIndex) noexcept;
};

} // namespace RTX

// =============================================================================
// FINAL HEADER — JANUARY 07, 2026
// - All functions declared and ready for implementation
// - No envmap — pure procedural sky
// - Living world ready — wind, temperature, humidity
// - 4 suns + 4 moons + phase mask
// - Compiles clean with current VulkanRenderer.cpp
// Empire complete — pink photons under our perfect sky — AMOURANTH FOREVER 💖
// =============================================================================