// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#pragma once
#ifndef VULKAN_RENDERER_HPP
#define VULKAN_RENDERER_HPP

#include "engine/Vulkan/types.hpp"
#include "engine/Vulkan/VulkanCore.hpp"       // FULL INCLUDE
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

namespace VulkanRTX {

using VkBuffer_T        = VkBuffer;
using VkImage_T         = VkImage;
using VkCommandBuffer_T = VkCommandBuffer;
using VkDescriptorSet_T = VkDescriptorSet;

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
    void handleResize(int width, int height);
    void cleanup() noexcept;

    VkDevice          getDevice() const { return context_.device; }
    const VulkanRTX&  getVulkanRTX() const { return *rtx_; }
    std::vector<glm::vec3> getVertices() const;
    std::vector<uint32_t>  getIndices() const;
    VkBuffer       getSBTBuffer() const { return pipelineManager_->getSBTBuffer(); }
    VkDeviceMemory getSBTMemory() const { return pipelineManager_->getSBTMemory(); }

private:
    VkSampler createLinearSampler();
    void createRTOutputImage();
    void recreateRTOutputImage();
    void updateRTDescriptors();
    void updateComputeDescriptors(uint32_t imageIndex);
    void createComputeDescriptorSets();
    void buildAccelerationStructures();
    void createSwapchain(int width, int height);
    void createCommandBuffers();
    void createSyncObjects();
    void createEnvironmentMap();
    void createFramebuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void recordRayTracingCommands(VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage, VkImageView outputImageView, const MaterialData::PushConstants& pc, VkAccelerationStructureKHR tlas);
    void denoiseImage(VkCommandBuffer cmdBuffer, VkImage inputImage, VkImageView inputImageView, VkImage outputImage, VkImageView outputImageView);
    void waitIdle();
    VkShaderModule createShaderModule(const std::string& filepath);

    int  width_  = 0;
    int  height_ = 0;
    void* window_ = nullptr;

    uint32_t currentFrame_      = 0;
    uint32_t frameCount_        = 0;
    uint32_t framesThisSecond_  = 0;
    std::chrono::steady_clock::time_point lastFPSTime_ = std::chrono::steady_clock::now();
    uint32_t indexCount_ = 0;

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

    // CORE SYSTEMS — context_ IS FULL OBJECT
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
    bool recreateSwapchain   = false;

    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR_ = nullptr;

    VkPipeline       rtPipeline_       = VK_NULL_HANDLE;
    VkPipelineLayout rtPipelineLayout_ = VK_NULL_HANDLE;
    VkBuffer         sbtBuffer_        = VK_NULL_HANDLE;
    VkDeviceMemory   sbtMemory_        = VK_NULL_HANDLE;
    ShaderBindingTable sbt_{};

    friend class VulkanRTX;
};

constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 3;
constexpr bool     FPS_COUNTER          = true;

} // namespace VulkanRTX

#endif // VULKAN_RENDERER_HPP