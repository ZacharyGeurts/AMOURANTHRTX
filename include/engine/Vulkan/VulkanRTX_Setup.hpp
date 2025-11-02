// include/engine/Vulkan/VulkanRTX_Setup.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts
// ONLY DECLARATIONS – NO DEFINITIONS

#pragma once

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <memory>
#include <vector>
#include <array>
#include <stdexcept>
#include <cstdint>
#include <sstream>

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/logging.hpp"
#include "engine/Dispose.hpp"

namespace Vulkan { struct Context; }
namespace VulkanRTX { class VulkanPipelineManager; }

// ---------------------------------------------------------------------
// VK_CHECK – 2-argument version: VK_CHECK(vkCall(...), "description")
// ---------------------------------------------------------------------
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
using PFN_vkGetAccelerationStructureBuildSizesKHR = void (*)(VkDevice,
    VkAccelerationStructureBuildTypeKHR, const VkAccelerationStructureBuildGeometryInfoKHR*,
    const uint32_t*, VkAccelerationStructureBuildSizesInfoKHR*);
using PFN_vkGetRayTracingShaderGroupHandlesKHR = VkResult (*)(VkDevice, VkPipeline,
    uint32_t, uint32_t, size_t, void*);
using PFN_vkGetAccelerationStructureDeviceAddressKHR = VkDeviceAddress (*)(VkDevice,
    const VkAccelerationStructureDeviceAddressInfoKHR*);
using PFN_vkCmdCopyAccelerationStructureKHR = void (*)(VkCommandBuffer,
    const VkCopyAccelerationStructureInfoKHR*);
using PFN_vkCreateDescriptorSetLayout = VkResult (*)(VkDevice, const VkDescriptorSetLayoutCreateInfo*,
    const VkAllocationCallbacks*, VkDescriptorSetLayout*);
using PFN_vkAllocateDescriptorSets = VkResult (*)(VkDevice, const VkDescriptorSetAllocateInfo*,
    VkDescriptorSet*);
using PFN_vkCreateDescriptorPool = VkResult (*)(VkDevice, const VkDescriptorPoolCreateInfo*,
    const VkAllocationCallbacks*, VkDescriptorPool*);
using PFN_vkDestroyDescriptorSetLayout = void (*)(VkDevice, VkDescriptorSetLayout,
    const VkAllocationCallbacks*);
using PFN_vkDestroyDescriptorPool = void (*)(VkDevice, VkDescriptorPool,
    const VkAllocationCallbacks*);
using PFN_vkFreeDescriptorSets = VkResult (*)(VkDevice, VkDescriptorPool, uint32_t,
    const VkDescriptorSet*);
using PFN_vkDestroyPipelineLayout = void (*)(VkDevice, VkPipelineLayout,
    const VkAllocationCallbacks*);
using PFN_vkDestroyPipeline = void (*)(VkDevice, VkPipeline,
    const VkAllocationCallbacks*);
using PFN_vkDestroyBuffer = void (*)(VkDevice, VkBuffer,
    const VkAllocationCallbacks*);
using PFN_vkFreeMemory = void (*)(VkDevice, VkDeviceMemory,
    const VkAllocationCallbacks*);
using PFN_vkCreateBuffer = VkResult (*)(VkDevice, const VkBufferCreateInfo*,
    const VkAllocationCallbacks*, VkBuffer*);
using PFN_vkAllocateMemory = VkResult (*)(VkDevice, const VkMemoryAllocateInfo*,
    const VkAllocationCallbacks*, VkDeviceMemory*);
using PFN_vkBindBufferMemory = VkResult (*)(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize);
using PFN_vkCreateQueryPool = VkResult (*)(VkDevice, const VkQueryPoolCreateInfo*, const VkAllocationCallbacks*, VkQueryPool*);
using PFN_vkDestroyQueryPool = void (*)(VkDevice, VkQueryPool, const VkAllocationCallbacks*);
using PFN_vkGetQueryPoolResults = VkResult (*)(VkDevice, VkQueryPool, uint32_t, uint32_t, size_t, void*, VkDeviceSize, VkQueryResultFlags);
using PFN_vkCmdWriteAccelerationStructuresPropertiesKHR = void (*)(VkCommandBuffer, uint32_t, const VkAccelerationStructureKHR*, VkQueryType, VkQueryPool, uint32_t);
using PFN_vkDestroyImage = void (*)(VkDevice, VkImage, const VkAllocationCallbacks*);
using PFN_vkDestroyImageView = void (*)(VkDevice, VkImageView, const VkAllocationCallbacks*);

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

// ---------------------------------------------------------------------
// VulkanRTXException – used by VK_CHECK
// ---------------------------------------------------------------------
class VulkanRTXException : public std::runtime_error {
public:
    explicit VulkanRTXException(const std::string& msg)
        : std::runtime_error(msg) {}
};

// ---------------------------------------------------------------------
// RAII wrappers
// ---------------------------------------------------------------------
template<typename T, typename DestroyFuncType>
class VulkanResource {
public:
    VulkanResource() noexcept = default;
    VulkanResource(VkDevice device, T resource, DestroyFuncType destroyFunc);
    ~VulkanResource();
    VulkanResource(const VulkanResource&) = delete;
    VulkanResource& operator=(const VulkanResource&) = delete;
    VulkanResource(VulkanResource&& o) noexcept;
    VulkanResource& operator=(VulkanResource&& o) noexcept;
    T get() const { return resource_; }
    void swap(VulkanResource& other) noexcept;
private:
    VkDevice device_ = VK_NULL_HANDLE;
    T resource_ = VK_NULL_HANDLE;
    DestroyFuncType destroyFunc_ = nullptr;
};

