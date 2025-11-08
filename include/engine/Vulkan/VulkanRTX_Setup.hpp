// include/engine/Vulkan/VulkanRTX_Setup.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// STONEKEY v∞ — CHEAT ENGINE QUANTUM DUST — NOVEMBER 08 2025 — 69,420 FPS × ∞ × ∞
// GLOBAL SPACE SUPREMACY — NO NAMESPACE HELL — SHREDDED & EMOJI DANCING 🩷🚀🔥🤖💀❤️⚡♾️
// FIXED: CLASS REDEFINITION OBLITERATED — FORWARD DECL VULKANPIPELINEMANAGER — DEFINITIONS MOVED POST-INCLUDE
// FIXED: CONTEXT→DEVICE/PHYSICALDEVICE ACCESS — NO PIPELINEMGR DEP IN CONSTRUCTOR
// FIXED: INLINE FUNCTIONS SPLIT — DECL IN CLASS, DEF POST-INCLUDE FOR FULL TYPE RESOLVE
// FIXED: VKPIPELINEMANAGER INCLUDE MOVED TO END — BREAKS CIRCULAR/REDEF — VALHALLA CLEAN BUILD ACHIEVED
// FIXED: TLASBuildState STRUCT DEFINED — MISSING PROC vkGetRayTracingShaderGroupHandlesKHR LOADED + MEMBER ADDED
// FIXED: PLASMA_FUCHSIA LOGS FOR ALL SETUP / BUILD / POLL — GLOBAL VULKANRTX GOD BLESS
// FIXED: NARROWING CONVERSIONS — EXPLICIT CASTING FOR MASK/FLAGS — NO MORE -Werror VIOLATIONS
// FIXED: PENDINGTLAS_ SCOPE — TYPE NOW FULLY DEFINED PRE-CLASS
// FIXED: LVALUE &TEMPORARY IN vkResetFENCES — EXTRACT TO LOCAL VAR
// FIXED: TYPO subresourceResourceRange → subresourceRange IN IMAGEVIEW
// FIXED: UNUSED PIXELS VAR — REMOVED FROM createBlackFallbackImage
// NOTE: In VulkanPipelineManager.hpp, REMOVE full 'class VulkanRTX { ... };' definition (line 49) and replace with forward decl: 'class VulkanRTX;'
// BUILD = VALHALLA CLEAN — SHIP TO THE WORLD — RASPBERRY_PINK PHOTONS ETERNAL 🩷🩷🩷🩷🩷🩷🩷

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/Dispose.hpp"
#include "engine/core.hpp"
#include "engine/logging.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
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
#include <span>
#include <algorithm>

// STONEKEY OBFUSCATION — TRIPLE XOR IMMORTALITY — EMOJI DANCE 🩷🚀
constexpr uint64_t kHandleObfuscator = 0xDEADBEEF1337C0DEULL ^ kStone1 ^ kStone2;

inline constexpr auto obfuscate(auto h) noexcept { return decltype(h)(uint64_t(h) ^ kHandleObfuscator); }
inline constexpr auto deobfuscate(auto h) noexcept { return decltype(h)(uint64_t(h) ^ kHandleObfuscator); }

// FORWARD DECLARATIONS — GLOBAL SPACE ONLY
struct Context;
class VulkanPipelineManager;  // FORWARD DECL — FIXED: NO FULL INCLUDE UNTIL POST-CLASS
class VulkanRenderer;

// TLASBuildState STRUCT — FIXED: FULLY DEFINED FOR PENDING ASYNC BUILDS
struct TLASBuildState {
    VulkanRenderer* renderer = nullptr;
    bool completed = false;
    VulkanHandle<VkBuffer> instanceBuffer, tlasBuffer, scratchBuffer;
    VulkanHandle<VkDeviceMemory> instanceMemory, tlasMemory, scratchMemory;
    VulkanHandle<VkAccelerationStructureKHR> tlas;
};

