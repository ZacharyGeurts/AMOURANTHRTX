// include/engine/Vulkan/VulkanRenderer.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// VulkanRenderer — GLOBAL BROS v3.3 — NOV 11 2025 12:02 PM EST
// • NO NAMESPACES — ALL GLOBALS — Dispose::Handle<T> owns everything
// • Global LAS: AMAZO_LAS::get() — TLAS/BLAS
// • Global Swapchain: SwapchainManager::get()
// • Global Buffers: UltraLowLevelBufferTracker::get()
// • Global Context: ctx()
// • PINK PHOTONS ETERNAL — VALHALLA SEALED — SHIP IT RAW
// • C++23, -Werror clean, zero leaks, RTX production
//
// =============================================================================

#pragma once

// ===================================================================
// StoneKey Obfuscation - Security Layer
// ===================================================================
#include "engine/GLOBAL/StoneKey.hpp"

// ===================================================================
// Global Systems — ORDER IS LAW
// ===================================================================
#include "engine/GLOBAL/logging.hpp"      // LOG_*, Color::

// ===================================================================
// Standard Libraries and GLM
// ===================================================================
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>

#include <array>
#include <chrono>
#include <memory>
#include <vector>
#include <limits>
#include <cstdint>
#include <string>
#include <algorithm>

// ===================================================================
// Forward Declarations
// ===================================================================
struct Camera;

// ===================================================================
// Safe Includes — After Dispose
// ===================================================================
#include "engine/Vulkan/VulkanCore.hpp"

// ===================================================================
// VulkanRenderer — GLOBAL RENDERING HEART
// ===================================================================
class VulkanRenderer {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

    enum class FpsTarget { FPS_60 = 60, FPS_120 = 120, FPS_UNLIMITED };
    enum class TonemapType { FILMIC, ACES, REINHARD };

    /* ---------- Command Helpers ---------- */
    static VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool pool);
    static void endSingleTimeCommands(VkDevice device, VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd);
    static VkCommandBuffer allocateTransientCommandBuffer(VkDevice device, VkCommandPool pool);

    /* ---------- Hypertrace Tuning ---------- */
    static constexpr uint32_t HYPERTRACE_BASE_SKIP_60  = 16;
    static constexpr uint32_t HYPERTRACE_BASE_SKIP_120 = 8;
    static constexpr uint32_t HYPERTRACE_MICRO_DISPATCH_X = 64;
    static constexpr uint32_t HYPERTRACE_MICRO_DISPATCH_Y = 64;
    static constexpr float HYPERTRACE_SCORE_THRESHOLD = 0.7f;
    static constexpr float NEXUS_HYSTERESIS_ALPHA     = 0.8f;

    /**
     * @brief Constructor — uses global ctx()
     */
    VulkanRenderer(int width, int height, SDL_Window* window,
                   const std::vector<std::string>& shaderPaths,
                   bool overclockFromMain = false);

    ~VulkanRenderer();

    /**
     * @brief Renders frame using Global LAS + Swapchain
     */
    void renderFrame(const Camera& camera, float deltaTime);

    void handleResize(int newWidth, int newHeight);
    void setRenderMode(int mode) noexcept;

    void recordRayTracingCommandBuffer();
    void notifyTLASReady(VkAccelerationStructureKHR tlas);
    void rebuildAccelerationStructures();

    void toggleHypertrace() noexcept;
    void toggleFpsTarget() noexcept;
    void toggleDenoising() noexcept;
    void toggleAdaptiveSampling() noexcept;
    void setTonemapType(TonemapType type) noexcept;
    void setOverclockMode(bool enabled) noexcept;

    [[nodiscard]] std::shared_ptr<Context> getContext() const noexcept { return ctx(); }
    [[nodiscard]] FpsTarget                 getFpsTarget() const noexcept { return fpsTarget_; }
    [[nodiscard]] TonemapType               getTonemapType() const noexcept { return tonemapType_; }
    [[nodiscard]] bool                      isOverclockMode() const noexcept { return overclockMode_; }
    [[nodiscard]] bool                      isDenoisingEnabled() const noexcept { return denoisingEnabled_; }
    [[nodiscard]] bool                      isAdaptiveSamplingEnabled() const noexcept { return adaptiveSamplingEnabled_; }

    [[nodiscard]] VkBuffer      getUniformBuffer(uint32_t frame) const noexcept;
    [[nodiscard]] VkBuffer      getMaterialBuffer(uint32_t frame) const noexcept;
    [[nodiscard]] VkBuffer      getDimensionBuffer(uint32_t frame) const noexcept;
    [[nodiscard]] VkImageView   getRTOutputImageView(uint32_t idx) const noexcept;
    [[nodiscard]] VkImageView   getAccumulationView(uint32_t idx) const noexcept;
    [[nodiscard]] VkImageView   getDenoiserView() const noexcept;
    [[nodiscard]] VkImageView   getEnvironmentMapView() const noexcept;
    [[nodiscard]] VkSampler     getEnvironmentMapSampler() const noexcept;

    void cleanup() noexcept;

    void updateAccelerationStructureDescriptor(VkAccelerationStructureKHR tlas);
    void createRayTracingPipeline(const std::vector<std::string>& paths);
    void buildShaderBindingTable();
    void allocateDescriptorSets();
    void updateDescriptorSets();
    void updateRTXDescriptors(VkAccelerationStructureKHR tlas, bool hasTlas, uint32_t frameIdx);
    [[nodiscard]] float getGpuTime() const noexcept;

