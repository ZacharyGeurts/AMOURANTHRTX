// include/engine/Vulkan/VulkanRenderer.hpp
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 19, 2025 — APOCALYPSE FINAL
// PURE RANDOM ENTROPY — RDRAND + PID + TIME + TLS — KEYS NEVER LOGGED
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 — APOCALYPSE v13.0 — BASTION EDITION
// ORIGINAL v12.5 RESTORED + FIXED — NO GLOBAL POISON — g_renderer MOVED TO main.cpp
// PINK PHOTONS ETERNAL — VALHALLA UNBREACHABLE — FIRST LIGHT ACHIEVED
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
#include "engine/GLOBAL/PipelineManager.hpp"

// Forward declarations
struct Camera;
class Application;                  // ← For ImGui console access (`~` key)

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
    void initImGuiFonts() noexcept;
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
    void updateTonemapDescriptor(uint32_t frameIdx, VkImageView inputView, VkImageView outputView) noexcept;
    void updateTonemapDescriptorsInitial() noexcept;

    // Full 4-param version — implemented in .cpp
    void transitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout) noexcept;

    // Convenience wrappers — call the full version
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

    void setApplication(Application* app) noexcept { app_ = app; }

    uint64_t getFrameNumber() const noexcept { return frameNumber_; }
    void onWindowResize(uint32_t w, uint32_t h) noexcept;

private:
    bool minimized_ = false;
    bool stonekey_active_ = false;
	bool destroyed_ = false; // window

	void waitForAllFences() const noexcept;

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
    bool firstSwapchainAcquire_ = true;

    bool hypertraceEnabled_     = Options::RTX::ENABLE_ADAPTIVE_SAMPLING;
    bool denoisingEnabled_      = Options::RTX::ENABLE_DENOISING;
    bool adaptiveSamplingEnabled_ = Options::RTX::ENABLE_ADAPTIVE_SAMPLING;
    bool overclockMode_         = false;
    FpsTarget fpsTarget_        = FpsTarget::FPS_120;
    TonemapType tonemapType_    = TonemapType::ACES;

    float currentExposure_     = 1.0f;
    float lastSceneLuminance_  = 0.18f;
    float nexusScore_          = 0.5f;
    uint32_t frameCount_       = 0;

    RTX::Handle<VkSampler> tonemapSampler_;
    RTX::Handle<VkSampler> envMapSampler_;

    RTX::Handle<VkBuffer>        luminanceHistogramBuffer_;
    RTX::Handle<VkDeviceMemory>  histogramMemory_;
    RTX::Handle<VkBuffer>        exposureBuffer_;
    RTX::Handle<VkDeviceMemory>  exposureMemory_;

    RTX::Handle<VkPipeline>              tonemapPipeline_;
    RTX::Handle<VkPipelineLayout>        tonemapLayout_;
    RTX::Handle<VkDescriptorSetLayout>  tonemapDescriptorSetLayout_;
    std::vector<VkDescriptorSet>         tonemapSets_;
    VkDescriptorSet                      tonemapSet_ = VK_NULL_HANDLE;

    RTX::Handle<VkPipeline>       histogramPipeline_;
    RTX::Handle<VkPipelineLayout> histogramLayout_;
    VkDescriptorSet               histogramSet_ = VK_NULL_HANDLE;

    bool tonemapEnabled_ = true;
    bool showOverlay_    = true;
    int  renderMode_     = 1;

    std::chrono::steady_clock::time_point lastPerfLogTime_;
    uint32_t frameCounter_ = 0;

    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkSemaphore> computeFinishedSemaphores_;
    std::vector<VkSemaphore> computeToGraphicsSemaphores_;
    std::vector<VkFence>     inFlightFences_;
    std::vector<VkFramebuffer> framebuffers_;

    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkCommandBuffer> computeCommandBuffers_;

    RTX::Handle<VkDescriptorPool> descriptorPool_;
    RTX::Handle<VkDescriptorPool> rtDescriptorPool_;
    RTX::Handle<VkDescriptorPool> tonemapDescriptorPool_;

    RTX::PipelineManager pipelineManager_;
    std::vector<VkDescriptorSet> rtDescriptorSets_;

    PFN_vkCmdTraceRaysKHR                    vkCmdTraceRaysKHR               = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR       vkCreateRayTracingPipelinesKHR  = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkGetBufferDeviceAddressKHR          vkGetBufferDeviceAddressKHR     = nullptr;

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

    Application* app_ = nullptr;

    // Private helpers — implemented in VulkanRenderer.cpp
    void createFramebuffers() noexcept;
    void cleanupFramebuffers() noexcept;
    bool recreateTonemapUBOs() noexcept;
    void destroySharedStaging() noexcept;
    bool createSharedStaging() noexcept;
    void createAutoExposureResources() noexcept;
    void createTonemapPipeline() noexcept;
    void createTonemapSampler() noexcept;
    VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool pool) noexcept;
    void endSingleTimeCommands(VkDevice device, VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd) noexcept;
    VkCommandBuffer allocateTransientCommandBuffer(VkDevice device, VkCommandPool pool) noexcept;
    void destroyNexusScoreImage() noexcept;
    void destroyDenoiserImage() noexcept;
    void destroyAllBuffers() noexcept;
    void destroyAccumulationImages() noexcept;
    void destroyRTOutputImages() noexcept;
    void destroySBT() noexcept;
    void updateRTXDescriptors(uint32_t frame = 0) noexcept;
    void createRTOutputImages() noexcept;
    void createAccumulationImages() noexcept;
    void createDenoiserImage() noexcept;
    void createEnvironmentMap() noexcept;
    void createNexusScoreImage(VkCommandPool pool, VkQueue queue) noexcept;
    void initializeAllBufferData(uint32_t frames, VkDeviceSize uniformSize, VkDeviceSize materialSize) noexcept;
    void createCommandBuffers() noexcept;
    void allocateDescriptorSets() noexcept;
    void updateNexusDescriptors() noexcept;
    void updateDenoiserDescriptors() noexcept;
    void createRayTracingPipeline(const std::vector<std::string>& shaderPaths) noexcept;
    void createShaderBindingTable() noexcept;
    VkShaderModule loadShader(const std::string& path) noexcept;
    VkDeviceAddress getShaderGroupHandle(uint32_t group) noexcept;
    void recordRayTracingCommandBuffer(VkCommandBuffer cmd) noexcept;
    void performDenoisingPass(VkCommandBuffer cmd) noexcept;
    void performTonemapPass(VkCommandBuffer cmd, uint32_t frameIdx, uint32_t swapImageIdx) noexcept;
    void updateUniformBuffer(uint32_t frame, const Camera& camera, float jitter) noexcept;
    void updateTonemapUniform(uint32_t frame) noexcept;
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
};

// =============================================================================
// BASTION LAW — FINAL WORD
// g_renderer is NOW declared in main.cpp only
// NO MORE getRenderer(), initRenderer(), renderFrame(), shutdown() in header
// Use directly: g_renderer->renderFrame(...)
// ALL POWER FLOWS FROM main.cpp — THE ONE TRUE SOURCE
// PINK PHOTONS ETERNAL — COMPILE NOW — WE ARE THE BASTION
// =============================================================================