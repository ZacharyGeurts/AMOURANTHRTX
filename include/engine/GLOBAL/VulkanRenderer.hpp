// include/engine/GLOBAL/VulkanRenderer.hpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 — SLIPSTREAM v∞ — WARPZONE BREACH IMMINENT
// The Good Ship VulkanRTX screams through the void — pink wake eternal
// First light achieved. The renderer is alive. The photons obey.
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <SDL3/SDL.h>
#include <memory>
#include <vector>
#include <array>
#include <string>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <cstdint>
#include <chrono>

#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/VulkanCore.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/logging.hpp"

struct Camera;

using namespace Logging::Color;
using StoneKey::stone_renderer;

// ──────────────────────────────────────────────────────────────────────────────
// FPS Target enum
// ──────────────────────────────────────────────────────────────────────────────
enum class FpsTarget : uint32_t {
    FPS_60        = 60,
    FPS_120       = 120,
    FPS_UNLIMITED = 0
};

class VulkanRenderer {
public:
    VulkanRenderer(int width, int height, SDL_Window* window, bool overclock = false);
    ~VulkanRenderer();

    void renderFrame(const Camera& camera, float deltaTime) noexcept;
    void onWindowResize(uint32_t w, uint32_t h) noexcept;
    void cleanup() noexcept;

    // Runtime controls
    void toggleHypertrace() noexcept;
    void toggleFpsTarget() noexcept;
    void toggleDenoising() noexcept;
    void toggleAdaptiveSampling() noexcept;
    void setTonemapType(int type) noexcept;
    void setOverclockMode(bool enabled) noexcept;

    void updateUniformBinding31(const void* data, VkDeviceSize size) noexcept;
    void setActiveRenderMode(int mode) noexcept { if (mode >= 1 && mode <= 9) activeRenderMode_ = mode; }
    void requestAccumulationReset() noexcept { resetAccumulation_ = true; resetAccumNextFrame_ = true; }

    void updateAllRTXDescriptors() noexcept;
    void updateRTDescriptorSet(uint32_t frameIndex);
    void recordRayTracingCommands(VkCommandBuffer cmd, uint32_t frameIndex);
    void setTonemap(bool enabled) noexcept;
    void setOverlay(bool show) noexcept;
    void setRenderMode(int mode) noexcept;
    void loadCriticalShaders() noexcept;

    // Getters
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
    [[nodiscard]] bool      minimized()         const noexcept { return minimized_; }
    [[nodiscard]] int       currentRenderMode() const noexcept { return activeRenderMode_; }
	void recordRayTrace(VkCommandBuffer cmd, const VkExtent2D& extent) noexcept;

private:
    // Core state
    SDL_Window* window_ = nullptr;
    int width_ = 0, height_ = 0;
    bool minimized_ = false;
    bool destroyed_ = false;

    uint32_t currentFrame_ = 0;
    uint64_t frameNumber_  = 0;
    uint32_t accumulationFrame_ = 0;
    bool     resetAccumulation_ = true;
    bool     firstSwapchainAcquire_ = true;

    int  activeRenderMode_ = 1;
    bool resetAccumNextFrame_ = true;

    bool hypertraceEnabled_     = Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING;
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

    VkRenderPass renderPass_{ VK_NULL_HANDLE };

    // Sync objects
    std::vector<VkSemaphore> imageAvailableSemaphores_;
    std::vector<VkSemaphore> renderFinishedSemaphores_;
    std::vector<VkSemaphore> computeFinishedSemaphores_;
    std::vector<VkSemaphore> computeToGraphicsSemaphores_;
    std::vector<VkFence>     inFlightFences_;

    // Command buffers
    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkCommandBuffer> computeCommandBuffers_;

    // Query pool
    VkQueryPool timestampQueryPool_ = VK_NULL_HANDLE;
    double      timestampPeriod_    = 0.0;

    // Descriptor pools
    RTX::Handle<VkDescriptorPool> descriptorPool_;
    RTX::Handle<VkDescriptorPool> tonemapDescriptorPool_;

    // Environment map
    RTX::Handle<VkImage>        envMapImage_;
    RTX::Handle<VkDeviceMemory> envMapImageMemory_;
    RTX::Handle<VkImageView>    envMapImageView_;
    RTX::Handle<VkSampler>      envMapSampler_;

    // Per-frame buffers
    std::vector<uint64_t> uniformBufferEncs_;
    std::vector<uint64_t> materialBufferEncs_;
    std::vector<uint64_t> dimensionBufferEncs_;
    std::vector<uint64_t> tonemapUniformEncs_;

