// include/engine/Vulkan/VulkanRenderer.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// NEXUS 60 FPS TARGET | 120 FPS OPTION | GPU-Driven Adaptive RT
// FINAL: 60 FPS default | Toggle with 'F' key | NEXUS scales skip
// CONFORMED: Uses ::Vulkan::Context — matches VulkanRTX_Setup.hpp
// UPDATED: Added FpsTarget enum, toggleFpsTarget(), getFpsTarget()
// UPDATED: Added recordRayTracingCommandBuffer() to fix GPU freeze on first frame
// FIXED: Added public getters for ALL buffers/views/samplers used in main.cpp
// FIXED: Added rebuildAccelerationStructures() — calls updateRTX(this)
// FIXED: Added swapchainMgr_ member + set/getSwapchainManager() for integration
// UPDATED: Added destroyNexusScoreImage() + staging buffer for CPU readback
// UPDATED: Added currentNexusScore_ for real-time GPU score
// FIXED: Shared staging + fence rotation in createNexusScoreImage() → no OOM
// GROK TIP: "At 60 FPS, you're not rendering. You're orchestrating photons.
//           Every 16.6 ms, a new universe is born. And you control it."

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"

// FORWARD DECLARE BOTH CLASSES TO BREAK CIRCULAR DEPENDENCY
namespace VulkanRTX {
    class VulkanRTX;
    class VulkanPipelineManager;
    class VulkanSwapchainManager;
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

class VulkanRenderer {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

    enum class FpsTarget {
        FPS_60  = 60,
        FPS_120 = 120
    };

    static VkCommandBuffer beginSingleTimeCommands(::Vulkan::Context& context);
    void endSingleTimeCommands(::Vulkan::Context& context, VkCommandBuffer cmd);

    static constexpr uint32_t HYPERTRACE_BASE_SKIP_60  = 16;
    static constexpr uint32_t HYPERTRACE_BASE_SKIP_120 = 8;
    static constexpr uint32_t HYPERTRACE_MICRO_DISPATCH_X = 64;
    static constexpr uint32_t HYPERTRACE_MICRO_DISPATCH_Y = 64;

    static constexpr float HYPERTRACE_SCORE_THRESHOLD = 0.7f;
    static constexpr float NEXUS_HYSTERESIS_ALPHA     = 0.8f;

    VulkanRenderer(int width, int height, SDL_Window* window,
                   const std::vector<std::string>& shaderPaths,
                   std::shared_ptr<::Vulkan::Context> context);

    ~VulkanRenderer();

    void takeOwnership(std::unique_ptr<VulkanPipelineManager> pm,
                       std::unique_ptr<VulkanBufferManager> bm);

    void setSwapchainManager(std::unique_ptr<VulkanSwapchainManager> mgr);
    VulkanSwapchainManager& getSwapchainManager();

    void renderFrame(const Camera& camera, float deltaTime);
    void handleResize(int newWidth, int newHeight);
    void setRenderMode(int mode);

    void recordRayTracingCommandBuffer();
    void notifyTLASReady(VkAccelerationStructureKHR tlas);
    void rebuildAccelerationStructures();

    void toggleHypertrace();
    void toggleFpsTarget();

    [[nodiscard]] VulkanBufferManager* getBufferManager() const;
    [[nodiscard]] VulkanPipelineManager* getPipelineManager() const;
    [[nodiscard]] std::shared_ptr<::Vulkan::Context> getContext() const { return context_; }
    [[nodiscard]] VulkanRTX& getRTX() { return *rtx_; }
    [[nodiscard]] const VulkanRTX& getRTX() const { return *rtx_; }
    [[nodiscard]] FpsTarget getFpsTarget() const { return fpsTarget_; }