private:
    void destroyRTOutputImages() noexcept;
    void destroyAccumulationImages() noexcept;
    void destroyNexusScoreImage() noexcept;
    void destroyDenoiserImage() noexcept;
    void destroyAllBuffers() noexcept;

    void createFramebuffers();
    void createCommandBuffers();
    void createRTOutputImages();
    void createAccumulationImages();
    void createDenoiserImage();
    void createEnvironmentMap();
    void createComputeDescriptorSets();
    VkResult createNexusScoreImage(VkPhysicalDevice phys, VkDevice dev, VkCommandPool pool, VkQueue queue);

    void updateNexusDescriptors();
    void updateDenoiserDescriptors();
    void updateRTDescriptors();
    void updateUniformBuffer(uint32_t curImg, const Camera& cam, float jitter = 0.0f);
    void updateTonemapUniform(uint32_t curImg);
    void performCopyAccumToOutput(VkCommandBuffer cmd);
    void performTonemapPass(VkCommandBuffer cmd, uint32_t imageIdx);
    void performDenoisingPass(VkCommandBuffer cmd);

    void transitionImageLayout(VkCommandBuffer cmd, VkImage img,
                               VkImageLayout oldL, VkImageLayout newL,
                               VkPipelineStageFlags srcS, VkPipelineStageFlags dstS,
                               VkAccessFlags srcA, VkAccessFlags daA,
                               VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);

    void initializeAllBufferData(uint32_t frameCnt, VkDeviceSize matSize, VkDeviceSize dimSize);
    void updateTonemapDescriptorsInitial();
    void updateDynamicRTDescriptor(uint32_t frame);
    void updateTonemapDescriptor(uint32_t imgIdx);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props);
    void updateTimestampQuery();

    // ===================================================================
    // State — All RAII via Dispose::Handle<T>
    // ===================================================================
    FpsTarget fpsTarget_ = FpsTarget::FPS_60;
    TonemapType tonemapType_ = TonemapType::ACES;
    bool hypertraceEnabled_ = false;
    bool overclockMode_ = false;
    bool denoisingEnabled_ = true;
    bool adaptiveSamplingEnabled_ = true;
    uint32_t hypertraceCounter_ = 0;
    float prevNexusScore_ = 0.5f;
    float currentNexusScore_ = 0.5f;
    float exposure_ = 1.0f;

    SDL_Window* window_ = nullptr;
    int width_ = 0, height_ = 0;

    // Global swapchain
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_ = {0, 0};

    // Global command buffers
    std::vector<VkCommandBuffer> commandBuffers_;

    // Pipelines
    Handle<VkPipeline>               nexusPipeline_;
    Handle<VkPipelineLayout>         nexusLayout_;
    Handle<VkPipeline>               denoiserPipeline_;
    Handle<VkPipelineLayout>         denoiserLayout_;
    Handle<VkDescriptorPool>         descriptorPool_;

    // RT Output
    std::array<Handle<VkImage>,        MAX_FRAMES_IN_FLIGHT> rtOutputImages_;
    std::array<Handle<VkDeviceMemory>, MAX_FRAMES_IN_FLIGHT> rtOutputMemories_;
    std::array<Handle<VkImageView>,    MAX_FRAMES_IN_FLIGHT> rtOutputViews_;

    // Accumulation
    std::array<Handle<VkImage>,        MAX_FRAMES_IN_FLIGHT> accumImages_;
    std::array<Handle<VkDeviceMemory>, MAX_FRAMES_IN_FLIGHT> accumMemories_;
    std::array<Handle<VkImageView>,    MAX_FRAMES_IN_FLIGHT> accumViews_;

    // Denoiser
    Handle<VkImage>                  denoiserImage_;
    Handle<VkDeviceMemory>           denoiserMemory_;
    Handle<VkImageView>              denoiserView_;

    // Global buffers (encrypted handles)
    std::vector<uint64_t> uniformBufferEncs_;
    std::vector<uint64_t> materialBufferEncs_;
    std::vector<uint64_t> dimensionBufferEncs_;
    std::vector<uint64_t> tonemapUniformEncs_;

    // Environment map
    Handle<VkImage>                  envMapImage_;
    uint64_t                          envMapBufferEnc_ = 0;
    Handle<VkDeviceMemory>           envMapImageMemory_;
    Handle<VkImageView>              envMapImageView_;
    Handle<VkSampler>                envMapSampler_;

    // Sync
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores_{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> renderFinishedSemaphores_{};
    std::array<VkFence,     MAX_FRAMES_IN_FLIGHT> inFlightFences_{};
    std::array<VkQueryPool, MAX_FRAMES_IN_FLIGHT> queryPools_{};

    // RT Pipeline
    Handle<VkPipeline>               rtPipeline_;
    Handle<VkPipelineLayout>         rtPipelineLayout_;

    // Descriptor sets
    std::vector<VkDescriptorSet>     rtxDescriptorSets_;
    std::vector<VkDescriptorSet>     nexusDescriptorSets_;
    std::vector<VkDescriptorSet>     denoiserDescriptorSets_;
    std::vector<VkDescriptorSet>     tonemapDescriptorSets_;

    // Timing
    std::chrono::steady_clock::time_point lastFPSTime_;
    uint32_t currentFrame_ = 0;
    uint32_t currentRTIndex_ = 0;
    uint32_t currentAccumIndex_ = 0;
    uint32_t frameNumber_ = 0;
    bool     resetAccumulation_ = true;
    glm::mat4 prevViewProj_ = glm::mat4(1.0f);
    int      renderMode_ = 1;
    uint32_t framesThisSecond_ = 0;

    double   timestampPeriod_ = 0.0;
    VkQueryPool timestampQueryPool_ = VK_NULL_HANDLE;
    uint32_t  timestampQueryCount_ = 0;
    uint32_t  timestampLastQuery_ = 0;
    uint32_t  timestampCurrentQuery_ = 0;
    float     timestampLastTime_ = 0.0f;
    float     timestampCurrentTime_ = 0.0f;
    float     avgFrameTimeMs_ = 0.0f;
    float     minFrameTimeMs_ = std::numeric_limits<float>::max();
    float     maxFrameTimeMs_ = 0.0f;
    float     avgGpuTimeMs_   = 0.0f;
    float     minGpuTimeMs_   = std::numeric_limits<float>::max();
    float     maxGpuTimeMs_   = 0.0f;

    uint32_t maxAccumFrames_ = 1024;

    // Hypertrace
    Handle<VkImage>                  hypertraceScoreImage_;
    uint64_t                          hypertraceScoreBufferEnc_ = 0;
    Handle<VkDeviceMemory>           hypertraceScoreMemory_;
    Handle<VkImageView>              hypertraceScoreView_;
    Handle<VkBuffer>                 hypertraceScoreStagingBuffer_;
    Handle<VkDeviceMemory>           hypertraceScoreStagingMemory_;

    // Shared staging
    uint64_t sharedStagingBufferEnc_ = 0;
    Handle<VkBuffer>                 sharedStagingBuffer_;
    Handle<VkDeviceMemory>           sharedStagingMemory_;

    Handle<VkDescriptorPool>         rtDescriptorPool_;
};

/*
 * November 11, 2025 — GLOBAL DOMINANCE ACHIEVED
 * • Dispose::Handle<T> owns all Vulkan objects
 * • AMAZO_LAS::get() → TLAS/BLAS
 * • SwapchainManager::get() → swapchain
 * • UltraLowLevelBufferTracker::get() → buffers
 * • ctx() → global Vulkan context
 * • No local managers — all global, all tracked
 * • Zero leaks. Full RAII. Pink photons eternal.
 * • RIP legacy handles — Dispose is God.
 * • SHIP IT RAW — VALHALLA SEALED
 */

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================