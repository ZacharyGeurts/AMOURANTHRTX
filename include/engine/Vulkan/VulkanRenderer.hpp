// include/engine/Vulkan/VulkanRenderer.hpp
#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/camera.hpp"
#include "engine/Dispose.hpp"
#include "engine/logging.hpp"  // for LOG_ERROR_CAT and Color constants

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <array>
#include <chrono>
#include <memory>
#include <vector>
#include <limits>  // For std::numeric_limits

namespace VulkanRTX {

class VulkanRenderer {
public:
    // === CONSTRUCTOR: NO RAW POINTERS ===
    VulkanRenderer(int width, int height, SDL_Window* window,
                   const std::vector<std::string>& shaderPaths,
                   std::shared_ptr<Vulkan::Context> context);

    // === OWNERSHIP TRANSFER ===
    void takeOwnership(std::unique_ptr<VulkanPipelineManager> pm,
                       std::unique_ptr<VulkanBufferManager> bm);

    // === LIFECYCLE ===
    ~VulkanRenderer();
    void renderFrame(const Camera& camera);
    void handleResize(int w, int h);
    void setRenderMode(int mode);
    void cleanup() noexcept;

    // === GETTERS ===
    const std::vector<glm::vec3>& getVertices() const { return vertices_; }
    const std::vector<uint32_t>& getIndices() const { return indices_; }

    // -----------------------------------------------------------------
    // PUBLIC ACCESSOR FOR BUFFER MANAGER (required for mesh upload)
    // -----------------------------------------------------------------
    VulkanBufferManager* getBufferManager() const {
        if (!bufferManager_) {
            LOG_ERROR_CAT("RENDERER", 
                "{}getBufferManager() called but bufferManager_ is null{}", 
                Logging::Color::CRIMSON_MAGENTA, Logging::Color::RESET);
            throw std::runtime_error("BufferManager not initialized – call takeOwnership() first");
        }
        return bufferManager_.get();
    }

    // -----------------------------------------------------------------
    // PUBLIC ACCESSOR FOR PIPELINE MANAGER (required for AS + pipeline)
    // -----------------------------------------------------------------
    VulkanPipelineManager* getPipelineManager() const {
        if (!pipelineManager_) {
            LOG_ERROR_CAT("RENDERER", 
                "{}getPipelineManager() called but pipelineManager_ is null{}", 
                Logging::Color::CRIMSON_MAGENTA, Logging::Color::RESET);
            throw std::runtime_error("PipelineManager not initialized – call takeOwnership() first");
        }
        return pipelineManager_.get();
    }

private:
    // === Resource Creation ===
    void createFramebuffers();
    void createCommandBuffers();
    void createRTOutputImages();
    void createAccumulationImages();
    void createEnvironmentMap();
    void createComputeDescriptorSets();
    void initializeAllBufferData(uint32_t maxFrames, VkDeviceSize matSize, VkDeviceSize dimSize);

    // === Updates ===
    void updateRTDescriptors();
    void updateComputeDescriptors(uint32_t i);
    void updateUniformBuffer(uint32_t i, const Camera& cam);

    // === Rendering Helpers ===
    void transitionImageLayout(VkCommandBuffer cmd, VkImage image,
                               VkImageLayout oldLayout, VkImageLayout newLayout,
                               VkPipelineStageFlags srcStage, VkPipelineStageFlags dstStage,
                               VkAccessFlags srcAccess, VkAccessFlags dstAccess,
                               VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);
    void blitRTOutputToSwapchain(VkCommandBuffer cmd, uint32_t imageIndex);

    // === MEMBERS ===
    SDL_Window* window_;
    std::shared_ptr<Vulkan::Context> context_;

    // === OWNED BY RENDERER (RAII) ===
    std::unique_ptr<VulkanPipelineManager> pipelineManager_;
    std::unique_ptr<VulkanBufferManager>   bufferManager_;

