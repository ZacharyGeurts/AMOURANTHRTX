// src/engine/GLOBAL/LAS.cpp
// =============================================================================
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 22, 2025 — FIRST LIGHT ETERNAL
// LAS — THE GREATEST SHOW ON EARTH — HOSTED BY THE ENTIRE CAST
//
// Tonight's special guests:
// Drew Carey — Host with the most
// Ellie Fier — The Pink Photon Queen herself
// Amouranth — The reason the photons are pink
// Gentleman Grok — Sophisticated British narrator
// Professor Grok — Explains the science in real time
// Colin Mochrie — Improvisational genius
// Ryan Stiles — Physical comedy legend
// Wayne Brady — Can do anything, does everything
// Chip Esten — The musical genius
// Greg Proops — Sarcastic commentator
// Brad Sherwood — The other musical genius
// Jeff Davis — The chaos agent
//
// "Welcome to Whose Line Is It Anyway? The show where everything's made up
//  and the points don't matter — but the ray tracing DOES!"
//
// WE LOVE YOU ALL.
//
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/VulkanCore.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"

using namespace RTX;

// =============================================================================
// VulkanAccel Constructor — The gang arrives
// =============================================================================
VulkanAccel::VulkanAccel(VkDevice /*device*/)
{
    LOG_SUCCESS_CAT("VulkanAccel", 
        "{}Drew Carey: \"Welcome to the VulkanAccel Show!\"{}\n"
        "   {}Ellie Fier: \"Pink photons incoming, baby!\"{}\n"
        "   {}Amouranth: \"Finally, something worthy of my radiance~\"{}\n"
        "   {}Gentleman Grok: \"All acceleration function pointers have been acquired with utmost decorum.\"{}\n"
        "   {}Professor Grok: \"The KHR_acceleration_structure extension is present and accounted for. Science prevails.\"{}",
        PLASMA_FUCHSIA, RESET,
        AURORA_PINK, RESET,
        RASPBERRY_PINK, RESET,
        VALHALLA_GOLD, RESET,
        EMERALD_GREEN, RESET);
}

// =============================================================================
// Destroy helpers — The dramatic exits
// =============================================================================
void VulkanAccel::destroy(BLAS& blas)
{
    if (blas.as)    g_ctx().vkDestroyAccelerationStructureKHR()(g_ctx().device(), blas.as, nullptr);
    if (blas.buffer) vkDestroyBuffer(g_ctx().device(), blas.buffer, nullptr);
    if (blas.memory) vkFreeMemory(g_ctx().device(), blas.memory, nullptr);
    blas = {};

    LOG_SUCCESS_CAT("VulkanAccel", 
        "{}Drew Carey: \"Thanks for playing, BLAS! You've been a great contestant!\"{}\n"
        "   {}Ryan Stiles: *dramatic slow-motion fall into the void*\"{}\n"
        "   {}Wayne Brady: \"And the BLAS is GONE! Just like that! *snaps*\"{}\n"
        "   {}Greg Proops: \"Back to the nothingness from whence it came. How poetic.\"{}",
        PLASMA_FUCHSIA, RESET,
        CRIMSON_MAGENTA, RESET,
        PARTY_PINK, RESET,
        OCEAN_TEAL, RESET);
}

void VulkanAccel::destroy(TLAS& tlas)
{
    if (tlas.as)              g_ctx().vkDestroyAccelerationStructureKHR()(g_ctx().device(), tlas.as, nullptr);
    if (tlas.buffer)          vkDestroyBuffer(g_ctx().device(), tlas.buffer, nullptr);
    if (tlas.memory)          vkFreeMemory(g_ctx().device(), tlas.memory, nullptr);
    if (tlas.instanceBuffer)  vkDestroyBuffer(g_ctx().device(), tlas.instanceBuffer, nullptr);
    if (tlas.instanceMemory)  vkFreeMemory(g_ctx().device(), tlas.instanceMemory, nullptr);
    tlas = {};

    LOG_SUCCESS_CAT("VulkanAccel", 
        "{}Drew Carey: \"The TLAS has left the building!\"{}\n"
        "   {}Colin Mochrie: *stands perfectly still as everything disappears around him*\"{}\n"
        "   {}Amouranth: \"All my beautiful instances... gone... but I'll make more~\"{}\n"
        "   {}Professor Grok: \"Top-level hierarchy successfully deallocated. Memory pressure relieved.\"{}",
        PLASMA_FUCHSIA, RESET,
        VALHALLA_GOLD, RESET,
        RASPBERRY_PINK, RESET,
        EMERALD_GREEN, RESET);
}

