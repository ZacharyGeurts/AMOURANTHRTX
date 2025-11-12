// engine/GLOBAL/LAS.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// LAS.hpp — LIGHT_WARRIORS_LAS → LAS — NOV 12 2025 6:00 AM EST
// • FULLY IN NAMESPACE RTX
// • RTX::Handle<T> — NO GLOBALS
// • g_ctx → RTX::ctx()
// • FASTER COMPILES — PRODUCTION READY
// =============================================================================

#pragma once

#include "engine/GLOBAL/RTXHandler.hpp"      // RTX::Handle, RTX::ctx(), BUFFER_*
#include "engine/GLOBAL/logging.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <span>
#include <mutex>

namespace RTX {

struct BlasBuildSizes {
    VkDeviceSize accelerationStructureSize = 0;
    VkDeviceSize buildScratchSize = 0;
    VkDeviceSize updateScratchSize = 0;
};

struct TlasBuildSizes {
    VkDeviceSize accelerationStructureSize = 0;
    VkDeviceSize buildScratchSize = 0;
    VkDeviceSize updateScratchSize = 0;
    VkDeviceSize instanceDataSize = 0;
};

// =============================================================================
// LAS — LIGHT_WARRIORS_LAS → LAS — SINGLETON IN RTX
// =============================================================================
class LAS {
public:
    static LAS& get() noexcept {
        static LAS instance;
        return instance;
    }

    LAS(const LAS&) = delete;
    LAS& operator=(const LAS&) = delete;

    void buildBLAS(VkCommandPool pool, VkQueue queue,
                   uint64_t vertexBuf, uint64_t indexBuf,
                   uint32_t vertexCount, uint32_t indexCount,
                   VkBuildAccelerationStructureFlagsKHR flags = 0);

    void buildTLAS(VkCommandPool pool, VkQueue queue,
                   std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances);

    void rebuildTLAS(VkCommandPool pool, VkQueue queue,
                     std::span<const std::pair<VkAccelerationStructureKHR, glm::mat4>> instances);

    [[nodiscard]] VkAccelerationStructureKHR getBLAS() const noexcept;
    [[nodiscard]] VkDeviceAddress getBLASAddress() const noexcept;
    [[nodiscard]] VkAccelerationStructureKHR getTLAS() const noexcept;
    [[nodiscard]] VkDeviceAddress getTLASAddress() const noexcept;
    [[nodiscard]] VkDeviceSize getTLASSize() const noexcept;

private:
    LAS();
    ~LAS() = default;

    mutable std::mutex mutex_;
    RTX::Handle<VkAccelerationStructureKHR> blas_;
    RTX::Handle<VkAccelerationStructureKHR> tlas_;
    uint64_t instanceBufferId_ = 0;
    VkDeviceSize tlasSize_ = 0;
};

// =============================================================================
// GLOBAL ALIASES — LAS::get() → RTX::LAS::get()
// =============================================================================
inline LAS& las() noexcept { return LAS::get(); }

} // namespace RTX

// =============================================================================
// LAS v63 FINAL — LIGHT_WARRIORS_LAS → LAS — NAMESPACE SUPREMACY
// RTX::ctx(), RTX::Handle<T> — PINK PHOTONS ETERNAL
// =============================================================================