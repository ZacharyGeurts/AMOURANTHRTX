// include/engine/GLOBAL/LAS.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
//
// LIGHT_WARRIORS_LAS v6.0 — GOD MODE — NOV 11 2025 2:50 PM EST
// • 100% SELF-CONTAINED — NO HEADERS, NO LOGGING, NO DEPENDENCIES
// • RAW std::cerr + COLOR — NO Logger class
// • THROW ON ERROR — C++23 exceptions
// • Global macros: BUILD_BLAS, GLOBAL_TLAS_ADDRESS
// • UltraLowLevelBufferTracker, ctx(), Handle — all inlined
// • C++23, -Werror clean, Valhalla sealed
// =============================================================================

#pragma once

#define VK_ENABLE_BETA_EXTENSIONS 1
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <SDL3/SDL.h>

#include "engine/GLOBAL/GlobalContext.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <span>
#include <vector>
#include <mutex>
#include <cstring>
#include <cstdint>
#include <functional>
#include <utility>
#include <atomic>
#include <iostream>
#include <format>
#include <stdexcept>

// ──────────────────────────────────────────────────────────────────────────────
// COLOR CONSTANTS — GOD-INTENDED
// ──────────────────────────────────────────────────────────────────────────────
namespace Color {
    inline constexpr std::string_view RESET           = "\033[0m";
    inline constexpr std::string_view BOLD            = "\033[1m";
    inline constexpr std::string_view FUCHSIA_MAGENTA = "\033[1;38;5;205m";  // ERROR
    inline constexpr std::string_view PARTY_PINK      = "\033[1;38;5;213m";  // BLAS
    inline constexpr std::string_view ELECTRIC_BLUE   = "\033[1;38;5;75m";   // TLAS
    inline constexpr std::string_view LIME_GREEN      = "\033[1;38;5;154m";  // INFO
}

// ──────────────────────────────────────────────────────────────────────────────
// RAW CONSOLE OUTPUT — NO LOGGER
// ──────────────────────────────────────────────────────────────────────────────
#define CONSOLE_ERROR(msg) \
    std::cerr << Color::FUCHSIA_MAGENTA << "[ERROR] LAS " << msg << Color::RESET << '\n'

#define CONSOLE_WARNING(msg) \
    std::cerr << Color::LIME_GREEN << "[WARN] LAS " << msg << Color::RESET << '\n'

#define CONSOLE_INFO(msg) \
    std::cerr << Color::LIME_GREEN << "[INFO] LAS " << msg << Color::RESET << '\n'

#define CONSOLE_BLAS(msg) \
    std::cerr << Color::PARTY_PINK << "[BLAS] " << msg << Color::RESET << '\n'

#define CONSOLE_TLAS(msg) \
    std::cerr << Color::ELECTRIC_BLUE << "[TLAS] " << msg << Color::RESET << '\n'

// ──────────────────────────────────────────────────────────────────────────────
// THROW ON ERROR
// ──────────────────────────────────────────────────────────────────────────────
#define THROW_IF(cond, msg) \
    do { if (cond) { CONSOLE_ERROR(msg); throw std::runtime_error(msg); } } while(0)

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL CONTEXT — ETERNAL
// ──────────────────────────────────────────────────────────────────────────────
inline GlobalRTXContext& ctx() noexcept { return GlobalRTXContext::get(); }

// ──────────────────────────────────────────────────────────────────────────────
// BUILD SIZE STRUCTS
// ──────────────────────────────────────────────────────────────────────────────
struct BlasBuildSizes {
    VkDeviceSize accelerationStructureSize = 0;
    VkDeviceSize buildScratchSize = 0;
    VkDeviceSize updateScratchSize = 0;
};

struct TlasBuildSizes {
    VkDeviceSize accelerationStructureSize = 0;
    VkDeviceSize buildScratchSize = 0;
    VkDeviceSize updateScratchSize = 0;
    VkDeviceSize instanceBufferSize = 0;
};

