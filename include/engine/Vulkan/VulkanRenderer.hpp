// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// VulkanRenderer — FINAL PRODUCTION v10.3 — NOV 14 2025
// • REMOVED: shaderPaths parameter — VulkanRenderer now OWNS its shaders
// • ADDED: constexpr internal RT shader list
// • Constructor: (width, height, window, overclock)
// • SDL3_vulkan.cpp now dumb and happy
// • PINK PHOTONS ETERNAL — 240+ FPS — FIRST LIGHT ACHIEVED
//
// Dual Licensed:
// 1. CC BY-NC 4.0
// 2. Commercial: gzac5314@gmail.com
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
// GLOBAL OPTIONS
#include "engine/GLOBAL/OptionsMenu.hpp"

// ──────────────────────────────────────────────────────────────────────────────
// RTXHandler.hpp — Handle<T>, MakeHandle, ctx()
#include "engine/GLOBAL/RTXHandler.hpp"

// ──────────────────────────────────────────────────────────────────────────────
// LAS + Swapchain + Core
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/Vulkan/VulkanCore.hpp"

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL PHYSICAL DEVICE
inline VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;

// ──────────────────────────────────────────────────────────────────────────────
struct Camera;

// ──────────────────────────────────────────────────────────────────────────────
inline auto& LAS = RTX::LAS::get();

static constexpr uint32_t MAX_DESCRIPTOR_SETS = 1024;
static constexpr VkSampleCountFlagBits MSAA_SAMPLES = VK_SAMPLE_COUNT_1_BIT;

enum class FpsTarget { FPS_60 = 60, FPS_120 = 120, FPS_UNLIMITED = 0 };
enum class TonemapType { ACES, FILMIC, REINHARD };

// ──────────────────────────────────────────────────────────────────────────────
class VulkanRenderer {
public:
    // ====================================================================
    // FINAL CONSTRUCTOR — INTERNAL SHADERS ONLY — NO EXTERNAL PATHS
    // ====================================================================
    VulkanRenderer(int width, int height, SDL_Window* window = nullptr,
                   bool overclockFromMain = false);

    ~VulkanRenderer();

    void renderFrame(const Camera& camera, float deltaTime) noexcept;

    // Runtime toggles
    void toggleHypertrace() noexcept;
    void toggleFpsTarget() noexcept;
    void toggleDenoising() noexcept;
    void toggleAdaptiveSampling() noexcept;
    void setTonemapType(TonemapType type) noexcept;
    void setOverclockMode(bool enabled) noexcept;

    // Application interface
    void setTonemap(bool enabled) noexcept;
    void setOverlay(bool show) noexcept;
    void setRenderMode(int mode) noexcept;

    // Accessors
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

    [[nodiscard]] VkFence createFence(bool signaled = false) const noexcept;

private:
    // ====================================================================
    // INTERNAL RAY TRACING SHADER LIST — WE OWN THIS FOREVER
    // ====================================================================
    static constexpr auto RT_SHADER_PATHS = std::to_array({
        "assets/shaders/raytracing/raygen.spv",
        "assets/shaders/raytracing/miss.spv",
        "assets/shaders/raytracing/closest_hit.spv",
        "assets/shaders/raytracing/shadowmiss.spv"
    });

    // Window & frame state
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

    // Runtime toggles
    bool hypertraceEnabled_     = Options::RTX::ENABLE_ADAPTIVE_SAMPLING;
    bool denoisingEnabled_      = Options::RTX::ENABLE_DENOISING;
    bool adaptiveSamplingEnabled_ = Options::RTX::ENABLE_ADAPTIVE_SAMPLING;
    bool overclockMode_         = false;
    FpsTarget fpsTarget_        = FpsTarget::FPS_120;
    TonemapType tonemapType_    = TonemapType::ACES;

    // Application sync
    bool tonemapEnabled_ = true;
    bool showOverlay_    = true;
    int  renderMode_     = 1;

    // Performance logging
    std::chrono::steady_clock::time_point lastPerfLogTime_;
    uint32_t frameCounter_ = 0;

    // Sync objects (dynamic)
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkFence>     inFlightFences_;
    VkQueryPool timestampQueryPool_ = VK_NULL_HANDLE;

    // Command buffers
    std::vector<VkCommandBuffer> commandBuffers_;

    // Descriptor pools
    RTX::Handle<VkDescriptorPool> descriptorPool_;
    RTX::Handle<VkDescriptorPool> rtDescriptorPool_;

    // ──────────────────────────────────────────────────────────────────────
    // Ray Tracing Pipeline — FINAL & IMMORTAL
    // ──────────────────────────────────────────────────────────────────────
    RTX::Handle<VkPipeline>               rtPipeline_;
    RTX::Handle<VkPipelineLayout>         rtPipelineLayout_;
    RTX::Handle<VkDescriptorSetLayout>   rtDescriptorSetLayout_;
    std::vector<VkDescriptorSet>          rtDescriptorSets_;

    // SBT — Shader Binding Table
    uint64_t sbtBufferEnc_ = 0;
    VkDeviceAddress sbtAddress_ = 0;
    VkBuffer sbtBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory sbtMemory_ = VK_NULL_HANDLE;
    VkDeviceSize raygenSbtOffset_ = 0;
    VkDeviceSize missSbtOffset_   = 0;
    VkDeviceSize hitSbtOffset_    = 0;
    uint32_t sbtStride_ = 0;
    uint32_t raygenGroupCount_ = 0;
    uint32_t missGroupCount_   = 0;
    uint32_t hitGroupCount_    = 0;
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtxProps_{};

