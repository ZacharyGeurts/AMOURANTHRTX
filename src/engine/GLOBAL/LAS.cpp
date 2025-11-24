// src/engine/GLOBAL/LAS.cpp
// =============================================================================
// AMOURANTH RTX — LAS v∞ — FULL MANUAL PFN CONTROL — FIRST LIGHT ETERNAL
// "We do not wait for Vulkan 1.4. We load the extensions ourselves."
// =============================================================================

#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_beta.h>

#include <cassert>
#include <cstring>
#include <span>

using namespace Logging::Color;
using StoneKey::stone_device;

namespace RTX {

namespace {

[[nodiscard]] inline VkCommandBuffer beginOneTimeSubmit(VkCommandPool pool) noexcept
{
    VkCommandBuffer cmd = VK_NULL_HANDLE;
    VkCommandBufferAllocateInfo allocInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.commandPool        = pool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VK_CHECK(vkAllocateCommandBuffers(stone_device(), &allocInfo, &cmd));

    VkCommandBufferBeginInfo beginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    return cmd;
}

inline void endOneTimeSubmit(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) noexcept
{
    VK_CHECK(vkEndCommandBuffer(cmd));

    VkSubmitInfo submit{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;

    VK_CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
    VK_CHECK(vkQueueWaitIdle(queue));
    vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);
}

} // anonymous namespace

VkDeviceAddress LAS::getBufferAddress(VkBuffer buffer) const noexcept
{
    assert(buffer != VK_NULL_HANDLE);

    VkBufferDeviceAddressInfo info{ .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    info.buffer = buffer;
    return vkGetBufferDeviceAddress(stone_device(), &info);
}

void LAS::buildBLAS(VkCommandPool pool,
                    VkQueue queue,
                    uint64_t vertexHandle,
                    uint64_t indexHandle,
                    uint32_t vertexCount,
                    uint32_t indexCount,
                    VkBuildAccelerationStructureFlagsKHR extraFlags) noexcept
{
    assert(vertexHandle && indexHandle && vertexCount && indexCount % 3 == 0);

    const VkDevice dev = stone_device();
    const VkBuffer vertexBuffer = RAW_BUFFER(vertexHandle);
    const VkBuffer indexBuffer  = RAW_BUFFER(indexHandle);

    VkAccelerationStructureGeometryTrianglesDataKHR triangles{};
    triangles.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    triangles.vertexFormat   = VK_FORMAT_R32G32B32_SFLOAT;
    triangles.vertexData.deviceAddress = getBufferAddress(vertexBuffer);
    triangles.vertexStride   = sizeof(glm::vec3);
    triangles.maxVertex      = vertexCount;
    triangles.indexType      = VK_INDEX_TYPE_UINT32;
    triangles.indexData.deviceAddress = getBufferAddress(indexBuffer);

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags        = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.triangles = triangles;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                              VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR |
                              extraFlags;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries   = &geometry;

    const uint32_t primitiveCount = indexCount / 3;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    RayTracingFunctions::vkGetAccelerationStructureBuildSizesKHR(
        dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &buildInfo, &primitiveCount, &sizeInfo);

    uint64_t storage = 0, scratch = 0;
    BUFFER_CREATE(storage, sizeInfo.accelerationStructureSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "BLAS_Storage");

    BUFFER_CREATE(scratch, sizeInfo.buildScratchSize,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "BLAS_Scratch");

    VkAccelerationStructureKHR rawAS = VK_NULL_HANDLE;
    VkAccelerationStructureCreateInfoKHR createInfo{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    createInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    createInfo.size   = sizeInfo.accelerationStructureSize;
    createInfo.buffer = RAW_BUFFER(storage);

    VK_CHECK(RayTracingFunctions::vkCreateAccelerationStructureKHR(dev, &createInfo, nullptr, &rawAS));

    buildInfo.mode                     = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = rawAS;
    buildInfo.scratchData.deviceAddress = getBufferAddress(RAW_BUFFER(scratch));

    const VkAccelerationStructureBuildRangeInfoKHR range{ primitiveCount };
    const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = { &range };

    VkCommandBuffer cmd = beginOneTimeSubmit(pool);
    RayTracingFunctions::vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, ranges);
    endOneTimeSubmit(cmd, queue, pool);

    if (blas_.valid()) blas_.reset();

    auto deleter = [dev, storage, scratch](VkDevice, VkAccelerationStructureKHR as, const VkAllocationCallbacks*) {
        if (as) RayTracingFunctions::vkDestroyAccelerationStructureKHR(dev, as, nullptr);
        if (storage) { uint64_t h = storage; BUFFER_DESTROY(h); }
        if (scratch) { uint64_t h = scratch; BUFFER_DESTROY(h); }
    };

    blas_ = Handle<VkAccelerationStructureKHR>(rawAS, dev, deleter, sizeInfo.accelerationStructureSize, "WonderBLAS");
    LOG_SUCCESS_CAT("RTX", "BLAS FORGED — {} TRIANGLES — PINK PHOTONS APPROVED", primitiveCount);
}

// TLAS and other functions — same pattern: use RayTracingFunctions::vkXXX

VkDeviceAddress LAS::getBLASAddress() const noexcept
{
    if (!blas_.valid()) return 0;
    VkAccelerationStructureDeviceAddressInfoKHR info{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    info.accelerationStructure = blas_.get();
    return RayTracingFunctions::vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &info);
}

VkDeviceAddress LAS::getTLASAddress() const noexcept
{
    if (!tlas_.valid()) return 0;
    VkAccelerationStructureDeviceAddressInfoKHR info{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    info.accelerationStructure = tlas_.get();
    return RayTracingFunctions::vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &info);
}

} // namespace RTX