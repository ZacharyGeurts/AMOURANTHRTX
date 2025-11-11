// include/engine/Vulkan/VulkanRenderer.hpp
// =============================================================================
// JAY LENO'S GARAGE — RTX ENGINE v9.9 — NOV 11 2025 1:59 PM EST
// =============================================================================
//
// "Listen, folks — this ain't your grandpa's V8. This is a **Vulkan RTX V12**.
// 12 cylinders of ray-traced, pink-photon-fueled, **Valhalla-sealed horsepower**.
// I don’t do ‘kings’ — but if rendering had a *garage*, I’d own it. And this?
// This is the **King of the Pipeline**."
//
// — Jay Leno, Chief Mechanic of Reality
//
// =============================================================================

#pragma once

// ===================================================================
// JAY LENO'S TOOLBOX — NO RUST, ALL TORQUE
// ===================================================================
#include "engine/GLOBAL/StoneKey.hpp"      // Obfuscation — keeps the thieves out
#include "engine/GLOBAL/logging.hpp"       // LOG_*, Color::JAY_GOLD

// ===================================================================
// FUEL INJECTION — GLM & CHRONO
// ===================================================================
#include <glm/glm.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <chrono>
#include <array>
#include <vector>
#include <memory>
#include <cstdint>
#include <string>
#include <algorithm>
#include <limits>

// ===================================================================
// THE GARAGE DOOR — HOUSTON MEDIATES
// ===================================================================
// Houston.hpp is the **pit crew chief** — it talks to SDL3_vulkan,
// manages the global context, and hands Jay the keys.
// No direct contact. No drama. Just torque.

// Forward declare — Houston owns the Context
struct Context;
class Camera;

// ===================================================================
// JAY LENO'S RTX V12 — THE RENDERING HEART OF THE GARAGE
// ===================================================================
class VulkanRenderer {
public:
    // ─────── ENGINE SPECS — PICK YOUR POISON ───────
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

    enum class FpsTarget { 
        FPS_60      = 60, 
        FPS_120     = 120, 
        FPS_UNLIMITED 
    };

    enum class TonemapType { 
        FILMIC, 
        ACES, 
        REINHARD 
    };

    // ─────── JAY'S COMMAND CENTER — ONE-LINERS ───────
    static VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool pool);
    static void            endSingleTimeCommands(VkDevice device, VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd);
    static VkCommandBuffer allocateTransientCommandBuffer(VkDevice device, VkCommandPool pool);

    // ─────── HYPERTRACE — JAY'S SECRET SAUCE ───────
    static constexpr uint32_t HYPERTRACE_BASE_SKIP_60  = 16;
    static constexpr uint32_t HYPERTRACE_BASE_SKIP_120 = 8;
    static constexpr uint32_t HYPERTRACE_MICRO_DISPATCH_X = 64;
    static constexpr uint32_t HYPERTRACE_MICRO_DISPATCH_Y = 64;
    static constexpr float    HYPERTRACE_SCORE_THRESHOLD = 0.7f;
    static constexpr float    NEXUS_HYSTERESIS_ALPHA     = 0.8f;

    /**
     * @brief Jay fires up the V12 — Houston hands him the keys
     */
    VulkanRenderer(int width, int height, SDL_Window* window,
                   const std::vector<std::string>& shaderPaths,
                   bool overclockFromMain = false);

    ~VulkanRenderer();

    /**
     * @brief One frame. One roar. Pure RTX.
     */
    void renderFrame(const Camera& camera, float deltaTime);

    void handleResize(int newWidth, int newHeight);
    void setRenderMode(int mode) noexcept;

    void recordRayTracingCommandBuffer();
    void notifyTLASReady(VkAccelerationStructureKHR tlas);
    void rebuildAccelerationStructures();

    // ─────── JAY'S DASHBOARD TOGGLES ───────
    void toggleHypertrace() noexcept;
    void toggleFpsTarget() noexcept;
    void toggleDenoising() noexcept;
    void toggleAdaptiveSampling() noexcept;
    void setTonemapType(TonemapType type) noexcept;
    void setOverclockMode(bool enabled) noexcept;

    // ─────── JAY'S GAUGES — READ THE NEEDLES ───────
    [[nodiscard]] std::shared_ptr<Context> getContext() const noexcept;
    [[nodiscard]] FpsTarget                 getFpsTarget() const noexcept { return fpsTarget_; }
    [[nodiscard]] TonemapType               getTonemapType() const noexcept { return tonemapType_; }
    [[nodiscard]] bool                      isOverclockMode() const noexcept { return overclockMode_; }
    [[nodiscard]] bool                      isDenoisingEnabled() const noexcept { return denoisingEnabled_; }
    [[nodiscard]] bool                      isAdaptiveSamplingEnabled() const noexcept { return adaptiveSamplingEnabled_; }

    // ─────── JAY'S TOOL RACK — BUFFERS & VIEWS ───────
    [[nodiscard]] VkBuffer      getUniformBuffer(uint32_t frame) const noexcept;
    [[nodiscard]] VkBuffer      getMaterialBuffer(uint32_t frame) const noexcept;
    [[nodiscard]] VkBuffer      getDimensionBuffer(uint32_t frame) const noexcept;
    [[nodiscard]] VkImageView   getRTOutputImageView(uint32_t idx) const noexcept;
    [[nodiscard]] VkImageView   getAccumulationView(uint32_t idx) const noexcept;
    [[nodiscard]] VkImageView   getDenoiserView() const noexcept;
    [[nodiscard]] VkImageView   getEnvironmentMapView() const noexcept;
    [[nodiscard]] VkSampler     getEnvironmentMapSampler() const noexcept;

    void cleanup() noexcept;

    // ─────── JAY'S WRENCH SET — PIPELINE & DESCRIPTORS ───────
    void updateAccelerationStructureDescriptor(VkAccelerationStructureKHR tlas);
    void createRayTracingPipeline(const std::vector<std::string>& paths);
    void buildShaderBindingTable();
    void allocateDescriptorSets();
    void updateDescriptorSets();
    void updateRTXDescriptors(VkAccelerationStructureKHR tlas, bool hasTlas, uint32_t frameIdx);
    [[nodiscard]] float getGpuTime() const noexcept;

