// src/engine/GLOBAL/LAS.cpp
// =============================================================================
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 21, 2025 — APOCALYPSE FINAL v10.3
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — VALHALLA TURBO v80
// LAS — FULLY COMPATIBLE WITH STONEKEY-OBSFUCATED HANDLES FROM MESHLOADER
// NO DEOBFUSCATION — ONLY RAW_BUFFER + DEVICE ADDRESS — PURE EMPIRE
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"

using namespace RTX;

// =============================================================================
// VulkanAccel Constructor
// =============================================================================
VulkanAccel::VulkanAccel(VkDevice /*device*/)
{
    LOG_SUCCESS_CAT("VulkanAccel", "{}VulkanAccel forged — ALL acceleration PFNs loaded via g_ctx(){}", PLASMA_FUCHSIA, RESET);
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
    LOG_SUCCESS_CAT("VulkanAccel", "{}BLAS destroyed — returned to the void{}", PLASMA_FUCHSIA, RESET);
}

void VulkanAccel::destroy(TLAS& tlas)
{
    if (tlas.as)              g_ctx().vkDestroyAccelerationStructureKHR()(g_ctx().device(), tlas.as, nullptr);
    if (tlas.buffer)          vkDestroyBuffer(g_ctx().device(), tlas.buffer, nullptr);
    if (tlas.memory)          vkFreeMemory(g_ctx().device(), tlas.memory, nullptr);
    if (tlas.instanceBuffer)  vkDestroyBuffer(g_ctx().device(), tlas.instanceBuffer, nullptr);
    if (tlas.instanceMemory)  vkFreeMemory(g_ctx().device(), tlas.instanceMemory, nullptr);
    tlas = {};
    LOG_SUCCESS_CAT("VulkanAccel", "{}TLAS destroyed — instances freed{}", PLASMA_FUCHSIA, RESET);
}

