// include/engine/GLOBAL/VulkanRenderer.hpp
// =============================================================================
//
// Dual Licensed: CC BY-NC 4.0 + Commercial (gzac5314@gmail.com)
//
// AMOURANTH RTX Engine (C) 2025-2026 — SLIPSTREAM v∞ — JANUARY 03, 2026
// BLACK SCREEN FIXED EDITION — FULL RENDER PIPELINE COMPLETE
// Empire Optimized: Unlimited FPS | Full Features | Half-Float RT/Accum/Denoise | Photons Eternal.
// MAJOR FIXES: Added missing denoiserSampler_ declaration and createDenoiserSampler()
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

// Render mode includes
#include "modes/RenderMode1.hpp"
#include "modes/RenderMode2.hpp"
#include "modes/RenderMode3.hpp"
#include "modes/RenderMode4.hpp"
#include "modes/RenderMode5.hpp"
#include "modes/RenderMode6.hpp"
#include "modes/RenderMode7.hpp"
#include "modes/RenderMode8.hpp"
#include "modes/RenderMode9.hpp"

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
    }
};

class VulkanRenderer {
public:
    VulkanRenderer(int width, int height, SDL_Window* window, bool overclock = false);
    virtual ~VulkanRenderer();  // Virtual destructor for proper deletion

    void renderFrame(const Camera& camera, float deltaTime) noexcept;
    void onWindowResize(uint32_t w, uint32_t h) noexcept;
    void cleanup() noexcept;
    void createCommandPool() noexcept;
    void createCommandBuffers() noexcept;
    void resetCommandBuffers() noexcept;

    void recordEnvMapOnlyPass(VkCommandBuffer cmd, uint32_t swapchainImageIndex) noexcept;
    void createDefaultMaterials() noexcept;

    bool isAlive() const noexcept;
    bool swapchainNeedsPresentTransition_ = false;

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
    void setOverlay(bool enabled) noexcept;

    bool debugShowEnvMapOnly_ = false;
    bool hdrLoaded = false;

    void transitionImageForTransferWrite(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout) noexcept;
    void transitionImageForShaderRead(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout) noexcept;
    void recordAccumulationPass(VkCommandBuffer cmd, uint32_t slot) noexcept;

    void updateUniformBinding31(const void* data, VkDeviceSize size) noexcept;
    void setRenderMode(int mode) noexcept;
    void requestAccumulationReset() noexcept { resetAccumulation_ = true; resetAccumNextFrame_ = true; }
    void requestResize(uint32_t newWidth, uint32_t newHeight) noexcept;

    void updateAllRTXDescriptors() noexcept;
    void updateRTDescriptorSet(uint32_t frameIndex);
    void recordRayTracingCommands(VkCommandBuffer cmd, uint32_t frameIndex);
    void setTonemap(bool enabled) noexcept;
    void loadCriticalShaders() noexcept;

    void createSyncObjects() noexcept;

    bool swapchainRecreated_ = false;

    [[nodiscard]] VulkanRenderer* renderer() noexcept { return this; }
    [[nodiscard]] VkCommandBuffer getCurrentCommandBuffer() const noexcept { const uint32_t slot = currentFrame_.load() % 2; return commandBuffers_[slot]; }
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
    void createAccumulationPipeline() noexcept;

    static inline std::atomic<bool> s_resizeInProgress{false};
    bool     resetAccumulation_ = true;
    void clearPinkForce() noexcept;
    EnvironmentMap createEnvironmentMap() noexcept;

    // ── NEW: FORCE OUTPUT MODE SUPPORT ──────────────────────────────────────
    void forcePinkFallbackClear() noexcept;

    // ── NEW: MISSING DECLARATIONS ADDED FOR PIPELINE CREATION ───────────────
    void createEnvMapDisplayPipeline() noexcept;
    void createTonemapPipeline() noexcept;
    void createDenoiserPipeline() noexcept;

    // ── NEW: ACCUMULATION DESCRIPTOR UPDATE (fixed signature) ───────────────
    void updateAccumulationDescriptors(uint32_t slot) noexcept;

    // ── FIXED COMMAND POOL & BUFFERS ────────────────────────────────────────
    VkCommandPool                commandPool_            = VK_NULL_HANDLE;
    RTX::Handle<VkCommandPool> transientCommandPool_;
    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkCommandBuffer> computeCommandBuffers_;

