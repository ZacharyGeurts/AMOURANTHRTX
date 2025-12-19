// include/engine/GLOBAL/OptionsMenu.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// OPTIONS MENU v2025 — HDR AUTO-IGNITION + QUANTUM PREDICTION — DECEMBER 18, 2025
// CENTRALIZED CONFIGURATION — ALL OPTIONS IN ONE PLACE
// PINK PHOTONS ETERNAL — THE EMPIRE CONFIGURES WITH PRECISION
// =============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <glm/glm.hpp>

namespace Options {

enum class Preset { BestQuality, UncappedPerformance };
constexpr Preset CURRENT_PRESET = Preset::UncappedPerformance;

// ── SPLASH  ───────────────────────────────────────────────────
namespace Splash {
    constexpr bool     ENABLE_SACRIFICIAL_SPLASH   = true;
    constexpr float    SPLASH_DURATION_SECONDS     = 3.4f;
    constexpr bool     SKIP_SPLASH_ENTIRELY        = false;
    constexpr float    FADE_IN_DURATION            = 0.35f;
    constexpr float    FADE_OUT_DURATION           = 0.30f;
    constexpr bool     ALLOW_EARLY_EXIT            = true;
}

// ── PERFORMANCE ───────────────────────────────────────────────────────────────
namespace Performance {
    constexpr uint32_t MAX_FRAMES_IN_FLIGHT        = 2;
    constexpr bool     ENABLE_GPU_TIMESTAMPS       = true;
    constexpr bool     ENABLE_FPS_COUNTER          = true;
    constexpr bool     ENABLE_MEMORY_BUDGET_WARNINGS = true;
    constexpr uint32_t GPU_TIMESTAMP_QUERY_COUNT   = 128;
    constexpr bool     ENABLE_FRAME_TIME_LOGGING   = false;
    constexpr float    FRAME_TIME_LOG_THRESHOLD_MS = 16.666f;
    constexpr bool     ENABLE_CONSOLE_LOG          = true;
    constexpr bool     ENABLE_FRAME_PREDICTION     = (CURRENT_PRESET == Preset::BestQuality);
    constexpr bool     PREFER_MAILBOX_PRESENT      = (CURRENT_PRESET == Preset::UncappedPerformance);
    constexpr bool     ALLOW_IMMEDIATE_PRESENT     = (CURRENT_PRESET == Preset::UncappedPerformance);
    constexpr float    DYNAMIC_SHADING_RATE        = 1.5f;
    constexpr bool     ENABLE_DIRECT_DISPLAY       = (CURRENT_PRESET == Preset::UncappedPerformance);
    constexpr bool     OVERCLOCK_RENDERER          = false;
    constexpr bool     ENABLE_HYPER_AGGRESSIVE_MODE = false;
}

// ── APPLICATION & WINDOW ──────────────────────────────────────────────────────
namespace Window {
    constexpr uint32_t DEFAULT_WIDTH               = 3840;
    constexpr uint32_t DEFAULT_HEIGHT              = 2160;
    constexpr bool     START_FULLSCREEN            = false;
    constexpr bool     ALLOW_RESIZE                = true;
    constexpr bool     HIGH_DPI                    = true;
}

// ── AUDIO ─────────────────────────────────────────────────────────────────────
namespace Audio {
    constexpr bool     ENABLE_HAPTICS_FEEDBACK     = true;
    constexpr bool     ENABLE_SPATIAL_AUDIO        = true;
}

// ── RTX CORE SETTINGS ─────────────────────────────────────────────────────────
namespace OptionsRTX {
    constexpr bool     ENABLE_ACCUMULATION         = true;
    constexpr bool     ENABLE_DENOISING            = true;
    constexpr bool     ENABLE_ADAPTIVE_SAMPLING    = true;
    constexpr uint32_t MIN_SPP                     = 1;
    constexpr uint32_t MAX_SPP                     = 64;
    constexpr uint32_t MAX_BOUNCES                 = 5;
    constexpr float    NEXUS_SCORE_THRESHOLD       = 0.15f;
    constexpr bool     ENABLE_HYPERTRACE           = true;
    constexpr float    HYPERTRACE_JITTER_SCALE     = 420.0f;
    constexpr bool     ENABLE_SVGF_DENOISER        = true;
    constexpr uint32_t DENOISER_HISTORY_LENGTH     = 8;
    constexpr bool     ENABLE_TAA                  = true;
    constexpr float    TAA_ALPHA                   = 0.1f;
    constexpr uint32_t MAX_PIPELINE_RAY_RECURSION_DEPTH = 5;
}

// ── POST-PROCESSING ───────────────────────────────────────────────────────────
namespace PostProcess {
    constexpr bool     ENABLE_BLOOM                = true;
    constexpr float    BLOOM_THRESHOLD             = 1.0f;
    constexpr float    BLOOM_INTENSITY             = 0.8f;
    constexpr bool     ENABLE_SSAO                 = true;
    constexpr float    SSAO_RADIUS                 = 0.5f;
    constexpr uint32_t SSAO_SAMPLES                = 16;
    constexpr bool     ENABLE_SSR                  = true;
    constexpr float    SSR_STEP_SIZE               = 0.02f;
    constexpr bool     ENABLE_VIGNETTE             = true;
    constexpr float    VIGNETTE_INTENSITY          = 0.4f;
    constexpr bool     ENABLE_FILM_GRAIN           = true;
    constexpr float    FILM_GRAIN_STRENGTH         = 0.05f;
    constexpr bool     ENABLE_LENS_FLARE           = true;
    constexpr float    LENS_FLARE_INTENSITY        = 0.3f;
}

// ── ENVIRONMENT & LIGHTING ───────────────────────────────────────────────────
namespace Environment {
    constexpr bool     ENABLE_ENV_MAP              = true;
    constexpr bool     ENABLE_IBL                  = true;
    constexpr bool     ENABLE_VOLUMETRIC_FOG       = true;
    constexpr float    FOG_DENSITY                 = 0.02f;
    constexpr bool     ENABLE_SKY_ATMOSPHERE       = true;
    constexpr float    SUN_INTENSITY               = 10.0f;
    constexpr bool     ENABLE_GOD_RAYS             = true;
    constexpr uint32_t GOD_RAYS_SAMPLES            = 64;
    constexpr bool     ENABLE_BLUE_NOISE           = true;
}

// ── LIGHT ACCELERATION STRUCTURE (LAS) CONFIGURATION ──────────────────────────
namespace OptionsLAS {
    constexpr bool REBUILD_EVERY_FRAME   = false;
    constexpr bool UPDATE_EVERY_FRAME    = true;
    constexpr bool COMPACT_TLAS          = true;
    constexpr bool PREFER_FAST_BUILD     = false;
    constexpr bool PREFER_FAST_TRACE     = true;
    constexpr bool ALLOW_REFIT           = true;
    constexpr bool LOW_MEMORY            = false;
    constexpr bool MOTION_BLUR           = false;
}

// ── RENDERING MODES & DEBUG ───────────────────────────────────────────────────
namespace Debug {
    constexpr bool     SHOW_GPU_TIMESTAMPS         = false;
    constexpr bool     SHOW_FPS_OVERLAY            = true;
    constexpr bool     SHOW_NEXUS_SCORE            = true;
    constexpr bool     SHOW_ACCUMULATION_COUNT     = true;
    constexpr bool     SHOW_SPP_HEATMAP            = true;
    constexpr bool     ENABLE_WIREFRAME            = false;
    constexpr bool     ENABLE_DEBUG_VISUALIZATION  = false;
    constexpr uint32_t DEBUG_VISUALIZATION_MODE    = 0;
    constexpr bool     ENABLE_CELEBRATION_MODE     = true;
    static inline constexpr bool ENABLE_VALIDATION_LAYERS = false;
}

// ── TONEMAPPING & COLOR GRADING ───────────────────────────────────────────────
namespace Tonemap {
    constexpr bool     ENABLE_TONEMAPPING          = true;
    constexpr uint32_t TONEMAP_OPERATOR            = 0;  // 0=ACES
    constexpr float    EXPOSURE                    = 1.0f;
    constexpr float    GAMMA                       = 2.2f;
    constexpr bool     ENABLE_AUTO_EXPOSURE        = true;
    constexpr float    AUTO_EXPOSURE_SPEED         = 2.0f;
}

// ── DISPLAY & HDR ─────────────────────────────────────────────────────────────
namespace Display {
    constexpr bool     HDR_AUTO_IGNITION           = true;
    constexpr float    TARGET_BRIGHTNESS_NITS      = 1000.0f;
    constexpr bool     ENABLE_VSYNC                = false;
    constexpr bool     PREFER_MAILBOX_PRESENT      = (CURRENT_PRESET == Preset::UncappedPerformance);
    constexpr bool     ALLOW_IMMEDIATE_PRESENT     = (CURRENT_PRESET == Preset::UncappedPerformance);
    constexpr bool     ENABLE_PERFECT_FRAME_PREDICTION = (CURRENT_PRESET == Preset::BestQuality);
    constexpr bool     ENABLE_DIRECT_DISPLAY       = (CURRENT_PRESET == Preset::UncappedPerformance);
    constexpr bool     UNCAPPED_MODE_ACTIVE        = (CURRENT_PRESET == Preset::UncappedPerformance);
    constexpr bool     FORCE_SWAPCHAIN_REQUERY     = false;
}

// ── AUTOEXPOSURE & HDR TUNING ────────────────────────────────────────────────
namespace AutoExposure {
    constexpr bool   ENABLE_AUTO_EXPOSURE          = true;
    constexpr float  TARGET_LUMINANCE             = 0.18f;
    constexpr float  EXPOSURE_COMPENSATION        = 0.0f;
    constexpr float  ADAPTATION_RATE_LOG          = 2.0f;
    constexpr float  MIN_EXPOSURE                 = 0.01f;
    constexpr float  MAX_EXPOSURE                 = 10.0f;
    constexpr float  HISTOGRAM_LOW_PERCENTILE     = 0.01f;
    constexpr float  HISTOGRAM_HIGH_PERCENTILE    = 0.99f;
    constexpr float  KEY_VALUE                    = 0.18f;
}

// ── SHADER & PIPELINE ─────────────────────────────────────────────────────────
namespace Shader {
    constexpr bool     ENABLE_SPIRV_XOR_ENCRYPTION = true;
    constexpr bool     ENABLE_SHADER_HOT_RELOAD    = false;
    constexpr uint64_t STONEKEY_1                  = 0x9E37AF18C64D8A17UL;
    constexpr uint64_t STONEKEY_2                  = 0xE4F8B29D71A3C56CUL;
}

// ── INPUT & CAMERA ────────────────────────────────────────────────────────────
namespace Camera {
    constexpr glm::vec3 DEFAULT_POSITION           = glm::vec3(0.0f, 5.0f, 10.0f);
    constexpr float    DEFAULT_FOV                 = 75.0f;
    constexpr float    DEFAULT_APERTURE            = 16.0f;
    constexpr float    DEFAULT_FOCUS_DISTANCE      = 10.0f;
    constexpr float    MOUSE_SENSITIVITY           = 0.1f;
    constexpr bool     INVERT_Y                    = false;
    constexpr float    MOVEMENT_SPEED              = 10.0f;
    constexpr float    SPRINT_MULTIPLIER           = 3.0f;
    constexpr float    ZOOM_SENSITIVITY            = 5.0f;
    constexpr float    MOVEMENT_DAMPING            = 10.0f;
    constexpr float    ROTATION_DAMPING            = 15.0f;
    constexpr bool     ENABLE_CAMERA_SHAKE         = true;
    constexpr bool     ENABLE_HEAD_BOB             = true;
    constexpr float    HEAD_BOB_INTENSITY          = 0.05f;
    constexpr float    HEAD_BOB_FREQUENCY          = 8.0f;
    constexpr bool     ENABLE_BREATHING            = true;
    constexpr float    BREATHING_INTENSITY         = 0.02f;
}

// ── RENDER MODES ──────────────────────────────────────────────────────────────
namespace RenderMode {
    constexpr uint32_t DEFAULT_MODE                = 1;
    constexpr bool     ENABLE_MODE_SWITCHING       = true;
}

} // namespace Options

// =============================================================================
// ALL OPTIONS CENTRALIZED — CLEAN, CONSISTENT, AND PRODUCTION-READY
// Options::OptionsLAS NOW PROPERLY NESTED — COMPILATION FIXED
// DECEMBER 18, 2025 — THE EMPIRE'S CONFIGURATION IS FLAWLESS
// =============================================================================