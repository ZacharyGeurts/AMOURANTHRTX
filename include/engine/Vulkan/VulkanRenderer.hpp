// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#pragma once
#ifndef VULKAN_RENDERER_HPP
#define VULKAN_RENDERER_HPP

// ---------------------------------------------------------------
// 1. Core engine types (DimensionData, UniformBufferObject, AMOURANTH)
// ---------------------------------------------------------------
#include "engine/Vulkan/types.hpp"          // <-- ONLY THIS

// ---------------------------------------------------------------
// 2. Vulkan core & helper components
// ---------------------------------------------------------------
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include "engine/camera.hpp"                // PerspectiveCamera, Camera base

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <string>
#include <array>
#include <chrono>

namespace VulkanRTX {

// -----------------------------------------------------------------
// Typedefs for readability
// -----------------------------------------------------------------
using VkBuffer_T        = VkBuffer;
using VkImage_T         = VkImage;
using VkCommandBuffer_T = VkCommandBuffer;
using VkDescriptorSet_T = VkDescriptorSet;

// -----------------------------------------------------------------
// Vertex layout (used by graphics pipeline)
// -----------------------------------------------------------------
struct Vertex {
    alignas(16) glm::vec3 pos;
    alignas(8)  glm::vec2 uv;
};

// -----------------------------------------------------------------
// Per-frame synchronization & descriptor sets
// -----------------------------------------------------------------
struct Frame {
    VkCommandBuffer commandBuffer               = VK_NULL_HANDLE;
    VkDescriptorSet rayTracingDescriptorSet     = VK_NULL_HANDLE;
    VkDescriptorSet graphicsDescriptorSet      = VK_NULL_HANDLE;
    VkDescriptorSet computeDescriptorSet        = VK_NULL_HANDLE;
    VkSemaphore     imageAvailableSemaphore     = VK_NULL_HANDLE;
    VkSemaphore     renderFinishedSemaphore     = VK_NULL_HANDLE;
    VkFence         fence                       = VK_NULL_HANDLE;
};

// -----------------------------------------------------------------
// Main renderer class
// -----------------------------------------------------------------
class VulkanRenderer {
public:
    VulkanRenderer(int width, int height, void* window,
                   const std::vector<std::string>& instanceExtensions);
    ~VulkanRenderer();

    // -----------------------------------------------------------------
    // Buffer initialisation
    // -----------------------------------------------------------------
    void initializeAllBufferData(uint32_t maxFrames,
                                 VkDeviceSize uniformBufferSize,
                                 VkDeviceSize dimensionBufferSize);
    void initializeBufferData(uint32_t frameIndex,
                              VkDeviceSize materialSize,
                              VkDeviceSize dimensionSize);

    // -----------------------------------------------------------------
    // Descriptor set updates
    // -----------------------------------------------------------------
    void updateDescriptorSetForFrame(uint32_t frameIndex,
                                     VkAccelerationStructureKHR tlas);
    void updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas);
    void updateGraphicsDescriptorSet(uint32_t frameIndex);
    void updateComputeDescriptorSet(uint32_t frameIndex);

    // -----------------------------------------------------------------
    // Rendering
    // -----------------------------------------------------------------
    void renderFrame(const Camera& camera);
    void handleResize(int width, int height);
    void cleanup() noexcept;

    // -----------------------------------------------------------------
    // Simple getters
    // -----------------------------------------------------------------
    VkDevice          getDevice() const { return context_.device; }
    const VulkanRTX&  getVulkanRTX() const { return *rtx_; }

    std::vector<glm::vec3> getVertices() const;
    std::vector<Vertex>    getFullVertices() const;
    std::vector<uint32_t>  getIndices() const;

private:
    // -----------------------------------------------------------------
    // Internal creation helpers
    // -----------------------------------------------------------------
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

    // -----------------------------------------------------------------
    // Member variables
    // -----------------------------------------------------------------
    int  width_;
    int  height_;
    void* window_;

    uint32_t currentFrame_      = 0;
    uint32_t frameCount_        = 0;
    uint32_t framesThisSecond_  = 0;
    std::chrono::steady_clock::time_point lastFPSTime_;

    uint32_t framesSinceLastLog_ = 0;
    std::chrono::steady_clock::time_point lastLogTime_;

    uint32_t indexCount_ = 0;

    // Pipelines / images
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

    // *** ACCELERATION STRUCTURES ***
    VkAccelerationStructureKHR blasHandle_ = VK_NULL_HANDLE;
    VkAccelerationStructureKHR tlasHandle_ = VK_NULL_HANDLE;

    // Core Vulkan components
    Vulkan::Context                         context_;
    std::unique_ptr<VulkanRTX>              rtx_;
    std::unique_ptr<VulkanSwapchainManager> swapchainManager_;
    std::unique_ptr<VulkanPipelineManager> pipelineManager_;
    std::unique_ptr<VulkanBufferManager>    bufferManager_;

    // Per-frame data
    std::vector<Frame>          frames_;
    std::vector<VkBuffer>       materialBuffers_;
    std::vector<VkDeviceMemory> materialBufferMemory_;
    std::vector<VkBuffer>       dimensionBuffers_;
    std::vector<VkDeviceMemory> dimensionBufferMemory_;

    // Camera (owned by renderer – we use PerspectiveCamera internally)
    std::unique_ptr<PerspectiveCamera> camera_;
};

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
constexpr bool     FPS_COUNTER          = true;

} // namespace VulkanRTX

#endif // VULKAN_RENDERER_HPP