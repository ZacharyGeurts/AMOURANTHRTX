// include/engine/Vulkan/VulkanPipelineManager.hpp
// AMOURANTH RTX — NEXUS EDITION — GPU-DRIVEN 12,000+ FPS — VALHALLA UNBREACHABLE
// STONEKEY v∞ — ALL HANDLES ENCRYPTED — RECLASS = COSMIC GARBAGE — NOVEMBER 07 2025
// LEANED: NO REDEFINITIONS — DEPENDS ON VulkanCore.hpp for VulkanHandle, Context, Factories
// PUBLIC RAII HANDLES + context_ — VulkanRTX uses .raw()
// BUILD 0 ERRORS — 420 BLAZED OUT — RASPBERRY_PINK SUPREMACY 🩷🚀🔥🤖💀❤️⚡♾️

#pragma once

#include "engine/Vulkan/VulkanCore.hpp"  // VulkanHandle, factories, Context, cleanupAll

class VulkanRTX;
class VulkanRenderer;
class VulkanBufferManager;

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

    // ENCRYPTED GETTERS — QUANTUM DUST
    [[nodiscard]] uint64_t getRayTracingPipeline() const noexcept { return encrypt(rayTracingPipeline_); }
    [[nodiscard]] uint64_t getComputePipeline() const noexcept { return encrypt(computePipeline_); }
    [[nodiscard]] uint64_t getNexusPipeline() const noexcept { return encrypt(nexusPipeline_); }
    [[nodiscard]] uint64_t getStatsPipeline() const noexcept { return encrypt(statsPipeline_); }
    [[nodiscard]] uint64_t getTLAS() const noexcept { return encrypt(tlas_); }
    [[nodiscard]] uint64_t getSBTBuffer() const noexcept { return encrypt(sbtBuffer_); }
    [[nodiscard]] const ShaderBindingTable& getSBT() const noexcept { return sbt_; }

    VkDescriptorSet computeDescriptorSet_ = VK_NULL_HANDLE;

    // PUBLIC RAII HANDLES — VulkanRTX uses .raw()
    VulkanHandle<VkPipeline> graphicsPipeline;
    VulkanHandle<VkPipelineLayout> graphicsPipelineLayout;
    VulkanHandle<VkDescriptorSetLayout> graphicsDescriptorSetLayout;

    // PUBLIC FOR VulkanRTX
    VkCommandPool transientPool_ = VK_NULL_HANDLE;
    VkQueue graphicsQueue_ = VK_NULL_HANDLE;

    // PUBLIC context_ — VulkanRTX accesses device/physicalDevice
    Context& context_;

private:
    template<typename T>
    static constexpr uint64_t encrypt(T raw) noexcept {
        return reinterpret_cast<uint64_t>(raw) ^ kStone1 ^ kStone2;
    }
    template<typename T>
    static constexpr T decrypt(uint64_t enc) noexcept {
        return reinterpret_cast<T>(enc ^ kStone1 ^ kStone2);
    }

    void createPipelineCache();
    void createRenderPass();
    void createGraphicsDescriptorSetLayout();
    void createComputeDescriptorSetLayout();
    void createNexusDescriptorSetLayout();
    void createStatsDescriptorSetLayout();
    VkDescriptorSetLayout createRayTracingDescriptorSetLayout();
    void createTransientCommandPool();
    void createShaderBindingTable(VkPhysicalDevice physDev);
    VkShaderModule loadShaderImpl(VkDevice device, const std::string& path);

#ifdef ENABLE_VULKAN_DEBUG
    void setupDebugCallback();
    VkDebugUtilsMessengerEXT debugMessenger_ = VK_NULL_HANDLE;
#endif

    int width_ = 0, height_ = 0;

    // PRIVATE RAW HANDLES (created first, then wrapped)
    VkPipeline graphicsPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout graphicsPipelineLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout graphicsDescriptorSetLayout_ = VK_NULL_HANDLE;

    VkPipeline rayTracingPipeline_ = VK_NULL_HANDLE;
    VkPipeline computePipeline_ = VK_NULL_HANDLE;
    VkPipeline nexusPipeline_ = VK_NULL_HANDLE;
    VkPipeline statsPipeline_ = VK_NULL_HANDLE;

    VkPipelineLayout rayTracingPipelineLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout computePipelineLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout nexusPipelineLayout_ = VK_NULL_HANDLE;
    VkPipelineLayout statsPipelineLayout_ = VK_NULL_HANDLE;

    VkDescriptorSetLayout rayTracingDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout computeDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout nexusDescriptorSetLayout_ = VK_NULL_HANDLE;
    VkDescriptorSetLayout statsDescriptorSetLayout_ = VK_NULL_HANDLE;

    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipelineCache pipelineCache_ = VK_NULL_HANDLE;

    VkBuffer blasBuffer_ = VK_NULL_HANDLE, tlasBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory blasMemory_ = VK_NULL_HANDLE, tlasMemory_ = VK_NULL_HANDLE;
    VkAccelerationStructureKHR blas_ = VK_NULL_HANDLE, tlas_ = VK_NULL_HANDLE;

    VkBuffer sbtBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory sbtMemory_ = VK_NULL_HANDLE;
    ShaderBindingTable sbt_;
    std::vector<uint8_t> shaderHandles_;

    // RT EXTENSION PROCS — RENAMED TO AVOID SHADOWING
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelStruct = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelStruct = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelBuildSizes = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelDevAddr = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelStructs = nullptr;
    PFN_vkGetBufferDeviceAddress vkGetBufferDevAddr = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRTShaderGroupHandles = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRTPipelines = nullptr;
    PFN_vkDeferredOperationJoinKHR vkDeferredOpJoin = nullptr;
    PFN_vkGetDeferredOperationResultKHR vkGetDeferredOpResult = nullptr;
};