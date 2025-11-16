// src/include/engine/GLOBAL/LAS.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// DUAL LICENSE: CC BY-NC 4.0 (Non-Commercial) | Commercial: gzac5314@gmail.com
// =============================================================================

#pragma once

#include "engine/GLOBAL/logging.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include <bit>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <span>
#include <mutex>
#include <format>
#include <algorithm>  // std::min, std::max

using namespace Logging::Color;

// Forward declare RTX::g_ctx()
namespace RTX {
    struct Context;
    [[nodiscard]] Context& g_ctx() noexcept;
}

namespace RTX {

// =============================================================================
// BUILD SIZES
// =============================================================================
struct BlasBuildSizes {
    VkDeviceSize accelerationStructureSize = 0;
    VkDeviceSize buildScratchSize = 0;
    VkDeviceSize updateScratchSize = 0;
};

struct TlasBuildSizes {
    VkDeviceSize accelerationStructureSize = 0;
    VkDeviceSize buildScratchSize = 0;
    VkDeviceSize updateScratchSize = 0;
    VkDeviceSize instanceDataSize = 0;
};

// =============================================================================
// AMOURANTH AI v3 — TECHNICAL DOMINANCE (NO AI VOICE)
// =============================================================================
class AmouranthAI {
public:
    static AmouranthAI& get() noexcept {
        static AmouranthAI instance;
        return instance;
    }

    void onBlasStart(uint32_t v, uint32_t i) {}
    void onBlasBuilt(double sizeGB, const BlasBuildSizes& sizes) {}
    void onTlasStart(size_t count) {}
    void onTlasBuilt(double sizeGB, VkDeviceAddress addr, const TlasBuildSizes& sizes) {}
    void onPhotonDispatch(uint32_t w, uint32_t h) {}
    void onMemoryEvent(const char* name, VkDeviceSize size) {}
    void onScratchPoolResize(VkDeviceSize oldSize, VkDeviceSize newSize, const char* type) {}
    void onBuildTime(const char* type, double gpu_us) {}

private:
    AmouranthAI() = default;
};

// =============================================================================
// INTERNAL: SIZE COMPUTATION + INSTANCE UPLOAD (OPTIMIZED)
// =============================================================================
namespace {

[[nodiscard]] inline BlasBuildSizes computeBlasSizes(VkDevice device, uint32_t vertexCount, uint32_t indexCount) {
    if (!g_ctx().vkGetAccelerationStructureBuildSizesKHR_) {
        throw std::runtime_error("vkGetAccelerationStructureBuildSizesKHR not available. Enable VK_KHR_acceleration_structure extension and load function pointer.");
    }

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry.geometry.triangles.vertexStride = sizeof(glm::vec3);
    geometry.geometry.triangles.maxVertex = vertexCount;
    geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                      VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    uint32_t primitiveCount = indexCount / 3;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    g_ctx().vkGetAccelerationStructureBuildSizesKHR_(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &primitiveCount, &sizeInfo);

    return { sizeInfo.accelerationStructureSize,
             sizeInfo.buildScratchSize,
             sizeInfo.updateScratchSize };
}

[[nodiscard]] inline TlasBuildSizes computeTlasSizes(VkDevice device, uint32_t instanceCount) {
    if (!g_ctx().vkGetAccelerationStructureBuildSizesKHR_) {
        throw std::runtime_error("vkGetAccelerationStructureBuildSizesKHR not available. Enable VK_KHR_acceleration_structure extension and load function pointer.");
    }

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.arrayOfPointers = VK_FALSE;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                      VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
    g_ctx().vkGetAccelerationStructureBuildSizesKHR_(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &instanceCount, &sizeInfo);

    VkDeviceSize instDataSize = static_cast<VkDeviceSize>(instanceCount) * sizeof(VkAccelerationStructureInstanceKHR);

    return { sizeInfo.accelerationStructureSize,
             sizeInfo.buildScratchSize,
             sizeInfo.updateScratchSize,
             instDataSize };
}

[[nodiscard]] inline uint64_t uploadInstances(
    VkDevice device, VkCommandPool pool, VkQueue queue,
    std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances)
{
    if (instances.empty()) {
        LOG_WARN_CAT("LAS", "uploadInstances: empty instance list");
        return 0;
    }

    const VkDeviceSize instSize = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);
    AmouranthAI::get().onMemoryEvent("TLAS instance staging", instSize);

