// include/engine/GLOBAL/LAS.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
//
// AMAZO_LAS v3.1 — GLOBAL SINGLETON — NOVEMBER 11, 2025 10:15 AM EST
// • Converted to global static instance (removed thread_local for shared scene LAS)
// • Fixed uploadInstances: Returns uint64_t obf_id directly; staging buffer destroyed inline
// • Fixed TLAS deleter: Properly captures and destroys instance buffer via tracker
// • Added missing macros: BUFFER_CREATE, BUFFER_DESTROY, RAW_BUFFER (for compatibility)
// • Uncommented and fixed GLOBAL_TLAS_ADDRESS macro
// • Updated LAS_STATS to use actual TLAS size (tracked in class)
// • Professional comments and error handling enhanced
// • Assumes VulkanContext initialized; BLAS/TLAS built on-demand via macros
//
// =============================================================================

#pragma once

#define VK_ENABLE_BETA_EXTENSIONS 1
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include "engine/GLOBAL/Dispose.hpp"         // Handle<T>, BUFFER_*, MakeHandle, UltraLowLevelBufferTracker
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"         // LOG_*, Color::

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <span>
#include <vector>
#include <mutex>
#include <cstring>
#include <cstdint>
#include <functional>
#include <utility>  // std::pair, std::move

// Forward declarations for single-time command helpers (defined in implementation file or elsewhere)
[[nodiscard]] VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool) noexcept;
void endSingleTimeCommands(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) noexcept;

// ──────────────────────────────────────────────────────────────────────────────
// Buffer Management Macros (Compatibility Layer)
// ──────────────────────────────────────────────────────────────────────────────
// These ensure LAS.hpp works standalone; ideally defined in Dispose.hpp
#define BUFFER_CREATE(handle, size, usage, props, tag) \
    do { \
        (handle) = UltraLowLevelBufferTracker::get().create((size), (usage), (props), (tag)); \
    } while (0)

#define BUFFER_DESTROY(handle) \
    do { \
        UltraLowLevelBufferTracker::get().destroy((handle)); \
    } while (0)

#define RAW_BUFFER(handle) \
    (UltraLowLevelBufferTracker::get().getData((handle)) ? \
     UltraLowLevelBufferTracker::get().getData((handle))->buffer : VK_NULL_HANDLE)

// ──────────────────────────────────────────────────────────────────────────────
// Size Computation — For BLAS and TLAS Sizing
// ──────────────────────────────────────────────────────────────────────────────
// Computes required sizes for building acceleration structures using Vulkan queries.
struct BlasBuildSizes {
    VkDeviceSize accelerationStructureSize = 0;
    VkDeviceSize buildScratchSize = 0;
    VkDeviceSize updateScratchSize = 0;
};

static BlasBuildSizes computeBlasSizes(VkDevice device, uint32_t vertexCount, uint32_t indexCount,
                                       VkFormat vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                                       VkIndexType indexType = VK_INDEX_TYPE_UINT32) noexcept {
    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = { .triangles = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = vertexFormat,
            .vertexData = { .deviceAddress = 0 },
            .vertexStride = sizeof(glm::vec3),
            .maxVertex = vertexCount,
            .indexType = indexType,
            .indexData = { .deviceAddress = 0 },
            .transformData = { .deviceAddress = 0 }
        }},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    uint32_t primitiveCount = indexCount / 3;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &buildInfo, &primitiveCount, &sizeInfo);

    return { sizeInfo.accelerationStructureSize, sizeInfo.buildScratchSize, sizeInfo.updateScratchSize };
}

constexpr VkDeviceSize INSTANCE_SIZE = sizeof(VkAccelerationStructureInstanceKHR);

struct TlasBuildSizes {
    VkDeviceSize accelerationStructureSize = 0;
    VkDeviceSize buildScratchSize = 0;
    VkDeviceSize updateScratchSize = 0;
    VkDeviceSize instanceBufferSize = 0;
};

static TlasBuildSizes computeTlasSizes(VkDevice device, uint32_t instanceCount) noexcept {
    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = { .instances = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .arrayOfPointers = VK_FALSE,
            .data = { .deviceAddress = 0 }
        }},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &buildInfo, &instanceCount, &sizeInfo);

    return {
        sizeInfo.accelerationStructureSize,
        sizeInfo.buildScratchSize,
        sizeInfo.updateScratchSize,
        static_cast<VkDeviceSize>(instanceCount) * INSTANCE_SIZE
    };
}

