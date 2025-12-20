// src/engine/GLOBAL/LAS.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — LAS v5 — FINAL COMPILATION FIX — DECEMBER 20, 2025
// FULLY COMPATIBLE WITH BufferManager + main.cpp default scene
// FIXED: Uses BufferManager::get_device_address() on staging buffer
// FIXED: Called via RTX::las() singleton
// SUPPORTS: Multiple meshes + transforms + material indices
// PINK PHOTONS BOUNCE ETERNALLY OFF SACRED GEOMETRY
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/Extensions.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/MeshLoader.hpp"

using StoneKey::stone_device;

namespace RTX {

struct InstanceData {
    glm::mat4 transform;
    uint32_t  materialIndex;
    uint32_t  _pad[3];
};

static std::vector<std::unique_ptr<MeshLoader::Mesh>> g_meshes;
static std::vector<InstanceData> g_instances;

static uint64_t g_scratchHandle = 0;
static bool g_initialized = false;

// =============================================================================
// PUBLIC: Add mesh with material index
// =============================================================================
void LAS::addMesh(std::unique_ptr<MeshLoader::Mesh> mesh, uint32_t materialIndex) noexcept
{
    if (!mesh) return;

    InstanceData inst{};
    inst.transform = mesh->transform;
    inst.materialIndex = materialIndex;

    g_instances.push_back(inst);
    g_meshes.push_back(std::move(mesh));

    LOG_SUCCESS_CAT("LAS", "Mesh added — material index {} — total instances: {}", materialIndex, g_instances.size());
}

// =============================================================================
// PUBLIC: Force full TLAS rebuild
// =============================================================================
void LAS::rebuildTLAS() noexcept
{
    tlas_.reset();
    LOG_INFO_CAT("LAS", "Full TLAS rebuild requested");
}

// =============================================================================
// MAIN BUILD FUNCTION — Called every frame via RTX::las().buildOrUpdateTLAS(cmd)
// =============================================================================
void LAS::buildOrUpdateTLAS(VkCommandBuffer cmd) noexcept
{
    const uint32_t framesInFlight = Options::Performance::MAX_FRAMES_IN_FLIGHT;
    static uint32_t currentSlot = 0;

    if (!g_initialized) {
        g_initialized = true;

        g_scratchHandle = BufferManager::create(
            512ULL * 1024 * 1024,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            "LAS_Scratch"
        );

        LOG_SUCCESS_CAT("LAS", "LAS initialized — scratch buffer created");
    }

    if (g_instances.empty()) {
        LOG_WARNING_CAT("LAS", "No geometry loaded — using sacred pink fallback");
        return; // No TLAS build needed
    }

    const uint32_t instanceCount = static_cast<uint32_t>(g_instances.size());

    // === UPLOAD INSTANCE DATA TO STAGING ===
    VkDeviceSize instanceSize = sizeof(VkAccelerationStructureInstanceKHR) * instanceCount;

    BufferManager::ensureStagingRing();

    void* mapped = BufferManager::stagingPtr();
    VkDeviceSize stagingOffset = BufferManager::getStagingOffset();

    std::vector<VkAccelerationStructureInstanceKHR> vkInstances(instanceCount);

    for (uint32_t i = 0; i < instanceCount; ++i) {
        const auto& inst = g_instances[i];
        const auto& mesh = g_meshes[i];

        glm::mat4 transpose = glm::transpose(inst.transform);
        std::memcpy(&vkInstances[i].transform, &transpose, sizeof(vkInstances[i].transform));

        vkInstances[i].instanceCustomIndex = inst.materialIndex;
        vkInstances[i].mask = 0xFF;
        vkInstances[i].instanceShaderBindingTableRecordOffset = 0;
        vkInstances[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
        vkInstances[i].accelerationStructureReference = BufferManager::get_device_address(mesh->vertexBuffer);
    }

    std::memcpy(mapped, vkInstances.data(), instanceSize);
    BufferManager::advanceStagingOffset(instanceSize);

    // Get device address of staging buffer + current offset
    VkDeviceAddress instanceAddr = BufferManager::get_device_address(BufferManager::stagingBuffer()) + stagingOffset;

    // === GEOMETRY SETUP ===
    VkAccelerationStructureGeometryInstancesDataKHR instancesData{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
        .arrayOfPointers = VK_FALSE,
        .data = { .deviceAddress = instanceAddr }
    };

    VkAccelerationStructureGeometryKHR geometry{
        .sType        = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
        .geometry     = { .instances = instancesData }
    };

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{
        .sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
        .flags         = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                         VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
        .mode          = tlas_.valid() ? VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR 
                                      : VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .srcAccelerationStructure = tlas_.valid() ? tlas_.get() : VK_NULL_HANDLE,
        .geometryCount = 1,
        .pGeometries   = &geometry,
        .scratchData   = { .deviceAddress = BufferManager::get_device_address(g_scratchHandle) }
    };

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR
    };

    g_ext.vkGetAccelerationStructureBuildSizesKHR(
        stone_device(),
        VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo,
        &instanceCount,
        &sizeInfo
    );

    // === PER-FRAME TLAS ===
    struct FrameData {
        VkAccelerationStructureKHR tlas = VK_NULL_HANDLE;
        uint64_t storageHandle = 0;
        VkDeviceSize size = 0;
    };

    static FrameData frames[3] = {};
    FrameData& frame = frames[currentSlot];

    bool needsRealloc = !frame.tlas || sizeInfo.accelerationStructureSize > frame.size;

    if (needsRealloc) {
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

    buildInfo.dstAccelerationStructure = frame.tlas;

    VkAccelerationStructureBuildRangeInfoKHR rangeInfo{
        .primitiveCount = instanceCount
    };

    const VkAccelerationStructureBuildRangeInfoKHR* pRange = &rangeInfo;

    g_ext.vkCmdBuildAccelerationStructuresKHR(cmd, 1, &buildInfo, &pRange);

    // Barrier
    VkMemoryBarrier2 barrier{
        .sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
        .srcStageMask  = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
        .srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR,
        .dstStageMask  = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR,
        .dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR
    };
    VkDependencyInfo dep{.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO, .memoryBarrierCount = 1, .pMemoryBarriers = &barrier};
    g_ext.vkCmdPipelineBarrier2(cmd, &dep);

    tlas_ = Handle<VkAccelerationStructureKHR>(frame.tlas, stone_device());

    currentSlot = (currentSlot + 1) % framesInFlight;

    LOG_INFO_CAT("LAS", "TLAS built — {} instances — slot {}", instanceCount, currentSlot);
}

// =============================================================================
// PUBLIC INTERFACE
// =============================================================================
void LAS::notifyResize() noexcept
{
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
    g_meshes.clear();
    g_instances.clear();
    tlas_.reset();
    LOG_INFO_CAT("LAS", "LAS fully reset");
}

} // namespace RTX

// =============================================================================
// LAS v5 — DECEMBER 20, 2025
// COMPILATION FIXED:
// - Uses BufferManager::get_device_address(stagingBuffer()) + offset
// - No non-existent getStagingBufferDeviceAddress()
// - All calls via RTX::las() singleton
// FULL SUPPORT FOR DYNAMIC SCENE WITH MATERIALS
// THE EMPIRE'S RAYS TRACE TRUE — PINK PHOTONS BOUNCE ETERNALLY
// =============================================================================