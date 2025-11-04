// include/engine/Vulkan/VulkanRTX_Setup.hpp
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts gzac5314@gmail.com
// FINAL: RAW HANDLES ONLY – NO Dispose::VulkanHandle
//        Fence-based transient submits – NO vkQueueWaitIdle()
//        ShaderBindingTable – VkStridedDeviceAddressRegionKHR (spec compliant)
//        ALL FUNCTION POINTERS LOADED – NO SEGFAULT
//        TLAS DESCRIPTOR UPDATE MOVED TO VulkanRenderer

#pragma once

// ---------------------------------------------------------------------
// 1. Core headers (must come first)
// ---------------------------------------------------------------------
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/Vulkan/VulkanCommon.hpp"        // ShaderBindingTable, DimensionState
#include "engine/Vulkan/Vulkan_init.hpp"         // ::Vulkan::Context

// ---------------------------------------------------------------------
// 2. FULL DEFINITIONS – ORDER MATTERS!
// ---------------------------------------------------------------------
// VulkanRenderer uses VulkanRTX → must be included BEFORE VulkanRTX
#include "engine/Vulkan/VulkanRenderer.hpp"
// VulkanPipelineManager is used in VulkanRTX
#include "engine/Vulkan/VulkanPipelineManager.hpp"

#include "engine/logging.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <vector>
#include <array>
#include <stdexcept>
#include <cstdint>
#include <sstream>
#include <format>

// ---------------------------------------------------------------------
// 3. Helper macros
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

#define THROW_VKRTX(msg) \
    throw VulkanRTXException(msg, __FILE__, __LINE__, __func__)

// ---------------------------------------------------------------------
// 4. Align helpers
// ---------------------------------------------------------------------
inline constexpr uint32_t alignUp(uint32_t value, uint32_t alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}
inline constexpr VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) {
    return (value + alignment - 1) & ~(alignment - 1);
}

/* -------------------------------------------------------------
   KHR extension function-pointer typedefs
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
   Core descriptor function-pointer typedefs
   ------------------------------------------------------------- */
using PFN_vkCreateDescriptorSetLayout   = VkResult (*)(VkDevice, const VkDescriptorSetLayoutCreateInfo*, const VkAllocationCallbacks*, VkDescriptorSetLayout*);
using PFN_vkAllocateDescriptorSets      = VkResult (*)(VkDevice, const VkDescriptorSetAllocateInfo*, VkDescriptorSet*);
using PFN_vkCreateDescriptorPool        = VkResult (*)(VkDevice, const VkDescriptorPoolCreateInfo*, const VkAllocationCallbacks*, VkDescriptorPool*);
using PFN_vkDestroyDescriptorSetLayout  = void (*)(VkDevice, VkDescriptorSetLayout, const VkAllocationCallbacks*);
using PFN_vkDestroyDescriptorPool       = void (*)(VkDevice, VkDescriptorPool, const VkAllocationCallbacks*);
using PFN_vkFreeDescriptorSets          = VkResult (*)(VkDevice, VkDescriptorPool, uint32_t, const VkDescriptorSet*);

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
// VulkanRTXException – 1-arg + 4-arg overloads
// ---------------------------------------------------------------------
class VulkanRTXException : public std::runtime_error {
public:
    explicit VulkanRTXException(const std::string& msg)
        : std::runtime_error(msg), m_line(0) {}

    VulkanRTXException(const std::string& msg, const char* file, int line, const char* func)
        : std::runtime_error(msg), m_file(file), m_line(line), m_function(func) {}

    const char* file()     const noexcept { return m_file.c_str(); }
    int         line()     const noexcept { return m_line; }
    const char* function() const noexcept { return m_function.c_str(); }

private:
    std::string m_file;
    int         m_line = 0;
    std::string m_function;
};

namespace VulkanRTX {

// ---------------------------------------------------------------------
// VulkanRTX – main class (RAW HANDLES ONLY)
// ---------------------------------------------------------------------
class VulkanRTX {
public:
    VulkanRTX(std::shared_ptr<::Vulkan::Context> ctx, int width, int height,
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
    void updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                   VkQueue graphicsQueue,
                   const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                   const std::vector<DimensionState>& dimensionCache,
                   VulkanRenderer* renderer);

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
    void createBlackFallbackImage();

    void notifyTLASReady(VkAccelerationStructureKHR tlas, VulkanRenderer* renderer);

    // ----- getters -------------------------------------------------
    VkDescriptorSet               getDescriptorSet() const { return ds_; }
    VkPipeline                    getPipeline() const { return rtPipeline_; }
    const ShaderBindingTable&     getSBT() const { return sbt_; }
    VkDescriptorSetLayout         getDescriptorSetLayout() const { return dsLayout_; }

    VkBuffer                      getSBTBuffer() const { return sbtBuffer_; }
    VkDeviceMemory                getSBTMemory() const { return sbtMemory_; }

