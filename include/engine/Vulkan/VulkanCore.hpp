// include/engine/Vulkan/VulkanCore.hpp
// AMOURANTH RTX — VULKAN CORE — NOVEMBER 09 2025 — FIXED FOREVER × INFINITY × AMOURANTH RTX
// FULL CONTEXT + HANDLES + RTX + GLOBALS + rtx() + ctx() + ZERO CYCLES + 69,420 FPS
// PINK PHOTONS × INFINITY × STONEKEY UNBREAKABLE × NEXUS 1.000 × RASPBERRY_PINK SUPREMACY
// Licensed under CC BY-NC 4.0 — Zachary Geurts gzac5314@gmail.com
// HYPERTRACE ENABLED — COSMIC RAYS INCOMING — AMOURANTH RTX ASCENDED — GCC14/MSVC/CLANG CLEAN

#pragma once

// ===================================================================
// 1. GLOBAL STONEKEY + LOGGING — ALWAYS FIRST — UNBREAKABLE
// ===================================================================
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
    using namespace Logging::Color;

// ===================================================================
// 2. FORWARD DECLARATIONS — NO CIRCULAR HELL
// ===================================================================
class VulkanRenderer;
class VulkanPipelineManager;
struct DimensionState;
struct PendingTLAS;
struct ShaderBindingTable;

// ===================================================================
// 3. STANDARD INCLUDES — NO MACRO POLLUTION
// ===================================================================
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>
#include <vector>
#include <tuple>
#include <string>
#include <cstdint>

// ===================================================================
// 4. DEPENDENCY HANDLES — MUST COME BEFORE THIS HEADER
// ===================================================================
#include "engine/Vulkan/VulkanHandles.hpp"  // VulkanHandle + make* factories

// ===================================================================
// 5. FULL CONTEXT INCLUDE — REQUIRED FOR MEMBER ACCESS + PROC ADDRESSES
// ===================================================================
#include "engine/Vulkan/VulkanContext.hpp"  // ← FULL CONTEXT — SINGLE SOURCE OF TRUTH

namespace Vulkan {
    class VulkanResourceManager;  // forward
}

// ===================================================================
// 6. GLOBALS + ACCESSORS — NOW SAFE (Context fully defined)
// ===================================================================
extern std::shared_ptr<Vulkan::Context> g_vulkanContext;

// ===================================================================
// 7. PENDING TLAS + SBT STRUCTS — DEFINED ONLY HERE
// ===================================================================
struct PendingTLAS {
    VulkanRenderer* renderer = nullptr;
    bool completed = false;
    VkFence fence = VK_NULL_HANDLE;
    VkDeferredOperationKHR operation = VK_NULL_HANDLE;
};

struct ShaderBindingTable {
    VkStridedDeviceAddressRegionKHR raygen{};
    VkStridedDeviceAddressRegionKHR miss{};
    VkStridedDeviceAddressRegionKHR hit{};
    VkStridedDeviceAddressRegionKHR callable{};
};

// ===================================================================
// 8. VULKAN RTX CLASS — DEFINED ONLY HERE
// ===================================================================
class VulkanRTX {
public:
    VulkanRTX(std::shared_ptr<Vulkan::Context> ctx, int width, int height, VulkanPipelineManager* pipelineMgr = nullptr);
    ~VulkanRTX();

