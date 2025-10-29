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
#include "engine/camera.hpp"

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <string>
#include <array>
#include <chrono>
#include <cstdint>

namespace VulkanRTX {

using VkBuffer_T        = VkBuffer;
using VkImage_T         = VkImage;
using VkCommandBuffer_T = VkCommandBuffer;
using VkDescriptorSet_T = VkDescriptorSet;

struct Vertex {
    alignas(16) glm::vec3 pos;
    alignas(8)  glm::vec2 uv;
};

struct Frame {
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkDescriptorSet rayTracingDescriptorSet     = VK_NULL_HANDLE;
    VkDescriptorSet graphicsDescriptorSet      = VK_NULL_HANDLE;
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

    void initializeAllBufferData(uint32_t maxFrames,
                                 VkDeviceSize materialSize,
                                 VkDeviceSize dimensionSize);
    void initializeBufferData(uint32_t frameIndex,
                              VkDeviceSize materialSize,
                              VkDeviceSize dimensionSize);

    void updateDescriptorSetForFrame(uint32_t frameIndex,
                                     VkAccelerationStructureKHR tlas);
    void updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas);
    void updateGraphicsDescriptorSet(uint32_t frameIndex);
    void updateComputeDescriptorSet(uint32_t frameIndex);

    void renderFrame(const Camera& camera);
    void handleResize(int width, int height);
    void cleanup() noexcept;

    VkDevice          getDevice() const { return context_.device; }
    const VulkanRTX&  getVulkanRTX() const { return *rtx_; }

    std::vector<glm::vec3> getVertices() const;
    std::vector<Vertex>    getFullVertices() const;
    std::vector<uint32_t>  getIndices() const;

private:
    void createSwapchain(int width, int height);
    void createCommandBuffers();
    void createSyncObjects();
    void createEnvironmentMap();
    void createFramebuffers();
    void createAccelerationStructures();
    void createDescriptorPool();
    void createDescriptorSets();

    void recordRayTracingCommands(VkCommandBuffer cmdBuffer,
                                  VkExtent2D extent,
                                  VkImage outputImage,
                                  VkImageView outputImageView,
                                  const MaterialData::PushConstants& pc,
                                  VkAccelerationStructureKHR tlas);
    void denoiseImage(VkCommandBuffer cmdBuffer,
                      VkImage inputImage, VkImageView inputImageView,
                      VkImage outputImage, VkImageView outputImageView);
    void waitIdle();

    // --- Basic window/state ---
    int  width_;
    int  height_;
    void* window_;

    // --- Frame tracking (must come before lastFPSTime_) ---
    uint32_t currentFrame_      = 0;
    uint32_t frameCount_        = 0;
    uint32_t framesThisSecond_  = 0;
    std::chrono::steady_clock::time_point lastFPSTime_;

    uint32_t framesSinceLastLog_ = 0;
    std::chrono::steady_clock::time_point lastLogTime_;

    uint32_t indexCount_ = 0;

    // --- Vulkan core objects ---
    VkPipeline          rtPipeline_               = VK_NULL_HANDLE;
    VkPipelineLayout    rtPipelineLayout_         = VK_NULL_HANDLE;
    VkImage             denoiseImage_             = VK_NULL_HANDLE;
    VkDeviceMemory      denoiseImageMemory_       = VK_NULL_HANDLE;
    VkImageView         denoiseImageView_         = VK_NULL_HANDLE;
    VkSampler           denoiseSampler_           = VK_NULL_HANDLE;
    VkImage             envMapImage_              = VK_NULL_HANDLE;
    VkDeviceMemory      envMapImageMemory_        = VK_NULL_HANDLE;
    VkImageView         envMapImageView_          = VK_NULL_HANDLE;
    VkSampler           envMapSampler_            = VK_NULL_HANDLE;
    VkDescriptorSetLayout computeDescriptorSetLayout_ = VK_NULL_HANDLE;

    VkAccelerationStructureKHR blasHandle_ = VK_NULL_HANDLE;
    VkAccelerationStructureKHR tlasHandle_ = VK_NULL_HANDLE;

    // --- Core systems ---
    Vulkan::Context                         context_;
    std::unique_ptr<VulkanRTX>              rtx_;
    std::unique_ptr<VulkanSwapchainManager> swapchainManager_;
    std::unique_ptr<VulkanPipelineManager> pipelineManager_;
    std::unique_ptr<VulkanBufferManager>    bufferManager_;

    // --- Per-frame resources ---
    std::vector<Frame>          frames_;
    std::vector<VkFramebuffer>  framebuffers_;
    std::vector<VkCommandBuffer> commandBuffers_;
    std::vector<VkDescriptorSet> descriptorSets_;
    VkDescriptorPool            descriptorPool_ = VK_NULL_HANDLE;

    // --- Buffer vectors ---
    std::vector<VkBuffer>       materialBuffers_;
    std::vector<VkDeviceMemory> materialBufferMemory_;
    std::vector<VkBuffer>       dimensionBuffers_;
    std::vector<VkDeviceMemory> dimensionBufferMemory_;

    // --- Camera ---
    std::unique_ptr<PerspectiveCamera> camera_;

    // --- Flags (should come after things they depend on) ---
    bool descriptorsUpdated_ = false;
    bool recreateSwapchain   = false;

    friend class VulkanRTX;
};

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
constexpr bool     FPS_COUNTER          = true;

} // namespace VulkanRTX

#endif // VULKAN_RENDERER_HPP