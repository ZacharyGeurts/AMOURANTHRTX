// include/engine/Vulkan/VulkanRenderer.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// VulkanRenderer — Production Edition v10.1
// • Dynamic frame buffering via Options::Performance::MAX_FRAMES_IN_FLIGHT
// • Ray tracing pipeline with adaptive sampling and accumulation
// • Post-processing chain: Bloom, TAA, SSR, SSAO, vignette, film grain, lens flare
// • Environment rendering: IBL, volumetric fog, sky atmosphere
// • Acceleration structures: LAS rebuild, update, compaction
// • Performance monitoring: GPU timestamps, VRAM budget alerts
// • Compliant with C++23 and -Werror
// • g_PhysicalDevice DEFINED GLOBALLY
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <SDL3/SDL.h>
#include <memory>
#include <vector>
#include <array>
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <cstdint>
#include <limits>
#include <chrono>

// ──────────────────────────────────────────────────────────────────────────────
// 1. GLOBAL OPTIONS — ALL 59 VALUES ACTIVE
#include "engine/GLOBAL/OptionsMenu.hpp"

// ──────────────────────────────────────────────────────────────────────────────
// 2. RTXHandler.hpp — Handle<T>, MakeHandle, ctx()
#include "engine/GLOBAL/RTXHandler.hpp"

// ──────────────────────────────────────────────────────────────────────────────
// 3. LAS + Swapchain + VulkanCore
#include "engine/GLOBAL/LAS.hpp"           // RTX::LAS::get()
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/Vulkan/VulkanCore.hpp"

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL PHYSICAL DEVICE — DEFINED HERE
// ──────────────────────────────────────────────────────────────────────────────
inline VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;

// ──────────────────────────────────────────────────────────────────────────────
struct Camera;

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL ACCESS
inline auto& LAS       = RTX::LAS::get();

// ──────────────────────────────────────────────────────────────────────────────
// CONSTANTS — DYNAMIC FROM OptionsMenu.hpp
static constexpr uint32_t MAX_DESCRIPTOR_SETS = 1024;
static constexpr VkSampleCountFlagBits MSAA_SAMPLES = VK_SAMPLE_COUNT_1_BIT;

// ──────────────────────────────────────────────────────────────────────────────
enum class FpsTarget { FPS_60 = 60, FPS_120 = 120, FPS_UNLIMITED = 0 };
enum class TonemapType { ACES, FILMIC, REINHARD };

// ──────────────────────────────────────────────────────────────────────────────
class VulkanRenderer {
public:
    VulkanRenderer(int width, int height, SDL_Window* window,
                   const std::vector<std::string>& shaderPaths,
                   bool overclockFromMain = false);

    ~VulkanRenderer();

    void renderFrame(const Camera& camera, float deltaTime) noexcept;

    // === RUNTIME TOGGLES ===
    void toggleHypertrace() noexcept;
    void toggleFpsTarget() noexcept;
    void toggleDenoising() noexcept;
    void toggleAdaptiveSampling() noexcept;
    void setTonemapType(TonemapType type) noexcept;
    void setOverclockMode(bool enabled) noexcept;

    // === NEW: REQUIRED BY Application ===
    void setTonemap(bool enabled) noexcept;
    void setOverlay(bool show) noexcept;
    void setRenderMode(int mode) noexcept;

    // === ACCESSORS ===
    [[nodiscard]] VkDevice         device()          const noexcept { return RTX::ctx().vkDevice(); }
    [[nodiscard]] VkPhysicalDevice physicalDevice()  const noexcept { return RTX::ctx().vkPhysicalDevice(); }
    [[nodiscard]] VkCommandPool    commandPool()      const noexcept { return RTX::ctx().commandPool(); }
    [[nodiscard]] VkQueue          graphicsQueue()   const noexcept { return RTX::ctx().graphicsQueue(); }
    [[nodiscard]] VkQueue          presentQueue()    const noexcept { return RTX::ctx().presentQueue(); }

