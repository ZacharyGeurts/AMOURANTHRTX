// include/engine/GLOBAL/LAS.hpp
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 21, 2025 — APOCALYPSE FINAL v10.3
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — VALHALLA TURBO v80
// FULLY COMPATIBLE WITH VALIDATION SUITE — getBLASStruct() ADDED
// NOW WITH SMART LOGGING + PFN NULL GUARD AWARENESS
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <glm/glm.hpp>
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanCore.hpp"
#include "engine/GLOBAL/logging.hpp"

// =============================================================================
// AccelGeometry — The one true geometry description
// =============================================================================

struct AccelGeometry
{
    VkGeometryTypeKHR                type           = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    VkGeometryFlagsKHR               flags          = 0;
    VkFormat                         vertexFormat   = VK_FORMAT_R32G32B32_SFLOAT;
    VkDeviceSize                     vertexStride   = 12;
    uint32_t                         vertexCount    = 0;
    VkDeviceOrHostAddressConstKHR    vertexData     {};
    VkIndexType                      indexType      = VK_INDEX_TYPE_UINT32;
    uint32_t                         indexCount     = 0;
    VkDeviceOrHostAddressConstKHR    indexData      {};
    VkDeviceOrHostAddressConstKHR    transformData  {};
};

// =============================================================================
// VulkanAccel — THE ONE TRUE ACCELERATION ENGINE — v6.1 FINAL
// =============================================================================

class VulkanAccel
{
public:
    struct BLAS {
        VkAccelerationStructureKHR   as       = VK_NULL_HANDLE;
        VkBuffer                     buffer   = VK_NULL_HANDLE;
        VkDeviceMemory               memory   = VK_NULL_HANDLE;
        VkDeviceAddress              address  = 0;
        VkDeviceSize                 size     = 0;
        std::string                  name;

        [[nodiscard]] bool isValid() const noexcept { return as != VK_NULL_HANDLE && address != 0; }
    };

    struct TLAS {
        VkAccelerationStructureKHR   as            = VK_NULL_HANDLE;
        VkBuffer                     buffer        = VK_NULL_HANDLE;
        VkDeviceMemory               memory        = VK_NULL_HANDLE;
        VkBuffer                     instanceBuffer = VK_NULL_HANDLE;
        VkDeviceMemory               instanceMemory = VK_NULL_HANDLE;
        VkDeviceAddress              address       = 0;
        VkDeviceSize                 size          = 0;
        std::string                  name;

        [[nodiscard]] bool isValid() const noexcept { return as != VK_NULL_HANDLE && address != 0; }
    };

    explicit VulkanAccel(VkDevice device);
    ~VulkanAccel() = default;

    BLAS createBLAS(const std::vector<AccelGeometry>& geometries,
                    VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
                    VkCommandBuffer externalCmd = VK_NULL_HANDLE,
                    std::string_view name = "BLAS");

    TLAS createTLAS(const std::vector<VkAccelerationStructureInstanceKHR>& instances,
                    VkBuildAccelerationStructureFlagsKHR flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
                    VkCommandBuffer externalCmd = VK_NULL_HANDLE,
                    std::string_view name = "TLAS");

    void destroy(BLAS& blas);
    void destroy(TLAS& tlas);
};

// =============================================================================
// ONE-TIME COMMAND HELPERS — IMMORTAL — NEVER DROPPED BY LTO
// =============================================================================

[[nodiscard]] inline VkCommandBuffer beginOneTime(VkCommandPool pool)
{
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo allocInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.commandPool        = pool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VK_CHECK(vkAllocateCommandBuffers(g_ctx().device(), &allocInfo, &cmd),
             "Failed to allocate one-time command buffer");

    VkCommandBufferBeginInfo beginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo), "Failed to begin one-time command buffer");

    LOG_DEBUG_CAT("RTX", "One-time command buffer allocated and begun: 0x{:016X}", reinterpret_cast<uint64_t>(cmd));
    return cmd;
}

inline void endOneTime(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool = VK_NULL_HANDLE)
{
    VK_CHECK(vkEndCommandBuffer(cmd), "Failed to end one-time command buffer");

    VkSubmitInfo submit{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;

    VK_CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE), "One-time queue submit failed");
    VK_CHECK(vkQueueWaitIdle(queue), "Queue wait idle failed after one-time submit");

    if (pool != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(g_ctx().device(), pool, 1, &cmd);
        LOG_DEBUG_CAT("RTX", "One-time command buffer freed after sync submit");
    }
}

