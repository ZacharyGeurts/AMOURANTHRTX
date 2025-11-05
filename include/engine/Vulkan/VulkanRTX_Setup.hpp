// include/engine/Vulkan/VulkanRTX_Setup.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: DECLARATIONS ONLY – NO DUPLICATES, NO PIPELINE LOGIC
//        VulkanRTX class → owns RT state, uses VulkanPipelineManager
//        All implementation → VulkanRTX_Setup.cpp

#pragma once

#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/core.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <stdexcept>

namespace VulkanRTX {

// Forward declarations
class VulkanPipelineManager;
class VulkanRenderer;

// VulkanRTXException – shared
class VulkanRTXException : public std::runtime_error {
public:
    explicit VulkanRTXException(const std::string& msg)
        : std::runtime_error(msg), m_line(0) {}
    VulkanRTXException(const std::string& msg, const char* file, int line, const char* func)
        : std::runtime_error(msg), m_file(file), m_line(line), m_function(func) {}

    [[nodiscard]] const char* file() const noexcept { return m_file.c_str(); }
    [[nodiscard]] int line() const noexcept { return m_line; }
    [[nodiscard]] const char* function() const noexcept { return m_function.c_str(); }

private:
    std::string m_file, m_function;
    int m_line = 0;
};

// DescriptorBindings – shared
enum class DescriptorBindings : uint32_t {
    TLAS               = 0,
    StorageImage       = 1,
    CameraUBO          = 2,
    MaterialSSBO       = 3,
    DimensionDataSSBO  = 4,
    DenoiseImage       = 5,
    EnvMap             = 6,
    DensityVolume      = 7,
    GDepth             = 8,
    GNormal            = 9,
    AlphaTex           = 10
};

// VulkanRTX – DECLARATION ONLY
class VulkanRTX {
public:
    // -----------------------------------------------------------------------
    //  CTOR: Takes ownership of pipeline manager (moved)
    // -----------------------------------------------------------------------
    VulkanRTX(std::shared_ptr<::Vulkan::Context> ctx,
          int width, int height,
          VulkanPipelineManager* pipelineMgr);

    ~VulkanRTX();

    // -----------------------------------------------------------------------
    //  INITIALIZATION
    // -----------------------------------------------------------------------
    void initializeRTX(VkPhysicalDevice physicalDevice,
                       VkCommandPool commandPool,
                       VkQueue graphicsQueue,
                       const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                       uint32_t maxRayRecursionDepth,
                       const std::vector<DimensionState>& dimensionCache);

    // -----------------------------------------------------------------------
    //  UPDATE (REBUILD AS + SBT)
    // -----------------------------------------------------------------------
    void updateRTX(VkPhysicalDevice physicalDevice,
                   VkCommandPool commandPool,
                   VkQueue graphicsQueue,
                   const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                   const std::vector<DimensionState>& dimensionCache);

    void updateRTX(VkPhysicalDevice physicalDevice,
                   VkCommandPool commandPool,
                   VkQueue graphicsQueue,
                   const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                   const std::vector<DimensionState>& dimensionCache,
                   uint32_t transferQueueFamily);

    void updateRTX(VkPhysicalDevice physicalDevice,
                   VkCommandPool commandPool,
                   VkQueue graphicsQueue,
                   const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                   const std::vector<DimensionState>& dimensionCache,
                   VulkanRenderer* renderer);

    // -----------------------------------------------------------------------
    //  DESCRIPTOR SYSTEM
    // -----------------------------------------------------------------------
    void createDescriptorPoolAndSet();

    // -----------------------------------------------------------------------
    //  ACCELERATION STRUCTURES
    // -----------------------------------------------------------------------
    void createShaderBindingTable(VkPhysicalDevice physicalDevice);

    void createBottomLevelAS(VkPhysicalDevice physicalDevice,
                             VkCommandPool commandPool,
                             VkQueue queue,
                             const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                             uint32_t transferQueueFamily = VK_QUEUE_FAMILY_IGNORED);

    void createTopLevelAS(VkPhysicalDevice physicalDevice,
                          VkCommandPool commandPool,
                          VkQueue queue,
                          const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances);

    // -----------------------------------------------------------------------
    //  DESCRIPTOR UPDATES
    // -----------------------------------------------------------------------
    void updateDescriptors(VkBuffer cameraBuffer,
                           VkBuffer materialBuffer,
                           VkBuffer dimensionBuffer,
                           VkImageView storageImageView,
                           VkImageView denoiseImageView,
                           VkImageView envMapView,
                           VkSampler envMapSampler,
                           VkImageView densityVolumeView,
                           VkImageView gDepthView,
                           VkImageView gNormalView);

    // -----------------------------------------------------------------------
    //  RECORDING
    // -----------------------------------------------------------------------
    void recordRayTracingCommands(VkCommandBuffer cmdBuffer,
                                  VkExtent2D extent,
                                  VkImage outputImage,
                                  VkImageView outputImageView);

