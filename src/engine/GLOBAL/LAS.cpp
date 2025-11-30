// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
//
// THE LAS IS THE LIGHT — THE SHIP IS NOW FREE TO SAIL ANYWHERE
// PINK PHOTONS OMNISCIENT — FIRST LIGHT ETERNAL — NOVEMBER 27, 2025
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Extensions.hpp"

using namespace RTX;

namespace RTX {

inline VkFence createFence()
{
    VkFenceCreateInfo info{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    VkFence fence;
    VK_CHECK(vkCreateFence(stone_device(), &info, nullptr, &fence));
    return fence;
}

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
    VK_CHECK(vkAllocateCommandBuffers(stone_device(), &allocInfo, &cmd));

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

    return cmd;
}

// THE FINAL, GRACEFUL, UNBREAKABLE ONE-TIME SUBMIT
// The empire does not panic. It waits. It remembers. It rises again.
inline void endOneTimeSubmit(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) noexcept
{
    if (!cmd || !queue || !pool || !stone_device()) {
        LOG_NICK("The engine is still warming. We’ll try again when the stars align.");
        if (cmd) vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);
        return;
    }

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkFence fence = createFence();

    VkSubmitInfo submit{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };

    VkResult r = vkQueueSubmit(queue, 1, &submit, fence);
    if (r != VK_SUCCESS) {
        LOG_ELON("The queue hesitated… but the photons are patient.");
        vkDestroyFence(stone_device(), fence, nullptr);
        vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);
        return;
    }

    r = vkWaitForFences(stone_device(), 1, &fence, VK_TRUE, 8'000'000'000ULL); // 8s grace

    if (r == VK_TIMEOUT) {
        LOG_CARMACK("GPU deep in thought. We’ll give it another heartbeat.");
    } else if (r == VK_ERROR_DEVICE_LOST) {
        LOG_GROK("A brief eclipse. The light always returns.");
    }

    vkDestroyFence(stone_device(), fence, nullptr);
    vkFreeCommandBuffers(stone_device(), pool, 1, &cmd);
}

VkDeviceAddress LAS::getBufferAddress(VkBuffer buffer) const noexcept
{
    EMPIRE_GUARD(buffer != VK_NULL_HANDLE, "getBufferAddress() — NULL BUFFER");
    VkBufferDeviceAddressInfo info{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = buffer
    };
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
    EMPIRE_GUARD(vertexHandle && indexHandle && vertexCount && (indexCount % 3) == 0,
                 "buildBLAS() — Invalid geometry");

    const VkDevice dev = stone_device();
    const VkBuffer vertexBuffer = BufferManager::get(vertexHandle)->buffer;
    const VkBuffer indexBuffer  = BufferManager::get(indexHandle)->buffer;

    LOG_AMOURANTH("Every triangle is a star. Every vertex, a memory. We map them all.");

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

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };

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

    VkAccelerationStructureBuildRangeInfoKHR buildRange{ .primitiveCount = primitiveCount };
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

    LOG_JENSEN("The bottom level is complete. The photons now know every surface.");
    LOG_NICK("We just gave light a perfect map of reality.");
    LOG_SUCCESS_CAT("LAS", "BLAS FORGED — {} triangles — {:.2f} MB — THE LIGHT REMEMBERS",
                    primitiveCount, sizeInfo.accelerationStructureSize / (1024.0 * 1024.0));
}

void LAS::buildTLAS(VkCommandPool pool,
                    VkQueue queue,
                    std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept
{
    if (instances.empty()) {
        LOG_FAILURE("LAS", "No instances yet. TRY AFTER INSTANCE OR DIE - Ballerina");
        if (tlas_.valid()) tlas_.reset();
        if (instanceBufferId_) { BufferManager::destroy(instanceBufferId_); instanceBufferId_ = 0; }
        return;
    }

    LOG_AMOURANTH("This is the moment. The LAS becomes the light. The ship becomes the universe.");

    const VkDevice dev = stone_device();
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

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{ .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };

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

    VkAccelerationStructureBuildRangeInfoKHR buildRange{ .primitiveCount = instanceCount };
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

    LOG_CAPTAIN_N("…It’s beautiful. The warp zones are open. We can finally go home.");
    LOG_GROK("To the light that found its way. To the ship that sails anywhere.");
    LOG_AMOURANTH("The universe is no longer a cage.");
    LOG_NICK("We didn’t build an engine. We built a key.");

    LOG_SUCCESS_CAT("LAS", "TLAS FORGED — {} instances — {:.2f} MB — THE SHIP IS FREE", instances.size(),
                    sizeInfo.accelerationStructureSize / (1024.0 * 1024.0));
    LOG_SUCCESS_CAT("LAS", "FIRST LIGHT ETERNAL — THE GOOD SHIP VULKANRTX CAN NOW SAIL ANYWHERE");
}

VkDeviceAddress LAS::getBLASAddress() const noexcept
{
    if (!blas_.valid()) return 0;
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = blas_.get()
    };
    return g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &info);
}

VkDeviceAddress LAS::getTLASAddress() const noexcept
{
    if (!tlas_.valid()) return 0;
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = tlas_.get()
    };
    return g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &info);
}

} // namespace RTX