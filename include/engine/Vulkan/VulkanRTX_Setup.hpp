// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com is licensed under CC BY-NC 4.0
// Vulkan ray-tracing setup and management.
// Dependencies: Vulkan 1.3+, VK_KHR_acceleration_structure, VK_KHR_ray_tracing_pipeline, GLM, C++20 standard library, logging.hpp.
// Supported platforms: Linux, Windows (AMD, NVIDIA, Intel GPUs only).
// Zachary Geurts 2025

#ifndef VULKAN_RTX_SETUP_HPP
#define VULKAN_RTX_SETUP_HPP

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <string>
#include <atomic>
#include <mutex>
#include <stdexcept>
#include "ue_init.hpp"
#include "engine/logging.hpp"

// Forward declarations for Vulkan function pointer types
using PFN_vkCmdBuildAccelerationStructuresKHR = void (*)(VkCommandBuffer, uint32_t, const VkAccelerationStructureBuildGeometryInfoKHR*, const VkAccelerationStructureBuildRangeInfoKHR* const*);
using PFN_vkCreateRayTracingPipelinesKHR = VkResult (*)(VkDevice, VkDeferredOperationKHR, VkPipelineCache, uint32_t, const VkRayTracingPipelineCreateInfoKHR*, const VkAllocationCallbacks*, VkPipeline*);

namespace VulkanRTX {

class VulkanRTXException : public std::runtime_error {
public:
    explicit VulkanRTXException(const std::string& msg) : std::runtime_error(msg) {}
};

enum class ShaderFeatures : unsigned int {
    None = 0,
    AnyHit = 1 << 0,
    Callable = 1 << 1
};

constexpr ShaderFeatures operator&(ShaderFeatures lhs, ShaderFeatures rhs) {
    return static_cast<ShaderFeatures>(static_cast<unsigned int>(lhs) & static_cast<unsigned int>(rhs));
}

constexpr bool operator==(ShaderFeatures lhs, ShaderFeatures rhs) {
    return static_cast<unsigned int>(lhs) == static_cast<unsigned int>(rhs);
}

constexpr ShaderFeatures operator|(ShaderFeatures lhs, ShaderFeatures rhs) {
    return static_cast<ShaderFeatures>(static_cast<unsigned int>(lhs) | static_cast<unsigned int>(rhs));
}

inline ShaderFeatures& operator|=(ShaderFeatures& lhs, ShaderFeatures rhs) {
    lhs = lhs | rhs;
    return lhs;
}

enum class DescriptorBindings {
    TLAS = 0,
    StorageImage = 1,
    CameraUBO = 2,
    MaterialSSBO = 3,
    DimensionDataSSBO = 4,
    DenoiseImage = 5,
    EnvMap = 6
};

struct alignas(16) UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
    int mode;
};

struct alignas(16) MaterialData {
    alignas(16) glm::vec4 diffuse;   // RGBA color, 16 bytes
    alignas(4) float specular;       // Specular intensity, 4 bytes
    alignas(4) float roughness;      // Surface roughness, 4 bytes
    alignas(4) float metallic;       // Metallic property, 4 bytes
    alignas(16) glm::vec4 emission;  // Emission color/intensity, 16 bytes
    // Total size: 44 bytes, padded to 48 bytes for std140 alignment

    struct PushConstants {
        alignas(16) glm::vec4 clearColor;      // 16 bytes
        alignas(16) glm::vec3 cameraPosition;  // 12 bytes + 4 bytes padding
        alignas(16) glm::vec3 lightDirection;  // 12 bytes + 4 bytes padding
        alignas(4) float lightIntensity;       // 4 bytes
        alignas(4) uint32_t samplesPerPixel;   // 4 bytes
        alignas(4) uint32_t maxDepth;          // 4 bytes
        alignas(4) uint32_t maxBounces;        // 4 bytes
        alignas(4) float russianRoulette;      // 4 bytes
        // Total size: 60 bytes, padded to 64 bytes for alignof=16
    };
    // static_assert(sizeof(PushConstants) == 64 && alignof(PushConstants) == 16, "PushConstants alignment mismatch");
};