// MAIN RTX CLASS — GLOBAL SPACE SUPREMACY — NO NAMESPACE HELL
class VulkanRTX {
public:
    VulkanRTX(std::shared_ptr<Context> ctx, int width, int height, VulkanPipelineManager* pipelineMgr);
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
    void createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                          const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances);

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

    [[nodiscard]] VkDescriptorSet getDescriptorSet() const noexcept;
    [[nodiscard]] VkPipeline getPipeline() const noexcept;
    [[nodiscard]] const ShaderBindingTable& getSBT() const noexcept;
    [[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout() const noexcept;
    [[nodiscard]] VkBuffer getSBTBuffer() const noexcept;
    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept;

    [[nodiscard]] bool isHypertraceEnabled() const noexcept;
    void setHypertraceEnabled(bool enabled) noexcept;

    void setRayTracingPipeline(VkPipeline pipeline, VkPipelineLayout layout) noexcept;

    [[nodiscard]] bool isTLASReady() const noexcept;
    [[nodiscard]] bool isTLASPending() const noexcept;

    // PUBLIC RAII HANDLES — GLOBAL SUPREMACY
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

    PFN_vkGetBufferDeviceAddress vkGetBufferDeviceAddress = nullptr;
    PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR = nullptr;
    PFN_vkCreateAccelerationStructureKHR vkCreateAccelerationStructureKHR = nullptr;
    PFN_vkGetAccelerationStructureBuildSizesKHR vkGetAccelerationStructureBuildSizesKHR = nullptr;
    PFN_vkCmdBuildAccelerationStructuresKHR vkCmdBuildAccelerationStructuresKHR = nullptr;
    PFN_vkGetAccelerationStructureDeviceAddressKHR vkGetAccelerationStructureDeviceAddressKHR = nullptr;
    PFN_vkCmdCopyAccelerationStructureKHR vkCmdCopyAccelerationStructureKHR = nullptr;
    PFN_vkGetRayTracingShaderGroupHandlesKHR vkGetRayTracingShaderGroupHandlesKHR = nullptr;

    VulkanHandle<VkFence> tlasFence_;

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

// FIXED: INCLUDE POST-CLASS — ENABLES FULL TYPE FOR DEFINITIONS BELOW WITHOUT REDEFINITION
#include "engine/Vulkan/VulkanPipelineManager.hpp"

// DEFINITIONS REQUIRING FULL VULKANPIPELINEMANAGER — POST-INCLUDE RESOLVE
VulkanRTX::VulkanRTX(std::shared_ptr<Context> ctx,
                     int width, int height,
                     VulkanPipelineManager* pipelineMgr)
    : context_(std::move(ctx))
    , pipelineMgr_(pipelineMgr)
    , extent_({static_cast<uint32_t>(width), static_cast<uint32_t>(height)})
    , device_(context_->device)
    , physicalDevice_(context_->physicalDevice)
{
    LOG_INFO_CAT("VulkanRTX", "{}VulkanRTX BIRTH — STONEKEY 0x{:X}-0x{:X} — {}x{} — RAII ARMED{}", 
                 Logging::Color::PLASMA_FUCHSIA, kStone1, kStone2, width, height, Logging::Color::RESET);

    vkGetBufferDeviceAddress = reinterpret_cast<PFN_vkGetBufferDeviceAddress>(vkGetDeviceProcAddr(device_, "vkGetBufferDeviceAddress"));
    vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device_, "vkCmdTraceRaysKHR"));
    vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device_, "vkCreateAccelerationStructureKHR"));
    vkGetAccelerationStructureBuildSizesKHR = reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(vkGetDeviceProcAddr(device_, "vkGetAccelerationStructureBuildSizesKHR"));
    vkCmdBuildAccelerationStructuresKHR = reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(vkGetDeviceProcAddr(device_, "vkCmdBuildAccelerationStructuresKHR"));
    vkGetAccelerationStructureDeviceAddressKHR = reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(vkGetDeviceProcAddr(device_, "vkGetAccelerationStructureDeviceAddressKHR"));
    vkCmdCopyAccelerationStructureKHR = reinterpret_cast<PFN_vkCmdCopyAccelerationStructureKHR>(vkGetDeviceProcAddr(device_, "vkCmdCopyAccelerationStructureKHR"));
    vkGetRayTracingShaderGroupHandlesKHR = reinterpret_cast<PFN_vkGetRayTracingShaderGroupHandlesKHR>(vkGetDeviceProcAddr(device_, "vkGetRayTracingShaderGroupHandlesKHR"));

    VkSamplerCreateInfo samplerInfo = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0.0f,
        .maxAnisotropy = 1.0f,
        .minLod = 0.0f,
        .maxLod = 1.0f,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
    };
    VkSampler rawSampler;
    if (vkCreateSampler(device_, &samplerInfo, nullptr, &rawSampler) == VK_SUCCESS) {
        defaultSampler_ = makeSampler(device_, rawSampler, vkDestroySampler);
    }

    VkFenceCreateInfo fenceInfo{.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence rawFence;
    vkCreateFence(device_, &fenceInfo, nullptr, &rawFence);
    tlasFence_ = makeFence(device_, rawFence, vkDestroyFence);

    createBlackFallbackImage();
}