    [[nodiscard]] VkBuffer getUniformBuffer(uint32_t frame) const noexcept;
    [[nodiscard]] VkBuffer getMaterialBuffer(uint32_t frame) const noexcept;
    [[nodiscard]] VkBuffer getDimensionBuffer(uint32_t frame) const noexcept;
    [[nodiscard]] VkImageView getRTOutputImageView(uint32_t index) const noexcept;
    [[nodiscard]] VkImageView getAccumulationView(uint32_t index) const noexcept;
    [[nodiscard]] VkImageView getEnvironmentMapView() const noexcept;
    [[nodiscard]] VkSampler getEnvironmentMapSampler() const noexcept;

    void cleanup() noexcept;
    void updateAccelerationStructureDescriptor(VkAccelerationStructureKHR tlas);

private:
    void updateRTXDescriptors(VkAccelerationStructureKHR tlas, bool hasTlas, uint32_t frameIdx);
    void destroyRTOutputImages() noexcept;
    void destroyAccumulationImages() noexcept;
    void destroyNexusScoreImage() noexcept;
    void destroyAllBuffers() noexcept;

    void createFramebuffers();
    void createCommandBuffers();
    void createRTOutputImages();
    void createAccumulationImages();
    void createEnvironmentMap();
    void createComputeDescriptorSets();

    // FINAL: Full signature + returns VkResult
    VkResult createNexusScoreImage(VkPhysicalDevice physicalDevice,
                                   VkDevice device,
                                   VkCommandPool commandPool,
                                   VkQueue queue);

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

    void initializeAllBufferData(uint32_t frameCount, VkDeviceSize matSize, VkDeviceSize dimSize);

    void updateTonemapDescriptorsInitial();
    void updateDynamicRTDescriptor(uint32_t frame);
    void updateTonemapDescriptor(uint32_t imageIndex);

    // === NEXUS SCORE IMAGE: CORRECT MEMBER NAMES (with trailing _) ===
    Dispose::VulkanHandle<VkImage>       nexusScoreImage_;
    Dispose::VulkanHandle<VkDeviceMemory> nexusScoreMemory_;
    Dispose::VulkanHandle<VkImageView>   nexusScoreView_;

    // === HELPER: Transient command buffer allocation ===
    VkCommandBuffer allocateTransientCommandBuffer(VkDevice device, VkCommandPool pool);

    FpsTarget fpsTarget_ = FpsTarget::FPS_60;
    bool     hypertraceEnabled_ = false;
    uint32_t hypertraceCounter_ = 0;
    float    prevNexusScore_ = 0.5f;
    float    currentNexusScore_ = 0.5f;

    SDL_Window* window_;
    std::shared_ptr<::Vulkan::Context> context_;

    std::unique_ptr<VulkanPipelineManager> pipelineManager_;
    std::unique_ptr<VulkanBufferManager>   bufferManager_;
    std::unique_ptr<VulkanSwapchainManager> swapchainMgr_;

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

    VkPipeline            nexusPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout      nexusLayout_    = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> nexusDescriptorSets_;

    Dispose::VulkanHandle<VkImage>       hypertraceScoreImage_;
    Dispose::VulkanHandle<VkDeviceMemory> hypertraceScoreMemory_;
    Dispose::VulkanHandle<VkImageView>   hypertraceScoreView_;

    VkBuffer             hypertraceScoreStagingBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory       hypertraceScoreStagingMemory_ = VK_NULL_HANDLE;

    // NEW: Shared staging for burst init (4KB)
    Dispose::VulkanHandle<VkBuffer>       sharedStagingBuffer_;
    Dispose::VulkanHandle<VkDeviceMemory> sharedStagingMemory_;

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

    std::vector<Dispose::VulkanHandle<VkBuffer>> tonemapUniformBuffers_;
    std::vector<Dispose::VulkanHandle<VkDeviceMemory>> tonemapUniformMemories_;

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

    int tonemapType_ = 1;
    float exposure_ = 1.0f;
    uint32_t maxAccumFrames_ = 1024;
};

} // namespace VulkanRTX