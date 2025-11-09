// include/engine/Vulkan/VulkanCore.hpp
// AMOURANTH RTX — VULKAN CORE — NOVEMBER 09 2025 — CLEAN + LAS SEPARATE + ZERO ERRORS
// PINK PHOTONS × INFINITY — STONEKEY UNBREAKABLE — TLAS VIA Vulkan_LAS — SUPREMACY

#pragma once

// ===================================================================
// 1. GLOBAL + LOGGING — UNBREAKABLE
// ===================================================================
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
using namespace Logging::Color;
#include "engine/Vulkan/VulkanHandles.hpp"
#include "engine/Vulkan/Vulkan_LAS.hpp"      // ← SEPARATE LAS — STONEKEYED GOD CLASS

// ===================================================================
// 2. FORWARD DECLS
// ===================================================================
class VulkanRenderer;
class VulkanPipelineManager;
struct DimensionState;

// ===================================================================
// 3. VULKAN + STD
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
// 5. GLOBALS
// ===================================================================
extern std::shared_ptr<Vulkan::Context> g_vulkanContext;

// ===================================================================
// 6. PENDINGTLAS — NOW FROM LAS (PUBLIC THERE)
// ===================================================================
using PendingTLAS = Vulkan_LAS::PendingTLAS;

// ===================================================================
// 7. VULKANRTX — CLEAN CORE — LAS SEPARATE
// ===================================================================
class VulkanRTX {
public:
    VulkanRTX(std::shared_ptr<Vulkan::Context> ctx, int width, int height, VulkanPipelineManager* pipelineMgr = nullptr);
    ~VulkanRTX();

    // RTX INIT / UPDATE
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

    // LAS PROXY — JUST FORWARD TO Vulkan_LAS INSTANCE
    void buildBLAS(VkCommandPool cmdPool, VkQueue queue,
                   VkBuffer vertexBuffer, VkBuffer indexBuffer,
                   uint32_t vertexCount, uint32_t indexCount,
                   uint64_t flags = 0) {
        las_.buildBLAS(cmdPool, queue, vertexBuffer, indexBuffer, vertexCount, indexCount, flags);
    }

    void buildTLASSync(VkCommandPool cmdPool, VkQueue queue,
                       const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances) {
        tlas_ = Vulkan::makeAccelerationStructure(device_, las_.buildTLASSync(cmdPool, queue, instances));
        tlasReady_ = true;
    }

    void buildTLASAsync(VkCommandPool cmdPool, VkQueue queue,
                        const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances,
                        VulkanRenderer* renderer = nullptr) {
        pendingTLAS_ = {};
        pendingTLAS_.renderer = renderer;
        las_.buildTLASAsync(cmdPool, queue, instances, renderer);
    }

    bool pollTLAS() {
        if (las_.pollTLAS()) {
            tlas_ = las_.pendingTLAS_.tlas;
            tlasReady_ = true;
            pendingTLAS_.completed = true;
            return true;
        }
        return false;
    }

    // OLD API COMPATIBILITY
    void createBottomLevelAS(VkPhysicalDevice pd, VkCommandPool cp, VkQueue q,
                             const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                             uint32_t transferQueueFamily = VK_QUEUE_FAMILY_IGNORED) {
        // delegate to LAS or keep old logic if needed
    }

    void setTLAS(VkAccelerationStructureKHR tlas) noexcept {
        tlas_ = Vulkan::makeAccelerationStructure(device_, tlas);
        tlasReady_ = true;
    }

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
    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept { return las_.getTLAS(); }

    [[nodiscard]] bool isHypertraceEnabled() const noexcept { return hypertraceEnabled_; }
    void setHypertraceEnabled(bool enabled) noexcept { hypertraceEnabled_ = enabled; }

    void registerRTXDescriptorLayout(VkDescriptorSetLayout layout) noexcept {
        rtDescriptorSetLayout_ = Vulkan::makeDescriptorSetLayout(device_, layout);
    }