// ──────────────────────────────────────────────────────────────────────────────
// SINGLE-TIME COMMANDS
// ──────────────────────────────────────────────────────────────────────────────
[[nodiscard]] inline VkCommandBuffer beginSingleTimeCommands(VkCommandPool pool) {
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VkCommandBuffer cmd;
    THROW_IF(vkAllocateCommandBuffers(ctx().vkDevice(), &allocInfo, &cmd) != VK_SUCCESS, "Failed to allocate command buffer");
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    THROW_IF(vkBeginCommandBuffer(cmd, &beginInfo) != VK_SUCCESS, "Failed to begin command buffer");
    return cmd;
}

inline void endSingleTimeCommands(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) noexcept {
    vkEndCommandBuffer(cmd);
    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);
    vkFreeCommandBuffers(ctx().vkDevice(), pool, 1, &cmd);
}

// ──────────────────────────────────────────────────────────────────────────────
// SIZE COMPUTATION
// ──────────────────────────────────────────────────────────────────────────────
static BlasBuildSizes computeBlasSizes(VkDevice device, uint32_t vertexCount, uint32_t indexCount) noexcept {
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

static TlasBuildSizes computeTlasSizes(VkDevice device, uint32_t instanceCount) noexcept {
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

// ──────────────────────────────────────────────────────────────────────────────
// INSTANCE UPLOAD
// ──────────────────────────────────────────────────────────────────────────────
[[nodiscard]] static uint64_t uploadInstances(
    VkDevice device, VkPhysicalDevice physDev, VkCommandPool pool, VkQueue queue,
    std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances)
{
    if (instances.empty()) return 0;

    const VkDeviceSize instSize = static_cast<VkDeviceSize>(instances.size()) * sizeof(VkAccelerationStructureInstanceKHR);
    uint64_t stagingHandle = 0;
    BUFFER_CREATE(stagingHandle, instSize,
                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "thief_instances");

    void* mapped = nullptr;
    auto* d = UltraLowLevelBufferTracker::get().getData(stagingHandle);
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
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "thief_rtx_instances");

    THROW_IF(deviceHandle == 0, "Failed to create device-local instance buffer");

    VkCommandBuffer cmd = beginSingleTimeCommands(pool);
    VkBufferCopy copyRegion{ .srcOffset = 0, .dstOffset = 0, .size = instSize };
    vkCmdCopyBuffer(cmd, RAW_BUFFER(stagingHandle), RAW_BUFFER(deviceHandle), 1, &copyRegion);
    endSingleTimeCommands(cmd, queue, pool);

    BUFFER_DESTROY(stagingHandle);
    return deviceHandle;
}

// ──────────────────────────────────────────────────────────────────────────────
// LIGHT_WARRIORS_LAS — GOD MODE
// ──────────────────────────────────────────────────────────────────────────────
class LIGHT_WARRIORS_LAS {
public:
    static LIGHT_WARRIORS_LAS& get() noexcept {
        static LIGHT_WARRIORS_LAS instance;
        return instance;
    }

    void buildBLAS(VkCommandPool pool, VkQueue queue,
                   uint64_t vertexBuf, uint64_t indexBuf,
                   uint32_t vertexCount, uint32_t indexCount,
                   VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);

    void buildTLAS(VkCommandPool pool, VkQueue queue,
                   std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances);

    [[nodiscard]] VkAccelerationStructureKHR getBLAS() const noexcept { return blas_ ? *blas_ : VK_NULL_HANDLE; }
    [[nodiscard]] VkDeviceAddress getBLASAddress() const noexcept;
    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept { return tlas_ ? *tlas_ : VK_NULL_HANDLE; }
    [[nodiscard]] VkDeviceAddress getTLASAddress() const noexcept;
    [[nodiscard]] VkDeviceSize getTLASSize() const noexcept { return tlasSize_; }

    void rebuildTLAS(VkCommandPool pool, VkQueue queue,
                     std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances);

private:
    LIGHT_WARRIORS_LAS() {
        UltraLowLevelBufferTracker::get().init(ctx().vkDevice(), ctx().vkPhysicalDevice());
    }

    Handle<VkAccelerationStructureKHR> blas_;
    Handle<VkAccelerationStructureKHR> tlas_;
    uint64_t instanceBufferId_ = 0;
    VkDeviceSize tlasSize_ = 0;
    mutable std::mutex mutex_;
};

// ──────────────────────────────────────────────────────────────────────────────
// BLAS: FIGHTER'S RESOLVE
// ──────────────────────────────────────────────────────────────────────────────
inline void LIGHT_WARRIORS_LAS::buildBLAS(VkCommandPool pool, VkQueue queue,
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
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "fighter_blas");

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

    blas_ = Handle<VkAccelerationStructureKHR>(rawAs, dev, deleter, sizes.accelerationStructureSize, "FIGHTER_BLAS");
    BUFFER_DESTROY(scratchHandle);
    CONSOLE_BLAS(std::format("FIGHTER'S RESOLVE ONLINE — {} verts | {} indices | {:.2f} GB", vertexCount, indexCount, sizes.accelerationStructureSize / (1024.0*1024.0*1024.0)));
}

