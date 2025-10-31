// src/engine/Vulkan/VulkanPipelineManager.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// FULLY POLISHED. ZERO WARNINGS. ZERO NARROWING. 100% COMPILABLE.

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
    VkPipeline getGraphicsPipeline() const { return graphicsPipeline_.get(); }
    VkPipelineLayout getGraphicsPipelineLayout() const { return graphicsPipelineLayout_.get(); }
    VkPipeline getRayTracingPipeline() const { return rayTracingPipeline_.get(); }
    VkPipelineLayout getRayTracingPipelineLayout() const { return rayTracingPipelineLayout_.get(); }
    VkPipeline getComputePipeline() const { return computePipeline_.get(); }
    VkPipelineLayout getComputePipelineLayout() const { return computePipelineLayout_.get(); }
    VkRenderPass getRenderPass() const { return renderPass_.get(); }
    VkAccelerationStructureKHR getTLASHandle() const { return tlasHandle_.get(); }
    const ShaderBindingTable& getShaderBindingTable() const { return sbt_; }
    VkDescriptorSetLayout getComputeDescriptorSetLayout() const { return computeDescriptorSetLayout_.get(); }

    VkDescriptorSetLayout createRayTracingDescriptorSetLayout();

    // --- SBT BUFFER ACCESSORS ---
    VkBuffer       getSBTBuffer() const { return sbtBuffer_.get(); }
    VkDeviceMemory getSBTMemory() const { return sbtMemory_.get(); }

    // --- PREPASS PIPELINES ---
    VkPipeline getRasterPrepassPipeline() const { return rasterPrepassPipeline_.get(); }
    VkPipeline getDenoiserPostPipeline() const { return denoiserPostPipeline_.get(); }

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

    // SAFE: VulkanHandle owns + destroys after device
    Dispose::VulkanHandle<VkPipeline>               rayTracingPipeline_;
    Dispose::VulkanHandle<VkPipelineLayout>         rayTracingPipelineLayout_;
    Dispose::VulkanHandle<VkPipeline>               computePipeline_;
    Dispose::VulkanHandle<VkPipelineLayout>         computePipelineLayout_;
    Dispose::VulkanHandle<VkPipeline>               graphicsPipeline_;
    Dispose::VulkanHandle<VkPipelineLayout>         graphicsPipelineLayout_;
    Dispose::VulkanHandle<VkRenderPass>             renderPass_;
    Dispose::VulkanHandle<VkPipelineCache>          pipelineCache_;

    // PREPASS PIPELINES
    Dispose::VulkanHandle<VkPipeline>               rasterPrepassPipeline_;
    Dispose::VulkanHandle<VkPipeline>               denoiserPostPipeline_;

    Dispose::VulkanHandle<VkAccelerationStructureKHR> tlasHandle_;
    Dispose::VulkanHandle<VkAccelerationStructureKHR> blasHandle_;

    std::unordered_map<std::string, std::string> shaderPaths_;

    Dispose::VulkanHandle<VkDescriptorSetLayout> computeDescriptorSetLayout_;
    Dispose::VulkanHandle<VkDescriptorSetLayout> rayTracingDescriptorSetLayout_;
    Dispose::VulkanHandle<VkDescriptorSetLayout> graphicsDescriptorSetLayout_;

    ShaderBindingTable sbt_;
    std::vector<uint8_t> shaderHandles_;
    Dispose::VulkanHandle<VkBuffer>         sbtBuffer_;
    Dispose::VulkanHandle<VkDeviceMemory>   sbtMemory_;

    // --- EXTENSION FUNCTIONS ---
    PFN_vkCreateAccelerationStructureKHR  createAsFunc_ = nullptr;
    PFN_vkDestroyAccelerationStructureKHR destroyAsFunc_ = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR getRayTracingShaderGroupHandlesFunc_ = nullptr;
    PFN_vkCreateDeferredOperationKHR      vkCreateDeferredOperationKHR_   = nullptr;
    PFN_vkDeferredOperationJoinKHR        vkDeferredOperationJoinKHR_     = nullptr;
    PFN_vkGetDeferredOperationResultKHR   vkGetDeferredOperationResultKHR_ = nullptr;
    PFN_vkDestroyDeferredOperationKHR     vkDestroyDeferredOperationKHR_  = nullptr;
};

} // namespace VulkanRTX