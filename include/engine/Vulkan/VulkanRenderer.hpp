// =============================================================================
// VulkanRenderer.hpp — FINAL v12.5 — NOVEMBER 17 2025 — IMGUI CONSOLE READY
// • Application* app_ added → `~` key now summons full debug console
// • firstSwapchainAcquire_ preserved — ALL ERRORS GONE
// • TONEMAP + AUTOEXPOSURE + PINK PHOTONS ETERNAL — FIRST LIGHT ACHIEVED
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

#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/ImGuiStoneKeyShield.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"

// Forward declarations
struct Camera;
class Application;                  // ← NEW: For ImGui console access

// ImGui forward declarations
struct ImFont;
#include <imgui.h>
// Do NOT include imgui_impl_*.h here — only in .cpp!

inline auto& LAS = RTX::LAS::get();

static constexpr uint32_t MAX_DESCRIPTOR_SETS = 1024;
static constexpr VkSampleCountFlagBits MSAA_SAMPLES = VK_SAMPLE_COUNT_1_BIT;

enum class FpsTarget { FPS_60 = 60, FPS_120 = 120, FPS_UNLIMITED = 0 };
enum class TonemapType { ACES, FILMIC, REINHARD };

struct TonemapPushConstants {
    float     exposure        = 1.0f;
    uint32_t  tonemapOperator = 0;
    uint32_t  enableBloom     = 0;
    float     bloomStrength   = 0.0f;
    uint32_t  frameCounter    = 0;
    float     nexusScore      = 0.0f;
    float     _pad[2]         = {0};
};

// ──────────────────────────────────────────────────────────────────────────────
class VulkanRenderer {
public:
    void initImGuiFonts();
    void drawLoadingOverlay() noexcept;
    VulkanRenderer(int width, int height, SDL_Window* window = nullptr, bool overclockFromMain = false);
    ~VulkanRenderer();

    void renderFrame(const Camera& camera, float deltaTime) noexcept;

    void toggleHypertrace() noexcept;
    void toggleFpsTarget() noexcept;
    void toggleDenoising() noexcept;
    void toggleAdaptiveSampling() noexcept;
    void setTonemapType(TonemapType type) noexcept;
    void setOverclockMode(bool enabled) noexcept;
    void createShaderBindingTable(VkCommandPool pool, VkQueue queue);

    void updateAllRTXDescriptors() noexcept
    {
        LOG_INFO_CAT("RENDERER", "{}FIRST RAYS ARMED — UPDATING RTX DESCRIPTORS WITH VALID TLAS{}", 
                     LIME_GREEN, RESET);

        for (uint32_t f = 0; f < Options::Performance::MAX_FRAMES_IN_FLIGHT; ++f) {
            updateRTXDescriptors(f);
        }

        LOG_SUCCESS_CAT("RENDERER", "ALL RTX DESCRIPTORS BOUND — TLAS VALID — FIRST LIGHT ACHIEVED — PINK PHOTONS UNLEASHED");
    }

    void updateTonemapDescriptor(VkImageView inputView) noexcept;
    void updateTonemapDescriptor(uint32_t frameIdx, VkImageView inputView) noexcept;
    void updateTonemapDescriptor(uint32_t frameIdx, VkImageView inputView, const RTX::Handle<VkImageView>& outputView) noexcept;
    void updateTonemapDescriptorsInitial() noexcept;