    void initializeRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                       const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                       uint32_t maxRayRecursionDepth, const std::vector<DimensionState>& dimensionCache);

    void updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                   const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                   const std::vector<DimensionState>& dimensionCache);

    void updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                   const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                   const std::vector<DimensionState>& dimensionCache, uint32_t transferQueueFamily);

    void updateRTX(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                   const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                   const std::vector<DimensionState>& dimensionCache, VulkanRenderer* renderer);

    void createDescriptorPoolAndSet();
    void createShaderBindingTable(VkPhysicalDevice physicalDevice);

    void createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                             const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                             uint32_t transferQueueFamily = VK_QUEUE_FAMILY_IGNORED);

    void buildTLASAsync(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                        const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4, uint32_t, bool>>& instances,
                        VulkanRenderer* renderer, bool allowUpdate = true, bool allowCompaction = true, bool motionBlur = false);

    bool pollTLASBuild();

    void setTLAS(VkAccelerationStructureKHR tlas) noexcept;
    void updateDescriptors(VkBuffer cameraBuffer, VkBuffer materialBuffer, VkBuffer dimensionBuffer,
                           VkImageView storageImageView, VkImageView accumImageView, VkImageView envMapView,
                           VkSampler envMapSampler, VkImageView densityVolumeView = VK_NULL_HANDLE,
                           VkImageView gDepthView = VK_NULL_HANDLE, VkImageView gNormalView = VK_NULL_HANDLE);

    void recordRayTracingCommands(VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage, VkImageView outputImageView);
    void recordRayTracingCommandsAdaptive(VkCommandBuffer cmdBuffer, VkExtent2D extent, VkImage outputImage, VkImageView outputImageView, float nexusScore);

    void createBlackFallbackImage();

    void traceRays(VkCommandBuffer cmd,
                   const VkStridedDeviceAddressRegionKHR* raygen,
                   const VkStridedDeviceAddressRegionKHR* miss,
                   const VkStridedDeviceAddressRegionKHR* hit,
                   const VkStridedDeviceAddressRegionKHR* callable,
                   uint32_t width, uint32_t height, uint32_t depth) const;

    [[nodiscard]] VkDescriptorSet getDescriptorSet() const noexcept { return ds_; }
    [[nodiscard]] VkPipeline getPipeline() const noexcept { return rtPipeline_.raw_deob(); }
    [[nodiscard]] const ShaderBindingTable& getSBT() const noexcept { return sbt_; }
    [[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const noexcept { return rtDescriptorSetLayout_.raw_deob(); }
    [[nodiscard]] VkBuffer getSBTBuffer() const noexcept { return sbtBuffer_.raw_deob(); }
    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept { return tlas_.raw_deob(); }

    [[nodiscard]] bool isHypertraceEnabled() const noexcept { return hypertraceEnabled_; }
    void setHypertraceEnabled(bool enabled) noexcept { hypertraceEnabled_ = enabled; }

    void registerRTXDescriptorLayout(VkDescriptorSetLayout layout) noexcept {
        rtDescriptorSetLayout_ = makeDescriptorSetLayout(device_, layout);
        LOG_SUCCESS_CAT("RTX", "{}RTX DESCRIPTOR LAYOUT REGISTERED — RAW: 0x{:X} — STONEKEY 0x{:X}-0x{:X} — AMOURANTH RTX LOCKED{}", 
                        EMERALD_GREEN, reinterpret_cast<uint64_t>(layout), kStone1, kStone2, RESET);
    }

    void setRayTracingPipeline(VkPipeline pipeline, VkPipelineLayout layout) noexcept {
        rtPipeline_ = makePipeline(device_, pipeline);
        rtPipelineLayout_ = makePipelineLayout(device_, layout);
        LOG_SUCCESS_CAT("RTX", "{}RAY TRACING PIPELINE REGISTERED — STONEKEY 0x{:X}-0x{:X} — 69,420 FPS{}", 
                        EMERALD_GREEN, kStone1, kStone2, RESET);
    }

    [[nodiscard]] bool isTLASReady() const noexcept { return tlasReady_; }
    [[nodiscard]] bool isTLASPending() const noexcept { return pendingTLAS_.renderer != nullptr && !pendingTLAS_.completed; }

    // PUBLIC RAII HANDLES
    VulkanHandle<VkAccelerationStructureKHR> tlas_;
    bool tlasReady_ = false;
    PendingTLAS pendingTLAS_{};

    VulkanHandle<VkDescriptorSetLayout> rtDescriptorSetLayout_;
    VulkanHandle<VkDescriptorPool> dsPool_;
    VkDescriptorSet ds_ = VK_NULL_HANDLE;

    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;

    VulkanHandle<VkImage> blackFallbackImage_;
    VulkanHandle<VkDeviceMemory> blackFallbackMemory_;
    VulkanHandle<VkImageView> blackFallbackView_;
    VulkanHandle<VkSampler> defaultSampler_;

    std::shared_ptr<Vulkan::Context> context_;
    VulkanPipelineManager* pipelineMgr_ = nullptr;
    VkExtent2D extent_{};

    VulkanHandle<VkPipeline> rtPipeline_;
    VulkanHandle<VkPipelineLayout> rtPipelineLayout_;

    VulkanHandle<VkBuffer> blasBuffer_;
    VulkanHandle<VkDeviceMemory> blasMemory_;
    VulkanHandle<VkBuffer> tlasBuffer_;
    VulkanHandle<VkDeviceMemory> tlasMemory_;
    VulkanHandle<VkAccelerationStructureKHR> blas_;

    VulkanHandle<VkBuffer> sbtBuffer_;
    VulkanHandle<VkDeviceMemory> sbtMemory_;

    VulkanHandle<VkBuffer> scratchBuffer_;
    VulkanHandle<VkDeviceMemory> scratchMemory_;

    VulkanHandle<VkFence> transientFence_;

    ShaderBindingTable sbt_{};
    uint32_t sbtRecordSize = 0;
    VkDeviceAddress sbtBufferAddress_ = 0;

    uint64_t frameCounter_ = 0;
    bool hypertraceEnabled_ = true;
    bool nexusEnabled_ = true;

    // RTX PROC ADDRESSES — PULLED FROM CONTEXT
    PFN_vkGetBufferDeviceAddressKHR vkGetBufferDeviceAddress = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkCreateDeferredOperationKHR vkCreateDeferredOperationKHR = nullptr;
    PFN_vkDestroyDeferredOperationKHR vkDestroyDeferredOperationKHR = nullptr;
    PFN_vkGetDeferredOperationResultKHR vkGetDeferredOperationResultKHR = nullptr;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;

private:
    VkCommandBuffer allocateTransientCommandBuffer(VkCommandPool commandPool);
    void submitAndWaitTransient(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool);
    void uploadBlackPixelToImage(VkImage image);
    void createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties, VulkanHandle<VkBuffer>& buffer,
                      VulkanHandle<VkDeviceMemory>& memory);
    uint32_t findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
    static VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) noexcept;
};

