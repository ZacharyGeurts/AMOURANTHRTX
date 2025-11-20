// src/engine/Vulkan/VulkanAccel.cpp
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 20, 2025 — APOCALYPSE FINAL v4.2
// VulkanAccel — FINAL — PERFECT 5-ARG BUFFER_CREATE — PINK PHOTONS ETERNAL
// =============================================================================

#include "engine/Vulkan/VulkanAccel.hpp"
#include "engine/GLOBAL/logging.hpp"

PFN_vkCreateAccelerationStructureKHR VulkanAccel::vkCreateAccelerationStructureKHR = nullptr;
PFN_vkDestroyAccelerationStructureKHR VulkanAccel::vkDestroyAccelerationStructureKHR = nullptr;

VulkanAccel::VulkanAccel(VkDevice device) {
    if (!vkCreateAccelerationStructureKHR) {
        vkCreateAccelerationStructureKHR = reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkCreateAccelerationStructureKHR"));
        vkDestroyAccelerationStructureKHR = reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(vkGetDeviceProcAddr(device, "vkDestroyAccelerationStructureKHR"));
        if (!vkCreateAccelerationStructureKHR || !vkDestroyAccelerationStructureKHR) {
            LOG_FATAL_CAT("RTX", "Failed to load acceleration structure functions");
        }
    }
}

void VulkanAccel::destroy(BLAS& blas)
{
    if (blas.as) VulkanAccel::vkDestroyAccelerationStructureKHR(RTX::g_ctx().device(), blas.as, nullptr);
    if (blas.buffer) vkDestroyBuffer(RTX::g_ctx().device(), blas.buffer, nullptr);
    if (blas.memory) vkFreeMemory(RTX::g_ctx().device(), blas.memory, nullptr);
    blas = {};
}

void VulkanAccel::destroy(TLAS& tlas)
{
    if (tlas.as) VulkanAccel::vkDestroyAccelerationStructureKHR(RTX::g_ctx().device(), tlas.as, nullptr);
    if (tlas.buffer) vkDestroyBuffer(RTX::g_ctx().device(), tlas.buffer, nullptr);
    if (tlas.memory) vkFreeMemory(RTX::g_ctx().device(), tlas.memory, nullptr);
    if (tlas.instanceBuffer) vkDestroyBuffer(RTX::g_ctx().device(), tlas.instanceBuffer, nullptr);
    if (tlas.instanceMemory) vkFreeMemory(RTX::g_ctx().device(), tlas.instanceMemory, nullptr);
    tlas = {};
}

VulkanAccel::BLAS VulkanAccel::createBLAS(const std::vector<AccelGeometry>& geometries,
                                          VkBuildAccelerationStructureFlagsKHR flags,
                                          VkCommandBuffer externalCmd,
                                          std::string_view name)
{
    BLAS blas{};
    blas.name = name;

    uint32_t primCount = 0;
    std::vector<VkAccelerationStructureGeometryKHR> geoms;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges;

    for (const auto& g : geometries) {
        primCount += g.indexCount / 3;

        VkAccelerationStructureGeometryKHR geo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
        geo.geometryType = g.type;
        geo.flags = g.flags;
        geo.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geo.geometry.triangles.vertexFormat = g.vertexFormat;
        geo.geometry.triangles.vertexStride = g.vertexStride;
        geo.geometry.triangles.maxVertex = g.vertexCount;
        geo.geometry.triangles.vertexData = g.vertexData;
        geo.geometry.triangles.indexType = g.indexType;
        geo.geometry.triangles.indexData = g.indexData;
        geo.geometry.triangles.transformData = g.transformData;

        geoms.push_back(geo);
        ranges.push_back({ g.indexCount / 3, 0, 0, 0 });
    }

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = flags | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = static_cast<uint32_t>(geoms.size());
    buildInfo.pGeometries = geoms.data();

    VkAccelerationStructureBuildSizesInfoKHR sizes = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    RTX::g_ctx().vkGetAccelerationStructureBuildSizesKHR()(RTX::g_ctx().device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &primCount, &sizes);

    uint64_t storage = 0ULL;
    BUFFER_CREATE(storage, sizes.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        std::string(name) + "_BLAS");

    VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    createInfo.buffer = RAW_BUFFER(storage);
    createInfo.size = sizes.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    VulkanAccel::vkCreateAccelerationStructureKHR(RTX::g_ctx().device(), &createInfo, nullptr, &blas.as);

    uint64_t scratch = 0ULL;
    BUFFER_CREATE(scratch, sizes.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        std::string(name) + "_scratch");

    VkBufferDeviceAddressInfo addrInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    addrInfo.buffer = RAW_BUFFER(scratch);
    VkDeviceAddress scratchAddr = RTX::g_ctx().vkGetBufferDeviceAddressKHR()(RTX::g_ctx().device(), &addrInfo);

    buildInfo.dstAccelerationStructure = blas.as;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkCommandBuffer cmd = externalCmd;
    if (cmd == VK_NULL_HANDLE) cmd = beginOneTime(RTX::g_ctx().commandPool());

    const VkAccelerationStructureBuildRangeInfoKHR* pRanges[] = { ranges.data() };
    RTX::g_ctx().vkCmdBuildAccelerationStructuresKHR()(cmd, 1, &buildInfo, pRanges);

    if (externalCmd == VK_NULL_HANDLE) endOneTime(cmd, RTX::g_ctx().graphicsQueue(), RTX::g_ctx().commandPool());

    VkAccelerationStructureDeviceAddressInfoKHR devAddrInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    devAddrInfo.accelerationStructure = blas.as;
    blas.address = RTX::g_ctx().vkGetAccelerationStructureDeviceAddressKHR()(RTX::g_ctx().device(), &devAddrInfo);

    blas.buffer = RAW_BUFFER(storage);
    blas.memory = BUFFER_MEMORY(storage);
    blas.size = sizes.accelerationStructureSize;

    BUFFER_DESTROY(scratch);

    LOG_SUCCESS("[VulkanAccel] BLAS \"%.*s\" FORGED — %.3f GB — ADDR 0x%llx",
                (int)name.size(), name.data(), blas.size / (1024.0*1024.0*1024.0), blas.address);

    return blas;
}