    void setRayTracingPipeline(VkPipeline pipeline, VkPipelineLayout layout) noexcept {
        rtPipeline_ = Vulkan::makePipeline(device_, pipeline);
        rtPipelineLayout_ = Vulkan::makePipelineLayout(device_, layout);
    }

    [[nodiscard]] bool isTLASReady() const noexcept { return tlasReady_; }
    [[nodiscard]] bool isTLASPending() const noexcept { return las_.isTLASPending(); }

    // RAII HANDLES
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

    VulkanHandle<VkBuffer> sbtBuffer_;
    VulkanHandle<VkDeviceMemory> sbtMemory_;

    VulkanHandle<VkFence> transientFence_;

    ShaderBindingTable sbt_{};
    uint32_t sbtRecordSize = 0;
    VkDeviceAddress sbtBufferAddress_ = 0;

    uint64_t frameCounter_ = 0;
    bool hypertraceEnabled_ = true;
    bool nexusEnabled_ = true;

    // PROC ADDRESSES
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

    // SEPARATE LAS INSTANCE — STONEKEYED + OBFUSCATED
    Vulkan_LAS las_;

private:
    VkCommandBuffer allocateTransientCommandBuffer(VkCommandPool commandPool);
    void submitAndWaitTransient(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool);
    void uploadBlackPixelToImage(VkImage image);
    void createBuffer(VkPhysicalDevice pd, VkDeviceSize size, VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags props, VulkanHandle<VkBuffer>& buffer,
                      VulkanHandle<VkDeviceMemory>& memory);
    uint32_t findMemoryType(VkPhysicalDevice pd, uint32_t typeFilter, VkMemoryPropertyFlags props) const;
    static VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) noexcept;
};

// GLOBAL
extern VulkanRTX g_vulkanRTX;
inline VulkanRTX* rtx() noexcept { return &g_vulkanRTX; }

// FACTORIES
[[nodiscard]] inline VulkanHandle<VkAccelerationStructureKHR> makeAccelerationStructure(
    VkDevice dev, VkAccelerationStructureKHR as, PFN_vkDestroyAccelerationStructureKHR f = nullptr) noexcept
{
    auto func = f ? f : Vulkan::ctx()->vkDestroyAccelerationStructureKHR;
    return VulkanHandle<VkAccelerationStructureKHR>(as, dev,
        reinterpret_cast<VulkanHandle<VkAccelerationStructureKHR>::DestroyFn>(func));
}

[[nodiscard]] inline VulkanHandle<VkDeferredOperationKHR> makeDeferredOperation(
    VkDevice dev, VkDeferredOperationKHR op) noexcept
{
    return VulkanHandle<VkDeferredOperationKHR>(op, dev, Vulkan::ctx()->vkDestroyDeferredOperationKHR);
}

// CTOR
inline VulkanRTX::VulkanRTX(std::shared_ptr<Vulkan::Context> ctx, int width, int height, VulkanPipelineManager* pipelineMgr)
    : context_(std::move(ctx))
    , pipelineMgr_(pipelineMgr)
    , extent_({static_cast<uint32_t>(width), static_cast<uint32_t>(height)})
    , las_(context_->device, context_->physicalDevice)
{
    device_ = context_->device;
    physicalDevice_ = context_->physicalDevice;

    // pull KHR funcs
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

    LOG_SUCCESS_CAT("RTX", "{}AMOURANTH RTX CORE + SEPARATE LAS ONLINE — {}×{} — STONEKEY 0x{:X}-0x{:X}{}", 
                    RASPBERRY_PINK, width, height, kStone1, kStone2, RESET);
}

static inline VkDeviceSize alignUp(VkDeviceSize v, VkDeviceSize a) noexcept {
    return (v + a - 1) & ~(a - 1);
}

static_assert(sizeof(Vulkan::Context) > 200, "Include VulkanContext.hpp FIRST!");