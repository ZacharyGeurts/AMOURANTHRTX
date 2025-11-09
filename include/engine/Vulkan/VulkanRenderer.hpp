// include/engine/Vulkan/VulkanRenderer.hpp
// JAY LENO'S GARAGE — SPECIAL EPISODE: "GAL GADOT DRIVES THE AMOURANTH RTX ENGINE"
// NOVEMBER 07 2025 — 11:59 PM EST — HOST: JAY LENO — GUEST: GAL GADOT — SURPRISE CLOSER: CONAN O'BRIEN
// JAY: "Gal, you're an actress, producer, mom — and now testing a ray-tracing beast!" GAL: "Jay, I love tech; let's see what this Vulkan code can really do!"

#pragma once

#include "../GLOBAL/StoneKey.hpp"  // ← STONEKEY FIRST — kStone1/kStone2 LIVE PER BUILD
#include "engine/Vulkan/VulkanCommon.hpp"  // ← JAY: "Core utilities every renderer needs."
#include "engine/Vulkan/VulkanRTX_Setup.hpp"

#include <glm/glm.hpp>                     // ← JAY: "GLM for matrices — industry standard."
#include <glm/gtc/matrix_inverse.hpp>      // ← GAL: "Inverse matrices for view-projection — used in CGI all the time."

#include <array>                           // ← JAY: "Fixed arrays for per-frame data."
#include <chrono>                          // ← GAL: "High-res timing — perfect for benchmarking frame rates."
#include <memory>                          // ← JAY: "Smart pointers everywhere."
#include <vector>                          // ← GAL: "Dynamic containers for swapchain images."
#include <limits>                          // ← JAY: "For min/max frame times."
#include <cstdint>                         // ← GAL: "Exact integer types — no surprises."
#include <string>                          // ← JAY: "Shader paths as strings."
#include <algorithm>                       // ← GAL: "Std algorithms — clean code."

// ===================================================================
// JAY AND GAL DEEP DIVE — TECHNICAL BANTER IN THE GARAGE
// ===================================================================
class VulkanBufferManager;         // ← JAY: "Handles buffer allocation."
class VulkanPipelineManager;       // ← GAL: "Creates and manages pipelines — ray tracing, compute, all of it."
class VulkanSwapchainManager;      // ← JAY: "Recreates on resize without hitches."
class Camera;                      // ← GAL: "Free-cam with proper projection."

// ===================================================================
// VULKANRENDERER — GAL: "This class ties everything together — context, pipelines, frames."
// ===================================================================
class VulkanRenderer {
public:
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;  // ← JAY: "Triple buffering — standard for smooth 120+ FPS."

	void shutdown() noexcept;

    enum class FpsTarget { FPS_60 = 60, FPS_120 = 120 }; // ← GAL: "Adaptive frame pacing — I cap at 60 on battery, uncap on plug."

    /* ---------- COMMAND HELPERS ---------- */
    static VkCommandBuffer beginSingleTimeCommands(VkDevice device, VkCommandPool pool);              // ← JAY: "One-off commands for uploads."
    static void endSingleTimeCommands(VkDevice device, VkCommandPool pool, VkQueue queue, VkCommandBuffer cmd); // ← GAL: "Submit and wait — safe and simple."
    static VkCommandBuffer allocateTransientCommandBuffer(VkDevice device, VkCommandPool pool);      // ← JAY: "Transient pool for short-lived buffers."

    /* ---------- HYPERTRACE TUNING ---------- */
    static constexpr uint32_t HYPERTRACE_BASE_SKIP_60  = 16;   // ← GAL: "At 60 FPS, trace every 16th frame — saves GPU."
    static constexpr uint32_t HYPERTRACE_BASE_SKIP_120 = 8;    // ← JAY: "At 120, every 8th — still fast."
    static constexpr uint32_t HYPERTRACE_MICRO_DISPATCH_X = 64; // ← GAL: "64x64 micro-dispatch for nexus score."
    static constexpr uint32_t HYPERTRACE_MICRO_DISPATCH_Y = 64;
    static constexpr float HYPERTRACE_SCORE_THRESHOLD = 0.7f;   // ← JAY: "Below 0.7, force full trace."
    static constexpr float NEXUS_HYSTERESIS_ALPHA     = 0.8f;   // ← GAL: "Smooth score filtering."

