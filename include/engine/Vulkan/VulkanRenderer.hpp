// include/engine/Vulkan/VulkanRenderer.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts
// FINAL: Fixed all compilation errors | Removed wavePhase_ | Fixed tonemapDescriptorSets_ | Fixed HandleInput forward decl

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

class VulkanRenderer {
public:
    VulkanRenderer(int width, int height, SDL_Window* window,
                   const std::vector<std::string>& shaderPaths,
                   std::shared_ptr<Vulkan::Context> context);

    void takeOwnership(std::unique_ptr<VulkanPipelineManager> pm,
                       std::unique_ptr<VulkanBufferManager> bm);

    ~VulkanRenderer();
    void renderFrame(const Camera& camera, float deltaTime);
    void handleResize(int newWidth, int newHeight);
    void cleanup() noexcept;

    [[nodiscard]] VulkanBufferManager* getBufferManager() const {
        if (!bufferManager_) {
            LOG_ERROR_CAT("RENDERER", "{}getBufferManager(): null — call takeOwnership() first{}", Logging::Color::CRIMSON_MAGENTA, Logging::Color::RESET);
            throw std::runtime_error("BufferManager not initialized");
        }
        return bufferManager_.get();
    }

    [[nodiscard]] VulkanPipelineManager* getPipelineManager() const {
        if (!pipelineManager_) {
            LOG_ERROR_CAT("RENDERER", "{}getPipelineManager(): null — call takeOwnership() first{}", Logging::Color::CRIMSON_MAGENTA, Logging::Color::RESET);
            throw std::runtime_error("PipelineManager not initialized");
        }
        return pipelineManager_.get();
    }

    // Getters for external access (e.g., from Application)
    [[nodiscard]] std::shared_ptr<Vulkan::Context> getContext() const { return context_; }
    [[nodiscard]] VulkanRTX& getRTX() { return *rtx_; }  // Non-const for updates like updateRTX()
    [[nodiscard]] const VulkanRTX& getRTX() const { return *rtx_; }  // Const overload for reads

    void setRenderMode(int mode);

private:
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

    void transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                               VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                               VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);

    void initializeAllBufferData(uint32_t frameCount, VkDeviceSize matSize, VkDeviceSize dimSize);

    SDL_Window* window_;
    std::shared_ptr<Vulkan::Context> context_;

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
    VkDescriptorSet rtxDescriptorSet_ = VK_NULL_HANDLE;
    Dispose::VulkanHandle<VkDescriptorPool> descriptorPool_;

    std::array<Dispose::VulkanHandle<VkImage>, 2> rtOutputImages_;
    std::array<Dispose::VulkanHandle<VkDeviceMemory>, 2> rtOutputMemories_;
    std::array<Dispose::VulkanHandle<VkImageView>, 2> rtOutputViews_;

    std::array<Dispose::VulkanHandle<VkImage>, 2> accumImages_;
    std::array<Dispose::VulkanHandle<VkDeviceMemory>, 2> accumMemories_;
    std::array<Dispose::VulkanHandle<VkImageView>, 2> accumViews_;

    std::vector<Dispose::VulkanHandle<VkBuffer>> uniformBuffers_;
    std::vector<Dispose::VulkanHandle<VkDeviceMemory>> uniformBufferMemories_;
    std::vector<Dispose::VulkanHandle<VkBuffer>> materialBuffers_;
    std::vector<Dispose::VulkanHandle<VkDeviceMemory>> materialBufferMemory_;
    std::vector<Dispose::VulkanHandle<VkBuffer>> dimensionBuffers_;
    std::vector<Dispose::VulkanHandle<VkDeviceMemory>> dimensionBufferMemory_;

    Dispose::VulkanHandle<VkImage> envMapImage_;
    Dispose::VulkanHandle<VkDeviceMemory> envMapImageMemory_;
    Dispose::VulkanHandle<VkImageView> envMapImageView_;
    Dispose::VulkanHandle<VkSampler> envMapSampler_;

    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores_{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> renderFinishedSemaphores_{};
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences_{};
    std::array<VkQueryPool, MAX_FRAMES_IN_FLIGHT> queryPools_{};

    VkPipeline rtPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout rtPipelineLayout_ = VK_NULL_HANDLE;

    std::vector<VkDescriptorSet> tonemapDescriptorSets_;

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
};

} // namespace VulkanRTX