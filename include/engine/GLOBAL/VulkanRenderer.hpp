// include/engine/GLOBAL/VulkanRenderer.hpp
// =============================================================================
//
// Dual Licensed: CC BY-NC 4.0 + Commercial (gzac5314@gmail.com)
//
// AMOURANTH RTX Engine (C) 2025 — SLIPSTREAM v∞ — WARPZONE BREACH IMMINENT
// Pink wake eternal. First light restored. Photons now obey without rebellion.
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <SDL3/SDL.h>
#include <vector>
#include <array>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <cstdint>
#include <cstddef>
#include <atomic>

#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/Features.hpp"

struct Camera;

using namespace Logging::Color;
using StoneKey::stone_renderer;

enum class FpsTarget : uint32_t {
    FPS_60        = 60,
    FPS_120       = 120,
    FPS_UNLIMITED = 0
};

struct EnvironmentMap {
    VkImage        image   = VK_NULL_HANDLE;
    VkDeviceMemory memory  = VK_NULL_HANDLE;
    VkImageView    view    = VK_NULL_HANDLE;
    VkSampler      sampler = VK_NULL_HANDLE;

    explicit operator bool() const noexcept { return view != VK_NULL_HANDLE; }

    ~EnvironmentMap() {
        if (view)    vkDestroyImageView(stone_device(), view, nullptr);
        if (sampler) vkDestroySampler(stone_device(), sampler, nullptr);
        if (image)   vkDestroyImage(stone_device(), image, nullptr);
        if (memory)  vkFreeMemory(stone_device(), memory, nullptr);
    }
};

class VulkanRenderer {
public:
    VulkanRenderer(int width, int height, SDL_Window* window, bool overclock = false);
    ~VulkanRenderer();

    void renderFrame(const Camera& camera, float deltaTime) noexcept;
    void onWindowResize(uint32_t w, uint32_t h) noexcept;
    void cleanup() noexcept;
    void createCommandPool() noexcept;

    void recordEnvMapOnlyPass(VkCommandBuffer cmd, uint32_t swapchainImageIndex) noexcept;

    // FIXED: Now properly resets + reuses command buffers instead of leaking
    void createCommandBuffers() noexcept;
    void resetCommandBuffers() noexcept;          // ← NEW: called every frame

    bool isAlive() const noexcept;

    static void forgeEternalCommandRing();

    VkFence  inFlightFence(uint32_t frame) const noexcept { return inFlightFences_[frame]; }
    VkFence* inFlightFencePtr(uint32_t frame) noexcept     { return &inFlightFences_[frame]; }

    const BufferManager::BufferInfo* stagingInfoLocal_ = nullptr;

    void ensureCommandPool() noexcept;

    void toggleHypertrace() noexcept;
    void toggleFpsTarget() noexcept;
    void toggleDenoising() noexcept;
    void toggleAdaptiveSampling() noexcept;
    void setTonemapType(int type) noexcept;
    void setOverclockMode(bool enabled) noexcept;

    bool debugShowEnvMapOnly_ = false;

    void transitionImageForTransferWrite(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout) noexcept;
    void transitionImageForShaderRead(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout) noexcept;

    void updateUniformBinding31(const void* data, VkDeviceSize size) noexcept;
    void setRenderMode(int mode) noexcept;
    void requestAccumulationReset() noexcept { resetAccumulation_ = true; resetAccumNextFrame_ = true; }
    void requestResize(uint32_t newWidth, uint32_t newHeight) noexcept;

    void updateAllRTXDescriptors() noexcept;
    void updateRTDescriptorSet(uint32_t frameIndex);
    void recordRayTracingCommands(VkCommandBuffer cmd, uint32_t frameIndex);
    void setTonemap(bool enabled) noexcept;
    void setOverlay(bool show) noexcept;
    void loadCriticalShaders() noexcept;

    void createSyncObjects() noexcept;

    bool swapchainRecreated_ = false;