VulkanAccel::TLAS VulkanAccel::createTLAS(const std::vector<VkAccelerationStructureInstanceKHR>& instances,
                                          VkBuildAccelerationStructureFlagsKHR flags,
                                          VkCommandBuffer externalCmd,
                                          std::string_view name)
{
    TLAS tlas{};
    tlas.name = name;

    const uint32_t count = static_cast<uint32_t>(instances.size());
    const VkDeviceSize size = count * sizeof(VkAccelerationStructureInstanceKHR);

    uint64_t instBuf = 0ULL;
    BUFFER_CREATE(instBuf, size,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        std::string(name) + "_instances");

    void* mapped = nullptr;
    BUFFER_MAP(instBuf, mapped);
    memcpy(mapped, instances.data(), size);
    BUFFER_UNMAP(instBuf);

    VkBufferDeviceAddressInfo addrInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    addrInfo.buffer = RAW_BUFFER(instBuf);
    VkDeviceAddress instAddr = RTX::g_ctx().vkGetBufferDeviceAddressKHR()(RTX::g_ctx().device(), &addrInfo);

    VkAccelerationStructureGeometryKHR geom = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
    geom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geom.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geom.geometry.instances.data.deviceAddress = instAddr;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = flags;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geom;

    VkAccelerationStructureBuildSizesInfoKHR sizes = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR };
    RTX::g_ctx().vkGetAccelerationStructureBuildSizesKHR()(RTX::g_ctx().device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &count, &sizes);

    uint64_t storage = 0ULL;
    BUFFER_CREATE(storage, sizes.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        std::string(name) + "_TLAS");

    VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    createInfo.buffer = RAW_BUFFER(storage);
    createInfo.size = sizes.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    VulkanAccel::vkCreateAccelerationStructureKHR(RTX::g_ctx().device(), &createInfo, nullptr, &tlas.as);

    uint64_t scratch = 0ULL;
    BUFFER_CREATE(scratch, sizes.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        std::string(name) + "_scratch");

    addrInfo.buffer = RAW_BUFFER(scratch);
    buildInfo.scratchData.deviceAddress = RTX::g_ctx().vkGetBufferDeviceAddressKHR()(RTX::g_ctx().device(), &addrInfo);
    buildInfo.dstAccelerationStructure = tlas.as;

    VkCommandBuffer cmd = externalCmd;
    if (cmd == VK_NULL_HANDLE) cmd = beginOneTime(RTX::g_ctx().commandPool());

    VkAccelerationStructureBuildRangeInfoKHR range = { count };
    const VkAccelerationStructureBuildRangeInfoKHR* pRanges[] = { &range };
    RTX::g_ctx().vkCmdBuildAccelerationStructuresKHR()(cmd, 1, &buildInfo, pRanges);

    if (externalCmd == VK_NULL_HANDLE) endOneTime(cmd, RTX::g_ctx().graphicsQueue(), RTX::g_ctx().commandPool());

    VkAccelerationStructureDeviceAddressInfoKHR devAddrInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    devAddrInfo.accelerationStructure = tlas.as;
    tlas.address = RTX::g_ctx().vkGetAccelerationStructureDeviceAddressKHR()(RTX::g_ctx().device(), &devAddrInfo);

    tlas.buffer = RAW_BUFFER(storage);
    tlas.memory = BUFFER_MEMORY(storage);
    tlas.instanceBuffer = RAW_BUFFER(instBuf);
    tlas.instanceMemory = BUFFER_MEMORY(instBuf);
    tlas.size = sizes.accelerationStructureSize;

    BUFFER_DESTROY(scratch);

    LOG_SUCCESS("[VulkanAccel] TLAS \"%.*s\" ASCENDED — %u instances — %.3f GB — ADDR 0x%llx",
                (int)name.size(), name.data(), count, tlas.size / (1024.0*1024.0*1024.0), tlas.address);

    return tlas;
}