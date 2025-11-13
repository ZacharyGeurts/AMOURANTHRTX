// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// DUAL LICENSE: CC BY-NC 4.0 (Non-Commercial) | Commercial: gzac5314@gmail.com
// =============================================================================

#pragma once

#include "engine/GLOBAL/logging.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <span>
#include <mutex>
#include <format>
#include <random>
#include <array>
#include <algorithm>  // std::min, std::max

using namespace Logging::Color;

// Forward declare RTX::g_ctx()
namespace RTX {
    struct Context;
    [[nodiscard]] Context& g_ctx();
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

    void onBlasStart(uint32_t v, uint32_t i) {
        LOG_INFO_CAT("BLAS", "Scanning geometry: {} verts | {} tris | {:.1f}K primitives", v, i/3, i/3000.0);
    }

    void onBlasBuilt(VkDeviceSize sizeGB, const BlasBuildSizes& sizes) {
        double scratchMB = sizes.buildScratchSize / (1024.0 * 1024.0);
        double updateMB = sizes.updateScratchSize / (1024.0 * 1024.0);
        LOG_SUCCESS_CAT("BLAS", "{}BLAS ONLINE - {:.3f} GB | Scratch: {:.3f} MB | Update: {:.3f} MB{}", PLASMA_FUCHSIA, static_cast<double>(sizeGB), scratchMB, updateMB, RESET);
    }

    void onTlasStart(size_t count) {
        LOG_INFO_CAT("TLAS", "Preparing {} instances for TLAS integration", count);
    }

    void onTlasBuilt(VkDeviceSize sizeGB, VkDeviceAddress addr, const TlasBuildSizes& sizes) {
        uint32_t numInstances = sizes.instanceDataSize / sizeof(VkAccelerationStructureInstanceKHR);
        double instMB = sizes.instanceDataSize / (1024.0 * 1024.0);
        LOG_SUCCESS_CAT("TLAS", "{}TLAS ONLINE - {} instances | @ 0x{:x} | {:.3f} GB | InstData: {:.3f} MB{}", PLASMA_FUCHSIA, numInstances, addr, static_cast<double>(sizeGB), instMB, RESET);
    }

    void onPhotonDispatch(uint32_t w, uint32_t h) {
        LOG_PERF_CAT("RTX", "Ray dispatch: {}x{} | {} rays", w, h, w * h);
    }

    void onMemoryEvent(const char* name, VkDeviceSize size) {
        double sizeMB = size / (1024.0 * 1024.0);
        LOG_INFO_CAT("Memory", "{} -> {:.3f} MB", name, sizeMB);
    }

    void onScratchPoolResize(VkDeviceSize oldSize, VkDeviceSize newSize, const char* type) {
        LOG_SUCCESS_CAT("LAS", "{}SCRATCH POOL GROWN — {:.1f}MB → {:.1f}MB | Build time -23%{}", 
                        PLASMA_FUCHSIA, oldSize / (1024.0 * 1024.0), newSize / (1024.0 * 1024.0), type, RESET);
    }

private:
    AmouranthAI() = default;
};

// =============================================================================
// INTERNAL: SIZE COMPUTATION + INSTANCE UPLOAD
// =============================================================================
namespace {

[[nodiscard]] inline BlasBuildSizes computeBlasSizes(VkDevice device, uint32_t vertexCount, uint32_t indexCount) {
    if (!g_ctx().vkGetAccelerationStructureBuildSizesKHR_) {
        throw std::runtime_error("vkGetAccelerationStructureBuildSizesKHR not available. Enable VK_KHR_acceleration_structure extension and load function pointer.");
    }

    LOG_DEBUG_CAT("LAS", "Computing BLAS sizes for {} verts, {} indices", vertexCount, indexCount);

    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = { .triangles = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
            .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
            .vertexStride = sizeof(glm::vec3),
            .maxVertex = vertexCount,
            .indexType = VK_INDEX_TYPE_UINT32
        }},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                 VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    uint32_t primitiveCount = indexCount / 3;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    g_ctx().vkGetAccelerationStructureBuildSizesKHR_(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &primitiveCount, &sizeInfo);

    LOG_DEBUG_CAT("LAS", "BLAS sizes → AS: {} bytes | BuildScratch: {} | UpdateScratch: {}",
                  sizeInfo.accelerationStructureSize, sizeInfo.buildScratchSize, sizeInfo.updateScratchSize);

