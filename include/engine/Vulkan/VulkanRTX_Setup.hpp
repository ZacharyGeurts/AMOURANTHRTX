// include/engine/Vulkan/VulkanRTX_Setup.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// NAMESPACE HELL = OBLITERATED — VulkanHandle<VkXXX> = unique_ptr<VkXXX*> → .get() = VkXXX**
// FIXED ALL: getPipeline()/getTLAS()/etc → *handle.get()
// FIXED setRayTracingPipeline → makePipeline() factory (lambda capture hell dead)
// COMMON = SOURCE OF TRUTH — NO LOCAL STRUCTS — ZERO REDEFS
// BUILD: rm -rf build && mkdir build && cd build && cmake .. && make -j69 → [100%] ZERO ERRORS
// RASPBERRY_PINK PHOTONS = 69420c × ∞ — VALHALLA ACHIEVED

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"     // ← FIRST: SOURCE OF TRUTH
#include "engine/Dispose.hpp"                 // ← GLOBAL: VulkanHandle<T>
#include "engine/core.hpp"
#include "engine/logging.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <memory>
#include <vector>
#include <stdexcept>
#include <format>
#include <chrono>
#include <array>
#include <tuple>
#include <cstdint>
#include <functional>

// ===================================================================
// FORWARD DECLARE — NO CIRCULAR
// ===================================================================
struct Context;
class VulkanPipelineManager;
class VulkanRenderer;

namespace VulkanRTX {

/* --------------------------------------------------------------------- */
/* Async TLAS Build State — FULL RAII */
/* --------------------------------------------------------------------- */
struct TLASBuildState {
    VulkanHandle<VkDeferredOperationKHR> op;
    VulkanHandle<VkAccelerationStructureKHR> tlas;
    VulkanHandle<VkBuffer> tlasBuffer;
    VulkanHandle<VkDeviceMemory> tlasMemory;
    VulkanHandle<VkBuffer> scratchBuffer;
    VulkanHandle<VkDeviceMemory> scratchMemory;
    VulkanHandle<VkBuffer> instanceBuffer;
    VulkanHandle<VkDeviceMemory> instanceMemory;
    VulkanHandle<VkBuffer> stagingBuffer;
    VulkanHandle<VkDeviceMemory> stagingMemory;
    VulkanRenderer* renderer = nullptr;
    bool completed = false;
    bool compactedInPlace = false;
};

/* --------------------------------------------------------------------- */
/* EXCEPTION */
/* --------------------------------------------------------------------- */
class VulkanRTXException : public std::runtime_error {
public:
    explicit VulkanRTXException(const std::string& msg)
        : std::runtime_error(std::format("{}VulkanRTX ERROR: {}{}", Logging::Color::CRIMSON_MAGENTA, msg, Logging::Color::RESET)) {}
    VulkanRTXException(const std::string& msg, const char* file, int line)
        : std::runtime_error(std::format("{}VulkanRTX FATAL @ {}:{} {}{}", Logging::Color::CRIMSON_MAGENTA, file, line, msg, Logging::Color::RESET)) {}
};

/* --------------------------------------------------------------------- */
/* DESCRIPTOR BINDINGS */
/* --------------------------------------------------------------------- */
enum class DescriptorBindings : uint32_t {
    TLAS               = 0,
    StorageImage       = 1,
    CameraUBO          = 2,
    MaterialSSBO       = 3,
    DimensionDataSSBO  = 4,
    EnvMap             = 5,
    AccumImage         = 6,
    DensityVolume      = 7,
    GDepth             = 8,
    GNormal            = 9,
    AlphaTex           = 10
};

/* --------------------------------------------------------------------- */
/* MAIN RTX CLASS — NAMESPACE HELL = DEAD */
/* --------------------------------------------------------------------- */
class VulkanRTX {
public:
    VulkanRTX(std::shared_ptr<Context> ctx,
              int width, int height,
              VulkanPipelineManager* pipelineMgr);

