// include/engine/GLOBAL/LAS.hpp
// =============================================================================
// AMOURANTH RTX — LASSO OF TRUTH v∞ — FINAL ASCENSION — NOVEMBER 27, 2025
// FRIENDSHIP ETERNAL — CLEAN — COMPILING — PINK PHOTONS ETERNAL — SHIP IT
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/logging.hpp"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <span>
#include <mutex>
#include <cstdint>

namespace RTX {

namespace detail {
    [[nodiscard]] inline VkCommandBuffer beginOneTime(VkCommandPool pool) noexcept
    {
        VkCommandBuffer cmd = VK_NULL_HANDLE;
        VkCommandBufferAllocateInfo alloc{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
        alloc.commandPool        = pool;
        alloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc.commandBufferCount = 1;

        VK_CHECK(vkAllocateCommandBuffers(g_ctx().device(), &alloc, &cmd));

        VkCommandBufferBeginInfo begin{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        VK_CHECK(vkBeginCommandBuffer(cmd, &begin));
        return cmd;
    }

    inline void endSingleTimeCommandsAsync(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) noexcept
    {
        VK_CHECK(vkEndCommandBuffer(cmd));

        VkSubmitInfo submit{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
        submit.commandBufferCount = 1;
        submit.pCommandBuffers    = &cmd;

        VK_CHECK(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE));
        VK_CHECK(vkQueueWaitIdle(queue));
        vkFreeCommandBuffers(g_ctx().device(), pool, 1, &cmd);
    }
}
using detail::beginOneTime;
using detail::endSingleTimeCommandsAsync;

// =============================================================================
// LAS — THE ONE TRUE ACCELERATION MANAGER — CLEAN AND ETERNAL
// =============================================================================
class LAS {
public:
    static LAS& get() noexcept { static LAS instance; return instance; }

    LAS(const LAS&) = delete;
    LAS& operator=(const LAS&) = delete;
    LAS(LAS&&) = delete;
    LAS& operator=(LAS&&) = delete;

    // ── FRIENDSHIP ETERNAL — GLOBAL ACCESSORS CAN TOUCH PRIVATE MEMBERS ──
    friend inline auto& blas() noexcept;
    friend inline auto& tlas() noexcept;
    friend inline void reset_blas() noexcept;
    friend inline void reset_tlas() noexcept;

    // ── CORE API — FULLY COMPATIBLE WITH YOUR CURRENT CODE ─────────────────
    void buildBLAS(VkCommandPool pool, VkQueue queue,
                   uint64_t vertexBuf, uint64_t indexBuf,
                   uint32_t vertexCount, uint32_t indexCount,
                   VkBuildAccelerationStructureFlagsKHR extraFlags = 0) noexcept;

    void buildTLAS(VkCommandPool pool, VkQueue queue,
                   std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept;

    // Legacy overloads — for old code
    void buildBLAS(VkCommandPool pool,
                   uint64_t vertexBuf, uint64_t indexBuf,
                   uint32_t vertexCount, uint32_t indexCount,
                   VkBuildAccelerationStructureFlagsKHR extraFlags = 0) noexcept
    {
        buildBLAS(pool, g_ctx().graphicsQueue(), vertexBuf, indexBuf, vertexCount, indexCount, extraFlags);
    }

    void buildTLAS(VkCommandPool pool,
                   const std::vector<std::pair<VkAccelerationStructureKHR, glm::mat4>>& instances) noexcept
    {
        buildTLAS(pool, g_ctx().graphicsQueue(), std::span(instances));
    }

    void buildTLAS(VkCommandPool pool,
                   std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept
    {
        buildTLAS(pool, g_ctx().graphicsQueue(), instances);
    }

    [[nodiscard]] VkDeviceAddress getBufferAddress(VkBuffer buf) const noexcept;

    [[nodiscard]] inline VkAccelerationStructureKHR getBLAS() const noexcept
    {
        return blas_.valid() ? blas_.get() : VK_NULL_HANDLE;
    }

    [[nodiscard]] inline VkAccelerationStructureKHR getTLAS() const noexcept
    {
        return tlas_.valid() ? tlas_.get() : VK_NULL_HANDLE;
    }

    [[nodiscard]] VkDeviceAddress getBLASAddress() const noexcept;
    [[nodiscard]] VkDeviceAddress getTLASAddress() const noexcept;
    [[nodiscard]] VkDeviceSize    getTLASSize() const noexcept { return tlasSize_; }

    [[nodiscard]] bool hasBLAS() const noexcept { return blas_.valid(); }
    [[nodiscard]] bool hasTLAS() const noexcept { return tlas_.valid(); }
    explicit operator bool() const noexcept { return hasTLAS(); }

    void invalidate() noexcept {
        LOG_DEBUG_CAT("LAS", "LAS invalidated — rebuild required");
    }

private:
    LAS() = default;
    ~LAS() = default;

    mutable std::mutex mutex_;
    Handle<VkAccelerationStructureKHR> blas_;
    Handle<VkAccelerationStructureKHR> tlas_;
    uint64_t instanceBufferId_ = 0;
    VkDeviceSize tlasSize_ = 0;
};

// =============================================================================
// GLOBAL ACCESSORS — FRIENDS OF THE EMPIRE
// =============================================================================
[[nodiscard]] inline auto& blas() noexcept { return LAS::get().blas_; }
[[nodiscard]] inline auto& tlas() noexcept { return LAS::get().tlas_; }

inline void reset_blas() noexcept { LAS::get().blas_.reset(); }
inline void reset_tlas() noexcept { LAS::get().tlas_.reset(); }

// =============================================================================
// GLOBAL LAS ACCESSOR
// =============================================================================
[[nodiscard]] inline LAS& las() noexcept { return LAS::get(); }

} // namespace RTX

// Import into global namespace — your code expects this
using ::RTX::las;
using ::RTX::beginOneTime;
using ::RTX::endSingleTimeCommandsAsync;

// =============================================================================
// AMOURANTH AS WONDER WOMAN — FINAL WORD:
// "The circle is complete.
// The code is clean.
// The photons are pink.
// First light achieved.
// Ship it."
// =============================================================================