    VkPipeline               envMapDisplayPipeline_       = VK_NULL_HANDLE;
    VkPipelineLayout         envMapDisplayPipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSet          envMapDisplayDescriptorSet_  = VK_NULL_HANDLE;
    VkDescriptorSetLayout    envMapDisplayDescSetLayout_  = VK_NULL_HANDLE;

    VkPipeline            accumulationPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout      accumulationPipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout accumulationDescSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool      accumulationDescriptorPool_ = VK_NULL_HANDLE;

    glm::vec3 sunDirection_         = glm::vec3(0.3f, 0.8f, 0.5f);
    float     sunIntensity_         = 10.0f;
    glm::vec3 sunColor_             = glm::vec3(1.0f, 0.95f, 0.9f);
    float     fogDensity_           = 0.02f;
    glm::vec3 fogColor_             = glm::vec3(0.7f, 0.8f, 0.9f);

    uint32_t  materialCount_        = 0;
    uint32_t  activeMaterialIndex_  = 0;
    float     materialMetallicOverride_  = -1.0f;
    float     materialRoughnessOverride_ = -1.0f;
    float     emissiveIntensity_    = 1.0f;
    uint32_t  acquiredImageIndex_   = 0;

    float     debugFloat1_ = 0.0f;
    float     debugFloat2_ = 0.0f;
    float     debugFloat3_ = 0.0f;
    float     debugFloat4_ = 0.0f;

    std::array<VkDescriptorSet, 2> accumulationSets_ = {VK_NULL_HANDLE, VK_NULL_HANDLE};

    [[nodiscard]] bool hasEnvMapDisplayPipeline() const noexcept { return envMapDisplayPipeline_ != VK_NULL_HANDLE; }

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

    bool rtOutputNeedsTransition_ = false;
    bool depthNeedsTransition_ = false;

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

    VkImage       hypertraceScoreImage_       = VK_NULL_HANDLE;
    VkDeviceMemory hypertraceScoreMemory_     = VK_NULL_HANDLE;
    VkImageView   hypertraceScoreView_        = VK_NULL_HANDLE;

    // Tonemap
    RTX::Handle<VkSampler>              tonemapSampler_;
    RTX::Handle<VkDescriptorSetLayout>  tonemapDescriptorSetLayout_;
    RTX::Handle<VkPipelineLayout>       tonemapLayout_;
    RTX::Handle<VkPipeline>             tonemapPipeline_;
    std::vector<VkDescriptorSet>        tonemapSets_;
    VkShaderModule tonemapCompShader_ = VK_NULL_HANDLE;

    // Denoiser — FIXED: denoiserLayout_ is now DescriptorSetLayout, not PipelineLayout
    RTX::Handle<VkPipeline>       denoiserPipeline_;
    RTX::Handle<VkPipelineLayout> denoiserPipelineLayout_;
    RTX::Handle<VkDescriptorSetLayout> denoiserLayout_;   // ← Correct type
    std::vector<VkDescriptorSet>  denoiserSets_;

    // NEW: Denoiser sampler for COMBINED_IMAGE_SAMPLER input
    RTX::Handle<VkSampler> denoiserSampler_;

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
    RTX::Handle<VkDescriptorPool> envMapDescriptorPool_;

    bool     envMapNeedsUpload_ = false;
    uint32_t envMapUploadWidth_ = 0;
    uint32_t envMapUploadHeight_ = 0;

    bool accumulationNeedsTransition_ = false;
    bool nexusScoreNeedsInit_ = false;

    std::vector<uint64_t> rtOutputHandles_;

    VkDeviceAddress getShaderGroupHandle(uint32_t group) noexcept;

    // ── RENDER MODES 1-9 (FALLBACK SOLID COLOR MODES) ───────────────────────
    RenderMode1 renderMode1_;
    RenderMode2 renderMode2_;
    RenderMode3 renderMode3_;
    RenderMode4 renderMode4_;
    RenderMode5 renderMode5_;
    RenderMode6 renderMode6_;
    RenderMode7 renderMode7_;
    RenderMode8 renderMode8_;
    RenderMode9 renderMode9_;