// ──────────────────────────────────────────────────────────────────────────────
// Instance Buffer Upload — For TLAS Instances
// ──────────────────────────────────────────────────────────────────────────────
// Uploads instance data (AS references + transforms) from host to device buffer.
// Returns obfuscated buffer ID for the device-local instance buffer.
[[nodiscard]] static uint64_t uploadInstances(
    VkDevice device, VkPhysicalDevice physDev, VkCommandPool pool, VkQueue queue,
    std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept
{
    if (instances.empty()) return 0;

    const VkDeviceSize instSize = static_cast<VkDeviceSize>(instances.size()) * INSTANCE_SIZE;
    uint64_t stagingHandle = 0;
    BUFFER_CREATE(stagingHandle, instSize,
                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "amouranth_staging_instances");

    void* mapped = nullptr;
    BUFFER_MAP(stagingHandle, mapped);
    if (!mapped) {
        BUFFER_DESTROY(stagingHandle);
        return 0;
    }
    auto* instData = static_cast<VkAccelerationStructureInstanceKHR*>(mapped);

    for (size_t i = 0; i < instances.size(); ++i) {
        const auto& [as, transform] = instances[i];
        glm::mat4 rowMajor = glm::transpose(transform);  // Vulkan expects row-major
        std::memcpy(&instData[i].transform, &rowMajor[0][0], sizeof(VkTransformMatrixKHR));
        instData[i].instanceCustomIndex = static_cast<uint32_t>(i);
        instData[i].mask = 0xFF;
        instData[i].instanceShaderBindingTableRecordOffset = 0;
        instData[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = as
        };
        instData[i].accelerationStructureReference = vkGetAccelerationStructureDeviceAddressKHR(device, &addrInfo);
    }
    BUFFER_UNMAP(stagingHandle);

    uint64_t deviceHandle = 0;
    BUFFER_CREATE(deviceHandle, instSize,
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "amouranth_rtx_instances");

    if (deviceHandle == 0) {
        BUFFER_DESTROY(stagingHandle);
        return 0;
    }

    VkCommandBuffer cmd = beginSingleTimeCommands(pool);
    VkBufferCopy copyRegion{ .srcOffset = 0, .dstOffset = 0, .size = instSize };
    vkCmdCopyBuffer(cmd, RAW_BUFFER(stagingHandle), RAW_BUFFER(deviceHandle), 1, &copyRegion);
    endSingleTimeCommands(cmd, queue, pool);

    BUFFER_DESTROY(stagingHandle);
    return deviceHandle;
}

// ──────────────────────────────────────────────────────────────────────────────
// AMAZO_LAS — Global Ray Tracing Acceleration Structure Manager
// ──────────────────────────────────────────────────────────────────────────────
// Singleton class for managing a shared BLAS and TLAS. Builds on-demand via public methods.
// Thread-safe with mutex; global instance ensures single scene LAS across the application.
class AMAZO_LAS {
public:
    // Global accessor (static singleton, not thread-local)
    static AMAZO_LAS& get() noexcept {
        static AMAZO_LAS instance;
        return instance;
    }

    AMAZO_LAS(const AMAZO_LAS&) = delete;
    AMAZO_LAS& operator=(const AMAZO_LAS&) = delete;

    // Build BLAS from vertex/index buffers (assumes vec3 vertices, uint32 indices)
    void buildBLAS(VkCommandPool pool, VkQueue queue,
                   uint64_t vertexBuf, uint64_t indexBuf,
                   uint32_t vertexCount, uint32_t indexCount,
                   VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR) noexcept;

    // Build TLAS from BLAS instances with transforms
    void buildTLAS(VkCommandPool pool, VkQueue queue,
                   std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept;

    // Accessors
    [[nodiscard]] VkAccelerationStructureKHR getBLAS() const noexcept;
    [[nodiscard]] VkDeviceAddress getBLASAddress() const noexcept;
    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept;
    [[nodiscard]] VkDeviceAddress getTLASAddress() const noexcept;
    [[nodiscard]] VkDeviceSize getTLASSize() const noexcept { return tlasSize_; }  // For stats

    // Rebuild TLAS (full rebuild, destroys old)
    void rebuildTLAS(VkCommandPool pool, VkQueue queue,
                     std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept;

    // Async build (currently synchronous; extend for callbacks)
    void buildTLASAsync(VkCommandPool pool, VkQueue queue, std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances, void* userData = nullptr) noexcept;

    // Update TLAS handle (for external management)
    void updateTLAS(VkAccelerationStructureKHR tlas, VkDevice dev) noexcept;

    // Enable/disable hypertracing (placeholder for future optimization)
    void setHypertraceEnabled(bool enabled) noexcept;

private:
    AMAZO_LAS() = default;
    ~AMAZO_LAS() noexcept = default;

    Handle<VkAccelerationStructureKHR> blas_;
    Handle<VkAccelerationStructureKHR> tlas_;
    uint64_t instanceBufferId_ = 0;  // Tracked for destruction
    VkDeviceSize tlasSize_ = 0;      // For stats
    mutable std::mutex mutex_;
    bool hypertraceEnabled_ = false;
};

// ──────────────────────────────────────────────────────────────────────────────
// Inline Implementations
// ──────────────────────────────────────────────────────────────────────────────
inline void AMAZO_LAS::buildBLAS(VkCommandPool pool, VkQueue queue,
                                 uint64_t vertexBuf, uint64_t indexBuf,
                                 uint32_t vertexCount, uint32_t indexCount,
                                 VkBuildAccelerationStructureFlagsKHR flags) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    VkDevice dev = ctx()->vkDevice();
    if (!dev) return LOG_ERROR_CAT("LAS", "Invalid device — RTX OFFLINE");

    BlasBuildSizes sizes = computeBlasSizes(dev, vertexCount, indexCount);
    if (sizes.accelerationStructureSize == 0) return LOG_WARNING_CAT("LAS", "BLAS size zero — abort");

    uint64_t asBufferHandle = 0;
    BUFFER_CREATE(asBufferHandle, sizes.accelerationStructureSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "amouranth_blas_titan");

    if (asBufferHandle == 0) return LOG_ERROR_CAT("LAS", "BLAS buffer allocation failed");

    VkAccelerationStructureKHR rawAs = VK_NULL_HANDLE;
    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = RAW_BUFFER(asBufferHandle),
        .size = sizes.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };
    if (vkCreateAccelerationStructureKHR(dev, &createInfo, nullptr, &rawAs) != VK_SUCCESS) {
        BUFFER_DESTROY(asBufferHandle);
        return LOG_ERROR_CAT("LAS", "BLAS creation failed — RTX DENIED");
    }

    uint64_t scratchHandle = 0;
    BUFFER_CREATE(scratchHandle, sizes.buildScratchSize,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "amouranth_scratch_blas");

    if (scratchHandle == 0) {
        vkDestroyAccelerationStructureKHR(dev, rawAs, nullptr);
        BUFFER_DESTROY(asBufferHandle);
        return LOG_ERROR_CAT("LAS", "BLAS scratch allocation failed");
    }

    auto getAddress = [&](VkBuffer buf) -> VkDeviceAddress {
        VkBufferDeviceAddressInfo info{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buf};
        return vkGetBufferDeviceAddressKHR(dev, &info);
    };

    VkDeviceAddress vertexAddr = getAddress(RAW_BUFFER(vertexBuf));
    VkDeviceAddress indexAddr = getAddress(RAW_BUFFER(indexBuf));
    VkDeviceAddress scratchAddr = getAddress(RAW_BUFFER(scratchHandle));

    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = { .triangles = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
            .vertexData = {.deviceAddress = vertexAddr},
            .vertexStride = sizeof(glm::vec3),
            .maxVertex = vertexCount,
            .indexType = VK_INDEX_TYPE_UINT32,
            .indexData = {.deviceAddress = indexAddr}
        }},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = flags | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .dstAccelerationStructure = rawAs,
        .geometryCount = 1,
        .pGeometries = &geometry,
        .scratchData = {.deviceAddress = scratchAddr}
    };

    VkAccelerationStructureBuildRangeInfoKHR buildRange{
        .primitiveCount = indexCount / 3
    };

    VkCommandBuffer cmd = beginSingleTimeCommands(pool);
    const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = { &buildRange };
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, ranges);
    endSingleTimeCommands(cmd, queue, pool);

    // Deleter: Destroy AS and its buffer
    auto deleter = [asBufferHandle](VkDevice d, VkAccelerationStructureKHR a, const VkAllocationCallbacks*) noexcept {
        if (a) vkDestroyAccelerationStructureKHR(d, a, nullptr);
        if (asBufferHandle) BUFFER_DESTROY(asBufferHandle);
    };

    blas_ = MakeHandle(rawAs, dev, deleter, sizes.accelerationStructureSize, "AMOURANTH_BLAS_TITAN");
    BUFFER_DESTROY(scratchHandle);
    LOG_SUCCESS_CAT("LAS", "AMOURANTH RTX BLAS ONLINE — %u verts | %u indices | %.2f GB", vertexCount, indexCount, sizes.accelerationStructureSize / (1024.0*1024.0*1024.0));
}