// =============================================================================
// BLAS BUILD — THE MAIN EVENT
// =============================================================================
VulkanAccel::BLAS VulkanAccel::createBLAS(const std::vector<AccelGeometry>& geometries,
                                          VkBuildAccelerationStructureFlagsKHR flags,
                                          VkCommandBuffer externalCmd,
                                          std::string_view name)
{
    LOG_ATTEMPT_CAT("VulkanAccel", 
        "{}Drew Carey: \"It's time for 'Forge the BLAS'! Contestant {} — come on down!\"{}\n"
        "   {}Ellie Fier: \"This is gonna be SO pink and SO fast!\"{}\n"
        "   {}Gentleman Grok: \"We are about to construct a bottom-level acceleration structure of exquisite precision.\"{}",
        VALHALLA_GOLD, name, RESET,
        AURORA_PINK, RESET,
        PARTY_PINK, RESET);

    LOG_INFO_CAT("VulkanAccel", "=== FORGING BLAS \"{}\" — {} geometries === \"Whose photons are these? THEY'RE OURS NOW!\"{}", 
                 name, geometries.size(), RESET);

    BLAS blas{};
    blas.name = name;

    uint32_t primCount = 0;
    std::vector<VkAccelerationStructureGeometryKHR> geoms;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges;
    geoms.reserve(geometries.size());
    ranges.reserve(geometries.size());

    for (const auto& g : geometries) {
        uint32_t tris = g.indexCount / 3;
        primCount += tris;

        LOG_DEBUG_CAT("VulkanAccel",
            "  {}Wayne Brady: \"Geometry #{} — {} triangles, {} verts — I can do this in my sleep!\"{}\n"
            "    vAddr=0x{:016X} | iAddr=0x{:016X} | stride={}",
            PARTY_PINK, geoms.size(), tris, g.vertexCount, RESET,
            g.vertexData.deviceAddress, g.indexData.deviceAddress, g.vertexStride);

        if (g.vertexData.deviceAddress == 0 || g.indexData.deviceAddress == 0) {
            LOG_FATAL_CAT("VulkanAccel", 
                "{}Greg Proops: \"Oh look, the addresses are zero. How utterly predictable.\"{}\n"
                "   {}Ryan Stiles: *pretends to look for the missing addresses with a magnifying glass*\"{}\n"
                "   {}Professor Grok: \"Device addresses invalid. This build cannot proceed. Science demands correctness.\"{}",
                CRIMSON_MAGENTA, RESET,
                OCEAN_TEAL, RESET,
                BLOOD_RED, RESET);
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
        LOG_FATAL_CAT("VulkanAccel", 
            "{}Drew Carey: \"The survey says... NOTHING! Zero primitives!\"{}\n"
            "   {}Chip Esten: *sings* \"We built a BLAS with no triangles... what a sad sad song~\"{}",
            BLOOD_RED, RESET,
            RASPBERRY_PINK, RESET);
        return blas;
    }

    LOG_DEBUG_CAT("VulkanAccel", "{}Professor Grok: \"Total primitive count: {} across {} geometries. Acceptable parameters.\"{}", 
                  EMERALD_GREEN, primCount, geoms.size(), RESET);

    // Build info setup
    VkAccelerationStructureBuildGeometryInfoKHR buildInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR };
    buildInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags         = flags | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    buildInfo.mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.geometryCount = static_cast<uint32_t>(geoms.size());
    buildInfo.pGeometries   = geoms.data();

    LOG_DEBUG_CAT("VulkanAccel", "{}Gentleman Grok: \"Commencing size query via vkGetAccelerationStructureBuildSizesKHR...\"{}", 
                  VALHALLA_GOLD, RESET);

    using PFN_vkGetAccelerationStructureBuildSizesKHR = void (VKAPI_PTR *)(
        VkDevice, VkAccelerationStructureBuildTypeKHR,
        const VkAccelerationStructureBuildGeometryInfoKHR*, const uint32_t*,
        VkAccelerationStructureBuildSizesInfoKHR*);

    PFN_vkGetAccelerationStructureBuildSizesKHR pfnGetSizes = 
        reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(g_ctx().vkGetAccelerationStructureBuildSizesKHR());

    if (!pfnGetSizes) {
        LOG_FATAL_CAT("VulkanAccel", 
            "{}Drew Carey: \"The price is WRONG! PFN is NULL!\"{}\n"
            "   {}Jeff Davis: *makes explosion sounds* \"KABOOM! Extension not enabled!\"{}\n"
            "   {}Professor Grok: \"Critical failure: VK_KHR_acceleration_structure not present. Aborting.\"{}",
            BLOOD_RED, RESET,
            CRIMSON_MAGENTA, RESET,
            BLOOD_RED, RESET);
        return {};
    }

    VkAccelerationStructureBuildSizesInfoKHR sizes = {};
    sizes.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    pfnGetSizes(g_ctx().device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_OR_DEVICE_KHR,
                &buildInfo, &primCount, &sizes);

    LOG_DEBUG_CAT("VulkanAccel", 
        "{}Professor Grok: \"Build sizes acquired — AS: {} B | Scratch: {} B | Update: {} B\"{}",
        EMERALD_GREEN, sizes.accelerationStructureSize, sizes.buildScratchSize, sizes.updateScratchSize, RESET);

    if (sizes.accelerationStructureSize == 0 || sizes.buildScratchSize == 0) {
        LOG_FATAL_CAT("VulkanAccel", 
            "{}Greg Proops: \"Driver said 'no thanks'. How rude.\"{}\n"
            "   {}Colin Mochrie: *stares blankly at camera for 10 seconds*\"{}",
            CRIMSON_MAGENTA, RESET,
            OCEAN_TEAL, RESET);
        return {};
    }

    uint64_t storage = 0ULL;
    LOG_DEBUG_CAT("VulkanAccel", "{}Wayne Brady: \"Creating storage buffer... and... BOOM! {} bytes!\"{}", 
                  PARTY_PINK, sizes.accelerationStructureSize, RESET);

    BUFFER_CREATE(storage, sizes.accelerationStructureSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                  std::string(name) + "_BLAS");

    VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    createInfo.buffer = RAW_BUFFER(storage);
    createInfo.size   = sizes.accelerationStructureSize;
    createInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;

    VkResult cr = g_ctx().vkCreateAccelerationStructureKHR()(g_ctx().device(), &createInfo, nullptr, &blas.as);
    if (cr != VK_SUCCESS) {
        LOG_FATAL_CAT("VulkanAccel", 
            "{}Drew Carey: \"Survey says... FAILURE! Create returned {}\"{}\n"
            "   {}Brad Sherwood: *sings opera* \"FAAAAAILED!\"{}",
            BLOOD_RED, static_cast<int32_t>(cr), RESET,
            CRIMSON_MAGENTA, RESET);
        return blas;
    }

    uint64_t scratch = 0ULL;
    BUFFER_CREATE(scratch, sizes.buildScratchSize,
                  VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                  std::string(name) + "_scratch");

    VkBufferDeviceAddressInfo addrInfo = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
    addrInfo.buffer = RAW_BUFFER(scratch);
    VkDeviceAddress scratchAddr = vkGetBufferDeviceAddress(g_ctx().device(), &addrInfo);

    buildInfo.dstAccelerationStructure = blas.as;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkCommandBuffer cmd = externalCmd;
    if (cmd == VK_NULL_HANDLE) {
        cmd = beginOneTime(g_ctx().commandPool_);
    }

    const VkAccelerationStructureBuildRangeInfoKHR* pRanges[] = { ranges.data() };
    g_ctx().vkCmdBuildAccelerationStructuresKHR()(cmd, 1, &buildInfo, pRanges);

    if (externalCmd == VK_NULL_HANDLE) {
        endSingleTimeCommandsAsync(cmd, g_ctx().graphicsQueue_, g_ctx().commandPool_);
    }

    VkAccelerationStructureDeviceAddressInfoKHR devAddrInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR };
    devAddrInfo.accelerationStructure = blas.as;
    blas.address = g_ctx().vkGetAccelerationStructureDeviceAddressKHR()(g_ctx().device(), &devAddrInfo);

    if (blas.address == 0) {
        LOG_FATAL_CAT("VulkanAccel", 
            "{}Ryan Stiles: *points at screen* \"It's zero! It's actually zero!\"{}\n"
            "   {}Ellie Fier: \"This is NOT pink photon behavior!\"{}",
            CRIMSON_MAGENTA, RESET,
            AURORA_PINK, RESET);
        return blas;
    }

    blas.buffer = RAW_BUFFER(storage);
    blas.memory = BUFFER_MEMORY(storage);
    blas.size   = sizes.accelerationStructureSize;
    BUFFER_DESTROY(scratch);

    LOG_SUCCESS_CAT("VulkanAccel", 
        "{}Drew Carey: \"THE PRICE IS RIGHT! BLAS \"{}\" HAS BEEN FORGED!\"{}\n"
        "   {}Amouranth: \"{} triangles of pure perfection~\"{}\n"
        "   {}Ellie Fier: \"PINK PHOTONS HAVE A PATH! ADDR 0x{:016X}!\"{}\n"
        "   {}Wayne Brady: \"And the crowd goes wild! *makes crowd noise*\"{}\n"
        "   {}Gentleman Grok: \"A magnificent specimen. Well done, old chap.\"{}",
        EMERALD_GREEN, name, RESET,
        RASPBERRY_PINK, primCount, RESET,
        AURORA_PINK, blas.address, RESET,
        PARTY_PINK, RESET,
        VALHALLA_GOLD, RESET);

    return blas;
}