    uint64_t stagingHandle = 0;
    BUFFER_CREATE(stagingHandle, instSize,
                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                  VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "las_instances_staging");

    void* mapped = nullptr;
    BUFFER_MAP(stagingHandle, mapped);
    auto* instData = static_cast<VkAccelerationStructureInstanceKHR*>(mapped);

    for (size_t i = 0; i < instances.size(); ++i) {
        const auto& [as, transform] = instances[i];
        glm::mat4 rowMajor = glm::transpose(transform);
        std::memcpy(&instData[i].transform, glm::value_ptr(rowMajor), sizeof(VkTransformMatrixKHR));
        instData[i].instanceCustomIndex = static_cast<uint32_t>(i);
        instData[i].mask = 0xFF;
        instData[i].instanceShaderBindingTableRecordOffset = 0;
        instData[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
        addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addrInfo.accelerationStructure = as;
        if (!g_ctx().vkGetAccelerationStructureDeviceAddressKHR_) {
            BUFFER_DESTROY(stagingHandle);
            throw std::runtime_error("vkGetAccelerationStructureDeviceAddressKHR not available. Enable VK_KHR_acceleration_structure and VK_KHR_buffer_device_address extensions.");
        }
        instData[i].accelerationStructureReference =
            g_ctx().vkGetAccelerationStructureDeviceAddressKHR_(device, &addrInfo);
    }
    BUFFER_UNMAP(stagingHandle);

    uint64_t deviceHandle = 0;
    BUFFER_CREATE(deviceHandle, instSize,
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                  "las_instances_device");

    VkCommandBuffer cmd = VulkanRTX::beginSingleTimeCommands(pool);
    VkBufferCopy copy{.srcOffset = 0, .dstOffset = 0, .size = instSize};
    vkCmdCopyBuffer(cmd, RAW_BUFFER(stagingHandle), RAW_BUFFER(deviceHandle), 1, &copy);
    VulkanRTX::endSingleTimeCommands(cmd, queue, pool);

    BUFFER_DESTROY(stagingHandle);
    return deviceHandle;
}

} // anonymous namespace

// =============================================================================
// LAS — SINGLETON — STONEKEY v∞ PUBLIC v0.3 (PERF: ~0.3µs TARGET | FIXED STATIC INIT + CLEANUP GUARDS)
// =============================================================================
class LAS {
public:
    static LAS& get() noexcept {
        static LAS instance;
        return instance;
    }

    LAS(const LAS&) = delete;
    LAS& operator=(const LAS&) = delete;

    void buildBLAS(VkCommandPool pool, VkQueue queue,
                   uint64_t vertexBuf, uint64_t indexBuf,
                   uint32_t vertexCount, uint32_t indexCount,
                   VkBuildAccelerationStructureFlagsKHR extraFlags = 0,
                   bool fastBuild = false)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        VkDevice dev = g_ctx().vkDevice();

        if (vertexBuf == 0 || indexBuf == 0) {
            throw std::runtime_error("buildBLAS: Invalid buffer handle (vertex or index is null)");
        }

        if (!g_ctx().vkCreateAccelerationStructureKHR_) {
            throw std::runtime_error("vkCreateAccelerationStructureKHR not available. Enable VK_KHR_acceleration_structure extension and load function pointer.");
        }
        if (!g_ctx().vkGetBufferDeviceAddressKHR_) {
            throw std::runtime_error("vkGetBufferDeviceAddressKHR not available. Enable VK_KHR_buffer_device_address extension and load function pointer.");
        }
        if (!g_ctx().vkCmdBuildAccelerationStructuresKHR_) {
            throw std::runtime_error("vkCmdBuildAccelerationStructuresKHR not available. Enable VK_KHR_acceleration_structure extension and load function pointer.");
        }

