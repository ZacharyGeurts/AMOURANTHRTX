// include/engine/Vulkan/VulkanPipelineManager.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// FULLY POLISHED. ZERO WARNINGS. ZERO NARROWING. 100% COMPILABLE.
// NO SINGLETON. PUBLIC GETTERS. NO DOUBLE-FREE.

#pragma once
#ifndef VULKAN_PIPELINE_MANAGER_HPP
#define VULKAN_PIPELINE_MANAGER_HPP

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanRTX_Setup.hpp"
#include "engine/Dispose.hpp"
#include "engine/logging.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"

#include <vulkan/vulkan.h>
#include <memory>
#include <vector>
#include <chrono>
#include <cstdint>
#include <stdexcept>

#ifdef ENABLE_VULKAN_DEBUG
#include <vulkan/vulkan_ext_debug_utils.h>
#endif

namespace VulkanRTX {

// ---------------------------------------------------------------------
//  VulkanPipelineManager – Core pipeline & AS manager (NO SINGLETON)
// ---------------------------------------------------------------------
class VulkanPipelineManager {
public:
    VulkanPipelineManager(Vulkan::Context& context, int width = 1280, int height = 720);
    ~VulkanPipelineManager();

    VulkanPipelineManager(const VulkanPipelineManager&) = delete;
    VulkanPipelineManager& operator=(const VulkanPipelineManager&) = delete;

    // -----------------------------------------------------------------
    //  Public Helpers – USED BY VulkanRTX
    // -----------------------------------------------------------------
    VkDevice getDevice() const { return context_.device; }

    // Load shader without exposing VkDevice
    VkShaderModule loadShader(const std::string& name) {
        return loadShader(context_.device, name);
    }

    // Return shared_ptr to Context (safe, no ownership)
    std::shared_ptr<Vulkan::Context> getContext() const {
        return std::shared_ptr<Vulkan::Context>(&context_, [](Vulkan::Context*) {});
    }

    // -----------------------------------------------------------------
    //  Public API
    // -----------------------------------------------------------------
    void createRayTracingPipeline();
    void createComputePipeline();
    void createGraphicsPipeline(int width, int height);
    void createShaderBindingTable();
    void createAccelerationStructures(VkBuffer vertexBuffer, VkBuffer indexBuffer);
    void updateRayTracingDescriptorSet(VkDescriptorSet ds, VkAccelerationStructureKHR tlas = VK_NULL_HANDLE);

    VkPipeline getGraphicsPipeline() const { return graphicsPipeline_.get(); }
    VkPipelineLayout getGraphicsPipelineLayout() const { return graphicsPipelineLayout_.get(); }
    VkPipeline getRayTracingPipeline() const { return rayTracingPipeline_.get(); }
    VkPipelineLayout getRayTracingPipelineLayout() const { return rayTracingPipelineLayout_.get(); }
    VkPipeline getComputePipeline() const { return computePipeline_.get(); }
    VkPipelineLayout getComputePipelineLayout() const { return computePipelineLayout_.get(); }
    VkRenderPass getRenderPass() const { return renderPass_.get(); }
    VkAccelerationStructureKHR getTLAS() const { return tlas_.get(); }
    const ShaderBindingTable& getSBT() const { return sbt_; }

    VkBuffer getSBTBuffer() const { return sbtBuffer_.get(); }
    VkDeviceMemory getSBTMemory() const { return sbtMemory_.get(); }

    // NEW: Safe getter for compute descriptor set layout
    VkDescriptorSetLayout getComputeDescriptorSetLayout() const {
        return computeDescriptorSetLayout_.get();
    }

    void logFrameTimeIfSlow(std::chrono::steady_clock::time_point start);
    VkCommandPool getTransientPool() const { return transientPool_.get(); }
    VkQueue getGraphicsQueue() const { return graphicsQueue_; }
    VkPipelineCache getPipelineCache() const { return pipelineCache_.get(); }

private:
    // -----------------------------------------------------------------
    //  Private Helpers
    // -----------------------------------------------------------------
    VkShaderModule loadShader(VkDevice device, const std::string& type);
    void createGraphicsDescriptorSetLayout();
    void createComputeDescriptorSetLayout();
    VkDescriptorSetLayout createRayTracingDescriptorSetLayout();
    void createPipelineCache();
    void createRenderPass();

#ifdef ENABLE_VULKAN_DEBUG
    void setupDebugCallback();
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
#endif

    // -----------------------------------------------------------------
    //  Core Members
    // -----------------------------------------------------------------
    Vulkan::Context& context_;
    int width_, height_;

    // === ALL RAII-SAFE WITH PROPER DELETERS ===
    Dispose::VulkanHandle<VkPipeline>               rayTracingPipeline_;
    Dispose::VulkanHandle<VkPipelineLayout>         rayTracingPipelineLayout_;
    Dispose::VulkanHandle<VkPipeline>               computePipeline_;
    Dispose::VulkanHandle<VkPipelineLayout>         computePipelineLayout_;
    Dispose::VulkanHandle<VkPipeline>               graphicsPipeline_;
    Dispose::VulkanHandle<VkPipelineLayout>         graphicsPipelineLayout_;
    Dispose::VulkanHandle<VkRenderPass>             renderPass_;
    Dispose::VulkanHandle<VkPipelineCache>          pipelineCache_;

    Dispose::VulkanHandle<VkAccelerationStructureKHR> blas_;
    Dispose::VulkanHandle<VkAccelerationStructureKHR> tlas_;
    Dispose::VulkanHandle<VkBuffer>                   blasBuffer_;
    Dispose::VulkanHandle<VkBuffer>                   tlasBuffer_;
    Dispose::VulkanHandle<VkDeviceMemory>             blasMemory_;
    Dispose::VulkanHandle<VkDeviceMemory>             tlasMemory_;

    Dispose::VulkanHandle<VkDescriptorSetLayout> computeDescriptorSetLayout_;
    Dispose::VulkanHandle<VkDescriptorSetLayout> rayTracingDescriptorSetLayout_;
    Dispose::VulkanHandle<VkDescriptorSetLayout> graphicsDescriptorSetLayout_;

    ShaderBindingTable sbt_;
    std::vector<uint8_t> shaderHandles_;
    Dispose::VulkanHandle<VkBuffer>         sbtBuffer_;
    Dispose::VulkanHandle<VkDeviceMemory>   sbtMemory_;

    Dispose::VulkanHandle<VkCommandPool> transientPool_;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;
};

} // namespace VulkanRTX

#endif // VULKAN_PIPELINE_MANAGER_HPP