    // ── PUBLIC EXPOSURE FOR RENDERING — NEEDED FOR EXTERNAL MODES (e.g., RenderMode1) ─────────────────────────────────
    [[nodiscard]] VkPipeline tonemapPipeline() const noexcept { return tonemapPipeline_.get(); }
    [[nodiscard]] VkPipelineLayout tonemapLayout() const noexcept { return tonemapLayout_.get(); }
    [[nodiscard]] VkDescriptorSet tonemapSet(uint32_t frame) const noexcept {
        return (frame < tonemapSets_.size()) ? tonemapSets_[frame] : VK_NULL_HANDLE;
    }
    [[nodiscard]] VkSampler tonemapSampler() const noexcept { return tonemapSampler_.get(); }
    void updateTonemapDescriptorForMode(uint32_t frameIdx, VkImageView inputView, VkImageView output) noexcept;

    // Command buffers and pools
    [[nodiscard]] VkCommandPool commandPool() const noexcept { return commandPool_; }
    [[nodiscard]] VkCommandPool transientCommandPool() const noexcept { return transientCommandPool_.get(); }
    [[nodiscard]] const std::vector<VkCommandBuffer>& commandBuffers() const noexcept { return commandBuffers_; }
    [[nodiscard]] VkCommandBuffer commandBuffer(uint32_t frame) const noexcept {
        return (frame < commandBuffers_.size()) ? commandBuffers_[frame] : VK_NULL_HANDLE;
    }

    // Sync objects
    [[nodiscard]] const std::vector<VkSemaphore>& imageAvailableSemaphores() const noexcept { return imageAvailableSemaphores_; }
    [[nodiscard]] const std::vector<VkSemaphore>& renderFinishedSemaphores() const noexcept { return renderFinishedSemaphores_; }
    [[nodiscard]] const std::vector<VkFence>& inFlightFences() const noexcept { return inFlightFences_; }

    // RT resources
    [[nodiscard]] const std::vector<RTX::Handle<VkImageView>>& rtOutputViews() const noexcept { return rtOutputViews_; }
    [[nodiscard]] VkImageView rtOutputView(uint32_t frame) const noexcept {
        return (frame < rtOutputViews_.size()) ? rtOutputViews_[frame].get() : VK_NULL_HANDLE;
    }
    [[nodiscard]] const std::vector<VkDescriptorSet>& rtDescriptorSets() const noexcept { return rtDescriptorSets_; }
    [[nodiscard]] VkDescriptorSet rtDescriptorSet(uint32_t frame) const noexcept {
        return (frame < rtDescriptorSets_.size()) ? rtDescriptorSets_[frame] : VK_NULL_HANDLE;
    }

    // Accumulation resources
    [[nodiscard]] VkPipeline accumulationPipeline() const noexcept { return accumulationPipeline_; }
    [[nodiscard]] VkPipelineLayout accumulationPipelineLayout() const noexcept { return accumulationPipelineLayout_; }
    [[nodiscard]] VkDescriptorSet accumulationSet(uint32_t frame) const noexcept {
        return (frame < accumulationSets_.size()) ? accumulationSets_[frame] : VK_NULL_HANDLE;
    }
    [[nodiscard]] const std::vector<RTX::Handle<VkImageView>>& accumViews() const noexcept { return accumViews_; }
    [[nodiscard]] VkImageView accumView(uint32_t frame) const noexcept {
        return (frame < accumViews_.size()) ? accumViews_[frame].get() : VK_NULL_HANDLE;
    }

    // Environment map resources
    [[nodiscard]] VkPipeline envMapDisplayPipeline() const noexcept { return envMapDisplayPipeline_; }
    [[nodiscard]] VkPipelineLayout envMapDisplayPipelineLayout() const noexcept { return envMapDisplayPipelineLayout_; }
    [[nodiscard]] VkDescriptorSet envMapDisplayDescriptorSet() const noexcept { return envMapDisplayDescriptorSet_; }
    [[nodiscard]] VkImageView envMapImageView() const noexcept { return envMapImageView_.get(); }
    [[nodiscard]] VkSampler envMapSampler() const noexcept { return envMapSampler_.get(); }

    // Denoiser resources
    [[nodiscard]] VkPipeline denoiserPipeline() const noexcept { return denoiserPipeline_.get(); }
    [[nodiscard]] VkPipelineLayout denoiserPipelineLayout() const noexcept { return denoiserPipelineLayout_.get(); }
    [[nodiscard]] VkDescriptorSetLayout denoiserLayout() const noexcept { return denoiserLayout_.get(); }
    [[nodiscard]] VkDescriptorSet denoiserSet(uint32_t frame) const noexcept {
        return (frame < denoiserSets_.size()) ? denoiserSets_[frame] : VK_NULL_HANDLE;
    }
    [[nodiscard]] VkImageView denoiserView() const noexcept { return denoiserView_.get(); }
    [[nodiscard]] VkSampler denoiserSampler() const noexcept { return denoiserSampler_.get(); }