        AmouranthAI::get().onBlasStart(vertexCount, indexCount);

        auto sizes = computeBlasSizes(dev, vertexCount, indexCount);
        if (sizes.accelerationStructureSize == 0)
            throw std::runtime_error("BLAS size zero");

        uint64_t asBufferHandle = 0;
        BUFFER_CREATE(asBufferHandle, sizes.accelerationStructureSize,
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                      "las_blas_storage");

        VkAccelerationStructureKHR rawAs = VK_NULL_HANDLE;
        VkAccelerationStructureCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = RAW_BUFFER(asBufferHandle);
        createInfo.size = sizes.accelerationStructureSize;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        VK_CHECK(g_ctx().vkCreateAccelerationStructureKHR_(dev, &createInfo, nullptr, &rawAs),
                 "Failed to create BLAS");

        uint64_t scratchHandle = getOrGrowScratch(sizes.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            "blas");

        VkBufferDeviceAddressInfo vertexAddrInfo{};
        vertexAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        vertexAddrInfo.buffer = RAW_BUFFER(vertexBuf);
        VkBufferDeviceAddressInfo indexAddrInfo{};
        indexAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        indexAddrInfo.buffer = RAW_BUFFER(indexBuf);
        VkBufferDeviceAddressInfo scratchAddrInfo{};
        scratchAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        scratchAddrInfo.buffer = RAW_BUFFER(scratchHandle);

        VkDeviceAddress vertexAddr = g_ctx().vkGetBufferDeviceAddressKHR_(dev, &vertexAddrInfo);
        VkDeviceAddress indexAddr  = g_ctx().vkGetBufferDeviceAddressKHR_(dev, &indexAddrInfo);
        VkDeviceAddress scratchAddr = g_ctx().vkGetBufferDeviceAddressKHR_(dev, &scratchAddrInfo);

        VkAccelerationStructureGeometryKHR geometry{};
        geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
        geometry.geometry.triangles.vertexData.deviceAddress = vertexAddr;
        geometry.geometry.triangles.vertexStride = sizeof(glm::vec3);
        geometry.geometry.triangles.maxVertex = vertexCount;
        geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
        geometry.geometry.triangles.indexData.deviceAddress = indexAddr;

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
        buildInfo.flags = extraFlags | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR |
                          (fastBuild ? VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR : VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
        buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.dstAccelerationStructure = rawAs;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &geometry;
        buildInfo.scratchData.deviceAddress = scratchAddr;

        VkAccelerationStructureBuildRangeInfoKHR buildRange{};
        buildRange.primitiveCount = indexCount / 3;

        VkCommandBuffer cmd = beginOptimizedCmd(pool);
        const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = { &buildRange };

        ensureTimestampPool();
        if (QUERY_POOL_TIMESTAMP != VK_NULL_HANDLE) {
            vkCmdResetQueryPool(cmd, QUERY_POOL_TIMESTAMP, 0, 2);
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, QUERY_POOL_TIMESTAMP, 0);
        }
        g_ctx().vkCmdBuildAccelerationStructuresKHR_(cmd, 1, &buildInfo, ranges);
        if (QUERY_POOL_TIMESTAMP != VK_NULL_HANDLE) {
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, QUERY_POOL_TIMESTAMP, 1);
        }

        submitOptimizedCmd(cmd, queue, pool);

        if (QUERY_POOL_TIMESTAMP != VK_NULL_HANDLE) {
            uint64_t timestamps[2] = {0, 0};
            VK_CHECK(vkGetQueryPoolResults(dev, QUERY_POOL_TIMESTAMP, 0, 2, sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT), "Failed to get query results for BLAS");
            double timestampPeriodNs = timestampPeriodNs_;
            double gpu_ns = static_cast<double>(timestamps[1] - timestamps[0]) * timestampPeriodNs;
            AmouranthAI::get().onBuildTime("BLAS", gpu_ns / 1000.0);
        }

