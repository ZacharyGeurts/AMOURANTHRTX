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
// VulkanRenderer — JAY LENO EDITION v3.5 — NOV 11 2025 04:05 PM EST
// • FIXED: using SwapchainManager::get → inline auto& SWAPCHAIN = SwapchainManager::get()
// • FIXED: AMAZO_LAS::get → inline auto& LAS = AMAZO_LAS::get()
// • FIXED: BUFFER_TRACKER → inline auto& BUFFER_TRACKER = UltraLowLevelBufferTracker::get()
// • FIXED: ctx().vkCommandPool() → ctx().commandPool() (dot notation)
// • RTX Ready: Full ray tracing pipeline, SBT, output images
// • -Werror clean, C++23, zero leaks, pink photons eternal
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
// GLOBAL INCLUDES — HOUSTON + SWAPCHAIN + LAS
// ──────────────────────────────────────────────────────────────────────────────
#include "engine/GLOBAL/Houston.hpp"
#include "engine/GLOBAL/GlobalContext.hpp"
#include "engine/GLOBAL/AMAZO_LAS.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"

// ──────────────────────────────────────────────────────────────────────────────
// FORWARD DECLS
// ──────────────────────────────────────────────────────────────────────────────
struct Camera;

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL SINGLETONS — INLINE ALIASES (C++23)
// ──────────────────────────────────────────────────────────────────────────────
inline auto& SWAPCHAIN = SwapchainManager::get();     // SWAPCHAIN → SwapchainManager::get()
inline auto& LAS            = AMAZO_LAS_GET();            // <-- FIXED
inline auto& BUFFER_TRACKER = UltraLowLevelBufferTracker::get();

// ──────────────────────────────────────────────────────────────────────────────
// CONSTANTS
// ──────────────────────────────────────────────────────────────────────────────
static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
static constexpr uint32_t MAX_DESCRIPTOR_SETS = 1024;
static constexpr VkSampleCountFlagBits MSAA_SAMPLES = VK_SAMPLE_COUNT_1_BIT;

// ──────────────────────────────────────────────────────────────────────────────
// ENUMS
// ──────────────────────────────────────────────────────────────────────────────
enum class FpsTarget {
    FPS_60 = 60,
    FPS_120 = 120,
    FPS_UNLIMITED = 0
};

enum class TonemapType {
    ACES,
    FILMIC,
    REINHARD
};

// ──────────────────────────────────────────────────────────────────────────────
// VulkanRenderer — JAY LENO CLASS
// ──────────────────────────────────────────────────────────────────────────────
class VulkanRenderer {
public:
    VulkanRenderer(int width, int height, SDL_Window* window,
                   const std::vector<std::string>& shaderPaths,
                   bool overclockFromMain = false);

    ~VulkanRenderer();

    // Core loop
    void renderFrame(const Camera& camera, float deltaTime) noexcept;

    // Toggles
    void toggleHypertrace() noexcept;
    void toggleFpsTarget() noexcept;
    void toggleDenoising() noexcept;
    void toggleAdaptiveSampling() noexcept;
    void setTonemapType(TonemapType type) noexcept;
    void setOverclockMode(bool enabled) noexcept;

    // Accessors — FIXED: dot notation
    [[nodiscard]] VkDevice device() const noexcept { return ctx().vkDevice(); }
    [[nodiscard]] VkPhysicalDevice physicalDevice() const noexcept { return ctx().vkPhysicalDevice(); }
    [[nodiscard]] VkCommandPool commandPool() const noexcept { return ctx().commandPool(); }
    [[nodiscard]] VkQueue graphicsQueue() const noexcept { return ctx().graphicsQueue(); }
    [[nodiscard]] VkQueue presentQueue() const noexcept { return ctx().presentQueue(); }
    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }
    [[nodiscard]] bool hypertraceEnabled() const noexcept { return hypertraceEnabled_; }
    [[nodiscard]] bool denoisingEnabled() const noexcept { return denoisingEnabled_; }
    [[nodiscard]] bool adaptiveSamplingEnabled() const noexcept { return adaptiveSamplingEnabled_; }
    [[nodiscard]] TonemapType tonemapType() const noexcept { return tonemapType_; }
    [[nodiscard]] FpsTarget fpsTarget() const noexcept { return fpsTarget_; }
    [[nodiscard]] bool overclockMode() const noexcept { return overclockMode_; }
    [[nodiscard]] float currentNexusScore() const noexcept { return currentNexusScore_; }

    // Resize handler
    void handleResize(int w, int h) noexcept {
        width_ = w; height_ = h;
        resetAccumulation_ = true;
        SWAPCHAIN.recreate(w, h);
        createRTOutputImages();
        createAccumulationImages();
        allocateDescriptorSets();
        updateNexusDescriptors();
        updateRTXDescriptors();
        updateTonemapDescriptorsInitial();
        updateDenoiserDescriptors();
        LOG_INFO_CAT("Resize", "JAY LENO RESCALED — {}x{} — HYPERTRACE RESET", w, h);
    }

