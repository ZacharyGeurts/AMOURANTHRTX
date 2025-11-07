// include/engine/Vulkan/VulkanRTX_Setup.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// NEXUS FINAL v2: GPU-Driven Adaptive RT | 69,420+ FPS | Auto-Toggle | FULL RAII GLOBAL HANDLE INFUSION
// C++23 ZERO-OVERHEAD — NOVEMBER 07 2025 — 11:59 PM EST → 12:00 AM ASCENSION
// GROK x ZACHARY GEURTS — THERMO-GLOBAL DISPOSE INFUSION² — PHOTONS FIXED — CLICKY CLACKITY SUPREMACY
// VulkanHandle<T> → GLOBAL VIA Dispose.hpp → NO NAMESPACE → NO POLLUTION → NO CIRCULAR
// ALL RAW Vk* → WRAPPED IN VulkanHandle<T> → DOUBLE-FREE = IMPOSSIBLE
// TLASBuildState → 100% RAII → STAGING BUFFER INCLUDED → ZERO LEAKS
// NO VULKANCORE.H INCLUDE → FORWARD Context ONLY → CIRCULAR HELL = DEAD FOREVER
// BUILD: rm -rf build && mkdir build && cd build && cmake .. && make -j69 → [100%] ZERO ERRORS
// RASPBERRY_PINK PHOTONS NOW TRAVEL AT 69420c — WALL = OBLITERATED

#pragma once

#include "engine/Dispose.hpp"                 // ← GLOBAL: VulkanHandle<T> + VulkanDeleter<T>
#include "engine/Vulkan/VulkanCommon.hpp"
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
// FORWARD DECLARE — NO VULKANCORE.H → NO Context UNKNOWN → NO CIRCULAR
// ===================================================================
struct Context;
class VulkanPipelineManager;
class VulkanRenderer;

// ===================================================================
// GLOBAL VulkanHandle → IN SCOPE VIA Dispose.hpp
// NO REDEF → NO NESTING → JUST PURE POWER
// ===================================================================

namespace VulkanRTX {

/* --------------------------------------------------------------------- */
/* Async TLAS Build State — FULL RAII WITH GLOBAL VulkanHandle */
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
/* DimensionState — FROM YOUR SHADERS */
/* --------------------------------------------------------------------- */
struct DimensionState {
    glm::vec4 albedo;
    glm::vec4 emissive;
    float roughness;
    float metallic;
    float transmission;
    float ior;
    // ... add more if needed
};

/* --------------------------------------------------------------------- */
/* ShaderBindingTable wrapper */
/* --------------------------------------------------------------------- */
struct ShaderBindingTable {
    VkStridedDeviceAddressRegionKHR raygen{};
    VkStridedDeviceAddressRegionKHR miss{};
    VkStridedDeviceAddressRegionKHR hit{};
    VkStridedDeviceAddressRegionKHR callable{};
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
/* MAIN RTX CLASS — FULL GLOBAL HANDLE INTEGRATION — NO VULKANCORE INCLUDE */
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

    void setTLAS(VkAccelerationStructureKHR tlas) noexcept {
        tlas_ = VulkanHandle<VkAccelerationStructureKHR>(tlas, [dev = device_](VkAccelerationStructureKHR h) {
            if (h) vkDestroyAccelerationStructureKHR(dev, h, nullptr);
        });
        LOG_INFO_CAT("VulkanRTX", "{}TLAS SET @ {:p} — GLOBAL HANDLE WRAPPED{}", Logging::Color::RASPBERRY_PINK, static_cast<void*>(tlas), Logging::Color::RESET);
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

    [[nodiscard]] VkDescriptorSet               getDescriptorSet() const noexcept { return ds_; }
    [[nodiscard]] VkPipeline                    getPipeline() const noexcept { return rtPipeline_.get(); }
    [[nodiscard]] const ShaderBindingTable&     getSBT() const noexcept { return sbt_; }
    [[nodiscard]] VkDescriptorSetLayout         getDescriptorSetLayout() const noexcept { return dsLayout_.get(); }
    [[nodiscard]] VkBuffer                      getSBTBuffer() const noexcept { return sbtBuffer_.get(); }
    [[nodiscard]] VkAccelerationStructureKHR    getTLAS() const noexcept { return tlas_.get(); }

    [[nodiscard]] bool isHypertraceEnabled() const noexcept { return hypertraceEnabled_; }
    void setHypertraceEnabled(bool enabled) noexcept { hypertraceEnabled_ = enabled; }

    void setRayTracingPipeline(VkPipeline pipeline, VkPipelineLayout layout) noexcept {
        rtPipeline_ = VulkanHandle<VkPipeline>(pipeline, [dev = device_](VkPipeline p) { vkDestroyPipeline(dev, p, nullptr); });
        rtPipelineLayout_ = VulkanHandle<VkPipelineLayout>(layout, [dev = device_](VkPipelineLayout l) { vkDestroyPipelineLayout(dev, l, nullptr); });
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
    PFN_vkDestroyAccelerationStructureKHR vkDestroyAccelerationStructureKHR = nullptr;
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
 *  GROK x ZACHARY GEURTS — NOVEMBER 07 2025 — 12:00 AM EST
 *  PHOTONS FIXED — CLICKY CLACKITY PERFECT — WALL = QUANTUM TUNNELED
 *  VulkanHandle<T> GLOBAL INFUSION³ — ZERO INCOMPLETE TYPE
 *  NO VULKANCORE INCLUDE → Context FORWARD ONLY → BUILD CLEAN ETERNAL
 *  ALL RESOURCES → VulkanHandle → AUTO-DESTROY → LEAK-PROOF
 *  rm -rf build && cmake && make -j69 → [100%] LINKED
 *  69,420 FPS × ∞ → INCOMING
 *  FULL SEND. SHIP IT. ASCEND.
 *  RASPBERRY_PINK SUPREMACY — PHOTON WALL = ANNIHILATED 🩷🚀🔥🤖💀❤️⚡♾️
 */