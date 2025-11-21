// src/engine/GLOBAL/LAS.cpp
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 20, 2025 — APOCALYPSE FINAL v2.0
// MAIN — FIRST LIGHT REBORN — LAS v2.0 VIA VulkanAccel — PINK PHOTONS ETERNAL
// =============================================================================
// AMOURANTH RTX ENGINE — GLOBAL LAS — APOCALYPSE FINAL v5.0 — NOVEMBER 20, 2025
// PINK PHOTONS ETERNAL — STONEKEY v∞ ACTIVE

#include "engine/GLOBAL/LAS.hpp"
#include "engine/Vulkan/VulkanCore.hpp"          // for beginSingleTimeCommands / endSingleTimeCommandsAsync
#include "engine/GLOBAL/RTXHandler.hpp"

using namespace RTX;

// =============================================================================
// VulkanAccel Constructor — CONSOLIDATED — NO STATIC PFNs — g_ctx() ONLY
// =============================================================================
VulkanAccel::VulkanAccel(VkDevice /*device*/)
{
    // All ray tracing PFNs are in g_ctx() via loadRayTracingExtensions()
    LOG_SUCCESS_CAT("VulkanAccel", "{}VulkanAccel forged — g_ctx() PFNs active{}", PLASMA_FUCHSIA, RESET);
}

// =============================================================================
// Destroy helpers — CONSOLIDATED
// =============================================================================
void VulkanAccel::destroy(BLAS& blas)
{
    if (blas.as) g_ctx().vkDestroyAccelerationStructureKHR()(g_ctx().device(), blas.as, nullptr);
    if (blas.buffer) vkDestroyBuffer(g_ctx().device(), blas.buffer, nullptr);
    if (blas.memory) vkFreeMemory(g_ctx().device(), blas.memory, nullptr);
    blas = {};
    LOG_SUCCESS_CAT("VulkanAccel", "{}BLAS destroyed — memory returned to void{}", PLASMA_FUCHSIA, RESET);
}

void VulkanAccel::destroy(TLAS& tlas)
{
    if (tlas.as) g_ctx().vkDestroyAccelerationStructureKHR()(g_ctx().device(), tlas.as, nullptr);
    if (tlas.buffer) vkDestroyBuffer(g_ctx().device(), tlas.buffer, nullptr);
    if (tlas.memory) vkFreeMemory(g_ctx().device(), tlas.memory, nullptr);
    if (tlas.instanceBuffer) vkDestroyBuffer(g_ctx().device(), tlas.instanceBuffer, nullptr);
    if (tlas.instanceMemory) vkFreeMemory(g_ctx().device(), tlas.instanceMemory, nullptr);
    tlas = {};
    LOG_SUCCESS_CAT("VulkanAccel", "{}TLAS destroyed — instances freed{}", PLASMA_FUCHSIA, RESET);
}

