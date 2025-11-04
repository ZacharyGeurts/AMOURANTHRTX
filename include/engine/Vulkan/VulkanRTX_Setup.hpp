// include/engine/Vulkan/VulkanRTX_Setup.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: NO Dispose::VulkanHandle → RAW HANDLES + ResourceManager
//        Fence-based transient submits → NO vkQueueWaitIdle() → NO SEGFAULT
//        ShaderBindingTable → VkStridedDeviceAddressRegionKHR (Vulkan spec compliant)
//        ALL FUNCTION POINTERS LOADED — NO SEGFAULT
//        FIXED: VK_CHECK(2 args), VulkanRTXException(1-arg + 4-arg), THROW_VKRTX
//        FIXED: frameNumber_ and time_ for dispatchRenderMode
//        FIXED: std::runtime582_error → std::runtime_error

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <vector>
#include <array>
#include <stdexcept>
#include <cstdint>
#include <sstream>
#include <format>  // C++20 std::format

#include "engine/logging.hpp"

namespace Vulkan { struct Context; }

// ---------------------------------------------------------------------
// VK_CHECK – 2 arguments: (result, "message")
// Throws VulkanRTXException(msg) on error
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

// ---------------------------------------------------------------------
// Helper macro for throwing with file/line/function (use in .cpp)
// ---------------------------------------------------------------------
#define THROW_VKRTX(msg) \
    throw VulkanRTXException(msg, __FILE__, __LINE__, __func__)

inline constexpr uint32_t alignUp(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

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
   Descriptor-related function pointers (CORE VULKAN — MUST BE LOADED)
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

struct DimensionState {
    uint32_t width;
    uint32_t height;
};

// -------------------------------------------------------------
// VulkanRTXException – supports both 1-arg and 4-arg
// -------------------------------------------------------------
class VulkanRTXException : public std::runtime_error {
public:
    // 1-argument: used by VK_CHECK and legacy code
    explicit VulkanRTXException(const std::string& msg)
        : std::runtime_error(msg), m_line(0) {}

    // 4-argument: used by THROW_VKRTX for full debug info
    VulkanRTXException(const std::string& msg, const char* file, int line, const char* func)
        : std::runtime_error(msg)
        , m_file(file)
        , m_line(line)
        , m_function(func)
    {}

    const char* file()     const noexcept { return m_file.c_str(); }
    int         line()     const noexcept { return m_line; }
    const char* function() const noexcept { return m_function.c_str(); }

private:
    std::string m_file;
    int         m_line = 0;
    std::string m_function;
};

// Forward declaration
class VulkanPipelineManager;

struct ShaderBindingTable {
    VkStridedDeviceAddressRegionKHR raygen;
    VkStridedDeviceAddressRegionKHR miss;
    VkStridedDeviceAddressRegionKHR hit;
    VkStridedDeviceAddressRegionKHR callable;
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
    void updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                   VkQueue graphicsQueue,
                   const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                   const std::vector<DimensionState>& dimensionCache,
                   uint32_t transferQueueFamily);

    void createDescriptorSetLayout();
    void createDescriptorPoolAndSet();
    void createRayTracingPipeline(uint32_t maxRayRecursionDepth);
    void createShaderBindingTable(VkPhysicalDevice physicalDevice);
    void createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                             VkQueue queue,
                             const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                             uint32_t transferQueueFamily = VK_QUEUE_FAMILY_IGNORED);
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
    VkDescriptorSet getDescriptorSet() const { return ds_; }
    VkPipeline getPipeline() const { return rtPipeline_; }
    const ShaderBindingTable& getSBT() const { return sbt_; }
    VkDescriptorSetLayout getDescriptorSetLayout() const { return dsLayout_; }

    VkBuffer       getSBTBuffer() const { return sbtBuffer_; }
    VkDeviceMemory getSBTMemory() const { return sbtMemory_; }

    VkAccelerationStructureKHR getBLAS() const { return blas_; }
    VkAccelerationStructureKHR getTLAS() const { return tlas_; }

    void traceRays(VkCommandBuffer cmd, const VkStridedDeviceAddressRegionKHR* raygen,
                   const VkStridedDeviceAddressRegionKHR* miss, const VkStridedDeviceAddressRegionKHR* hit,
                   const VkStridedDeviceAddressRegionKHR* callable, uint32_t width, uint32_t height, uint32_t depth) const {
        if (vkCmdTraceRaysKHR) {
            vkCmdTraceRaysKHR(cmd, raygen, miss, hit, callable, width, height, depth);
        } else {
            throw std::runtime_error("vkCmdTraceRaysKHR not loaded");
        }
    }

    void setRayTracingPipeline(VkPipeline pipeline, VkPipelineLayout layout);

    // dispatchRenderMode IS NOW INSIDE THE CLASS
    void dispatchRenderMode(
        uint32_t imageIndex,
        VkBuffer vertexBuffer,
        VkCommandBuffer cmd,
        VkBuffer indexBuffer,
        float zoom,
        int width,
        int height,
        float wavePhase,
        VkPipelineLayout layout,
        VkDescriptorSet ds,
        VkDevice device,
        VkDeviceMemory uniformMem,
        VkPipeline pipeline,
        float deltaTime,
        VkRenderPass renderPass,
        VkFramebuffer fb,
        const Vulkan::Context& ctx,
        int mode
    );

private:
    VkCommandBuffer allocateTransientCommandBuffer(VkCommandPool commandPool);
    void submitAndWaitTransient(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool);
    void uploadBlackPixelToImage(VkImage image);
    void createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size,
                      VkBufferUsageFlags usage, VkMemoryPropertyFlags properties,
                      VkBuffer& buffer, VkDeviceMemory& memory);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                            VkMemoryPropertyFlags properties);
    void compactAccelerationStructures(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue);
    void createStorageImage(VkPhysicalDevice physicalDevice, VkExtent2D extent,
                            VkImage& image, VkImageView& imageView, VkDeviceMemory& memory);

    void cleanupBLASResources(VkBuffer asBuffer, VkDeviceMemory asMemory,
                              VkBuffer scratchBuffer, VkDeviceMemory scratchMemory);

    std::shared_ptr<Vulkan::Context> context_;
    VulkanPipelineManager* pipelineMgr_ = nullptr;
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkExtent2D extent_{};

    // RAW HANDLES – NO Dispose::VulkanHandle
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

    // CORE DESCRIPTOR FUNCTION POINTERS
    PFN_vkCreateDescriptorSetLayout   vkCreateDescriptorSetLayout   = nullptr;
    PFN_vkAllocateDescriptorSets      vkAllocateDescriptorSets      = nullptr;
    PFN_vkCreateDescriptorPool        vkCreateDescriptorPool        = nullptr;
    PFN_vkDestroyDescriptorSetLayout  vkDestroyDescriptorSetLayout  = nullptr;
    PFN_vkDestroyDescriptorPool       vkDestroyDescriptorPool       = nullptr;
    PFN_vkFreeDescriptorSets          vkFreeDescriptorSets          = nullptr;

    // Transient fence
    VkFence transientFence_ = VK_NULL_HANDLE;
    bool deviceLost_ = false;

    // === NEW: Frame and time tracking for ray tracing ===
    uint32_t frameNumber_ = 0;
    float time_ = 0.0f;
};

} // namespace VulkanRTX