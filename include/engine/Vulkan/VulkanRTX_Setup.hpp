// AMOURANTH RTX Engine Demo © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0

#ifndef VULKAN_RTX_SETUP_HPP
#define VULKAN_RTX_SETUP_HPP

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>
#include <stdexcept>
#include <tuple>
#include <atomic>

#include "engine/Vulkan/types.hpp"
using DimensionState = ::DimensionState;
#include "engine/logging.hpp"
#include "engine/Vulkan/VulkanCore.hpp"

namespace VulkanRTX {

// ---------------------------------------------------------------------
//  Descriptor Bindings – shared between VulkanRTX and VulkanPipelineManager
// ---------------------------------------------------------------------
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
//  Vulkan function-pointer typedefs
// ---------------------------------------------------------------------
using PFN_vkCmdBuildAccelerationStructuresKHR = void (*)(VkCommandBuffer, uint32_t,
                                                       const VkAccelerationStructureBuildGeometryInfoKHR*,
                                                       const VkAccelerationStructureBuildRangeInfoKHR* const*);
using PFN_vkCreateRayTracingPipelinesKHR = VkResult (*)(VkDevice, VkDeferredOperationKHR, VkPipelineCache,
                                                       uint32_t, const VkRayTracingPipelineCreateInfoKHR*,
                                                       const VkAllocationCallbacks*, VkPipeline*);
using PFN_vkGetBufferDeviceAddress = VkDeviceAddress (*)(VkDevice, const VkBufferDeviceAddressInfo*);
using PFN_vkCmdTraceRaysKHR = void (*)(VkCommandBuffer,
                                      const VkStridedDeviceAddressRegionKHR*,
                                      const VkStridedDeviceAddressRegionKHR*,
                                      const VkStridedDeviceAddressRegionKHR*,
                                      const VkStridedDeviceAddressRegionKHR*,
                                      uint32_t, uint32_t, uint32_t);
using PFN_vkCreateAccelerationStructureKHR = VkResult (*)(VkDevice,
                                                          const VkAccelerationStructureCreateInfoKHR*,
                                                          const VkAllocationCallbacks*,
                                                          VkAccelerationStructureKHR*);
using PFN_vkDestroyAccelerationStructureKHR = void (*)(VkDevice, VkAccelerationStructureKHR,
                                                       const VkAllocationCallbacks*);
using PFN_vkGetAccelerationStructureBuildSizesKHR = void (*)(VkDevice,
                                                            VkAccelerationStructureBuildTypeKHR,
                                                            const VkAccelerationStructureBuildGeometryInfoKHR*,
                                                            const uint32_t*,
                                                            VkAccelerationStructureBuildSizesInfoKHR*);
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
using PFN_vkGetPhysicalDeviceProperties2 = void (*)(VkPhysicalDevice, VkPhysicalDeviceProperties2*);
using PFN_vkCreateShaderModule = VkResult (*)(VkDevice, const VkShaderModuleCreateInfo*,
                                             const VkAllocationCallbacks*, VkShaderModule*);
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
using PFN_vkCreateQueryPool = VkResult (*)(VkDevice, const VkQueryPoolCreateInfo*,
                                          const VkAllocationCallbacks*, VkQueryPool*);
using PFN_vkDestroyQueryPool = void (*)(VkDevice, VkQueryPool,
                                       const VkAllocationCallbacks*);
using PFN_vkGetQueryPoolResults = VkResult (*)(VkDevice, VkQueryPool, uint32_t, uint32_t,
                                              size_t, void*, VkDeviceSize, VkQueryResultFlags);
using PFN_vkCmdWriteAccelerationStructuresPropertiesKHR = void (*)(VkCommandBuffer, uint32_t,
                                                                   const VkAccelerationStructureKHR*,
                                                                   VkQueryType, VkQueryPool, uint32_t);
using PFN_vkCreateBuffer = VkResult (*)(VkDevice, const VkBufferCreateInfo*,
                                       const VkAllocationCallbacks*, VkBuffer*);
using PFN_vkAllocateMemory = VkResult (*)(VkDevice, const VkMemoryAllocateInfo*,
                                         const VkAllocationCallbacks*, VkDeviceMemory*);
using PFN_vkBindBufferMemory = VkResult (*)(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize);
using PFN_vkGetPhysicalDeviceMemoryProperties = void (*)(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties*);
using PFN_vkBeginCommandBuffer = VkResult (*)(VkCommandBuffer, const VkCommandBufferBeginInfo*);
using PFN_vkEndCommandBuffer = VkResult (*)(VkCommandBuffer);
using PFN_vkAllocateCommandBuffers = VkResult (*)(VkDevice, const VkCommandBufferAllocateInfo*,
                                                 VkCommandBuffer*);
using PFN_vkQueueSubmit = VkResult (*)(VkQueue, uint32_t, const VkSubmitInfo*, VkFence);
using PFN_vkQueueWaitIdle = VkResult (*)(VkQueue);
using PFN_vkFreeCommandBuffers = void (*)(VkDevice, VkCommandPool, uint32_t, const VkCommandBuffer*);
using PFN_vkCmdResetQueryPool = void (*)(VkCommandBuffer, VkQueryPool, uint32_t, uint32_t);
using PFN_vkGetBufferMemoryRequirements = void (*)(VkDevice, VkBuffer, VkMemoryRequirements*);
using PFN_vkMapMemory = VkResult (*)(VkDevice, VkDeviceMemory, VkDeviceSize, VkDeviceSize,
                                    VkMemoryMapFlags, void**);
using PFN_vkUnmapMemory = void (*)(VkDevice, VkDeviceMemory);
using PFN_vkCreateImage = VkResult (*)(VkDevice, const VkImageCreateInfo*,
                                      const VkAllocationCallbacks*, VkImage*);
using PFN_vkDestroyImage = void (*)(VkDevice, VkImage, const VkAllocationCallbacks*);
using PFN_vkGetImageMemoryRequirements = void (*)(VkDevice, VkImage, VkMemoryRequirements*);
using PFN_vkBindImageMemory = VkResult (*)(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize);
using PFN_vkCreateImageView = VkResult (*)(VkDevice, const VkImageViewCreateInfo*,
                                         const VkAllocationCallbacks*, VkImageView*);
using PFN_vkDestroyImageView = void (*)(VkDevice, VkImageView, const VkAllocationCallbacks*);
using PFN_vkUpdateDescriptorSets = void (*)(VkDevice, uint32_t, const VkWriteDescriptorSet*,
                                           uint32_t, const VkCopyDescriptorSet*);
using PFN_vkCmdPipelineBarrier = void (*)(VkCommandBuffer, VkPipelineStageFlags, VkPipelineStageFlags,
                                         VkDependencyFlags, uint32_t, const VkMemoryBarrier*,
                                         uint32_t, const VkBufferMemoryBarrier*,
                                         uint32_t, const VkImageMemoryBarrier*);
using PFN_vkCmdBindPipeline = void (*)(VkCommandBuffer, VkPipelineBindPoint, VkPipeline);
using PFN_vkCmdBindDescriptorSets = void (*)(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout,
                                            uint32_t, uint32_t, const VkDescriptorSet*,
                                            uint32_t, const uint32_t*);
using PFN_vkCmdPushConstants = void (*)(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags,
                                       uint32_t, uint32_t, const void*);
using PFN_vkCmdCopyBuffer = void (*)(VkCommandBuffer, VkBuffer, VkBuffer, uint32_t, const VkBufferCopy*);
using PFN_vkCreatePipelineLayout = VkResult (*)(VkDevice, const VkPipelineLayoutCreateInfo*,
                                               const VkAllocationCallbacks*, VkPipelineLayout*);
using PFN_vkCreateComputePipelines = VkResult (*)(VkDevice, VkPipelineCache, uint32_t,
                                                 const VkComputePipelineCreateInfo*,
                                                 const VkAllocationCallbacks*, VkPipeline*);
using PFN_vkCmdDispatch = void (*)(VkCommandBuffer, uint32_t, uint32_t, uint32_t);
using PFN_vkDestroyShaderModule = void (*)(VkDevice, VkShaderModule, const VkAllocationCallbacks*);

// ---------------------------------------------------------------------
//  Exception
// ---------------------------------------------------------------------
class VulkanRTXException : public std::runtime_error {
public:
    explicit VulkanRTXException(const std::string& msg) : std::runtime_error(msg) {}
};

// ---------------------------------------------------------------------
//  Shader features & counts
// ---------------------------------------------------------------------
enum class ShaderFeatures : unsigned int {
    None         = 0,
    Raygen       = 1 << 0,
    Miss         = 1 << 1,
    ClosestHit   = 1 << 2,
    AnyHit       = 1 << 3,
    Intersection = 1 << 4,
    Callable     = 1 << 5
};

constexpr ShaderFeatures operator|(ShaderFeatures lhs, ShaderFeatures rhs) {
    return static_cast<ShaderFeatures>(static_cast<unsigned int>(lhs) |
                                      static_cast<unsigned int>(rhs));
}
inline ShaderFeatures& operator|=(ShaderFeatures& lhs, ShaderFeatures rhs) {
    lhs = lhs | rhs;
    return lhs;
}

struct ShaderCounts {
    uint32_t raygen       = 0;
    uint32_t miss         = 0;
    uint32_t chit         = 0;
    uint32_t ahit         = 0;
    uint32_t intersection = 0;
    uint32_t callable     = 0;
};

// ---------------------------------------------------------------------
//  RAII: VulkanResource
// ---------------------------------------------------------------------
template<typename T, typename DestroyFuncType>
class VulkanResource {
public:
    VulkanResource() noexcept
        : device_(VK_NULL_HANDLE), resource_(VK_NULL_HANDLE), destroyFunc_(nullptr) {}

