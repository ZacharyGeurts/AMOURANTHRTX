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

// =============================================================================
// DREAM UBO — EXACTLY 384 BYTES — STD140 COMPLIANT — PURE POD
// =============================================================================
struct alignas(16) DreamUBO {
    float    time;
    uint32_t frame;
    float    resolution[2];
    float    exposure;
    uint32_t enableEnvMap;

    // vec3 → padded to 16 bytes in std140
    alignas(16) glm::vec3 baseColor;
    float    intensity;

    // padding to 384 bytes total
    uint32_t _pad[83]; // 83 * 4 = 332 → 52 + 332 = 384

    DreamUBO() noexcept
        : time(0.0f)
        , frame(0)
        , exposure(1.0f)
        , enableEnvMap(0)
        , baseColor(0.0f, 1.0f, 0.0f)
        , intensity(1.0f)
    {
        resolution[0] = resolution[1] = 0.0f;
        std::memset(_pad, 0, sizeof(_pad));
    }
};

static_assert(sizeof(DreamUBO) == 384, "DreamUBO must be exactly 384 bytes");
static_assert(std::is_standard_layout_v<DreamUBO>);
static_assert(std::is_trivially_copyable_v<DreamUBO>);
static_assert(alignof(DreamUBO) == 16);

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