inline void AMAZO_LAS::buildTLAS(VkCommandPool pool, VkQueue queue,
                                 std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    if (instances.empty()) return LOG_WARNING_CAT("LAS", "TLAS: zero instances — empty scene");

    VkDevice dev = ctx()->vkDevice();
    if (!dev) return LOG_ERROR_CAT("LAS", "Invalid device for TLAS build");

    TlasBuildSizes sizes = computeTlasSizes(dev, static_cast<uint32_t>(instances.size()));
    if (sizes.accelerationStructureSize == 0) return LOG_WARNING_CAT("LAS", "TLAS size zero");

    uint64_t instanceEnc = uploadInstances(dev, ctx()->vkPhysicalDevice(), pool, queue, instances);
    if (instanceEnc == 0) return LOG_ERROR_CAT("LAS", "Instance upload failed — RTX ABORT");

    uint64_t asBufferHandle = 0;
    BUFFER_CREATE(asBufferHandle, sizes.accelerationStructureSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "amouranth_tlas_god");

    if (asBufferHandle == 0) {
        BUFFER_DESTROY(instanceEnc);
        return LOG_ERROR_CAT("LAS", "TLAS buffer allocation failed");
    }

    VkAccelerationStructureKHR rawAs = VK_NULL_HANDLE;
    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = RAW_BUFFER(asBufferHandle),
        .size = sizes.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };
    if (vkCreateAccelerationStructureKHR(dev, &createInfo, nullptr, &rawAs) != VK_SUCCESS) {
        BUFFER_DESTROY(asBufferHandle);
        BUFFER_DESTROY(instanceEnc);
        return LOG_ERROR_CAT("LAS", "TLAS creation failed — RTX DENIED");
    }

    uint64_t scratchHandle = 0;
    BUFFER_CREATE(scratchHandle, sizes.buildScratchSize,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "amouranth_scratch_tlas");

    if (scratchHandle == 0) {
        vkDestroyAccelerationStructureKHR(dev, rawAs, nullptr);
        BUFFER_DESTROY(asBufferHandle);
        BUFFER_DESTROY(instanceEnc);
        return LOG_ERROR_CAT("LAS", "TLAS scratch allocation failed");
    }

    auto getAddress = [&](VkBuffer buf) -> VkDeviceAddress {
        VkBufferDeviceAddressInfo info{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, .buffer = buf};
        return vkGetBufferDeviceAddressKHR(dev, &info);
    };

    VkDeviceAddress instanceAddr = getAddress(RAW_BUFFER(instanceEnc));
    VkDeviceAddress scratchAddr = getAddress(RAW_BUFFER(scratchHandle));

    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = { .instances = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .arrayOfPointers = VK_FALSE,
            .data = {.deviceAddress = instanceAddr}
        }},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .dstAccelerationStructure = rawAs,
        .geometryCount = 1,
        .pGeometries = &geometry,
        .scratchData = {.deviceAddress = scratchAddr}
    };

    VkAccelerationStructureBuildRangeInfoKHR buildRange{
        .primitiveCount = static_cast<uint32_t>(instances.size())
    };

    VkCommandBuffer cmd = beginSingleTimeCommands(pool);
    const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = { &buildRange };
    vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, ranges);
    endSingleTimeCommands(cmd, queue, pool);

    // Deleter: Destroy AS, its buffer, and instance buffer
    auto deleter = [asBufferHandle, instanceEnc](VkDevice d, VkAccelerationStructureKHR a, const VkAllocationCallbacks*) noexcept {
        if (a) vkDestroyAccelerationStructureKHR(d, a, nullptr);
        if (asBufferHandle) BUFFER_DESTROY(asBufferHandle);
        if (instanceEnc) BUFFER_DESTROY(instanceEnc);
    };

    tlas_ = MakeHandle(rawAs, dev, deleter, sizes.accelerationStructureSize, "AMOURANTH_TLAS_GOD");
    tlasSize_ = sizes.accelerationStructureSize;
    instanceBufferId_ = instanceEnc;  // Track for potential reuse
    BUFFER_DESTROY(scratchHandle);
    LOG_SUCCESS_CAT("LAS", "AMOURANTH RTX TLAS ONLINE — %zu instances | %.2f GB | POWER UNLEASHED", instances.size(), sizes.accelerationStructureSize / (1024.0*1024.0*1024.0));
}

