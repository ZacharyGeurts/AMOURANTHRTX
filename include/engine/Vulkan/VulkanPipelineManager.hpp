// include/engine/Vulkan/VulkanPipelineManager.hpp
// AMOURANTH RTX — Vulkan Pipeline Manager
// FULLY MODULAR. FULLY SCALABLE. FULLY GLOWING.
// NEXUS EDITION: GPU-Driven Adaptive RT Decision | 12,000+ FPS
// FINAL: dispatchCompute() + computeDescriptorSet_ + NEXUS PIPELINE + STATS ANALYZER + FRIEND VulkanRTX
// ADDED: StatsPipeline for image metrics (variance/entropy/grad)
//       dispatchStats() for post-RT analysis
// UPDATED: createAccelerationStructures() takes VulkanRenderer* for TLAS notifyTLASReady()

#pragma once

#include "engine/Vulkan/Vulkan_init.hpp"  // Assuming VulkanCore.hpp → Vulkan_init.hpp
#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/Vulkan/VulkanBufferManager.hpp"
#include "engine/logging.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <chrono>

namespace VulkanRTX {

class VulkanRTX;  // FORWARD DECLARATION
class VulkanRenderer;  // ← ADDED: FOR NOTIFY

class VulkanPipelineManager {
    friend class VulkanRTX;  // ALLOWS VulkanRTX to call private layout creation

public:
    VulkanPipelineManager(Vulkan::Context& context, int width, int height);
    ~VulkanPipelineManager();

    // RAY TRACING PIPELINE
    void createRayTracingPipeline(uint32_t maxRayRecursionDepth = 1);
    [[nodiscard]] VkPipeline getRayTracingPipeline() const noexcept { return rayTracingPipeline_; }
    [[nodiscard]] VkPipelineLayout getRayTracingPipelineLayout() const noexcept { return rayTracingPipelineLayout_; }
    [[nodiscard]] VkDescriptorSetLayout getRayTracingDescriptorSetLayout() const noexcept { return rayTracingDescriptorSetLayout_; }

    // COMPUTE PIPELINE (generic)
    void createComputePipeline();
    [[nodiscard]] VkPipeline getComputePipeline() const noexcept { return computePipeline_; }
    [[nodiscard]] VkPipelineLayout getComputePipelineLayout() const noexcept { return computePipelineLayout_; }
    [[nodiscard]] VkDescriptorSetLayout getComputeDescriptorSetLayout() const noexcept { return computeDescriptorSetLayout_; }

    // NEXUS GPU DECISION PIPELINE (1x1 dispatch)
    void createNexusPipeline();
    [[nodiscard]] VkPipeline getNexusPipeline() const noexcept { return nexusPipeline_; }
    [[nodiscard]] VkPipelineLayout getNexusPipelineLayout() const noexcept { return nexusPipelineLayout_; }
    [[nodiscard]] VkDescriptorSetLayout getNexusDescriptorSetLayout() const noexcept { return nexusDescriptorSetLayout_; }

    // STATS ANALYZER PIPELINE (for Nexus metrics from prev output)
    void createStatsPipeline();
    void dispatchStats(VkCommandBuffer cmd, VkDescriptorSet statsSet);
    [[nodiscard]] VkPipeline getStatsPipeline() const noexcept { return statsPipeline_; }
    [[nodiscard]] VkPipelineLayout getStatsPipelineLayout() const noexcept { return statsPipelineLayout_; }
    [[nodiscard]] VkDescriptorSetLayout getStatsDescriptorSetLayout() const noexcept { return statsDescriptorSetLayout_; }

    // EPIC COMPUTE DISPATCH — SLI / CLUSTERS / MULTI-GPU
    void dispatchCompute(uint32_t x, uint32_t y, uint32_t z = 1);

    // GRAPHICS PIPELINE
    void createGraphicsPipeline(int width, int height);
    [[nodiscard]] VkPipeline getGraphicsPipeline() const noexcept { return graphicsPipeline_; }
    [[nodiscard]] VkPipelineLayout getGraphicsPipelineLayout() const noexcept { return graphicsPipelineLayout_; }
    [[nodiscard]] VkDescriptorSetLayout getGraphicsDescriptorSetLayout() const noexcept { return graphicsDescriptorSetLayout_; }