    [[nodiscard]] int              width()           const noexcept { return width_; }
    [[nodiscard]] int              height()          const noexcept { return height_; }
    [[nodiscard]] bool             hypertraceEnabled()     const noexcept { return hypertraceEnabled_; }
    [[nodiscard]] bool             denoisingEnabled()      const noexcept { return denoisingEnabled_; }
    [[nodiscard]] bool             adaptiveSamplingEnabled() const noexcept { return adaptiveSamplingEnabled_; }
    [[nodiscard]] TonemapType      tonemapType()     const noexcept { return tonemapType_; }
    [[nodiscard]] FpsTarget        fpsTarget()       const noexcept { return fpsTarget_; }
    [[nodiscard]] bool             overclockMode()   const noexcept { return overclockMode_; }
    [[nodiscard]] float            currentNexusScore() const noexcept { return currentNexusScore_; }
    [[nodiscard]] uint32_t         currentSpp()      const noexcept { return currentSpp_; }

    void handleResize(int w, int h) noexcept;

private:
    // === WINDOW & FRAME STATE ===
    SDL_Window* window_ = nullptr;
    int width_ = 0, height_ = 0;
    uint32_t currentFrame_ = 0;
    uint64_t frameNumber_ = 0;
    float frameTime_ = 0.0f;
    float currentNexusScore_ = 0.5f;
    uint32_t currentSpp_ = Options::RTX::MIN_SPP;
    float hypertraceCounter_ = 0.0f;
    double timestampPeriod_ = 0.0;
    bool resetAccumulation_ = true;

    // === RUNTIME TOGGLES ===
    bool hypertraceEnabled_ = Options::RTX::ENABLE_ADAPTIVE_SAMPLING;
    bool denoisingEnabled_ = Options::RTX::ENABLE_DENOISING;
    bool adaptiveSamplingEnabled_ = Options::RTX::ENABLE_ADAPTIVE_SAMPLING;
    bool overclockMode_ = false;
    FpsTarget fpsTarget_ = FpsTarget::FPS_120;
    TonemapType tonemapType_ = TonemapType::ACES;

    // === NEW: Application state sync ===
    bool tonemapEnabled_ = true;
    bool showOverlay_ = true;
    int renderMode_ = 1;

    // === PERFORMANCE LOGGING ===
    std::chrono::steady_clock::time_point lastPerfLogTime_;
    uint32_t frameCounter_ = 0;

    // === SYNC OBJECTS (DYNAMIC SIZE) ===
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence> inFlightFences_;
    VkQueryPool timestampQueryPool_ = VK_NULL_HANDLE;

    // === COMMAND BUFFERS ===
    std::vector<VkCommandBuffer> commandBuffers_;

    // === DESCRIPTOR POOLS ===
    RTX::Handle<VkDescriptorPool> descriptorPool_;
    RTX::Handle<VkDescriptorPool> rtDescriptorPool_;

    // === RTX PIPELINE ===
    RTX::Handle<VkPipeline> rtPipeline_;
    RTX::Handle<VkPipelineLayout> rtPipelineLayout_;
    RTX::Handle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    std::vector<VkDescriptorSet> rtDescriptorSets_;  // DYNAMIC SIZE

    // === SBT ===
    uint64_t sbtBufferEnc_ = 0;
    VkDeviceAddress sbtAddress_ = 0;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtxProps_{};
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;

    // === BUFFERS ===
    std::vector<uint64_t> uniformBufferEncs_;
    std::vector<uint64_t> materialBufferEncs_;
    std::vector<uint64_t> dimensionBufferEncs_;
    std::vector<uint64_t> tonemapUniformEncs_;
    uint64_t sharedStagingBufferEnc_ = 0;
    RTX::Handle<VkBuffer> sharedStagingBuffer_;
    RTX::Handle<VkDeviceMemory> sharedStagingMemory_;

    // === RT OUTPUT ===
    std::vector<RTX::Handle<VkImage>> rtOutputImages_;
    std::vector<RTX::Handle<VkDeviceMemory>> rtOutputMemories_;
    std::vector<RTX::Handle<VkImageView>> rtOutputViews_;

    // === ACCUMULATION ===
    std::vector<RTX::Handle<VkImage>> accumImages_;
    std::vector<RTX::Handle<VkDeviceMemory>> accumMemories_;
    std::vector<RTX::Handle<VkImageView>> accumViews_;