    inline void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout newLayout) noexcept
    {
        transitionImageLayout(cmd, image, VK_IMAGE_LAYOUT_UNDEFINED, newLayout);
    }
    inline void transitionToWrite(VkCommandBuffer cmd, VkImage image) noexcept
    {
        transitionImageLayout(cmd, image, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_LAYOUT_GENERAL);
    }
    inline void transitionToPresent(VkCommandBuffer cmd, VkImage image) noexcept
    {
        transitionImageLayout(cmd, image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    }

    void updateAutoExposure(VkCommandBuffer cmd, VkImage finalColorImage) noexcept;
    void applyTonemap(VkCommandBuffer cmd) noexcept;

    [[nodiscard]] VkRenderPass renderPass() const noexcept { return SWAPCHAIN.renderPass(); }

    void setTonemap(bool enabled) noexcept;
    void setOverlay(bool show) noexcept;
    void setRenderMode(int mode) noexcept;
    void cleanup() noexcept;

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
    [[nodiscard]] float            currentExposure() const noexcept { return currentExposure_; }

    [[nodiscard]] VkFence createFence(bool signaled = false) const noexcept;

    // =============================================================================
    // NEW: Application link for ImGui debug console (`~` key)
    // =============================================================================
    void setApplication(Application* app) noexcept { app_ = app; }

    static VulkanRenderer& getInstance();  // already exists

    uint64_t getFrameNumber() const noexcept { return frameNumber_; }
	void onWindowResize(uint32_t w, uint32_t h) noexcept; 

private:
    bool minimized_ = false;
    bool stonekey_active_ = false;
    static inline ImFont* plasmaticaFont = nullptr;
    static inline ImFont* arialBoldFont  = nullptr;
    static inline ImFont* arialFont      = nullptr;
    static inline ImFont* iconFont       = nullptr;

    static constexpr auto RT_SHADER_PATHS = std::to_array({
        "assets/shaders/raytracing/raygen.spv",
        "assets/shaders/raytracing/miss.spv",
        "assets/shaders/raytracing/closest_hit.spv",
        "assets/shaders/raytracing/shadowmiss.spv"
    });

    // Core state
    SDL_Window* window_ = nullptr;
    int width_ = 0, height_ = 0;
    uint32_t currentFrame_ = 0;
    uint32_t imageIndex_ = 0;
    uint64_t frameNumber_ = 0;
    float frameTime_ = 0.0f;
    float deltaTime_ = 0.016f;
    float currentNexusScore_ = 0.5f;
    uint32_t currentSpp_ = Options::RTX::MIN_SPP;
    float hypertraceCounter_ = 0.0f;
    VkQueryPool timestampQueryPool_ = VK_NULL_HANDLE;
    double timestampPeriod_ = 0.0;
    bool resetAccumulation_ = true;
    bool firstSwapchainAcquire_ = true;  // ← Critical fix preserved

    // Runtime toggles
    bool hypertraceEnabled_     = Options::RTX::ENABLE_ADAPTIVE_SAMPLING;
    bool denoisingEnabled_      = Options::RTX::ENABLE_DENOISING;
    bool adaptiveSamplingEnabled_ = Options::RTX::ENABLE_ADAPTIVE_SAMPLING;
    bool overclockMode_         = false;
    FpsTarget fpsTarget_        = FpsTarget::FPS_120;
    TonemapType tonemapType_    = TonemapType::ACES;

    // AUTOEXPOSURE + TONEMAP STATE
    float currentExposure_     = 1.0f;
    float lastSceneLuminance_  = 0.18f;
    float nexusScore_          = 0.5f;
    uint32_t frameCount_       = 0;

    // GPU Resources — Samplers
    RTX::Handle<VkSampler> tonemapSampler_;
    RTX::Handle<VkSampler> envMapSampler_;

    // GPU Resources — AutoExposure
    RTX::Handle<VkBuffer>        luminanceHistogramBuffer_;
    RTX::Handle<VkDeviceMemory>  histogramMemory_;
    RTX::Handle<VkBuffer>        exposureBuffer_;
    RTX::Handle<VkDeviceMemory>  exposureMemory_;

    // GPU Resources — Tonemap
    RTX::Handle<VkPipeline>              tonemapPipeline_;
    RTX::Handle<VkPipelineLayout>        tonemapLayout_;
    RTX::Handle<VkDescriptorSetLayout>  tonemapDescriptorSetLayout_;
    std::vector<VkDescriptorSet>         tonemapSets_;
    VkDescriptorSet                      tonemapSet_ = VK_NULL_HANDLE;

    // Compute Pipelines — AutoExposure
    RTX::Handle<VkPipeline>       histogramPipeline_;
    RTX::Handle<VkPipelineLayout> histogramLayout_;
    VkDescriptorSet               histogramSet_ = VK_NULL_HANDLE;

    // Application sync & UI
    bool tonemapEnabled_ = true;
    bool showOverlay_    = true;
    int  renderMode_     = 1;

    // Performance
    std::chrono::steady_clock::time_point lastPerfLogTime_;
    uint32_t frameCounter_ = 0;

    // Sync objects
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkSemaphore> computeFinishedSemaphores_;
    std::vector<VkSemaphore> computeToGraphicsSemaphores_;
    std::vector<VkFence>     inFlightFences_;
    std::vector<VkFramebuffer> framebuffers_;

    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkCommandBuffer> computeCommandBuffers_;

    // Descriptor pools
    RTX::Handle<VkDescriptorPool> descriptorPool_;
    RTX::Handle<VkDescriptorPool> rtDescriptorPool_;
    RTX::Handle<VkDescriptorPool> tonemapDescriptorPool_;

    // Ray Tracing
    RTX::PipelineManager pipelineManager_;
    std::vector<VkDescriptorSet> rtDescriptorSets_;

    PFN_vkCmdTraceRaysKHR                    vkCmdTraceRaysKHR               = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR       vkCreateRayTracingPipelinesKHR  = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkGetBufferDeviceAddressKHR          vkGetBufferDeviceAddressKHR     = nullptr;

    // Buffers & Images
    std::vector<uint64_t> uniformBufferEncs_;
    std::vector<uint64_t> materialBufferEncs_;
    std::vector<uint64_t> dimensionBufferEncs_;
    std::vector<uint64_t> tonemapUniformEncs_;
    std::vector<RTX::Handle<VkImage>> rtOutputImages_;
    std::vector<RTX::Handle<VkDeviceMemory>> rtOutputMemories_;
    std::vector<RTX::Handle<VkImageView>> rtOutputViews_;
    std::vector<RTX::Handle<VkImage>> accumImages_;
    std::vector<RTX::Handle<VkDeviceMemory>> accumMemories_;
    std::vector<RTX::Handle<VkImageView>> accumViews_;
    RTX::Handle<VkImage> denoiserImage_;
    RTX::Handle<VkDeviceMemory> denoiserMemory_;
    RTX::Handle<VkImageView> denoiserView_;

    RTX::Handle<VkImage>        envMapImage_;
    RTX::Handle<VkDeviceMemory> envMapImageMemory_;
    RTX::Handle<VkImageView>    envMapImageView_;

    RTX::Handle<VkImage>        hypertraceScoreImage_;
    RTX::Handle<VkDeviceMemory> hypertraceScoreMemory_;
    RTX::Handle<VkImageView>    hypertraceScoreView_;
    RTX::Handle<VkBuffer>       hypertraceScoreStagingBuffer_;
    RTX::Handle<VkDeviceMemory> hypertraceScoreStagingMemory_;

    RTX::Handle<VkPipeline>       denoiserPipeline_;
    RTX::Handle<VkPipelineLayout> denoiserLayout_;
    std::vector<VkDescriptorSet>  denoiserSets_;

    // =============================================================================
    // NEW: Application pointer — enables `~` key ImGui console
    // =============================================================================
    Application* app_ = nullptr;

    // ──────────────────────────────────────────────────────────────────────────────
    // Private Helper Functions
    // ──────────────────────────────────────────────────────────────────────────────
    void createFramebuffers();
    void cleanupFramebuffers() noexcept;

    bool recreateTonemapUBOs() noexcept;
    void destroySharedStaging() noexcept;
    bool createSharedStaging() noexcept;

    void createAutoExposureResources() noexcept;
    void createTonemapPipeline() noexcept;
    void createTonemapSampler();

    VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool pool);
    void endSingleTimeCommands(VkDevice device, VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd);
    VkCommandBuffer allocateTransientCommandBuffer(VkDevice device, VkCommandPool pool);

    void destroyNexusScoreImage() noexcept;
    void destroyDenoiserImage() noexcept;
    void destroyAllBuffers() noexcept;
    void destroyAccumulationImages() noexcept;
    void destroyRTOutputImages() noexcept;
    void destroySBT() noexcept;

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
    void performTonemapPass(VkCommandBuffer cmd, uint32_t frameIdx, uint32_t swapImageIdx) noexcept;

    void updateUniformBuffer(uint32_t frame, const Camera& camera, float jitter);
    void updateTonemapUniform(uint32_t frame);

    uint32_t findMemoryType(VkPhysicalDevice pd, uint32_t filter, VkMemoryPropertyFlags props) const noexcept;

    void createImageArray(
        std::vector<RTX::Handle<VkImage>>& images,
        std::vector<RTX::Handle<VkDeviceMemory>>& memories,
        std::vector<RTX::Handle<VkImageView>>& views,
        const std::string& tag) noexcept;

    void createImage(RTX::Handle<VkImage>& image,
                     RTX::Handle<VkDeviceMemory>& memory,
                     RTX::Handle<VkImageView>& view,
                     const std::string& tag) noexcept;

    void dispatchLuminanceHistogram(VkCommandBuffer cmd, VkImage colorImage) noexcept;
    float computeSceneLuminanceFromHistogram() noexcept;
    void uploadToBuffer(RTX::Handle<VkBuffer>& buffer, const void* data, VkDeviceSize size) noexcept;
    void uploadToBufferImmediate(RTX::Handle<VkBuffer>& buffer, const void* data, VkDeviceSize size) noexcept;
    void downloadFromBuffer(RTX::Handle<VkBuffer>& buffer, void* data, VkDeviceSize size) noexcept;
    VkImage getCurrentHDRColorImage() noexcept;
    void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) noexcept;
};