    // ACCELERATION STRUCTURES — NOW TAKES RENDERER FOR NOTIFY
    void createAccelerationStructures(VkBuffer vertexBuffer,
                                      VkBuffer indexBuffer,
                                      VulkanBufferManager& bufferMgr,
                                      VulkanRenderer* renderer);  // ← ADDED: renderer

    [[nodiscard]] VkAccelerationStructureKHR getBLAS() const noexcept { return blas_; }
    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept { return tlas_; }

    // SHADER BINDING TABLE
    void createShaderBindingTable(VkPhysicalDevice physDev);
    [[nodiscard]] const ShaderBindingTable& getSBT() const noexcept { return sbt_; }
    [[nodiscard]] VkBuffer getSBTBuffer() const noexcept { return sbtBuffer_; }
    [[nodiscard]] VkDeviceMemory getSBTMemory() const noexcept { return sbtMemory_; }

    void updateRayTracingDescriptorSet(VkDescriptorSet ds, VkAccelerationStructureKHR tlas);
    void logFrameTimeIfSlow(std::chrono::steady_clock::time_point start);

    // Compute descriptor set (for dispatch)
    VkDescriptorSet computeDescriptorSet_ = VK_NULL_HANDLE;

private:
    VkShaderModule loadShaderImpl(VkDevice device, const std::string& shaderType);
    void createPipelineCache();
    void createRenderPass();
    void createGraphicsDescriptorSetLayout();
    void createComputeDescriptorSetLayout();
    void createNexusDescriptorSetLayout();  // NEW: For 1x1 score image
    void createStatsDescriptorSetLayout();  // NEW: For stats analysis
    VkDescriptorSetLayout createRayTracingDescriptorSetLayout();  // PRIVATE BUT FRIEND-ACCESSIBLE
    void createTransientCommandPool();

#ifdef ENABLE_VULKAN_DEBUG
    void setupDebugCallback();
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
#endif

    Vulkan::Context& context_;
    int width_ = 0;
    int height_ = 0;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;

    // PIPELINES
    VkPipeline rayTracingPipeline_ = VK_NULL_HANDLE;
    VkPipeline computePipeline_ = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline_ = VK_NULL_HANDLE;
    VkPipeline nexusPipeline_ = VK_NULL_HANDLE;  // NEW: GPU Nexus Decision
    VkPipeline statsPipeline_ = VK_NULL_HANDLE;  // NEW: Image stats analyzer

    // PIPELINE LAYOUTS
    VkPipelineLayout rayTracingPipelineLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout computePipelineLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout graphicsPipelineLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout nexusPipelineLayout_ = VK_NULL_HANDLE;  // NEW
    VkPipelineLayout statsPipelineLayout_ = VK_NULL_HANDLE;  // NEW

    // DESCRIPTOR SET LAYOUTS
    VkDescriptorSetLayout rayTracingDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout computeDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout graphicsDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout nexusDescriptorSetLayout_ = VK_NULL_HANDLE;  // NEW
    VkDescriptorSetLayout statsDescriptorSetLayout_ = VK_NULL_HANDLE;  // NEW

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipelineCache pipelineCache_ = VK_NULL_HANDLE;
    VkCommandPool transientPool_ = VK_NULL_HANDLE;

    // ACCELERATION STRUCTURES
    VkBuffer blasBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory blasMemory_ = VK_NULL_HANDLE;
    VkBuffer tlasBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory tlasMemory_ = VK_NULL_HANDLE;
    VkAccelerationStructureKHR blas_ = VK_NULL_HANDLE;
    VkAccelerationStructureKHR tlas_ = VK_NULL_HANDLE;

    // SHADER BINDING TABLE
    VkBuffer sbtBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory sbtMemory_ = VK_NULL_HANDLE;
    ShaderBindingTable sbt_;
    std::vector<uint8_t> shaderHandles_;

    // RAY TRACING EXTENSION FUNCTIONS (loaded in ctor if needed)
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;

    // FRIEND ACCESS TO VulkanRTX
    std::unique_ptr<VulkanRTX> rtx_;  // ← NOW OWNS RTX (optional, or keep in renderer)
};

} // namespace VulkanRTX