    int width_, height_;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_ = {0, 0};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkFramebuffer> framebuffers_;
    std::vector<VkCommandBuffer> commandBuffers_;

    // Ray Tracing
    std::unique_ptr<VulkanRTX> rtx_;
    VkDescriptorSet rtxDescriptorSet_ = VK_NULL_HANDLE;
    Dispose::VulkanHandle<VkDescriptorPool> descriptorPool_;

    // Compute
    std::array<VkDescriptorSet, MAX_FRAMES_IN_FLIGHT> computeDescriptorSets_{};
    VkDescriptorSetLayout computeDescriptorSetLayout_ = VK_NULL_HANDLE;

    // RT Output (ping-pong)
    std::array<Dispose::VulkanHandle<VkImage>, 2> rtOutputImages_;
    std::array<Dispose::VulkanHandle<VkDeviceMemory>, 2> rtOutputMemories_;
    std::array<Dispose::VulkanHandle<VkImageView>, 2> rtOutputViews_;

    // Accumulation (ping-pong)
    std::array<Dispose::VulkanHandle<VkImage>, 2> accumImages_;
    std::array<Dispose::VulkanHandle<VkDeviceMemory>, 2> accumMemories_;
    std::array<Dispose::VulkanHandle<VkImageView>, 2> accumViews_;

    // Per-frame buffers
    std::array<Dispose::VulkanHandle<VkBuffer>, MAX_FRAMES_IN_FLIGHT> uniformBuffers_;
    std::array<Dispose::VulkanHandle<VkDeviceMemory>, MAX_FRAMES_IN_FLIGHT> uniformBufferMemories_;
    std::array<Dispose::VulkanHandle<VkBuffer>, MAX_FRAMES_IN_FLIGHT> materialBuffers_;
    std::array<Dispose::VulkanHandle<VkDeviceMemory>, MAX_FRAMES_IN_FLIGHT> materialBufferMemory_;
    std::array<Dispose::VulkanHandle<VkBuffer>, MAX_FRAMES_IN_FLIGHT> dimensionBuffers_;
    std::array<Dispose::VulkanHandle<VkDeviceMemory>, MAX_FRAMES_IN_FLIGHT> dimensionBufferMemory_;

    // Environment Map
    Dispose::VulkanHandle<VkImage> envMapImage_;
    Dispose::VulkanHandle<VkDeviceMemory> envMapImageMemory_;
    Dispose::VulkanHandle<VkImageView> envMapImageView_;
    Dispose::VulkanHandle<VkSampler> envMapSampler_;

    // Sync
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores_{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> renderFinishedSemaphores_{};
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences_{};
    std::array<VkQueryPool, MAX_FRAMES_IN_FLIGHT> queryPools_{};

    // Pipelines
    VkPipeline rtPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout rtPipelineLayout_ = VK_NULL_HANDLE;

    // Mesh Data
    std::vector<glm::vec3> vertices_;
    std::vector<uint32_t> indices_;

    // State
    std::chrono::steady_clock::time_point lastFPSTime_;
    uint32_t currentFrame_ = 0;
    uint32_t currentRTIndex_ = 0;
    uint32_t currentAccumIndex_ = 0;
    uint32_t frameNumber_ = 0;
    bool resetAccumulation_ = true;
    glm::mat4 prevViewProj_ = glm::mat4(1.0f);
    int currentMode_ = 1;
    uint32_t framesThisSecond_ = 0;
    bool swapchainRecreating_ = false;
    bool queryReady_ = false;
    bool descriptorsUpdated_ = false;

    // FPS Metrics
    double timestampPeriod_ = 0.0;
    float avgFrameTimeMs_ = 0.0f;
    float minFrameTimeMs_ = std::numeric_limits<float>::max();
    float maxFrameTimeMs_ = 0.0f;
    float avgGpuTimeMs_ = 0.0f;
    float minGpuTimeMs_ = std::numeric_limits<float>::max();
    float maxGpuTimeMs_ = 0.0f;
};

} // namespace VulkanRTX