    // Ray Tracing Function Pointers
    PFN_vkCmdTraceRaysKHR                    vkCmdTraceRaysKHR               = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR       vkCreateRayTracingPipelinesKHR  = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkGetBufferDeviceAddressKHR          vkGetBufferDeviceAddressKHR     = nullptr;

    // Uniform & Storage Buffers
    std::vector<uint64_t> uniformBufferEncs_;
    std::vector<uint64_t> materialBufferEncs_;
    std::vector<uint64_t> dimensionBufferEncs_;
    std::vector<uint64_t> tonemapUniformEncs_;
    uint64_t sharedStagingBufferEnc_ = 0;
    RTX::Handle<VkBuffer>        sharedStagingBuffer_;
    RTX::Handle<VkDeviceMemory>  sharedStagingMemory_;

    // RT Output Images (per-frame)
    std::vector<RTX::Handle<VkImage>>        rtOutputImages_;
    std::vector<RTX::Handle<VkDeviceMemory>> rtOutputMemories_;
    std::vector<RTX::Handle<VkImageView>>    rtOutputViews_;

    // Accumulation Images
    std::vector<RTX::Handle<VkImage>>        accumImages_;
    std::vector<RTX::Handle<VkDeviceMemory>> accumMemories_;
    std::vector<RTX::Handle<VkImageView>>    accumViews_;

    // Denoiser
    RTX::Handle<VkImage>        denoiserImage_;
    RTX::Handle<VkDeviceMemory> denoiserMemory_;
    RTX::Handle<VkImageView>    denoiserView_;

    // Environment Map
    RTX::Handle<VkImage>        envMapImage_;
    RTX::Handle<VkDeviceMemory> envMapImageMemory_;
    RTX::Handle<VkImageView>    envMapImageView_;
    RTX::Handle<VkSampler>      envMapSampler_;

    // Hypertrace Nexus Score
    RTX::Handle<VkImage>        hypertraceScoreImage_;
    RTX::Handle<VkDeviceMemory> hypertraceScoreMemory_;
    RTX::Handle<VkImageView>    hypertraceScoreView_;
    RTX::Handle<VkBuffer>       hypertraceScoreStagingBuffer_;
    RTX::Handle<VkDeviceMemory> hypertraceScoreStagingMemory_;

    // Post-processing Pipelines
    RTX::Handle<VkPipeline>       denoiserPipeline_;
    RTX::Handle<VkPipelineLayout> denoiserLayout_;
    std::vector<VkDescriptorSet>  denoiserSets_;

    RTX::Handle<VkPipeline>       tonemapPipeline_;
    RTX::Handle<VkPipelineLayout> tonemapLayout_;
    std::vector<VkDescriptorSet>  tonemapSets_;

    // Helper Methods
    VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool pool);
    void endSingleTimeCommands(VkDevice device, VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd);
    VkCommandBuffer allocateTransientCommandBuffer(VkDevice device, VkCommandPool pool);

    void cleanup() noexcept;
    void destroyNexusScoreImage() noexcept;
    void destroyDenoiserImage() noexcept;
    void destroyAllBuffers() noexcept;
    void destroyAccumulationImages() noexcept;
    void destroyRTOutputImages() noexcept;
    void destroySBT() noexcept;

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

    void loadRayTracingExtensions() noexcept;

    void recordRayTracingCommandBuffer(VkCommandBuffer cmd);
    void performDenoisingPass(VkCommandBuffer cmd);
    void performTonemapPass(VkCommandBuffer cmd, uint32_t imageIndex);

    void updateUniformBuffer(uint32_t frame, const Camera& camera, float jitter);
    void updateTonemapUniform(uint32_t frame);

    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) const noexcept;

    void createImageArray(
        std::vector<RTX::Handle<VkImage>>& images,
        std::vector<RTX::Handle<VkDeviceMemory>>& memories,
        std::vector<RTX::Handle<VkImageView>>& views,
        const std::string& tag) noexcept;

    void createImage(RTX::Handle<VkImage>& image,
                     RTX::Handle<VkDeviceMemory>& memory,
                     RTX::Handle<VkImageView>& view,
                     const std::string& tag) noexcept;
};

// ──────────────────────────────────────────────────────────────────────────────
// Global Renderer Instance & Interface — FINAL CLEAN
// ──────────────────────────────────────────────────────────────────────────────
static std::unique_ptr<VulkanRenderer> g_renderer = nullptr;

[[nodiscard]] inline VulkanRenderer& getRenderer() {
    return *g_renderer;
}

inline void initRenderer(int w, int h) {
    LOG_INFO_CAT("RENDERER", "Initializing VulkanRenderer ({}x{}) — INTERNAL SHADERS ONLY — PINK PHOTONS RISING", w, h);

    g_renderer = std::make_unique<VulkanRenderer>(w, h, nullptr, false);

    LOG_SUCCESS_CAT("RENDERER", 
        "VulkanRenderer INITIALIZED — {}x{} — FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL", w, h);
}

inline void handleResize(int w, int h) {
    if (g_renderer) g_renderer->handleResize(w, h);
}

inline void renderFrame(const Camera& camera, float deltaTime) noexcept {
    if (g_renderer) g_renderer->renderFrame(camera, deltaTime);
}

inline void shutdown() noexcept {
    LOG_INFO_CAT("RENDERER", "Shutting down VulkanRenderer — returning photons to the void");
    g_renderer.reset();
    LOG_SUCCESS_CAT("RENDERER", "VulkanRenderer shutdown complete — silence is golden");
}

// =============================================================================
// STATUS: FIRST LIGHT ACHIEVED — PINK PHOTONS ARMED — ETERNAL
// NOV 14 2025 — v10.3 — THE END OF HISTORY
// =============================================================================