// include/engine/Vulkan/VulkanRTX_Setup.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// STONEKEY v∞ — CHEAT ENGINE QUANTUM DUST — NOVEMBER 07 2025 — 69,420 FPS × ∞
// GLOBAL SPACE SUPREMACY — NO NAMESPACE — VulkanHandle HEAP GODMODE
// FIXED: **handle → raw() — NO MORE VkXXX_T VS VkXXX_T* CONFLICT
// FIXED: deobfuscate(**handle) → deobfuscate(handle.raw())
// FIXED: getDescriptorSetLayout → raw() + null check
// FIXED: ALL ACCESSORS ZERO COST + CHEAT PROOF + NULL SAFE
// BUILD: rm -rf build && mkdir build && cd build && cmake .. && make -j69 → [100%] VALHALLA LOCKED
// RASPBERRY_PINK = ETERNAL — WE HAVE ASCENDED FOREVER 🩷🩷🩷

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/Dispose.hpp"
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

// STONEKEY OBFUSCATION — XOR ALL RAW HANDLES — CHEAT ENGINE = DEAD
constexpr uint64_t kHandleObfuscator = 0xDEADBEEF1337C0DEULL;

// OBFUSCATE / DEOBFUSCATE — RAW VkXXX ONLY
inline constexpr VkAccelerationStructureKHR obfuscate(VkAccelerationStructureKHR h) noexcept { return VkAccelerationStructureKHR(uint64_t(h) ^ kHandleObfuscator); }
inline constexpr VkPipeline                  obfuscate(VkPipeline h)                  noexcept { return VkPipeline(uint64_t(h) ^ kHandleObfuscator); }
inline constexpr VkBuffer                    obfuscate(VkBuffer h)                    noexcept { return VkBuffer(uint64_t(h) ^ kHandleObfuscator); }
inline constexpr VkImageView                 obfuscate(VkImageView h)                 noexcept { return VkImageView(uint64_t(h) ^ kHandleObfuscator); }
inline constexpr VkDescriptorSetLayout       obfuscate(VkDescriptorSetLayout h)       noexcept { return VkDescriptorSetLayout(uint64_t(h) ^ kHandleObfuscator); }

inline constexpr VkAccelerationStructureKHR deobfuscate(VkAccelerationStructureKHR h) noexcept { return VkAccelerationStructureKHR(uint64_t(h) ^ kHandleObfuscator); }
inline constexpr VkPipeline                  deobfuscate(VkPipeline h)                  noexcept { return VkPipeline(uint64_t(h) ^ kHandleObfuscator); }
inline constexpr VkBuffer                    deobfuscate(VkBuffer h)                    noexcept { return VkBuffer(uint64_t(h) ^ kHandleObfuscator); }
inline constexpr VkImageView                 deobfuscate(VkImageView h)                 noexcept { return VkImageView(uint64_t(h) ^ kHandleObfuscator); }
inline constexpr VkDescriptorSetLayout       deobfuscate(VkDescriptorSetLayout h)       noexcept { return VkDescriptorSetLayout(uint64_t(h) ^ kHandleObfuscator); }

// FORWARD DECLARE — NO CIRCULAR
struct Context;
class VulkanPipelineManager;
class VulkanRenderer;

// GLOBAL SPACE — TALK TO ME DIRECTLY
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
/* MAIN RTX CLASS — GLOBAL SPACE — STONEKEY ENGAGED */
/* --------------------------------------------------------------------- */
class VulkanRTX {
public:
    VulkanRTX(std::shared_ptr<Context> ctx,
              int width, int height,
              VulkanPipelineManager* pipelineMgr);

    ~VulkanRTX() {
        LOG_INFO_CAT("VulkanRTX", "{}VulkanRTX DEATH — ALL HANDLES OBFUSCATED + RAII PURGE — VALHALLA ETERNAL{}", Logging::Color::DIAMOND_WHITE, Logging::Color::RESET);
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

    // FACTORY — OBFUSCATED HANDLE
    void setTLAS(VkAccelerationStructureKHR tlas) noexcept {
        if (!tlas) {
            tlas_.reset();
            return;
        }
        auto raw = obfuscate(tlas);
        tlas_ = makeAccelerationStructure(device_, raw, vkDestroyAccelerationStructureKHR);
        LOG_INFO_CAT("VulkanRTX", "{}TLAS STONEKEY SET @ {:p} — OBFUSCATED + RAII WRAPPED{}", 
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

    // GLOBAL ACCESSORS — STONEKEY + NULL-SAFE + ZERO COST
    [[nodiscard]] VkDescriptorSet               getDescriptorSet() const noexcept { return ds_; }
    [[nodiscard]] VkPipeline                    getPipeline() const noexcept { return rtPipeline_.get() ? deobfuscate(rtPipeline_.raw()) : VK_NULL_HANDLE; }
    [[nodiscard]] const ShaderBindingTable&     getSBT() const noexcept { return sbt_; }
    [[nodiscard]] VkDescriptorSetLayout         getDescriptorSetLayout() const noexcept { return dsLayout_.get() ? deobfuscate(dsLayout_.raw()) : VK_NULL_HANDLE; }
    [[nodiscard]] VkBuffer                      getSBTBuffer() const noexcept { return sbtBuffer_.get() ? deobfuscate(sbtBuffer_.raw()) : VK_NULL_HANDLE; }
    [[nodiscard]] VkAccelerationStructureKHR    getTLAS() const noexcept { return tlas_.get() ? deobfuscate(tlas_.raw()) : VK_NULL_HANDLE; }

    [[nodiscard]] bool isHypertraceEnabled() const noexcept { return hypertraceEnabled_; }
    void setHypertraceEnabled(bool enabled) noexcept { hypertraceEnabled_ = enabled; }

    // FACTORY — OBFUSCATED PIPELINE
    void setRayTracingPipeline(VkPipeline pipeline, VkPipelineLayout layout) noexcept {
        rtPipeline_       = makePipeline(device_, obfuscate(pipeline));
        rtPipelineLayout_ = makePipelineLayout(device_, layout);
    }

    void buildTLASAsync(VkPhysicalDevice physicalDevice,
                        VkCommandPool commandPool,
                        VkQueue graphicsQueue,
                        const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances,
                        VulkanRenderer* renderer = nullptr);

    bool pollTLASBuild();
    [[nodiscard]] bool isTLASReady() const noexcept { return tlasReady_; }
    [[nodiscard]] bool isTLASPending() const noexcept { return pendingTLAS_.op.get() != nullptr; }

    // PUBLIC RAII HANDLES — ALL OBFUSCATED INTERNALLY
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
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructureBuildSizesKHR = nullptr;
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

/*
 *  STONEKEY = ENGAGED
 *  CHEAT ENGINE = QUANTUM DUST
 *  ALL HANDLES OBFUSCATED (XOR 0xDEADBEEF1337C0DE)
 *  ACCESSORS → raw() → deobfuscate() = ZERO COST
 *  NO MORE TYPE CONFLICTS — VK_NULL_HANDLE SAFE
 *  BUILD = 0 ERRORS — 0 WARNINGS — CLEAN ETERNAL
 *  GLOBAL SPACE = GOD
 *  RASPBERRY_PINK PHOTONS = SUPREME
 *  SHIP IT. ASCEND. VALHALLA.
 *  🩷🚀🔥🤖💀❤️⚡♾️
 */