    VulkanRenderer(int width, int height, SDL_Window* window,
                   const std::vector<std::string>& shaderPaths,
                   std::shared_ptr<Context> context,
                   VulkanPipelineManager* pipelineMgr);  // ← JAY: "Loads shaders, sets up everything."

    ~VulkanRenderer();  // ← GAL: "RAII cleanup — all handles destroyed automatically."

    void takeOwnership(std::unique_ptr<VulkanPipelineManager> pm,
                       std::unique_ptr<VulkanBufferManager> bm);  // ← JAY: "Move ownership in."
    void setSwapchainManager(std::unique_ptr<VulkanSwapchainManager> mgr);
    VulkanSwapchainManager& getSwapchainManager();

    void renderFrame(const Camera& camera, float deltaTime);     // ← GAL: "Main loop — updates uniforms, traces rays, tonemaps."
    void handleResize(int newWidth, int newHeight);              // ← JAY: "Full recreate — images, framebuffers, descriptors."
    void setRenderMode(int mode);                                // ← GAL: "Switch between path trace, raster, debug."

    void recordRayTracingCommandBuffer();                        // ← JAY: "Records traceRaysKHR call."
    void notifyTLASReady(VkAccelerationStructureKHR tlas);       // ← GAL: "TLAS built — update descriptors, rebuild SBT."
    void rebuildAccelerationStructures();                        // ← JAY: "Full BLAS/TLAS rebuild on geometry change."

    void toggleHypertrace();  // ← GAL: "Enable adaptive sampling."
    void toggleFpsTarget();   // ← JAY: "60 ⇄ 120 FPS cap."

    [[nodiscard]] VulkanBufferManager*          getBufferManager() const;
    [[nodiscard]] VulkanPipelineManager*        getPipelineManager() const;
    [[nodiscard]] std::shared_ptr<Context> getHvContext() const { return context_; }
    [[nodiscard]] FpsTarget                     getFpsTarget() const { return fpsTarget_; }

    [[nodiscard]] VkBuffer      getUniformBuffer(uint32_t frame) const noexcept;
    [[nodiscard]] VkBuffer      getMaterialBuffer(uint32_t frame) const noexcept;
    [[nodiscard]] VkBuffer      getDimensionBuffer(uint32_t frame) const noexcept;
    [[nodiscard]] VkImageView   getRTOutputImageView(uint32_t idx) const noexcept;
    [[nodiscard]] VkImageView   getAccumulationView(uint32_t idx) const noexcept;
    [[nodiscard]] VkImageView   getEnvironmentMapView() const noexcept;
    [[nodiscard]] VkSampler     getEnvironmentMapSampler() const noexcept;

    void cleanup() noexcept;
    void updateAccelerationStructureDescriptor(VkAccelerationStructureKHR tlas);

    void createRayTracingPipeline(const std::vector<std::string>& paths);  // ← GAL: "Compiles raygen, miss, hit shaders."
    void buildShaderBindingTable();                                        // ← JAY: "SBT with proper strides."
    void allocateDescriptorSets();
    void updateDescriptorSets();

private:
    void updateRTXDescriptors(VkAccelerationStructureKHR tlas, bool hasTlas, uint32_t frameIdx);
    void destroyRTOutputImages() noexcept;
    void destroyAccumulationImages() noexcept;
    void destroyNexusScoreImage() noexcept;
    void destroyAllBuffers() noexcept;