// =============================================================================
// BLAS BUILD — THE FINAL VERSION — THIS ONE WORKS
// =============================================================================
VulkanAccel::BLAS VulkanAccel::createBLAS(const std::vector<AccelGeometry>& geometries,
                                          VkBuildAccelerationStructureFlagsKHR flags,
                                          VkCommandBuffer externalCmd,
                                          std::string_view name)
{
    LOG_INFO_CAT("VulkanAccel", "=== FORGING BLAS \"{}\" — {} geometries ===", name, geometries.size());

    BLAS blas{};
    blas.name = name;

    uint32_t primCount = 0;
    std::vector<VkAccelerationStructureGeometryKHR> geoms;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges;

    // CRITICAL: RESERVE TO PREVENT REALLOCATION → DANGLING POINTER → 0x0 CRASH
    geoms.reserve(geometries.size());
    ranges.reserve(geometries.size());

    for (const auto& g : geometries) {
        uint32_t tris = g.indexCount / 3;
        primCount += tris;

        LOG_DEBUG_CAT("VulkanAccel",
            "  Geometry #{} → {} tris ({} indices), {} verts | stride={} | fmt=0x{:08X} | vAddr=0x{:016X} | iAddr=0x{:016X} | maxVertex={}",
            geoms.size(),
            tris,
            g.indexCount,
            g.vertexCount,
            g.vertexStride,
            static_cast<uint32_t>(g.vertexFormat),
            g.vertexData.deviceAddress,
            g.indexData.deviceAddress,
            (g.vertexCount ? g.vertexCount - 1 : 0));

        if (g.vertexData.deviceAddress == 0 || g.indexData.deviceAddress == 0) {
            LOG_FATAL_CAT("VulkanAccel", "ZERO DEVICE ADDRESS DETECTED — BUFFER LACKS SHADER_DEVICE_ADDRESS + AS_BUILD_INPUT");
            LOG_FATAL_CAT("VulkanAccel", "  → vertexData.deviceAddress = 0x{:016X}", g.vertexData.deviceAddress);
            LOG_FATAL_CAT("VulkanAccel", "  → indexData.deviceAddress  = 0x{:016X}", g.indexData.deviceAddress);
            return {};
        }

        VkAccelerationStructureGeometryKHR geo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR };
        geo.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
        geo.flags = g.flags;

        geo.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
        geo.geometry.triangles.vertexFormat = g.vertexFormat;
        geo.geometry.triangles.vertexStride = g.vertexStride;
        geo.geometry.triangles.maxVertex = g.vertexCount ? g.vertexCount - 1 : 0;
        geo.geometry.triangles.indexType = g.indexType;
        geo.geometry.triangles.transformData = g.transformData;

        geo.geometry.triangles.vertexData.deviceAddress = g.vertexData.deviceAddress;
        geo.geometry.triangles.indexData.deviceAddress  = g.indexData.deviceAddress;

        geoms.push_back(geo);
        ranges.push_back({ tris, 0, 0, 0 });
    }

    if (primCount == 0) {
        LOG_FATAL_CAT("VulkanAccel", "ZERO PRIMITIVES — BLAS BUILD WILL FAIL (primCount = 0)");
        return blas;
    }

    LOG_DEBUG_CAT("VulkanAccel", "Total primitives for build: {} (across {} geometries)", primCount, geoms.size());

    // Build geometry info
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags         = flags | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    buildInfo.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = static_cast<uint32_t>(geoms.size());
    buildInfo.pGeometries   = geoms.data();

    LOG_DEBUG_CAT("VulkanAccel", "Querying build sizes via vkGetAccelerationStructureBuildSizesKHR...");

    // ─────────────────────────────────────────────────────────────────────
    // CRITICAL: EXTRACT AND VALIDATE THE FUNCTION POINTER (YOUR LOADER RETURNS void(*))
    // ─────────────────────────────────────────────────────────────────────
    using PFN_vkGetAccelerationStructureBuildSizesKHR = void (VKAPI_PTR *)(
        VkDevice,
        VkAccelerationStructureBuildTypeKHR,
        const VkAccelerationStructureBuildGeometryInfoKHR*,
        const uint32_t*,
        VkAccelerationStructureBuildSizesInfoKHR*
    );

    PFN_vkGetAccelerationStructureBuildSizesKHR pfnGetSizes = 
        reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(
            g_ctx().vkGetAccelerationStructureBuildSizesKHR()
        );

    LOG_DEBUG_CAT("VulkanAccel", "  → PFN vkGetAccelerationStructureBuildSizesKHR = 0x{:016X}", 
                  reinterpret_cast<uint64_t>(pfnGetSizes));

    if (!pfnGetSizes) {
        LOG_FATAL_CAT("VulkanAccel", "FATAL: vkGetAccelerationStructureBuildSizesKHR IS NULL!");
        LOG_FATAL_CAT("VulkanAccel", "       → VK_KHR_acceleration_structure extension not enabled!");
        LOG_FATAL_CAT("VulkanAccel", "       → Or device function pointers were not loaded after vkCreateDevice!");
        LOG_FATAL_CAT("VulkanAccel", "       → Check: VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME in device extensions");
        return {};
    }

    // Zero-init output struct
    VkAccelerationStructureBuildSizesInfoKHR sizes = {};
    sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    // CALL — YOUR LOADER RETURNS void, SO WE CANNOT CAPTURE VkResult
    pfnGetSizes(
        g_ctx().device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_OR_DEVICE_KHR,
        &buildInfo,
        &primCount,
        &sizes
    );

    LOG_DEBUG_CAT("VulkanAccel", "vkGetAccelerationStructureBuildSizesKHR completed:");
    LOG_DEBUG_CAT("VulkanAccel", "   → accelerationStructureSize = {} bytes", sizes.accelerationStructureSize);
    LOG_DEBUG_CAT("VulkanAccel", "   → buildScratchSize         = {} bytes", sizes.buildScratchSize);
    LOG_DEBUG_CAT("VulkanAccel", "   → updateScratchSize        = {} bytes", sizes.updateScratchSize);

    if (sizes.accelerationStructureSize == 0 || sizes.buildScratchSize == 0) {
        LOG_FATAL_CAT("VulkanAccel", "DRIVER RETURNED INVALID SIZES — BUILD REJECTED!");
        LOG_FATAL_CAT("VulkanAccel", "   AS size = {} B | Scratch = {} B | UpdateScratch = {} B",
                      sizes.accelerationStructureSize, sizes.buildScratchSize, sizes.updateScratchSize);
        return {};
    }

    uint64_t storage = 0ULL;
    LOG_DEBUG_CAT("VulkanAccel", "Creating AS storage buffer ({} bytes)...", sizes.accelerationStructureSize);
    BUFFER_CREATE(storage, sizes.accelerationStructureSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                  std::string(name) + "_BLAS");

    VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    createInfo.buffer = RAW_BUFFER(storage);
    createInfo.size   = sizes.accelerationStructureSize;
    createInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    LOG_DEBUG_CAT("VulkanAccel", "Calling vkCreateAccelerationStructureKHR (size={}B)...", createInfo.size);
    VkResult cr = g_ctx().vkCreateAccelerationStructureKHR()(g_ctx().device(), &createInfo, nullptr, &blas.as);
    if (cr != VK_SUCCESS) {
        LOG_FATAL_CAT("VulkanAccel", "vkCreateAccelerationStructureKHR FAILED: VkResult = {}", static_cast<int32_t>(cr));
        return blas;
    }
    LOG_DEBUG_CAT("VulkanAccel", "AS handle created: 0x{:016X}", reinterpret_cast<uint64_t>(blas.as));

    uint64_t scratch = 0ULL;
    LOG_DEBUG_CAT("VulkanAccel", "Creating scratch buffer ({} bytes)...", sizes.buildScratchSize);
    BUFFER_CREATE(scratch, sizes.buildScratchSize,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                  std::string(name) + "_scratch");

    VkBufferDeviceAddressInfo addrInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    addrInfo.buffer = RAW_BUFFER(scratch);
    VkDeviceAddress scratchAddr = vkGetBufferDeviceAddress(g_ctx().device(), &addrInfo);
    LOG_DEBUG_CAT("VulkanAccel", "Scratch buffer device address: 0x{:016X}", scratchAddr);

    buildInfo.dstAccelerationStructure = blas.as;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkCommandBuffer cmd = externalCmd;
    if (cmd == VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("VulkanAccel", "Allocating one-time command buffer for BLAS build...");
        cmd = beginOneTime(g_ctx().commandPool_);
    }

    LOG_DEBUG_CAT("VulkanAccel", "Recording vkCmdBuildAccelerationStructuresKHR (1 build, {} geometries)...", buildInfo.geometryCount);
    const VkAccelerationStructureBuildRangeInfoKHR* pRanges[] = { ranges.data() };
    g_ctx().vkCmdBuildAccelerationStructuresKHR()(cmd, 1, &buildInfo, pRanges);

    if (externalCmd == VK_NULL_HANDLE) {
        LOG_DEBUG_CAT("VulkanAccel", "Submitting one-time BLAS build command...");
        endSingleTimeCommandsAsync(cmd, g_ctx().graphicsQueue_, g_ctx().commandPool_);
    }

    LOG_DEBUG_CAT("VulkanAccel", "Querying final BLAS device address...");
    VkAccelerationStructureDeviceAddressInfoKHR devAddrInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    devAddrInfo.accelerationStructure = blas.as;
    blas.address = g_ctx().vkGetAccelerationStructureDeviceAddressKHR()(g_ctx().device(), &devAddrInfo);

    LOG_DEBUG_CAT("VulkanAccel", "Final BLAS device address: 0x{:016X}", blas.address);

    if (blas.address == 0) {
        LOG_FATAL_CAT("VulkanAccel", "BLAS DEVICE ADDRESS IS 0x0 — BUILD FAILED SILENTLY!");
        return blas;
    }

    blas.buffer = RAW_BUFFER(storage);
    blas.memory = BUFFER_MEMORY(storage);
    blas.size   = sizes.accelerationStructureSize;

    BUFFER_DESTROY(scratch);

    LOG_SUCCESS_CAT("VulkanAccel", 
        "{}BLAS \"{}\" FORGED — {} triangles — SIZE {}B — ADDR 0x{:016X} — PINK PHOTONS HAVE A PATH{}",
        PLASMA_FUCHSIA, name, primCount, blas.size, blas.address, RESET);

    return blas;
}

