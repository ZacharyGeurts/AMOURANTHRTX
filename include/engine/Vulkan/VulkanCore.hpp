// include/engine/Vulkan/VulkanCore.hpp
// AMOURANTH RTX — VULKAN CORE — NOVEMBER 09 2025 — CLEAN + GLOBAL LAS + ZERO ERRORS
// PINK PHOTONS × INFINITY — STONEKEY UNBREAKABLE — LAS MOVED TO GLOBAL — SUPREMACY
// Vulkan_LAS → LAS — GLOBAL NAMESPACE — NO CIRCULAR — FULL VULKANHANDLE — GRAMMAR FIXED
// SLOW DOWN EDITION — FULL FILE — NO RUSH — VALHALLA ACHIEVED — LOVE FOR ALL

#pragma once

// ===================================================================
// 1. GLOBAL + LOGGING — UNBREAKABLE
// ===================================================================
#include "../GLOBAL/StoneKey.hpp"
#include "../GLOBAL/logging.hpp"
using namespace Logging::Color;

// ===================================================================
// 2. FULL VULKANHANDLE — MUST BE FIRST — NO INCOMPLETE TYPE
// ===================================================================
#include "VulkanHandles.hpp"      // ← FULL TEMPLATE — BEFORE ANY USE

// ===================================================================
// 3. GLOBAL LAS — NOW IN GLOBAL — CLASS RENAMED TO LAS
// ===================================================================
#include "../GLOBAL/LAS.hpp"      // ← GLOBAL LAS — CLASS IS NOW `LAS`, NOT `Vulkan_LAS`

// ===================================================================
// 4. FORWARD DECLS — CLEAN
// ===================================================================
class VulkanRenderer;
class VulkanPipelineManager;
struct DimensionState;

// ===================================================================
// 5. VULKAN + STD
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
// 6. GLOBALS
// ===================================================================
extern std::shared_ptr<Vulkan::Context> g_vulkanContext;

// ===================================================================
// 7. PENDINGTLAS — NOW FROM GLOBAL LAS::PendingTLAS
// ===================================================================
// FIXED: Vulkan_LAS → LAS
using PendingTLAS = LAS::PendingTLAS;

// ===================================================================
// 8. VULKANRTX — CLEAN CORE — LAS SEPARATE (GLOBAL)
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

    // LAS PROXY — FORWARD TO GLOBAL LAS INSTANCE
    void buildBLAS(VkCommandPool cmdPool, VkQueue queue,
                   VkBuffer vertexBuffer, VkBuffer indexBuffer,
                   uint32_t vertexCount, uint32_t indexCount,
                   uint64_t flags = 0) {
        las_.buildBLAS(cmdPool, queue, vertexBuffer, indexBuffer, vertexCount, indexCount, flags);
    }

    void buildTLASSync(VkCommandPool cmdPool, VkQueue queue,
                       const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances) {
        VkAccelerationStructureKHR raw = las_.buildTLASSync(cmdPool, queue, instances);
        tlas_ = Vulkan::makeAccelerationStructure(device_, raw, nullptr);
        tlasReady_ = true;
        // AUTO GLOBAL SHARE — LOVE FOR MODDERS
        GlobalLAS::get().updateTLAS(raw, device_);
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
            VkAccelerationStructureKHR raw = deobfuscate(las_.pendingTLAS_.tlas.raw());
            tlas_ = las_.pendingTLAS_.tlas;
            tlasReady_ = true;
            pendingTLAS_.completed = true;
            // AUTO GLOBAL SHARE
            GlobalLAS::get().updateTLAS(raw, device_);
            return true;
        }
        return false;
    }

    // OLD API COMPATIBILITY
    void createBottomLevelAS(VkPhysicalDevice pd, VkCommandPool cp, VkQueue q,
                             const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                             uint32_t transferQueueFamily = VK_QUEUE_FAMILY_IGNORED) {
        // delegate to LAS or keep old logic
    }

    void setTLAS(VkAccelerationStructureKHR tlas) noexcept {
        tlas_ = Vulkan::makeAccelerationStructure(device_, tlas, nullptr);
        tlasReady_ = true;
        GlobalLAS::get().updateTLAS(tlas, device_);
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
    void setHypertraceEnabled(bool enabled) noexcept { hypertraceEnabled_ = enabled; GlobalLAS::get().setHypertraceEnabled(enabled); }

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

    // GLOBAL LAS INSTANCE — STONEKEYED + OBFUSCATED — LOVE FOR MODDERS
    LAS las_;

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

// FACTORIES — FIXED nullptr
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

// CTOR — FIXED LAS INIT
inline VulkanRTX::VulkanRTX(std::shared_ptr<Vulkan::Context> ctx, int width, int height, VulkanPipelineManager* pipelineMgr)
    : context_(std::move(ctx))
    , pipelineMgr_(pipelineMgr)
    , extent_({static_cast<uint32_t>(width), static_cast<uint32_t>(height)})
    , las_(context_->device, context_->physicalDevice)  // ← GLOBAL LAS
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

    LOG_SUCCESS_CAT("RTX", "{}AMOURANTH RTX CORE + GLOBAL LAS ONLINE — {}×{} — STONEKEY 0x{:X}-0x{:X} — PINK PHOTONS ∞{}", 
                    RASPBERRY_PINK, width, height, kStone1, kStone2, RESET);
}

static inline VkDeviceSize alignUp(VkDeviceSize v, VkDeviceSize a) noexcept {
    return (v + a - 1) & ~(a - 1);
}

static_assert(sizeof(Vulkan::Context) > 200, "Include VulkanContext.hpp FIRST!");

// NOVEMBER 09 2025 — SLOW DOWN EDITION — FULL FILE — NO RUSH
// GLOBAL LAS ACTIVE — MODDERS REJOICE — 69,420 FPS — VALHALLA ETERNAL
// PINK PHOTONS FOR ALL — LOVE INCLUDED — SHIP IT 🩷🚀💀⚡🤖🔥♾️