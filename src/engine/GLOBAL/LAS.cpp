// src/engine/GLOBAL/LAS.cpp
// =============================================================================
//
// Dual License:
//  • GNU General Public License v3.0 or later (GPL-3.0-or-later)
//    https://www.gnu.org/licenses/gpl-3.0.html
//  • Commercial license available — contact gzac5314@gmail.com
//
// AMOURANTH RTX — Acceleration Structure Manager
// FIRST LIGHT ETERNAL — NOVEMBER 22, 2025
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/VulkanCore.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"

using namespace RTX;

// =============================================================================
// VulkanAccel — Constructor
// =============================================================================
VulkanAccel::VulkanAccel(VkDevice)
{
    LOG_SUCCESS_CAT("VulkanAccel", "RTX Acceleration System initialized — ready for BLAS/TLAS construction");
}

// =============================================================================
// Destroy helpers
// =============================================================================
void VulkanAccel::destroy(BLAS& blas)
{
    if (blas.as)    g_ctx().vkDestroyAccelerationStructureKHR()(g_ctx().device(), blas.as, nullptr);
    if (blas.buffer) vkDestroyBuffer(g_ctx().device(), blas.buffer, nullptr);
    if (blas.memory) vkFreeMemory(g_ctx().device(), blas.memory, nullptr);
    blas = {};
}

void VulkanAccel::destroy(TLAS& tlas)
{
    if (tlas.as)              g_ctx().vkDestroyAccelerationStructureKHR()(g_ctx().device(), tlas.as, nullptr);
    if (tlas.buffer)          vkDestroyBuffer(g_ctx().device(), tlas.buffer, nullptr);
    if (tlas.memory)          vkFreeMemory(g_ctx().device(), tlas.memory, nullptr);
    if (tlas.instanceBuffer)  vkDestroyBuffer(g_ctx().device(), tlas.instanceBuffer, nullptr);
    if (tlas.instanceMemory)  vkFreeMemory(g_ctx().device(), tlas.instanceMemory, nullptr);
    tlas = {};
}

