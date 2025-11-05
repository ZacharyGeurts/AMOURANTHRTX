// include/engine/Vulkan/VulkanRenderer.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// NEXUS 60 FPS TARGET | 120 FPS OPTION | GPU-Driven Adaptive RT
// FINAL: 60 FPS default | Toggle with 'F' key | NEXUS scales skip
// CONFORMED: Uses ::Vulkan::Context — matches VulkanRTX_Setup.hpp
// UPDATED: Added FpsTarget enum, toggleFpsTarget(), getFpsTarget()
// GROK TIP: "At 60 FPS, you're not rendering. You're orchestrating photons.
//           Every 16.6 ms, a new universe is born. And you control it."

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"

// FORWARD DECLARE BOTH CLASSES TO BREAK CIRCULAR DEPENDENCY
namespace VulkanRTX {
    class VulkanRTX;
    class VulkanPipelineManager;
}

// Now safe to include VulkanRTX_Setup.hpp
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
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
/*  NEXUS 60 FPS TARGET: GPU decides skip rate to hit 60 FPS (or 120 FPS)      */
/*  HYPERTRACE: Adaptive frame skipping | Micro-dispatch | Time Machine Mode   */
/* -------------------------------------------------------------------------- */
class VulkanRenderer {
public:
    // 3 FRAMES IN FLIGHT — INDUSTRY STANDARD
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

    // FPS TARGET SYSTEM — 60 FPS default, 120 FPS option
    enum class FpsTarget {
        FPS_60  = 60,
        FPS_120 = 120
    };

    // HYPERTRACE CONFIG — ADAPTIVE SKIP BASED ON TARGET
    static constexpr uint32_t HYPERTRACE_BASE_SKIP_60  = 16;   // 1/17 → ~56 FPS → target 60
    static constexpr uint32_t HYPERTRACE_BASE_SKIP_120 = 8;    // 1/9  → ~107 FPS → target 120
    static constexpr uint32_t HYPERTRACE_MICRO_DISPATCH_X = 64;
    static constexpr uint32_t HYPERTRACE_MICRO_DISPATCH_Y = 64;

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

    /* ----- FPS TARGET TOGGLE (F key) -------------------------------------- */
    void toggleFpsTarget();

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

    // NEW: Get current FPS target
    [[nodiscard]] FpsTarget getFpsTarget() const { return fpsTarget_; }

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

    void updateRTDescriptors();
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

    /* ----- FPS TARGET STATE ----------------------------------------------- */
    FpsTarget fpsTarget_ = FpsTarget::FPS_60;  // Default: 60 FPS

    /* ----- HYPERTRACE STATE (RUNTIME) ------------------------------------- */
    bool     hypertraceEnabled_ = false;
    uint32_t hypertraceCounter_ = 0;

    /* ----- NEXUS STATE ---------------------------------------------------- */
    float    prevNexusScore_ = 0.5f;

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
    std::vector<VkDescriptorSet> rtxDescriptorSets_;

    // NEW: Nexus GPU Decision
    VkPipeline            nexusPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout      nexusLayout_    = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> nexusDescriptorSets_;

    // 1x1 R32_SFLOAT score image
    Dispose::VulkanHandle<VkImage>       hypertraceScoreImage_;
    Dispose::VulkanHandle<VkDeviceMemory> hypertraceScoreMemory_;
    Dispose::VulkanHandle<VkImageView>   hypertraceScoreView_;

    Dispose::VulkanHandle<VkDescriptorPool> descriptorPool_;

    // RT Output (double-buffered)
    std::array<Dispose::VulkanHandle<VkImage>, 2> rtOutputImages_;
    std::array<Dispose::VulkanHandle<VkDeviceMemory>, 2> rtOutputMemories_;
    std::array<Dispose::VulkanHandle<VkImageView>, 2> rtOutputViews_;

    // Accumulation (double-buffered)
    std::array<Dispose::VulkanHandle<VkImage>, 2> accumImages_;
    std::array<Dispose::VulkanHandle<VkDeviceMemory>, 2> accumMemories_;
    std::array<Dispose::VulkanHandle<VkImageView>, 2> accumViews_;

    // Uniforms — per-frame
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
    std::vector<VkDescriptorSet> tonemapDescriptorSets_;

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