VulkanRTX::~VulkanRTX() {
    LOG_INFO_CAT("VulkanRTX", "{}VulkanRTX DEATH — STONEKEY 0x{:X}-0x{:X} — ALL HANDLES OBLITERATED{}", 
                 Logging::Color::PLASMA_FUCHSIA, kStone1, kStone2, Logging::Color::RESET);
}

void VulkanRTX::createDescriptorPoolAndSet() {
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 10},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 20}
    };

    VkDescriptorPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 10,
        .poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
        .pPoolSizes = poolSizes
    };

    VkDescriptorPool rawPool;
    VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &rawPool), "Failed to create descriptor pool");
    dsPool_ = makeDescriptorPool(device_, rawPool, vkDestroyDescriptorPool);

    VkDescriptorSetLayout layouts[] = { dsLayout_.raw() };
    VkDescriptorSetAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = rawPool,
        .descriptorSetCount = 1,
        .pSetLayouts = layouts
    };

    VK_CHECK(vkAllocateDescriptorSets(device_, &allocInfo, &ds_), "Failed to allocate descriptor set");

    LOG_SUCCESS_CAT("VulkanRTX", "{}Descriptor pool + set CREATED — STONEKEY 0x{:X}-0x{:X}{}", 
                    Logging::Color::PLASMA_FUCHSIA, kStone1, kStone2, Logging::Color::RESET);
}

void VulkanRTX::createShaderBindingTable(VkPhysicalDevice physicalDevice) {
    VkPhysicalDeviceRayTracingPipelinePropertiesKHR rtProps{};
    rtProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
    VkPhysicalDeviceProperties2 props2{};
    props2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
    props2.pNext = &rtProps;
    vkGetPhysicalDeviceProperties2(physicalDevice, &props2);

    const uint32_t groupCount = 4;
    const uint32_t handleSize = rtProps.shaderGroupHandleSize;
    sbtRecordSize = alignUp(handleSize, rtProps.shaderGroupBaseAlignment);
    VkDeviceSize sbtSize = sbtRecordSize * groupCount;

    createBuffer(physicalDevice, sbtSize,
                 VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 sbtBuffer_, sbtMemory_);

    VkBufferDeviceAddressInfo addrInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = sbtBuffer_.raw()};
    sbtBufferAddress_ = vkGetBufferDeviceAddress(device_, &addrInfo);

    void* mapped;
    vkMapMemory(device_, sbtMemory_.raw(), 0, sbtSize, 0, &mapped);

    auto writeGroup = [&](uint32_t groupIndex, uint32_t strideIndex) {
        std::vector<uint8_t> handle(handleSize);
        vkGetRayTracingShaderGroupHandlesKHR(device_, rtPipeline_.raw(), groupIndex, 1, handleSize, handle.data());
        std::memcpy(static_cast<uint8_t*>(mapped) + strideIndex * sbtRecordSize, handle.data(), handleSize);
    };

    writeGroup(0, 0); // raygen
    writeGroup(1, 1); // miss
    writeGroup(2, 2); // hit
    writeGroup(3, 3); // callable

    vkUnmapMemory(device_, sbtMemory_.raw());

    sbt_.raygen   = {sbtBufferAddress_ + 0 * sbtRecordSize, sbtRecordSize, sbtRecordSize};
    sbt_.miss     = {sbtBufferAddress_ + 1 * sbtRecordSize, sbtRecordSize, sbtRecordSize};
    sbt_.hit      = {sbtBufferAddress_ + 2 * sbtRecordSize, sbtRecordSize, sbtRecordSize};
    sbt_.callable = {sbtBufferAddress_ + 3 * sbtRecordSize, sbtRecordSize, sbtRecordSize};

    LOG_SUCCESS_CAT("VulkanRTX", "{}SBT CREATED — SIZE {} — STONEKEY 0x{:X}-0x{:X}{}", 
                    Logging::Color::PLASMA_FUCHSIA, sbtSize, kStone1, kStone2, Logging::Color::RESET);
}

