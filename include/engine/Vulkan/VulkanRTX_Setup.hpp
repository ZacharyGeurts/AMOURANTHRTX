// include/engine/Vulkan/VulkanRTX_Setup.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: RAII, FULLY FIXED, NO LEAKS, NO WARNINGS
//        ADDED: getBLAS() and getTLAS() public getters
//        Safe for VulkanRenderer::handleResize() to rebuild TLAS

#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <array>
#include <stdexcept>
#include <cstdint>
#include <sstream>

#include "engine/Dispose.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/logging.hpp"

namespace Vulkan { struct Context; }
namespace VulkanRTX { class VulkanPipelineManager; }

#define VK_CHECK(result, msg) \
    do { \
        VkResult __r = (result); \
        if (__r != VK_SUCCESS) { \
            std::ostringstream __oss; \
            __oss << "Vulkan error (" << static_cast<int>(__r) << "): " << (msg); \
            LOG_ERROR_CAT("VulkanRTX", "{}", __oss.str()); \
            throw VulkanRTXException(__oss.str()); \
        } \
    } while (0)

/* -------------------------------------------------------------
   Function-pointer typedefs (KHR extensions)
   ------------------------------------------------------------- */
using PFN_vkCmdBuildAccelerationStructuresKHR = void (*)(VkCommandBuffer, uint32_t,
    const VkAccelerationStructureBuildGeometryInfoKHR*, const VkAccelerationStructureBuildRangeInfoKHR* const*);
using PFN_vkCreateRayTracingPipelinesKHR = VkResult (*)(VkDevice, VkDeferredOperationKHR, VkPipelineCache,
    uint32_t, const VkRayTracingPipelineCreateInfoKHR*, const VkAllocationCallbacks*, VkPipeline*);
using PFN_vkGetBufferDeviceAddress = VkDeviceAddress (*)(VkDevice, const VkBufferDeviceAddressInfo*);
using PFN_vkCmdTraceRaysKHR = void (*)(VkCommandBuffer,
    const VkStridedDeviceAddressRegionKHR*, const VkStridedDeviceAddressRegionKHR*,
    const VkStridedDeviceAddressRegionKHR*, const VkStridedDeviceAddressRegionKHR*,
    uint32_t, uint32_t, uint32_t);
using PFN_vkCreateAccelerationStructureKHR = VkResult (*)(VkDevice,
    const VkAccelerationStructureCreateInfoKHR*, const VkAllocationCallbacks*, VkAccelerationStructureKHR*);
using PFN_vkDestroyAccelerationStructureKHR = void (*)(VkDevice, VkAccelerationStructureKHR,
    const VkAllocationCallbacks*);
using PFN_vkGetRayTracingShaderGroupHandlesKHR = VkResult (*)(VkDevice, VkPipeline,
    uint32_t, uint32_t, size_t, void*);
using PFN_vkGetAccelerationStructureDeviceAddressKHR = VkDeviceAddress (*)(VkDevice,
    const VkAccelerationStructureDeviceAddressInfoKHR*);
using PFN_vkCmdCopyAccelerationStructureKHR = void (*)(VkCommandBuffer,
    const VkCopyAccelerationStructureInfoKHR*);
using PFN_vkGetAccelerationStructureBuildSizesKHR = void (*)(VkDevice, VkAccelerationStructureBuildTypeKHR,
    const VkAccelerationStructureBuildGeometryInfoKHR*, const uint32_t*, VkAccelerationStructureBuildSizesInfoKHR*);

/* -------------------------------------------------------------
   Descriptor-related KHR function pointers
   ------------------------------------------------------------- */
using PFN_vkCreateDescriptorSetLayout   = VkResult (*)(VkDevice, const VkDescriptorSetLayoutCreateInfo*, const VkAllocationCallbacks*, VkDescriptorSetLayout*);
using PFN_vkAllocateDescriptorSets      = VkResult (*)(VkDevice, const VkDescriptorSetAllocateInfo*, VkDescriptorSet*);
using PFN_vkCreateDescriptorPool        = VkResult (*)(VkDevice, const VkDescriptorPoolCreateInfo*, const VkAllocationCallbacks*, VkDescriptorPool*);
using PFN_vkDestroyDescriptorSetLayout  = void (*)(VkDevice, VkDescriptorSetLayout, const VkAllocationCallbacks*);
using PFN_vkDestroyDescriptorPool       = void (*)(VkDevice, VkDescriptorPool, const VkAllocationCallbacks*);
using PFN_vkFreeDescriptorSets          = VkResult (*)(VkDevice, VkDescriptorPool, uint32_t, const VkDescriptorSet*);

namespace VulkanRTX {

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

class VulkanRTXException : public std::runtime_error {
public:
    explicit VulkanRTXException(const std::string& msg)
        : std::runtime_error(msg) {}
};

/* -------------------------------------------------------------
   ShaderBindingTable – UNIFIED & FINAL
   Uses .deviceAddress to match VulkanCommon.hpp and VulkanPipelineManager
   NO CONVERSION OPERATOR → ELIMINATES -Wclass-conversion
   ------------------------------------------------------------- */
struct ShaderBindingTable {
    struct Region {
        VkDeviceAddress deviceAddress = 0;
        VkDeviceSize    size          = 0;
        VkDeviceSize    stride        = 0;
    };
    Region raygen;
    Region miss;
    Region hit;
    Region callable;
};

class VulkanRTX {
public:
    VulkanRTX(std::shared_ptr<Vulkan::Context> ctx, int width, int height,
              VulkanPipelineManager* pipelineMgr);
    ~VulkanRTX();