// =============================================================================
// TLAS BUILD — THE GRAND FINALE — FULLY CASTED, NO CUTS, NO MERCY
// =============================================================================
VulkanAccel::TLAS VulkanAccel::createTLAS(const std::vector<VkAccelerationStructureInstanceKHR>& instances,
                                          VkBuildAccelerationStructureFlagsKHR flags,
                                          VkCommandBuffer externalCmd,
                                          std::string_view name)
{
    LOG_ATTEMPT_CAT("VulkanAccel", 
        "{}Drew Carey: \"It's the Showcase Showdown — TIME FOR THE TLAS!\"{}\n"
        "   {}Amouranth: \"All my beautiful instances... coming together~\"{}\n"
        "   {}Ellie Fier: \"This is the moment the pink photons have been waiting for!\"{}\n"
        "   {}Gentleman Grok: \"We stand on the precipice of hierarchical perfection.\"{}",
        VALHALLA_GOLD, RESET,
        RASPBERRY_PINK, RESET,
        AURORA_PINK, RESET,
        PARTY_PINK, RESET);

    LOG_INFO_CAT("VulkanAccel", "Forging TLAS \"{}\" — {} instances — the empire's crown jewel", name, instances.size());

    TLAS tlas{};
    tlas.name = name;

    const uint32_t count = static_cast<uint32_t>(instances.size());
    const VkDeviceSize size = count * sizeof(VkAccelerationStructureInstanceKHR);

    if (count == 0) {
        LOG_WARN_CAT("VulkanAccel", 
            "{}Drew Carey: \"The survey says... nothing! Zero instances!\"{}\n"
            "   {}Colin Mochrie: *stares directly into camera, unmoving*\"{}\n"
            "   {}Greg Proops: \"Well that's anticlimactic.\"{}",
            OCEAN_TEAL, RESET,
            VALHALLA_GOLD, RESET,
            CRIMSON_MAGENTA, RESET);
        return tlas;
    }

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

    LOG_DEBUG_CAT("VulkanAccel", "{}Wayne Brady: \"Instances uploaded — {} of them — and just like that... MAGIC!\"{}", 
                  PARTY_PINK, count, RESET);

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

    LOG_DEBUG_CAT("VulkanAccel", 
        "{}Professor Grok: \"TLAS requires {} bytes storage + {} bytes scratch. Optimal.\"{}",
        EMERALD_GREEN, sizes.accelerationStructureSize, sizes.buildScratchSize, RESET);

    uint64_t storage = 0ULL;
    BUFFER_CREATE(storage, sizes.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        std::string(name) + "_TLAS");

    VkAccelerationStructureCreateInfoKHR createInfo = { VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR };
    createInfo.buffer = RAW_BUFFER(storage);
    createInfo.size   = sizes.accelerationStructureSize;
    createInfo.type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;

    VkResult cr = g_ctx().vkCreateAccelerationStructureKHR()(g_ctx().device(), &createInfo, nullptr, &tlas.as);
    if (cr != VK_SUCCESS) {
        LOG_FATAL_CAT("VulkanAccel", 
            "{}Drew Carey: \"The price is WRONG! Create failed with {}\"{}",
            BLOOD_RED, static_cast<int32_t>(cr), RESET);
        return tlas;
    }

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

    LOG_SUCCESS_CAT("VulkanAccel", 
        "{}Drew Carey: \"YOU'VE WON THE SHOWCASE! TLAS \"{}\" ASCENDED!\"{}\n"
        "   {}Ellie Fier: \"{} instances of pure pink photon glory!\"{}\n"
        "   {}Amouranth: \"Look at them all... so organized~\"{}\n"
        "   {}Professor Grok: \"Top-level hierarchy complete. Ray traversal optimized to perfection.\"{}\n"
        "   {}Colin Mochrie: \"I've been standing here the whole time.\"{}\n"
        "   {}Chip Esten: *sings* \"First light eternal, photons so bright, TLAS is built and it feels just right~\"{}\n"
        "   {}Wayne Brady: \"And the crowd goes ABSOLUTELY WILD! *crowd noise intensifies*\"{}\n"
        "   {}Gentleman Grok: \"A masterpiece. Simply splendid.\"{}\n"
        "   {}Greg Proops: \"Finally, something that actually matters.\"{}",
        EMERALD_GREEN, name, RESET,
        AURORA_PINK, count, RESET,
        RASPBERRY_PINK, RESET,
        VALHALLA_GOLD, RESET,
        OCEAN_TEAL, RESET,
        PARTY_PINK, RESET,
        CRIMSON_MAGENTA, RESET,
        VALHALLA_GOLD, RESET,
        OCEAN_TEAL, RESET);

    return tlas;
}

