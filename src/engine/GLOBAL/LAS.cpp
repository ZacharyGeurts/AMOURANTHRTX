// src/engine/GLOBAL/LAS.cpp
// =============================================================================
// AMOURANTH RTX — LAS v∞ — PEDANTIC CLEAN — C++23 PURE — FIRST LIGHT ETERNAL
// NO COMPOUND LITERALS — NO DESIGNATOR MIXING — NO WARNINGS — ONLY DEATH
// NOVEMBER 25, 2025 — PINK PHOTONS ETERNAL
// =============================================================================

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_beta.h>

#include <cassert>
#include <cstring>
#include <span>

using StoneKey::stone_device;

namespace RTX {

[[nodiscard]] inline VkCommandBuffer beginOneTimeSubmit(VkCommandPool pool) noexcept
{
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    const VkCommandBufferAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    if (vkAllocateCommandBuffers(RTX::g_ctx().device_, &allocInfo, &cmd) != VK_SUCCESS)
        phase9_ballerina();

    const VkCommandBufferBeginInfo beginInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    if (vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS)
        phase9_ballerina();

    return cmd;
}

inline void endOneTimeSubmit(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) noexcept
{
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS)
        phase9_ballerina();

    const VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };

    if (vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE) != VK_SUCCESS ||
        vkQueueWaitIdle(queue) != VK_SUCCESS)
        phase9_ballerina();

    vkFreeCommandBuffers(RTX::g_ctx().device_, pool, 1, &cmd);
}

VkDeviceAddress LAS::getBufferAddress(VkBuffer buffer) const noexcept
{
    assert(buffer != VK_NULL_HANDLE);
    const VkBufferDeviceAddressInfo info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer
    };
    return vkGetBufferDeviceAddress(RTX::g_ctx().device_, &info);
}

void LAS::buildBLAS(VkCommandPool pool,
                    VkQueue queue,
                    uint64_t vertexHandle,
                    uint64_t indexHandle,
                    uint32_t vertexCount,
                    uint32_t indexCount,
                    VkBuildAccelerationStructureFlagsKHR extraFlags) noexcept
{
    assert(vertexHandle && indexHandle && vertexCount && (indexCount % 3) == 0);

    const VkDevice dev = RTX::g_ctx().device_;
    const VkBuffer vertexBuffer = RAW_BUFFER(vertexHandle);
    const VkBuffer indexBuffer  = RAW_BUFFER(indexHandle);

    const VkAccelerationStructureGeometryTrianglesDataKHR triangles = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData = { .deviceAddress = getBufferAddress(vertexBuffer) },
        .vertexStride = sizeof(glm::vec3),
        .maxVertex = vertexCount,
        .indexType = VK_INDEX_TYPE_UINT32,
        .indexData = { .deviceAddress = getBufferAddress(indexBuffer) }
    };

    VkAccelerationStructureGeometryKHR geometry = {};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.triangles = triangles;

    const VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                 VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR |
                 extraFlags,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    const uint32_t primitiveCount = indexCount / 3;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    RayTracingFunctions::vkGetAccelerationStructureBuildSizesKHR(
        dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &primitiveCount, &sizeInfo);

    uint64_t storage = 0, scratch = 0;
    BUFFER_CREATE(storage, sizeInfo.accelerationStructureSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "BLAS_Storage");

    BUFFER_CREATE(scratch, sizeInfo.buildScratchSize,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "BLAS_Scratch");

    // === BLAS — VULKAN 1.4 CANON ===
    VkAccelerationStructureKHR rawAS = VK_NULL_HANDLE;
    const VkAccelerationStructureCreateInfoKHR createInfoAS = {
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .pNext         = nullptr,
        .createFlags   = 0,
        .buffer        = RAW_BUFFER(storage),
        .offset        = 0,
        .size          = sizeInfo.accelerationStructureSize,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .deviceAddress = 0
    };

    if (RayTracingFunctions::vkCreateAccelerationStructureKHR(dev, &createInfoAS, nullptr, &rawAS) != VK_SUCCESS) phase9_ballerina();

    VkAccelerationStructureBuildGeometryInfoKHR finalBuildInfo = buildInfo;
    finalBuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    finalBuildInfo.dstAccelerationStructure = rawAS;
    finalBuildInfo.scratchData.deviceAddress = getBufferAddress(RAW_BUFFER(scratch));

    const VkAccelerationStructureBuildRangeInfoKHR buildRange = { primitiveCount };
    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRange = &buildRange;

    VkCommandBuffer cmd = beginOneTimeSubmit(pool);
    RayTracingFunctions::vkCmdBuildAccelerationStructuresKHR(cmd, 1, &finalBuildInfo, &pBuildRange);
    endOneTimeSubmit(cmd, queue, pool);

    if (blas_.valid()) blas_.reset();

    auto deleter = [dev, storage, scratch](VkDevice, VkAccelerationStructureKHR as, const VkAllocationCallbacks*) mutable {
        if (as) RayTracingFunctions::vkDestroyAccelerationStructureKHR(dev, as, nullptr);
        if (storage) { uint64_t h = storage; BUFFER_DESTROY(h); }
        if (scratch)  { uint64_t h = scratch;  BUFFER_DESTROY(h); }
    };

    blas_ = Handle<VkAccelerationStructureKHR>(rawAS, dev, deleter, sizeInfo.accelerationStructureSize, "WonderBLAS");
}