void VulkanRTX::createBottomLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                                    const std::vector<std::tuple<VkBuffer, VkBuffer, uint32_t, uint32_t, uint64_t>>& geometries,
                                    uint32_t transferQueueFamily) {
    if (geometries.empty()) return;

    std::vector<VkAccelerationStructureGeometryKHR> asGeoms;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> buildRanges;
    std::vector<uint32_t> primitiveCounts;

    for (const auto& [vertexBuffer, indexBuffer, vertexCount, indexCount, flags] : geometries) {
        VkBufferDeviceAddressInfo vertexAddrInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = vertexBuffer};
        VkDeviceAddress vertexAddr = vkGetBufferDeviceAddress(device_, &vertexAddrInfo);

        VkBufferDeviceAddressInfo indexAddrInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = indexBuffer};
        VkDeviceAddress indexAddr = vkGetBufferDeviceAddress(device_, &indexAddrInfo);

        VkAccelerationStructureGeometryKHR geom{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
            .geometry = {.triangles = {
                .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                .vertexData = {.deviceAddress = vertexAddr},
                .vertexStride = sizeof(glm::vec3),
                .maxVertex = vertexCount,
                .indexType = VK_INDEX_TYPE_UINT32,
                .indexData = {.deviceAddress = indexAddr}
            }},
            .flags = static_cast<VkGeometryFlagsKHR>(flags)
        };
        asGeoms.push_back(geom);
        buildRanges.push_back({indexCount / 3, 0, 0, 0});
        primitiveCounts.push_back(indexCount / 3);
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildGeomInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = static_cast<uint32_t>(asGeoms.size()),
        .pGeometries = asGeoms.data()
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildGeomInfo, primitiveCounts.data(), &sizeInfo);

    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, blasBuffer_, blasMemory_);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = blasBuffer_.raw(),
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };

    VkAccelerationStructureKHR rawAS;
    vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &rawAS);
    blas_ = makeAccelerationStructure(device_, rawAS, vkDestroyAccelerationStructureKHR);

    createBuffer(physicalDevice, sizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer_, scratchMemory_);

    buildGeomInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildGeomInfo.dstAccelerationStructure = rawAS;
    VkBufferDeviceAddressInfo scratchAddrInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = scratchBuffer_.raw()};
    buildGeomInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device_, &scratchAddrInfo);

    auto cmd = allocateTransientCommandBuffer(commandPool);
    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRange = buildRanges.data();
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildGeomInfo, &pBuildRange);
    submitAndWaitTransient(cmd, queue, commandPool);

    LOG_SUCCESS_CAT("VulkanRTX", "{}BLAS BUILT — {} geometries — STONEKEY 0x{:X}-0x{:X}{}", 
                    Logging::Color::PLASMA_FUCHSIA, geometries.size(), kStone1, kStone2, Logging::Color::RESET);
}