    VulkanResource(VkDevice device, T resource, DestroyFuncType destroyFunc)
        : device_(device), resource_(resource), destroyFunc_(destroyFunc) {
        if (!device || !resource || !destroyFunc)
            throw VulkanRTXException("VulkanResource initialized with null");
    }

    ~VulkanResource() {
        if constexpr (std::is_same_v<T, VkDescriptorSet>) {
            // Handled by VulkanDescriptorSet
        } else {
            if (resource_ != VK_NULL_HANDLE && destroyFunc_)
                destroyFunc_(device_, resource_, nullptr);
        }
    }

    VulkanResource(const VulkanResource&) = delete;
    VulkanResource& operator=(const VulkanResource&) = delete;

    VulkanResource(VulkanResource&& o) noexcept
        : device_(o.device_), resource_(o.resource_), destroyFunc_(o.destroyFunc_) {
        o.resource_ = VK_NULL_HANDLE;
    }

    VulkanResource& operator=(VulkanResource&& o) noexcept {
        if (this != &o) {
            if constexpr (!std::is_same_v<T, VkDescriptorSet>) {
                if (resource_ && destroyFunc_) destroyFunc_(device_, resource_, nullptr);
            }
            device_     = o.device_;
            resource_   = o.resource_;
            destroyFunc_= o.destroyFunc_;
            o.resource_ = VK_NULL_HANDLE;
        }
        return *this;
    }