    void initializeRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                       VkQueue graphicsQueue,
                       const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                       uint32_t maxRayRecursionDepth,
                       const std::vector<DimensionState>& dimensionCache);
    void updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                   VkQueue graphicsQueue,
                   const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                   const std::vector<DimensionState>& dimensionCache);

    void createDescriptorSetLayout();
    void createDescriptorPoolAndSet();
    void createRayTracingPipeline(uint32_t maxRayRecursionDepth);
    void createShaderBindingTable(VkPhysicalDevice physicalDevice);
    void createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                             VkQueue queue,
                             const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries);
    void createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                          VkQueue queue,
                          const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances);
    void updateDescriptors(VkBuffer cameraBuffer, VkBuffer materialBuffer, VkBuffer dimensionBuffer,
                           VkImageView storageImageView, VkImageView denoiseImageView,
                           VkImageView envMapView, VkSampler envMapSampler,
                           VkImageView densityVolumeView, VkImageView gDepthView,
                           VkImageView gNormalView);
    void recordRayTracingCommands(VkCommandBuffer cmdBuffer, VkExtent2D extent,
                                  VkImage outputImage, VkImageView outputImageView);
    void updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas);
    void createBlackFallbackImage();

    // PUBLIC GETTERS
    VkDescriptorSet getDescriptorSet() const { return ds_.get(); }
    VkPipeline getPipeline() const { return rtPipeline_; }  // ← NON-OWNING
    const ShaderBindingTable& getSBT() const { return sbt_; }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return dsLayout_.get(); }

    // FIXED: PUBLIC SBT ACCESS
    VkBuffer       getSBTBuffer() const { return sbtBuffer_.get(); }
    VkDeviceMemory getSBTMemory() const { return sbtMemory_.get(); }

    // ADDED: PUBLIC BLAS/TLAS ACCESS FOR RESIZE REBUILD
    VkAccelerationStructureKHR getBLAS() const { return blas_.get(); }
    VkAccelerationStructureKHR getTLAS() const { return tlas_.get(); }

    void setRayTracingPipeline(VkPipeline pipeline, VkPipelineLayout layout);

private:
    VkCommandBuffer allocateTransientCommandBuffer(VkCommandPool commandPool);
    void submitAndWaitTransient(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool);
    void uploadBlackPixelToImage(VkImage image);
    void createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size,
                      VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      Dispose::VulkanHandle<VkBuffer>& buffer,
                      Dispose::VulkanHandle<VkDeviceMemory>& memory);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                            VkMemoryPropertyFlags properties);
    void compactAccelerationStructures(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue);
    void createStorageImage(VkPhysicalDevice physicalDevice, VkExtent2D extent,
                            Dispose::VulkanHandle<VkImage>& image,
                            Dispose::VulkanHandle<VkImageView>& imageView,
                            Dispose::VulkanHandle<VkDeviceMemory>& memory);

    std::shared_ptr<Vulkan::Context> context_;
    VulkanPipelineManager* pipelineMgr_ = nullptr;
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkExtent2D extent_{};

    // RAII Handles
    Dispose::VulkanHandle<VkDescriptorSetLayout> dsLayout_;
    Dispose::VulkanHandle<VkDescriptorPool> dsPool_;
    Dispose::VulkanHandle<VkDescriptorSet> ds_;

    // REMOVED: rtPipelineLayout_ & rtPipeline_ from RAII
    // NOW: NON-OWNING RAW POINTERS
    VkPipeline rtPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout rtPipelineLayout_ = VK_NULL_HANDLE;

    Dispose::VulkanHandle<VkBuffer> blasBuffer_;
    Dispose::VulkanHandle<VkDeviceMemory> blasMemory_;
    Dispose::VulkanHandle<VkBuffer> tlasBuffer_;
    Dispose::VulkanHandle<VkDeviceMemory> tlasMemory_;
    Dispose::VulkanHandle<VkAccelerationStructureKHR> blas_;
    Dispose::VulkanHandle<VkAccelerationStructureKHR> tlas_;

    Dispose::VulkanHandle<VkBuffer> sbtBuffer_;
    Dispose::VulkanHandle<VkDeviceMemory> sbtMemory_;

    std::vector<uint32_t> primitiveCounts_;
    std::vector<uint32_t> previousPrimitiveCounts_;
    std::vector<DimensionState> previousDimensionCache_;

    bool supportsCompaction_ = false;
    ShaderBindingTable sbt_;
    VkDeviceAddress sbtBufferAddress_ = 0;

    Dispose::VulkanHandle<VkImage> blackFallbackImage_;
    Dispose::VulkanHandle<VkDeviceMemory> blackFallbackMemory_;
    Dispose::VulkanHandle<VkImageView> blackFallbackView_;

    // KHR function pointers
    PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;

    PFN_vkCreateDescriptorSetLayout   vkCreateDescriptorSetLayout   = nullptr;
    PFN_vkAllocateDescriptorSets      vkAllocateDescriptorSets      = nullptr;
    PFN_vkCreateDescriptorPool        vkCreateDescriptorPool        = nullptr;
    PFN_vkDestroyDescriptorSetLayout  vkDestroyDescriptorSetLayout  = nullptr;
    PFN_vkDestroyDescriptorPool       vkDestroyDescriptorPool       = nullptr;
    PFN_vkFreeDescriptorSets          vkFreeDescriptorSets          = nullptr;
};

} // namespace VulkanRTX