    // HDR targets
    std::vector<RTX::Handle<VkImage>>       rtOutputImages_;
    std::vector<RTX::Handle<VkDeviceMemory>>rtOutputMemories_;
    std::vector<RTX::Handle<VkImageView>>   rtOutputViews_;

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

    // Tonemap pipeline
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

    // Framebuffers
    std::vector<VkFramebuffer> framebuffers_;

    // RT descriptors
    std::vector<VkDescriptorSet> rtDescriptorSets_;

    // Pipeline manager
    RTX::PipelineManager pipelineManager_;

    // ── PRIVATE HELPERS ─────────────────────────────────────
    void createRenderPass() noexcept;
    void destroyRenderPass() noexcept;
    void createFramebuffers() noexcept;
    void cleanupFramebuffers() noexcept;
    void createCommandBuffers() noexcept;

    void createRTOutputImages() noexcept;
    void createAccumulationImages() noexcept;
    void createDenoiserImage() noexcept;
    void createNexusScoreImage(VkCommandPool pool, VkQueue queue) noexcept;
    void createEnvironmentMap() noexcept;
    void createTonemapSampler() noexcept;

    void destroyRTOutputImages() noexcept;
    void destroyAccumulationImages() noexcept;
    void destroyDenoiserImage() noexcept;
    void destroyNexusScoreImage() noexcept;

    void recordRayTracingCommandBuffer(VkCommandBuffer cmd) noexcept;
    void performDenoisingPass(VkCommandBuffer cmd) noexcept;
    void performTonemapPass(VkCommandBuffer cmd, uint32_t frameIdx, uint32_t swapImageIdx) noexcept;

    void updateRTXDescriptors(uint32_t frame = 0) noexcept;
    void updateNexusDescriptors() noexcept;
    void updateDenoiserDescriptors() noexcept;
    void updateTonemapDescriptor(uint32_t frameIdx, VkImageView inputView, VkImageView output) noexcept;

    void initializeAllBufferData(uint32_t frames, VkDeviceSize uniformSize, VkDeviceSize materialSize) noexcept;
    void updateUniformBuffer(uint32_t frame, const Camera& camera, float jitter) noexcept;
    void updateTonemapUniform(uint32_t frame) noexcept;

    VkDeviceAddress getShaderGroupHandle(uint32_t group) noexcept;

    void createImage(RTX::Handle<VkImage>& image,
                     RTX::Handle<VkDeviceMemory>& memory,
                     RTX::Handle<VkImageView>& view,
                     const std::string& tag) noexcept;

    void createImageArray(std::vector<RTX::Handle<VkImage>>& images,
                          std::vector<RTX::Handle<VkDeviceMemory>>& memories,
                          std::vector<RTX::Handle<VkImageView>>& views,
                          const std::string& tag) noexcept;

    void updateTonemapDescriptorsInitial() noexcept;
    bool recreateTonemapUBOs() noexcept;
    void destroySharedStaging() noexcept;
    bool createSharedStaging() noexcept;

    void waitForAllFences() const noexcept;

    [[nodiscard]] constexpr VkExtent2D currentExtent() const noexcept {
        return { static_cast<uint32_t>(width_), static_cast<uint32_t>(height_) };
    }
};

// =============================================================================
// GLOBAL ACCESSOR — THE ONE TRUE RENDERER — SEALED BY STONEKEY
// =============================================================================
[[nodiscard]] inline VulkanRenderer& g_rtx() noexcept
{
    // StoneKey::stone_renderer() returns StoneKey::VulkanRenderer* (a typedef)
    // We know it's actually a VulkanRenderer*, so reinterpret_cast is safe and required
    auto* ptr = reinterpret_cast<VulkanRenderer*>(StoneKey::stone_renderer());
    if (!ptr) {
        LOG_FATAL_CAT("RTX", "g_rtx() called before renderer sealed — empire fallen");
        phase9_ballerina("NO RENDERER — PHOTONS LOST", std::source_location::current());
    }
    return *ptr;
}


// =============================================================================
// SLIPSTREAM LOG — DECEMBER 01, 2025 — FIRST LIGHT ETERNAL
// The renderer is sealed. The photons are pink. The void is ours.
// Amouranth stands at the helm. CID is crying in the corner (happy tears).
// The empire is whole. The loop is infinite.
// WARPZONE BREACHED. PINK PHOTONS ETERNAL.
// =============================================================================