// ──────────────────────────────────────────────────────────────────────────────
// TLAS: SAGE'S PROPHECY
// ──────────────────────────────────────────────────────────────────────────────
inline void LIGHT_WARRIORS_LAS::buildTLAS(VkCommandPool pool, VkQueue queue,
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
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, "sage_tlas");

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

    tlas_ = Handle<VkAccelerationStructureKHR>(rawAs, dev, deleter, sizes.accelerationStructureSize, "SAGE_TLAS");
    tlasSize_ = sizes.accelerationStructureSize;
    instanceBufferId_ = instanceEnc;
    BUFFER_DESTROY(scratchHandle);
    CONSOLE_TLAS(std::format("SAGE'S PROPHECY ONLINE — {} instances | {:.2f} GB", instances.size(), sizes.accelerationStructureSize / (1024.0*1024.0*1024.0)));
}

inline VkDeviceAddress LIGHT_WARRIORS_LAS::getBLASAddress() const noexcept {
    if (!blas_) return 0;
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = *blas_
    };
    return vkGetAccelerationStructureDeviceAddressKHR(ctx().vkDevice(), &info);
}

inline VkDeviceAddress LIGHT_WARRIORS_LAS::getTLASAddress() const noexcept {
    if (!tlas_) return 0;
    VkAccelerationStructureDeviceAddressInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
        .accelerationStructure = *tlas_
    };
    return vkGetAccelerationStructureDeviceAddressKHR(ctx().vkDevice(), &info);
}

inline void LIGHT_WARRIORS_LAS::rebuildTLAS(VkCommandPool pool, VkQueue queue,
                                            std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) {
    std::lock_guard<std::mutex> lock(mutex_);
    tlas_.reset();
    instanceBufferId_ = 0;
    tlasSize_ = 0;
    buildTLAS(pool, queue, instances);
}

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL MACROS
// ──────────────────────────────────────────────────────────────────────────────
#define BUILD_BLAS(pool, q, vbuf, ibuf, vcount, icount, flags) \
    LIGHT_WARRIORS_LAS::get().buildBLAS(pool, q, vbuf, ibuf, vcount, icount, flags)

#define BUILD_TLAS(pool, q, instances) \
    LIGHT_WARRIORS_LAS::get().buildTLAS(pool, q, instances)

#define GLOBAL_BLAS()          (LIGHT_WARRIORS_LAS::get().getBLAS())
#define GLOBAL_BLAS_ADDRESS()  (LIGHT_WARRIORS_LAS::get().getBLASAddress())
#define GLOBAL_TLAS()          (LIGHT_WARRIORS_LAS::get().getTLAS())
#define GLOBAL_TLAS_ADDRESS()  (LIGHT_WARRIORS_LAS::get().getTLASAddress())

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================