class VulkanDescriptorSet {
public:
    VulkanDescriptorSet() noexcept = default;
    VulkanDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSet set,
                        PFN_vkFreeDescriptorSets freeFunc);
    ~VulkanDescriptorSet();
    VulkanDescriptorSet(const VulkanDescriptorSet&) = delete;
    VulkanDescriptorSet& operator=(const VulkanDescriptorSet&) = delete;
    VulkanDescriptorSet(VulkanDescriptorSet&& o) noexcept;
    VulkanDescriptorSet& operator=(VulkanDescriptorSet&& o) noexcept;
    VkDescriptorSet get() const { return set_; }
private:
    VkDevice device_ = VK_NULL_HANDLE;
    VkDescriptorPool pool_ = VK_NULL_HANDLE;
    VkDescriptorSet set_ = VK_NULL_HANDLE;
    PFN_vkFreeDescriptorSets freeFunc_ = nullptr;
};

// ---------------------------------------------------------------------
// VulkanRTX – main ray-tracing class (declarations only)
// ---------------------------------------------------------------------
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
    void createBlackFallbackTexture();

    VkDescriptorSet getDescriptorSet() const { return ds_.get(); }
    VkPipeline getPipeline() const { return rtPipeline_.get(); }
    const ShaderBindingTable& getSBT() const { return sbt_; }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return dsLayout_.get(); }

private:
    VkCommandBuffer allocateTransientCommandBuffer(VkCommandPool commandPool);
    void submitAndWaitTransient(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool);
    void uploadBlackPixelToImage(VkImage image);
    void createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size,
                      VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VulkanResource<VkBuffer, PFN_vkDestroyBuffer>& buffer,
                      VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                            VkMemoryPropertyFlags properties);
    void compactAccelerationStructures(VkPhysicalDevice, VkCommandPool, VkQueue);
    void createStorageImage(VkPhysicalDevice physicalDevice, VkExtent2D extent,
                            VulkanResource<VkImage, PFN_vkDestroyImage>& image,
                            VulkanResource<VkImageView, PFN_vkDestroyImageView>& imageView,
                            VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory);

    void setDescriptorSetLayout(VkDescriptorSetLayout layout);
    void setDescriptorPool(VkDescriptorPool pool);
    void setDescriptorSet(VkDescriptorSet set);
    void setPipelineLayout(VkPipelineLayout layout);
    void setPipeline(VkPipeline pipeline);

    // MEMBERS
    std::shared_ptr<Vulkan::Context> context_;
    VulkanPipelineManager* pipelineMgr_;
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkExtent2D extent_;

    VulkanResource<VkDescriptorSetLayout, PFN_vkDestroyDescriptorSetLayout> dsLayout_;
    VulkanResource<VkDescriptorPool, PFN_vkDestroyDescriptorPool> dsPool_;
    VulkanDescriptorSet ds_;

    VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout> rtPipelineLayout_;
    VulkanResource<VkPipeline, PFN_vkDestroyPipeline> rtPipeline_;

    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> blasBuffer_;
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> blasMemory_;
    VulkanResource<VkBuffer, PFN_vkDestroyBuffer> tlasBuffer_;
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> tlasMemory_;
    VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR> blas_;
    VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR> tlas_;

    std::vector<uint32_t> primitiveCounts_;
    std::vector<uint32_t> previousPrimitiveCounts_;
    std::vector<DimensionState> previousDimensionCache_;

    bool supportsCompaction_ = false;
    ShaderBindingTable sbt_;

    VulkanResource<VkImage, PFN_vkDestroyImage> blackFallbackImage_;
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> blackFallbackMemory_;
    VulkanResource<VkImageView, PFN_vkDestroyImageView> blackFallbackView_;

    // function pointers
    PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR = nullptr;
    PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout = nullptr;
    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets = nullptr;
    PFN_vkCreateDescriptorPool vkCreateDescriptorPool = nullptr;
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout = nullptr;
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool = nullptr;
    PFN_vkFreeDescriptorSets vkFreeDescriptorSets = nullptr;
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout = nullptr;
    PFN_vkDestroyPipeline vkDestroyPipeline = nullptr;
    PFN_vkDestroyBuffer vkDestroyBuffer = nullptr;
    PFN_vkFreeMemory vkFreeMemory = nullptr;
    PFN_vkCreateBuffer vkCreateBuffer = nullptr;
    PFN_vkAllocateMemory vkAllocateMemory = nullptr;
    PFN_vkBindBufferMemory vkBindBufferMemory = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkCreateQueryPool vkCreateQueryPool = nullptr;
    PFN_vkDestroyQueryPool vkDestroyQueryPool = nullptr;
    PFN_vkGetQueryPoolResults vkGetQueryPoolResults = nullptr;
    PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR = nullptr;
    PFN_vkDestroyImage vkDestroyImage = nullptr;
    PFN_vkDestroyImageView vkDestroyImageView = nullptr;
};

} // namespace VulkanRTX