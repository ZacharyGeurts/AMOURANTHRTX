// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — FINAL APOCALYPSE — FIRST LIGHT ETERNAL
// ALL RAY TRACING EXTENSIONS VIA RTX::g_ext — NO MORE RayTracingFunctions CORPSES
// PINK PHOTONS BOUNCE UNBROKEN — NOVEMBER 27, 2025
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Extensions.hpp"  // ← THE ONE TRUE SOURCE OF LIGHT

namespace RTX {

[[nodiscard]] inline VkCommandBuffer beginOneTimeSubmit(VkCommandPool pool) noexcept
{
    EMPIRE_GUARD(pool != VK_NULL_HANDLE, "beginOneTimeSubmit() — NULL COMMAND POOL");

    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VK_CHECK(vkAllocateCommandBuffers(g_ctx().device(), &allocInfo, &cmd));

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    return cmd;
}

inline void endOneTimeSubmit(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) noexcept
{
    EMPIRE_GUARD(cmd != VK_NULL_HANDLE && queue != VK_NULL_HANDLE, "endOneTimeSubmit() — invalid params");

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };

    VK_CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(queue));

    vkFreeCommandBuffers(g_ctx().device(), pool, 1, &cmd);
}

VkDeviceAddress LAS::getBufferAddress(VkBuffer buffer) const noexcept
{
    EMPIRE_GUARD(buffer != VK_NULL_HANDLE, "getBufferAddress() — NULL BUFFER");
    VkBufferDeviceAddressInfo info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer
    };
    return vkGetBufferDeviceAddress(g_ctx().device(), &info);
}

void LAS::buildBLAS(VkCommandPool pool,
                    VkQueue queue,
                    uint64_t vertexHandle,
                    uint64_t indexHandle,
                    uint32_t vertexCount,
                    uint32_t indexCount,
                    VkBuildAccelerationStructureFlagsKHR extraFlags) noexcept
{
    EMPIRE_GUARD(vertexHandle && indexHandle && vertexCount && (indexCount % 3) == 0,
                 "buildBLAS() — Invalid geometry");

    const VkDevice dev = g_ctx().device();
    const VkBuffer vertexBuffer = BufferManager::get(vertexHandle)->buffer;
    const VkBuffer indexBuffer  = BufferManager::get(indexHandle)->buffer;

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = { .deviceAddress = getBufferAddress(vertexBuffer) },
        .vertexStride = sizeof(glm::vec3),
        .maxVertex = vertexCount - 1,
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = { .deviceAddress = getBufferAddress(indexBuffer) }
    };

    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry = { .triangles = triangles },
        .flags = VK_GEOMETRY_OPAQUE_BIT_KHR        
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                 VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR |
                 extraFlags,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    const uint32_t primitiveCount = indexCount / 3;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &primitiveCount, &sizeInfo);

    uint64_t storage = 0, scratch = 0;
    BUFFER_CREATE(storage, sizeInfo.accelerationStructureSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "BLAS_Storage");
    BUFFER_CREATE(scratch, sizeInfo.buildScratchSize,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "BLAS_Scratch");

    VkAccelerationStructureKHR rawAS = VK_NULL_HANDLE;
    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = BufferManager::get(storage)->buffer,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };
    VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(dev, &createInfo, nullptr, &rawAS));

    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = rawAS;
    buildInfo.scratchData.deviceAddress = getBufferAddress(BufferManager::get(scratch)->buffer);

    VkAccelerationStructureBuildRangeInfoKHR buildRange{
        .primitiveCount = primitiveCount
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRange = &buildRange;

    VkCommandBuffer cmd = beginOneTimeSubmit(pool);
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pBuildRange);
    endOneTimeSubmit(cmd, queue, pool);

    if (blas_.valid()) blas_.reset();

    auto deleter = [dev, storage, scratch](VkDevice, VkAccelerationStructureKHR as, const VkAllocationCallbacks*) {
        if (as) g_ext.vkDestroyAccelerationStructureKHR(dev, as, nullptr);
        BufferManager::destroy(storage);
        BufferManager::destroy(scratch);
    };

    blas_ = Handle<VkAccelerationStructureKHR>(rawAS, dev, deleter, sizeInfo.accelerationStructureSize, "WonderBLAS");

    LOG_SUCCESS_CAT("LAS", "BLAS FORGED — {} triangles — size {:.2f} MB",
                    primitiveCount, sizeInfo.accelerationStructureSize / (1024.0 * 1024.0));
}

