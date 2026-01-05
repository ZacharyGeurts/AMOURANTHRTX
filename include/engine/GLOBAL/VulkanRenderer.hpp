// =============================================================================
//
// Dual Licensed: CC BY-NC 4.0 + Commercial (gzac5314@gmail.com)
//
// AMOURANTH RTX Engine (C) 2025-2026 — SLIPSTREAM v∞ — JANUARY 05, 2026
// FINAL FIXED HEADER — 100% COMPILATION CLEAN — ALL MEMBERS & ACCESSORS CORRECT
// Empire Optimized: Unlimited FPS | Full Features | Half-Float RT/Accum/Denoise | Photons Eternal.
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
};

class VulkanRenderer {
public:
    VulkanRenderer(int width, int height, SDL_Window* window, bool overclock = false);
    virtual ~VulkanRenderer();

    void renderFrame(const Camera& camera, float deltaTime) noexcept;
    void onResize(int newWidth, int newHeight) noexcept;
    void cleanup() noexcept;

    void createCommandPool() noexcept;
    void createCommandBuffers() noexcept;
    void resetCommandBuffers() noexcept;

    void recordEnvMapOnlyPass(VkCommandBuffer cmd, uint32_t swapchainImageIndex) noexcept;
    void createDefaultMaterials() noexcept;

    bool isAlive() const noexcept;

    static void forgeEternalCommandRing();

    VkFence  inFlightFence(uint32_t frame) const noexcept { return inFlightFences_[frame]; }
    VkFence* inFlightFencePtr(uint32_t frame) noexcept     { return &inFlightFences_[frame]; }

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

    void transitionImage(
        VkCommandBuffer       cmd,
        VkImage               image,
        VkImageLayout         oldLayout,
        VkImageLayout         newLayout,
        VkAccessFlags         srcAccess = 0,
        VkAccessFlags         dstAccess = 0,
        VkPipelineStageFlags  srcStage  = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VkPipelineStageFlags  dstStage  = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT
    ) noexcept;

    RTX::PipelineManager pipelineManager_;

    void setMaxFramesInFlight(uint32_t count) noexcept;
    void recreateSwapchainDependentResources() noexcept;

    // === ALL REQUIRED METHODS ===
    EnvironmentMap createEnvironmentMap() noexcept;
    void createEnvMapDescriptorPool() noexcept;
    void createEnvMapDisplayPipeline() noexcept;
    void createDepthResources() noexcept;
    void createRTOutputImages() noexcept;
    void createAccumulationImages() noexcept;
    void createAccumulationPipeline() noexcept;
    void createNexusScoreImage(VkCommandPool pool, VkQueue queue) noexcept;
    void createTonemapSampler() noexcept;
    void createDenoiserSampler() noexcept;
    void createDenoiserPipeline() noexcept;
    void createTonemapPipeline() noexcept;
    void createTonemapDescriptorPool() noexcept;
    void createTonemapDescriptorSetLayout() noexcept;
    void createTonemapDescriptorSets() noexcept;

    void submitAndPresent(uint32_t slot, uint32_t imageIndex);
    void waitForGPU() noexcept;
    void clearAccumulationImages(VkCommandBuffer cmd);
    void transitionImageForTransferWrite(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout) noexcept;
    void transitionImageForShaderRead(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout) noexcept;
    void recordRayTracingCommands(VkCommandBuffer cmd, uint32_t frameIndex);
    void initializeAllBufferData(uint32_t frames, VkDeviceSize uniformSize, VkDeviceSize materialSize) noexcept;
    void createTransientCommandPool() noexcept;
    void updateNexusDescriptors() noexcept;
    void updateDenoiserDescriptors() noexcept;
    void updateAccumulationDescriptors(uint32_t slot) noexcept;
    void recordAccumulationPass(VkCommandBuffer cmd, uint32_t slot) noexcept;
    void updateTonemapDescriptor(uint32_t frameIdx, VkImageView inputView, VkImageView output) noexcept;
    void performDenoisingPass(VkCommandBuffer cmd) noexcept;
    void performTonemapPass(VkCommandBuffer cmd, uint32_t frameIdx, uint32_t swapImageIdx) noexcept;
    void updateUniformBuffer(uint32_t frame, const Camera& camera, float deltaTime) noexcept;
    void updateTonemapUniform(uint32_t frame) noexcept;
    bool recreateTonemapUBOs() noexcept;

