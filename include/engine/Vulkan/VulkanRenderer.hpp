// include/engine/Vulkan/VulkanRenderer.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#pragma once
#ifndef VULKAN_RENDERER_HPP
#define VULKAN_RENDERER_HPP

#include "engine/Vulkan/types.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/Dispose.hpp"
#include "engine/camera.hpp"
#include "engine/logging.hpp"

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <string>
#include <array>
#include <chrono>
#include <cstdint>
#include <atomic>

namespace VulkanRTX {

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;

struct StridedDeviceAddressRegionKHR {
    VkDeviceAddress deviceAddress = 0;
    VkDeviceSize    stride        = 0;
    VkDeviceSize    size          = 0;
};

struct Frame {
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkDescriptorSet rayTracingDescriptorSet     = VK_NULL_HANDLE;
    VkDescriptorSet graphicsDescriptorSet       = VK_NULL_HANDLE;
    VkDescriptorSet computeDescriptorSet        = VK_NULL_HANDLE;
    VkSemaphore     imageAvailableSemaphore     = VK_NULL_HANDLE;
    VkSemaphore     renderFinishedSemaphore     = VK_NULL_HANDLE;
    VkFence         fence                       = VK_NULL_HANDLE;
};

class VulkanRenderer {
public:
    VulkanRenderer(int width, int height, void* window,
                   const std::vector<std::string>& instanceExtensions);
    ~VulkanRenderer();

    void initializeAllBufferData(uint32_t maxFrames, VkDeviceSize materialSize, VkDeviceSize dimensionSize);
    void initializeBufferData(uint32_t frameIndex, VkDeviceSize materialSize, VkDeviceSize dimensionSize);
    void updateDescriptorSetForFrame(uint32_t frameIndex, VkAccelerationStructureKHR tlas);
    void updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas);
    void updateGraphicsDescriptorSet(uint32_t frameIndex);
    void updateComputeDescriptorSet(uint32_t frameIndex);
    void updateUniformBuffer(uint32_t frameIndex, const Camera& camera);
    void renderFrame(const Camera& camera);
    void handleResize(int width, int height);   // immediate resize
    void cleanup() noexcept;

    VkDevice          getDevice() const { return context_.device; }
    const VulkanRTX&  getVulkanRTX() const { return *rtx_; }
    std::vector<glm::vec3> getVertices() const;
    std::vector<uint32_t>  getIndices() const;
    VkBuffer       getSBTBuffer() const { return pipelineManager_->getSBTBuffer(); }
    VkDeviceMemory getSBTMemory() const { return pipelineManager_->getSBTMemory(); }

private:
    // --- Shader & Resource Creation ---
    VkShaderModule createShaderModule(const std::string& filepath);

    // --- Image Management ---
    void createRTOutputImage();
    void createAccumulationImage();
    void recreateRTOutputImage();
    void recreateAccumulationImage();

    // --- Descriptor Management ---
    void updateRTDescriptors();
    void updateComputeDescriptors(uint32_t imageIndex);
    void createComputeDescriptorSets();
    void createDescriptorPool();
    void createDescriptorSets();

    // --- Acceleration Structures ---
    void buildAccelerationStructures();

    // --- Render Setup ---
    void createCommandBuffers();
    void createEnvironmentMap();
    void createFramebuffers();

    // --- Resize ---
    void applyResize(int newWidth, int newHeight);   // takes width/height

    // --- Member Variables ---
    int  width_  = 0;
    int  height_ = 0;
    void* window_ = nullptr;

    uint32_t currentFrame_      = 0;
    uint32_t frameCount_        = 0;
    uint32_t framesThisSecond_  = 0;
    std::chrono::steady_clock::time_point lastFPSTime_ = std::chrono::steady_clock::now();
    uint32_t indexCount_ = 0;

