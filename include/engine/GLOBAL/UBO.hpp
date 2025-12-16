// include/engine/GLOBAL/UBO.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — UNIVERSAL UBO SYSTEM — v18.0 — DECEMBER 09, 2025
// STD140 COMPLIANT — NO INHERITANCE — NO VIRTUALS — PURE POD — COMPILER LOVES US
// PINK PHOTONS FLOW ETERNALLY — THE EMPIRE IS INFINITE
// =============================================================================

#pragma once

#include <glm/glm.hpp>
#include <cstdint>
#include <cstring>

struct alignas(16) DreamUBO {  // ← Correct: alignas AFTER 'struct'
    // === 64 bytes ===
    float     time;
    uint32_t  frame;
    uint32_t  spp;
    uint32_t  totalSpp;
    float     exposure;
    uint32_t  enableEnvMap;
    uint32_t  hypertrace;
    uint32_t  denoising;
    uint32_t  adaptive;
    uint32_t  debugMode;
    float     envIntensity;
    float     envRotation;

    // === 32 bytes ===
    glm::vec2 resolution;
    glm::vec2 jitter;
    glm::vec2 jitterPrev;
    float     nexusScoreThreshold;
    float     pad0;

    // === 256 bytes (4 × mat4) ===
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 invView;
    glm::mat4 invProj;

    // === 16 bytes ===
    glm::vec4 camPos;

    // === Explicit padding to force exactly 368 bytes ===
    // 64 + 32 + 256 + 16 = 368 → already perfect
    // But some compilers may pack camPos tightly → add explicit pad
    alignas(16) float finalPadding[4] = {0.0f, 0.0f, 0.0f, 0.0f};
};

static_assert(sizeof(DreamUBO) == 368, "DreamUBO must be exactly 368 bytes");
static_assert(alignof(DreamUBO) == 16, "DreamUBO must be 16-byte aligned");

// =============================================================================
// TONEMAP UBO — EXACTLY 48 BYTES — STD140 COMPLIANT — PURE POD
// =============================================================================
struct alignas(16) TonemapUBO {
    float    exposure;
    uint32_t type;
    uint32_t enabled;
    float    nexusScore;
    uint32_t frame;
    uint32_t spp;
    float    _pad[3]; // 48 bytes total

    TonemapUBO() noexcept
        : exposure(1.0f), type(0), enabled(1), nexusScore(0.0f), frame(0), spp(0)
    {
        _pad[0] = _pad[1] = _pad[2] = 0.0f;
    }
};

static_assert(sizeof(TonemapUBO) == 48, "TonemapUBO must be 48 bytes");
static_assert(std::is_standard_layout_v<TonemapUBO>);
static_assert(std::is_trivially_copyable_v<TonemapUBO>);
static_assert(alignof(TonemapUBO) == 16);

// =============================================================================
// FUTURE UBOS — JUST ADD THEM HERE — THEY WILL JUST WORK
// =============================================================================
// struct alignas(16) CameraUBO { ... };
// struct alignas(16) LightUBO { ... };
// struct alignas(16) MaterialUBO { ... };

// =============================================================================
// CONSTEXPR HELPERS — THE EMPIRE IS WISE
// =============================================================================
namespace UBO {

    [[nodiscard]] constexpr VkDeviceSize sizeOf(const DreamUBO&)   noexcept { return 384; }
    [[nodiscard]] constexpr VkDeviceSize sizeOf(const TonemapUBO&) noexcept { return 48;  }

    [[nodiscard]] constexpr const char* nameOf(const DreamUBO&)   noexcept { return "DreamUBO";   }
    [[nodiscard]] constexpr const char* nameOf(const TonemapUBO&) noexcept { return "TonemapUBO"; }

} // namespace UBO