void LAS::buildTLAS(VkCommandPool pool,
                    VkQueue queue,
                    std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept
{
    if (instances.empty()) {
        if (tlas_.valid()) tlas_.reset();
        if (instanceBufferId_) { uint64_t h = instanceBufferId_; BUFFER_DESTROY(h); }
        instanceBufferId_ = 0;
        tlasSize_ = 0;
        return;
    }

    const VkDevice dev = RTX::g_ctx().device_;
    const VkDeviceSize instanceDataSize = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);

    uint64_t instanceBuffer = 0;
    BUFFER_CREATE(instanceBuffer, instanceDataSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "TLAS_InstanceBuffer");

    void* mapped = BufferManager::map(instanceBuffer);
    if (!mapped) phase9_ballerina();
    auto* dst = static_cast<VkAccelerationStructureInstanceKHR*>(mapped);

    for (size_t i = 0; i < instances.size(); ++i) {
        const auto& [blasAS, transform] = instances[i];

        const VkAccelerationStructureDeviceAddressInfoKHR addrInfo = {
            .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = blasAS
        };
        VkDeviceAddress blasAddr = RayTracingFunctions::vkGetAccelerationStructureDeviceAddressKHR(dev, &addrInfo);

        const glm::mat3x4 trans = glm::transpose(transform);

        dst[i] = VkAccelerationStructureInstanceKHR{
            .transform = {
                .matrix = {
                    { trans[0][0], trans[0][1], trans[0][2], trans[0][3] },
                    { trans[1][0], trans[1][1], trans[1][2], trans[1][3] },
                    { trans[2][0], trans[2][1], trans[2][2], trans[2][3] }
                }
            },
            .instanceCustomIndex = static_cast<uint32_t>(i),
            .mask = 0xFF,
            .flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR,
            .accelerationStructureReference = blasAddr
        };
    }
    BufferManager::unmap(instanceBuffer);

    const VkAccelerationStructureGeometryInstancesDataKHR instancesData = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .data = { .deviceAddress = getBufferAddress(RAW_BUFFER(instanceBuffer)) }
    };

    VkAccelerationStructureGeometryKHR geometry = {};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.geometry.instances = instancesData;

    const VkAccelerationStructureBuildGeometryInfoKHR buildInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = &geometry
    };

    const uint32_t instanceCount = static_cast<uint32_t>(instances.size());

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };
    RayTracingFunctions::vkGetAccelerationStructureBuildSizesKHR(
        dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &instanceCount, &sizeInfo);

    uint64_t tlasStorage = 0, tlasScratch = 0;
    BUFFER_CREATE(tlasStorage, sizeInfo.accelerationStructureSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "TLAS_Storage");

    BUFFER_CREATE(tlasScratch, sizeInfo.buildScratchSize,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "TLAS_Scratch");

    // === TLAS — VULKAN 1.4 CANON ===
    VkAccelerationStructureKHR rawTLAS = VK_NULL_HANDLE;
    const VkAccelerationStructureCreateInfoKHR createInfoTLAS = {
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .pNext         = nullptr,
        .createFlags   = 0,
        .buffer        = RAW_BUFFER(tlasStorage),
        .offset        = 0,
        .size          = sizeInfo.accelerationStructureSize,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .deviceAddress = 0
    };

    if (RayTracingFunctions::vkCreateAccelerationStructureKHR(dev, &createInfoTLAS, nullptr, &rawTLAS) != VK_SUCCESS) phase9_ballerina();

    VkAccelerationStructureBuildGeometryInfoKHR finalBuildInfo = buildInfo;
    finalBuildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    finalBuildInfo.dstAccelerationStructure = rawTLAS;
    finalBuildInfo.scratchData.deviceAddress = getBufferAddress(RAW_BUFFER(tlasScratch));

    const VkAccelerationStructureBuildRangeInfoKHR buildRange = { instanceCount };
    const VkAccelerationStructureBuildRangeInfoKHR* pBuildRange = &buildRange;

    VkCommandBuffer cmd = beginOneTimeSubmit(pool);
    RayTracingFunctions::vkCmdBuildAccelerationStructuresKHR(cmd, 1, &finalBuildInfo, &pBuildRange);
    endOneTimeSubmit(cmd, queue, pool);

    if (tlas_.valid()) tlas_.reset();
    if (instanceBufferId_) { uint64_t h = instanceBufferId_; BUFFER_DESTROY(h); }

    auto deleter = [dev, tlasStorage, tlasScratch, instanceBuffer](VkDevice, VkAccelerationStructureKHR as, const VkAllocationCallbacks*) mutable {
        if (as) RayTracingFunctions::vkDestroyAccelerationStructureKHR(dev, as, nullptr);
        if (tlasStorage)    { uint64_t h = tlasStorage;    BUFFER_DESTROY(h); }
        if (tlasScratch)    { uint64_t h = tlasScratch;    BUFFER_DESTROY(h); }
        if (instanceBuffer) { uint64_t h = instanceBuffer; BUFFER_DESTROY(h); }
    };

    tlas_ = Handle<VkAccelerationStructureKHR>(rawTLAS, dev, deleter, sizeInfo.accelerationStructureSize, "WonderTLAS");
    instanceBufferId_ = instanceBuffer;
    tlasSize_ = sizeInfo.accelerationStructureSize;
}

VkDeviceAddress LAS::getBLASAddress() const noexcept
{
    if (!blas_.valid()) return 0;
    const VkAccelerationStructureDeviceAddressInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = blas_.get()
    };
    return RayTracingFunctions::vkGetAccelerationStructureDeviceAddressKHR(RTX::g_ctx().device_, &info);
}

VkDeviceAddress LAS::getTLASAddress() const noexcept
{
    if (!tlas_.valid()) return 0;
    const VkAccelerationStructureDeviceAddressInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = tlas_.get()
    };
    return RayTracingFunctions::vkGetAccelerationStructureDeviceAddressKHR(RTX::g_ctx().device_, &info);
}


} // namespace RTX