void LAS::buildTLAS(VkCommandPool pool,
                    VkQueue queue,
                    std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept
{
    if (instances.empty()) {
        if (tlas_.valid()) tlas_.reset();
        if (instanceBufferId_) { BufferManager::destroy(instanceBufferId_); instanceBufferId_ = 0; }
        return;
    }

    const VkDevice dev = g_ctx().device();
    const VkDeviceSize dataSize = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);

    uint64_t instanceBuffer = 0;
    BUFFER_CREATE(instanceBuffer, dataSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "TLAS_InstanceBuffer");

    auto* mapped = static_cast<VkAccelerationStructureInstanceKHR*>(BufferManager::map(instanceBuffer));
    EMPIRE_GUARD(mapped, "TLAS — Failed to map instance buffer");

    for (size_t i = 0; i < instances.size(); ++i) {
        const auto& [blasAS, transform] = instances[i];

        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = blasAS
        };
        VkDeviceAddress blasAddr = g_ext.vkGetAccelerationStructureDeviceAddressKHR(dev, &addrInfo);

        const glm::mat3x4 trans = glm::transpose(transform);

        mapped[i] = VkAccelerationStructureInstanceKHR{
            .transform = VkTransformMatrixKHR{ .matrix = {
                { trans[0][0], trans[0][1], trans[0][2], trans[0][3] },
                { trans[1][0], trans[1][1], trans[1][2], trans[1][3] },
                { trans[2][0], trans[2][1], trans[2][2], trans[2][3] }
            }},
            .instanceCustomIndex = static_cast<uint32_t>(i),
            .mask = 0xFF,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference = blasAddr
        };
    }
    BufferManager::unmap(instanceBuffer);

    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .data = { .deviceAddress = getBufferAddress(BufferManager::get(instanceBuffer)->buffer) }
    };

    VkAccelerationStructureGeometryKHR geometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry = { .instances = instancesData }
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    const uint32_t instanceCount = static_cast<uint32_t>(instances.size());

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &instanceCount, &sizeInfo);

    uint64_t tlasStorage = 0, tlasScratch = 0;
    BUFFER_CREATE(tlasStorage, sizeInfo.accelerationStructureSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "TLAS_Storage");
    BUFFER_CREATE(tlasScratch, sizeInfo.buildScratchSize,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "TLAS_Scratch");

    VkAccelerationStructureKHR rawTLAS = VK_NULL_HANDLE;
    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = BufferManager::get(tlasStorage)->buffer,
        .size = sizeInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };
    VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(dev, &createInfo, nullptr, &rawTLAS));

    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = rawTLAS;
    buildInfo.scratchData.deviceAddress = getBufferAddress(BufferManager::get(tlasScratch)->buffer);

    VkAccelerationStructureBuildRangeInfoKHR buildRange{
        .primitiveCount = instanceCount
    };
    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRange = &buildRange;

    VkCommandBuffer cmd = beginOneTimeSubmit(pool);
    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pBuildRange);
    endOneTimeSubmit(cmd, queue, pool);

    if (tlas_.valid()) tlas_.reset();
    if (instanceBufferId_) BufferManager::destroy(instanceBufferId_);

    auto deleter = [dev, tlasStorage, tlasScratch, instanceBuffer](VkDevice, VkAccelerationStructureKHR as, const VkAllocationCallbacks*) {
        if (as) g_ext.vkDestroyAccelerationStructureKHR(dev, as, nullptr);
        BufferManager::destroy(tlasStorage);
        BufferManager::destroy(tlasScratch);
        BufferManager::destroy(instanceBuffer);
    };

    tlas_ = Handle<VkAccelerationStructureKHR>(rawTLAS, dev, deleter, sizeInfo.accelerationStructureSize, "WonderTLAS");
    instanceBufferId_ = instanceBuffer;
    tlasSize_ = sizeInfo.accelerationStructureSize;

    LOG_SUCCESS_CAT("LAS", "TLAS FORGED — {} instances — size {:.2f} MB", instances.size(),
                    sizeInfo.accelerationStructureSize / (1024.0 * 1024.0));
}

VkDeviceAddress LAS::getBLASAddress() const noexcept
{
    if (!blas_.valid()) return 0;
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = blas_.get()
    };
    return g_ext.vkGetAccelerationStructureDeviceAddressKHR(g_ctx().device(), &info);
}

VkDeviceAddress LAS::getTLASAddress() const noexcept
{
    if (!tlas_.valid()) return 0;
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = tlas_.get()
    };
    return g_ext.vkGetAccelerationStructureDeviceAddressKHR(g_ctx().device(), &info);
}

} // namespace RTX