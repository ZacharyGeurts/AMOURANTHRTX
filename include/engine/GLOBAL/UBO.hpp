// include/engine/GLOBAL/UBO.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — UNIVERSAL UBO SYSTEM — v18.3 — DECEMBER 16, 2025
// FULL STD140 COMPLIANT — EXACT SIZES — NO COMPILER SURPRISES — MATERIAL SIZE INCLUDED
// PINK PHOTONS FLOW ETERNALLY — THE EMPIRE IS INFINITE
// =============================================================================

#pragma once

#include <glm/glm.hpp>
#include <cstdint>

constexpr VkDeviceSize MB = 1024ULL * 1024ULL;
constexpr VkDeviceSize MATERIAL_BUFFER_SIZE = 16ULL * MB;  // 16 MiB — eternal

// =============================================================================
// DREAM UBO — EXACTLY 352 BYTES — STD140 COMPLIANT
// =============================================================================
struct alignas(16) DreamUBO
{
    // Time & Frame Data — 64 bytes
    float     time           = 0.0f;
    uint32_t  frame          = 0;
    uint32_t  spp            = 0;
    uint32_t  totalSpp       = 0;
    float     exposure       = 1.0f;
    uint32_t  enableEnvMap   = 0;
    uint32_t  hypertrace     = 0;
    uint32_t  denoising      = 0;
    uint32_t  adaptive       = 0;
    uint32_t  debugMode      = 0;
    float     envIntensity   = 1.0f;
    float     envRotation    = 0.0f;

    // Resolution & Jitter — 32 bytes
    glm::vec2 resolution     = glm::vec2(1920.0f, 1080.0f);
    glm::vec2 jitter         = glm::vec2(0.0f);
    glm::vec2 jitterPrev     = glm::vec2(0.0f);
    float     nexusScoreThreshold = 0.0f;
    float     _pad0          = 0.0f;

    // Matrices — 256 bytes
    glm::mat4 view           = glm::mat4(1.0f);
    glm::mat4 proj           = glm::mat4(1.0f);
    glm::mat4 invView        = glm::mat4(1.0f);
    glm::mat4 invProj        = glm::mat4(1.0f);

    // Camera Position — 16 bytes
    glm::vec4 camPos         = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
};

static_assert(sizeof(DreamUBO) == 352, "DreamUBO must be exactly 352 bytes (STD140)");
static_assert(alignof(DreamUBO) == 16, "DreamUBO must be 16-byte aligned");

// =============================================================================
// TONEMAP UBO — EXACTLY 32 BYTES — STD140 COMPLIANT
// =============================================================================
struct alignas(16) TonemapUBO
{
    float    exposure   = 1.0f;
    uint32_t type       = 0;
    uint32_t enabled    = 1;
    float    nexusScore = 0.0f;
    uint32_t frame      = 0;
    uint32_t spp        = 0;
    float    _pad[2]    = {0.0f, 0.0f};

    TonemapUBO() noexcept = default;
};

static_assert(sizeof(TonemapUBO) == 32, "TonemapUBO must be exactly 32 bytes (STD140)");
static_assert(alignof(TonemapUBO) == 16, "TonemapUBO must be 16-byte aligned");

// =============================================================================
// CONSTEXPR HELPERS — THE EMPIRE IS WISE
// =============================================================================
namespace UBO {

    [[nodiscard]] constexpr VkDeviceSize sizeOf(const DreamUBO&)   noexcept { return 352; }
    [[nodiscard]] constexpr VkDeviceSize sizeOf(const TonemapUBO&) noexcept { return 32;  }

    [[nodiscard]] constexpr const char* nameOf(const DreamUBO&)   noexcept { return "DreamUBO";   }
    [[nodiscard]] constexpr const char* nameOf(const TonemapUBO&) noexcept { return "TonemapUBO"; }

} // namespace UBO

// =============================================================================
// MATERIAL BUFFER SIZE — ETERNAL AND SHARED
// =============================================================================
[[nodiscard]] constexpr VkDeviceSize materialBufferSize() noexcept { return MATERIAL_BUFFER_SIZE; }

// THE EMPIRE IS ETERNAL — PINK PHOTONS FLOW — DECEMBER 16, 2025
// =============================================================================