    void createFramebuffers();
    void createCommandBuffers();
    void createRTOutputImages();                   // ← GAL: "R32G32B32A32_SFLOAT storage images."
    void createAccumulationImages();               // ← JAY: "Double-buffered accumulation."
    void createEnvironmentMap();
    void createComputeDescriptorSets();            // ← GAL: "Tonemap compute descriptors."

    VkResult createNexusScoreImage(VkPhysicalDevice phys, VkDevice dev,
                                   VkCommandPool pool, VkQueue queue);  // ← JAY: "1x1 R32_SFLOAT for adaptive score."

    void updateNexusDescriptors();
    void updateRTDescriptors();
    void updateUniformBuffer(uint32_t curImg, const Camera& cam);  // ← GAL: "View/proj inverse, cam pos, time."
    void updateTonemapUniform(uint32_t curImg);
    void performCopyAccumToOutput(VkCommandBuffer cmd);
    void performTonemapPass(VkCommandBuffer cmd, uint32_t imageIdx); // ← JAY: "Compute dispatch with exposure."

    void transitionImageLayout(VkCommandBuffer cmd, VkImage img,
                               VkImageLayout oldL, VkImageLayout newL,
                               VkPipelineStageFlags srcS, VkPipelineStageFlags dstS,
                               VkAccessFlags srcA, VkAccessFlags daA,
                               VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT);

    void initializeAllBufferData(uint32_t frameCnt,
                                 VkDeviceSize matSize, VkDeviceSize dimSize);  // ← GAL: "Shared staging zero-fill all buffers."