    void createBlackFallbackImage();
    void notifyTLASReady(VkAccelerationStructureKHR tlas, VulkanRenderer* renderer);

    // -----------------------------------------------------------------------
    //  RAY TRACING DISPATCH
    // -----------------------------------------------------------------------
    void traceRays(VkCommandBuffer cmd,
                   const VkStridedDeviceAddressRegionKHR* raygen,
                   const VkStridedDeviceAddressRegionKHR* miss,
                   const VkStridedDeviceAddressRegionKHR* hit,
                   const VkStridedDeviceAddressRegionKHR* callable,
                   uint32_t width, uint32_t height, uint32_t depth) const;

    // -----------------------------------------------------------------------
    //  GETTERS
    // -----------------------------------------------------------------------
    [[nodiscard]] VkDescriptorSet               getDescriptorSet() const noexcept { return ds_; }
    [[nodiscard]] VkPipeline                    getPipeline() const noexcept { return rtPipeline_; }
    [[nodiscard]] const ShaderBindingTable&     getSBT() const noexcept { return sbt_; }
    [[nodiscard]] VkDescriptorSetLayout         getDescriptorSetLayout() const noexcept { return dsLayout_; }
    [[nodiscard]] VkBuffer                      getSBTBuffer() const noexcept { return sbtBuffer_; }
    [[nodiscard]] VkDeviceMemory                getSBTMemory() const noexcept { return sbtMemory_; }
    [[nodiscard]] VkAccelerationStructureKHR    getBLAS() const noexcept { return blas_; }
    [[nodiscard]] VkAccelerationStructureKHR    getTLAS() const noexcept { return tlas_; }

    // -----------------------------------------------------------------------
    //  PIPELINE SETTER (called from VulkanRenderer after pipeline creation)
    // -----------------------------------------------------------------------
    void setRayTracingPipeline(VkPipeline pipeline, VkPipelineLayout layout) noexcept {
        rtPipeline_ = pipeline;
        rtPipelineLayout_ = layout;
    }

private:
    // -----------------------------------------------------------------------
    //  HELPER METHODS
    // -----------------------------------------------------------------------
    [[nodiscard]] VkCommandBuffer allocateTransientCommandBuffer(VkCommandPool commandPool);
    void submitAndWaitTransient(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool);
    void uploadBlackPixelToImage(VkImage image);
    void createBuffer(VkPhysicalDevice physicalDevice,
                      VkDeviceSize size,
                      VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties,
                      VkBuffer& buffer,
                      VkDeviceMemory& memory);
    [[nodiscard]] uint32_t findMemoryType(VkPhysicalDevice physicalDevice,
                                          uint32_t typeFilter,
                                          VkMemoryPropertyFlags properties);
    void cleanupBLASResources(VkBuffer asBuffer, VkDeviceMemory asMemory,
                              VkBuffer scratchBuffer, VkDeviceMemory scratchMemory);

    // -----------------------------------------------------------------------
    //  MEMBERS
    // -----------------------------------------------------------------------
    std::shared_ptr<::Vulkan::Context> context_;
    std::unique_ptr<VulkanPipelineManager> pipelineMgr_;  // OWNED
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkExtent2D extent_{};

    VkDescriptorSetLayout dsLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool dsPool_ = VK_NULL_HANDLE;
    VkDescriptorSet ds_ = VK_NULL_HANDLE;

    VkPipeline rtPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout rtPipelineLayout_ = VK_NULL_HANDLE;

    VkBuffer blasBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory blasMemory_ = VK_NULL_HANDLE;
    VkBuffer tlasBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory tlasMemory_ = VK_NULL_HANDLE;
    VkAccelerationStructureKHR blas_ = VK_NULL_HANDLE;
    VkAccelerationStructureKHR tlas_ = VK_NULL_HANDLE;

    VkBuffer sbtBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory sbtMemory_ = VK_NULL_HANDLE;

    std::vector<uint32_t> primitiveCounts_;
    std::vector<uint32_t> previousPrimitiveCounts_;
    std::vector<DimensionState> previousDimensionCache_;

    bool supportsCompaction_ = false;
    ShaderBindingTable sbt_;
    VkDeviceAddress sbtBufferAddress_ = 0;

    VkImage blackFallbackImage_ = VK_NULL_HANDLE;
    VkDeviceMemory blackFallbackMemory_ = VK_NULL_HANDLE;
    VkImageView blackFallbackView_ = VK_NULL_HANDLE;

    // -----------------------------------------------------------------------
    //  FUNCTION POINTERS
    // -----------------------------------------------------------------------
    PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;

    PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout = nullptr;
    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets = nullptr;
    PFN_vkCreateDescriptorPool vkCreateDescriptorPool = nullptr;
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout = nullptr;
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool = nullptr;
    PFN_vkFreeDescriptorSets vkFreeDescriptorSets = nullptr;

    VkFence transientFence_ = VK_NULL_HANDLE;
    bool deviceLost_ = false;
};

} // namespace VulkanRTX