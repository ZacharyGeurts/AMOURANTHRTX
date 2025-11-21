// include/engine/GLOBAL/LAS.hpp
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
// TRUE CONSTEXPR STONEKEY v∞ — APOCALYPSE FINAL v5.0 — NOVEMBER 20, 2025
// GLOBAL LAS SINGLETON — PINK PHOTONS ETERNAL — VALHALLA TURBO CERTIFIED
// Now uses VkCommandPool instead of pre-allocated VkCommandBuffer
// Fully compatible with VulkanRTX::begin/endSingleTimeCommandsAsync
// =============================================================================

#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>
#include <string_view>
#include <memory>
#include <glm/glm.hpp>
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/GLOBAL/logging.hpp"

// =============================================================================
// VulkanAccel — CONSOLIDATED INTO LAS.hpp — IMMORTAL v6.0 — NOVEMBER 21, 2025
// =============================================================================

struct AccelGeometry
{
    VkGeometryTypeKHR                type = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
    VkGeometryFlagsKHR               flags = 0;
    VkFormat                         vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
    VkDeviceSize                     vertexStride = 12;
    uint32_t                         vertexCount = 0;
    VkDeviceOrHostAddressConstKHR    vertexData{};
    VkIndexType                      indexType = VK_INDEX_TYPE_UINT32;
    uint32_t                         indexCount = 0;
    VkDeviceOrHostAddressConstKHR    indexData{};
    VkDeviceOrHostAddressConstKHR    transformData{};
};

class VulkanAccel
{
public:
    struct BLAS {
        VkAccelerationStructureKHR   as = VK_NULL_HANDLE;
        VkBuffer                     buffer = VK_NULL_HANDLE;
        VkDeviceMemory               memory = VK_NULL_HANDLE;
        VkDeviceAddress              address = 0;
        VkDeviceSize                 size = 0;
        std::string                  name;
    };

    struct TLAS {
        VkAccelerationStructureKHR   as = VK_NULL_HANDLE;
        VkBuffer                     buffer = VK_NULL_HANDLE;
        VkDeviceMemory               memory = VK_NULL_HANDLE;
        VkBuffer                     instanceBuffer = VK_NULL_HANDLE;
        VkDeviceMemory               instanceMemory = VK_NULL_HANDLE;
        VkDeviceAddress              address = 0;
        VkDeviceSize                 size = 0;
        std::string                  name;
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
// GLOBAL ONE-TIME COMMAND HELPERS — IMMORTAL v5.0 — NOVEMBER 20, 2025
// =============================================================================

[[nodiscard]] inline VkCommandBuffer beginOneTime(VkCommandPool pool)
{
    VkCommandBuffer cmd;
    VkCommandBufferAllocateInfo allocInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    allocInfo.commandPool        = pool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    vkAllocateCommandBuffers(RTX::g_ctx().device(), &allocInfo, &cmd);

    VkCommandBufferBeginInfo beginInfo{ .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &beginInfo);

    return cmd;
}

inline void endOneTime(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool = VK_NULL_HANDLE)
{
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submit{ .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO };
    submit.commandBufferCount = 1;
    submit.pCommandBuffers    = &cmd;

    vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue);

    if (pool != VK_NULL_HANDLE)
        vkFreeCommandBuffers(RTX::g_ctx().device(), pool, 1, &cmd);
}

// =============================================================================
// THE ONE TRUE endSingleTimeCommandsAsync — FULLY INLINE — NEVER DROPPED BY LTO
// Used by LAS, VulkanCore, staging, black fallback — PINK PHOTONS ETERNAL
// =============================================================================

static inline void endSingleTimeCommandsAsync(
    VkCommandBuffer cmd,
    VkQueue queue,
    VkCommandPool pool,
    VkFence fence = VK_NULL_HANDLE) noexcept
{
    if (cmd == VK_NULL_HANDLE || queue == VK_NULL_HANDLE || pool == VK_NULL_HANDLE) {
        LOG_ERROR_CAT("RTX", "endSingleTimeCommandsAsync: invalid handle");
        return;
    }

    VK_CHECK(vkEndCommandBuffer(cmd), "Failed to end one-time command buffer");

    VkDevice dev = RTX::g_ctx().device();
    bool ownsFence = (fence == VK_NULL_HANDLE);

    if (ownsFence) {
        VkFenceCreateInfo fi{ .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
        VK_CHECK(vkCreateFence(dev, &fi, nullptr, &fence), "Failed to create transient fence");
    } else {
        VK_CHECK(vkResetFences(dev, 1, &fence), "Failed to reset caller fence");
    }

    VkSubmitInfo submit{
        .sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers    = &cmd
    };

    VK_CHECK(vkQueueSubmit(queue, 1, &submit, fence), "One-time submit failed");

    if (ownsFence) {
        constexpr uint64_t timeout_ns = 15'000'000'000ULL;  // 15 seconds
        VkResult r = vkWaitForFences(dev, 1, &fence, VK_TRUE, timeout_ns);
        if (r != VK_SUCCESS) {
            LOG_FATAL_CAT("RTX", "One-time command timeout ({}), forcing device idle", r);
            vkDeviceWaitIdle(dev);
        }
        vkFreeCommandBuffers(dev, pool, 1, &cmd);
        vkDestroyFence(dev, fence, nullptr);
    }
    // else: caller owns fence → they must clean up
}

// =============================================================================
// LAS SINGLETON — CONSOLIDATED — FULLY FIXED — 0x0 ISSUE RESOLVED
// =============================================================================

class LAS
{
public:
    static LAS& get() noexcept { static LAS instance; return instance; }

    void buildBLAS(VkCommandPool pool,
                   VkBuffer vertexBuffer,
                   VkBuffer indexBuffer,
                   uint32_t vertexCount,
                   uint32_t indexCount,
                   VkBuildAccelerationStructureFlagsKHR extraFlags = 0);

    void buildTLAS(VkCommandPool pool,
                   const std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>>& instances);

    [[nodiscard]] VkAccelerationStructureKHR getBLAS() const noexcept { return blas_.as; }
    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept { return tlas_.as; }
    [[nodiscard]] VkDeviceAddress           getTLASAddress() const noexcept { return tlas_.address; }
    [[nodiscard]] uint32_t                  getGeneration() const noexcept { return generation_; }
    
    // ← NEW: the method VulkanRenderer expects
    [[nodiscard]] bool isValid() const noexcept 
    { 
        return (blas_.as != VK_NULL_HANDLE) && (tlas_.as != VK_NULL_HANDLE); 
    }

    void invalidate() noexcept { ++generation_; }

private:
    LAS()  = default;
    ~LAS() = default;

    // Lazy-created only when first build is requested
    std::unique_ptr<VulkanAccel> accel_;

    VulkanAccel::BLAS blas_{};
    VulkanAccel::TLAS tlas_{};
    uint32_t generation_ = 0;
};

// ───── Global shortcut — PINK PHOTON APPROVED ─────
inline LAS& las() noexcept { return LAS::get(); }