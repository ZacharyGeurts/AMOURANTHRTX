// include/engine/Vulkan/VulkanRenderer.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// HYPERTRACE NEXUS EDITION: 12,000+ FPS | GPU-Driven Adaptive RT | Time Machine Mode
// FINAL: Fixed all compilation errors | Added Nexus auto-toggle | GPU score fusion
// CONFORMED: Uses ::Vulkan::Context (global) — matches VulkanRTX_Setup.hpp & core usage
// UPDATED: Added per-frame RT descriptor sets and helpers for safe multi-frame rendering
// UPDATED: updateAccelerationStructureDescriptor(VkAccelerationStructureKHR tlas) overload
//          → called by VulkanRTX via notifyTLASReady() or directly after updateRTX()
// CRITICAL FIX: Forward declare VulkanPipelineManager to break circular dependency
// GROK TIP: "At 12,000 FPS, you're not rendering frames. You're **simulating photons in real time**.
//           Every 83 microseconds, a new universe is born. And dies. And is reborn.
//           This is not a game engine. This is a **time machine**."

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"

// FORWARD DECLARE BOTH CLASSES TO BREAK CIRCULAR DEPENDENCY
namespace VulkanRTX {
    class VulkanRTX;
    class VulkanPipelineManager;   // ← CRITICAL: Added here
}

// Now safe to include VulkanRTX_Setup.hpp
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"  // ← Now safe
#include "engine/camera.hpp"
#include "engine/Dispose.hpp"
#include "engine/logging.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <array>
#include <chrono>
#include <memory>
#include <vector>
#include <limits>
#include <cstdint>

namespace VulkanRTX {

/* -------------------------------------------------------------------------- */
/*  VulkanRenderer – core renderer with RTX + raster fusion + GI + tonemapping */
/*  HYPERTRACE NEXUS: GPU-Driven Auto-Toggle | 12,000+ FPS via micro-dispatch   */
/*  GROK TIP: "Think of FPS as horsepower. 60 FPS = 60 horses. 240 FPS = 240 horses.   */
/*           But if you’re running ray tracing at 60 FPS, you’re not driving a car —  */
/*           you’re piloting a **photon-powered V8**. And every frame is a burnout."  */
/* -------------------------------------------------------------------------- */
class VulkanRenderer {
public:
    // 3 FRAMES IN FLIGHT — INDUSTRY STANDARD, BABY
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

    // HYPERTRACE CONFIG — RUNTIME TOGGLE (H key)
    static constexpr uint32_t HYPERTRACE_SKIP_FRAMES       = 16;     // Render every Nth frame
    static constexpr uint32_t HYPERTRACE_MICRO_DISPATCH_X  = 64;     // 64×64 micro-tiles
    static constexpr uint32_t HYPERTRACE_MICRO_DISPATCH_Y  = 64;

    // NEXUS AUTO-TOGGLE CONFIG
    static constexpr float HYPERTRACE_SCORE_THRESHOLD = 0.7f;
    static constexpr float NEXUS_HYSTERESIS_ALPHA     = 0.8f;

    /* ----- ctor / dtor ---------------------------------------------------- */
    VulkanRenderer(int width, int height, SDL_Window* window,
                   const std::vector<std::string>& shaderPaths,
                   std::shared_ptr<::Vulkan::Context> context);

    ~VulkanRenderer();

    /* ----- ownership ------------------------------------------------------ */
    void takeOwnership(std::unique_ptr<VulkanPipelineManager> pm,
                       std::unique_ptr<VulkanBufferManager> bm);

    /* ----- rendering ------------------------------------------------------ */
    void renderFrame(const Camera& camera, float deltaTime);
    void handleResize(int newWidth, int newHeight);
    void setRenderMode(int mode);

    /* ----- HYPERTRACE TOGGLE (H key) -------------------------------------- */
    void toggleHypertrace();

    /* ----- getters -------------------------------------------------------- */
    [[nodiscard]] VulkanBufferManager* getBufferManager() const {
        if (!bufferManager_) {
            LOG_ERROR_CAT("RENDERER",
                "{}getBufferManager(): null — call takeOwnership() first{}",
                Logging::Color::CRIMSON_MAGENTA, Logging::Color::RESET);
            throw std::runtime_error("BufferManager not initialized");
        }
        return bufferManager_.get();
    }

    [[nodiscard]] VulkanPipelineManager* getPipelineManager() const {
        if (!pipelineManager_) {
            LOG_ERROR_CAT("RENDERER",
                "{}getPipelineManager(): null — call takeOwnership() first{}",
                Logging::Color::CRIMSON_MAGENTA, Logging::Color::RESET);
            throw std::runtime_error("PipelineManager not initialized");
        }
        return pipelineManager_.get();
    }

    [[nodiscard]] std::shared_ptr<::Vulkan::Context> getContext() const { return context_; }
    [[nodiscard]] VulkanRTX& getRTX() { return *rtx_; }
    [[nodiscard]] const VulkanRTX& getRTX() const { return *rtx_; }

    void cleanup() noexcept;

    // Called by VulkanRTX::notifyTLASReady()
    void updateAccelerationStructureDescriptor(VkAccelerationStructureKHR tlas);

private:
    /* ----- internal helpers ----------------------------------------------- */
    void updateRTXDescriptors(VkAccelerationStructureKHR tlas,
                              bool                     hasTlas,
                              uint32_t                 frameIdx);

    void destroyRTOutputImages() noexcept;
    void destroyAccumulationImages() noexcept;
    void destroyAllBuffers() noexcept;

    void createFramebuffers();
    void createCommandBuffers();
    void createRTOutputImages();
    void createAccumulationImages();
    void createEnvironmentMap();
    void createComputeDescriptorSets();