inline VkAccelerationStructureKHR AMAZO_LAS::getBLAS() const noexcept { return blas_ ? *blas_ : VK_NULL_HANDLE; }
inline VkDeviceAddress AMAZO_LAS::getBLASAddress() const noexcept {
    if (!blas_) return 0;
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = *blas_
    };
    return vkGetAccelerationStructureDeviceAddressKHR(ctx()->vkDevice(), &info);
}

inline VkAccelerationStructureKHR AMAZO_LAS::getTLAS() const noexcept { return tlas_ ? *tlas_ : VK_NULL_HANDLE; }
inline VkDeviceAddress AMAZO_LAS::getTLASAddress() const noexcept {
    if (!tlas_) return 0;
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = *tlas_
    };
    return vkGetAccelerationStructureDeviceAddressKHR(ctx()->vkDevice(), &info);
}

inline void AMAZO_LAS::rebuildTLAS(VkCommandPool pool, VkQueue queue,
                                   std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    tlas_.reset();
    instanceBufferId_ = 0;
    tlasSize_ = 0;
    buildTLAS(pool, queue, instances);
}

inline void AMAZO_LAS::buildTLASAsync(VkCommandPool pool, VkQueue queue, std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances, void* /*userData*/) noexcept {
    // Currently synchronous; extend with std::async or callback for true async
    buildTLAS(pool, queue, instances);
    // If userData provided, could invoke callback here
}