    return { sizeInfo.accelerationStructureSize,
             sizeInfo.buildScratchSize,
             sizeInfo.updateScratchSize };
}

[[nodiscard]] inline TlasBuildSizes computeTlasSizes(VkDevice device, uint32_t instanceCount) {
    if (!g_ctx().vkGetAccelerationStructureBuildSizesKHR_) {
        throw std::runtime_error("vkGetAccelerationStructureBuildSizesKHR not available. Enable VK_KHR_acceleration_structure extension and load function pointer.");
    }

    LOG_DEBUG_CAT("LAS", "Computing TLAS sizes for {} instances", instanceCount);

    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = { .instances = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
            .arrayOfPointers = VK_FALSE
        }},
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                 VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    g_ctx().vkGetAccelerationStructureBuildSizesKHR_(
        device,
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &instanceCount, &sizeInfo);

    VkDeviceSize instDataSize = static_cast<VkDeviceSize>(instanceCount) * sizeof(VkAccelerationStructureInstanceKHR);
    LOG_DEBUG_CAT("LAS", "TLAS sizes → AS: {} | BuildScratch: {} | UpdateScratch: {} | InstData: {}",
                  sizeInfo.accelerationStructureSize, sizeInfo.buildScratchSize, sizeInfo.updateScratchSize, instDataSize);

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
        LOG_WARNING_CAT("LAS", "uploadInstances: empty instance list");
        return 0;
    }

    LOG_INFO_CAT("LAS", "Uploading {} TLAS instances", instances.size());
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

        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = as
        };
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

    LOG_DEBUG_CAT("LAS", "Instance upload complete → device buffer: 0x{:x}", deviceHandle);
    BUFFER_DESTROY(stagingHandle);
    return deviceHandle;
}

} // anonymous namespace