    // --- Images ---
    VkImage             denoiseImage_             = VK_NULL_HANDLE;
    VkDeviceMemory      denoiseImageMemory_       = VK_NULL_HANDLE;
    VkImageView         denoiseImageView_         = VK_NULL_HANDLE;
    VkSampler           denoiseSampler_           = VK_NULL_HANDLE;
    VkImage             envMapImage_              = VK_NULL_HANDLE;
    VkDeviceMemory      envMapImageMemory_        = VK_NULL_HANDLE;
    VkImageView         envMapImageView_          = VK_NULL_HANDLE;
    VkSampler           envMapSampler_            = VK_NULL_HANDLE;

    std::array<VkImage,        2> rtOutputImages_   = {};
    std::array<VkDeviceMemory, 2> rtOutputMemories_ = {};
    std::array<VkImageView,    2> rtOutputViews_    = {};
    uint32_t currentRTIndex_ = 0;

    std::array<VkImage,        2> accumImages_   = {};
    std::array<VkDeviceMemory, 2> accumMemories_ = {};
    std::array<VkImageView,    2> accumViews_    = {};
    uint32_t currentAccumIndex_ = 0;
    bool resetAccumulation_ = true;

    // ACCELERATION STRUCTURES
    VkAccelerationStructureKHR blasHandle_ = VK_NULL_HANDLE;
    VkBuffer                    blasBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory              blasBufferMemory_ = VK_NULL_HANDLE;
    VkAccelerationStructureKHR tlasHandle_ = VK_NULL_HANDLE;
    VkBuffer                    tlasBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory              tlasBufferMemory_ = VK_NULL_HANDLE;
    VkBuffer                    instanceBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory              instanceBufferMemory_ = VK_NULL_HANDLE;
    VkDeviceAddress             tlasDeviceAddress_ = 0;

    Dispose::VulkanHandle<VkImage>        rtOutputImage_;
    Dispose::VulkanHandle<VkDeviceMemory> rtOutputImageMemory_;
    Dispose::VulkanHandle<VkImageView>    rtOutputImageView_;
    Dispose::VulkanHandle<VkImage>        accumImage_;
    Dispose::VulkanHandle<VkDeviceMemory> accumImageMemory_;
    Dispose::VulkanHandle<VkImageView>    accumImageView_;

    // CORE SYSTEMS
    Vulkan::Context                         context_;
    std::unique_ptr<VulkanRTX>              rtx_;
    std::unique_ptr<VulkanSwapchainManager> swapchainManager_;
    std::unique_ptr<VulkanPipelineManager> pipelineManager_;
    std::unique_ptr<VulkanBufferManager>    bufferManager_;

    std::vector<Frame>          frames_;
    std::vector<VkFramebuffer>  framebuffers_;
    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkDescriptorSet> descriptorSets_;
    std::vector<VkDescriptorSet> computeDescriptorSets_;
    VkDescriptorPool            descriptorPool_ = VK_NULL_HANDLE;

    std::vector<VkBuffer>       materialBuffers_;
    std::vector<VkDeviceMemory> materialBufferMemory_;
    std::vector<VkBuffer>       dimensionBuffers_;
    std::vector<VkDeviceMemory> dimensionBufferMemory_;

    std::unique_ptr<PerspectiveCamera> camera_;
    bool descriptorsUpdated_ = false;

    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR_ = nullptr;

    VkPipeline       rtPipeline_       = VK_NULL_HANDLE;
    VkPipelineLayout rtPipelineLayout_ = VK_NULL_HANDLE;
    VkBuffer         sbtBuffer_        = VK_NULL_HANDLE;
    VkDeviceMemory   sbtMemory_        = VK_NULL_HANDLE;
    ShaderBindingTable sbt_{};

    // RESIZE STATE
    std::atomic<bool> swapchainRecreating_{false};

    // GPU TIMING
    std::array<VkQueryPool, MAX_FRAMES_IN_FLIGHT> queryPools_ = {};
    std::array<bool, MAX_FRAMES_IN_FLIGHT>       queryReady_{false};
    std::array<double, MAX_FRAMES_IN_FLIGHT>     lastRTTimeMs_{0.0};

    VkDescriptorSetLayout computeDescriptorSetLayout_ = VK_NULL_HANDLE;

    friend class VulkanRTX;
};

constexpr bool FPS_COUNTER = true;

} // namespace VulkanRTX

#endif // VULKAN_RENDERER_HPP