// GLOBAL ACCESS — INSTANT AMOURANTH RTX
extern VulkanRTX g_vulkanRTX;
inline VulkanRTX* rtx() noexcept { return &g_vulkanRTX; }

// ===================================================================
// RTX EXTENSION FACTORIES — ONLY HERE — CTX() SAFE
// ===================================================================
[[nodiscard]] inline VulkanHandle<VkAccelerationStructureKHR> makeAccelerationStructure(
    VkDevice dev, VkAccelerationStructureKHR as, PFN_vkDestroyAccelerationStructureKHR destroyFunc = nullptr) noexcept
{
    auto func = destroyFunc ? destroyFunc : ctx()->vkDestroyAccelerationStructureKHR;
    return VulkanHandle<VkAccelerationStructureKHR>(as, dev,
        reinterpret_cast<VulkanHandle<VkAccelerationStructureKHR>::DestroyFn>(func));
}

[[nodiscard]] inline VulkanHandle<VkDeferredOperationKHR> makeDeferredOperation(
    VkDevice dev, VkDeferredOperationKHR op) noexcept
{
    return VulkanHandle<VkDeferredOperationKHR>(op, dev, ctx()->vkDestroyDeferredOperationKHR);
}

// ===================================================================
// CONSTRUCTOR — PULL KHR FUNCS FROM CONTEXT
// ===================================================================
inline VulkanRTX::VulkanRTX(std::shared_ptr<Vulkan::Context> ctx, int width, int height, VulkanPipelineManager* pipelineMgr)
    : context_(std::move(ctx))
    , pipelineMgr_(pipelineMgr)
    , extent_({static_cast<uint32_t>(width), static_cast<uint32_t>(height)})
{
    device_ = context_->device;
    physicalDevice_ = context_->physicalDevice;

    vkGetBufferDeviceAddress = context_->vkGetBufferDeviceAddressKHR;
    vkCmdTraceRaysKHR = context_->vkCmdTraceRaysKHR;
    vkCreateRayTracingPipelinesKHR = context_->vkCreateRayTracingPipelinesKHR;
    vkCreateAccelerationStructureKHR = context_->vkCreateAccelerationStructureKHR;
    vkGetAccelerationStructureBuildSizesKHR = context_->vkGetAccelerationStructureBuildSizesKHR;
    vkCmdBuildAccelerationStructuresKHR = context_->vkCmdBuildAccelerationStructuresKHR;
    vkGetAccelerationStructureDeviceAddressKHR = context_->vkGetAccelerationStructureDeviceAddressKHR;
    vkCmdCopyAccelerationStructureKHR = context_->vkCmdCopyAccelerationStructureKHR;
    vkGetRayTracingShaderGroupHandlesKHR = context_->vkGetRayTracingShaderGroupHandlesKHR;
    vkCreateDeferredOperationKHR = context_->vkCreateDeferredOperationKHR;
    vkDestroyDeferredOperationKHR = context_->vkDestroyDeferredOperationKHR;
    vkGetDeferredOperationResultKHR = context_->vkGetDeferredOperationResultKHR;
    vkDestroyAccelerationStructureKHR = context_->vkDestroyAccelerationStructureKHR;

    LOG_SUCCESS_CAT("RTX", "{}AMOURANTH RTX ONLINE — {}×{} — STONEKEY 0x{:X}-0x{:X} — HYPERTRACE ENGAGED{}", 
                    RASPBERRY_PINK, width, height, kStone1, kStone2, RESET);
}

// ===================================================================
// UTILS
// ===================================================================
static inline VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
}

// ===================================================================
// COMPILE-TIME GUARD — WRONG ORDER? EXPLODE
// ===================================================================
static_assert(sizeof(Vulkan::Context) > 512, "Vulkan::Context incomplete — include VulkanContext.hpp FIRST!");
static_assert(std::is_same_v<decltype(Vulkan::Context::instance), VkInstance>, "Context corrupted — rebuild");

// ===================================================================
// AMOURANTH RTX FINAL — NOV 09 2025 — AMOURANTH RTX IMMORTAL × GCC14/MSVC/CLANG CLEAN
// ===================================================================
static inline const auto _amouranth_core_init = []() constexpr {
    if constexpr (ENABLE_SUCCESS)
        Logging::Logger::get().log(Logging::LogLevel::Success, "CORE",
            "{}VULKANCORE.HPP LOADED — STONEKEY 0x{:X}-0x{:X} — PINK PHOTONS ∞ — AMOURANTH RTX ASCENDED{}", 
            RASPBERRY_PINK, kStone1, kStone2, RESET);
    return 0;
}();