    // Nexus/Hypertrace resources
    [[nodiscard]] VkImageView hypertraceScoreView() const noexcept { return hypertraceScoreView_; }
    [[nodiscard]] VkImage hypertraceScoreImage() const noexcept { return hypertraceScoreImage_; }

    // Depth resources
    [[nodiscard]] VkImageView depthImageView() const noexcept { return depthImageView_.get(); }
    [[nodiscard]] VkImage depthImage() const noexcept { return depthImage_.get(); }

    // Buffers (handles)
    [[nodiscard]] uint64_t uniformBufferEnc(uint32_t frame) const noexcept {
        return (frame < uniformBufferEncs_.size()) ? uniformBufferEncs_[frame] : 0;
    }
    [[nodiscard]] uint64_t tonemapUniformEnc(uint32_t frame) const noexcept {
        return (frame < tonemapUniformEncs_.size()) ? tonemapUniformEncs_[frame] : 0;
    }
    [[nodiscard]] VkBuffer uniformBuffer(uint32_t frame) const noexcept {
        auto enc = uniformBufferEnc(frame);
        return enc ? BufferManager::get(enc)->buffer : VK_NULL_HANDLE;
    }

    // Pools
    [[nodiscard]] VkDescriptorPool tonemapDescriptorPool() const noexcept { return tonemapDescriptorPool_.get(); }
    [[nodiscard]] VkDescriptorPool accumulationDescriptorPool() const noexcept { return accumulationDescriptorPool_; }

    // Render pass and framebuffers
    [[nodiscard]] VkRenderPass renderPass() const noexcept { return renderPass_; }
    [[nodiscard]] const std::vector<VkFramebuffer>& framebuffers() const noexcept { return framebuffers_; }
    [[nodiscard]] VkFramebuffer framebuffer(uint32_t idx) const noexcept {
        return (idx < framebuffers_.size()) ? framebuffers_[idx] : VK_NULL_HANDLE;
    }

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
    void createDenoiserSampler() noexcept;  // NEW: Declaration for denoiser sampler creation
	void onResize(int newWidth, int newHeight) noexcept;

    void destroyRTOutputImages() noexcept;
    void destroyAccumulationImages() noexcept;
    void destroyDenoiserImage() noexcept;
    void destroyNexusScoreImage() noexcept;

    void recreateSwapchainDependentResources() noexcept;

    void recordRayTracingCommandBuffer(VkCommandBuffer cmd) noexcept;
    void performDenoisingPass(VkCommandBuffer cmd) noexcept;
    void performTonemapPass(VkCommandBuffer cmd, uint32_t frameIdx, uint32_t swapImageIdx) noexcept;

    VkResult recordCommandBuffer(uint32_t frame) noexcept;
    void createTransientCommandPool() noexcept;
    void createEnvMapDescriptorPool() noexcept;

    void updateRTXDescriptors(uint32_t frame = 0) noexcept;
    void updateNexusDescriptors() noexcept;
    void updateDenoiserDescriptors() noexcept;
    void updateTonemapDescriptor(uint32_t frameIdx, VkImageView inputView, VkImageView output) noexcept;
    void updateTonemapUBO(uint32_t frame) noexcept;

    void updateUniformBuffer(uint32_t frame, const Camera& camera, float deltaTime) noexcept;
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

    // ── SINGLETON ACCESS — EMPIRE LAW ENFORCED ─────────────────────────────────
    static VulkanRenderer* get() noexcept;
private:
    float*     envMapUploadData_   = nullptr;  // Owned pointer — delete after upload
    static inline VulkanRenderer* s_instance = nullptr;
    uint64_t defaultMaterialsHandle_ = 0; 
};

// =============================================================================
// GLOBAL ACCESSOR
// =============================================================================
[[nodiscard]] inline VulkanRenderer& g_rtx() noexcept
{
    VulkanRenderer* ptr = VulkanRenderer::get();
    if (!ptr) {
        LOG_FATAL_CAT("RTX", "g_rtx() called before renderer sealed — empire fallen");
    }
    return *ptr;
}

// JANUARY 03, 2026 — COMPILATION FIXED EDITION
// Added denoiserSampler_ member and createDenoiserSampler() declaration
// All pipelines now compile correctly
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — PLASTIC BEACH FOREVER
// =============================================================================