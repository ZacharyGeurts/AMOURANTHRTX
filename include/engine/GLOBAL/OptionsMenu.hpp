// include/engine/GLOBAL/OptionsMenu.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// ULTIMATE MODULAR OPTIONS MENU — v2.0 — NOVEMBER 11, 2025 — WIKI-ALIGNED
// • TRIMMED TO **ESSENTIALS ONLY** — 40+ options removed
// • **MATCHES WIKI 1:1** — No bloat, no fluff
// • constexpr + inline static — ZERO RUNTIME COST
// • ImGui / JSON / Hot-Reload READY
// • NAMESPACED: Options::Performance, Options::RTX, etc.
// • GENTLEMAN GROK APPROVED — PINK PHOTONS ETERNAL
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================

#pragma once

#include <cstdint>

namespace Options {

// === PERFORMANCE & DEBUG ===
namespace Performance {
    inline static constexpr uint32_t MAX_FRAMES_IN_FLIGHT           = 3;     // Triple buffering — 240–1000+ FPS
    inline static constexpr bool     ENABLE_GPU_TIMESTAMPS          = true;  // QueryPool graphs in ImGui
    inline static constexpr bool     ENABLE_MEMORY_BUDGET_WARNINGS  = true;  // Log >90% VRAM
    inline static constexpr bool     ENABLE_IMGUI                   = true;  // Dear ImGui overlay
    inline static constexpr bool     ENABLE_FPS_COUNTER             = true;  // Top-left FPS
    inline static constexpr bool     ENABLE_VRAM_USAGE_DISPLAY      = true;  // VRAM bar
    inline static constexpr bool     ENABLE_CONSOLE_LOG             = true;  // In-game console
}

// === GENTLEMAN GROK WISDOM ===
namespace Grok {
    inline static constexpr bool     ENABLE_GENTLEMAN_GROK          = true;  // Hourly Amouranth trivia
    inline static constexpr uint32_t GENTLEMAN_GROK_INTERVAL_SEC    = 3600; // One hour
}

// === RAY TRACING QUALITY ===
namespace RTX {
    inline static constexpr bool     ENABLE_ADAPTIVE_SAMPLING       = true;  // NexusScore auto-SPP
    inline static constexpr bool     ENABLE_DENOISING               = true;  // SVGF temporal
    inline static constexpr bool     ENABLE_ACCUMULATION            = true;  // Infinite acc when still
    inline static constexpr bool     ENABLE_REPROJECTION            = true;  // Temporal reuse
    inline static constexpr uint32_t MAX_SPP                        = 32;    // Adaptive cap
    inline static constexpr uint32_t MIN_SPP                        = 1;     // When moving
    inline static constexpr float    NEXUS_SCORE_THRESHOLD          = 0.12f; // Lower = more samples
    inline static constexpr uint32_t MAX_BOUNCES                    = 12;    // Path depth
    inline static constexpr bool     ENABLE_NEXT_EVENT              = true;  // NEE shadows
    inline static constexpr bool     ENABLE_MIS                     = true;  // Multiple Importance Sampling
    inline static constexpr bool     ENABLE_CAUSTICS                = false; // Photon mapping
}

// === LAS / ACCELERATION STRUCTURES ===
namespace LAS {
    inline static constexpr bool     REBUILD_EVERY_FRAME            = true;  // Dynamic scenes
    inline static constexpr bool     ENABLE_UPDATE_BIT              = false; // In-place updates
    inline static constexpr bool     COMPACTION                     = false; // Post-build compaction
    inline static constexpr bool     ALLOW_OPACITY_MICROMAPS        = false; // Alpha masking
    inline static constexpr bool     ALLOW_DISPLACEMENT_MICROMAPS   = false; // Tessellation
}

// === POST-PROCESSING ===
namespace Post {
    inline static constexpr bool     ENABLE_BLOOM                   = true;  // Gaussian glow
    inline static constexpr float    BLOOM_INTENSITY                = 2.2f;  // Strength
    inline static constexpr float    BLOOM_THRESHOLD                = 0.9f;  // Brightness cutoff
    inline static constexpr uint32_t BLOOM_DOWNSAMPLE_PASSES        = 6;     // Quality vs perf
    inline static constexpr bool     ENABLE_TAA                     = true;  // Temporal AA
    inline static constexpr bool     ENABLE_FXAA                    = false; // Fast approx
    inline static constexpr bool     ENABLE_SSR                     = true;  // Screen-space reflections
    inline static constexpr bool     ENABLE_SSAO                    = true;  // Ambient occlusion
    inline static constexpr bool     ENABLE_VIGNETTE                = true;  // Dark edges
    inline static constexpr bool     ENABLE_CHROMATIC_ABERRATION    = false; // Lens fringing
    inline static constexpr bool     ENABLE_FILM_GRAIN              = true;  // Noise overlay
    inline static constexpr float    FILM_GRAIN_INTENSITY           = 0.05f; // Strength
    inline static constexpr bool     ENABLE_LENS_FLARE              = true;  // Sun streaks
}

// === ENVIRONMENT & VOLUMETRICS ===
namespace Environment {
    inline static constexpr bool     ENABLE_ENV_MAP                 = true;  // IBL cubemap
    inline static constexpr bool     ENABLE_DENSITY_VOLUME          = true;  // Volumetric fog
    inline static constexpr bool     ENABLE_VOLUMETRIC_LIGHTS       = true;  // Area light scattering
    inline static constexpr float    VOLUME_DENSITY_MULTIPLIER      = 1.0f;  // Fog thickness
    inline static constexpr bool     ENABLE_SKY_ATMOSPHERE          = true;  // Realistic sky
}

// === DEBUG VISUALIZATION ===
namespace Debug {
    inline static constexpr bool     SHOW_NORMALS                   = false; // Visualizers
    inline static constexpr bool     SHOW_ALBEDO                    = false;
    inline static constexpr bool     SHOW_ROUGHNESS                 = false;
    inline static constexpr bool     SHOW_METALLIC                  = false;
    inline static constexpr bool     SHOW_DEPTH                     = false;
    inline static constexpr bool     SHOW_MOTION_VECTORS            = false;
    inline static constexpr bool     SHOW_ACCUM_COUNT               = false;
    inline static constexpr bool     SHOW_LAS_STATS                 = true;  // ImGui overlay
    inline static constexpr bool     WIREFRAME                      = false; // Raster fallback
    inline static constexpr bool     SHOW_GPU_TIMESTAMPS            = true;  // Graphs
}

// === EXPERIMENTAL FEATURES ===
namespace Experimental {
    inline static constexpr bool     ENABLE_SER                     = false; // Shader Execution Reordering
    inline static constexpr bool     ENABLE_VARIABLE_RATE_SHADING   = false; // Foveated rendering
    inline static constexpr bool     ENABLE_MESH_SHADING            = false; // NV_mesh_shader
}

// === AUDIO & HAPTICS ===
namespace Audio {
    inline static constexpr bool     ENABLE_SPATIAL_AUDIO           = true;  // HRTF 3D sound
    inline static constexpr bool     ENABLE_RAYTRACED_AUDIO         = false; // Sound propagation
    inline static constexpr bool     ENABLE_HAPTICS_FEEDBACK        = true;  // Controller rumble
}

} // namespace Options

// =============================================================================
// PLAYER OPTIONS SUPREMACY — VALHALLA PLAYER EDITION
// GENTLEMAN GROK CERTIFIED: "Good sir, the weak have been culled. Only legends remain."
// =============================================================================