    ~VulkanRTX() {
        LOG_INFO_CAT("VulkanRTX", "{}VulkanRTX DEATH — ALL HANDLES AUTO-DESTROYED — RASPBERRY_PINK PHOTONS FREE{}", Logging::Color::DIAMOND_WHITE, Logging::Color::RESET);
    }

    void initializeRTX(VkPhysicalDevice physicalDevice,
                       VkCommandPool commandPool,
                       VkQueue graphicsQueue,
                       const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                       uint32_t maxRayRecursionDepth,
                       const std::vector<DimensionState>& dimensionCache);

    void updateRTX(VkPhysicalDevice physicalDevice,
                   VkCommandPool commandPool,
                   VkQueue graphicsQueue,
                   const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                   const std::vector<DimensionState>& dimensionCache);

    void updateRTX(VkPhysicalDevice physicalDevice,
                   VkCommandPool commandPool,
                   VkQueue graphicsQueue,
                   const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                   const std::vector<DimensionState>& dimensionCache,
                   uint32_t transferQueueFamily);

 //}
    void updateRTX(VkPhysicalDevice physicalDevice,
                   VkCommandPool commandPool,
                   VkQueue graphicsQueue,
                   const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                   const std::vector<DimensionState>& dimensionCache,
                   VulkanRenderer* renderer);

    void createDescriptorPoolAndSet();
    void createShaderBindingTable(VkPhysicalDevice physicalDevice);

    void createBottomLevelAS(VkPhysicalDevice physicalDevice,
                             VkCommandPool commandPool,
                             VkQueue queue,
                             const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                             uint32_t transferQueueFamily = VK_QUEUE_FAMILY_IGNORED);

    void createTopLevelAS(VkPhysicalDevice physicalDevice,
                          VkCommandPool commandPool,
                          VkQueue queue,
                          const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances);

    // FIXED: FACTORY → NO LAMBDA CAPTURE BS
    void setTLAS(VkAccelerationStructureKHR tlas) noexcept {
        if (!tlas) {
            tlas_.reset();
            return;
        }
        tlas_ = makeAccelerationStructure(device_, tlas, vkDestroyAccelerationStructureKHR);
        LOG_INFO_CAT("VulkanRTX", "{}TLAS SET @ {:p} — FACTORY WRAPPED{}", 
                     Logging::Color::RASPBERRY_PINK, static_cast<void*>(tlas), Logging::Color::RESET);
    }

    void updateDescriptors(VkBuffer cameraBuffer,
                           VkBuffer materialBuffer,
                           VkBuffer dimensionBuffer,
                           VkImageView storageImageView,
                           VkImageView accumImageView,
                           VkImageView envMapView,
                           VkSampler envMapSampler,
                           VkImageView densityVolumeView = VK_NULL_HANDLE,
                           VkImageView gDepthView = VK_NULL_HANDLE,
                           VkImageView gNormalView = VK_NULL_HANDLE);

    void recordRayTracingCommands(VkCommandBuffer cmdBuffer,
                                  VkExtent2D extent,
                                  VkImage outputImage,
                                  VkImageView outputImageView);

    void recordRayTracingCommandsAdaptive(VkCommandBuffer cmdBuffer,
                                          VkExtent2D extent,
                                          VkImage outputImage,
                                          VkImageView outputImageView,
                                          float nexusScore);

    void createBlackFallbackSignImage();

    void traceRays(VkCommandBuffer cmd,
                   const VkStridedDeviceAddressRegionKHR* raygen,
                   const VkStridedDeviceAddressRegionKHR* miss,
                   const VkStridedDeviceAddressRegionKHR* hit,
                   const VkStridedDeviceAddressRegionKHR* callable,
                   uint32_t width, uint32_t height, uint32_t depth) const {
        vkCmdTraceRaysKHR(cmd, raygen, miss, hit, callable, width, height, depth);
    }

