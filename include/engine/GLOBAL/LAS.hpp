// =============================================================================
// LAS.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Acceleration Structure Management for Ray Tracing.
// Provides a single global Bottom-Level Acceleration Structure (BLAS) and
// Top-Level Acceleration Structure (TLAS), along with Shader Binding Table (SBT)
// support. Fully compliant with Vulkan 1.3+ Ray Tracing extensions (VK_KHR_ray_tracing_pipeline).
// Header-only implementation for streamlined integration.
//
// Features:
// - Single static BLAS for primary geometry and dynamic per-frame TLAS rebuilds.
// - Automatic buffer management via BufferManager integration.
// - Thread-safe singleton with std::mutex protection.
// - RAII-based resource handling with custom deleters for zero leaks.
// - Optimized build flags for fast tracing (VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR).
// - Device address querying for shader usage.
// - Convenience macros for build and query operations.
// - Logging integration for build success/warnings.
//
// Licensed under Creative Commons Attribution-NonCommercial 4.0 International
// (CC BY-NC 4.0) for non-commercial use. See https://creativecommons.org/licenses/by-nc/4.0/.
// For commercial licensing, contact gzac5314@gmail.com.
//
// =============================================================================

#pragma once

#define VK_ENABLE_BETA_EXTENSIONS 1
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/Dispose.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/Vulkan/VulkanContext.hpp"
#include "engine/Vulkan/VulkanHandles.hpp"

#include <glm/glm.hpp>
#include <span>
#include <vector>
#include <mutex>
#include <cstring>
#include <cstdint>

// Forward declaration for single-time command utilities.
namespace Vulkan {
    [[nodiscard]] VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool) noexcept;
    void endSingleTimeCommands(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) noexcept;
}

// =============================================================================
// Shader Binding Table (SBT) Structure
// =============================================================================
// Global namespace placement for direct usage (e.g., plain ShaderBindingTable in shaders/pipelines).
struct ShaderBindingTable {
    VkStridedDeviceAddressRegionKHR raygen{};   // Ray generation shaders.
    VkStridedDeviceAddressRegionKHR miss{};     // Miss shaders.
    VkStridedDeviceAddressRegionKHR hit{};      // Hit/closest-hit shaders.
    VkStridedDeviceAddressRegionKHR callable{}; // Callable shaders.

    [[nodiscard]] bool empty() const noexcept {
        return raygen.size == 0 && miss.size == 0 && hit.size == 0 && callable.size == 0;
    }
};

// =============================================================================
// BLAS Build Size Computation
// =============================================================================
struct BlasBuildSizes {
    VkDeviceSize accelerationStructureSize = 0;
    VkDeviceSize buildScratchSize = 0;
    VkDeviceSize updateScratchSize = 0;
};

static BlasBuildSizes computeBlasSizes(VkDevice device, uint32_t vertexCount, uint32_t indexCount,
                                       VkFormat vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                                       VkIndexType indexType = VK_INDEX_TYPE_UINT32) noexcept {
    VkAccelerationStructureGeometryKHR geometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR
    };

    VkAccelerationStructureGeometryTrianglesDataKHR triangles = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = vertexFormat,
        .vertexData = { .deviceAddress = 0 },
        .vertexStride = sizeof(glm::vec3),
        .maxVertex = vertexCount,
        .indexType = indexType,
        .indexData = { .deviceAddress = 0 },
        .transformData = { .deviceAddress = 0 }
    };
    geometry.geometry.triangles = triangles;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    uint32_t primitiveCount = indexCount / 3;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    vkGetAccelerationStructureBuildSizesKHR(device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                            &buildInfo, &primitiveCount, &sizeInfo);

    return { sizeInfo.accelerationStructureSize, sizeInfo.buildScratchSize, sizeInfo.updateScratchSize };
}

// =============================================================================
// TLAS Build Size Computation and Instance Upload
// =============================================================================
constexpr VkDeviceSize INSTANCE_SIZE = sizeof(VkAccelerationStructureInstanceKHR);

struct TlasBuildSizes {
    VkDeviceSize accelerationStructureSize = 0;
    VkDeviceSize buildScratchSize = 0;
    VkDeviceSize updateScratchSize = 0;
    VkDeviceSize instanceBufferSize = 0;
};

