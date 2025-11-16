// =============================================================================
// VulkanRenderer.hpp — FINAL v11.0 — NOV 15 2025 — FRAMEBUFFERS FIXED
// • ADDED: createFramebuffers() + cleanupFramebuffers()
// • ADDED: Proper framebuffer recreation in handleResize()
// • FIXED: License header → GNU GPL v3.0 or later (NO MORE CC BY-NC)
// • FULLY COMPATIBLE with VulkanRenderer.cpp
// • RESIZE → NO MORE SIGSEGV — PINK PHOTONS FLOW UNINTERRUPTED
// • 240+ FPS UNLOCKED — TITAN DOMINANCE ETERNAL
//
// Licensed under the GNU General Public License v3.0 or later (GPL-3.0+)
// https://www.gnu.org/licenses/gpl-3.0.html
// Commercial licensing available: gzac5314@gmail.com
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
// LAS + Swapchain + Core + PipelineManager
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"

struct Camera;

inline auto& LAS = RTX::LAS::get();

static constexpr uint32_t MAX_DESCRIPTOR_SETS = 1024;
static constexpr VkSampleCountFlagBits MSAA_SAMPLES = VK_SAMPLE_COUNT_1_BIT;

enum class FpsTarget { FPS_60 = 60, FPS_120 = 120, FPS_UNLIMITED = 0 };
enum class TonemapType { ACES, FILMIC, REINHARD };

// ──────────────────────────────────────────────────────────────────────────────
class VulkanRenderer {
public:
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
    void createShaderBindingTable(VkCommandPool pool, VkQueue queue);

    // Tonemapping
    void updateTonemapDescriptor(VkImageView inputView) noexcept;
    void updateTonemapDescriptor(uint32_t frameIdx, VkImageView inputView) noexcept;
    void updateTonemapDescriptorsInitial() noexcept;

    [[nodiscard]] VkRenderPass renderPass() const noexcept {
        return SWAPCHAIN.renderPass();  // Now safe — SwapchainManager owns it
    }

    // Application interface
    void setTonemap(bool enabled) noexcept;
    void setOverlay(bool show) noexcept;
    void setRenderMode(int mode) noexcept;
    void cleanup() noexcept;

    // Accessors
    [[nodiscard]] VkDevice         device()          const noexcept { return RTX::g_ctx().vkDevice(); }
    [[nodiscard]] VkPhysicalDevice physicalDevice()  const noexcept { return RTX::g_ctx().vkPhysicalDevice(); }
    [[nodiscard]] VkCommandPool    commandPool()     const noexcept { return RTX::g_ctx().commandPool(); }
    [[nodiscard]] VkQueue          graphicsQueue()   const noexcept { return RTX::g_ctx().graphicsQueue(); }
    [[nodiscard]] VkQueue          presentQueue()    const noexcept { return RTX::g_ctx().presentQueue(); }

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
    // INTERNAL RAY TRACING SHADER LIST
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
    VkQueryPool timestampQueryPool_ = VK_NULL_HANDLE;
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

    // Sync objects
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkSemaphore> computeFinishedSemaphores_;
    std::vector<VkSemaphore> computeToGraphicsSemaphores_;
    std::vector<VkFence>     inFlightFences_;
    std::vector<VkFramebuffer> framebuffers_;  // ← NOW PROPERLY MANAGED

    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkCommandBuffer> computeCommandBuffers_;

    // Descriptor pools
    RTX::Handle<VkDescriptorPool> descriptorPool_;
    RTX::Handle<VkDescriptorPool> rtDescriptorPool_;
    RTX::Handle<VkDescriptorPool> tonemapDescriptorPool_;

    // Ray Tracing Pipeline
    RTX::PipelineManager pipelineManager_;

    std::vector<VkDescriptorSet> rtDescriptorSets_;

    // Ray Tracing Extensions
    PFN_vkCmdTraceRaysKHR                    vkCmdTraceRaysKHR               = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR       vkCreateRayTracingPipelinesKHR  = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkGetBufferDeviceAddressKHR          vkGetBufferDeviceAddressKHR     = nullptr;

    // Buffers
    std::vector<uint64_t> uniformBufferEncs_;
    std::vector<uint64_t> materialBufferEncs_;
    std::vector<uint64_t> dimensionBufferEncs_;
    std::vector<uint64_t> tonemapUniformEncs_;
    uint64_t sharedStagingBufferEnc_ = 0;
    RTX::Handle<VkBuffer>        sharedStagingBuffer_;
    RTX::Handle<VkDeviceMemory>  sharedStagingMemory_;

