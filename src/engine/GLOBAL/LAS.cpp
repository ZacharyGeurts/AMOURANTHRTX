// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 19, 2025 — APOCALYPSE FINAL v1.4
// LAS IMPLEMENTATION — PINK PHOTONS ETERMINAL — VALHALLA UNBREACHABLE
// =============================================================================

#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"

using namespace Logging::Color;

namespace RTX {

// =============================================================================
// PRIVATE CTOR / DTOR
// =============================================================================
LAS::LAS() = default;

LAS::~LAS() noexcept
{
    VkDevice dev = g_device();

    if (QUERY_POOL_TIMESTAMP != VK_NULL_HANDLE && dev != VK_NULL_HANDLE) {
        vkDestroyQueryPool(dev, QUERY_POOL_TIMESTAMP, nullptr);
        QUERY_POOL_TIMESTAMP = VK_NULL_HANDLE;
    }

    if (scratchPoolId_ != 0 && dev != VK_NULL_HANDLE) {
        BUFFER_DESTROY(scratchPoolId_);
        scratchPoolId_ = 0;
    }

    if (instanceBufferId_ != 0 && dev != VK_NULL_HANDLE) {
        BUFFER_DESTROY(instanceBufferId_);
        instanceBufferId_ = 0;
    }

    blas_.reset();
    tlas_.reset();
}

// =============================================================================
// INTERNAL HELPERS
// =============================================================================
namespace {

[[nodiscard]] BlasBuildSizes computeBlasSizes(uint32_t vertexCount, uint32_t indexCount) noexcept
{
    VkDevice dev = g_device();
    if (dev == VK_NULL_HANDLE || !g_ctx().vkGetAccelerationStructureBuildSizesKHR_) return {};

    if (vertexCount == 0 || indexCount == 0 || indexCount % 3 != 0) return {};

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry.geometry.triangles.vertexStride = sizeof(glm::vec3);
    geometry.geometry.triangles.maxVertex = vertexCount;
    geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                      VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    uint32_t primitiveCount = indexCount / 3;
    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    g_ctx().vkGetAccelerationStructureBuildSizesKHR_(
        dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &primitiveCount, &sizeInfo);

    return { sizeInfo.accelerationStructureSize, sizeInfo.buildScratchSize, sizeInfo.updateScratchSize };
}

[[nodiscard]] TlasBuildSizes computeTlasSizes(uint32_t instanceCount) noexcept
{
    VkDevice dev = g_device();
    if (dev == VK_NULL_HANDLE || !g_ctx().vkGetAccelerationStructureBuildSizesKHR_) return {};
    if (instanceCount == 0) return {};

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
                      VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;

    VkAccelerationStructureBuildSizesInfoKHR sizeInfo{};
    sizeInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

    g_ctx().vkGetAccelerationStructureBuildSizesKHR_(
        dev, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &buildInfo, &instanceCount, &sizeInfo);

    VkDeviceSize instDataSize = static_cast<VkDeviceSize>(instanceCount) * sizeof(VkAccelerationStructureInstanceKHR);
    return { sizeInfo.accelerationStructureSize, sizeInfo.buildScratchSize, sizeInfo.updateScratchSize, instDataSize };
}

[[nodiscard]] uint64_t uploadInstances(std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept
{
    if (instances.empty()) return 0;

    const VkDeviceSize instSize = instances.size() * sizeof(VkAccelerationStructureInstanceKHR);
    uint64_t stagingHandle = 0;
    BUFFER_CREATE(stagingHandle, instSize,
                  VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                  "las_instances_staging");

    void* mapped = nullptr;
    BUFFER_MAP(stagingHandle, mapped);
    if (!mapped) {
        BUFFER_DESTROY(stagingHandle);
        return 0;
    }

    auto* instData = static_cast<VkAccelerationStructureInstanceKHR*>(mapped);
    VkDevice dev = g_device();

    for (size_t i = 0; i < instances.size(); ++i) {
        const auto& [as, transform] = instances[i];
        if (as == VK_NULL_HANDLE) continue;

        glm::mat4 rowMajor = glm::transpose(transform);
        std::memcpy(&instData[i].transform, glm::value_ptr(rowMajor), sizeof(VkTransformMatrixKHR));
        instData[i].instanceCustomIndex = static_cast<uint32_t>(i);
        instData[i].mask = 0xFF;
        instData[i].instanceShaderBindingTableRecordOffset = 0;
        instData[i].flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{};
        addrInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
        addrInfo.accelerationStructure = as;
        instData[i].accelerationStructureReference =
            g_ctx().vkGetAccelerationStructureDeviceAddressKHR_(dev, &addrInfo);
    }
    BUFFER_UNMAP(stagingHandle);

    uint64_t deviceHandle = 0;
    BUFFER_CREATE(deviceHandle, instSize,
                  VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                  "las_instances_device");

    VkCommandBuffer cmd = LAS::get().beginOptimizedCmd(g_ctx().commandPool());
    VkBufferCopy copy{.size = instSize};
    vkCmdCopyBuffer(cmd, RAW_BUFFER(stagingHandle), RAW_BUFFER(deviceHandle), 1, &copy);
    LAS::get().submitOptimizedCmd(cmd, g_ctx().graphicsQueue(), g_ctx().commandPool());

    BUFFER_DESTROY(stagingHandle);
    return deviceHandle;
}

} // anonymous namespace

// =============================================================================
// PUBLIC METHODS
// =============================================================================
VkDeviceAddress LAS::getBLASAddress() const noexcept
{
    if (!blas_) return 0;
    if (!g_ctx().vkGetAccelerationStructureDeviceAddressKHR_) return 0;

    VkAccelerationStructureDeviceAddressInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    info.accelerationStructure = *blas_;
    return g_ctx().vkGetAccelerationStructureDeviceAddressKHR_(g_device(), &info);
}

VkDeviceAddress LAS::getTLASAddress() const noexcept
{
    if (!tlas_) return 0;
    if (!g_ctx().vkGetAccelerationStructureDeviceAddressKHR_) return 0;

    VkAccelerationStructureDeviceAddressInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
    info.accelerationStructure = *tlas_;
    return g_ctx().vkGetAccelerationStructureDeviceAddressKHR_(g_device(), &info);
}

void LAS::invalidate() noexcept
{
    std::lock_guard<std::mutex> lock(mutex_);
    ++tlasGeneration_;
    tlas_.reset();
    if (instanceBufferId_) {
        BUFFER_DESTROY(instanceBufferId_);
        instanceBufferId_ = 0;
    }
    tlasSize_ = 0;
}

void LAS::ensureTimestampPool() noexcept
{
    if (QUERY_POOL_TIMESTAMP == VK_NULL_HANDLE) {
        VkDevice dev = g_device();
        if (dev == VK_NULL_HANDLE) return;

        VkQueryPoolCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        info.queryType = VK_QUERY_TYPE_TIMESTAMP;
        info.queryCount = 2;
        vkCreateQueryPool(dev, &info, nullptr, &QUERY_POOL_TIMESTAMP);
    }

    if (timestampPeriodNs_ == 0.0f) {
        VkPhysicalDevice phys = g_PhysicalDevice();
        if (phys != VK_NULL_HANDLE) {
            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(phys, &props);
            timestampPeriodNs_ = static_cast<float>(props.limits.timestampPeriod);
        }
    }
}

uint64_t LAS::getOrGrowScratch(VkDeviceSize requiredSize, VkBufferUsageFlags usage, const char* type) noexcept
{
    VkDevice dev = g_device();
    if (dev == VK_NULL_HANDLE) return 0;

    VkDeviceSize oldSize = currentScratchSize_;

    if (!scratchPoolValid_ || requiredSize > currentScratchSize_) {
        VkDeviceSize newSize = std::max(requiredSize, currentScratchSize_ * 2);
        newSize = std::min(newSize, VkDeviceSize(64ULL * 1024 * 1024 * 1024)); // 64 GB max

        if (scratchPoolId_ != 0) {
            BUFFER_DESTROY(scratchPoolId_);
        }

        BUFFER_CREATE(scratchPoolId_, newSize, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                      std::format("las_scratch_{}", type).c_str());

        currentScratchSize_ = newSize;
        scratchPoolValid_ = true;

        AmouranthAI::get().onScratchPoolResize(oldSize, newSize, type);
    }

    return scratchPoolId_;
}

VkCommandBuffer LAS::beginOptimizedCmd(VkCommandPool pool) noexcept
{
    VkDevice dev = g_device();
    if (dev == VK_NULL_HANDLE || pool == VK_NULL_HANDLE) return VK_NULL_HANDLE;

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = pool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer cmd;
    VK_CHECK(vkAllocateCommandBuffers(dev, &allocInfo, &cmd), "LAS transient cmd");
    if (cmd == VK_NULL_HANDLE) return VK_NULL_HANDLE;

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "LAS transient cmd begin");