        auto deleter = [asBufferHandle](VkDevice d,
                                        VkAccelerationStructureKHR a,
                                        const VkAllocationCallbacks*) noexcept {
            if (d == VK_NULL_HANDLE) {
                LOG_WARN_CAT("LAS", "Skipped BLAS deleter — null device");
                return;
            }
            if (a != VK_NULL_HANDLE && g_ctx().vkDestroyAccelerationStructureKHR_) {
                g_ctx().vkDestroyAccelerationStructureKHR_(d, a, nullptr);
            }
            if (asBufferHandle != 0) {
                BUFFER_DESTROY(asBufferHandle);
            }
        };

        blas_ = Handle<VkAccelerationStructureKHR>(rawAs, dev, deleter,
                                                   sizes.accelerationStructureSize,
                                                   "LAS_BLAS");

        double sizeGB = sizes.accelerationStructureSize / (1024.0 * 1024.0 * 1024.0);
        AmouranthAI::get().onBlasBuilt(sizeGB, sizes);
    }

    void buildTLAS(VkCommandPool pool, VkQueue queue,
                   std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances,
                   bool fastBuild = false)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (instances.empty()) throw std::runtime_error("TLAS: zero instances");

        VkDevice dev = g_ctx().vkDevice();

        if (!g_ctx().vkCreateAccelerationStructureKHR_) {
            throw std::runtime_error("vkCreateAccelerationStructureKHR not available. Enable VK_KHR_acceleration_structure extension and load function pointer.");
        }
        if (!g_ctx().vkGetBufferDeviceAddressKHR_) {
            throw std::runtime_error("vkGetBufferDeviceAddressKHR not available. Enable VK_KHR_buffer_device_address extension and load function pointer.");
        }
        if (!g_ctx().vkCmdBuildAccelerationStructuresKHR_) {
            throw std::runtime_error("vkCmdBuildAccelerationStructuresKHR not available. Enable VK_KHR_acceleration_structure extension and load function pointer.");
        }

        AmouranthAI::get().onTlasStart(instances.size());

        auto sizes = computeTlasSizes(dev, static_cast<uint32_t>(instances.size()));
        if (sizes.accelerationStructureSize == 0)
            throw std::runtime_error("TLAS size zero");

        uint64_t instanceEnc = uploadInstances(dev, pool, queue, instances);
        if (!instanceEnc) throw std::runtime_error("Instance upload failed");

        uint64_t asBufferHandle = 0;
        BUFFER_CREATE(asBufferHandle, sizes.accelerationStructureSize,
                      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                      VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                      "las_tlas_storage");

        VkAccelerationStructureKHR rawAs = VK_NULL_HANDLE;
        VkAccelerationStructureCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
        createInfo.buffer = RAW_BUFFER(asBufferHandle);
        createInfo.size = sizes.accelerationStructureSize;
        createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        VK_CHECK(g_ctx().vkCreateAccelerationStructureKHR_(dev, &createInfo, nullptr, &rawAs),
                 "Failed to create TLAS");

        uint64_t scratchHandle = getOrGrowScratch(sizes.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            "tlas");

        VkBufferDeviceAddressInfo instanceAddrInfo{};
        instanceAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        instanceAddrInfo.buffer = RAW_BUFFER(instanceEnc);
        VkBufferDeviceAddressInfo scratchAddrInfo{};
        scratchAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        scratchAddrInfo.buffer = RAW_BUFFER(scratchHandle);

        VkDeviceAddress instanceAddr = g_ctx().vkGetBufferDeviceAddressKHR_(dev, &instanceAddrInfo);
        VkDeviceAddress scratchAddr  = g_ctx().vkGetBufferDeviceAddressKHR_(dev, &scratchAddrInfo);

        VkAccelerationStructureGeometryKHR geometry{};
        geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
        geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
        geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        geometry.geometry.instances.arrayOfPointers = VK_FALSE;
        geometry.geometry.instances.data.deviceAddress = instanceAddr;

        VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
        buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
        buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
        buildInfo.flags = (fastBuild ? VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR : VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR) |
                          VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
        buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
        buildInfo.dstAccelerationStructure = rawAs;
        buildInfo.geometryCount = 1;
        buildInfo.pGeometries = &geometry;
        buildInfo.scratchData.deviceAddress = scratchAddr;

        VkAccelerationStructureBuildRangeInfoKHR buildRange{};
        buildRange.primitiveCount = static_cast<uint32_t>(instances.size());

        VkCommandBuffer cmd = beginOptimizedCmd(pool);
        const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = { &buildRange };

        ensureTimestampPool();
        if (QUERY_POOL_TIMESTAMP != VK_NULL_HANDLE) {
            vkCmdResetQueryPool(cmd, QUERY_POOL_TIMESTAMP, 0, 2);
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, QUERY_POOL_TIMESTAMP, 0);
        }
        g_ctx().vkCmdBuildAccelerationStructuresKHR_(cmd, 1, &buildInfo, ranges);
        if (QUERY_POOL_TIMESTAMP != VK_NULL_HANDLE) {
            vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, QUERY_POOL_TIMESTAMP, 1);
        }

        submitOptimizedCmd(cmd, queue, pool);

        if (QUERY_POOL_TIMESTAMP != VK_NULL_HANDLE) {
            uint64_t timestamps[2] = {0, 0};
            VK_CHECK(vkGetQueryPoolResults(dev, QUERY_POOL_TIMESTAMP, 0, 2, sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT), "Failed to get query results for TLAS");
            double timestampPeriodNs = timestampPeriodNs_;
            double gpu_ns = static_cast<double>(timestamps[1] - timestamps[0]) * timestampPeriodNs;
            AmouranthAI::get().onBuildTime("TLAS", gpu_ns / 1000.0);
        }

        auto deleter = [asBufferHandle, instanceEnc](VkDevice d,
                                                     VkAccelerationStructureKHR a,
                                                     const VkAllocationCallbacks*) noexcept {
            if (d == VK_NULL_HANDLE) {
                LOG_WARN_CAT("LAS", "Skipped TLAS deleter — null device");
                return;
            }
            if (a != VK_NULL_HANDLE && g_ctx().vkDestroyAccelerationStructureKHR_) {
                g_ctx().vkDestroyAccelerationStructureKHR_(d, a, nullptr);
            }
            if (asBufferHandle != 0) {
                BUFFER_DESTROY(asBufferHandle);
            }
            if (instanceEnc != 0) {
                BUFFER_DESTROY(instanceEnc);
            }
        };

        tlas_ = Handle<VkAccelerationStructureKHR>(rawAs, dev, deleter,
                                                   sizes.accelerationStructureSize,
                                                   "LAS_TLAS");
        tlasSize_ = sizes.accelerationStructureSize;
        instanceBufferId_ = instanceEnc;

        VkDeviceAddress addr = getTLASAddress();
        double sizeGB = sizes.accelerationStructureSize / (1024.0 * 1024.0 * 1024.0);
        AmouranthAI::get().onTlasBuilt(sizeGB, addr, sizes);
    }

    void rebuildTLAS(VkCommandPool pool, VkQueue queue,
                     std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances,
                     bool fastBuild = false)
    {
        tlas_.reset();
        if (instanceBufferId_) BUFFER_DESTROY(instanceBufferId_);
        instanceBufferId_ = 0;
        tlasSize_ = 0;
        buildTLAS(pool, queue, instances, fastBuild);
    }

    [[nodiscard]] VkAccelerationStructureKHR getBLAS() const noexcept { return blas_ ? *blas_ : VK_NULL_HANDLE; }
    [[nodiscard]] VkDeviceAddress getBLASAddress() const noexcept {
        if (!blas_) return 0;
        if (!g_ctx().vkGetAccelerationStructureDeviceAddressKHR_) {
            LOG_ERROR_CAT("LAS", "vkGetAccelerationStructureDeviceAddressKHR not available");
            return 0;
        }
        VkAccelerationStructureDeviceAddressInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        info.accelerationStructure = *blas_;
        return g_ctx().vkGetAccelerationStructureDeviceAddressKHR_(g_ctx().vkDevice(), &info);
    }

    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept { return tlas_ ? *tlas_ : VK_NULL_HANDLE; }
    [[nodiscard]] VkDeviceAddress getTLASAddress() const noexcept {
        if (!tlas_) return 0;
        if (!g_ctx().vkGetAccelerationStructureDeviceAddressKHR_) {
            LOG_ERROR_CAT("LAS", "vkGetAccelerationStructureDeviceAddressKHR not available");
            return 0;
        }
        VkAccelerationStructureDeviceAddressInfoKHR info{};
        info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        info.accelerationStructure = *tlas_;
        return g_ctx().vkGetAccelerationStructureDeviceAddressKHR_(g_ctx().vkDevice(), &info);
    }

    [[nodiscard]] VkDeviceSize getTLASSize() const noexcept { return tlasSize_; }