    void destroyRTOutputImages() noexcept;
    void destroyAccumulationImages() noexcept;
    void destroyDenoiserImage() noexcept;
    void destroyNexusScoreImage() noexcept;

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

    void forcePinkFallbackClear() noexcept;
    void createSyncObjects() noexcept;

    // === PUBLIC ACCESSORS (fixed) ===
    [[nodiscard]] uint32_t accumulationFrame() const noexcept { return accumulationFrame_; }
    [[nodiscard]] uint32_t currentSpp() const noexcept { return currentSpp_; }

    // === ALL REQUIRED MEMBERS (in safe initialization order) ===
    // Core window/state first
    SDL_Window* window_ = nullptr;
    int width_ = 0;
    int height_ = 0;
    bool minimized_ = false;
    bool destroyed_ = false;

    // Runtime flags
    bool rtOutputNeedsTransition_ = false;
    bool depthNeedsTransition_ = false;
    bool accumulationNeedsTransition_ = false;
    bool nexusScoreNeedsInit_ = false;

    float totalTime_ = 0.0f;

    std::atomic<uint32_t> currentFrame_{0};
    uint64_t frameNumber_ = 0;
    uint32_t accumulationFrame_ = 0;
    bool resetAccumNextFrame_ = true;
    uint32_t acquiredImageIndex_ = 0;

    // Command system
    VkCommandPool                       commandPool_ = VK_NULL_HANDLE;
    RTX::Handle<VkCommandPool>          transientCommandPool_;
    std::vector<VkCommandBuffer>        commandBuffers_;

    // Sync
    std::vector<VkSemaphore>            imageAvailableSemaphores_;
    std::vector<VkSemaphore>            renderFinishedSemaphores_;
    std::vector<VkFence>                inFlightFences_;

    // Buffers
    std::vector<uint64_t>               uniformBufferEncs_;
    std::vector<uint64_t>               tonemapUniformEncs_;
    std::vector<uint64_t>               materialBufferEncs_;
    std::vector<uint64_t>               dimensionBufferEncs_;

    // Descriptor sets
    std::vector<VkDescriptorSet>        rtDescriptorSets_;

    // RT / Accumulation / Denoiser images
    std::vector<RTX::Handle<VkImage>>       rtOutputImages_;
    std::vector<RTX::Handle<VkDeviceMemory>>rtOutputMemories_;
    std::vector<RTX::Handle<VkImageView>>   rtOutputViews_;

    std::vector<RTX::Handle<VkImage>>       accumImages_;
    std::vector<RTX::Handle<VkDeviceMemory>>accumMemories_;
    std::vector<RTX::Handle<VkImageView>>   accumViews_;

    RTX::Handle<VkImage>       denoiserImage_;
    RTX::Handle<VkDeviceMemory>denoiserMemory_;
    RTX::Handle<VkImageView>   denoiserView_;

    VkImage       hypertraceScoreImage_   = VK_NULL_HANDLE;
    VkDeviceMemory hypertraceScoreMemory_ = VK_NULL_HANDLE;
    VkImageView   hypertraceScoreView_    = VK_NULL_HANDLE;
    uint32_t      hypertraceScoreWidth_   = 0;
    uint32_t      hypertraceScoreHeight_  = 0;

    // Depth
    RTX::Handle<VkImage>        depthImage_;
    RTX::Handle<VkDeviceMemory> depthImageMemory_;
    RTX::Handle<VkImageView>    depthImageView_;

    // Envmap
    RTX::Handle<VkImage>        envMapImage_;
    RTX::Handle<VkDeviceMemory> envMapMemory_;
    RTX::Handle<VkImageView>    envMapImageView_;
    RTX::Handle<VkSampler>      envMapSampler_;
    bool                        envMapNeedsUpload_ = false;
    uint32_t                    envMapUploadWidth_ = 0;
    uint32_t                    envMapUploadHeight_ = 0;
    float*                      envMapUploadData_ = nullptr;