static TlasBuildSizes computeTlasSizes(VkDevice device, uint32_t instanceCount) noexcept {
    VkAccelerationStructureGeometryKHR geometry = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureGeometryInstancesDataKHR instances = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .arrayOfPointers = VK_FALSE,
        .data = { .deviceAddress = 0 }
    };
    geometry.geometry.instances = instances;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
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

static uint64_t uploadInstances(VkDevice device, VkPhysicalDevice physDev, VkCommandPool pool, VkQueue queue,
                                std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept {
    if (instances.empty()) return 0;

    const VkDeviceSize instSize = static_cast<VkDeviceSize>(instances.size()) * INSTANCE_SIZE;
    uint64_t stagingHandle = 0;
    BUFFER_CREATE(stagingHandle, instSize,
                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, "staging_instances");

    void* mapped = nullptr;
    BUFFER_MAP(stagingHandle, mapped);
    auto* instData = static_cast<VkAccelerationStructureInstanceKHR*>(mapped);

    for (size_t i = 0; i < instances.size(); ++i) {
        const auto& [as, transform] = instances[i];

        // Copy transform matrix (row-major for Vulkan).
        std::memcpy(&instData[i].transform, glm::value_ptr(transform), sizeof(glm::mat4));

        instData[i].instanceCustomIndex = 0;
        instData[i].mask = 0xFF;
        instData[i].instanceShaderBindingTableRecordOffset = 0;
        instData[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

        VkAccelerationStructureDeviceAddressInfoKHR addrInfo = {
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
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "device_instances");

    VkCommandBuffer cmd = Vulkan::beginSingleTimeCommands(pool);
    VkBufferCopy copyRegion = { .srcOffset = 0, .dstOffset = 0, .size = instSize };
    vkCmdCopyBuffer(cmd, RAW_BUFFER(stagingHandle), RAW_BUFFER(deviceHandle), 1, &copyRegion);
    Vulkan::endSingleTimeCommands(cmd, queue, pool);

    BUFFER_DESTROY(stagingHandle);
    return deviceHandle;
}

// =============================================================================
// AMAZO_LAS: Thread-Safe Singleton for Global BLAS/TLAS Management
// =============================================================================
class AMAZO_LAS {
public:
    // Thread-local singleton for per-thread instances (avoids global contention).
    static AMAZO_LAS& get() noexcept {
        static thread_local AMAZO_LAS instance;
        return instance;
    }

    AMAZO_LAS(const AMAZO_LAS&) = delete;
    AMAZO_LAS& operator=(const AMAZO_LAS&) = delete;

    // Build BLAS from vertex/index buffers.
    void buildBLAS(VkCommandPool pool, VkQueue queue,
                   uint64_t vertexBuf, uint64_t indexBuf,
                   uint32_t vertexCount, uint32_t indexCount,
                   VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        VkDevice dev = Vulkan::ctx()->device;
        if (!dev) {
            LOG_ERROR_CAT("LAS", "Invalid Vulkan device for BLAS build");
            return;
        }

        BlasBuildSizes sizes = computeBlasSizes(dev, vertexCount, indexCount);
        if (sizes.accelerationStructureSize == 0) {
            LOG_WARNING_CAT("LAS", "Invalid BLAS sizes computed");
            return;
        }

        uint64_t asBufferHandle = 0;
        BUFFER_CREATE(asBufferHandle, sizes.accelerationStructureSize,
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "blas_buffer");

        VkAccelerationStructureKHR rawAs = VK_NULL_HANDLE;
        VkAccelerationStructureCreateInfoKHR createInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = RAW_BUFFER(asBufferHandle),
            .size = sizes.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
        };
        if (vkCreateAccelerationStructureKHR(dev, &createInfo, nullptr, &rawAs) != VK_SUCCESS) {
            LOG_ERROR_CAT("LAS", "Failed to create BLAS");
            BUFFER_DESTROY(asBufferHandle);
            return;
        }

        uint64_t scratchHandle = 0;
        BUFFER_CREATE(scratchHandle, sizes.buildScratchSize,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "blas_scratch");

        auto getAddress = [&](VkBuffer buf) -> VkDeviceAddress {
            VkBufferDeviceAddressInfoKHR info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR,
                .buffer = buf
            };
            return vkGetBufferDeviceAddressKHR(dev, &info);
        };

        VkDeviceAddress vertexAddr = getAddress(RAW_BUFFER(vertexBuf));
        VkDeviceAddress indexAddr = getAddress(RAW_BUFFER(indexBuf));
        VkDeviceAddress scratchAddr = getAddress(RAW_BUFFER(scratchHandle));

        VkAccelerationStructureGeometryKHR geometry = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
            .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR
        };
        geometry.geometry.triangles = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
            .vertexData = { .deviceAddress = vertexAddr },
            .vertexStride = sizeof(glm::vec3),
            .maxVertex = vertexCount,
            .indexType = VK_INDEX_TYPE_UINT32,
            .indexData = { .deviceAddress = indexAddr }
        };

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
            .flags = flags,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .dstAccelerationStructure = rawAs,
            .geometryCount = 1,
            .pGeometries = &geometry,
            .scratchData = { .deviceAddress = scratchAddr }
        };

        VkAccelerationStructureBuildRangeInfoKHR buildRange = {
            .primitiveCount = indexCount / 3,
            .primitiveOffset = 0,
            .firstVertex = 0,
            .transformOffset = 0
        };

        VkCommandBuffer cmd = Vulkan::beginSingleTimeCommands(pool);
        const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = { &buildRange };
        vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, ranges);
        Vulkan::endSingleTimeCommands(cmd, queue, pool);

        // Custom deleter: Destroy AS and buffer.
        auto deleter = [asBufferHandle](VkDevice d, VkAccelerationStructureKHR a, const VkAllocationCallbacks* p) mutable noexcept {
            if (a != VK_NULL_HANDLE) vkDestroyAccelerationStructureKHR(d, a, p);
            if (asBufferHandle) BUFFER_DESTROY(asBufferHandle);
        };
        blas_ = Vulkan::makeAccelerationStructure(dev, rawAs, std::move(deleter));

        BUFFER_DESTROY(scratchHandle);
        LOG_SUCCESS_CAT("LAS", "BLAS built successfully (%u vertices, %u indices)", vertexCount, indexCount);
    }

    // Build TLAS from BLAS instances with transforms.
    void buildTLAS(VkCommandPool pool, VkQueue queue,
                   std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        if (instances.empty()) {
            LOG_WARNING_CAT("LAS", "TLAS build requested with zero instances");
            return;
        }

        VkDevice dev = Vulkan::ctx()->device;
        if (!dev) {
            LOG_ERROR_CAT("LAS", "Invalid Vulkan device for TLAS build");
            return;
        }

        TlasBuildSizes sizes = computeTlasSizes(dev, static_cast<uint32_t>(instances.size()));
        if (sizes.accelerationStructureSize == 0) {
            LOG_WARNING_CAT("LAS", "Invalid TLAS sizes computed");
            return;
        }

        uint64_t instanceBufferHandle = uploadInstances(dev, Vulkan::ctx()->physicalDevice, pool, queue, instances);

        uint64_t asBufferHandle = 0;
        BUFFER_CREATE(asBufferHandle, sizes.accelerationStructureSize,
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "tlas_buffer");

        VkAccelerationStructureKHR rawAs = VK_NULL_HANDLE;
        VkAccelerationStructureCreateInfoKHR createInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = RAW_BUFFER(asBufferHandle),
            .size = sizes.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
        };
        if (vkCreateAccelerationStructureKHR(dev, &createInfo, nullptr, &rawAs) != VK_SUCCESS) {
            LOG_ERROR_CAT("LAS", "Failed to create TLAS");
            BUFFER_DESTROY(asBufferHandle);
            if (instanceBufferHandle) BUFFER_DESTROY(instanceBufferHandle);
            return;
        }

        uint64_t scratchHandle = 0;
        BUFFER_CREATE(scratchHandle, sizes.buildScratchSize,
                      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "tlas_scratch");

        auto getAddress = [&](VkBuffer buf) -> VkDeviceAddress {
            VkBufferDeviceAddressInfoKHR info = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR,
                .buffer = buf
            };
            return vkGetBufferDeviceAddressKHR(dev, &info);
        };

        VkDeviceAddress instanceAddr = getAddress(RAW_BUFFER(instanceBufferHandle));
        VkDeviceAddress scratchAddr = getAddress(RAW_BUFFER(scratchHandle));

        VkAccelerationStructureGeometryKHR geometry = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
            .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR
        };
        geometry.geometry.instances = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .arrayOfPointers = VK_FALSE,
            .data = { .deviceAddress = instanceAddr }
        };

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .dstAccelerationStructure = rawAs,
            .geometryCount = 1,
            .pGeometries = &geometry,
            .scratchData = { .deviceAddress = scratchAddr }
        };

        VkAccelerationStructureBuildRangeInfoKHR buildRange = {
            .primitiveCount = static_cast<uint32_t>(instances.size())
        };

        VkCommandBuffer cmd = Vulkan::beginSingleTimeCommands(pool);
        const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = { &buildRange };
        vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, ranges);
        Vulkan::endSingleTimeCommands(cmd, queue, pool);

        // Custom deleter: Destroy AS, instance buffer, and AS buffer.
        auto deleter = [asBufferHandle, instanceBufferHandle](VkDevice d, VkAccelerationStructureKHR a, const VkAllocationCallbacks* p) mutable noexcept {
            if (a != VK_NULL_HANDLE) vkDestroyAccelerationStructureKHR(d, a, p);
            if (asBufferHandle) BUFFER_DESTROY(asBufferHandle);
            if (instanceBufferHandle) BUFFER_DESTROY(instanceBufferHandle);
        };
        tlas_ = Vulkan::makeAccelerationStructure(dev, rawAs, std::move(deleter));

        BUFFER_DESTROY(scratchHandle);
        LOG_SUCCESS_CAT("LAS", "TLAS built successfully (%zu instances)", instances.size());
    }

    // Query accessors.
    [[nodiscard]] VkAccelerationStructureKHR getBLAS() const noexcept { return blas_.raw_deob(); }
    [[nodiscard]] VkDeviceAddress getBLASAddress() const noexcept {
        VkAccelerationStructureKHR as = getBLAS();
        if (as == VK_NULL_HANDLE) return 0;
        VkAccelerationStructureDeviceAddressInfoKHR info = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = as
        };
        return vkGetAccelerationStructureDeviceAddressKHR(Vulkan::ctx()->device, &info);
    }

    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept { return tlas_.raw_deob(); }
    [[nodiscard]] VkDeviceAddress getTLASAddress() const noexcept {
        VkAccelerationStructureKHR as = getTLAS();
        if (as == VK_NULL_HANDLE) return 0;
        VkAccelerationStructureDeviceAddressInfoKHR info = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = as
        };
        return vkGetAccelerationStructureDeviceAddressKHR(Vulkan::ctx()->device, &info);
    }

    // Rebuild TLAS (resets previous and builds new).
    void rebuildTLAS(VkCommandPool pool, VkQueue queue,
                     std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept {
        std::lock_guard<std::mutex> lock(mutex_);
        tlas_.reset();
        buildTLAS(pool, queue, instances);
    }

private:
    AMAZO_LAS() = default;
    ~AMAZO_LAS() noexcept {
        blas_.reset();
        tlas_.reset();
    }

    Vulkan::VulkanHandle<VkAccelerationStructureKHR> blas_;
    Vulkan::VulkanHandle<VkAccelerationStructureKHR> tlas_;
    mutable std::mutex mutex_;
};

