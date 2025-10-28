// include/engine/Vulkan/VulkanPipelineManager.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0

#pragma once

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"  // <-- MUST include for ShaderBindingTable
#include "engine/Dispose.hpp"
#include "engine/MaterialData.hpp"
#include <vulkan/vulkan.h>
#include <memory>
#include <unordered_map>
#include <string>

#ifdef ENABLE_VULKAN_DEBUG
#include <vulkan/vulkan_ext_debug_utils.h>
#endif

namespace VulkanRTX {

// ---------------------------------------------------------------------
//  VulkanPipelineManager
// ---------------------------------------------------------------------
class VulkanPipelineManager {
public:
    VulkanPipelineManager(Vulkan::Context& context, int width, int height);
    ~VulkanPipelineManager();

    // Pipeline creation
    void createRayTracingPipeline();
    void createComputePipeline();
    void createGraphicsPipeline(int width, int height);
    void createShaderBindingTable();

    // Acceleration structures
    void createAccelerationStructures(VkBuffer vertexBuffer, VkBuffer indexBuffer);
    void updateRayTracingDescriptorSet(VkDescriptorSet descriptorSet, VkAccelerationStructureKHR tlasHandle);

    // Command recording
    void recordGraphicsCommands(VkCommandBuffer cmd, VkFramebuffer fb, VkDescriptorSet ds,
                                uint32_t w, uint32_t h, VkImage denoiseImage = VK_NULL_HANDLE);
    void recordRayTracingCommands(VkCommandBuffer cmd, VkImage outputImage,
                                  VkDescriptorSet descSet, uint32_t width, uint32_t height,
                                  VkImage gDepth, VkImage gNormal);
    void recordComputeCommands(VkCommandBuffer cmd, VkImage outputImage,
                               VkDescriptorSet ds, uint32_t w, uint32_t h,
                               VkImage gDepth, VkImage gNormal, VkImage denoiseImage);

    // --------------------- PUBLIC GETTERS ---------------------
    VkPipeline               getGraphicsPipeline() const { return graphicsPipeline_ ? graphicsPipeline_->get() : VK_NULL_HANDLE; }
    VkPipelineLayout         getGraphicsPipelineLayout() const { return graphicsPipelineLayout_ ? graphicsPipelineLayout_->get() : VK_NULL_HANDLE; }
    VkPipeline               getRayTracingPipeline() const { return rayTracingPipeline_ ? rayTracingPipeline_->get() : VK_NULL_HANDLE; }
    VkPipelineLayout         getRayTracingPipelineLayout() const { return rayTracingPipelineLayout_ ? rayTracingPipelineLayout_->get() : VK_NULL_HANDLE; }
    VkPipeline               getComputePipeline() const { return computePipeline_ ? computePipeline_->get() : VK_NULL_HANDLE; }
    VkPipelineLayout         getComputePipelineLayout() const { return computePipelineLayout_ ? computePipelineLayout_->get() : VK_NULL_HANDLE; }
    VkRenderPass             getRenderPass() const { return renderPass_; }
    VkAccelerationStructureKHR getTLASHandle() const { return tlasHandle_; }
    const ShaderBindingTable& getShaderBindingTable() const { return sbt_; }

private:
    VkShaderModule loadShader(VkDevice device, const std::string& shaderType);
    void createRayTracingDescriptorSetLayout();
    void createGraphicsDescriptorSetLayout();
    void createPipelineCache();

#ifdef ENABLE_VULKAN_DEBUG
    void setupDebugCallback();
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
#endif

    // Context
    Vulkan::Context& context_;
    int width_, height_;

    // Pipelines
    std::unique_ptr<VulkanResource<VkPipeline, PFN_vkDestroyPipeline>> rayTracingPipeline_;
    std::unique_ptr<VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>> rayTracingPipelineLayout_;
    std::unique_ptr<VulkanResource<VkPipeline, PFN_vkDestroyPipeline>> computePipeline_;
    std::unique_ptr<VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>> computePipelineLayout_;
    std::unique_ptr<VulkanResource<VkPipeline, PFN_vkDestroyPipeline>> graphicsPipeline_;
    std::unique_ptr<VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>> graphicsPipelineLayout_;

    // Render pass
    VkRenderPass renderPass_ = VK_NULL_HANDLE;

    // Pipeline cache
    VkPipelineCache pipelineCache_ = VK_NULL_HANDLE;

    // Extra compute pipelines
    VkPipeline rasterPrepassPipeline_ = VK_NULL_HANDLE;
    VkPipeline denoiserPostPipeline_ = VK_NULL_HANDLE;

    // Acceleration structures (order fixed for init warning)
    VkAccelerationStructureKHR tlasHandle_ = VK_NULL_HANDLE;
    VkAccelerationStructureKHR blasHandle_ = VK_NULL_HANDLE;

    // Shader paths
    std::unordered_map<std::string, std::string> shaderPaths_;

    // SBT
    ShaderBindingTable sbt_;

    // Platform config
    struct PlatformConfig {
        uint32_t graphicsQueueFamily;
        uint32_t computeQueueFamily;
        bool preferDeviceLocalMemory;
    } platformConfig_;

    // AS function pointers
    PFN_vkCreateAccelerationStructureKHR  createAsFunc_ = nullptr;
    PFN_vkDestroyAccelerationStructureKHR destroyAsFunc_ = nullptr;
};

} // namespace VulkanRTX