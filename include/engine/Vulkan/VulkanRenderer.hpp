// include/engine/Vulkan/VulkanRenderer.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts
// FINAL: Fixed all compilation errors | Removed wavePhase_ | Fixed tonemapDescriptorSets_
// CONFORMED: Uses ::Vulkan::Context (global) — matches VulkanRTX_Setup.hpp & core usage
// UPDATED: Added per-frame RT descriptor sets and helpers for safe multi-frame rendering
// UPDATED: updateAccelerationStructureDescriptor(VkAccelerationStructureKHR tlas) overload
//          → called by VulkanRTX via notifyTLASReady() or directly after updateRTX()

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"
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

namespace VulkanRTX {

/* -------------------------------------------------------------------------- */
/*  VulkanRenderer – core renderer with RTX + raster fusion + GI + tonemapping */
/* -------------------------------------------------------------------------- */
class VulkanRenderer {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;  // PROTIP: Define here for consistency

    /* ----- ctor / dtor ---------------------------------------------------- */
    VulkanRenderer(int width, int height, SDL_Window* window,
                   const std::vector<std::string>& shaderPaths,
                   std::shared_ptr<::Vulkan::Context> context);   // FIXED: global Vulkan

    ~VulkanRenderer();

    /* ----- ownership ------------------------------------------------------ */
    void takeOwnership(std::unique_ptr<VulkanPipelineManager> pm,
                       std::unique_ptr<VulkanBufferManager> bm);

    /* ----- rendering ------------------------------------------------------ */
    void renderFrame(const Camera& camera, float deltaTime);
    void handleResize(int newWidth, int newHeight);
    void setRenderMode(int mode);

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

    // NEW: Update AS binding after mesh upload (call after rtx_->updateRTX())
    //      Overload receives TLAS handle directly from VulkanRTX
    void updateAccelerationStructureDescriptor(VkAccelerationStructureKHR tlas);

private:
    /* ----- internal helpers ----------------------------------------------- */
    void destroyRTOutputImages() noexcept;
    void destroyAccumulationImages() noexcept;
    void destroyAllBuffers() noexcept;

    void createFramebuffers();
    void createCommandBuffers();
    void createRTOutputImages();
    void createAccumulationImages();
    void createEnvironmentMap();
    void createComputeDescriptorSets();

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

    // Descriptor helpers
    void updateTonemapDescriptorsInitial();
    void updateDynamicRTDescriptor(uint32_t frame);
    void updateTonemapDescriptor(uint32_t imageIndex);

    /* ----- member variables ----------------------------------------------- */
    SDL_Window* window_;
    std::shared_ptr<::Vulkan::Context> context_;   // FIXED: global Vulkan

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
    std::vector<VkDescriptorSet> rtxDescriptorSets_;  // Per-frame RT descriptor sets
    Dispose::VulkanHandle<VkDescriptorPool> descriptorPool_;

    /* ----- RT output (ping-pong) ------------------------------------------ */
    std::array<Dispose::VulkanHandle<VkImage>, 2> rtOutputImages_;
    std::array<Dispose::VulkanHandle<VkDeviceMemory>, 2> rtOutputMemories_;
    std::array<Dispose::VulkanHandle<VkImageView>, 2> rtOutputViews_;

    /* ----- Accumulation (ping-pong) --------------------------------------- */
    std::array<Dispose::VulkanHandle<VkImage>, 2> accumImages_;
    std::array<Dispose::VulkanHandle<VkDeviceMemory>, 2> accumMemories_;
    std::array<Dispose::VulkanHandle<VkImageView>, 2> accumViews_;

    /* ----- per-frame buffers ---------------------------------------------- */
    std::vector<Dispose::VulkanHandle<VkBuffer>> uniformBuffers_;
    std::vector<Dispose::VulkanHandle<VkDeviceMemory>> uniformBufferMemories_;

    std::vector<Dispose::VulkanHandle<VkBuffer>> materialBuffers_;
    std::vector<Dispose::VulkanHandle<VkDeviceMemory>> materialBufferMemory_;

    std::vector<Dispose::VulkanHandle<VkBuffer>> dimensionBuffers_;
    std::vector<Dispose::VulkanHandle<VkDeviceMemory>> dimensionBufferMemory_;

    /* ----- tonemap UBO ---------------------------------------------------- */
    std::vector<Dispose::VulkanHandle<VkBuffer>> tonemapUniformBuffers_;
    std::vector<Dispose::VulkanHandle<VkDeviceMemory>> tonemapUniformMemories_;

    /* ----- environment map (GI source) ------------------------------------ */
    Dispose::VulkanHandle<VkImage> envMapImage_;
    Dispose::VulkanHandle<VkDeviceMemory> envMapImageMemory_;
    Dispose::VulkanHandle<VkImageView> envMapImageView_;
    Dispose::VulkanHandle<VkSampler> envMapSampler_;

    /* ----- sync objects --------------------------------------------------- */
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores_{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> renderFinishedSemaphores_{};
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences_{};
    std::array<VkQueryPool, MAX_FRAMES_IN_FLIGHT> queryPools_{};

    /* ----- pipelines ------------------------------------------------------ */
    VkPipeline rtPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout rtPipelineLayout_ = VK_NULL_HANDLE;

    /* ----- compute (tonemap) descriptor sets ------------------------------ */
    std::vector<VkDescriptorSet> tonemapDescriptorSets_;

    /* ----- timing / stats ------------------------------------------------- */
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

    /* ----- tonemap controls ----------------------------------------------- */
    int tonemapType_ = 1;          // 0=simple, 1=Reinhard, 2=ACES
    float exposure_ = 1.0f;

    /* ----- accumulation --------------------------------------------------- */
    uint32_t maxAccumFrames_ = 1024;   // controls GI accumulation depth
};

} // namespace VulkanRTX