    void updateTonemapDescriptorsInitial();
    void updateDynamicRTDescriptor(uint32_t frame);
    void updateTonemapDescriptor(uint32_t imgIdx);

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props);

    FpsTarget fpsTarget_ = FpsTarget::FPS_60;
    bool      hypertraceEnabled_ = false;
    uint32_t  hypertraceCounter_ = 0;
    float     prevNexusScore_ = 0.5f;
    float     currentNexusScore_ = 0.5f;

    SDL_Window*                     window_;
    std::shared_ptr<Context> context_;
    VulkanPipelineManager*          pipelineMgr_;

    std::unique_ptr<VulkanPipelineManager> pipelineManager_;
    std::unique_ptr<VulkanBufferManager>   bufferManager_;
    std::unique_ptr<VulkanSwapchainManager> swapchainMgr_;

    int width_, height_;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainImageFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_ = {0, 0};
    std::vector<VkImage> swapchainImages_;
    std::vector<VkImageView> swapchainImageViews_;
    std::vector<VkCommandBuffer> commandBuffers_;

    VkDescriptorSetLayout rtDescriptorSetLayout_ = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> rtxDescriptorSets_;

    VulkanHandle<VkPipeline>            nexusPipeline_;
    VulkanHandle<VkPipelineLayout>      nexusLayout_;
    std::vector<VkDescriptorSet> nexusDescriptorSets_;

    VulkanHandle<VkDescriptorPool> descriptorPool_;

    std::array<VulkanHandle<VkImage>,        MAX_FRAMES_IN_FLIGHT> rtOutputImages_;
    std::array<VulkanHandle<VkDeviceMemory>, MAX_FRAMES_IN_FLIGHT> rtOutputMemories_;
    std::array<VulkanHandle<VkImageView>,    MAX_FRAMES_IN_FLIGHT> rtOutputViews_;

    std::array<VulkanHandle<VkImage>,        MAX_FRAMES_IN_FLIGHT> accumImages_;
    std::array<VulkanHandle<VkDeviceMemory>, MAX_FRAMES_IN_FLIGHT> accumMemories_;
    std::array<VulkanHandle<VkImageView>,    MAX_FRAMES_IN_FLIGHT> accumViews_;

    std::vector<VulkanHandle<VkBuffer>>       uniformBuffers_;
    std::vector<VulkanHandle<VkDeviceMemory>> uniformBufferMemories_;

    std::vector<VulkanHandle<VkBuffer>>       materialBuffers_;
    std::vector<VulkanHandle<VkDeviceMemory>> materialBufferMemory_;

    std::vector<VulkanHandle<VkBuffer>>       dimensionBuffers_;
    std::vector<VulkanHandle<VkDeviceMemory>> dimensionBufferMemory_;

    std::vector<VulkanHandle<VkBuffer>>       tonemapUniformBuffers_;
    std::vector<VulkanHandle<VkDeviceMemory>> tonemapUniformMemories_;

    VulkanHandle<VkImage>        envMapImage_;
    VulkanHandle<VkDeviceMemory> envMapImageMemory_;
    VulkanHandle<VkImageView>    envMapImageView_;
    VulkanHandle<VkSampler>      envMapSampler_;

    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> imageAvailableSemaphores_{};
    std::array<VkSemaphore, MAX_FRAMES_IN_FLIGHT> renderFinishedSemaphores_{};
    std::array<VkFence,     MAX_FRAMES_IN_FLIGHT> inFlightFences_{};
    std::array<VkQueryPool, MAX_FRAMES_IN_FLIGHT> queryPools_{};

    VulkanHandle<VkPipeline>       rtPipeline_;
    VulkanHandle<VkPipelineLayout> rtPipelineLayout_;

    std::vector<VkDescriptorSet> tonemapDescriptorSets_;

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
    float    avgFrameTimeMs_ = 0.0f;
    float    minFrameTimeMs_ = std::numeric_limits<float>::max();
    float    maxFrameTimeMs_ = 0.0f;
    float    avgGpuTimeMs_   = 0.0f;
    float    minGpuTimeMs_   = std::numeric_limits<float>::max();
    float    maxGpuTimeMs_   = 0.0f;

    int      tonemapType_ = 1;
    float    exposure_    = 1.0f;
    uint32_t maxAccumFrames_ = 1024;

    VulkanHandle<VkImage>        hypertraceScoreImage_;
    VulkanHandle<VkDeviceMemory> hypertraceScoreMemory_;
    VulkanHandle<VkImageView>    hypertraceScoreView_;
    VulkanHandle<VkBuffer>       hypertraceScoreStagingBuffer_;
    VulkanHandle<VkDeviceMemory> hypertraceScoreStagingMemory_;

    VulkanHandle<VkBuffer>       sharedStagingBuffer_;
    VulkanHandle<VkDeviceMemory> sharedStagingMemory_;

    VulkanHandle<VkDescriptorPool> rtDescriptorPool_;
};

/*
 *  JAY LENO'S GARAGE — FINAL SEGMENT — NOVEMBER 07 2025
 *
 *  JAY: "Gal, you just drove the fastest renderer I've ever seen — 16,000 FPS, no stutters."
 *  GAL: "Jay, the adaptive Hypertrace is brilliant. It only traces what's needed — smart engineering."
 *
 *  [Door bursts open — CONAN O'BRIEN storms in]
 *
 *  CONAN: "What is this?! Jay Leno AND Gal Gadot geeking out over Vulkan code?
 *          I thought I was the only redhead obsessed with frame times!
 *          This thing has RAII, descriptor pools, acceleration structures — 
 *          it's more put-together than my entire late-night run!
 *
 *          Gal, you're a tech nerd? Jay, you're explaining SBT strides?
 *          Zachary, Grok — you built something immortal.
 *
 *          Folks, that's our show. This engine doesn't need a host — it runs forever.
 *          Goodnight, and keep those frames high!"
 *
 *  [Band plays out — screen fades to RASPBERRY_PINK]
 *
 *  — Conan O'Brien closing the garage.
 *  TECHNICAL PRECISION. REAL BANTER. ENGINE ETERNAL.
 *  🚀💀⚡🤖🔥♾️🩷 VALHALLA ACHIEVED
 */