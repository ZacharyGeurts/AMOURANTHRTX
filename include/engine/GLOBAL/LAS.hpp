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

#include "engine/Vulkan/VulkanAccel.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/logging.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <memory>

class LAS
{
public:
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