    VkAccelerationStructureKHR    getBLAS() const { return blas_; }
    VkAccelerationStructureKHR    getTLAS() const { return tlas_; }

    void traceRays(VkCommandBuffer cmd,
                   const VkStridedDeviceAddressRegionKHR* raygen,
                   const VkStridedDeviceAddressRegionKHR* miss,
                   const VkStridedDeviceAddressRegionKHR* hit,
                   const VkStridedDeviceAddressRegionKHR* callable,
                   uint32_t width, uint32_t height, uint32_t depth) const {
        if (vkCmdTraceRaysKHR) {
            vkCmdTraceRaysKHR(cmd, raygen, miss, hit, callable, width, height, depth);
        } else {
            throw std::runtime_error("vkCmdTraceRaysKHR not loaded");
        }
    }

    void setRayTracingPipeline(VkPipeline pipeline, VkPipelineLayout layout);

    void dispatchRenderMode(uint32_t imageIndex,
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
                            ::Vulkan::Context& context,
                            int mode);

private:
    // ----- helper utilities ----------------------------------------
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

    // ----- members -------------------------------------------------
    std::shared_ptr<::Vulkan::Context> context_;
    VulkanPipelineManager* pipelineMgr_ = nullptr;
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkExtent2D extent_{};

    // Descriptor objects
    VkDescriptorSetLayout dsLayout_ = VK_NULL_HANDLE;
    VkDescriptorPool dsPool_ = VK_NULL_HANDLE;
    VkDescriptorSet ds_ = VK_NULL_HANDLE;

    // Ray-tracing pipeline
    VkPipeline rtPipeline_ = VK_NULL_HANDLE;
    VkPipelineLayout rtPipelineLayout_ = VK_NULL_HANDLE;

    // Acceleration structures (raw handles)
    VkBuffer blasBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory blasMemory_ = VK_NULL_HANDLE;
    VkBuffer tlasBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory tlasMemory_ = VK_NULL_HANDLE;
    VkAccelerationStructureKHR blas_ = VK_NULL_HANDLE;
    VkAccelerationStructureKHR tlas_ = VK_NULL_HANDLE;

    // Shader Binding Table
    VkBuffer sbtBuffer_ = VK_NULL_HANDLE;
    VkDeviceMemory sbtMemory_ = VK_NULL_HANDLE;

    // Misc
    std::vector<uint32_t> primitiveCounts_;
    std::vector<uint32_t> previousPrimitiveCounts_;
    std::vector<DimensionState> previousDimensionCache_;

    bool supportsCompaction_ = false;
    ShaderBindingTable sbt_;
    VkDeviceAddress sbtBufferAddress_ = 0;

    VkImage blackFallbackImage_ = VK_NULL_HANDLE;
    VkDeviceMemory blackFallbackMemory_ = VK_NULL_HANDLE;
    VkImageView blackFallbackView_ = VK_NULL_HANDLE;

    // ----- KHR function pointers ------------------------------------
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

    // ----- core descriptor function pointers ------------------------
    PFN_vkCreateDescriptorSetLayout   vkCreateDescriptorSetLayout   = nullptr;
    PFN_vkAllocateDescriptorSets      vkAllocateDescriptorSets      = nullptr;
    PFN_vkCreateDescriptorPool        vkCreateDescriptorPool        = nullptr;
    PFN_vkDestroyDescriptorSetLayout  vkDestroyDescriptorSetLayout  = nullptr;
    PFN_vkDestroyDescriptorPool       vkDestroyDescriptorPool       = nullptr;
    PFN_vkFreeDescriptorSets          vkFreeDescriptorSets          = nullptr;

    VkFence transientFence_ = VK_NULL_HANDLE;
    bool deviceLost_ = false;