private:
    static constexpr VkDeviceSize INITIAL_SCRATCH_SIZE = 1024ULL * 1024ULL;
    static constexpr VkDeviceSize MAX_SCRATCH_SIZE = 64ULL * 1024ULL * 1024ULL;

    LAS() = default;
    ~LAS() noexcept {
        VkDevice dev = g_ctx().vkDevice();
        if (dev != VK_NULL_HANDLE && QUERY_POOL_TIMESTAMP != VK_NULL_HANDLE) {
            vkDestroyQueryPool(dev, QUERY_POOL_TIMESTAMP, nullptr);
            QUERY_POOL_TIMESTAMP = VK_NULL_HANDLE;
        } else if (QUERY_POOL_TIMESTAMP != VK_NULL_HANDLE) {
            LOG_WARN_CAT("LAS", "Skipped timestamp query pool destroy — null device");
        }
        if (dev != VK_NULL_HANDLE && scratchPoolId_ != 0) {
            BUFFER_DESTROY(scratchPoolId_);
        } else if (scratchPoolId_ != 0) {
            LOG_WARN_CAT("LAS", "Skipped scratch pool destroy — null device");
            scratchPoolId_ = 0;
        }
        blas_.reset();
        tlas_.reset();
        if (instanceBufferId_ != 0 && dev != VK_NULL_HANDLE) {
            BUFFER_DESTROY(instanceBufferId_);
        } else if (instanceBufferId_ != 0) {
            LOG_WARN_CAT("LAS", "Skipped instance buffer destroy — null device");
            instanceBufferId_ = 0;
        }
    }

    mutable std::mutex mutex_;
    Handle<VkAccelerationStructureKHR> blas_;
    Handle<VkAccelerationStructureKHR> tlas_;
    uint64_t instanceBufferId_ = 0;
    VkDeviceSize tlasSize_ = 0;
    VkQueryPool QUERY_POOL_TIMESTAMP = VK_NULL_HANDLE;
    float timestampPeriodNs_ = 0.0f;

    uint64_t scratchPoolId_ = 0;
    VkDeviceSize currentScratchSize_ = INITIAL_SCRATCH_SIZE;
    bool scratchPoolValid_ = false;

    static constexpr float GROWTH_FACTOR = 2.0f;

    void ensureTimestampPool() noexcept {
        if (QUERY_POOL_TIMESTAMP == VK_NULL_HANDLE) {
            VkDevice dev = g_ctx().vkDevice();
            if (dev == VK_NULL_HANDLE) {
                LOG_WARN_CAT("LAS", "Skipped timestamp pool create — null device");
                return;
            }
            VkQueryPoolCreateInfo qpoolInfo{};
            qpoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            qpoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
            qpoolInfo.queryCount = 2;
            VK_CHECK(vkCreateQueryPool(dev, &qpoolInfo, nullptr, &QUERY_POOL_TIMESTAMP), "Failed to create timestamp query pool");
        }
        if (timestampPeriodNs_ == 0.0f) {
            VkPhysicalDevice phys = g_ctx().physicalDevice();
            if (phys != VK_NULL_HANDLE) {
                VkPhysicalDeviceProperties props{};
                vkGetPhysicalDeviceProperties(phys, &props);
                timestampPeriodNs_ = static_cast<float>(props.limits.timestampPeriod);
            } else {
                LOG_WARN_CAT("LAS", "Skipped timestamp period query — null phys dev");
                timestampPeriodNs_ = 1.0f;
            }
        }
    }

    [[nodiscard]] uint64_t getOrGrowScratch(VkDeviceSize requiredSize, VkBufferUsageFlags usage, const char* type) {
        VkDevice dev = g_ctx().vkDevice();
        if (dev == VK_NULL_HANDLE) {
            LOG_WARN_CAT("LAS", "Skipped scratch grow — null device");
            return 0;
        }
        if (!scratchPoolValid_ || requiredSize > currentScratchSize_) {
            VkDeviceSize oldSize = currentScratchSize_;
            VkDeviceSize growth = static_cast<VkDeviceSize>(static_cast<double>(currentScratchSize_) * GROWTH_FACTOR);
            VkDeviceSize newSize = std::min(growth, MAX_SCRATCH_SIZE);
            newSize = std::max(newSize, requiredSize);

            if (scratchPoolId_ != 0) {
                BUFFER_DESTROY(scratchPoolId_);
            }
            BUFFER_CREATE(scratchPoolId_, newSize, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, std::format("scratch_{}", type).c_str());
            currentScratchSize_ = newSize;
            scratchPoolValid_ = true;
            AmouranthAI::get().onScratchPoolResize(oldSize, currentScratchSize_, type);
        }
        return scratchPoolId_;
    }

    [[nodiscard]] VkCommandBuffer beginOptimizedCmd(VkCommandPool pool) {
        VkDevice dev = g_ctx().vkDevice();
        if (dev == VK_NULL_HANDLE) {
            throw std::runtime_error("beginOptimizedCmd: null device");
        }
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = pool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer cmd;
        VK_CHECK(vkAllocateCommandBuffers(dev, &allocInfo, &cmd), "Alloc RT cmdbuf");

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "Begin RT cmdbuf");

        return cmd;
    }

    void submitOptimizedCmd(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) {
        VkDevice dev = g_ctx().vkDevice();
        if (dev == VK_NULL_HANDLE) {
            LOG_WARN_CAT("LAS", "Skipped submitOptimizedCmd — null device");
            return;
        }
        VK_CHECK(vkEndCommandBuffer(cmd), "End RT cmdbuf");

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;

        VkFence fence = VK_NULL_HANDLE;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        VK_CHECK(vkCreateFence(dev, &fenceInfo, nullptr, &fence), "Create RT fence");

        VkResult submitRes = vkQueueSubmit(queue, 1, &submitInfo, fence);
        if (submitRes != VK_SUCCESS) {
            if (fence != VK_NULL_HANDLE) vkDestroyFence(dev, fence, nullptr);
            if (submitRes == VK_ERROR_DEVICE_LOST) {
                LOG_ERROR_CAT("LAS", "Device lost during RT submit — recreate device/context");
                throw std::runtime_error("VK_ERROR_DEVICE_LOST during RT submit");
            }
            VK_CHECK(submitRes, "Submit RT cmd");
        }

        VkResult waitRes = vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX);
        vkDestroyFence(dev, fence, nullptr);
        if (waitRes != VK_SUCCESS) {
            if (waitRes == VK_ERROR_DEVICE_LOST) {
                LOG_ERROR_CAT("LAS", "Device lost during RT fence wait — recreate device/context");
                throw std::runtime_error("VK_ERROR_DEVICE_LOST during RT fence wait");
            }
            VK_CHECK(waitRes, "Wait for RT fence");
        }

        vkFreeCommandBuffers(dev, pool, 1, &cmd);
    }
};

inline LAS& las() noexcept { return LAS::get(); }

} // namespace RTX

// =============================================================================
// STONEKEY v∞ PUBLIC — PINK PHOTONS ETERNAL — DOMINANCE ETERNAL
// =============================================================================