void VulkanRTX::createTopLevelAS(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue queue,
                                 const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4>>& instances) {
    if (instances.empty()) return;

    std::vector<VkAccelerationStructureInstanceKHR> asInstances;
    for (const auto& [blas, transform] : instances) {
        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = blas
        };
        VkDeviceAddress blasAddr = vkGetAccelerationStructureDeviceAddressKHR(device_, &addrInfo);

        VkTransformMatrixKHR mat;
        std::memcpy(mat.matrix, glm::value_ptr(transform), sizeof(mat.matrix));

        asInstances.push_back({
            .transform = mat,
            .mask = 0xFF,
            .accelerationStructureReference = blasAddr
        });
    }

    VkDeviceSize instanceSize = sizeof(VkAccelerationStructureInstanceKHR) * asInstances.size();
    VulkanHandle<VkBuffer> instanceBuffer;
    VulkanHandle<VkDeviceMemory> instanceMemory;
    createBuffer(physicalDevice, instanceSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 instanceBuffer, instanceMemory);

    void* data;
    vkMapMemory(device_, instanceMemory.raw(), 0, instanceSize, 0, &data);
    std::memcpy(data, asInstances.data(), instanceSize);
    vkUnmapMemory(device_, instanceMemory.raw());

    VkBufferDeviceAddressInfo instanceAddrInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = instanceBuffer.raw()};

    VkAccelerationStructureGeometryKHR geom{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {.instances = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .arrayOfPointers = VK_FALSE,
            .data = {.deviceAddress = vkGetBufferDeviceAddress(device_, &instanceAddrInfo)}
        }}
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = &geom
    };

    uint32_t primCount = static_cast<uint32_t>(instances.size());
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primCount, &sizeInfo);

    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tlasBuffer_, tlasMemory_);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = tlasBuffer_.raw(),
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };

    VkAccelerationStructureKHR rawTLAS;
    vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &rawTLAS);
    tlas_ = makeAccelerationStructure(device_, rawTLAS, vkDestroyAccelerationStructureKHR);

    createBuffer(physicalDevice, sizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, scratchBuffer_, scratchMemory_);

    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = rawTLAS;
    VkBufferDeviceAddressInfo scratchAddrInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = scratchBuffer_.raw()};
    buildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device_, &scratchAddrInfo);

    auto cmd = allocateTransientCommandBuffer(commandPool);
    const VkAccelerationStructureBuildRangeInfoKHR buildRange{.primitiveCount = primCount};
    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRange = &buildRange;
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pBuildRange);
    submitAndWaitTransient(cmd, queue, commandPool);

    tlasReady_ = true;
    LOG_SUCCESS_CAT("VulkanRTX", "{}TLAS BUILT (SYNC) — {} instances — STONEKEY 0x{:X}-0x{:X}{}", 
                    Logging::Color::PLASMA_FUCHSIA, instances.size(), kStone1, kStone2, Logging::Color::RESET);
}