// ──────────────────────────────────────────────────────────────────────────────
// Global Instance
// ──────────────────────────────────────────────────────────────────────────────
static std::unique_ptr<VulkanRenderer> g_renderer = nullptr;

[[nodiscard]] inline VulkanRenderer& getRenderer() { return *g_renderer; }

inline void initRenderer(int w, int h) {
    LOG_INFO_CAT("RENDERER", "Initializing VulkanRenderer ({}x{}) — PINK PHOTONS RISING", w, h);
    g_renderer = std::make_unique<VulkanRenderer>(w, h, nullptr, false);
    LOG_SUCCESS_CAT("RENDERER", "VulkanRenderer INITIALIZED — AUTOEXPOSURE v∞ — FIRST LIGHT ACHIEVED");
}

inline void renderFrame(const Camera& camera, float deltaTime) noexcept { if (g_renderer) g_renderer->renderFrame(camera, deltaTime); }
inline void shutdown() noexcept {
    LOG_INFO_CAT("RENDERER", "Shutting down — returning photons to the void");
    if (g_renderer) g_renderer->cleanup();
    g_renderer.reset();
    LOG_SUCCESS_CAT("RENDERER", "Shutdown complete — silence is golden");
}

// =============================================================================
// GPL-3.0+ — IMGUI CONSOLE READY — `~` KEY SUMMONS EMPIRE CONSOLE
// NOVEMBER 17 2025 — VALHALLA v80 TURBO — PINK PHOTONS ETERNAL — SHIP IT
// =============================================================================