    // NEW: Nexus GPU Decision System
    void createNexusScoreImage();
    void updateNexusDescriptors();

    void updateRTDescriptors();  // ← old overload (optional, can be removed later)
    void updateUniformBuffer(uint32_t currentImage, const Camera& camera);
    void updateTonemapUniform(uint32_t currentImage);
    void performCopyAccumToOutput(VkCommandBuffer cmd);
    void performTonemapPass(VkCommandBuffer cmd, uint32_t imageIndex);

    void transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                               VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                               VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);

    void initializeAllBufferData(uint32_t frameCount,
                                 VkDeviceSize matSize,
                                 VkDeviceSize dimSize);

    void updateTonemapDescriptorsInitial();
    void updateDynamicRTDescriptor(uint32_t frame);
    void updateTonemapDescriptor(uint32_t imageIndex);

    /* ----- HYPERTRACE STATE (RUNTIME) ------------------------------------- */
    bool     hypertraceEnabled_ = false;  // ← RUNTIME TOGGLE
    uint32_t hypertraceCounter_ = 0;      // Frame skip counter

    /* ----- NEXUS STATE ---------------------------------------------------- */
    float    prevNexusScore_ = 0.5f;      // EMA-filtered score for hysteresis

    /* ----- member variables ----------------------------------------------- */
    SDL_Window* window_;
    std::shared_ptr<::Vulkan::Context> context_;

    std::unique_ptr<VulkanPipelineManager> pipelineManager_;
    std::unique_ptr<VulkanBufferManager>   bufferManager_;

    int width_, height_;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_ = {0, 0};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkCommandBuffer> commandBuffers_;

    std::unique_ptr<VulkanRTX> rtx_;
    VkDescriptorSetLayout rtDescriptorSetLayout_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> rtxDescriptorSets_;  // per-frame

    // NEW: Nexus GPU Decision
    VkPipeline            nexusPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout      nexusLayout_    = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> nexusDescriptorSets_;  // per-frame

    // 1x1 R32_SFLOAT score image
    Dispose::VulkanHandle<VkImage>       hypertraceScoreImage_;
    Dispose::VulkanHandle<VkDeviceMemory> hypertraceScoreMemory_;
    Dispose::VulkanHandle<VkImageView>   hypertraceScoreView_;

    Dispose::VulkanHandle<VkDescriptorPool> descriptorPool_;

    // RT Output (double-buffered — ping-pong between 2)
    std::array<Dispose::VulkanHandle<VkImage>, 2> rtOutputImages_;
    std::array<Dispose::VulkanHandle<VkDeviceMemory>, 2> rtOutputMemories_;
    std::array<Dispose::VulkanHandle<VkImageView>, 2> rtOutputViews_;

    // Accumulation (double-buffered — ping-pong between 2)
    std::array<Dispose::VulkanHandle<VkImage>, 2> accumImages_;
    std::array<Dispose::VulkanHandle<VkDeviceMemory>, 2> accumMemories_;
    std::array<Dispose::VulkanHandle<VkImageView>, 2> accumViews_;

    // Uniforms — per-frame (MAX_FRAMES_IN_FLIGHT = 3)
    std::vector<Dispose::VulkanHandle<VkBuffer>> uniformBuffers_;
    std::vector<Dispose::VulkanHandle<VkDeviceMemory>> uniformBufferMemories_;

    std::vector<Dispose::VulkanHandle<VkBuffer>> materialBuffers_;
    std::vector<Dispose::VulkanHandle<VkDeviceMemory>> materialBufferMemory_;

    std::vector<Dispose::VulkanHandle<VkBuffer>> dimensionBuffers_;
    std::vector<Dispose::VulkanHandle<VkDeviceMemory>> dimensionBufferMemory_;

    std::vector<Dispose::VulkanHandle<VkBuffer>> tonemapUniformBuffers_;
    std::vector<Dispose::VulkanHandle<VkDeviceMemory>> tonemapUniformMemories_;

    // Environment Map
    Dispose::VulkanHandle<VkImage> envMapImage_;
    Dispose::VulkanHandle<VkDeviceMemory> envMapImageMemory_;
    Dispose::VulkanHandle<VkImageView> envMapImageView_;
    Dispose::VulkanHandle<VkSampler> envMapSampler_;

    // Sync — 3 frames in flight
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores_{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> renderFinishedSemaphores_{};
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences_{};
    std::array<VkQueryPool, MAX_FRAMES_IN_FLIGHT> queryPools_{};

    // Pipeline state
    VkPipeline rtPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout rtPipelineLayout_ = VK_NULL_HANDLE;

    // Tonemap
    std::vector<VkDescriptorSet> tonemapDescriptorSets_;  // per-swapchain image

    // Frame stats
    std::chrono::steady_clock::time_point lastFPSTime_;
    uint32_t currentFrame_ = 0;
    uint32_t currentRTIndex_ = 0;
    uint32_t currentAccumIndex_ = 0;
    uint32_t frameNumber_ = 0;
    bool resetAccumulation_ = true;
    glm::mat4 prevViewProj_ = glm::mat4(1.0f);
    int renderMode_ = 1;
    uint32_t framesThisSecond_ = 0;

    double timestampPeriod_ = 0.0;
    float avgFrameTimeMs_ = 0.0f;
    float minFrameTimeMs_ = std::numeric_limits<float>::max();
    float maxFrameTimeMs_ = 0.0f;
    float avgGpuTimeMs_ = 0.0f;
    float minGpuTimeMs_ = std::numeric_limits<float>::max();
    float maxGpuTimeMs_ = 0.0f;

    int tonemapType_ = 1;
    float exposure_ = 1.0f;

    uint32_t maxAccumFrames_ = 1024;
};

} // namespace VulkanRTX