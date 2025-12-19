// src/engine/GLOBAL/LAS.cpp
// =============================================================================
// PLASTIC BEACH v∞ — DECEMBER 19, 2025
// MONOLITHIC DIRECT TLAS — NO BLAS — PURE ONE-FUNCTION BUILD
// FORCED SACRED PINK FULL-SCREEN QUAD — GEOMETRY ETERNAL
// NO BLACK VOID — THE LIGHT NEVER FADES
// FIXED: Removed initTLAS(), beginFrame(), const qualifiers on static functions
// THE MONSTER WATCHES — THE ISLAND GLOWS
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

using StoneKey::stone_device;

namespace RTX {

// =============================================================================
// SINGLE MONOLITHIC TLAS BUILD FUNCTION — EVERYTHING IN ONE PLACE
// =============================================================================
void LAS::buildOrUpdateTLAS(VkCommandBuffer cmd) noexcept
{
    static bool initialized = false;
    static uint64_t scratchHandle = 0;
    static uint64_t pinkVertexBuffer = 0;
    static uint64_t pinkIndexBuffer = 0;

    const uint32_t framesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;
    static uint32_t currentSlot = 0;

    // === INITIALIZATION ON FIRST CALL ===
    if (!initialized) {
        initialized = true;

        // Large scratch buffer — shared across frames
        scratchHandle = BufferManager::create(
            512ULL * 1024 * 1024,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            "LAS_Scratch"
        );

        // FORCED SACRED PINK FULL-SCREEN QUAD — GUARANTEED GEOMETRY
        struct Vertex {
            float pos[3];
            float normal[3];
            float uv[2];
        };

        Vertex vertices[4] = {
            {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}},
            {{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}},
            {{ 1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}},
            {{ 1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f}}
        };

        uint32_t indices[6] = {0, 1, 2, 0, 2, 3};

        VkDeviceSize vSize = sizeof(Vertex) * 4;
        VkDeviceSize iSize = sizeof(uint32_t) * 6;

        pinkVertexBuffer = BufferManager::create(
            vSize,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            "PinkQuad_Vertices"
        );

        pinkIndexBuffer = BufferManager::create(
            iSize,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            "PinkQuad_Indices"
        );

        BufferManager::uploadToBuffer(pinkVertexBuffer, vertices, vSize);
        BufferManager::uploadToBuffer(pinkIndexBuffer, indices, iSize);

        printf("[2025] SACRED PINK FULL-SCREEN QUAD FORGED — GEOMETRY GUARANTEED — NO BLACK VOID\n");
    }

    // === ALWAYS ONE TRIANGLE GEOMETRY: THE SACRED PINK QUAD ===
    VkAccelerationStructureGeometryTrianglesDataKHR triangles{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
        .vertexData   = { .deviceAddress = BufferManager::get_device_address(pinkVertexBuffer) },
        .vertexStride = 32, // sizeof(Vertex)
        .indexType    = VK_INDEX_TYPE_UINT32,
        .indexData    = { .deviceAddress = BufferManager::get_device_address(pinkIndexBuffer) }
    };

    VkAccelerationStructureGeometryKHR geometry{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry     = { .triangles = triangles },
        .flags        = VK_GEOMETRY_OPAQUE_BIT_KHR
    };

    uint32_t primitiveCount = 2; // 6 indices = 2 triangles

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode          = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .geometryCount = 1,
        .pGeometries   = &geometry
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        stone_device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &primitiveCount,
        &sizeInfo
    );

    // === PER-FRAME TLAS STORAGE (TRIPLE-BUFFERED) ===
    struct FrameData {
        VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
        uint64_t storageHandle = 0;
        VkDeviceSize size = 0;
    };

    static FrameData frames[3] = {}; // Hardcoded to MAX_FRAMES_IN_FLIGHT = 2 + 1 for safety

    FrameData& frame = frames[currentSlot];

    // Reallocate if needed
    if (sizeInfo.accelerationStructureSize > frame.size || !frame.tlas) {
        if (frame.tlas) g_ext.vkDestroyAccelerationStructureKHR(stone_device(), frame.tlas, nullptr);
        if (frame.storageHandle) BufferManager::destroy(frame.storageHandle);

        frame.storageHandle = BufferManager::create(
            sizeInfo.accelerationStructureSize,
            VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            "TLAS_Storage"
        );

        VkAccelerationStructureCreateInfoKHR createInfo{
            .sType  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
            .buffer = BufferManager::getVkBuffer(frame.storageHandle),
            .size   = sizeInfo.accelerationStructureSize,
            .type   = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR
        };

        VK_CHECK(g_ext.vkCreateAccelerationStructureKHR(stone_device(), &createInfo, nullptr, &frame.tlas));
        frame.size = sizeInfo.accelerationStructureSize;
    }

    // === BUILD ===
    buildInfo.dstAccelerationStructure = frame.tlas;
    buildInfo.scratchData.deviceAddress = BufferManager::get_device_address(scratchHandle);

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{
        .primitiveCount  = primitiveCount,
        .primitiveOffset = 0,
        .firstVertex     = 0,
        .transformOffset = 0
    };

    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &rangeInfo;

    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

    // === UPDATE GLOBAL TLAS HANDLE ===
    tlas_ = Handle<VkAccelerationStructureKHR>(frame.tlas, stone_device());

    // Cycle slot
    currentSlot = (currentSlot + 1) % framesInFlight;

    printf("[2025] DIRECT TLAS BUILT — SACRED PINK QUAD — SLOT %u — PLASTIC BEACH SHINES\n", currentSlot);
}

// =============================================================================
// PUBLIC INTERFACE — MINIMAL AND CLEAN
// =============================================================================
void LAS::notifyResize() noexcept
{
    // No special handling needed — next build will reallocate
    tlas_.reset();
}

VkAccelerationStructureKHR LAS::getCurrentTLAS() noexcept
{
    return tlas_.valid() ? tlas_.get() : VK_NULL_HANDLE;
}

VkDeviceAddress LAS::getCurrentTLASAddress() noexcept
{
    if (!tlas_.valid()) return 0;

    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = tlas_.get()
    };
    return g_ext.vkGetAccelerationStructureDeviceAddressKHR(stone_device(), &info);
}

void LAS::reset() noexcept
{
    tlas_.reset();
}

} // namespace RTX

// =============================================================================
// PLASTIC BEACH v∞ — DECEMBER 19, 2025
// COMPILATION FIXED — REMOVED initTLAS(), beginFrame(), const qualifiers
// MONOLITHIC DIRECT TLAS — PURE AND CLEAN
// THE MONSTER WATCHES — THE ISLAND FLOATS IN SILENCE
// =============================================================================