    T get() const { return resource_; }
    T* getPtr() { return &resource_; }

private:
    VkDevice device_;
    T resource_;
    DestroyFuncType destroyFunc_;
};

// ---------------------------------------------------------------------
//  RAII: VulkanDescriptorSet
// ---------------------------------------------------------------------
class VulkanDescriptorSet {
public:
    VulkanDescriptorSet() noexcept
        : device_(VK_NULL_HANDLE), pool_(VK_NULL_HANDLE), set_(VK_NULL_HANDLE), vkFreeDescriptorSets_(nullptr) {}

    VulkanDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSet set,
                        PFN_vkFreeDescriptorSets freeFunc)
        : device_(device), pool_(pool), set_(set), vkFreeDescriptorSets_(freeFunc) {
        if (!device || !pool || !set || !freeFunc)
            throw VulkanRTXException("VulkanDescriptorSet initialized with null");
    }

    ~VulkanDescriptorSet() {
        if (set_ != VK_NULL_HANDLE && vkFreeDescriptorSets_)
            vkFreeDescriptorSets_(device_, pool_, 1, &set_);
    }

    VulkanDescriptorSet(const VulkanDescriptorSet&) = delete;
    VulkanDescriptorSet& operator=(const VulkanDescriptorSet&) = delete;

    VulkanDescriptorSet(VulkanDescriptorSet&& o) noexcept
        : device_(o.device_), pool_(o.pool_), set_(o.set_), vkFreeDescriptorSets_(o.vkFreeDescriptorSets_) {
        o.set_ = VK_NULL_HANDLE;
    }

    VulkanDescriptorSet& operator=(VulkanDescriptorSet&& o) noexcept {
        if (this != &o) {
            if (set_ && vkFreeDescriptorSets_) vkFreeDescriptorSets_(device_, pool_, 1, &set_);
            device_            = o.device_;
            pool_              = o.pool_;
            set_               = o.set_;
            vkFreeDescriptorSets_ = o.vkFreeDescriptorSets_;
            o.set_ = VK_NULL_HANDLE;
        }
        return *this;
    }

    VkDescriptorSet get() const { return set_; }

private:
    VkDevice device_;
    VkDescriptorPool pool_;
    VkDescriptorSet set_;
    PFN_vkFreeDescriptorSets vkFreeDescriptorSets_;
};