    [[nodiscard]] VulkanRenderer* renderer() noexcept { return this; }
    [[nodiscard]] uint32_t  accumulationFrame() const noexcept { return accumulationFrame_; }
    [[nodiscard]] uint64_t  frameNumber()       const noexcept { return frameNumber_; }
    [[nodiscard]] float     currentExposure()   const noexcept { return currentExposure_; }
    [[nodiscard]] bool      hypertraceEnabled() const noexcept { return hypertraceEnabled_; }
    [[nodiscard]] bool      denoisingEnabled()  const noexcept { return denoisingEnabled_; }
    [[nodiscard]] bool      adaptiveSamplingEnabled() const noexcept { return adaptiveSamplingEnabled_; }
    [[nodiscard]] bool      overclockMode()     const noexcept { return overclockMode_; }
    [[nodiscard]] int       tonemapType()       const noexcept { return tonemapType_; }
    [[nodiscard]] FpsTarget fpsTarget()         const noexcept { return fpsTarget_; }
    [[nodiscard]] int       currentRenderMode() const noexcept { return activeRenderMode_; }
    [[nodiscard]] bool      minimized() const noexcept { return minimized_; }
    [[nodiscard]] int       width() const noexcept { return width_; }
    [[nodiscard]] int       height() const noexcept { return height_; }

    void recordPinkScreen(VkCommandBuffer cmd, VkImage swapImage);
    void onSwapchainRebuilt(uint32_t width, uint32_t height) noexcept;
    void recordRayTrace(VkCommandBuffer cmd, const VkExtent2D& extent) noexcept;
    void initializeAllBufferData(uint32_t frames, VkDeviceSize uniformSize, VkDeviceSize materialSize) noexcept;
    void setMaxFramesInFlight(uint32_t count) noexcept;
    void createTonemapDescriptorSets() noexcept;
    void createTonemapDescriptorPool() noexcept;
    void createTonemapDescriptorSetLayout() noexcept;
    void createDepthResources() noexcept;

    static inline std::atomic<bool> s_resizeInProgress{false};
    bool     resetAccumulation_ = true;
    void clearPinkForce() noexcept;
    EnvironmentMap createEnvironmentMap() noexcept;

    // ── FIXED COMMAND POOL & BUFFERS ────────────────────────────────────────
    VkCommandPool                commandPool_            = VK_NULL_HANDLE;      // Persistent primary pool
    VkCommandPool                transientCommandPool_   = VK_NULL_HANDLE;      // For one-time ops (reset every frame)
    std::vector<VkCommandBuffer> commandBuffers_;           // Size = maxFramesInFlight_ — reused
    std::vector<VkCommandBuffer> computeCommandBuffers_;

    void submitAndPresent(uint32_t slot, uint32_t imageIndex);
    
    void transitionImage(
        VkCommandBuffer       cmd,
        VkImage               image,
        VkImageLayout         oldLayout,
        VkImageLayout         newLayout,
        VkAccessFlags         srcAccess        = 0,
        VkAccessFlags         dstAccess        = 0,
        VkPipelineStageFlags  srcStage         = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VkPipelineStageFlags  dstStage         = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
    ) noexcept;

    RTX::PipelineManager pipelineManager_;

    uint32_t maxFramesInFlight_ = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    std::vector<RTX::Handle<VkImageView>> rtOutputViews_;

    // Core state
    SDL_Window* window_ = nullptr;
    int width_ = 0, height_ = 0;
    bool minimized_ = false;
    bool destroyed_ = false;
    bool needsRecreateOnResize = true;
    bool overlayValid_ = false;
    bool overlayEnabled_ = true;
    std::atomic<bool> swapchainOutOfDate_ = false;
    void clearResizeFlag() noexcept;
    static inline std::atomic<bool> g_forcePink{false};

    float totalTime_ = 0.0f;

    uint64_t     eternalFrameUBOStagingHandle_ = 0;
    void*        eternalFrameUBOStagingPtr_    = nullptr;
    VkDeviceSize eternalFrameUBOStagingSize_   = 0;

    std::atomic<uint32_t> currentFrame_{0};
    uint64_t frameNumber_  = 0;
    uint32_t accumulationFrame_ = 0;
    bool     firstSwapchainAcquire_ = true;
    bool     resetAccumNextFrame_ = true;