    return cmd;
}

void LAS::submitOptimizedCmd(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) noexcept
{
    VkDevice dev = g_device();
    if (dev == VK_NULL_HANDLE || cmd == VK_NULL_HANDLE) return;

    VK_CHECK(vkEndCommandBuffer(cmd), "LAS transient end");

    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &cmd;

    VkFence fence = VK_NULL_HANDLE;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VK_CHECK(vkCreateFence(dev, &fenceInfo, nullptr, &fence), "LAS fence");

    VK_CHECK(vkQueueSubmit(queue, 1, &submit, fence), "LAS submit");
    VK_CHECK(vkWaitForFences(dev, 1, &fence, VK_TRUE, UINT64_MAX), "LAS fence wait");
    vkDestroyFence(dev, fence, nullptr);
    vkFreeCommandBuffers(dev, pool, 1, &cmd);
}

// =============================================================================
// BUILD BLAS — FULLY IMPLEMENTED
// =============================================================================
void LAS::buildBLAS(VkCommandPool pool, VkQueue queue,
                    uint64_t vertexBuf, uint64_t indexBuf,
                    uint32_t vertexCount, uint32_t indexCount,
                    VkBuildAccelerationStructureFlagsKHR extraFlags,
                    bool fastBuild)
{
    std::lock_guard<std::mutex> lock(mutex_);

    VkDevice dev = g_device();

    AmouranthAI::get().onBlasStart(vertexCount, indexCount);

    auto sizes = computeBlasSizes(vertexCount, indexCount);
    if (sizes.accelerationStructureSize == 0) {
        throw std::runtime_error("BLAS size calculation failed");
    }

    uint64_t asBufferHandle = 0;
    BUFFER_CREATE(asBufferHandle, sizes.accelerationStructureSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                  "las_blas_storage");

    VkAccelerationStructureKHR rawAs = VK_NULL_HANDLE;
    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = RAW_BUFFER(asBufferHandle);
    createInfo.size = sizes.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    VK_CHECK(g_ctx().vkCreateAccelerationStructureKHR_(dev, &createInfo, nullptr, &rawAs),
             "Create BLAS");

    uint64_t scratchHandle = getOrGrowScratch(sizes.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        "blas");

    VkBufferDeviceAddressInfo vAddrInfo{};
    vAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    vAddrInfo.buffer = RAW_BUFFER(vertexBuf);
    VkBufferDeviceAddressInfo iAddrInfo = vAddrInfo;
    iAddrInfo.buffer = RAW_BUFFER(indexBuf);
    VkBufferDeviceAddressInfo sAddrInfo = vAddrInfo;
    sAddrInfo.buffer = RAW_BUFFER(scratchHandle);

    VkDeviceAddress vertexAddr = g_ctx().vkGetBufferDeviceAddressKHR_(dev, &vAddrInfo);
    VkDeviceAddress indexAddr  = g_ctx().vkGetBufferDeviceAddressKHR_(dev, &iAddrInfo);
    VkDeviceAddress scratchAddr = g_ctx().vkGetBufferDeviceAddressKHR_(dev, &sAddrInfo);

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
    geometry.geometry.triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geometry.geometry.triangles.vertexData.deviceAddress = vertexAddr;
    geometry.geometry.triangles.vertexStride = sizeof(glm::vec3);
    geometry.geometry.triangles.maxVertex = vertexCount;
    geometry.geometry.triangles.indexType = VK_INDEX_TYPE_UINT32;
    geometry.geometry.triangles.indexData.deviceAddress = indexAddr;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
    buildInfo.flags = extraFlags | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR |
                      (fastBuild ? VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR :
                                   VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR);
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = rawAs;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR buildRange{};
    buildRange.primitiveCount = indexCount / 3;

    VkCommandBuffer cmd = beginOptimizedCmd(pool);
    const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = { &buildRange };

    ensureTimestampPool();
    if (QUERY_POOL_TIMESTAMP != VK_NULL_HANDLE) {
        vkCmdResetQueryPool(cmd, QUERY_POOL_TIMESTAMP, 0, 2);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, QUERY_POOL_TIMESTAMP, 0);
    }