// =============================================================================
// Convenience Macros
// =============================================================================
#define BUILD_BLAS(pool, q, vbuf, ibuf, vcount, icount, flags) \
    AMAZO_LAS::get().buildBLAS(pool, q, vbuf, ibuf, vcount, icount, flags)

#define BUILD_TLAS(pool, q, instances) \
    AMAZO_LAS::get().buildTLAS(pool, q, instances)

#define REBUILD_TLAS(pool, q, instances) \
    AMAZO_LAS::get().rebuildTLAS(pool, q, instances)

#define GLOBAL_BLAS()          AMAZO_LAS::get().getBLAS()
#define GLOBAL_BLAS_ADDRESS()  AMAZO_LAS::get().getBLASAddress()
#define GLOBAL_TLAS()          AMAZO_LAS::get().getTLAS()
#define GLOBAL_TLAS_ADDRESS()  AMAZO_LAS::get().getTLASAddress()

#define LAS_STATS() \
    LOG_INFO_CAT("LAS", "BLAS: %s | TLAS: %s", \
                 (GLOBAL_BLAS() != VK_NULL_HANDLE ? "VALID" : "INVALID"), \
                 (GLOBAL_TLAS() != VK_NULL_HANDLE ? "VALID" : "INVALID"))

// =============================================================================
// End of File
// =============================================================================