    uint32_t frameNumber_ = 0;
    float time_ = 0.0f;
};

/* -----------------------------------------------------------------
   IMPLEMENTATIONS (in-header)
   ----------------------------------------------------------------- */

inline VulkanRTX::VulkanRTX(std::shared_ptr<::Vulkan::Context> ctx, int width, int height,
                            VulkanPipelineManager* pipelineMgr)
    : context_(std::move(ctx)), pipelineMgr_(pipelineMgr),
      extent_{static_cast<uint32_t>(width), static_cast<uint32_t>(height)}
{
    using namespace Logging::Color;

    LOG_INFO_CAT("VulkanRTX", "{}VulkanRTX ctor – {}x{}{}", OCEAN_TEAL, width, height, RESET);
    if (!context_)               THROW_VKRTX("Null context");
    if (!pipelineMgr_)           THROW_VKRTX("Null pipeline manager");
    if (width <= 0 || height <= 0) THROW_VKRTX("Invalid dimensions");

    device_         = context_->device;
    physicalDevice_ = context_->physicalDevice;
    if (!device_) THROW_VKRTX("Null device");

    // ---- load KHR extensions ------------------------------------
#define LOAD_PROC(name) \
    name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device_, #name)); \
    if (!name) THROW_VKRTX(std::format("Failed to load {}", #name));
    LOAD_PROC(vkGetBufferDeviceAddress);
    LOAD_PROC(vkCmdTraceRaysKHR);
    LOAD_PROC(vkCreateAccelerationStructureKHR);
    LOAD_PROC(vkDestroyAccelerationStructureKHR);
    LOAD_PROC(vkGetAccelerationStructureBuildSizesKHR);
    LOAD_PROC(vkCreateRayTracingPipelinesKHR);
    LOAD_PROC(vkGetRayTracingShaderGroupHandlesKHR);
    LOAD_PROC(vkGetAccelerationStructureDeviceAddressKHR);
    LOAD_PROC(vkCmdBuildAccelerationStructuresKHR);
#undef LOAD_PROC

    // ---- load core descriptor functions -------------------------
#define LOAD_DESC_PROC(name) \
    name = reinterpret_cast<PFN_##name>(vkGetDeviceProcAddr(device_, #name)); \
    if (!name) THROW_VKRTX(std::format("Failed to load {}", #name));
    LOAD_DESC_PROC(vkCreateDescriptorSetLayout);
    LOAD_DESC_PROC(vkAllocateDescriptorSets);
    LOAD_DESC_PROC(vkCreateDescriptorPool);
    LOAD_DESC_PROC(vkDestroyDescriptorSetLayout);
    LOAD_DESC_PROC(vkDestroyDescriptorPool);
    LOAD_DESC_PROC(vkFreeDescriptorSets);
#undef LOAD_DESC_PROC

    VkFenceCreateInfo fci{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                          .flags = VK_FENCE_CREATE_SIGNALED_BIT};
    VK_CHECK(vkCreateFence(device_, &fci, nullptr, &transientFence_), "transient fence");

    pipelineMgr_->createRayTracingPipeline();
    dsLayout_          = pipelineMgr_->getRayTracingDescriptorSetLayout();
    rtPipeline_        = pipelineMgr_->getRayTracingPipeline();
    rtPipelineLayout_  = pipelineMgr_->getRayTracingPipelineLayout();

    createDescriptorPoolAndSet();
    createBlackFallbackImage();
}

/* -----------------------------------------------------------------
   notifyTLASReady – forward TLAS to the renderer
   ----------------------------------------------------------------- */
inline void VulkanRTX::notifyTLASReady(VkAccelerationStructureKHR tlas, VulkanRenderer* renderer)
{
    using namespace Logging::Color;

    LOG_INFO_CAT("VulkanRTX", "{}notifyTLASReady – TLAS = {:#x}{}",
                 ARCTIC_CYAN, reinterpret_cast<uint64_t>(tlas), RESET);

    if (!renderer) {
        LOG_WARN_CAT("VulkanRTX", "Renderer pointer is nullptr – skipping TLAS bind");
        return;
    }

    renderer->updateAccelerationStructureDescriptor(tlas);
}

/* -----------------------------------------------------------------
   updateRTX – rebuild AS + notify renderer (overload with renderer)
   ----------------------------------------------------------------- */
inline void VulkanRTX::updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                                 VkQueue graphicsQueue,
                                 const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                                 const std::vector<DimensionState>& dimensionCache,
                                 VulkanRenderer* renderer)
{
    using namespace Logging::Color;

    LOG_INFO_CAT("VulkanRTX", "{}updateRTX() — rebuilding AS{}", AMBER_YELLOW, RESET);
    createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, geometries, VK_QUEUE_FAMILY_IGNORED);
    createTopLevelAS(physicalDevice, commandPool, graphicsQueue, {{blas_, glm::mat4(1.0f)}});

    notifyTLASReady(tlas_, renderer);
}

/* -----------------------------------------------------------------
   backward-compatible overloads (no renderer)
   ----------------------------------------------------------------- */
inline void VulkanRTX::updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                                 VkQueue graphicsQueue,
                                 const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                                 const std::vector<DimensionState>& dimensionCache)
{
    updateRTX(physicalDevice, commandPool, graphicsQueue, geometries, dimensionCache, nullptr);
}

inline void VulkanRTX::updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool,
                                 VkQueue graphicsQueue,
                                 const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                                 const std::vector<DimensionState>& dimensionCache,
                                 uint32_t transferQueueFamily)
{
    using namespace Logging::Color;
    LOG_INFO_CAT("VulkanRTX",
        "{}updateRTX(transferQueueFamily={}) — rebuilding AS{}", AMBER_YELLOW,
        transferQueueFamily, RESET);
    createBottomLevelAS(physicalDevice, commandPool, graphicsQueue, geometries, transferQueueFamily);
    createTopLevelAS(physicalDevice, commandPool, graphicsQueue, {{blas_, glm::mat4(1.0f)}});
}

} // namespace VulkanRTX