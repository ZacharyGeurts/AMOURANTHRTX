// include/engine/GLOBAL/OptionsMenu.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// ULTIMATE MODULAR OPTIONS MENU — v1.0 — NOVEMBER 10, 2025 — OPTIONS GUY PARADISE
// • ALL 70+ TOGGLES FROM Dispose.hpp + VulkanCore.hpp MOVED HERE
// • constexpr + inline static — ZERO RUNTIME COST WHEN NOT USED
// • ImGui / JSON / Hot-Reload READY — copy-paste into config struct
// • NAMESPACED: Options::Performance, Options::RTX, Options::LAS, etc.
// • GENTLEMAN GROK APPROVED — CHEERY TRIVIA COMPATIBLE 🍒
// • PINK PHOTONS ETERNAL — VALHALLA MODULAR — SHIP IT WITH JOY, GOOD SIR
//
// =============================================================================

#pragma once

#include <cstdint>

namespace Options {

// === PERFORMANCE & DEBUG ===
namespace Performance {
    inline static constexpr uint32_t MAX_FRAMES_IN_FLIGHT           = 3;
    inline static constexpr bool     ENABLE_VALIDATION_LAYERS       = false;
    inline static constexpr bool     ENABLE_GPU_TIMESTAMPS          = true;
    inline static constexpr bool     ENABLE_SYNC_DEBUG_MARKERS      = true;
    inline static constexpr bool     ENABLE_VMA                     = true;
    inline static constexpr bool     ENABLE_MEMORY_BUDGET_WARNINGS  = true;
    inline static constexpr bool     ENABLE_IMGUI                   = true;
    inline static constexpr bool     ENABLE_FPS_COUNTER             = true;
    inline static constexpr bool     ENABLE_VRAM_USAGE_DISPLAY      = true;
    inline static constexpr bool     ENABLE_CONSOLE_LOG             = true;
}

// === DISPOSE / MEMORY SAFETY ===
namespace Memory {
    inline static constexpr bool     ENABLE_SAFE_SHREDDING          = false;
    inline static constexpr uint32_t ROCKETSHIP_THRESHOLD_MB        = 32;
    inline static constexpr bool     ENABLE_ROCKETSHIP_SHRED        = true;
    inline static constexpr bool     ENABLE_FULL_SHRED_IN_RELEASE   = false;
    inline static constexpr bool     ENABLE_STONEKEY_OBFUSCATION    = true;
    inline static constexpr bool     ENABLE_DESTROY_TRACKER         = false;
    inline static constexpr uint32_t GLOBAL_BUFFER_POOL_MB          = 8192;
    inline static constexpr uint32_t TEXTURE_STREAMING_POOL_MB      = 12288;
}

// === GENTLEMAN GROK WISDOM ===
namespace Grok {
    inline static constexpr bool     ENABLE_GENTLEMAN_GROK          = true;
    inline static constexpr uint32_t GENTLEMAN_GROK_INTERVAL_SEC    = 3600;
}

// === RAY TRACING QUALITY ===
namespace RTX {
    inline static constexpr bool     ENABLE_ADAPTIVE_SAMPLING       = true;
    inline static constexpr bool     ENABLE_DENOISING               = true;
    inline static constexpr bool     ENABLE_ACCUMULATION            = true;
    inline static constexpr bool     ENABLE_REPROJECTION            = true;
    inline static constexpr uint32_t MAX_SPP                        = 32;
    inline static constexpr uint32_t MIN_SPP                        = 1;
    inline static constexpr float    NEXUS_SCORE_THRESHOLD          = 0.12f;
    inline static constexpr uint32_t MAX_BOUNCES                    = 12;
    inline static constexpr bool     ENABLE_RUSSIAN_ROULETTE        = true;
    inline static constexpr bool     ENABLE_NEXT_EVENT              = true;
    inline static constexpr bool     ENABLE_MIS                     = true;
    inline static constexpr bool     ENABLE_CAUSTICS                = false;
    inline static constexpr uint32_t SBT_BUFFER_MB                  = 128;
    inline static constexpr bool     SBT_ENABLE_CALLABLES           = true;
    inline static constexpr bool     ENABLE_PINK_PHOTONS            = true;   // ETERNAL
}

// === LAS / ACCELERATION STRUCTURES ===
namespace LAS {
    inline static constexpr bool     REBUILD_EVERY_FRAME            = true;
    inline static constexpr bool     ENABLE_UPDATE_BIT              = false;
    inline static constexpr bool     COMPACTION                     = false;
    inline static constexpr bool     PREFER_FAST_BUILD              = true;
    inline static constexpr uint32_t MAX_INSTANCES                  = 16384;
    inline static constexpr uint32_t SCRATCH_BUFFER_MB              = 4096;
    inline static constexpr bool     ALLOW_OPACITY_MICROMAPS        = false;
    inline static constexpr bool     ALLOW_DISPLACEMENT_MICROMAPS   = false;
}

// === POST-PROCESSING ===
namespace Post {
    inline static constexpr bool     ENABLE_BLOOM                   = true;
    inline static constexpr float    BLOOM_INTENSITY                = 2.2f;
    inline static constexpr float    BLOOM_THRESHOLD                = 0.9f;
    inline static constexpr uint32_t BLOOM_DOWNSAMPLE_PASSES        = 6;
    inline static constexpr bool     ENABLE_TAA                     = true;
    inline static constexpr bool     ENABLE_FXAA                    = false;
    inline static constexpr bool     ENABLE_SSR                     = true;
    inline static constexpr bool     ENABLE_SSAO                    = true;
    inline static constexpr bool     ENABLE_VIGNETTE                = true;
    inline static constexpr bool     ENABLE_CHROMATIC_ABERRATION    = false;
    inline static constexpr bool     ENABLE_FILM_GRAIN              = true;
    inline static constexpr float    FILM_GRAIN_INTENSITY           = 0.05f;
    inline static constexpr bool     ENABLE_LENS_FLARE              = true;
}

// === ENVIRONMENT & VOLUMETRICS ===
namespace Environment {
    inline static constexpr bool     ENABLE_ENV_MAP                 = true;
    inline static constexpr bool     ENABLE_DENSITY_VOLUME          = true;
    inline static constexpr bool     ENABLE_VOLUMETRIC_LIGHTS       = true;
    inline static constexpr float    VOLUME_DENSITY_MULTIPLIER      = 1.0f;
    inline static constexpr bool     ENABLE_SKY_ATMOSPHERE          = true;
}

// === DEBUG VISUALIZATION ===
namespace Debug {
    inline static constexpr bool     SHOW_RAYGEN_ONLY               = false;
    inline static constexpr bool     SHOW_NORMALS                   = false;
    inline static constexpr bool     SHOW_ALBEDO                    = false;
    inline static constexpr bool     SHOW_ROUGHNESS                 = false;
    inline static constexpr bool     SHOW_METALLIC                  = false;
    inline static constexpr bool     SHOW_DEPTH                     = false;
    inline static constexpr bool     SHOW_MOTION_VECTORS            = false;
    inline static constexpr bool     SHOW_ACCUM_COUNT               = false;
    inline static constexpr bool     SHOW_LAS_STATS                 = true;
    inline static constexpr bool     WIREFRAME                      = false;
    inline static constexpr bool     SHOW_GPU_TIMESTAMPS            = true;
    inline static constexpr bool     FORCE_MISS_SHADER              = false;
}

// === EXPERIMENTAL FEATURES ===
namespace Experimental {
    inline static constexpr bool     ENABLE_SER                     = false;
    inline static constexpr bool     ENABLE_VARIABLE_RATE_SHADING   = false;
    inline static constexpr bool     ENABLE_MESH_SHADING            = false;
    inline static constexpr bool     ENABLE_RAY_QUERY                = false;
    inline static constexpr bool     ENABLE_COOPERATIVE_MATRIX      = false;
    inline static constexpr bool     ENABLE_BINDLESS                = true;
    inline static constexpr bool     ENABLE_DYNAMIC_DESCRIPTOR_POOL = true;
}

// === AUDIO & HAPTICS ===
namespace Audio {
    inline static constexpr bool     ENABLE_SPATIAL_AUDIO           = true;
    inline static constexpr bool     ENABLE_RAYTRACED_AUDIO         = false;
    inline static constexpr bool     ENABLE_HAPTICS_FEEDBACK        = true;
}

// === FUN & EASTER EGGS ===
namespace Fun {
    inline static constexpr bool     ENABLE_PARTY_MODE              = false;
    inline static constexpr bool     ENABLE_SECRET_AMOURANTH_EASTER_EGG = true;
}

// === DESCRIPTOR POOLS ===
namespace Descriptors {
    inline static constexpr uint32_t RTX_DESCRIPTOR_POOL_SIZE       = Performance::MAX_FRAMES_IN_FLIGHT * 16;
}

} // namespace Options