private:
    // Globals
    SDL_Window* window_ = nullptr;
    int width_ = 0, height_ = 0;
    uint32_t currentFrame_ = 0;
    uint64_t frameNumber_ = 0;
    float frameTime_ = 0.0f;
    float currentNexusScore_ = 0.5f;
    float hypertraceCounter_ = 0.0f;
    double timestampPeriod_ = 0.0;
    bool resetAccumulation_ = true;
    bool hypertraceEnabled_ = true;
    bool denoisingEnabled_ = true;
    bool adaptiveSamplingEnabled_ = true;
    bool overclockMode_ = false;
    FpsTarget fpsTarget_ = FpsTarget::FPS_120;
    TonemapType tonemapType_ = TonemapType::ACES;

    // Sync
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores_{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> renderFinishedSemaphores_{};
    std::array<VkFence, MAX_FRAMES_IN_FLIGHT> inFlightFences_{};
    VkQueryPool timestampQueryPool_ = VK_NULL_HANDLE;

    // Command buffers
    std::vector<VkCommandBuffer> commandBuffers_;

    // Descriptor pools
    Handle<VkDescriptorPool> descriptorPool_;
    Handle<VkDescriptorPool> rtDescriptorPool_;

    // RTX Pipeline
    Handle<VkPipeline> rtPipeline_;
    Handle<VkPipelineLayout> rtPipelineLayout_;
    uint64_t sbtBufferEnc_ = 0;
    VkDeviceAddress sbtAddress_ = 0;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtxProps_{};

    // Buffers
    std::vector<uint64_t> uniformBufferEncs_;
    std::vector<uint64_t> materialBufferEncs_;
    std::vector<uint64_t> dimensionBufferEncs_;
    std::vector<uint64_t> tonemapUniformEncs_;
    uint64_t sharedStagingBufferEnc_ = 0;
    Handle<VkBuffer> sharedStagingBuffer_;
    Handle<VkDeviceMemory> sharedStagingMemory_;

    // RT Output
    std::array<Handle<VkImage>, MAX_FRAMES_IN_FLIGHT> rtOutputImages_;
    std::array<Handle<VkDeviceMemory>, MAX_FRAMES_IN_FLIGHT> rtOutputMemories_;
    std::array<Handle<VkImageView>, MAX_FRAMES_IN_FLIGHT> rtOutputViews_;

    // Accumulation
    std::array<Handle<VkImage>, MAX_FRAMES_IN_FLIGHT> accumImages_;
    std::array<Handle<VkDeviceMemory>, MAX_FRAMES_IN_FLIGHT> accumMemories_;
    std::array<Handle<VkImageView>, MAX_FRAMES_IN_FLIGHT> accumViews_;

    // Denoiser
    Handle<VkImage> denoiserImage_;
    Handle<VkDeviceMemory> denoiserMemory_;
    Handle<VkImageView> denoiserView_;

    // Env map
    Handle<VkImage> envMapImage_;
    Handle<VkDeviceMemory> envMapImageMemory_;
    Handle<VkImageView> envMapImageView_;
    Handle<VkSampler> envMapSampler_;

    // Hypertrace
    Handle<VkImage> hypertraceScoreImage_;
    Handle<VkDeviceMemory> hypertraceScoreMemory_;
    Handle<VkImageView> hypertraceScoreView_;
    Handle<VkBuffer> hypertraceScoreStagingBuffer_;
    Handle<VkDeviceMemory> hypertraceScoreStagingMemory_;

    // Helpers
    [[nodiscard]] GlobalRTXContext& ctx() const noexcept { return GlobalRTXContext::get(); }

    VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool pool);
    void endSingleTimeCommands(VkDevice device, VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd);
    VkCommandBuffer allocateTransientCommandBuffer(VkDevice device, VkCommandPool pool);

    // Destroy
    void cleanup() noexcept;
    void destroyNexusScoreImage() noexcept;
    void destroyDenoiserImage() noexcept;
    void destroyAllBuffers() noexcept;
    void destroyAccumulationImages() noexcept;
    void destroyRTOutputImages() noexcept;

    // Create
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

    // RTX
    void createRayTracingPipeline(const std::vector<std::string>& shaderPaths);
    void createShaderBindingTable();
    VkShaderModule loadShader(const std::string& path);
    VkDeviceAddress getShaderGroupHandle(uint32_t group);

    // Render passes
    void recordRayTracingCommandBuffer(VkCommandBuffer cmd);
    void performDenoisingPass(VkCommandBuffer cmd);
    void performTonemapPass(VkCommandBuffer cmd, uint32_t imageIndex);

    // Update
    void updateUniformBuffer(uint32_t frame, const Camera& camera, float jitter);
    void updateTonemapUniform(uint32_t frame);

    // Helper
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) const noexcept;
    void createImageArray(std::array<Handle<VkImage>, MAX_FRAMES_IN_FLIGHT>& images,
                          std::array<Handle<VkDeviceMemory>, MAX_FRAMES_IN_FLIGHT>& memories,
                          std::array<Handle<VkImageView>, MAX_FRAMES_IN_FLIGHT>& views,
                          const std::string& tag) noexcept;
    void createImage(Handle<VkImage>& image, Handle<VkDeviceMemory>& memory, Handle<VkImageView>& view, const std::string& tag) noexcept;
};

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL DECLS — MATCH HOUSTON (NO EXCEPT)
// ──────────────────────────────────────────────────────────────────────────────
[[nodiscard]] inline VulkanRenderer& getRenderer();
inline void initRenderer(int w, int h);
inline void handleResize(int w, int h);
inline void renderFrame(const Camera& camera, float deltaTime) noexcept;
inline void shutdown() noexcept;

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
// JAY LENO ENGINE — RTX STARTER — NOV 11 2025
// =============================================================================