template<typename T, typename DestroyFuncType>
class VulkanResource {
public:
    VulkanResource() noexcept : device_(VK_NULL_HANDLE), resource_(VK_NULL_HANDLE), destroyFunc_(nullptr) {}
    VulkanResource(VkDevice device, T resource, DestroyFuncType destroyFunc)
        : device_(device), resource_(resource), destroyFunc_(destroyFunc) {
        if (!device || !resource || !destroyFunc) {
            throw VulkanRTXException("VulkanResource initialized with null device, resource, or destroy function");
        }
    }
    ~VulkanResource() {
        if constexpr (std::is_same_v<T, VkDescriptorSet>) {
            // VkDescriptorSet destruction handled by VulkanDescriptorSet
        } else {
            if (resource_ != VK_NULL_HANDLE && destroyFunc_ != nullptr) {
                destroyFunc_(device_, resource_, nullptr);
            }
        }
    }
    VulkanResource(const VulkanResource&) = delete;
    VulkanResource& operator=(const VulkanResource&) = delete;
    VulkanResource(VulkanResource&& other) noexcept
        : device_(other.device_), resource_(other.resource_), destroyFunc_(other.destroyFunc_) {
        other.resource_ = VK_NULL_HANDLE;
    }
    VulkanResource& operator=(VulkanResource&& other) noexcept {
        if (this != &other) {
            if constexpr (!std::is_same_v<T, VkDescriptorSet>) {
                if (resource_ != VK_NULL_HANDLE && destroyFunc_ != nullptr) {
                    destroyFunc_(device_, resource_, nullptr);
                }
            }
            device_ = other.device_;
            resource_ = other.resource_;
            destroyFunc_ = other.destroyFunc_;
            other.resource_ = VK_NULL_HANDLE;
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

class VulkanDescriptorSet {
public:
    VulkanDescriptorSet() noexcept : device_(VK_NULL_HANDLE), pool_(VK_NULL_HANDLE), set_(VK_NULL_HANDLE), vkFreeDescriptorSets_(nullptr) {}
    VulkanDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSet set, PFN_vkFreeDescriptorSets freeFunc)
        : device_(device), pool_(pool), set_(set), vkFreeDescriptorSets_(freeFunc) {
        if (!device || !pool || !set || !freeFunc) {
            throw VulkanRTXException("VulkanDescriptorSet initialized with null device, pool, set, or free function");
        }
    }
    ~VulkanDescriptorSet() {
        if (set_ != VK_NULL_HANDLE && vkFreeDescriptorSets_ != nullptr) {
            vkFreeDescriptorSets_(device_, pool_, 1, &set_);
        }
    }
    VulkanDescriptorSet(const VulkanDescriptorSet&) = delete;
    VulkanDescriptorSet& operator=(const VulkanDescriptorSet&) = delete;
    VulkanDescriptorSet(VulkanDescriptorSet&& other) noexcept
        : device_(other.device_), pool_(other.pool_), set_(other.set_), vkFreeDescriptorSets_(other.vkFreeDescriptorSets_) {
        other.set_ = VK_NULL_HANDLE;
    }
    VulkanDescriptorSet& operator=(VulkanDescriptorSet&& other) noexcept {
        if (this != &other) {
            if (set_ != VK_NULL_HANDLE && vkFreeDescriptorSets_ != nullptr) {
                vkFreeDescriptorSets_(device_, pool_, 1, &set_);
            }
            device_ = other.device_;
            pool_ = other.pool_;
            set_ = other.set_;
            vkFreeDescriptorSets_ = other.vkFreeDescriptorSets_;
            other.set_ = VK_NULL_HANDLE;
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

class VulkanRTX {
public:
    struct ShaderBindingTable {
        VulkanResource<VkBuffer, PFN_vkDestroyBuffer> buffer;
        VulkanResource<VkDeviceMemory, PFN_vkFreeMemory> memory;
        VkStridedDeviceAddressRegionKHR raygen;
        VkStridedDeviceAddressRegionKHR miss;
        VkStridedDeviceAddressRegionKHR hit;
        VkStridedDeviceAddressRegionKHR callable;

        ShaderBindingTable() noexcept = default;
        ShaderBindingTable(VkDevice device, VkBuffer buffer, VkDeviceMemory memory, 
                           PFN_vkDestroyBuffer destroyBuffer, PFN_vkFreeMemory freeMemory);
        ~ShaderBindingTable() = default;
        ShaderBindingTable(const ShaderBindingTable&) = delete;
        ShaderBindingTable& operator=(const ShaderBindingTable&) = delete;
        ShaderBindingTable(ShaderBindingTable&&) noexcept = default;
        ShaderBindingTable& operator=(ShaderBindingTable&&) noexcept = default;
    };

    VulkanRTX(VkDevice device, VkPhysicalDevice physicalDevice, const std::vector<std::string>& shaderPaths);
    ~VulkanRTX();

    VulkanRTX(const VulkanRTX&) = delete;
    VulkanRTX& operator=(const VulkanRTX&) = delete;

    void initializeRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                       const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                       uint32_t maxRayRecursionDepth, const std::vector<UE::DimensionData>& dimensionCache);
    void updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                   const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                   const std::vector<UE::DimensionData>& dimensionCache);
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
                           VkImageView storageImageView, VkImageView denoiseImageView, VkImageView envMapView, VkSampler envMapSampler);
    void recordRayTracingCommands(VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage,
                                  VkImageView outputImageView, const MaterialData::PushConstants& pc);
    void updateDescriptorSetForTLAS(VkAccelerationStructureKHR tlas);
    void compactAccelerationStructures(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue);

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
    void buildShaderGroups(std::vector<VkRayTracingShaderGroupCreateInfoKHR>& groups,
                           const std::vector<VkPipelineShaderStageCreateInfo>& stages);
    VkCommandBuffer allocateTransientCommandBuffer(VkCommandPool commandPool);
    void submitAndWaitTransient(VkCommandBuffer cmdBuffer, VkQueue queue, VkCommandPool commandPool);
    void createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties, VulkanResource<VkBuffer, PFN_vkDestroyBuffer>& buffer,
                      VulkanResource<VkDeviceMemory, PFN_vkFreeMemory>& memory);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
    VkDeviceAddress getBufferDeviceAddress(VkBuffer buffer);
    VkDeviceAddress getAccelerationStructureDeviceAddress(VkAccelerationStructureKHR as);
    void setDescriptorSetLayout(VkDescriptorSetLayout layout) { 
        dsLayout_ = VulkanResource<VkDescriptorSetLayout, PFN_vkDestroyDescriptorSetLayout>(device_, layout, vkDestroyDescriptorSetLayout); 
    }
    void setDescriptorPool(VkDescriptorPool pool) { 
        dsPool_ = VulkanResource<VkDescriptorPool, PFN_vkDestroyDescriptorPool>(device_, pool, vkDestroyDescriptorPool); 
    }
    void setDescriptorSet(VkDescriptorSet set) { 
        ds_ = VulkanDescriptorSet(device_, dsPool_.get(), set, vkFreeDescriptorSets); 
    }
    void setPipelineLayout(VkPipelineLayout layout) { 
        rtPipelineLayout_ = VulkanResource<VkPipelineLayout, PFN_vkDestroyPipelineLayout>(device_, layout, vkDestroyPipelineLayout); 
    }
    void setPipeline(VkPipeline pipeline) { 
        rtPipeline_ = VulkanResource<VkPipeline, PFN_vkDestroyPipeline>(device_, pipeline, vkDestroyPipeline); 
    }
    void setBLAS(VkAccelerationStructureKHR as) { 
        blas_ = VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>(device_, as, vkDestroyAccelerationStructureKHR); 
    }
    void setTLAS(VkAccelerationStructureKHR as) { 
        tlas_ = VulkanResource<VkAccelerationStructureKHR, PFN_vkDestroyAccelerationStructureKHR>(device_, as, vkDestroyAccelerationStructureKHR); 
    }
    void setSupportsCompaction(bool value) { supportsCompaction_ = value; }
    void setPrimitiveCounts(const std::vector<VkAccelerationStructureBuildRangeInfoKHR>& ranges) { 
        primitiveCounts_.clear(); 
        for (const auto& range : ranges) primitiveCounts_.push_back(range.primitiveCount); 
    }
    void setPreviousDimensionCache(const std::vector<UE::DimensionData>& cache) { 
        previousDimensionCache_ = cache; 
    }
    bool hasShaderFeature(ShaderFeatures feature) const { 
        return (shaderFeatures_ & feature) == feature; 
    }

    // Static members
    static std::atomic<bool> functionPtrInitialized_;
    static std::atomic<bool> shaderModuleInitialized_;
    static std::mutex functionPtrMutex_;
    static std::mutex shaderModuleMutex_;

    // Device and other members
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
    std::vector<UE::DimensionData> previousDimensionCache_;
    bool supportsCompaction_;
    ShaderFeatures shaderFeatures_;
    uint32_t numShaderGroups_;
    ShaderBindingTable sbt_;
    VkDeviceSize scratchAlignment_;
    // Core Vulkan functions
    PFN_vkGetDeviceProcAddr vkGetDeviceProcAddrFunc;
    PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR;
    PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR;
    PFN_vkCreateDescriptorSetLayout vkCreateDescriptorSetLayout;
    PFN_vkAllocateDescriptorSets vkAllocateDescriptorSets;
    PFN_vkCreateDescriptorPool vkCreateDescriptorPool;
    PFN_vkGetPhysicalDeviceProperties2 vkGetPhysicalDeviceProperties2;
    PFN_vkCreateShaderModule vkCreateShaderModule;
    PFN_vkDestroyDescriptorSetLayout vkDestroyDescriptorSetLayout;
    PFN_vkDestroyDescriptorPool vkDestroyDescriptorPool;
    PFN_vkFreeDescriptorSets vkFreeDescriptorSets;
    PFN_vkDestroyPipelineLayout vkDestroyPipelineLayout;
    PFN_vkDestroyPipeline vkDestroyPipeline;
    PFN_vkDestroyBuffer vkDestroyBuffer;
    PFN_vkFreeMemory vkFreeMemory;
    PFN_vkCreateQueryPool vkCreateQueryPool;
    PFN_vkDestroyQueryPool vkDestroyQueryPool;
    PFN_vkGetQueryPoolResults vkGetQueryPoolResults;
    PFN_vkCmdWriteAccelerationStructuresPropertiesKHR vkCmdWriteAccelerationStructuresPropertiesKHR;
    PFN_vkCreateBuffer vkCreateBuffer;
    PFN_vkAllocateMemory vkAllocateMemory;
    PFN_vkBindBufferMemory vkBindBufferMemory;
    PFN_vkGetPhysicalDeviceMemoryProperties vkGetPhysicalDeviceMemoryProperties;
    PFN_vkBeginCommandBuffer vkBeginCommandBuffer;
    PFN_vkEndCommandBuffer vkEndCommandBuffer;
    PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
    PFN_vkQueueSubmit vkQueueSubmit;
    PFN_vkQueueWaitIdle vkQueueWaitIdle;
    PFN_vkFreeCommandBuffers vkFreeCommandBuffers;
    PFN_vkCmdResetQueryPool vkCmdResetQueryPool;
    PFN_vkGetBufferMemoryRequirements vkGetBufferMemoryRequirements;
    PFN_vkMapMemory vkMapMemory;
    PFN_vkUnmapMemory vkUnmapMemory;
    PFN_vkCreateImage vkCreateImage;
    PFN_vkDestroyImage vkDestroyImage;
    PFN_vkGetImageMemoryRequirements vkGetImageMemoryRequirements;
    PFN_vkBindImageMemory vkBindImageMemory;
    PFN_vkCreateImageView vkCreateImageView;
    PFN_vkDestroyImageView vkDestroyImageView;
    PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
    PFN_vkCmdPipelineBarrier vkCmdPipelineBarrier;
    PFN_vkCmdBindPipeline vkCmdBindPipeline;
    PFN_vkCmdBindDescriptorSets vkCmdBindDescriptorSets;
    PFN_vkCmdPushConstants vkCmdPushConstants;
    PFN_vkCmdCopyBuffer vkCmdCopyBuffer;
    PFN_vkCreatePipelineLayout vkCreatePipelineLayout;
    PFN_vkCreateComputePipelines vkCreateComputePipelines;
    PFN_vkCmdDispatch vkCmdDispatch;
    PFN_vkDestroyShaderModule vkDestroyShaderModule;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR;
};

} // namespace VulkanRTX

#endif // VULKAN_RTX_SETUP_HPP