struct ShaderBindingTable {
    VkStridedDeviceAddressRegionKHR raygen = {};
    VkStridedDeviceAddressRegionKHR miss = {};
    VkStridedDeviceAddressRegionKHR hit = {};
    VkStridedDeviceAddressRegionKHR callable = {};

    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};

// ---------------------------------------------------------------------
//  VulkanRTX – main class
// ---------------------------------------------------------------------
class VulkanRTX {
public:
    VulkanRTX(VkDevice device, VkPhysicalDevice physicalDevice,
              const std::vector<std::string>& shaderPaths);
    ~VulkanRTX();

    VulkanRTX(const VulkanRTX&) = delete;
    VulkanRTX& operator=(const VulkanRTX&) = delete;

    void initializeRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                       const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                       uint32_t maxRayRecursionDepth, const std::vector<DimensionState>& dimensionCache);
    void updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                   const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                   const std::vector<DimensionState>& dimensionCache);
    void createDescriptorSetLayout();
    void createDescriptorPoolAndSet();
    void createRayTracingPipeline(uint32_t maxRayRecursionDepth);
    void createShaderBindingTable(VkPhysicalDevice physicalDevice);
    void createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                             const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries);
    void createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                          const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances);
    void createStorageImage(VkPhysicalDevice physicalDevice, VkExtent2D extent, VkFormat format,
                           VulkanResource<VkImage, PFN_vkDestroyImage>& image,
                           VulkanResource<VkImageView, PFN_vkDestroyImageView>& imageView,
                           VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory);
    void updateDescriptors(VkBuffer cameraBuffer, VkBuffer materialBuffer, VkBuffer dimensionBuffer,
                          VkImageView storageImageView, VkImageView denoiseImageView, VkImageView envMapView, VkSampler envMapSampler,
                          VkImageView densityVolumeView, VkImageView gDepthView, VkImageView gNormalView);
    void recordRayTracingCommands(VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage,
                                 VkImageView outputImageView, const MaterialData::PushConstants& pc);
    void updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas);
    void compactAccelerationStructures(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue);
    void createBlackFallbackTexture();

    bool getSupportsCompaction() const { return supportsCompaction_; }
    VkDescriptorSet getDescriptorSet() const { return ds_.get(); }
    VkPipeline getPipeline() const { return rtPipeline_.get(); }
    const ShaderBindingTable& getSBT() const { return sbt_; }
    PFN_vkDestroyBuffer getVkDestroyBuffer() const { return vkDestroyBuffer; }
    PFN_vkFreeMemory getVkFreeMemory() const { return vkFreeMemory; }