    // Envmap display pipeline
    VkPipeline               envMapDisplayPipeline_       = VK_NULL_HANDLE;
    VkPipelineLayout         envMapDisplayPipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSet          envMapDisplayDescriptorSet_  = VK_NULL_HANDLE;
    VkDescriptorSetLayout    envMapDisplayDescSetLayout_  = VK_NULL_HANDLE;
    RTX::Handle<VkDescriptorPool> envMapDescriptorPool_;

    // Accumulation pipeline
    VkPipeline               accumulationPipeline_       = VK_NULL_HANDLE;
    VkPipelineLayout         accumulationPipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout    accumulationDescSetLayout_  = VK_NULL_HANDLE;
    VkDescriptorPool         accumulationDescriptorPool_ = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, 2> accumulationSets_{};

    // Tonemap
    RTX::Handle<VkSampler>              tonemapSampler_;
    RTX::Handle<VkDescriptorSetLayout>  tonemapDescriptorSetLayout_;
    RTX::Handle<VkPipelineLayout>       tonemapLayout_;
    RTX::Handle<VkPipeline>             tonemapPipeline_;
    std::vector<VkDescriptorSet>        tonemapSets_;
    RTX::Handle<VkDescriptorPool>       tonemapDescriptorPool_;

    // Denoiser
    RTX::Handle<VkPipeline>             denoiserPipeline_;
    RTX::Handle<VkPipelineLayout>       denoiserPipelineLayout_;
    RTX::Handle<VkDescriptorSetLayout>  denoiserLayout_;
    std::vector<VkDescriptorSet>        denoiserSets_;
    RTX::Handle<VkSampler>              denoiserSampler_;

    // Runtime state
    bool     hypertraceEnabled_     = Options::OptionsRTX::ENABLE_HYPERTRACE;
    bool     denoisingEnabled_      = Options::OptionsRTX::ENABLE_DENOISING;
    bool     adaptiveSamplingEnabled_ = Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING;
    bool     overclockMode_         = false;
    bool     tonemapEnabled_        = true;
    bool     showOverlay_           = true;
    int      tonemapType_           = 0;
    FpsTarget fpsTarget_            = FpsTarget::FPS_120;
    float    currentExposure_       = 1.0f;
    float    currentNexusScore_     = 0.0f;
    uint32_t currentSpp_            = 0;

    int      activeRenderMode_      = 0;
    uint32_t materialCount_         = 0;
    uint32_t activeMaterialIndex_   = 0;
    float    materialMetallicOverride_  = -1.0f;
    float    materialRoughnessOverride_ = -1.0f;
    float    emissiveIntensity_     = 1.0f;

    glm::vec3 sunDirection_         = glm::vec3(0.3f, 0.8f, 0.5f);
    float     sunIntensity_         = 10.0f;
    glm::vec3 sunColor_             = glm::vec3(1.0f, 0.95f, 0.9f);
    float     fogDensity_           = 0.02f;
    glm::vec3 fogColor_             = glm::vec3(0.7f, 0.8f, 0.9f);

    uint32_t maxFramesInFlight_ = Options::Performance::MAX_FRAMES_IN_FLIGHT;

    // Render modes
    RenderMode1 renderMode1_;
    RenderMode2 renderMode2_;
    RenderMode3 renderMode3_;
    RenderMode4 renderMode4_;
    RenderMode5 renderMode5_;
    RenderMode6 renderMode6_;
    RenderMode7 renderMode7_;
    RenderMode8 renderMode8_;
    RenderMode9 renderMode9_;

    // Singleton
    static VulkanRenderer* get() noexcept;
private:
    static inline VulkanRenderer* s_instance = nullptr;
    uint64_t defaultMaterialsHandle_ = 0;
};

// Global accessor
[[nodiscard]] inline VulkanRenderer& g_rtx() noexcept
{
    auto* ptr = VulkanRenderer::get();
    if (!ptr) LOG_FATAL_CAT("RTX", "g_rtx() called before renderer initialized");
    return *ptr;
}

// JANUARY 05, 2026 — FINAL COMPILATION-CLEAN HEADER
// All members declared: rtDescriptorSets_, materialBufferEncs_, currentSpp_, etc.
// Accessors fixed: accumulationFrame() → accumulationFrame_
// setTonemap removed (was unused)
// Initialization order corrected
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — PLASTIC BEACH FOREVER
// =============================================================================