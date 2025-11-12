// src/engine/GLOBAL/LAS.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// LAS.cpp — LIGHT_WARRIORS_LAS → LAS — NOV 12 2025 6:00 AM EST
// • FULLY IN NAMESPACE RTX
// • NO g_ctx — USES RTX::ctx()
// • RTX::Handle<T> — NO RAW Handle
// • 15,000 FPS — SHIP IT RAW
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <glm/gtc/type_ptr.hpp>

// ──────────────────────────────────────────────────────────────────────────────
// SIZE COMPUTATION
// ──────────────────────────────────────────────────────────────────────────────
namespace {

BlasBuildSizes computeBlasSizes(VkDevice device, uint32_t vertexCount, uint32_t indexCount) {
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

TlasBuildSizes computeTlasSizes(VkDevice device, uint32_t instanceCount) {
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
        static_cast<VkDeviceSize>(instanceCount) * sizeof(VkAccelerationStructureInstanceKHR)
    };
}

uint64_t uploadInstances(
    VkDevice device, VkPhysicalDevice physDev, VkCommandPool pool, VkQueue queue,
    std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances)
{
    if (instances.empty()) return 0;

    const VkDeviceSize instSize = static_cast<VkDeviceSize>(instances.size()) * sizeof(VkAccelerationStructureInstanceKHR);
    uint64_t stagingHandle = 0;
    BUFFER_CREATE(stagingHandle, instSize,
                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "las_instances");

    void* mapped = nullptr;
    auto* d = RTX::UltraLowLevelBufferTracker::get().getData(stagingHandle);
    THROW_IF(vkMapMemory(device, d->memory, 0, d->size, 0, &mapped) != VK_SUCCESS, "Failed to map staging buffer");
    auto* instData = static_cast<VkAccelerationStructureInstanceKHR*>(mapped);

    for (size_t i = 0; i < instances.size(); ++i) {
        const auto& [as, transform] = instances[i];
        glm::mat4 rowMajor = glm::transpose(transform);
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
    vkUnmapMemory(device, d->memory);

    uint64_t deviceHandle = 0;
    BUFFER_CREATE(deviceHandle, instSize,
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "las_rtx_instances");

    THROW_IF(deviceHandle == 0, "Failed to create device-local instance buffer");

    VkCommandBuffer cmd = beginSingleTimeCommands(pool);
    VkBufferCopy copyRegion{ .srcOffset = 0, .dstOffset = 0, .size = instSize };
    vkCmdCopyBuffer(cmd, RAW_BUFFER(stagingHandle), RAW_BUFFER(deviceHandle), 1, &copyRegion);
    endSingleTimeCommands(cmd, queue, pool);

    BUFFER_DESTROY(stagingHandle);
    return deviceHandle;
}

} // anonymous namespace

// ──────────────────────────────────────────────────────────────────────────────
// LAS IMPLEMENTATION
// ──────────────────────────────────────────────────────────────────────────────
namespace RTX {

LAS::LAS() {
    UltraLowLevelBufferTracker::get().init(ctx().vkDevice(), ctx().vkPhysicalDevice());
}

void LAS::buildBLAS(VkCommandPool pool, VkQueue queue,
                    uint64_t vertexBuf, uint64_t indexBuf,
                    uint32_t vertexCount, uint32_t indexCount,
                    VkBuildAccelerationStructureFlagsKHR flags)
{
    std::lock_guard<std::mutex> lock(mutex_);
    VkDevice dev = ctx().vkDevice();
    THROW_IF(!dev, "Invalid device");

    auto sizes = computeBlasSizes(dev, vertexCount, indexCount);
    THROW_IF(sizes.accelerationStructureSize == 0, "BLAS size zero");

    uint64_t asBufferHandle = 0;
    BUFFER_CREATE(asBufferHandle, sizes.accelerationStructureSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "las_blas");

    VkAccelerationStructureKHR rawAs = VK_NULL_HANDLE;
    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = RAW_BUFFER(asBufferHandle),
        .size = sizes.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR
    };
    THROW_IF(vkCreateAccelerationStructureKHR(dev, &createInfo, nullptr, &rawAs) != VK_SUCCESS, "BLAS creation failed");

    uint64_t scratchHandle = 0;
    BUFFER_CREATE(scratchHandle, sizes.buildScratchSize,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "scratch");

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