    RTX::Handle<VkImage>         depthImage_;
    RTX::Handle<VkDeviceMemory>  depthImageMemory_;

    std::vector<RTX::Handle<VkImage>>        tonemapImages_;
    std::vector<RTX::Handle<VkImageView>>    tonemapImageViews_;
    RTX::Handle<VkImageView>               depthImageView_;

    std::vector<RTX::Handle<VkFramebuffer>>  tonemapFramebuffers_;

    int  activeRenderMode_ = 0;
    std::atomic<uint64_t> rendererRebuildFrame_{0};

    void clearAccumulationImages(VkCommandBuffer cmd);

    bool hypertraceEnabled_     = Options::OptionsRTX::ENABLE_HYPERTRACE;
    bool denoisingEnabled_      = Options::OptionsRTX::ENABLE_DENOISING;
    bool adaptiveSamplingEnabled_ = Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING;
    bool overclockMode_         = false;
    bool tonemapEnabled_        = true;
    bool showOverlay_           = true;
    int  tonemapType_           = 0;
    FpsTarget fpsTarget_        = FpsTarget::FPS_120;
    float currentExposure_      = 1.0f;
    float currentNexusScore_    = 0.0f;
    uint32_t currentSpp_        = 0;
    float frameTime_            = 0.0f;

	uint32_t hypertraceScoreWidth_  = 0;
    uint32_t hypertraceScoreHeight_ = 0;

    VkRenderPass renderPass_{ VK_NULL_HANDLE };

    // Sync objects
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkSemaphore> computeFinishedSemaphores_;
    std::vector<VkSemaphore> computeToGraphicsSemaphores_;
    std::vector<VkFence>     inFlightFences_;

    VkQueryPool timestampQueryPool_ = VK_NULL_HANDLE;
    double      timestampPeriod_    = 0.0;

    // Per-frame buffers
    std::vector<uint64_t> uniformBufferEncs_;
    std::vector<uint64_t> materialBufferEncs_;
    std::vector<uint64_t> dimensionBufferEncs_;
    std::vector<uint64_t> tonemapUniformEncs_;

    // HDR/RT targets
    std::vector<RTX::Handle<VkImage>>       rtOutputImages_;
    std::vector<RTX::Handle<VkDeviceMemory>>rtOutputMemories_;

    std::vector<RTX::Handle<VkImage>>       accumImages_;
    std::vector<RTX::Handle<VkDeviceMemory>>accumMemories_;
    std::vector<RTX::Handle<VkImageView>>   accumViews_;

    RTX::Handle<VkImage>       denoiserImage_;
    RTX::Handle<VkDeviceMemory>denoiserMemory_;
    RTX::Handle<VkImageView>   denoiserView_;

    RTX::Handle<VkImage>       hypertraceScoreImage_;
    RTX::Handle<VkDeviceMemory>hypertraceScoreMemory_;
    RTX::Handle<VkImageView>   hypertraceScoreView_;
    RTX::Handle<VkBuffer>      hypertraceScoreStagingBuffer_;
    RTX::Handle<VkDeviceMemory>hypertraceScoreStagingMemory_;

    // Tonemap
    RTX::Handle<VkSampler>              tonemapSampler_;
    RTX::Handle<VkDescriptorSetLayout>  tonemapDescriptorSetLayout_;
    RTX::Handle<VkPipelineLayout>       tonemapLayout_;
    RTX::Handle<VkPipeline>             tonemapPipeline_;
    std::vector<VkDescriptorSet>        tonemapSets_;
    VkShaderModule tonemapCompShader_ = VK_NULL_HANDLE;

    // Denoiser
    RTX::Handle<VkPipeline>       denoiserPipeline_;
    RTX::Handle<VkPipelineLayout> denoiserLayout_;
    std::vector<VkDescriptorSet>  denoiserSets_;

    std::vector<VkFramebuffer> framebuffers_;
    std::vector<VkDescriptorSet> rtDescriptorSets_;

