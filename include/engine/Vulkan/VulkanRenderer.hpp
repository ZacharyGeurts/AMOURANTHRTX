// In include/engine/Vulkan/VulkanRenderer.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
#pragma once
#ifndef VULKAN_RENDERER_HPP
#define VULKAN_RENDERER_HPP

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Vulkan/VulkanPipelineManager.hpp"
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/Vulkan/VulkanSwapchainManager.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <memory>
#include <glm/glm.hpp>
#include <string>
#include <array>
#include <chrono>

namespace UE {
struct DimensionData; // Forward declaration
struct UniformBufferObject; // Forward declaration
}

class Camera; // Forward declaration

namespace VulkanRTX {

struct Vertex {
    alignas(16) glm::vec3 pos;
    alignas(8) glm::vec2 uv;
};

struct Frame {
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkDescriptorSet rayTracingDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet graphicsDescriptorSet = VK_NULL_HANDLE;
    VkDescriptorSet computeDescriptorSet = VK_NULL_HANDLE;
    VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
    VkFence fence = VK_NULL_HANDLE;
};

class VulkanRenderer {
public:
    VulkanRenderer(int width, int height, void* window, const std::vector<std::string>& instanceExtensions);
    ~VulkanRenderer();

    void initializeAllBufferData(uint32_t maxFrames, VkDeviceSize uniformBufferSize, VkDeviceSize dimensionBufferSize);
    void initializeBufferData(uint32_t frameIndex, VkDeviceSize materialSize, VkDeviceSize dimensionSize);
    void updateDescriptorSetForFrame(uint32_t frameIndex, VkAccelerationStructureKHR tlas);
    void updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas);
    void updateGraphicsDescriptorSet(uint32_t frameIndex);
    void updateComputeDescriptorSet(uint32_t frameIndex);
    void renderFrame(const Camera& camera);
    void handleResize(int width, int height);
    void cleanup() noexcept;

    VkDevice getDevice() const { return context_.device; }
    const VulkanRTX& getVulkanRTX() const { return *rtx_; }

    std::vector<glm::vec3> getVertices() const;
    std::vector<Vertex> getFullVertices() const; // Added declaration
    std::vector<uint32_t> getIndices() const;

private:
    void createSwapchain(int width, int height);
    void createCommandBuffers();
    void createSyncObjects();
    void createEnvironmentMap();
    void createFramebuffers();
    void createAccelerationStructures();
    void recordRayTracingCommands(VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage,
                                  VkImageView outputImageView, const MaterialData::PushConstants& pc,
                                  VkAccelerationStructureKHR tlas);
    void denoiseImage(VkCommandBuffer cmdBuffer, VkImage inputImage, VkImageView inputImageView,
                      VkImage outputImage, VkImageView outputImageView);

    int width_;
    int height_;
    void* window_;
    uint32_t currentFrame_;
    uint32_t frameCount_;
    std::chrono::steady_clock::time_point lastLogTime_;
    uint32_t framesSinceLastLog_;
    VkPipeline rtPipeline_;
    VkPipelineLayout rtPipelineLayout_;
    VkImage denoiseImage_;
    VkDeviceMemory denoiseImageMemory_;
    VkImageView denoiseImageView_;
    VkSampler denoiseSampler_;
    VkImage envMapImage_;
    VkDeviceMemory envMapImageMemory_;
    VkImageView envMapImageView_;
    VkSampler envMapSampler_;
    VkDescriptorSetLayout computeDescriptorSetLayout_;
    Vulkan::Context context_;
    std::unique_ptr<VulkanRTX> rtx_;
    std::unique_ptr<VulkanSwapchainManager> swapchainManager_;
    std::unique_ptr<VulkanPipelineManager> pipelineManager_;
    std::unique_ptr<VulkanBufferManager> bufferManager_;
    std::vector<Frame> frames_;
    std::vector<VkBuffer> materialBuffers_;
    std::vector<VkDeviceMemory> materialBufferMemory_;
    std::vector<VkBuffer> dimensionBuffers_;
    std::vector<VkDeviceMemory> dimensionBufferMemory_;
};

const uint32_t MAX_FRAMES_IN_FLIGHT = 3;

} // namespace VulkanRTX

#endif // VULKAN_RENDERER_HPP