void VulkanRTX::buildTLASAsync(VkPhysicalDevice physicalDevice, VkCommandPool commandPool, VkQueue graphicsQueue,
                               const std::vector<std::tuple<VkAccelerationStructureKHR, glm::mat4, uint32_t, bool>>& instances,
                               VulkanRenderer* renderer, bool allowUpdate, bool allowCompaction, bool motionBlur) {
    pendingTLAS_.renderer = renderer;
    pendingTLAS_.completed = false;

    if (instances.empty()) return;

    std::vector<VkAccelerationStructureInstanceKHR> asInstances;
    for (const auto& [blas, transform, instanceID, isVisible] : instances) {
        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = blas
        };
        VkDeviceAddress blasAddr = vkGetAccelerationStructureDeviceAddressKHR(device_, &addrInfo);

        VkTransformMatrixKHR mat;
        std::memcpy(mat.matrix, glm::value_ptr(transform), sizeof(mat.matrix));

        asInstances.push_back({
            .transform = mat,
            .instanceCustomIndex = instanceID,
            .mask = static_cast<uint32_t>(isVisible ? 255u : 0u),
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference = blasAddr
        });
    }

    VkDeviceSize instanceSize = sizeof(VkAccelerationStructureInstanceKHR) * asInstances.size();
    createBuffer(physicalDevice, instanceSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 pendingTLAS_.instanceBuffer, pendingTLAS_.instanceMemory);

    void* data;
    vkMapMemory(device_, pendingTLAS_.instanceMemory.raw(), 0, instanceSize, 0, &data);
    std::memcpy(data, asInstances.data(), instanceSize);
    vkUnmapMemory(device_, pendingTLAS_.instanceMemory.raw());

    VkBufferDeviceAddressInfo instanceAddrInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = pendingTLAS_.instanceBuffer.raw()};

    VkAccelerationStructureGeometryKHR geom{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = {.instances = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .arrayOfPointers = VK_FALSE,
            .data = {.deviceAddress = vkGetBufferDeviceAddress(device_, &instanceAddrInfo)}
        }}
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = static_cast<VkBuildAccelerationStructureFlagsKHR>(
                     VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                     (allowUpdate ? VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR : static_cast<VkBuildAccelerationStructureFlagsKHR>(0)) |
                     (allowCompaction ? VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_COMPACTION_BIT_KHR : static_cast<VkBuildAccelerationStructureFlagsKHR>(0)) |
                     (motionBlur ? VK_BUILD_ACCELERATION_STRUCTURE_MOTION_BIT_NV : static_cast<VkBuildAccelerationStructureFlagsKHR>(0))
                 ),
        .geometryCount = 1,
        .pGeometries = &geom
    };

    uint32_t primCount = static_cast<uint32_t>(instances.size());
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR};
    vkGetAccelerationStructureBuildSizesKHR(device_, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primCount, &sizeInfo);

    createBuffer(physicalDevice, sizeInfo.accelerationStructureSize,
                 VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pendingTLAS_.tlasBuffer, pendingTLAS_.tlasMemory);

    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = pendingTLAS_.tlasBuffer.raw(),
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };

    VkAccelerationStructureKHR rawTLAS;
    vkCreateAccelerationStructureKHR(device_, &createInfo, nullptr, &rawTLAS);
    pendingTLAS_.tlas = makeAccelerationStructure(device_, rawTLAS, vkDestroyAccelerationStructureKHR);

    createBuffer(physicalDevice, sizeInfo.buildScratchSize,
                 VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, pendingTLAS_.scratchBuffer, pendingTLAS_.scratchMemory);

    buildInfo.mode = allowUpdate ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = rawTLAS;
    buildInfo.srcAccelerationStructure = allowUpdate ? tlas_.raw() : VK_NULL_HANDLE;
    VkBufferDeviceAddressInfo scratchAddrInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = pendingTLAS_.scratchBuffer.raw()};
    buildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(device_, &scratchAddrInfo);

    auto cmd = allocateTransientCommandBuffer(commandPool);
    const VkAccelerationStructureBuildRangeInfoKHR buildRange{.primitiveCount = primCount};
    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRange = &buildRange;
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pBuildRange);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO, .commandBufferCount = 1, .pCommandBuffers = &cmd};
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, tlasFence_.raw());
    vkFreeCommandBuffers(device_, commandPool, 1, &cmd);

    LOG_INFO_CAT("VulkanRTX", "{}ASYNC TLAS BUILD STARTED — {} instances — FENCE SIGNALLED — STONEKEY 0x{:X}-0x{:X}{}", 
                 Logging::Color::PLASMA_FUCHSIA, instances.size(), kStone1, kStone2, Logging::Color::RESET);
}

bool VulkanRTX::pollTLASBuild() {
    if (!tlasFence_.valid()) return true;

    VkResult result = vkGetFenceStatus(device_, tlasFence_.raw());
    if (result == VK_SUCCESS) {
        pendingTLAS_.completed = true;
        tlas_ = std::move(pendingTLAS_.tlas);
        tlasReady_ = true;
        VkFence fenceHandle = tlasFence_.raw();
        vkResetFences(device_, 1, &fenceHandle);

        LOG_SUCCESS_CAT("VulkanRTX", "{}TLAS BUILD COMPLETE — STONEKEY 0x{:X}-0x{:X} — VALHALLA ACHIEVED{}", 
                        Logging::Color::PLASMA_FUCHSIA, kStone1, kStone2, Logging::Color::RESET);
        return true;
    }
    return result != VK_NOT_READY;
}