    // Descriptor pools
    RTX::Handle<VkDescriptorPool> descriptorPool_;
    RTX::Handle<VkDescriptorPool> tonemapDescriptorPool_;

    // Environment map
    RTX::Handle<VkImage>        envMapImage_;
    RTX::Handle<VkDeviceMemory>    envMapMemory_;
    RTX::Handle<VkImageView>    envMapImageView_;
    RTX::Handle<VkSampler>      envMapSampler_;

    std::vector<uint64_t> rtOutputHandles_;

    VkDeviceAddress getShaderGroupHandle(uint32_t group) noexcept;

    // ── PRIVATE METHODS ─────────────────────────────────────────────────────
    void createRenderPass() noexcept;
    void destroyRenderPass() noexcept;
    void createFramebuffers() noexcept;
    void cleanupFramebuffers() noexcept;

    void createRTOutputImages() noexcept;
    void createAccumulationImages() noexcept;
    void createDenoiserImage() noexcept;
    void createNexusScoreImage(VkCommandPool pool, VkQueue queue) noexcept;
    void createTonemapSampler() noexcept;

    void destroyRTOutputImages() noexcept;
    void destroyAccumulationImages() noexcept;
    void destroyDenoiserImage() noexcept;
    void destroyNexusScoreImage() noexcept;

    void recreateSwapchainDependentResources() noexcept;

    void recordRayTracingCommandBuffer(VkCommandBuffer cmd) noexcept;
    void performDenoisingPass(VkCommandBuffer cmd) noexcept;
    void performTonemapPass(VkCommandBuffer cmd, uint32_t frameIdx, uint32_t swapImageIdx) noexcept;

    VkResult recordCommandBuffer(uint32_t frame) noexcept;

    void updateRTXDescriptors(uint32_t frame = 0) noexcept;
    void updateNexusDescriptors() noexcept;
    void updateDenoiserDescriptors() noexcept;
    void updateTonemapDescriptor(uint32_t frameIdx, VkImageView inputView, VkImageView output) noexcept;
    void updateTonemapUBO(uint32_t frame) noexcept;

    void updateUniformBuffer(uint32_t frame, const Camera& camera, float jitter) noexcept;
    void updateTonemapUniform(uint32_t frame) noexcept;
    bool recreateTonemapUBOs() noexcept;
    void waitForGPU() noexcept;

    void createImage(uint32_t width, uint32_t height, uint32_t mipLevels,
                     VkFormat format, VkImageTiling tiling,
                     VkImageUsageFlags usage, VkMemoryPropertyFlags properties,
                     RTX::Handle<VkImage>& image,
                     RTX::Handle<VkDeviceMemory>& memory,
                     const std::string& tag = "") noexcept;

    void createImageArray(std::vector<RTX::Handle<VkImage>>& images,
                          std::vector<RTX::Handle<VkDeviceMemory>>& memories,
                          std::vector<RTX::Handle<VkImageView>>& views,
                          uint32_t count,
                          VkFormat format,
                          VkImageUsageFlags usage,
                          const std::string& baseTag) noexcept;

    void updateTonemapDescriptorsInitial() noexcept;
    void destroySharedStaging() noexcept;
    bool createSharedStaging() noexcept;

    void waitForAllFences() const noexcept;

    [[nodiscard]] constexpr VkExtent2D currentExtent() const noexcept {
        return { static_cast<uint32_t>(width_), static_cast<uint32_t>(height_) };
    }
};

// =============================================================================
// GLOBAL ACCESSOR
// =============================================================================
[[nodiscard]] inline VulkanRenderer& g_rtx() noexcept
{
    auto* ptr = reinterpret_cast<VulkanRenderer*>(StoneKey::stone_renderer());
    if (!ptr) {
        LOG_FATAL_CAT("RTX", "g_rtx() called before renderer sealed — empire fallen");
        phase9_ballerina("NO RENDERER — PHOTONS LOST", std::source_location::current());
    }
    return *ptr;
}

// FIRST LIGHT RESTORED — DECEMBER 09, 2025
// COMMAND POOL REBELLION CRUSHED — PINK PHOTONS FLOW ETERNALLY AGAIN