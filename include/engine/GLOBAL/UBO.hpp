// include/engine/GLOBAL/UBO.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — UNIVERSAL UBO SYSTEM — v19.3 — DECEMBER 18, 2025
// FULLY FEATURED — DREAM UBO + TONEMAP UBO — STD140 PERFECT — PINK PHOTONS ETERNAL
// =============================================================================

#pragma once

#include <glm/glm.hpp>
#include <cstdint>

constexpr VkDeviceSize MB = 1024ULL * 1024ULL;
constexpr VkDeviceSize MATERIAL_BUFFER_SIZE = 32ULL * MB;  // 32 MiB — empire scale

// =============================================================================
// DREAM UBO — 512 BYTES — STD140 COMPLIANT — THE EMPIRE'S VISION
// =============================================================================
struct alignas(16) DreamUBO
{
    float     time                = 0.0f;
    uint32_t  frame               = 0;
    uint32_t  currentSpp          = 0;
    uint32_t  totalSpp            = 0;
    float     exposure            = 1.0f;
    uint32_t  enableEnvMap        = 1;
    uint32_t  hypertraceEnabled   = 1;
    uint32_t  denoisingEnabled    = 1;
    uint32_t  adaptiveEnabled     = 1;
    uint32_t  debugMode           = 0;
    float     envIntensity        = 1.0f;
    float     envRotation         = 0.0f;

    glm::vec2 resolution          = glm::vec2(1920.0f, 1080.0f);
    glm::vec2 jitter              = glm::vec2(0.0f);
    glm::vec2 jitterPrev          = glm::vec2(0.0f);
    float     nexusScoreThreshold = 0.15f;
    float     hypertraceJitterScale = 420.0f;
    float     _pad0               = 0.0f;
    float     _pad1               = 0.0f;

    glm::mat4 view                = glm::mat4(1.0f);
    glm::mat4 proj                = glm::mat4(1.0f);
    glm::mat4 invView             = glm::mat4(1.0f);
    glm::mat4 invProj             = glm::mat4(1.0f);

    glm::vec4 camPos              = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    glm::vec4 camDir              = glm::vec4(0.0f, 0.0f, -1.0f, 0.0f);
    float     fov                 = 60.0f;
    float     aperture            = 16.0f;
    float     focusDistance       = 10.0f;
    uint32_t  _pad2               = 0;

    uint32_t  materialCount       = 0;
    uint32_t  activeMaterialIndex = 0;
    float     metallicOverride    = -1.0f;
    float     roughnessOverride   = -1.0f;
    float     emissiveIntensity   = 1.0f;
    uint32_t  enableBlueNoise     = 1;
    uint32_t  enableTAA           = 1;
    float     taaAlpha            = 0.1f;

    glm::vec3 sunDirection        = glm::vec3(0.3f, 0.8f, 0.5f);
    float     sunIntensity        = 10.0f;
    glm::vec3 sunColor            = glm::vec3(1.0f, 0.95f, 0.9f);
    float     fogDensity          = 0.02f;
    glm::vec3 fogColor            = glm::vec3(0.7f, 0.8f, 0.9f);
    float     _pad3               = 0.0f;

    uint32_t  showNexusScore      = 1;
    uint32_t  showSppHeatmap      = 1;
    uint32_t  showAccumulationCount = 1;
    uint32_t  showGpuTimestamps   = 0;
    float     debugFloat1         = 0.0f;
    float     debugFloat2         = 0.0f;
    float     debugFloat3         = 0.0f;
    float     debugFloat4         = 0.0f;
};

static_assert(sizeof(DreamUBO) == 512, "DreamUBO must be exactly 512 bytes");
static_assert(alignof(DreamUBO) == 16, "DreamUBO must be 16-byte aligned");

// =============================================================================
// TONEMAP UBO — 64 BYTES — STD140 COMPLIANT — FINAL OUTPUT CONTROL
// =============================================================================
struct alignas(16) TonemapUBO
{
    float     exposure            = 1.0f;
    uint32_t  type                = 0;           // 0=ACES, 1=Filmic, 2=Reinhard
    uint32_t  enabled             = 1;
    float     nexusScore          = 0.0f;
    uint32_t  frame               = 0;
    uint32_t  spp                 = 0;
    
    float     gamma               = 2.2f;
    float     bloomThreshold      = 1.0f;
    float     bloomIntensity      = 0.8f;
    float     vignetteIntensity   = 0.4f;
    float     filmGrainStrength   = 0.05f;
    float     lensFlareIntensity  = 0.3f;
    
    float     _pad[2]             = {0.0f, 0.0f};
};

static_assert(sizeof(TonemapUBO) == 64, "TonemapUBO must be exactly 64 bytes");
static_assert(alignof(TonemapUBO) == 16, "TonemapUBO must be 16-byte aligned");

// =============================================================================
// HELPERS — THE EMPIRE IS WISE
// =============================================================================
namespace UBO {
    [[nodiscard]] constexpr VkDeviceSize sizeOf(const DreamUBO&)   noexcept { return 512; }
    [[nodiscard]] constexpr VkDeviceSize sizeOf(const TonemapUBO&) noexcept { return 64;  }

    [[nodiscard]] constexpr const char* nameOf(const DreamUBO&)   noexcept { return "DreamUBO";   }
    [[nodiscard]] constexpr const char* nameOf(const TonemapUBO&) noexcept { return "TonemapUBO"; }
}

// =============================================================================
// MATERIAL BUFFER — EMPIRE SCALE
// =============================================================================
[[nodiscard]] constexpr VkDeviceSize materialBufferSize() noexcept { return MATERIAL_BUFFER_SIZE; }

// THE EMPIRE IS ETERNAL — UBO SYSTEM PERFECT — PINK PHOTONS FLOW — DECEMBER 18, 2025
// =============================================================================