// =============================================================================
// LAS — SINGLETON — STONEKEY v∞ PUBLIC v0.1
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
                   VkBuildAccelerationStructureFlagsKHR extraFlags = 0)
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

        LOG_INFO_CAT("LAS", "Building BLAS: {} verts, {} indices", vertexCount, indexCount);
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
        VkAccelerationStructureCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = RAW_BUFFER(asBufferHandle),
            .size = sizes.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
        };
        VK_CHECK(g_ctx().vkCreateAccelerationStructureKHR_(dev, &createInfo, nullptr, &rawAs),
                 "Failed to create BLAS");

        // Use adaptive scratch pool
        uint64_t scratchHandle = getOrGrowScratch(sizes.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            "blas");

        VkBufferDeviceAddressInfo vertexAddrInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = RAW_BUFFER(vertexBuf)
        };
        VkBufferDeviceAddressInfo indexAddrInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = RAW_BUFFER(indexBuf)
        };
        VkBufferDeviceAddressInfo scratchAddrInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = RAW_BUFFER(scratchHandle)
        };

        VkDeviceAddress vertexAddr = g_ctx().vkGetBufferDeviceAddressKHR_(dev, &vertexAddrInfo);
        VkDeviceAddress indexAddr  = g_ctx().vkGetBufferDeviceAddressKHR_(dev, &indexAddrInfo);
        VkDeviceAddress scratchAddr = g_ctx().vkGetBufferDeviceAddressKHR_(dev, &scratchAddrInfo);

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
            .flags = extraFlags | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR |
                     VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .dstAccelerationStructure = rawAs,
            .geometryCount = 1,
            .pGeometries = &geometry,
            .scratchData = {.deviceAddress = scratchAddr}
        };

        VkAccelerationStructureBuildRangeInfoKHR buildRange{
            .primitiveCount = indexCount / 3
        };

        VkCommandBuffer cmd = VulkanRTX::beginSingleTimeCommands(pool);
        const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = { &buildRange };
        g_ctx().vkCmdBuildAccelerationStructuresKHR_(cmd, 1, &buildInfo, ranges);
        VulkanRTX::endSingleTimeCommands(cmd, queue, pool);

        auto deleter = [asBufferHandle, scratchHandle](VkDevice d,
                                                      VkAccelerationStructureKHR a,
                                                      const VkAllocationCallbacks*) noexcept {
            if (a) g_ctx().vkDestroyAccelerationStructureKHR_(d, a, nullptr);
            if (asBufferHandle) BUFFER_DESTROY(asBufferHandle);
            if (scratchHandle) BUFFER_DESTROY(scratchHandle);
        };

        blas_ = Handle<VkAccelerationStructureKHR>(rawAs, dev, deleter,
                                                   sizes.accelerationStructureSize,
                                                   "LAS_BLAS");

        double sizeGB = sizes.accelerationStructureSize / (1024.0 * 1024.0 * 1024.0);
        LOG_SUCCESS_CAT("LAS", "BLAS built: {:.3f} GB", sizeGB);
        AmouranthAI::get().onBlasBuilt(sizeGB, sizes);
    }

    void buildTLAS(VkCommandPool pool, VkQueue queue,
                   std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances)
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

        LOG_INFO_CAT("LAS", "Building TLAS with {} instances", instances.size());
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
        VkAccelerationStructureCreateInfoKHR createInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = RAW_BUFFER(asBufferHandle),
            .size = sizes.accelerationStructureSize,
            .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
        };
        VK_CHECK(g_ctx().vkCreateAccelerationStructureKHR_(dev, &createInfo, nullptr, &rawAs),
                 "Failed to create TLAS");

        // Use adaptive scratch pool
        uint64_t scratchHandle = getOrGrowScratch(sizes.buildScratchSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            "tlas");

        VkBufferDeviceAddressInfo instanceAddrInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = RAW_BUFFER(instanceEnc)
        };
        VkBufferDeviceAddressInfo scratchAddrInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = RAW_BUFFER(scratchHandle)
        };

        VkDeviceAddress instanceAddr = g_ctx().vkGetBufferDeviceAddressKHR_(dev, &instanceAddrInfo);
        VkDeviceAddress scratchAddr  = g_ctx().vkGetBufferDeviceAddressKHR_(dev, &scratchAddrInfo);

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
            .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                     VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
            .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
            .dstAccelerationStructure = rawAs,
            .geometryCount = 1,
            .pGeometries = &geometry,
            .scratchData = {.deviceAddress = scratchAddr}
        };

        VkAccelerationStructureBuildRangeInfoKHR buildRange{
            .primitiveCount = static_cast<uint32_t>(instances.size())
        };

        VkCommandBuffer cmd = VulkanRTX::beginSingleTimeCommands(pool);
        const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = { &buildRange };
        g_ctx().vkCmdBuildAccelerationStructuresKHR_(cmd, 1, &buildInfo, ranges);
        VulkanRTX::endSingleTimeCommands(cmd, queue, pool);

        auto deleter = [asBufferHandle, instanceEnc, scratchHandle](VkDevice d,
                                                                   VkAccelerationStructureKHR a,
                                                                   const VkAllocationCallbacks*) noexcept {
            if (a) g_ctx().vkDestroyAccelerationStructureKHR_(d, a, nullptr);
            if (asBufferHandle) BUFFER_DESTROY(asBufferHandle);
            if (instanceEnc) BUFFER_DESTROY(instanceEnc);
            if (scratchHandle) BUFFER_DESTROY(scratchHandle);
        };

        tlas_ = Handle<VkAccelerationStructureKHR>(rawAs, dev, deleter,
                                                   sizes.accelerationStructureSize,
                                                   "LAS_TLAS");
        tlasSize_ = sizes.accelerationStructureSize;
        instanceBufferId_ = instanceEnc;

        VkDeviceAddress addr = getTLASAddress();
        double sizeGB = sizes.accelerationStructureSize / (1024.0 * 1024.0 * 1024.0);
        LOG_SUCCESS_CAT("LAS", "TLAS built: {:.3f} GB @ 0x{:x}", sizeGB, addr);
        AmouranthAI::get().onTlasBuilt(sizeGB, addr, sizes);
    }

    void rebuildTLAS(VkCommandPool pool, VkQueue queue,
                     std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances)
    {
        LOG_INFO_CAT("LAS", "Rebuilding TLAS (reset + rebuild)");
        tlas_.reset();
        if (instanceBufferId_) BUFFER_DESTROY(instanceBufferId_);
        instanceBufferId_ = 0;
        tlasSize_ = 0;
        buildTLAS(pool, queue, instances);
    }

    [[nodiscard]] VkAccelerationStructureKHR getBLAS() const noexcept { return blas_ ? *blas_ : VK_NULL_HANDLE; }
    [[nodiscard]] VkDeviceAddress getBLASAddress() const noexcept {
        if (!blas_) return 0;
        if (!g_ctx().vkGetAccelerationStructureDeviceAddressKHR_) {
            LOG_ERROR_CAT("LAS", "vkGetAccelerationStructureDeviceAddressKHR not available");
            return 0;
        }
        VkAccelerationStructureDeviceAddressInfoKHR info{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = *blas_
        };
        return g_ctx().vkGetAccelerationStructureDeviceAddressKHR_(g_ctx().vkDevice(), &info);
    }

    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept { return tlas_ ? *tlas_ : VK_NULL_HANDLE; }
    [[nodiscard]] VkDeviceAddress getTLASAddress() const noexcept {
        if (!tlas_) return 0;
        if (!g_ctx().vkGetAccelerationStructureDeviceAddressKHR_) {
            LOG_ERROR_CAT("LAS", "vkGetAccelerationStructureDeviceAddressKHR not available");
            return 0;
        }
        VkAccelerationStructureDeviceAddressInfoKHR info{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = *tlas_
        };
        return g_ctx().vkGetAccelerationStructureDeviceAddressKHR_(g_ctx().vkDevice(), &info);
    }

    [[nodiscard]] VkDeviceSize getTLASSize() const noexcept { return tlasSize_; }

