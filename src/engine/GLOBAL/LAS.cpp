// src/engine/GLOBAL/LAS.cpp
// AMOURANTH RTX ENGINE — GLOBAL LAS — APOCALYPSE FINAL v5.0 — NOVEMBER 20, 2025
// PINK PHOTONS ETERNAL — STONEKEY v∞ ACTIVE

#include "engine/GLOBAL/LAS.hpp"
#include "engine/Vulkan/VulkanCore.hpp"          // for beginSingleTimeCommands / endSingleTimeCommandsAsync
#include "engine/GLOBAL/RTXHandler.hpp"

using namespace RTX;

// =============================================================================
// BLAS BUILD — uses global VulkanRTX helpers
// =============================================================================
void LAS::buildBLAS(VkCommandPool pool,
                    VkBuffer vertexBuffer,
                    VkBuffer indexBuffer,
                    uint32_t vertexCount,
                    uint32_t indexCount,
                    VkBuildAccelerationStructureFlagsKHR extraFlags)
{
    // Lazy-create the VulkanAccel helper exactly once
    if (!accel_) {
        accel_ = std::make_unique<VulkanAccel>(RTX::g_ctx().device());
    }

    AccelGeometry geom{};
    geom.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    geom.vertexStride = sizeof(glm::vec3);
    geom.vertexCount  = vertexCount;

    // Proper temporary → address taken is now legal
    VkBufferDeviceAddressInfo vAddrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = vertexBuffer
    };
    geom.vertexData.deviceAddress = RTX::g_ctx().vkGetBufferDeviceAddressKHR()(RTX::g_ctx().device(), &vAddrInfo);

    geom.indexType  = VK_INDEX_TYPE_UINT32;
    geom.indexCount = indexCount;

    VkBufferDeviceAddressInfo iAddrInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = indexBuffer
    };
    geom.indexData.deviceAddress = RTX::g_ctx().vkGetBufferDeviceAddressKHR()(RTX::g_ctx().device(), &iAddrInfo);

    VkCommandBuffer cmd = VulkanRTX::beginSingleTimeCommands(pool);

    blas_ = accel_->createBLAS(
        {geom},
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | extraFlags,
        cmd,
        "AmouranthCube_BLAS"
    );

    endSingleTimeCommandsAsync(cmd, g_ctx().graphicsQueue(), pool);
    ++generation_;
}

// =============================================================================
// TLAS BUILD — signature now matches header (takes command pool, not cmd)
// =============================================================================
void LAS::buildTLAS(VkCommandPool pool,
                    const std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>>& instances)
{
    if (instances.empty()) {
        LOG_WARN_CAT("LAS", "buildTLAS called with zero instances — skipping");
        return;
    }

    if (!accel_) {
        accel_ = std::make_unique<VulkanAccel>(RTX::g_ctx().device());
    }

    std::vector<VkAccelerationStructureInstanceKHR> vkInstances;
    vkInstances.reserve(instances.size());

    for (const auto& [as, transform] : instances) {
        VkAccelerationStructureInstanceKHR inst{};

        // glm::mat4 → VkTransformMatrixKHR (row-major → column-major)
        const float* m = glm::value_ptr(transform);
        inst.transform.matrix[0][0] = m[0];  inst.transform.matrix[0][1] = m[1];  inst.transform.matrix[0][2] = m[2];  inst.transform.matrix[0][3] = m[3];
        inst.transform.matrix[1][0] = m[4];  inst.transform.matrix[1][1] = m[5];  inst.transform.matrix[1][2] = m[6];  inst.transform.matrix[1][3] = m[7];
        inst.transform.matrix[2][0] = m[8];  inst.transform.matrix[2][1] = m[9];  inst.transform.matrix[2][2] = m[10]; inst.transform.matrix[2][3] = m[11];

        inst.instanceCustomIndex                    = 0;
        inst.mask                                   = 0xFF;
        inst.instanceShaderBindingTableRecordOffset = 0;
        inst.flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;

        VkAccelerationStructureDeviceAddressInfoKHR addrInfo{
            .sType                  = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
            .accelerationStructure = as
        };
        inst.accelerationStructureReference = RTX::g_ctx().vkGetAccelerationStructureDeviceAddressKHR()(
            RTX::g_ctx().device(), &addrInfo);

        vkInstances.push_back(inst);
    }

    VkCommandBuffer cmd = VulkanRTX::beginSingleTimeCommands(pool);

    tlas_ = accel_->createTLAS(
        vkInstances,
        VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        cmd,
        "AmouranthScene_TLAS"
    );

    endSingleTimeCommandsAsync(cmd, g_ctx().graphicsQueue(), pool);
    ++generation_;

    LOG_SUCCESS_CAT("LAS", "{}TLAS built — {} instances — generation {}{}",
                    PLASMA_FUCHSIA, instances.size(), generation_, RESET);
}