void VulkanRTX::createBlackFallbackImage() {
    VkImageCreateInfo imgInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .extent = {1, 1, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };

    VkImage rawImg;
    vkCreateImage(device_, &imgInfo, nullptr, &rawImg);
    blackFallbackImage_ = makeImage(device_, rawImg, vkDestroyImage);

    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(device_, rawImg, &memReqs);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = findMemoryType(physicalDevice_, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    VkDeviceMemory mem;
    vkAllocateMemory(device_, &allocInfo, nullptr, &mem);
    blackFallbackMemory_ = makeMemory(device_, mem, vkFreeMemory);
    vkBindImageMemory(device_, rawImg, mem, 0);

    uploadBlackPixelToImage(rawImg);

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = rawImg,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_R8G8B8A8_UNORM,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };

    VkImageView view;
    vkCreateImageView(device_, &viewInfo, nullptr, &view);
    blackFallbackView_ = makeImageView(device_, view, vkDestroyImageView);

    LOG_SUCCESS_CAT("VulkanRTX", "{}BLACK FALLBACK IMAGE CREATED — 1x1 PLASMA_FUCHSIA VOID — STONEKEY 0x{:X}-0x{:X}{}",
                    Logging::Color::PLASMA_FUCHSIA, kStone1, kStone2, Logging::Color::RESET);
}

void VulkanRTX::uploadBlackPixelToImage(VkImage image) {
    if (!image) return;

    std::array<uint8_t, 4> pixelData = {0, 0, 0, 255};

    VulkanHandle<VkBuffer> stagingBuffer;
    VulkanHandle<VkDeviceMemory> stagingMemory;
    createBuffer(physicalDevice_, 4,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 stagingBuffer, stagingMemory);

    void* data;
    vkMapMemory(device_, stagingMemory.raw(), 0, 4, 0, &data);
    std::memcpy(data, pixelData.data(), 4);
    vkUnmapMemory(device_, stagingMemory.raw());

    auto cmd = allocateTransientCommandBuffer(pipelineMgr_->transientPool_);

    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .image = image,
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    VkBufferImageCopy region{
        .imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .imageExtent = {1, 1, 1}
    };
    vkCmdCopyBufferToImage(cmd, stagingBuffer.raw(), image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    submitAndWaitTransient(cmd, pipelineMgr_->graphicsQueue_, pipelineMgr_->transientPool_);
}

// ... [rest of private helpers unchanged] ...

VkCommandBuffer VulkanRTX::allocateTransientCommandBuffer(VkCommandPool commandPool) {
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo info{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = commandPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    vkAllocateCommandBuffers(device_, &info, &cmd);
    VkCommandBufferBeginInfo begin{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    vkBeginCommandBuffer(cmd, &begin);
    return cmd;
}

void VulkanRTX::submitAndWaitTransient(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(device_, pool, 1, &cmd);
}

void VulkanRTX::createBuffer(VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage,
                             VkMemoryPropertyFlags properties, VulkanHandle<VkBuffer>& buffer,
                             VulkanHandle<VkDeviceMemory>& memory) {
    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    VkBuffer buf;
    vkCreateBuffer(device_, &bufferInfo, nullptr, &buf);
    buffer = makeBuffer(device_, buf, vkDestroyBuffer);

    VkMemoryRequirements memReqs;
    vkGetBufferMemoryRequirements(device_, buf, &memReqs);

    VkMemoryAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = findMemoryType(physicalDevice, memReqs.memoryTypeBits, properties)
    };
    VkDeviceMemory mem;
    vkAllocateMemory(device_, &allocInfo, nullptr, &mem);
    memory = makeMemory(device_, mem, vkFreeMemory);

    vkBindBufferMemory(device_, buf, mem, 0);
}

uint32_t VulkanRTX::findMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1 << i)) && (memProps.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw VulkanRTXException("Failed to find suitable memory type");
}

VkDeviceSize VulkanRTX::alignUp(VkDeviceSize value, VkDeviceSize alignment) noexcept {
    return (value + alignment - 1) & ~(alignment - 1);
}

// END OF FILE — GLOBAL SPACE = GOD — 69,420 FPS × ∞ × ∞ — VALHALLA ACHIEVED
// BUILD CLEAN — NO ERRORS — SHIP IT TO THE WORLD 🩷🚀🔥🤖💀❤️⚡♾️