private:
    VkShaderModule createShaderModule(const std::string& filename);
    bool shaderFileExists(const std::string& filename) const;
    void loadShadersAsync(std::vector<VkShaderModule>& modules, const std::vector<std::string>& paths);
    void buildShaderGroups(std::vector<VkRayTracingShaderGroupCreateInfoKHR>& groups);
    VkCommandBuffer allocateTransientCommandBuffer(VkCommandPool commandPool);
    void submitAndWaitTransient(VkCommandBuffer cmdBuffer, VkQueue queue, VkCommandPool commandPool);
    void createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties, VulkanResource<VkBuffer, PFN_vkDestroyBuffer>& buffer,
                      VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer);
    VkDeviceAddress getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR as);
    void uploadBlackPixelToImage(VkImage image);

    void setDescriptorSetLayout(VkDescriptorSetLayout layout) {
        dsLayout_ = VulkanResource<VkDescriptorSetLayout, PFN_vkDestroyDescriptorSetLayout>(
            device_, layout, vkDestroyDescriptorSetLayout);
    }
    void setDescriptorPool(VkDescriptorPool pool) {
        dsPool_ = VulkanResource<VkDescriptorPool, PFN_vkDestroyDescriptorPool>(
            device_, pool, vkDestroyDescriptorPool);
    }
    void setDescriptorSet(VkDescriptorSet set) {
        ds_ = VulkanDescriptorSet(device_, dsPool_.get(), set, vkFreeDescriptorSets);
    }
    void setPipelineLayout(VkPipelineLayout layout) {
        rtPipelineLayout_ = VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>(
            device_, layout, vkDestroyPipelineLayout);
    }
    void setPipeline(VkPipeline pipeline) {
        rtPipeline_ = VulkanResource<VkPipeline, PFN_vkDestroyPipeline>(
            device_, pipeline, vkDestroyPipeline);
    }
    void setBLAS(VkAccelerationStructureKHR as) {
        blas_ = VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>(
            device_, as, vkDestroyAccelerationStructureKHR);
    }
    void setTLAS(VkAccelerationStructureKHR as) {
        tlas_ = VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>(
            device_, as, vkDestroyAccelerationStructureKHR);
    }
    void setSupportsCompaction(bool value) { supportsCompaction_ = value; }
    void setPrimitiveCounts(const std::vector<VkAccelerationStructureBuildRangeInfoKHR>& ranges) {
        primitiveCounts_.clear();
        for (const auto& r : ranges) primitiveCounts_.push_back(r.primitiveCount);
    }
    void setPreviousDimensionCache(const std::vector<DimensionState>& cache) {
        previousDimensionCache_ = cache;
    }
    bool hasShaderFeature(ShaderFeatures feature) const {
        return (static_cast<unsigned int>(shaderFeatures_) &
                static_cast<unsigned int>(feature)) != 0;
    }

    VkDevice device_;
    VkPhysicalDevice physicalDevice_;
    std::vector<std::string> shaderPaths_;

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

    VkExtent2D extent_;
    std::vector<uint32_t> primitiveCounts_;
    std::vector<uint32_t> previousPrimitiveCounts_;
    std::vector<DimensionState> previousDimensionCache_;

    bool supportsCompaction_ = false;
    ShaderFeatures shaderFeatures_ = ShaderFeatures::None;
    uint32_t numShaderGroups_ = 0;
    ShaderCounts counts_;
    ShaderBindingTable sbt_;
    VkDeviceSize scratchAlignment_ = 0;

    VulkanResource<VkImage, PFN_vkDestroyImage> blackFallbackImage_;
    VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> blackFallbackMemory_;
    VulkanResource<VkImageView, PFN_vkDestroyImageView> blackFallbackView_;

    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProperties_;

    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddrFunc = nullptr;
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
    PFN_vkGetPhysicalDeviceProperties2 vkGetPhysicalDeviceProperties2 = nullptr;
    PFN_vkCreateShaderModule vkCreateShaderModule = nullptr;
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout = nullptr;
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool = nullptr;
    PFN_vkFreeDescriptorSets vkFreeDescriptorSets = nullptr;
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout = nullptr;
    PFN_vkDestroyPipeline vkDestroyPipeline = nullptr;
    PFN_vkDestroyBuffer vkDestroyBuffer = nullptr;
    PFN_vkFreeMemory vkFreeMemory = nullptr;
    PFN_vkCreateQueryPool vkCreateQueryPool = nullptr;
    PFN_vkDestroyQueryPool vkDestroyQueryPool = nullptr;
    PFN_vkGetQueryPoolResults vkGetQueryPoolResults = nullptr;
    PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR = nullptr;
    PFN_vkCreateBuffer vkCreateBuffer = nullptr;
    PFN_vkAllocateMemory vkAllocateMemory = nullptr;
    PFN_vkBindBufferMemory vkBindBufferMemory = nullptr;
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties = nullptr;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer = nullptr;
    PFN_vkEndCommandBuffer vkEndCommandBuffer = nullptr;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers = nullptr;
    PFN_vkQueueSubmit vkQueueSubmit = nullptr;
    PFN_vkQueueWaitIdle vkQueueWaitIdle = nullptr;
    PFN_vkFreeCommandBuffers vkFreeCommandBuffers = nullptr;
    PFN_vkCmdResetQueryPool vkCmdResetQueryPool = nullptr;
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements = nullptr;
    PFN_vkMapMemory vkMapMemory = nullptr;
    PFN_vkUnmapMemory vkUnmapMemory = nullptr;
    PFN_vkCreateImage vkCreateImage = nullptr;
    PFN_vkDestroyImage vkDestroyImage = nullptr;
    PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements = nullptr;
    PFN_vkBindImageMemory vkBindImageMemory = nullptr;
    PFN_vkCreateImageView vkCreateImageView = nullptr;
    PFN_vkDestroyImageView vkDestroyImageView = nullptr;
    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets = nullptr;
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier = nullptr;
    PFN_vkCmdBindPipeline vkCmdBindPipeline = nullptr;
    PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets = nullptr;
    PFN_vkCmdPushConstants vkCmdPushConstants = nullptr;
    PFN_vkCmdCopyBuffer vkCmdCopyBuffer = nullptr;
    PFN_vkCreatePipelineLayout vkCreatePipelineLayout = nullptr;
    PFN_vkCreateComputePipelines vkCreateComputePipelines = nullptr;
    PFN_vkCmdDispatch vkCmdDispatch = nullptr;
    PFN_vkDestroyShaderModule vkDestroyShaderModule = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;

    friend class VulkanRenderer;
};

} // namespace VulkanRTX

#endif // VULKAN_RTX_SETUP_HPP