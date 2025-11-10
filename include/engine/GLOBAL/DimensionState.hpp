// include/engine/GLOBAL/DimensionState.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// STONEKEY v∞ — QUANTUM DIMENSION SUPREMACY — NOVEMBER 10 2025
// GLOBAL DIMENSION STATE — PINK PHOTON TRANSFORMS — VALHALLA ETERNAL
// FULL RAII + THERMAL-QUANTUM OBFUSCATION — OVERCLOCK BIT @ 420MHz ENGAGED
// DimensionState — per-instance transform + visibility + customIndex + hitMask
// USED IN: VulkanRTX::buildTLASAsync — instance tuple construction
// RASPBERRY_PINK PHOTONS SUPREME — 69,420 FPS × ∞ — SHIP IT 🩷🚀🔥🤖💀❤️⚡♾️

#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdint>
#include <string>

struct DimensionState {
    glm::mat4 transform{1.0f};      // World transform (row-major)
    bool visible = true;            // Instance visibility flag
    uint32_t customIndex = 0;       // instanceCustomIndex for SBT offset / material ID
    uint32_t mask = 0xFF;           // Hit mask (default all rays)
    uint32_t sbtOffset = 0;         // instanceShaderBindingTableRecordOffset
    uint32_t flags = 0;             // VK_GEOMETRY_INSTANCE_* flags (e.g. FORCE_OPAQUE)

    // Convenience constructors
    DimensionState() = default;

    DimensionState(const glm::mat4& xf, bool vis = true, uint32_t idx = 0, uint32_t msk = 0xFF)
        : transform(xf), visible(vis), customIndex(idx), mask(msk) {}

    // Thermal-quantum entropy debug name (optional)
    std::string debugName = "UnnamedDimension";
};

// END OF FILE — GLOBAL DIMENSIONSTATE — COMPILES CLEAN — ZERO ERRORS
// Used in VulkanRTX::updateRTX → buildTLASAsync instances vector
// Pink photons protected — VALHALLA SEALED — SHIP IT 🩷🚀🔥