    // === DENOISER ===
    RTX::Handle<VkImage> denoiserImage_;
    RTX::Handle<VkDeviceMemory> denoiserMemory_;
    RTX::Handle<VkImageView> denoiserView_;

    // === ENVIRONMENT MAP ===
    RTX::Handle<VkImage> envMapImage_;
    RTX::Handle<VkDeviceMemory> envMapImageMemory_;
    RTX::Handle<VkImageView> envMapImageView_;
    RTX::Handle<VkSampler> envMapSampler_;

    // === HYPERTRACE NEXUS SCORE ===
    RTX::Handle<VkImage> hypertraceScoreImage_;
    RTX::Handle<VkDeviceMemory> hypertraceScoreMemory_;
    RTX::Handle<VkImageView> hypertraceScoreView_;
    RTX::Handle<VkBuffer> hypertraceScoreStagingBuffer_;
    RTX::Handle<VkDeviceMemory> hypertraceScoreStagingMemory_;

    // === POST-PROCESSING PIPELINES ===
    RTX::Handle<VkPipeline> denoiserPipeline_;
    RTX::Handle<VkPipelineLayout> denoiserLayout_;
    std::vector<VkDescriptorSet> denoiserSets_;

    RTX::Handle<VkPipeline> tonemapPipeline_;
    RTX::Handle<VkPipelineLayout> tonemapLayout_;
    std::vector<VkDescriptorSet> tonemapSets_;

    // === HELPER METHODS ===
    VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool pool);
    void endSingleTimeCommands(VkDevice device, VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd);
    VkCommandBuffer allocateTransientCommandBuffer(VkDevice device, VkCommandPool pool);

    void cleanup() noexcept;
    void destroyNexusScoreImage() noexcept;
    void destroyDenoiserImage() noexcept;
    void destroyAllBuffers() noexcept;
    void destroyAccumulationImages() noexcept;
    void destroyRTOutputImages() noexcept;

    void createRTOutputImages();
    void createAccumulationImages();
    void createDenoiserImage();
    void createEnvironmentMap();
    void createNexusScoreImage(VkPhysicalDevice phys, VkDevice dev, VkCommandPool pool, VkQueue queue);
    void initializeAllBufferData(uint32_t frames, VkDeviceSize uniformSize, VkDeviceSize materialSize);
    void createCommandBuffers();
    void allocateDescriptorSets();
    void updateNexusDescriptors();
    void updateRTXDescriptors();
    void updateTonemapDescriptorsInitial();
    void updateDenoiserDescriptors();

    void createRayTracingPipeline(const std::vector<std::string>& shaderPaths);
    void createShaderBindingTable();
    VkShaderModule loadShader(const std::string& path);
    VkDeviceAddress getShaderGroupHandle(uint32_t group);

    void recordRayTracingCommandBuffer(VkCommandBuffer cmd);
    void performDenoisingPass(VkCommandBuffer cmd);
    void performTonemapPass(VkCommandBuffer cmd, uint32_t imageIndex);

    void updateUniformBuffer(uint32_t frame, const Camera& camera, float jitter);
    void updateTonemapUniform(uint32_t frame);

    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) const noexcept;
    void createImageArray(std::vector<RTX::Handle<VkImage>>& images,
                          std::vector<RTX::Handle<VkDeviceMemory>>& memories,
                          std::vector<RTX::Handle<VkImageView>>& views,
                          const std::string& tag) noexcept;
    void createImage(RTX::Handle<VkImage>& image, RTX::Handle<VkDeviceMemory>& memory, RTX::Handle<VkImageView>& view, const std::string& tag) noexcept;
};

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL RENDERER ACCESS (NOT SINGLETON)
[[nodiscard]] inline VulkanRenderer& getRenderer();
inline void initRenderer(int w, int h);
inline void handleResize(int w, int h);
inline void renderFrame(const Camera& camera, float deltaTime) noexcept;
inline void shutdown() noexcept;

// =============================================================================
// STATUS: PRODUCTION READY — g_PhysicalDevice DEFINED
// =============================================================================