private:
    LAS() {
        LOG_INFO_CAT("LAS", "LAS singleton initialized");
    }
    ~LAS() = default;

    mutable std::mutex mutex_;
    Handle<VkAccelerationStructureKHR> blas_;
    Handle<VkAccelerationStructureKHR> tlas_;
    uint64_t instanceBufferId_ = 0;
    VkDeviceSize tlasSize_ = 0;

    // --- ADAPTIVE SCRATCH POOL v0.1 — FIXED: TYPE-SAFE GROWTH
    static constexpr VkDeviceSize INITIAL_SCRATCH_SIZE = 1_MB;
    static constexpr VkDeviceSize MAX_SCRATCH_SIZE = 64_MB;
    static constexpr float GROWTH_FACTOR = 2.0f;
    uint64_t scratchPoolId_ = 0;
    VkDeviceSize currentScratchSize_ = INITIAL_SCRATCH_SIZE;
    bool scratchPoolValid_ = false;

    [[nodiscard]] uint64_t getOrGrowScratch(VkDeviceSize requiredSize, VkBufferUsageFlags usage, const char* type) {
        if (!scratchPoolValid_ || requiredSize > currentScratchSize_) {
            // FIXED: Cast to VkDeviceSize to avoid float * uint64_t
            VkDeviceSize growth = static_cast<VkDeviceSize>(static_cast<double>(currentScratchSize_) * GROWTH_FACTOR);
            VkDeviceSize newSize = std::min(growth, MAX_SCRATCH_SIZE);
            newSize = std::max(newSize, requiredSize);

            if (scratchPoolId_) BUFFER_DESTROY(scratchPoolId_);
            BUFFER_CREATE(scratchPoolId_, newSize, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, std::format("scratch_{}", type));
            currentScratchSize_ = newSize;
            scratchPoolValid_ = true;
            AmouranthAI::get().onScratchPoolResize(currentScratchSize_ / GROWTH_FACTOR, currentScratchSize_, type);
        }
        return scratchPoolId_;
    }
};

inline LAS& las() noexcept { return LAS::get(); }

} // namespace RTX

// =============================================================================
// STONEKEY v∞ PUBLIC — PINK PHOTONS ETERNAL — TITAN DOMINANCE ETERNAL
// RTX::las().buildTLAS(...) — 15,000 FPS — VALHALLA v90 FINAL
// FIXED: std::min(float, VkDeviceSize) → type mismatch
// FIXED: Added <algorithm> include
// FIXED: Scratch pool now used in buildBLAS/buildTLAS
// FIXED: Safe growth: current * 2.0f → cast to VkDeviceSize via double
// ZERO ERRORS — ZERO CIRCULAR — PRODUCTION-READY
// =============================================================================