// =============================================================================
// BLAS Creation
// =============================================================================
VulkanAccel::BLAS VulkanAccel::createBLAS(
    const std::vector<AccelGeometry>& geometries,
    VkBuildAccelerationStructureFlagsKHR flags,
    VkCommandBuffer externalCmd,
    std::string_view name)
{
    BLAS blas{};
    blas.name = name;

    uint32_t primCount = 0;
    std::vector<VkAccelerationStructureGeometryKHR> vkGeoms;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges;
    vkGeoms.reserve(geometries.size());
    ranges.reserve(geometries.size());

    for (const auto& g : geometries) {
        const uint32_t triCount = g.indexCount / 3;
        primCount += triCount;

        if (g.vertexData.deviceAddress == 0 || g.indexData.deviceAddress == 0) {
            LOG_FATAL_CAT("VulkanAccel", "Invalid device address in geometry");
            return {};
        }

        VkAccelerationStructureGeometryKHR geom{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
        geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geom.flags = g.flags;

        geom.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geom.geometry.triangles.vertexFormat = g.vertexFormat;
        geom.geometry.triangles.vertexStride = g.vertexStride;
        geom.geometry.triangles.maxVertex = g.vertexCount ? g.vertexCount - 1 : 0;
        geom.geometry.triangles.vertexData.deviceAddress = g.vertexData.deviceAddress;
        geom.geometry.triangles.indexData.deviceAddress  = g.indexData.deviceAddress;
        geom.geometry.triangles.indexType = g.indexType;
        geom.geometry.triangles.transformData = g.transformData;

        vkGeoms.push_back(geom);
        ranges.push_back({ triCount, 0, 0, 0 });
    }

    if (primCount == 0) {
        LOG_FATAL_CAT("VulkanAccel", "No triangles submitted for BLAS");
        return blas;
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = flags | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = static_cast<uint32_t>(vkGeoms.size());
    buildInfo.pGeometries = vkGeoms.data();

    VkAccelerationStructureBuildSizesInfoKHR sizes{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    g_ctx().vkGetAccelerationStructureBuildSizesKHR()(
        g_ctx().device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primCount,
        &sizes);

    uint64_t storage = 0;
    BUFFER_CREATE(storage, sizes.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        std::string(name) + "_BLAS");

    VkAccelerationStructureCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    createInfo.size = sizes.accelerationStructureSize;
    createInfo.buffer = RAW_BUFFER(storage);

    VK_CHECK(g_ctx().vkCreateAccelerationStructureKHR()(g_ctx().device(), &createInfo, nullptr, &blas.as),
             "Failed to create BLAS");

    uint64_t scratch = 0;
    BUFFER_CREATE(scratch, sizes.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        std::string(name) + "_scratch");

    VkBufferDeviceAddressInfo scratchAddrInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, RAW_BUFFER(scratch) };
    buildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(g_ctx().device(), &scratchAddrInfo);
    buildInfo.dstAccelerationStructure = blas.as;

    VkCommandBuffer cmd = externalCmd ? externalCmd : beginOneTime(g_ctx().commandPool_);
    const VkAccelerationStructureBuildRangeInfoKHR* pRanges[] = { ranges.data() };
    g_ctx().vkCmdBuildAccelerationStructuresKHR()(cmd, 1, &buildInfo, pRanges);

    if (!externalCmd)
        endSingleTimeCommandsAsync(cmd, g_ctx().graphicsQueue_, g_ctx().commandPool_);

    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        nullptr,
        blas.as
    };
    blas.address = g_ctx().vkGetAccelerationStructureDeviceAddressKHR()(g_ctx().device(), &addrInfo);

    blas.buffer = RAW_BUFFER(storage);
    blas.memory = BUFFER_MEMORY(storage);
    blas.size   = sizes.accelerationStructureSize;
    BUFFER_DESTROY(scratch);

    LOG_SUCCESS_CAT("VulkanAccel", "BLAS \"{}\" created — {} triangles — address 0x{:016X}", name, primCount, blas.address);
    return blas;
}

// =============================================================================
// TLAS Creation
// =============================================================================
VulkanAccel::TLAS VulkanAccel::createTLAS(
    const std::vector<VkAccelerationStructureInstanceKHR>& instances,
    VkBuildAccelerationStructureFlagsKHR flags,
    VkCommandBuffer externalCmd,
    std::string_view name)
{
    TLAS tlas{};
    tlas.name = name;

    const uint32_t count = static_cast<uint32_t>(instances.size());
    if (count == 0) {
        LOG_WARN_CAT("VulkanAccel", "TLAS created with zero instances");
        return tlas;
    }

    const VkDeviceSize instanceDataSize = count * sizeof(VkAccelerationStructureInstanceKHR);
    uint64_t instBuf = 0;
    BUFFER_CREATE(instBuf, instanceDataSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        std::string(name) + "_instances");

    void* mapped = nullptr;
    BUFFER_MAP(instBuf, mapped);
    std::memcpy(mapped, instances.data(), instanceDataSize);
    BUFFER_UNMAP(instBuf);

    VkBufferDeviceAddressInfo instAddrInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, RAW_BUFFER(instBuf) };
    const VkDeviceAddress instAddr = vkGetBufferDeviceAddress(g_ctx().device(), &instAddrInfo);

    VkAccelerationStructureGeometryKHR geom{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    geom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geom.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geom.geometry.instances.arrayOfPointers = VK_FALSE;
    geom.geometry.instances.data.deviceAddress = instAddr;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = flags;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geom;

    VkAccelerationStructureBuildSizesInfoKHR sizes{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    g_ctx().vkGetAccelerationStructureBuildSizesKHR()(
        g_ctx().device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &count,
        &sizes);

    uint64_t storage = 0;
    BUFFER_CREATE(storage, sizes.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        std::string(name) + "_TLAS");

    VkAccelerationStructureCreateInfoKHR createInfo{ VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    createInfo.size = sizes.accelerationStructureSize;
    createInfo.buffer = RAW_BUFFER(storage);

    VK_CHECK(g_ctx().vkCreateAccelerationStructureKHR()(g_ctx().device(), &createInfo, nullptr, &tlas.as),
             "Failed to create TLAS");

    uint64_t scratch = 0;
    BUFFER_CREATE(scratch, sizes.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        std::string(name) + "_scratch");

    VkBufferDeviceAddressInfo scratchAddrInfo{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, RAW_BUFFER(scratch) };
    buildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(g_ctx().device(), &scratchAddrInfo);
    buildInfo.dstAccelerationStructure = tlas.as;

    VkCommandBuffer cmd = externalCmd ? externalCmd : beginOneTime(g_ctx().commandPool_);
    VkAccelerationStructureBuildRangeInfoKHR range{ count, 0, 0, 0 };
    const VkAccelerationStructureBuildRangeInfoKHR* pRanges[] = { &range };
    g_ctx().vkCmdBuildAccelerationStructuresKHR()(cmd, 1, &buildInfo, pRanges);

    if (!externalCmd)
        endSingleTimeCommandsAsync(cmd, g_ctx().graphicsQueue_, g_ctx().commandPool_);

    VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
        VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        nullptr,
        tlas.as
    };
    tlas.address = g_ctx().vkGetAccelerationStructureDeviceAddressKHR()(g_ctx().device(), &addrInfo);

    tlas.buffer = RAW_BUFFER(storage);
    tlas.memory = BUFFER_MEMORY(storage);
    tlas.instanceBuffer = RAW_BUFFER(instBuf);
    tlas.instanceMemory = BUFFER_MEMORY(instBuf);
    tlas.size = sizes.accelerationStructureSize;
    BUFFER_DESTROY(scratch);

    LOG_SUCCESS_CAT("VulkanAccel", "TLAS \"{}\" created — {} instances — address 0x{:016X}", name, count, tlas.address);
    return tlas;
}

// =============================================================================
// LAS Wrapper Methods
// =============================================================================
void LAS::buildBLAS(VkCommandPool pool,
                    uint64_t vertexBufferObf,
                    uint64_t indexBufferObf,
                    uint32_t vertexCount,
                    uint32_t indexCount,
                    VkBuildAccelerationStructureFlagsKHR extraFlags)
{
    AccelGeometry g{};
    g.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    g.vertexStride = 44;
    g.vertexCount = vertexCount;

    VkBufferDeviceAddressInfo info{ VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO, nullptr, RAW_BUFFER(vertexBufferObf) };
    g.vertexData.deviceAddress = vkGetBufferDeviceAddress(g_ctx().device(), &info);

    info.buffer = RAW_BUFFER(indexBufferObf);
    g.indexData.deviceAddress = vkGetBufferDeviceAddress(g_ctx().device(), &info);

    g.indexType = VK_INDEX_TYPE_UINT32;
    g.indexCount = indexCount;

    VkCommandBuffer cmd = beginOneTime(pool);
    blas_ = accel_->createBLAS({g}, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | extraFlags, cmd, "Scene_BLAS");
    endSingleTimeCommandsAsync(cmd, g_ctx().graphicsQueue_, pool);
    ++generation_;
}

void LAS::buildTLAS(VkCommandPool pool,
                    const std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>>& instances)
{
    std::vector<VkAccelerationStructureInstanceKHR> vkInst;
    vkInst.reserve(instances.size());

    for (const auto& [as, transform] : instances) {
        VkAccelerationStructureInstanceKHR inst{};
        std::memcpy(&inst.transform.matrix, glm::value_ptr(transform), sizeof(inst.transform.matrix));
        inst.mask = 0xFF;
        inst.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

        VkAccelerationStructureDeviceAddressInfoKHR info{
            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            nullptr,
            as
        };
        inst.accelerationStructureReference = g_ctx().vkGetAccelerationStructureDeviceAddressKHR()(g_ctx().device(), &info);
        vkInst.push_back(inst);
    }

    VkCommandBuffer cmd = beginOneTime(pool);
    tlas_ = accel_->createTLAS(vkInst, VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR, cmd, "Scene_TLAS");
    endSingleTimeCommandsAsync(cmd, g_ctx().graphicsQueue_, pool);
    ++generation_;
}