// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0

#pragma once

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Dispose.hpp"
#include "engine/Vulkan/types.hpp"
#include <vulkan/vulkan.h>
#include <memory>
#include <unordered_map>
#include <string>
#include <vector>
#include <chrono>

#ifdef ENABLE_VULKAN_DEBUG
#include <vulkan/vulkan_ext_debug_utils.h>
#endif

namespace VulkanRTX {

class VulkanPipelineManager {
public:
    VulkanPipelineManager(Vulkan::Context& context, int width, int height);
    ~VulkanPipelineManager();

    void createRayTracingPipeline();
    void createComputePipeline();
    void createGraphicsPipeline(int width, int height);
    void createShaderBindingTable();

    void createAccelerationStructures(VkBuffer vertexBuffer, VkBuffer indexBuffer);
    void updateRayTracingDescriptorSet(VkDescriptorSet descriptorSet, VkAccelerationStructureKHR tlasHandle);

    void recordGraphicsCommands(VkCommandBuffer cmd, VkFramebuffer fb, VkDescriptorSet ds,
                                uint32_t w, uint32_t h, VkImage denoiseImage = VK_NULL_HANDLE);
    void recordRayTracingCommands(VkCommandBuffer cmd, VkImage outputImage,
                                  VkDescriptorSet descSet, uint32_t width, uint32_t height,
                                  VkImage gDepth, VkImage gNormal);
    void recordComputeCommands(VkCommandBuffer cmd, VkImage outputImage,
                               VkDescriptorSet ds, uint32_t w, uint32_t h,
                               VkImage gDepth, VkImage gNormal, VkImage denoiseImage);

    // --- PUBLIC GETTERS ---
    VkPipeline getGraphicsPipeline() const { return graphicsPipeline_ ? graphicsPipeline_->get() : VK_NULL_HANDLE; }
    VkPipelineLayout getGraphicsPipelineLayout() const { return graphicsPipelineLayout_ ? graphicsPipelineLayout_->get() : VK_NULL_HANDLE; }
    VkPipeline getRayTracingPipeline() const { return rayTracingPipeline_ ? rayTracingPipeline_->get() : VK_NULL_HANDLE; }
    VkPipelineLayout getRayTracingPipelineLayout() const { return rayTracingPipelineLayout_ ? rayTracingPipelineLayout_->get() : VK_NULL_HANDLE; }
    VkPipeline getComputePipeline() const { return computePipeline_ ? computePipeline_->get() : VK_NULL_HANDLE; }
    VkPipelineLayout getComputePipelineLayout() const { return computePipelineLayout_ ? computePipelineLayout_->get() : VK_NULL_HANDLE; }
    VkRenderPass getRenderPass() const { return renderPass_; }
    VkAccelerationStructureKHR getTLASHandle() const { return tlasHandle_; }
    const ShaderBindingTable& getShaderBindingTable() const { return sbt_; }
    VkDescriptorSetLayout getComputeDescriptorSetLayout() const { return computeDescriptorSetLayout_; }

    VkDescriptorSetLayout createRayTracingDescriptorSetLayout();

    // --- NEW: SBT BUFFER ACCESSORS ---
    VkBuffer       getSBTBuffer() const { return sbtBuffer_; }
    VkDeviceMemory getSBTMemory() const { return sbtMemory_; }

    // --- FRAME TIME LOGGING ---
    void logFrameTimeIfSlow(std::chrono::steady_clock::time_point start);

private:
    VkShaderModule loadShader(VkDevice device, const std::string& shaderType);
    void createGraphicsDescriptorSetLayout();
    void createComputeDescriptorSetLayout();
    void createPipelineCache();
    void createRenderPass();
    void compileDeferredRayTracingPipeline(
        const std::vector<VkPipelineShaderStageCreateInfo>& stages,
        const std::vector<VkRayTracingShaderGroupCreateInfoKHR>& groups
    );

#ifdef ENABLE_VULKAN_DEBUG
    void setupDebugCallback();
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
#endif

    Vulkan::Context& context_;
    int width_, height_;

    std::unique_ptr<VulkanResource<VkPipeline, PFN_vkDestroyPipeline>> rayTracingPipeline_;
    std::unique_ptr<VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>> rayTracingPipelineLayout_;
    std::unique_ptr<VulkanResource<VkPipeline, PFN_vkDestroyPipeline>> computePipeline_;
    std::unique_ptr<VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>> computePipelineLayout_;
    std::unique_ptr<VulkanResource<VkPipeline, PFN_vkDestroyPipeline>> graphicsPipeline_;
    std::unique_ptr<VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>> graphicsPipelineLayout_;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipelineCache pipelineCache_ = VK_NULL_HANDLE;
    VkPipeline rasterPrepassPipeline_ = VK_NULL_HANDLE;
    VkPipeline denoiserPostPipeline_ = VK_NULL_HANDLE;

    VkAccelerationStructureKHR tlasHandle_ = VK_NULL_HANDLE;
    VkAccelerationStructureKHR blasHandle_ = VK_NULL_HANDLE;

    std::unordered_map<std::string, std::string> shaderPaths_;

    VkDescriptorSetLayout computeDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout rayTracingDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout graphicsDescriptorSetLayout_ = VK_NULL_HANDLE;

    ShaderBindingTable sbt_;
    std::vector<uint8_t> shaderHandles_;
    VkBuffer             sbtBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory       sbtMemory_ = VK_NULL_HANDLE;

    struct PlatformConfig {
        uint32_t graphicsQueueFamily;
        uint32_t computeQueueFamily;
        bool preferDeviceLocalMemory;
    } platformConfig_;

    // --- INIT ORDER FIXED: resourceManager_ first ---
    VulkanResourceManager resourceManager_;                     // <-- BEFORE function pointers

    PFN_vkCreateAccelerationStructureKHR  createAsFunc_ = nullptr;
    PFN_vkDestroyAccelerationStructureKHR destroyAsFunc_ = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR getRayTracingShaderGroupHandlesFunc_ = nullptr;

    // --- DEFERRED COMPILATION ---
    PFN_vkCreateDeferredOperationKHR      vkCreateDeferredOperationKHR_   = nullptr;
    PFN_vkDeferredOperationJoinKHR        vkDeferredOperationJoinKHR_     = nullptr;
    PFN_vkGetDeferredOperationResultKHR   vkGetDeferredOperationResultKHR_= nullptr;
    PFN_vkDestroyDeferredOperationKHR     vkDestroyDeferredOperationKHR_  = nullptr;
};

} // namespace VulkanRTX