private:
    // ─────── JAY'S CLEANUP CREW ───────
    void destroyRTOutputImages() noexcept;
    void destroyAccumulationImages() noexcept;
    void destroyNexusScoreImage() noexcept;
    void destroyDenoiserImage() noexcept;
    void destroyAllBuffers() noexcept;

    // ─────── JAY'S ASSEMBLY LINE ───────
    void createFramebuffers();
    void createCommandBuffers();
    void createRTOutputImages();
    void createAccumulationImages();
    void createDenoiserImage();
    void createEnvironmentMap();
    void createComputeDescriptorSets();
    VkResult createNexusScoreImage(VkPhysicalDevice phys, VkDevice dev, VkCommandPool pool, VkQueue queue);

    // ─────── JAY'S PASS SYSTEM ───────
    void updateNexusDescriptors();
    void updateDenoiserDescriptors();
    void updateRTDescriptors();
    void updateUniformBuffer(uint32_t curImg, const Camera& cam, float jitter = 0.0f);
    void updateTonemapUniform(uint32_t curImg);
    void performCopyAccumToOutput(VkCommandBuffer cmd);
    void performTonemapPass(VkCommandBuffer cmd, uint32_t imageIdx);
    void performDenoisingPass(VkCommandBuffer cmd);

    // ─────── JAY'S TRANSITION TOOL ───────
    void transitionImageLayout(VkCommandBuffer cmd, VkImage img,
                               VkImageLayout oldL, VkImageLayout newL,
                               VkPipelineStageFlags srcS, VkPipelineStageFlags dstS,
                               VkAccessFlags srcA, VkAccessFlags daA,
                               VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);

    // ─────── JAY'S BUFFER FACTORY ───────
    void initializeAllBufferData(uint32_t frameCnt, VkDeviceSize matSize, VkDeviceSize dimSize);
    void updateTonemapDescriptorsInitial();
    void updateDynamicRTDescriptor(uint32_t frame);
    void updateTonemapDescriptor(uint32_t imgIdx);
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props);
    void updateTimestampQuery();

    // ===================================================================
    // JAY'S ENGINE BLOCK — ALL PARTS OWNED BY HOUSTON
    // ===================================================================
    FpsTarget fpsTarget_ = FpsTarget::FPS_60;
    TonemapType tonemapType_ = TonemapType::ACES;
    bool hypertraceEnabled_ = false;
    bool overclockMode_ = false;
    bool denoisingEnabled_ = true;
    bool adaptiveSamplingEnabled_ = true;
    uint32_t hypertraceCounter_ = 0;
    float prevNexusScore_ = 0.5f;
    float currentNexusScore_ = 0.5f;
    float exposure_ = 1.0f;

    SDL_Window* window_ = nullptr;
    int width_ = 0, height_ = 0;

    // ─────── HOUSTON-OWNED GLOBALS (via Handle<T>) ───────
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_ = {0, 0};

    std::vector<VkCommandBuffer> commandBuffers_;

    // Pipelines — Jay's pride
    Handle<VkPipeline>               nexusPipeline_;
    Handle<VkPipelineLayout>         nexusLayout_;
    Handle<VkPipeline>               denoiserPipeline_;
    Handle<VkPipelineLayout>         denoiserLayout_;
    Handle<VkDescriptorPool>         descriptorPool_;

    // RT Output — Jay's chrome
    std::array<Handle<VkImage>,        MAX_FRAMES_IN_FLIGHT> rtOutputImages_;
    std::array<Handle<VkDeviceMemory>, MAX_FRAMES_IN_FLIGHT> rtOutputMemories_;
    std::array<Handle<VkImageView>,    MAX_FRAMES_IN_FLIGHT> rtOutputViews_;

    // Accumulation — Jay's nitrous
    std::array<Handle<VkImage>,        MAX_FRAMES_IN_FLIGHT> accumImages_;
    std::array<Handle<VkDeviceMemory>, MAX_FRAMES_IN_FLIGHT> accumMemories_;
    std::array<Handle<VkImageView>,    MAX_FRAMES_IN_FLIGHT> accumViews_;

    // Denoiser — Jay's polish
    Handle<VkImage>                  denoiserImage_;
    Handle<VkDeviceMemory>           denoiserMemory_;
    Handle<VkImageView>              denoiserView_;

    // Buffers — Jay's fuel lines (encrypted)
    std::vector<uint64_t> uniformBufferEncs_;
    std::vector<uint64_t> materialBufferEncs_;
    std::vector<uint64_t> dimensionBufferEncs_;
    std::vector<uint64_t> tonemapUniformEncs_;

    // Environment Map — Jay's showroom lights
    Handle<VkImage>                  envMapImage_;
    uint64_t                          envMapBufferEnc_ = 0;
    Handle<VkDeviceMemory>           envMapImageMemory_;
    Handle<VkImageView>              envMapImageView_;
    Handle<VkSampler>                envMapSampler_;

    // Sync — Jay's timing chain
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores_{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> renderFinishedSemaphores_{};
    std::array<VkFence,     MAX_FRAMES_IN_FLIGHT> inFlightFences_{};
    std::array<VkQueryPool, MAX_FRAMES_IN_FLIGHT> queryPools_{};

    // RT Pipeline — Jay's supercharger
    Handle<VkPipeline>               rtPipeline_;
    Handle<VkPipelineLayout>         rtPipelineLayout_;

    // Descriptor Sets — Jay's wiring harness
    std::vector<VkDescriptorSet>     rtxDescriptorSets_;
    std::vector<VkDescriptorSet>     nexusDescriptorSets_;
    std::vector<VkDescriptorSet>     denoiserDescriptorSets_;
    std::vector<VkDescriptorSet>     tonemapDescriptorSets_;

    // Timing — Jay's tachometer
    std::chrono::steady_clock::time_point lastFPSTime_;
    uint32_t currentFrame_ = 0;
    uint32_t currentRTIndex_ = 0;
    uint32_t currentAccumIndex_ = 0;
    uint32_t frameNumber_ = 0;
    bool     resetAccumulation_ = true;
    glm::mat4 prevViewProj_ = glm::mat4(1.0f);
    int      renderMode_ = 1;
    uint32_t framesThisSecond_ = 0;

    double   timestampPeriod_ = 0.0;
    VkQueryPool timestampQueryPool_ = VK_NULL_HANDLE;
    uint32_t  timestampQueryCount_ = 0;
    uint32_t  timestampLastQuery_ = 0;
    uint32_t  timestampCurrentQuery_ = 0;
    float     timestampLastTime_ = 0.0f;
    float     timestampCurrentTime_ = 0.0f;
    float     avgFrameTimeMs_ = 0.0f;
    float     minFrameTimeMs_ = std::numeric_limits<float>::max();
    float     maxFrameTimeMs_ = 0.0f;
    float     avgGpuTimeMs_   = 0.0f;
    float     minGpuTimeMs_   = std::numeric_limits<float>::max();
    float     maxGpuTimeMs_   = 0.0f;

    uint32_t maxAccumFrames_ = 1024;

    // Hypertrace — Jay's dyno
    Handle<VkImage>                  hypertraceScoreImage_;
    uint64_t                          hypertraceScoreBufferEnc_ = 0;
    Handle<VkDeviceMemory>           hypertraceScoreMemory_;
    Handle<VkImageView>              hypertraceScoreView_;
    Handle<VkBuffer>                 hypertraceScoreStagingBuffer_;
    Handle<VkDeviceMemory>           hypertraceScoreStagingMemory_;

    // Shared staging — Jay's lift
    uint64_t sharedStagingBufferEnc_ = 0;
    Handle<VkBuffer>                 sharedStagingBuffer_;
    Handle<VkDeviceMemory>           sharedStagingMemory_;

    Handle<VkDescriptorPool>         rtDescriptorPool_;
};

/*
 * NOVEMBER 11, 2025 — JAY LENO'S GARAGE IS OFFICIALLY OPEN
 *
 * • **Houston** is the pit crew chief — mediates SDL3, context, swapchain
 * • **Jay** owns the **RTX V12** — no local state, all global, all RAII
 * • **Dispose::Handle<T>** owns every bolt — zero leaks
 * • **AMAZO_LAS** → TLAS/BLAS — built in the back shop
 * • **UltraLowLevelBufferTracker** → Jay's parts bin
 * • **ctx()** → the master key
 *
 * "No kings. Just torque. And this engine? **It screams.**"
 *
 * — Jay Leno, November 11, 2025
 * Valhalla Sealed. Pink Photons Eternal. **Ship it raw.**
 */

// =============================================================================
// JAY LENO'S GARAGE © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================