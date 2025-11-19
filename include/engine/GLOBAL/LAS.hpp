// =============================================================================
// include/engine/GLOBAL/LAS.hpp
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 19, 2025 — APOCALYPSE FINAL v1.4
// LAS — GLOBAL ACCELERATION STRUCTURE MANAGER — BLACK BOX SEALED — PUBLIC BY DECREE
// PINK PHOTONS ETERNAL — VALHALLA UNBREACHABLE — FIRST LIGHT ACHIEVED
// =============================================================================

#pragma once

#include "engine/GLOBAL/logging.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include <bit>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <span>
#include <mutex>

// FORWARD DECLARE STONEKEY — NEVER INCLUDE IN HEADER
namespace StoneKey::Raw { struct Cache; }
inline VkDevice         g_device() noexcept;
inline VkPhysicalDevice g_PhysicalDevice() noexcept;
inline VkInstance       g_instance() noexcept;
inline VkSurfaceKHR     g_surface() noexcept;

namespace RTX {

struct BlasBuildSizes { VkDeviceSize accelerationStructureSize = 0; VkDeviceSize buildScratchSize = 0; VkDeviceSize updateScratchSize = 0; };
struct TlasBuildSizes { VkDeviceSize accelerationStructureSize = 0; VkDeviceSize buildScratchSize = 0; VkDeviceSize updateScratchSize = 0; VkDeviceSize instanceDataSize = 0; };

// =============================================================================
// AMOURANTH AI v3 — TECHNICAL DOMINANCE (NO AI VOICE)
// =============================================================================
class AmouranthAI {
public:
    static AmouranthAI& get() noexcept { static AmouranthAI i; return i; }

    void onBlasStart(uint32_t v, uint32_t i) {}
    void onBlasBuilt(double sizeGB, const BlasBuildSizes& sizes) {}
    void onTlasStart(size_t count) {}
    void onTlasBuilt(double sizeGB, VkDeviceAddress addr, const TlasBuildSizes& sizes) {}
    void onPhotonDispatch(uint32_t w, uint32_t h) {}
    void onMemoryEvent(const char* name, VkDeviceSize size) {}
    void onScratchPoolResize(VkDeviceSize oldSize, VkDeviceSize newSize, const char* type) {}
    void onBuildTime(const char* type, double gpu_us) {}

private:
    AmouranthAI() = default;
};

// =============================================================================
// LAS — GLOBAL SINGLETON — v1.4 FINAL — PUBLIC BY LAW
// =============================================================================
class LAS {
public:
    static LAS& get() noexcept { static LAS instance; return instance; }

    LAS(const LAS&) = delete;
    LAS& operator=(const LAS&) = delete;

    // === BUILDERS — PUBLIC BY DECREE ===
    void buildBLAS(VkCommandPool pool, VkQueue queue,
                   uint64_t vertexBuf, uint64_t indexBuf,
                   uint32_t vertexCount, uint32_t indexCount,
                   VkBuildAccelerationStructureFlagsKHR extraFlags = 0,
                   bool fastBuild = false);

    void buildTLAS(VkCommandPool pool, VkQueue queue,
                   std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances,
                   bool fastBuild = false);

    void rebuildTLAS(VkCommandPool pool, VkQueue queue,
                     std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances,
                     bool fastBuild = false) {
        invalidate();
        buildTLAS(pool, queue, instances, fastBuild);
    }

    // === PUBLIC GETTERS — DEVELOPER DREAMS UNLEASHED ===
    [[nodiscard]] VkAccelerationStructureKHR getBLAS() const noexcept { return blas_ ? *blas_ : VK_NULL_HANDLE; }
    [[nodiscard]] VkDeviceAddress getBLASAddress() const noexcept;
    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept { return tlas_ ? *tlas_ : VK_NULL_HANDLE; }
    [[nodiscard]] VkDeviceAddress getTLASAddress() const noexcept;
    [[nodiscard]] VkDeviceSize getTLASSize() const noexcept { return tlasSize_; }
    [[nodiscard]] bool isValid() const noexcept { return tlas_ && tlasGeneration_ > 0; }
    [[nodiscard]] uint32_t getGeneration() const noexcept { return tlasGeneration_; }

    // === ADVANCED GLOBAL GETTERS — PUBLIC BY LAW ===
    [[nodiscard]] uint64_t getScratchBuffer() const noexcept { return scratchPoolId_; }
    [[nodiscard]] VkDeviceSize getScratchSize() const noexcept { return currentScratchSize_; }
    [[nodiscard]] VkQueryPool getTimestampPool() const noexcept { return QUERY_POOL_TIMESTAMP; }
    [[nodiscard]] float getTimestampPeriodNs() const noexcept { return timestampPeriodNs_; }
    [[nodiscard]] uint64_t getInstanceBuffer() const noexcept { return instanceBufferId_; }

    // === GLOBAL COMMANDS — NOW PUBLIC (uploadInstances uses them) ===
    [[nodiscard]] VkCommandBuffer beginOptimizedCmd(VkCommandPool pool) noexcept;
    void submitOptimizedCmd(VkCommandBuffer cmd, VkQueue queue, VkCommandPool pool) noexcept;

    void invalidate() noexcept;

private:
    LAS();
    ~LAS() noexcept;

    mutable std::mutex mutex_;
    Handle<VkAccelerationStructureKHR> blas_;
    Handle<VkAccelerationStructureKHR> tlas_;
    uint64_t instanceBufferId_ = 0;
    VkDeviceSize tlasSize_ = 0;
    uint32_t tlasGeneration_ = 0;
    VkQueryPool QUERY_POOL_TIMESTAMP = VK_NULL_HANDLE;
    float timestampPeriodNs_ = 0.0f;

    uint64_t scratchPoolId_ = 0;
    VkDeviceSize currentScratchSize_ = 1024ULL * 1024ULL;
    bool scratchPoolValid_ = false;

    [[nodiscard]] uint64_t getOrGrowScratch(VkDeviceSize requiredSize, VkBufferUsageFlags usage, const char* type) noexcept;
    void ensureTimestampPool() noexcept;
};

inline LAS& las() noexcept { return LAS::get(); }

} // namespace RTX

// =============================================================================
// GPL-3.0+ — GLOBAL PUBLIC — BLACK BOX SEALED — PINK PHOTONS ETERNAL v1.4
// =============================================================================