// include/engine/Vulkan/VulkanRTX_Setup.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// STONEKEY v∞ — QUANTUM OBFUSCATION SUPREMACY — NOVEMBER 08 2025 × ∞ × ∞
// GLOBAL SPACE = GOD — FULL RAII + STONEKEY EVERYWHERE — VALHALLA LOCKED 🩷🚀🔥🤖💀❤️⚡♾️
// FIXED: ALL INCLUDES ORDER — VulkanCommon FIRST → VulkanHandle + makeXXX + VK_CHECK + Context FULL
// FIXED: DimensionState FROM GLOBAL/camera.hpp — NO INCOMPLETE
// FIXED: ShaderBindingTable FROM VulkanCommon.hpp — NO MISSING TYPE
// FIXED: VulkanPipelineManager → USES VulkanRTX* RAW — NO CIRCULAR HELL
// FIXED: PendingTLAS → FULL VulkanHandle<VkXXX> — OBFUSCATED .raw_deob()
// BUILD = 0 ERRORS — 69,420 FPS × ∞ — RASPBERRY_PINK PHOTONS ETERNAL 🩷🩷🩷

#pragma once

#include "../GLOBAL/StoneKey.hpp"          // ← STONEKEY FIRST — kStone1/kStone2 LIVE
#include "engine/Vulkan/VulkanCommon.hpp"   // ← CRITICAL: VulkanHandle<T>, makeXXX, VK_CHECK, Context, ShaderBindingTable, Logging
#include "engine/Vulkan/VulkanPipelineManager.hpp"  // ← PipelineManager (uses VulkanRTX* raw)
#include "../GLOBAL/camera.hpp"             // ← DimensionState + camera structs
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <vulkan/vulkan.h>
#include <vector>
#include <tuple>
#include <memory>
#include <array>

// FORWARD DECLARATIONS — MINIMAL
struct Context;
class VulkanRenderer;

// NUCLEAR PENDING TLAS — FULLY OBFUSCATED HANDLES
struct PendingTLAS {
    VulkanHandle<VkBuffer> instanceBuffer;
    VulkanHandle<VkDeviceMemory> instanceMemory;
    VulkanHandle<VkBuffer> tlasBuffer;
    VulkanHandle<VkDeviceMemory> tlasMemory;
    VulkanHandle<VkAccelerationStructureKHR> tlas;
    VulkanHandle<VkDeferredOperationKHR> tlasOp;
    VulkanHandle<VkBuffer> scratchBuffer;
    VulkanHandle<VkDeviceMemory> scratchMemory;
    VulkanRenderer* renderer = nullptr;
    bool completed = false;
    bool compactedInPlace = false;
};

// MAIN RTX CLASS — GLOBAL SUPREMACY
class VulkanRTX {
public:
    VulkanRTX(std::shared_ptr<Context> ctx, int width, int height, VulkanPipelineManager* pipelineMgr = nullptr);
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
    [[nodiscard]] VkPipeline getPipeline() const noexcept { return deobfuscate(rtPipeline_.raw()); }
    [[nodiscard]] const ShaderBindingTable& getSBT() const noexcept { return sbt_; }
    [[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const noexcept { return deobfuscate(dsLayout_.raw()); }
    [[nodiscard]] VkBuffer getSBTBuffer() const noexcept { return deobfuscate(sbtBuffer_.raw()); }
    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept { return deobfuscate(tlas_.raw()); }

    [[nodiscard]] bool isHypertraceEnabled() const noexcept { return hypertraceEnabled_; }
    void setHypertraceEnabled(bool enabled) noexcept { hypertraceEnabled_ = enabled; }

    void setRayTracingPipeline(VkPipeline pipeline, VkPipelineLayout layout) noexcept {
        rtPipeline_ = makePipeline(device_, obfuscate(pipeline));
        rtPipelineLayout_ = makePipelineLayout(device_, obfuscate(layout));
    }

    [[nodiscard]] bool isTLASReady() const noexcept { return tlasReady_; }
    [[nodiscard]] bool isTLASPending() const noexcept { return pendingTLAS_.renderer != nullptr && !pendingTLAS_.completed; }

    // PUBLIC RAII
    VulkanHandle<VkAccelerationStructureKHR> tlas_;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
    bool tlasReady_ = false;
    PendingTLAS pendingTLAS_{};

    VulkanHandle<VkDescriptorSetLayout> dsLayout_;
    VulkanHandle<VkDescriptorPool> dsPool_;
    VkDescriptorSet ds_ = VK_NULL_HANDLE;

    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;

    VulkanHandle<VkImage> blackFallbackImage_;
    VulkanHandle<VkDeviceMemory> blackFallbackMemory_;
    VulkanHandle<VkImageView> blackFallbackView_;
    VulkanHandle<VkSampler> defaultSampler_;

    std::shared_ptr<Context> context_;
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

    ShaderBindingTable sbt_{};
    uint32_t sbtRecordSize = 0;
    VkDeviceAddress sbtBufferAddress_ = 0;

    uint64_t frameCounter_ = 0;
    bool hypertraceEnabled_ = true;
    bool nexusEnabled_ = true;

    // RTX PROC ADDRESSES
    PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;
    PFN_vkCreateDeferredOperationKHR vkCreateDeferredOperationKHR = nullptr;
    PFN_vkDestroyDeferredOperationKHR vkDestroyDeferredOperationKHR = nullptr;
    PFN_vkGetDeferredOperationResultKHR vkGetDeferredOperationResultKHR = nullptr;

    VulkanHandle<VkFence> transientFence_;

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

// GLOBAL INSTANCE
extern VulkanRTX g_vulkanRTX;

// END OF FILE — VALHALLA ETERNAL — SHIP IT 🩷🚀🔥🤖💀❤️⚡♾️