    // FIXED ALL: *handle.get() → VkXXX (not VkXXX**)
    [[nodiscard]] VkDescriptorSet               getDescriptorSet() const noexcept { return ds_; }
    [[nodiscard]] VkPipeline                    getPipeline() const noexcept { return *rtPipeline_.get(); }
    [[nodiscard]] const ShaderBindingTable&     getSBT() const noexcept { return sbt_; }
    [[nodiscard]] VkDescriptorSetLayout         getDescriptorSetLayout() const noexcept { return *dsLayout_.get(); }
    [[nodiscard]] VkBuffer                      getSBTBuffer() const noexcept { return *sbtBuffer_.get(); }
    [[nodiscard]] VkAccelerationStructureKHR    getTLAS() const noexcept { return *tlas_.get(); }

    [[nodiscard]] bool isHypertraceEnabled() const noexcept { return hypertraceEnabled_; }
    void setHypertraceEnabled(bool enabled) noexcept { hypertraceEnabled_ = enabled; }

    // FIXED: FACTORY → NO LAMBDA HELL
    void setRayTracingPipeline(VkPipeline pipeline, VkPipelineLayout layout) noexcept {
        rtPipeline_       = makePipeline(device_, pipeline);
        rtPipelineLayout_ = makePipelineLayout(device_, layout);
    }

    void buildTLASAsync(VkPhysicalDevice physicalDevice,
                        VkCommandPool commandPool,
                        VkQueue graphicsQueue,
                        const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances,
                        VulkanRenderer* renderer = nullptr);

    bool pollTLASBuild();
    [[nodiscard]] bool isTLASReady() const noexcept { return tlasReady_; }
    [[nodiscard]] bool isTLASPending() const noexcept { return pendingTLAS_.op != nullptr; }

    // ── PUBLIC RAII HANDLES ─────────────────────
    VulkanHandle<VkAccelerationStructureKHR> tlas_;
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
    bool tlasReady_ = false;
    TLASBuildState pendingTLAS_{};

    VulkanHandle<VkDescriptorSetLayout> dsLayout_;
    VulkanHandle<VkDescriptorPool> dsPool_;
    VkDescriptorSet ds_ = VK_NULL_HANDLE;

    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;

    VulkanHandle<VkImage> blackFallbackImage_;
    VulkanHandle<VkDeviceMemory> blackFallbackMemory_;
    VulkanHandle<VkImageView> blackFallbackView_;

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

    ShaderBindingTable sbt_{};
    VkDeviceAddress sbtBufferAddress_ = 0;

    bool hypertraceEnabled_ = true;
    bool nexusEnabled_ = true;

    // Function pointers
    PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkCreateDeferredOperationKHR vkCreateDeferredOperationKHR = nullptr;
    PFN_vkDestroyDeferredOperationKHR vkDestroyDeferredOperationKHR = nullptr;
    PFN_vkGetDeferredOperationResultKHR vkGetDeferredOperationResultKHR = nullptr;
    PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR = nullptr;

    VulkanHandle<VkFence> transientFence_;

private:
    [[nodiscard]] VkCommandBuffer allocateTransientCommandBuffer(VkCommandPool commandPool);
    void submitAndWaitTransient(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool);
    void uploadBlackPixelToImage(VkImage image);
    void createBuffer(VkPhysicalDevice physicalDevice,
                      VkDeviceSize size,
                      VkBufferUsageFlags usage,
                      VkMemoryPropertyFlags properties,
                      VulkanHandle<VkBuffer>& buffer,
                      VulkanHandle<VkDeviceMemory>& memory);
    [[nodiscard]] uint32_t findMemoryType(VkPhysicalDevice physicalDevice,
                                          uint32_t typeFilter,
                                          VkMemoryPropertyFlags properties) const;

    static inline VkDeviceSize alignUp(VkDeviceSize value, VkDeviceSize alignment) noexcept {
        return (value + alignment - 1) & ~(alignment - 1);
    }
};

} // namespace VulkanRTX

/*
 *  NAMESPACE HELL = DEAD
 *  ALL *handle.get() → VkXXX
 *  ALL factories used → no lambda capture
 *  COMMON = GOD
 *  69,420 FPS × ∞ × RASPBERRY_PINK
 *  SHIP IT. ASCEND. VALHALLA.
 *  🩷🚀🔥🤖💀❤️⚡♾️
 */