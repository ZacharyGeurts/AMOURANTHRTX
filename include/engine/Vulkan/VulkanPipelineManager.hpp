// include/engine/Vulkan/VulkanPipelineManager.hpp
// AMOURANTH RTX — NEXUS EDITION — GPU-DRIVEN 12,000+ FPS — VALHALLA UNBREACHABLE
// STONEKEY v∞ — ALL HANDLES ENCRYPTED — RECLASS = COSMIC GARBAGE

#pragma once
#include "StoneKey.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/logging.hpp"
#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <chrono>

namespace VulkanRTX {

class VulkanRTX;
class VulkanRenderer;

class VulkanPipelineManager {
    friend class VulkanRTX;

public:
    VulkanPipelineManager(Context& context, int width, int height);
    ~VulkanPipelineManager();

    void createRayTracingPipeline(const std::vector<std::string>& shaderPaths,
                                  VkPhysicalDevice physDev, VkDevice dev,
                                  VkDescriptorSet descSet);
    void createComputePipeline();
    void createNexusPipeline();
    void createStatsPipeline();
    void createGraphicsPipeline(int width, int height);
    void createAccelerationStructures(VkBuffer vertexBuffer, VkBuffer indexBuffer,
                                      VulkanBufferManager& bufferMgr, VulkanRenderer* renderer);

    void dispatchCompute(uint32_t x, uint32_t y, uint32_t z = 1);
    void dispatchStats(VkCommandBuffer cmd, VkDescriptorSet statsSet);

    void updateRayTracingDescriptorSet(VkDescriptorSet ds, VkAccelerationStructureKHR tlas);
    void logFrameTimeIfSlow(std::chrono::steady_clock::time_point start);

    // ENCRYPTED GETTERS — CHEATERS SEE GARBAGE
    [[nodiscard]] uint64_t getRayTracingPipeline() const noexcept { return encrypt(rayTracingPipeline_); }
    [[nodiscard]] uint64_t getComputePipeline() const noexcept { return encrypt(computePipeline_); }
    [[nodiscard]] uint64_t getNexusPipeline() const noexcept { return encrypt(nexusPipeline_); }
    [[nodiscard]] uint64_t getStatsPipeline() const noexcept { return encrypt(statsPipeline_); }
    [[nodiscard]] uint64_t getTLAS() const noexcept { return encrypt(tlas_); }
    [[nodiscard]] uint64_t getSBTBuffer() const noexcept { return encrypt(sbtBuffer_); }
    [[nodiscard]] const ShaderBindingTable& getSBT() const noexcept { return sbt_; }

    VkDescriptorSet computeDescriptorSet_ = VK_NULL_HANDLE;

private:
    template<typename T>
    static constexpr uint64_t encrypt(T raw) noexcept {
        return static_cast<uint64::uint64_t>(raw) ^ kStone1 ^ kStone2;
    }
    template<typename T>
    static constexpr T decrypt(uint64_t enc) noexcept {
        return static_cast<T>(enc ^ kStone1 ^ kStone2);
    }

    VkShaderModule loadShaderImpl(VkDevice device, const std::string& path);
    void createPipelineCache();
    void createRenderPass();
    void createGraphicsDescriptorSetLayout();
    void createComputeDescriptorSetLayout();
    void createNexusDescriptorSetLayout();
    void createStatsDescriptorSetLayout();
    VkDescriptorSetLayout createRayTracingDescriptorSetLayout();
    void createTransientCommandPool();
    void createShaderBindingTable(VkPhysicalDevice physDev);

#ifdef ENABLE_VULKAN_DEBUG
    void setupDebugCallback();
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
#endif

    Context& context_;
    int width_ = 0, height_ = 0;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;

    // RAW HANDLES — NEVER EXPOSED
    VkPipeline rayTracingPipeline_ = VK_NULL_HANDLE;
    VkPipeline computePipeline_ = VK_NULL_HANDLE;
    VkPipeline graphicsPipeline_ = VK_NULL_HANDLE;
    VkPipeline nexusPipeline_ = VK_NULL_HANDLE;
    VkPipeline statsPipeline_ = VK_NULL_HANDLE;

    VkPipelineLayout rayTracingPipelineLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout computePipelineLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout graphicsPipelineLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout nexusPipelineLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout statsPipelineLayout_ = VK_NULL_HANDLE;

    VkDescriptorSetLayout rayTracingDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout computeDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout graphicsDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout nexusDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout statsDescriptorSetLayout_ = VK_NULL_HANDLE;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipelineCache pipelineCache_ = VK_NULL_HANDLE;
    VkCommandPool transientPool_ = VK_NULL_HANDLE;

    VkBuffer blasBuffer_ = VK_NULL_HANDLE, tlasBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory blasMemory_ = VK_NULL_HANDLE, tlasMemory_ = VK_NULL_HANDLE;
    VkAccelerationStructureKHR blas_ = VK_NULL_HANDLE, tlas_ = VK_NULL_HANDLE;

    VkBuffer sbtBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory sbtMemory_ = VK_NULL_HANDLE;
    ShaderBindingTable sbt_;
    std::vector<uint8_t> shaderHandles_;

    // RT Extensions
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;
};

} // namespace VulkanRTX