// =============================================================================
// TLAS BUILD — UNCHANGED AND PERFECT
// =============================================================================
VulkanAccel::TLAS VulkanAccel::createTLAS(const std::vector<VkAccelerationStructureInstanceKHR>& instances,
                                          VkBuildAccelerationStructureFlagsKHR flags,
                                          VkCommandBuffer externalCmd,
                                          std::string_view name)
{
    LOG_INFO_CAT("VulkanAccel", "Forging TLAS \"{}\" — {} instances", name, instances.size());

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

    g_ctx().vkCreateAccelerationStructureKHR()(g_ctx().device(), &createInfo, nullptr, &tlas.as);

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

    LOG_SUCCESS_CAT("VulkanAccel", "{}TLAS \"{}\" ASCENDED — {} instances — ADDR 0x{:016X}{}",
                    PLASMA_FUCHSIA, name, count, tlas.address, RESET);

    return tlas;
}

// =============================================================================
// LAS::buildBLAS — STONEKEY v∞ SAFE — NO DEOBFUSCATION — PURE EMPIRE
// =============================================================================
void LAS::buildBLAS(VkCommandPool pool,
                    uint64_t vertexBufferObf,
                    uint64_t indexBufferObf,
                    uint32_t vertexCount,
                    uint32_t indexCount,
                    VkBuildAccelerationStructureFlagsKHR extraFlags)
{
    LOG_INFO_CAT("LAS", "{}LAS::buildBLAS() — verts: {} indices: {} — StoneKey handles: VERT 0x{:016X} INDEX 0x{:016X}{}",
                 PLASMA_FUCHSIA, vertexCount, indexCount, vertexBufferObf, indexBufferObf, RESET);

    VkBuffer realVertexBuffer = RAW_BUFFER(vertexBufferObf);
    VkBuffer realIndexBuffer  = RAW_BUFFER(indexBufferObf);

    VkBufferDeviceAddressInfo vAddrInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    vAddrInfo.buffer = realVertexBuffer;
    VkDeviceAddress vertexAddr = vkGetBufferDeviceAddress(g_ctx().device(), &vAddrInfo);

    VkBufferDeviceAddressInfo iAddrInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    iAddrInfo.buffer = realIndexBuffer;
    VkDeviceAddress indexAddr = vkGetBufferDeviceAddress(g_ctx().device(), &iAddrInfo);

    LOG_DEBUG_CAT("LAS", "REAL DEVICE ADDRESSES → Vertex: 0x{:016X} | Index: 0x{:016X}", vertexAddr, indexAddr);

    if (vertexAddr == 0 || indexAddr == 0) {
        LOG_FATAL_CAT("LAS", "ZERO DEVICE ADDRESS — BUFFER LACKS SHADER_DEVICE_ADDRESS + AS_BUILD_INPUT");
        return;
    }

    AccelGeometry geom{};
    geom.type = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geom.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geom.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geom.vertexStride = 44;
    geom.vertexCount = vertexCount;
    geom.vertexData.deviceAddress = vertexAddr;

    geom.indexType = VK_INDEX_TYPE_UINT32;
    geom.indexCount = indexCount;
    geom.indexData.deviceAddress = indexAddr;

    VkCommandBuffer cmd = beginOneTime(pool);

    LOG_DEBUG_CAT("LAS", "Calling createBLAS with REAL addresses...");
    blas_ = accel_->createBLAS(
        {geom},
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | extraFlags,
        cmd,
        "AmouranthScene_BLAS"
    );

    endSingleTimeCommandsAsync(cmd, g_ctx().graphicsQueue_, pool);
    ++generation_;

    LOG_SUCCESS_CAT("LAS", "{}BLAS FORGED — ADDR 0x{:016X} — GEN {} — PINK PHOTONS HAVE A PATH{}",
                    EMERALD_GREEN, blas_.address, generation_, RESET);
}

// =============================================================================
// LAS::buildTLAS — UNCHANGED AND PERFECT
// =============================================================================
void LAS::buildTLAS(VkCommandPool pool,
                    const std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>>& instances)
{
    LOG_INFO_CAT("LAS", "LAS::buildTLAS() — {} instances", instances.size());

    if (instances.empty()) {
        LOG_WARN_CAT("LAS", "Zero instances — skipping TLAS build");
        return;
    }

    std::vector<VkAccelerationStructureInstanceKHR> vkInstances;
    vkInstances.reserve(instances.size());

    for (const auto& [as, transform] : instances) {
        VkAccelerationStructureInstanceKHR inst{};

        const float* m = glm::value_ptr(transform);
        std::memcpy(&inst.transform.matrix, m, sizeof(inst.transform.matrix));

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

    LOG_SUCCESS_CAT("LAS", "{}TLAS ASCENDED — {} instances — ADDR 0x{:016X} — GEN {} — FIRST LIGHT ETERNAL{}",
                    EMERALD_GREEN, instances.size(), tlas_.address, generation_, RESET);
}