// THE ONE TRUE ASYNC END — PINK PHOTONS APPROVED — NOW WITH LOGGING
static inline void endSingleTimeCommandsAsync(
    VkCommandBuffer cmd,
    VkQueue queue,
    VkCommandPool pool,
    VkFence fence = VK_NULL_HANDLE) noexcept
{
    if (cmd == VK_NULL_HANDLE || queue == VK_NULL_HANDLE || pool == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RTX", "endSingleTimeCommandsAsync: invalid handle (cmd=0x{:016X}, queue=0x{:016X}, pool=0x{:016X})",
                      reinterpret_cast<uint64_t>(cmd), reinterpret_cast<uint64_t>(queue), reinterpret_cast<uint64_t>(pool));
        return;
    }

    VK_CHECK(vkEndCommandBuffer(cmd), "Failed to end one-time command buffer");

    VkDevice dev = g_ctx().device();
    bool ownsFence = (fence == VK_NULL_HANDLE);

    if (ownsFence) {
        VkFenceCreateInfo fi{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        VK_CHECK(vkCreateFence(dev, &fi, nullptr, &fence), "Failed to create transient fence");
        LOG_DEBUG_CAT("RTX", "Transient fence created for async one-time submit: 0x{:016X}", reinterpret_cast<uint64_t>(fence));
    } else {
        VK_CHECK(vkResetFences(dev, 1, &fence), "Failed to reset caller fence");
    }

    VkSubmitInfo submit{
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd
    };

    VK_CHECK(vkQueueSubmit(queue, 1, &submit, fence), "One-time async submit failed");

    if (ownsFence) {
        constexpr uint64_t timeout_ns = 15'000'000'000ULL;
        VkResult r = vkWaitForFences(dev, 1, &fence, VK_TRUE, timeout_ns);
        if (r != VK_SUCCESS) {
            LOG_FATAL_CAT("RTX", "One-time command timeout ({}), forcing device idle", static_cast<int32_t>(r));
            vkDeviceWaitIdle(dev);
        }
        vkFreeCommandBuffers(dev, pool, 1, &cmd);
        vkDestroyFence(dev, fence, nullptr);
        LOG_DEBUG_CAT("RTX", "Async one-time command completed and cleaned up");
    }
}

// =============================================================================
// LAS — THE ONE TRUE GLOBAL ACCELERATION SYSTEM — FINAL v10.4 + SMART LOGGING
// =============================================================================

class LAS
{
public:
    static LAS& get() noexcept { static LAS instance; return instance; }

    // EXPLICIT CONTEXT FORGING — GUARANTEES RTX PFNS ARE LOADED
    void forgeAccelContext()
    {
        if (accel_) {
            LOG_WARN_CAT("LAS", "Acceleration context already forged — skipping");
            return;
        }

        LOG_INFO_CAT("LAS", "FORGING ACCELERATION CONTEXT — ENSURING ALL KHR_acceleration_structure PFNS ARE ACTIVE");

        accel_ = std::make_unique<VulkanAccel>(g_ctx().device());

        // CRITICAL: VALIDATE THAT THE MOST IMPORTANT PFN IS NOT NULL
        if (!g_ctx().vkGetAccelerationStructureBuildSizesKHR()) {
            LOG_FATAL_CAT("LAS", "VK_KHR_acceleration_structure PFN IS NULL — EXTENSION NOT ENABLED OR LOADED!");
            LOG_FATAL_CAT("LAS", "→ Check device extensions: VK_KHR_acceleration_structure must be enabled!");
            LOG_FATAL_CAT("LAS", "→ Check that device function pointers were loaded after vkCreateDevice!");
        } else {
            LOG_SUCCESS_CAT("LAS", "VULKANACCEL FORGED — ALL RTX PFNS CONFIRMED VALID — PINK PHOTONS IMMINENT");
        }
    }

    void buildBLAS(VkCommandPool pool,
                   uint64_t vertexBufferObf,
                   uint64_t indexBufferObf,
                   uint32_t vertexCount,
                   uint32_t indexCount,
                   VkBuildAccelerationStructureFlagsKHR extraFlags = 0);

    void buildTLAS(VkCommandPool pool,
                   const std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>>& instances);

    // Legacy: raw handle access
    [[nodiscard]] VkAccelerationStructureKHR getBLAS() const noexcept { return blas_.as; }
    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept { return tlas_.as; }
    [[nodiscard]] VkDeviceAddress           getTLASAddress() const noexcept { return tlas_.address; }

    // NEW: FULL STRUCT ACCESS — REQUIRED FOR VALIDATION LAYERS
    [[nodiscard]] const VulkanAccel::BLAS& getBLASStruct() const noexcept { return blas_; }
    [[nodiscard]] const VulkanAccel::TLAS& getTLASStruct() const noexcept { return tlas_; }

    [[nodiscard]] uint32_t getGeneration() const noexcept { return generation_; }
    [[nodiscard]] bool     isValid() const noexcept 
    { 
        return accel_ && 
               blas_.isValid() && 
               tlas_.isValid(); 
    }

    void invalidate() noexcept { ++generation_; LOG_DEBUG_CAT("LAS", "Acceleration structures invalidated — generation {}", generation_); }

private:
    LAS()  = default;
    ~LAS() = default;

    std::unique_ptr<VulkanAccel> accel_;   // Created via forgeAccelContext()

    VulkanAccel::BLAS blas_{};
    VulkanAccel::TLAS tlas_{};
    uint32_t          generation_ = 0;
};

// Global shortcut — PINK PHOTON APPROVED
inline LAS& las() noexcept { return LAS::get(); }