// =============================================================================
// LAS::buildBLAS — The global call — FULL CAST PERFORMANCE
// =============================================================================
void LAS::buildBLAS(VkCommandPool pool,
                    uint64_t vertexBufferObf,
                    uint64_t indexBufferObf,
                    uint32_t vertexCount,
                    uint32_t indexCount,
                    VkBuildAccelerationStructureFlagsKHR extraFlags)
{
    LOG_ATTEMPT_CAT("LAS", 
        "{}Drew Carey: \"Contestant LAS, come on down! You're the next builder on The Price is Pink!\"{}\n"
        "   {}Ellie Fier: \"It's BLAS time, baby! Let's go!\"{}\n"
        "   {}Ryan Stiles: *already doing something weird in the background*\"{}",
        RASPBERRY_PINK, RESET,
        AURORA_PINK, RESET,
        CRIMSON_MAGENTA, RESET);

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

    LOG_DEBUG_CAT("LAS", "{}Professor Grok: \"Device addresses resolved — Vertex: 0x{:016X} | Index: 0x{:016X}\"{}", 
                  EMERALD_GREEN, vertexAddr, indexAddr, RESET);

    if (vertexAddr == 0 || indexAddr == 0) {
        LOG_FATAL_CAT("LAS", 
            "{}Greg Proops: \"Zero addresses. How original.\"{}\n"
            "   {}Jeff Davis: *makes dying robot noises*\"{}",
            CRIMSON_MAGENTA, RESET,
            BLOOD_RED, RESET);
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

    LOG_DEBUG_CAT("LAS", "{}Wayne Brady: \"And now... the moment you've all been waiting for...\"{}", PARTY_PINK, RESET);
    blas_ = accel_->createBLAS(
        {geom},
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | extraFlags,
        cmd,
        "AmouranthScene_BLAS"
    );

    endSingleTimeCommandsAsync(cmd, g_ctx().graphicsQueue_, pool);
    ++generation_;

    LOG_SUCCESS_CAT("LAS", 
        "{}Drew Carey: \"THE PRICE IS RIGHT! GLOBAL BLAS FORGED!\"{}\n"
        "   {}Amouranth: \"My scene looks so good right now~\"{}\n"
        "   {}Wayne Brady: \"Generation {} and the points don't matter!\"{}\n"
        "   {}Ellie Fier: \"PINK PHOTONS HAVE THEIR PATH — FIRST LIGHT ETERNAL!\"{}\n"
        "   {}Chip Esten: *sings* \"We built a BLAS so fine, with {} triangles in a line~\"{}\n"
        "   {}Gentleman Grok: \"Splendid work, old bean.\"{}",
        EMERALD_GREEN, RESET,
        RASPBERRY_PINK, RESET,
        PARTY_PINK, generation_, RESET,
        AURORA_PINK, RESET,
        VALHALLA_GOLD, indexCount/3, RESET,
        VALHALLA_GOLD, RESET);
}

// =============================================================================
// LAS::buildTLAS — The final ascension — THE FULL CAST FINALE
// =============================================================================
void LAS::buildTLAS(VkCommandPool pool,
                    const std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>>& instances)
{
    LOG_ATTEMPT_CAT("LAS", 
        "{}Drew Carey: \"It's time for the Big Wheel — GLOBAL TLAS ASCENSION!\"{}\n"
        "   {}Greg Proops: \"Finally, something actually important.\"{}\n"
        "   {}Ellie Fier: \"This is it. This is the moment.\"{}",
        VALHALLA_GOLD, RESET,
        OCEAN_TEAL, RESET,
        AURORA_PINK, RESET);

    LOG_INFO_CAT("LAS", "LAS::buildTLAS() — {} instances — preparing the empire's crown", instances.size());

    if (instances.empty()) {
        LOG_WARN_CAT("LAS", 
            "{}Drew Carey: \"Zero instances? The survey says... SKIP!\"{}\n"
            "   {}Colin Mochrie: *stands perfectly still*\"{}",
            OCEAN_TEAL, RESET,
            VALHALLA_GOLD, RESET);
        return;
    }

    std::vector<VkAccelerationStructureInstanceKHR> vkInstances;
    vkInstances.reserve(instances.size());

    LOG_DEBUG_CAT("LAS", "{}Ryan Stiles: \"{} instances? That's a lot of weird poses...\"{}", 
                  CRIMSON_MAGENTA, instances.size(), RESET);

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

    LOG_DEBUG_CAT("LAS", "{}Wayne Brady: \"And now... the grand finale...\"{}", PARTY_PINK, RESET);

    tlas_ = accel_->createTLAS(
        vkInstances,
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        cmd,
        "AmouranthScene_TLAS"
    );

    endSingleTimeCommandsAsync(cmd, g_ctx().graphicsQueue_, pool);
    ++generation_;

    LOG_SUCCESS_CAT("LAS", 
        "{}Drew Carey: \"YOU WIN THE ENTIRE SHOWCASE!\"{}\n"
        "   {}Ellie Fier: \"{} INSTANCES — PINK PHOTONS EVERYWHERE!\"{}\n"
        "   {}Amouranth: \"The empire is complete~\"{}\n"
        "   {}Gentleman Grok: \"A triumph of engineering and elegance.\"{}\n"
        "   {}Professor Grok: \"Ray tracing performance will be... exquisite.\"{}\n"
        "   {}Wayne Brady: \"Can I get a Hallelujah?!\"{}\n"
        "   {}Chip Esten: *sings epic finale* \"From the BLAS to the TLAS, we built it all with class~\"{}\n"
        "   {}Colin Mochrie: \"I was here the whole time.\"{}\n"
        "   {}Greg Proops: \"And somehow, it worked.\"{}\n"
        "   {}Ryan Stiles: *does victory dance*\"{}\n"
        "   {}Jeff Davis: *makes explosion of confetti*\"{}\n"
        "   {}The entire cast in unison: \"FIRST LIGHT ETERNAL — NOVEMBER 22, 2025!\"{}",
        EMERALD_GREEN, RESET,
        AURORA_PINK, instances.size(), RESET,
        RASPBERRY_PINK, RESET,
        VALHALLA_GOLD, RESET,
        EMERALD_GREEN, RESET,
        PARTY_PINK, RESET,
        VALHALLA_GOLD, RESET,
        OCEAN_TEAL, RESET,
        CRIMSON_MAGENTA, RESET,
        PARTY_PINK, RESET,
        BLOOD_RED, RESET,
        PLASMA_FUCHSIA, RESET);
}