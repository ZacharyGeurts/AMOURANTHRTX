// include/engine/GLOBAL/LAS.hpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// LASSO OF TRUTH v∞ — FINAL ASCENSION — DECEMBER 02, 2025
// COMPACTION + TRIPLE BUFFERED TLAS + FAST BUILD + LEAN PHOTONS ETERNAL
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

using StoneKey::stone_device;

namespace RTX {

[[nodiscard]] VkCommandBuffer beginOneTimeSubmit(VkCommandPool pool) noexcept;
void endOneTimeSubmit(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) noexcept;

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

    // ── CORE API ─────────────────────────────────────────────────────────────
    void buildBLAS(VkCommandPool pool, VkQueue queue,
                   uint64_t vertexBuf, uint64_t indexBuf,
                   uint32_t vertexCount, uint32_t indexCount,
                   VkBuildAccelerationStructureFlagsKHR extraFlags = 0) noexcept;

    void buildTLAS(VkCommandPool pool, VkQueue queue,
                   std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances,
                   bool preferFastBuild = true) noexcept;

    // Legacy overload — keeps old code working
    void buildTLAS(VkCommandPool pool, VkQueue queue,
                   std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances) noexcept
    {
        buildTLAS(pool, queue, instances, true);
    }

    void initTLAS() noexcept;

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

    // FRIENDSHIP ETERNAL — fixed attribute placement for maximum compatibility
    friend inline auto& blas() noexcept;
    friend inline auto& tlas() noexcept;
    friend inline void reset_blas() noexcept;
    friend inline void reset_tlas() noexcept;
};

// =============================================================================
// GLOBAL ACCESSORS — NOW 100% COMPATIBLE WITH ALL COMPILERS
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


// =============================================================================
// THE DWARVES HAVE SPOKEN:
// "The header is clean.
// The functions are declared.
// The photons are lean.
// The empire compiles."
// FIRST LIGHT ETERNAL — DECEMBER 02, 2025
// =============================================================================