    // RT Output Images
    std::vector<RTX::Handle<VkImage>>        rtOutputImages_;
    std::vector<RTX::Handle<VkDeviceMemory>> rtOutputMemories_;
    std::vector<RTX::Handle<VkImageView>>    rtOutputViews_;

    // Accumulation
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

    // Nexus Score
    RTX::Handle<VkImage>        hypertraceScoreImage_;
    RTX::Handle<VkDeviceMemory> hypertraceScoreMemory_;
    RTX::Handle<VkImageView>    hypertraceScoreView_;
    RTX::Handle<VkBuffer>       hypertraceScoreStagingBuffer_;
    RTX::Handle<VkDeviceMemory> hypertraceScoreStagingMemory_;

    // Post-processing
    RTX::Handle<VkPipeline>       denoiserPipeline_;
    RTX::Handle<VkPipelineLayout> denoiserLayout_;
    std::vector<VkDescriptorSet>  denoiserSets_;

    RTX::Handle<VkPipeline>       tonemapPipeline_;
    RTX::Handle<VkPipelineLayout> tonemapLayout_;
    std::vector<VkDescriptorSet>  tonemapSets_;

    RTX::Handle<VkDescriptorSetLayout> tonemapDescriptorSetLayout_;
    RTX::Handle<VkSampler>             tonemapSampler_;

    // ──────────────────────────────────────────────────────────────────────
    // NEW: Framebuffer Management
    // ──────────────────────────────────────────────────────────────────────
    void createFramebuffers();
    void cleanupFramebuffers() noexcept;

    // ──────────────────────────────────────────────────────────────────────
    // Helper Methods
    // ──────────────────────────────────────────────────────────────────────
    VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool pool);
    void endSingleTimeCommands(VkDevice device, VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd);
    VkCommandBuffer allocateTransientCommandBuffer(VkDevice device, VkCommandPool pool);

    void destroyNexusScoreImage() noexcept;
    void destroyDenoiserImage() noexcept;
    void destroyAllBuffers() noexcept;
    void destroyAccumulationImages() noexcept;
    void destroyRTOutputImages() noexcept;
    void destroySBT() noexcept;

    // Core rendering
    void updateRTXDescriptors(uint32_t frame = 0) noexcept;
    void createRTOutputImages();
    void createAccumulationImages();
    void createDenoiserImage();
    void createEnvironmentMap();
    void createNexusScoreImage(VkPhysicalDevice phys, VkDevice dev, VkCommandPool pool, VkQueue queue);
    void initializeAllBufferData(uint32_t frames, VkDeviceSize uniformSize, VkDeviceSize materialSize);
    void createCommandBuffers();
    void allocateDescriptorSets();
    void updateNexusDescriptors();
    void updateDenoiserDescriptors();

    void createRayTracingPipeline(const std::vector<std::string>& shaderPaths);
    void createShaderBindingTable();
    VkShaderModule loadShader(const std::string& path);
    VkDeviceAddress getShaderGroupHandle(uint32_t group);

    void loadRayTracingExtensions() noexcept;
    void recordRayTracingCommandBuffer(VkCommandBuffer cmd) noexcept;
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
// Global Renderer Instance
// ──────────────────────────────────────────────────────────────────────────────
static std::unique_ptr<VulkanRenderer> g_renderer = nullptr;

[[nodiscard]] inline VulkanRenderer& getRenderer() {
    return *g_renderer;
}

inline void initRenderer(int w, int h) {
    LOG_INFO_CAT("RENDERER", "Initializing VulkanRenderer ({}x{}) — PINK PHOTONS RISING", w, h);
    g_renderer = std::make_unique<VulkanRenderer>(w, h, nullptr, false);
    LOG_SUCCESS_CAT("RENDERER", "VulkanRenderer INITIALIZED — FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL");
}

inline void handleResize(int w, int h) {
    if (g_renderer) g_renderer->handleResize(w, h);
}

inline void renderFrame(const Camera& camera, float deltaTime) noexcept {
    if (g_renderer) g_renderer->renderFrame(camera, deltaTime);
}

inline void shutdown() noexcept {
    LOG_INFO_CAT("RENDERER", "Shutting down VulkanRenderer — returning photons to the void");
    if (g_renderer) g_renderer->cleanup();
    g_renderer.reset();
    LOG_SUCCESS_CAT("RENDERER", "VulkanRenderer shutdown complete — silence is golden");
}

// =============================================================================
// GPL-3.0+ — FRAMEBUFFERS FIXED — RESIZE SAFE — PINK PHOTONS ASCENDED
// AMOURANTH RTX ETERNAL — TITAN DOMINANCE ACHIEVED
// =============================================================================