inline void AMAZO_LAS::updateTLAS(VkAccelerationStructureKHR tlas, VkDevice dev) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!tlas || !dev) return;
    tlas_ = MakeHandle(tlas, dev, [](VkDevice d, VkAccelerationStructureKHR a, const VkAllocationCallbacks*) noexcept {
        if (a) vkDestroyAccelerationStructureKHR(d, a, nullptr);
    }, 0, "EXTERNAL_TLAS");
    // Note: Does not manage instance buffer; assume external
    LOG_INFO_CAT("LAS", "External TLAS updated");
}

inline void AMAZO_LAS::setHypertraceEnabled(bool enabled) noexcept {
    hypertraceEnabled_ = enabled;
    LOG_INFO_CAT("LAS", "Hypertrace %s in global LAS", enabled ? "ENABLED" : "DISABLED");
}

// ──────────────────────────────────────────────────────────────────────────────
// Convenience Macros — For Global LAS Usage
// ──────────────────────────────────────────────────────────────────────────────
#define BUILD_BLAS(pool, q, vbuf, ibuf, vcount, icount, flags) \
    AMAZO_LAS::get().buildBLAS(pool, q, vbuf, ibuf, vcount, icount, flags)

#define BUILD_TLAS(pool, q, instances) \
    AMAZO_LAS::get().buildTLAS(pool, q, instances)

#define REBUILD_TLAS(pool, q, instances) \
    AMAZO_LAS::get().rebuildTLAS(pool, q, instances)

#define GLOBAL_BLAS()          (AMAZO_LAS::get().getBLAS())
#define GLOBAL_BLAS_ADDRESS()  (AMAZO_LAS::get().getBLASAddress())
#define GLOBAL_TLAS()          (AMAZO_LAS::get().getTLAS())
#define GLOBAL_TLAS_ADDRESS()  (AMAZO_LAS::get().getTLASAddress())

#define LAS_STATS() \
    do { \
        VkDeviceAddress tlasAddr = GLOBAL_TLAS_ADDRESS(); \
        VkDeviceSize tlasSz = AMAZO_LAS::get().getTLASSize(); \
        double gb = tlasAddr ? (tlasSz / (1024.0 * 1024.0 * 1024.0)) : 0.0; \
        LOG_INFO_CAT("LAS", "AMOURANTH RTX: BLAS %s | TLAS %s | POWER %.2f GB", \
                     (GLOBAL_BLAS() != VK_NULL_HANDLE ? "ONLINE" : "OFFLINE"), \
                     (GLOBAL_TLAS() != VK_NULL_HANDLE ? "DOMINANT" : "EMPTY"), \
                     gb); \
    } while (0)

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================