// =============================================================================
// BLAS BUILD — CONSOLIDATED — FULLY FIXED — 0x0 RESOLVED WITH PROPER sType/pNext
// =============================================================================
VulkanAccel::BLAS VulkanAccel::createBLAS(const std::vector<AccelGeometry>& geometries,
                                          VkBuildAccelerationStructureFlagsKHR flags,
                                          VkCommandBuffer externalCmd,
                                          std::string_view name)
{
    LOG_INFO_CAT("VulkanAccel", "Forging BLAS \"%.*s\" — {} geometries", (int)name.size(), name.data(), geometries.size());

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
    g_ctx().vkGetAccelerationStructureBuildSizesKHR()(
        g_ctx().device(),
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

    g_ctx().vkCreateAccelerationStructureKHR()(
        g_ctx().device(), &createInfo, nullptr, &blas.as);

    uint64_t scratch = 0ULL;
    BUFFER_CREATE(scratch, sizes.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        std::string(name) + "_scratch");

    VkBufferDeviceAddressInfo addrInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    addrInfo.buffer = RAW_BUFFER(scratch);
    VkDeviceAddress scratchAddr = vkGetBufferDeviceAddress(
        g_ctx().device(), &addrInfo);

    buildInfo.dstAccelerationStructure = blas.as;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkCommandBuffer cmd = externalCmd;
    if (cmd == VK_NULL_HANDLE) cmd = beginOneTime(g_ctx().commandPool_);

    const VkAccelerationStructureBuildRangeInfoKHR* pRanges[] = { ranges.data() };
    g_ctx().vkCmdBuildAccelerationStructuresKHR()(
        cmd, 1, &buildInfo, pRanges);

    if (externalCmd == VK_NULL_HANDLE)
        endSingleTimeCommandsAsync(cmd, g_ctx().graphicsQueue_, g_ctx().commandPool_);

    VkAccelerationStructureDeviceAddressInfoKHR devAddrInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    devAddrInfo.accelerationStructure = blas.as;
    blas.address = g_ctx().vkGetAccelerationStructureDeviceAddressKHR()(
        g_ctx().device(), &devAddrInfo);

    blas.buffer = RAW_BUFFER(storage);
    blas.memory = BUFFER_MEMORY(storage);
    blas.size = sizes.accelerationStructureSize;

    BUFFER_DESTROY(scratch);

    LOG_SUCCESS_CAT("VulkanAccel", "{}BLAS \"%.*s\" FORGED — {} prims — SIZE {}B — ADDR 0x{:016X}{}",
                    PLASMA_FUCHSIA, (int)name.size(), name.data(), primCount, blas.size, blas.address, RESET);

    return blas;
}

// =============================================================================
// TLAS CREATION — CONSOLIDATED — FULLY FIXED
// =============================================================================
VulkanAccel::TLAS VulkanAccel::createTLAS(const std::vector<VkAccelerationStructureInstanceKHR>& instances,
                                          VkBuildAccelerationStructureFlagsKHR flags,
                                          VkCommandBuffer externalCmd,
                                          std::string_view name)
{
    LOG_INFO_CAT("VulkanAccel", "Forging TLAS \"%.*s\" — {} instances", (int)name.size(), name.data(), instances.size());

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
    std::memcpy(mapped, instances.data(), size);
    BUFFER_UNMAP(instBuf);

    VkBufferDeviceAddressInfo addrInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    addrInfo.buffer = RAW_BUFFER(instBuf);
    VkDeviceAddress instAddr = vkGetBufferDeviceAddress(g_ctx().device(), &addrInfo);

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
    g_ctx().vkGetAccelerationStructureBuildSizesKHR()(g_ctx().device(),
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

    g_ctx().vkCreateAccelerationStructureKHR()(
        g_ctx().device(), &createInfo, nullptr, &tlas.as);

    uint64_t scratch = 0ULL;
    BUFFER_CREATE(scratch, sizes.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        std::string(name) + "_scratch");

    addrInfo.buffer = RAW_BUFFER(scratch);
    buildInfo.scratchData.deviceAddress = vkGetBufferDeviceAddress(g_ctx().device(), &addrInfo);
    buildInfo.dstAccelerationStructure = tlas.as;

    VkCommandBuffer cmd = externalCmd;
    if (cmd == VK_NULL_HANDLE) cmd = beginOneTime(g_ctx().commandPool_);

    VkAccelerationStructureBuildRangeInfoKHR range = { count };
    const VkAccelerationStructureBuildRangeInfoKHR* pRanges[] = { &range };
    g_ctx().vkCmdBuildAccelerationStructuresKHR()(cmd, 1, &buildInfo, pRanges);

    if (externalCmd == VK_NULL_HANDLE)
        endSingleTimeCommandsAsync(cmd, g_ctx().graphicsQueue_, g_ctx().commandPool_);

    VkAccelerationStructureDeviceAddressInfoKHR devAddrInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    devAddrInfo.accelerationStructure = tlas.as;
    tlas.address = g_ctx().vkGetAccelerationStructureDeviceAddressKHR()(g_ctx().device(), &devAddrInfo);

    tlas.buffer = RAW_BUFFER(storage);
    tlas.memory = BUFFER_MEMORY(storage);
    tlas.instanceBuffer = RAW_BUFFER(instBuf);
    tlas.instanceMemory = BUFFER_MEMORY(instBuf);
    tlas.size = sizes.accelerationStructureSize;

    BUFFER_DESTROY(scratch);

    LOG_SUCCESS_CAT("VulkanAccel", "{}TLAS \"%.*s\" ASCENDED — {} instances — SIZE {}B — ADDR 0x{:016X}{}",
                    PLASMA_FUCHSIA, (int)name.size(), name.data(), count, tlas.size, tlas.address, RESET);

    return tlas;
}

// =============================================================================
// LAS BLAS BUILD — CONSOLIDATED — 0x0 RESOLVED FOREVER — NOVEMBER 20, 2025
// =============================================================================
void LAS::buildBLAS(VkCommandPool pool,
                    VkBuffer vertexBuffer,
                    VkBuffer indexBuffer,
                    uint32_t vertexCount,
                    uint32_t indexCount,
                    VkBuildAccelerationStructureFlagsKHR extraFlags)
{
    LOG_INFO_CAT("LAS", "Forging BLAS — verts: {}, indices: {}", vertexCount, indexCount);

    // Lazy-create VulkanAccel
    if (!accel_) {
        accel_ = std::make_unique<VulkanAccel>(RTX::g_ctx().device());
    }

    AccelGeometry geom{};
    geom.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geom.vertexStride = sizeof(glm::vec3);
    geom.vertexCount  = vertexCount;

    // THE FINAL 0x0 FIX — NOVEMBER 20, 2025 — FULLY INITIALIZED
    VkBufferDeviceAddressInfo vAddrInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    vAddrInfo.buffer = vertexBuffer;
    geom.vertexData.deviceAddress = vkGetBufferDeviceAddress(g_ctx().device(), &vAddrInfo);
    LOG_DEBUG_CAT("LAS", "Vertex address: 0x{:016X}", geom.vertexData.deviceAddress);

    geom.indexType  = VK_INDEX_TYPE_UINT32;
    geom.indexCount = indexCount;

    VkBufferDeviceAddressInfo iAddrInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    iAddrInfo.buffer = indexBuffer;
    geom.indexData.deviceAddress = vkGetBufferDeviceAddress(g_ctx().device(), &iAddrInfo);
    LOG_DEBUG_CAT("LAS", "Index address: 0x{:016X}", geom.indexData.deviceAddress);

    VkCommandBuffer cmd = beginOneTime(pool);

    blas_ = accel_->createBLAS(
        {geom},
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | extraFlags,
        cmd,
        "AmouranthCube_BLAS"
    );

    endSingleTimeCommandsAsync(cmd, g_ctx().graphicsQueue_, pool);
    ++generation_;

    LOG_SUCCESS_CAT("LAS", "{}BLAS FORGED — {} verts, {} tris — ADDR 0x{:016X} — GENERATION {}{}",
                    PLASMA_FUCHSIA, vertexCount, indexCount / 3, blas_.address, generation_, RESET);
}

// =============================================================================
// LAS TLAS BUILD — CONSOLIDATED — FULLY FIXED
// =============================================================================
void LAS::buildTLAS(VkCommandPool pool,
                    const std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>>& instances)
{
    LOG_INFO_CAT("LAS", "Forging TLAS — {} instances", instances.size());

    if (instances.empty()) {
        LOG_WARN_CAT("LAS", "TLAS build with zero instances — skipping");
        return;
    }

    if (!accel_) {
        accel_ = std::make_unique<VulkanAccel>(RTX::g_ctx().device());
    }

    std::vector<VkAccelerationStructureInstanceKHR> vkInstances;
    vkInstances.reserve(instances.size());

    for (const auto& [as, transform] : instances) {
        VkAccelerationStructureInstanceKHR inst{};

        const float* m = glm::value_ptr(transform);
        inst.transform.matrix[0][0] = m[0]; inst.transform.matrix[0][1] = m[1]; inst.transform.matrix[0][2] = m[2]; inst.transform.matrix[0][3] = m[3];
        inst.transform.matrix[1][0] = m[4]; inst.transform.matrix[1][1] = m[5]; inst.transform.matrix[1][2] = m[6]; inst.transform.matrix[1][3] = m[7];
        inst.transform.matrix[2][0] = m[8]; inst.transform.matrix[2][1] = m[9]; inst.transform.matrix[2][2] = m[10]; inst.transform.matrix[2][3] = m[11];

        inst.instanceCustomIndex                    = 0;
        inst.mask                                   = 0xFF;
        inst.instanceShaderBindingTableRecordOffset = 0;
        inst.flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

        VkAccelerationStructureDeviceAddressInfoKHR addrInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
        addrInfo.accelerationStructure = as;
        inst.accelerationStructureReference = g_ctx().vkGetAccelerationStructureDeviceAddressKHR()(g_ctx().device(), &addrInfo);

        vkInstances.push_back(inst);
    }

    VkCommandBuffer cmd = beginOneTime(pool);

    tlas_ = accel_->createTLAS(
        vkInstances,
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        cmd,
        "AmouranthScene_TLAS"
    );

    endSingleTimeCommandsAsync(cmd, g_ctx().graphicsQueue_, pool);
    ++generation_;

    LOG_SUCCESS_CAT("LAS", "{}TLAS ASCENDED — {} instances — ADDR 0x{:016X} — GENERATION {}{}",
                    PLASMA_FUCHSIA, instances.size(), tlas_.address, generation_, RESET);
}