    auto deleter = [asBufferHandle](VkDevice d, VkAccelerationStructureKHR a, const VkAllocationCallbacks*) noexcept {
        if (a) vkDestroyAccelerationStructureKHR(d, a, nullptr);
        if (asBufferHandle) BUFFER_DESTROY(asBufferHandle);
    };

    blas_ = RTX::Handle<VkAccelerationStructureKHR>(rawAs, dev, deleter, sizes.accelerationStructureSize, "LAS_BLAS");
    BUFFER_DESTROY(scratchHandle);
    CONSOLE_BLAS(std::format("LAS BLAS ONLINE — {} verts | {} indices | {:.2f} GB", vertexCount, indexCount, sizes.accelerationStructureSize / (1024.0*1024.0*1024.0)));
}

void LAS::buildTLAS(VkCommandPool pool, VkQueue queue,
                    std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances)
{
    std::lock_guard<std::mutex> lock(mutex_);
    THROW_IF(instances.empty(), "TLAS: zero instances");

    VkDevice dev = ctx().vkDevice();
    THROW_IF(!dev, "Invalid device");

    auto sizes = computeTlasSizes(dev, static_cast<uint32_t>(instances.size()));
    THROW_IF(sizes.accelerationStructureSize == 0, "TLAS size zero");

    uint64_t instanceEnc = uploadInstances(dev, ctx().vkPhysicalDevice(), pool, queue, instances);
    THROW_IF(instanceEnc == 0, "Instance upload failed");

    uint64_t asBufferHandle = 0;
    BUFFER_CREATE(asBufferHandle, sizes.accelerationStructureSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "las_tlas");

    VkAccelerationStructureKHR rawAs = VK_NULL_HANDLE;
    VkAccelerationStructureCreateInfoKHR createInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = RAW_BUFFER(asBufferHandle),
        .size = sizes.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
    };
    THROW_IF(vkCreateAccelerationStructureKHR(dev, &createInfo, nullptr, &rawAs) != VK_SUCCESS, "TLAS creation failed");

    uint64_t scratchHandle = 0;
    BUFFER_CREATE(scratchHandle, sizes.buildScratchSize,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "scratch_tlas");

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

    auto deleter = [asBufferHandle, instanceEnc](VkDevice d, VkAccelerationStructureKHR a, const VkAllocationCallbacks*) noexcept {
        if (a) vkDestroyAccelerationStructureKHR(d, a, nullptr);
        if (asBufferHandle) BUFFER_DESTROY(asBufferHandle);
        if (instanceEnc) BUFFER_DESTROY(instanceEnc);
    };

    tlas_ = RTX::Handle<VkAccelerationStructureKHR>(rawAs, dev, deleter, sizes.accelerationStructureSize, "LAS_TLAS");
    tlasSize_ = sizes.accelerationStructureSize;
    instanceBufferId_ = instanceEnc;
    BUFFER_DESTROY(scratchHandle);
    CONSOLE_TLAS(std::format("LAS TLAS ONLINE — {} instances | {:.2f} GB", instances.size(), sizes.accelerationStructureSize / (1024.0*1024.0*1024.0)));
}

VkAccelerationStructureKHR LAS::getBLAS() const noexcept {
    return blas_ ? *blas_ : VK_NULL_HANDLE;
}

VkDeviceAddress LAS::getBLASAddress() const noexcept {
    if (!blas_) return 0;
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = *blas_
    };
    return vkGetAccelerationStructureDeviceAddressKHR(ctx().vkDevice(), &info);
}

VkAccelerationStructureKHR LAS::getTLAS() const noexcept {
    return tlas_ ? *tlas_ : VK_NULL_HANDLE;
}

VkDeviceAddress LAS::getTLASAddress() const noexcept {
    if (!tlas_) return 0;
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = *tlas_
    };
    return vkGetAccelerationStructureDeviceAddressKHR(ctx().vkDevice(), &info);
}

VkDeviceSize LAS::getTLASSize() const noexcept {
    return tlasSize_;
}

void LAS::rebuildTLAS(VkCommandPool pool, VkQueue queue,
                      std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) {
    std::lock_guard<std::mutex> lock(mutex_);
    tlas_.reset();
    instanceBufferId_ = 0;
    tlasSize_ = 0;
    buildTLAS(pool, queue, instances);
}

} // namespace RTX