    g_ctx().vkCmdBuildAccelerationStructuresKHR_(cmd, 1, &buildInfo, ranges);

    if (QUERY_POOL_TIMESTAMP != VK_NULL_HANDLE) {
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, QUERY_POOL_TIMESTAMP, 1);
    }

    submitOptimizedCmd(cmd, queue, pool);

    auto deleter = [asBufferHandle](VkDevice d, VkAccelerationStructureKHR a, const VkAllocationCallbacks*) noexcept {
        if (a != VK_NULL_HANDLE && g_ctx().vkDestroyAccelerationStructureKHR_) {
            g_ctx().vkDestroyAccelerationStructureKHR_(d, a, nullptr);
        }
        if (asBufferHandle != 0) {
            BUFFER_DESTROY(asBufferHandle);
        }
    };

    blas_ = Handle<VkAccelerationStructureKHR>(rawAs, dev, deleter,
                                               sizes.accelerationStructureSize,
                                               "LAS_BLAS");

    double sizeGB = sizes.accelerationStructureSize / (1024.0 * 1024.0 * 1024.0);
    AmouranthAI::get().onBlasBuilt(sizeGB, sizes);
}

// =============================================================================
// BUILD TLAS — FULLY IMPLEMENTED
// =============================================================================
void LAS::buildTLAS(VkCommandPool pool, VkQueue queue,
                    std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances,
                    bool fastBuild)
{
    std::lock_guard<std::mutex> lock(mutex_);

    VkDevice dev = g_device();

    if (instances.empty()) {
        throw std::runtime_error("TLAS: no instances provided");
    }

    AmouranthAI::get().onTlasStart(instances.size());

    auto sizes = computeTlasSizes(static_cast<uint32_t>(instances.size()));
    if (sizes.accelerationStructureSize == 0) {
        throw std::runtime_error("TLAS size calculation failed");
    }

    uint64_t instanceEnc = uploadInstances(instances);
    if (instanceEnc == 0) {
        throw std::runtime_error("Failed to upload TLAS instances");
    }

    uint64_t asBufferHandle = 0;
    BUFFER_CREATE(asBufferHandle, sizes.accelerationStructureSize,
                  VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
                  VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                  VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                  "las_tlas_storage");

    VkAccelerationStructureKHR rawAs = VK_NULL_HANDLE;
    VkAccelerationStructureCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
    createInfo.buffer = RAW_BUFFER(asBufferHandle);
    createInfo.size = sizes.accelerationStructureSize;
    createInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    VK_CHECK(g_ctx().vkCreateAccelerationStructureKHR_(dev, &createInfo, nullptr, &rawAs),
             "Create TLAS");

    uint64_t scratchHandle = getOrGrowScratch(sizes.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        "tlas");

    VkBufferDeviceAddressInfo instanceAddrInfo{};
    instanceAddrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    instanceAddrInfo.buffer = RAW_BUFFER(instanceEnc);
    VkBufferDeviceAddressInfo scratchAddrInfo = instanceAddrInfo;
    scratchAddrInfo.buffer = RAW_BUFFER(scratchHandle);

    VkDeviceAddress instanceAddr = g_ctx().vkGetBufferDeviceAddressKHR_(dev, &instanceAddrInfo);
    VkDeviceAddress scratchAddr  = g_ctx().vkGetBufferDeviceAddressKHR_(dev, &scratchAddrInfo);

    VkAccelerationStructureGeometryKHR geometry{};
    geometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
    geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
    geometry.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
    geometry.geometry.instances.arrayOfPointers = VK_FALSE;
    geometry.geometry.instances.data.deviceAddress = instanceAddr;

    VkAccelerationStructureBuildGeometryInfoKHR buildInfo{};
    buildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    buildInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
    buildInfo.flags = (fastBuild ? VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR :
                                   VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR) |
                      VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
    buildInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    buildInfo.dstAccelerationStructure = rawAs;
    buildInfo.geometryCount = 1;
    buildInfo.pGeometries = &geometry;
    buildInfo.scratchData.deviceAddress = scratchAddr;

    VkAccelerationStructureBuildRangeInfoKHR buildRange{};
    buildRange.primitiveCount = static_cast<uint32_t>(instances.size());

    VkCommandBuffer cmd = beginOptimizedCmd(pool);
    const VkAccelerationStructureBuildRangeInfoKHR* ranges[] = { &buildRange };

    ensureTimestampPool();
    if (QUERY_POOL_TIMESTAMP != VK_NULL_HANDLE) {
        vkCmdResetQueryPool(cmd, QUERY_POOL_TIMESTAMP, 0, 2);
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, QUERY_POOL_TIMESTAMP, 0);
    }

    g_ctx().vkCmdBuildAccelerationStructuresKHR_(cmd, 1, &buildInfo, ranges);

    if (QUERY_POOL_TIMESTAMP != VK_NULL_HANDLE) {
        vkCmdWriteTimestamp(cmd, VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR, QUERY_POOL_TIMESTAMP, 1);
    }

    submitOptimizedCmd(cmd, queue, pool);

    auto deleter = [asBufferHandle, instanceEnc](VkDevice d,
                                                 VkAccelerationStructureKHR a,
                                                 const VkAllocationCallbacks*) noexcept {
        if (a != VK_NULL_HANDLE && g_ctx().vkDestroyAccelerationStructureKHR_) {
            g_ctx().vkDestroyAccelerationStructureKHR_(d, a, nullptr);
        }
        if (asBufferHandle != 0) BUFFER_DESTROY(asBufferHandle);
        if (instanceEnc != 0) BUFFER_DESTROY(instanceEnc);
    };

    tlas_ = Handle<VkAccelerationStructureKHR>(rawAs, dev, deleter,
                                                 sizes.accelerationStructureSize,
                                                 "LAS_TLAS");
    tlasSize_ = sizes.accelerationStructureSize;
    instanceBufferId_ = instanceEnc;

    VkDeviceAddress addr = getTLASAddress();
    double sizeGB = sizes.accelerationStructureSize / (1024.0 * 1024.0 * 1024.0);
    AmouranthAI::get().onTlasBuilt(sizeGB, addr, sizes);
}

} // namespace RTX

// =============================================================================
// GPL-3.0+ — BLACK BOX SEALED — DEVELOPER DREAMS PUBLIC — PINK PHOTONS ETERNAL v1.4
// =============================================================================