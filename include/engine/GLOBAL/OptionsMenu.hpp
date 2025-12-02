#pragma once

#include <cstdint>
#include <cstddef>

namespace Options {

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
    constexpr uint32_t MAX_FRAMES_IN_FLIGHT        = 3;
    constexpr bool     ENABLE_GPU_TIMESTAMPS       = true;
    constexpr bool     ENABLE_FPS_COUNTER          = true;
    constexpr bool     ENABLE_MEMORY_BUDGET_WARNINGS = true;
    constexpr uint32_t GPU_TIMESTAMP_QUERY_COUNT   = 128;
    constexpr bool     ENABLE_FRAME_TIME_LOGGING   = false;
    constexpr float    FRAME_TIME_LOG_THRESHOLD_MS = 16.666f;
    constexpr bool     START_FULLSCREEN            = false;
    constexpr bool     ENABLE_CONSOLE_LOG          = true;

    constexpr bool     ENABLE_FRAME_PREDICTION     = true;
    constexpr bool     PREFER_MAILBOX_PRESENT      = true;
    constexpr bool     ALLOW_IMMEDIATE_PRESENT     = false;

    constexpr float    DYNAMIC_SHADING_RATE        = 1.0f;     // Full res only
    constexpr bool     ENABLE_DIRECT_DISPLAY       = true;

    constexpr bool     OVERCLOCK_RENDERER           = true;
    constexpr bool     ENABLE_HYPER_AGGRESSIVE_MODE = true;
}

// ── APPLICATION & WINDOW ──────────────────────────────────────────────────────
namespace Window {
    constexpr uint32_t DEFAULT_WIDTH               = 3840;
    constexpr uint32_t DEFAULT_HEIGHT              = 2160;
    constexpr bool     START_FULLSCREEN            = false;
    constexpr bool     ALLOW_RESIZE                = true;
    constexpr bool     HIGH_DPI                    = true;
    constexpr bool     ENABLE_QUANTUM_RESIZE_PREDICTION = true;
}

// ── AUDIO ─────────────────────────────────────────────────────────────────────
namespace Audio {
    constexpr bool     ENABLE_HAPTICS_FEEDBACK     = false;
    constexpr bool     ENABLE_SPATIAL_AUDIO        = false;
}

// ── RTX CORE SETTINGS ─────────────────────────────────────────────────────────
namespace OptionsRTX {
    constexpr bool     ENABLE_ACCUMULATION         = true;
    constexpr bool     ENABLE_DENOISING            = false;   // Disabled — raw photons
    constexpr bool     ENABLE_ADAPTIVE_SAMPLING    = true;
    constexpr uint32_t MIN_SPP                     = 1;
    constexpr uint32_t MAX_SPP                     = 64;
    constexpr uint32_t MAX_BOUNCES                 = 3;
    constexpr float    NEXUS_SCORE_THRESHOLD       = 0.15f;
    constexpr bool     ENABLE_HYPERTRACE           = true;
    constexpr float    HYPERTRACE_JITTER_SCALE     = 420.0f;
    constexpr bool     ENABLE_SVGF_DENOISER        = false;   // Disabled
    constexpr uint32_t DENOISER_HISTORY_LENGTH     = 8;
    constexpr bool     ENABLE_TAA                  = false;    // Disabled — no TAA
    constexpr float    TAA_ALPHA                   = 0.1f;
    constexpr uint32_t MAX_PIPELINE_RAY_RECURSION_DEPTH = 3;
}

// ── POST-PROCESSING — ALL DISABLED — RAW PHOTONS ONLY ─────────────────────────
namespace PostProcess {
    constexpr bool     ENABLE_BLOOM                = false;
    constexpr float    BLOOM_THRESHOLD             = 1.0f;
    constexpr float    BLOOM_INTENSITY             = 0.8f;
    constexpr bool     ENABLE_SSAO                 = false;
    constexpr float    SSAO_RADIUS                 = 0.5f;
    constexpr uint32_t SSAO_SAMPLES                = 16;
    constexpr bool     ENABLE_SSR                  = false;
    constexpr float    SSR_STEP_SIZE               = 0.02f;
    constexpr bool     ENABLE_VIGNETTE             = false;
    constexpr float    VIGNETTE_INTENSITY          = 0.4f;
    constexpr bool     ENABLE_FILM_GRAIN           = false;
    constexpr float    FILM_GRAIN_STRENGTH         = 0.05f;
    constexpr bool     ENABLE_LENS_FLARE           = false;
    constexpr float    LENS_FLARE_INTENSITY        = 0.3f;
}

// ── ENVIRONMENT & LIGHTING ───────────────────────────────────────────────────
namespace Environment {
    constexpr bool     ENABLE_ENV_MAP              = true;
    constexpr bool     ENABLE_IBL                  = true;
    constexpr bool     ENABLE_VOLUMETRIC_FOG       = false;   // Disabled — pure
    constexpr float    FOG_DENSITY                 = 0.02f;
    constexpr bool     ENABLE_SKY_ATMOSPHERE       = true;
    constexpr float    SUN_INTENSITY               = 10.0f;
    constexpr bool     ENABLE_GOD_RAYS             = false;   // Disabled — raw light
    constexpr uint32_t GOD_RAYS_SAMPLES            = 64;
}

// ── LAS (Lightweight Acceleration Structure) ─────────────────────────────────
namespace OptionsLAS {
    constexpr bool     REBUILD_EVERY_FRAME         = false;
    constexpr bool     UPDATE_EVERY_FRAME          = true;
    constexpr bool     COMPACT_TLAS                = true;
    constexpr bool     PREFER_FAST_BUILD           = true;
    constexpr bool     PREFER_FAST_TRACE           = false;
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

// ── TONEMAPPING & COLOR GRADING — DISABLED — RAW LINEAR OUTPUT ────────────────
namespace Tonemap {
    constexpr bool     ENABLE_TONEMAPPING          = false;   // Disabled — no tonemapping
    constexpr uint32_t TONEMAP_OPERATOR            = 0;
    constexpr float    EXPOSURE                    = 1.0f;
    constexpr float    GAMMA                       = 1.0f;     // Linear
    constexpr bool     ENABLE_AUTO_EXPOSURE        = false;   // Disabled
    constexpr float    AUTO_EXPOSURE_SPEED         = 2.0f;
}

// ── DISPLAY & HDR — RAW LINEAR OUTPUT ONLY ───────────────────────────────────
namespace Display {
    constexpr bool     HDR_AUTO_IGNITION           = false;   // Disabled — we are in raw mode
    constexpr float    TARGET_BRIGHTNESS_NITS      = 1000.0f;
    constexpr bool     ENABLE_VSYNC                = false;
}

// ── AUTOEXPOSURE & HDR TUNING — DISABLED ─────────────────────────────────────
namespace AutoExposure {
    constexpr bool   ENABLE_AUTO_EXPOSURE          = false;
    constexpr float  TARGET_LUMINANCE             = 0.18f;
    constexpr float  EXPOSURE_COMPENSATION        = 0.0f;
    constexpr float  ADAPTATION_RATE_LOG          = 2.0f;
    constexpr float  MIN_EXPOSURE                 = 0.01f;
    constexpr float  MAX_EXPOSURE                 = 10.0f;
    constexpr float  HISTOGRAM_LOW_PERCENTILE     = 0.01f;
    constexpr float  HISTOGRAM_HIGH_PERCENTILE    = 0.99f;
    constexpr float  KEY_VALUE                    = 0.18f;
}

// ── MEMORY & ALLOCATION ───────────────────────────────────────────────────────
namespace Memory {
    constexpr size_t   UNIFORM_BUFFER_SIZE_PER_FRAME = 64 * 1024 * 1024;
    constexpr size_t   MATERIAL_BUFFER_SIZE          = 16 * 1024 * 1024;
    constexpr size_t   RESERVOIR_BUFFER_SIZE         = 512 * 1024 * 1024;
    constexpr size_t   FRAME_DATA_BUFFER_SIZE        = 128 * 1024 * 1024;
    constexpr bool     ENABLE_MEMORY_POOLING         = true;
    constexpr bool     ENABLE_ZERO_INIT              = false;
}

// ── SHADER & PIPELINE ─────────────────────────────────────────────────────────
namespace Shader {
    constexpr bool     ENABLE_SPIRV_XOR_ENCRYPTION = true;
    constexpr bool     ENABLE_SHADER_HOT_RELOAD    = true;
    constexpr uint64_t STONEKEY_1                  = 0x9E3779B97F4A7C15ULL;
    constexpr uint64_t STONEKEY_2                  = 0x7F4A7C158E3779B9ULL;
}

// ── INPUT & CAMERA ────────────────────────────────────────────────────────────
namespace Input {
    constexpr float    MOUSE_SENSITIVITY           = 0.1f;
    constexpr float    MOVEMENT_SPEED              = 5.0f;
    constexpr float    SPRINT_MULTIPLIER           = 3.0f;
    constexpr bool     INVERT_Y                    = false;
}

// ── RENDER MODES ──────────────────────────────────────────────────────────────
namespace RenderMode {
    constexpr uint32_t DEFAULT_MODE                = 0;
    constexpr bool     ENABLE_MODE_SWITCHING       = true;
}

// ── KOJIMA — THE DIRECTOR’S CUT OF REALITY — DISABLED UNTIL HUMANITY IS READY ─
namespace Kojima {
    constexpr bool ENABLE_BLUE_NOISE                     = false;
    constexpr bool ENABLE_NORMAN_REEDUS_PHOTON           = false;
    constexpr bool ENABLE_BB_POD_AUTOEXPOSURE            = false;
    constexpr bool ENABLE_KOJIMA_TEARS_TONEMAPPER        = false;
    constexpr bool ENABLE_MAILMAN_RAY_TRACING            = false;
    constexpr bool ENABLE_HIDEO_KOJIMA_SIGNATURE_IN_SKY  = false;
    constexpr bool ENABLE_CONCEPTUAL_FPS_COUNTER         = false;

    constexpr bool ENABLE_12_HOUR_CUTSCENE_BETWEEN_FRAMES = false;
    constexpr bool ENABLE_KOJIMA_MODE                     = false;

    constexpr const char* KOJIMA_WHISPER = "The light is raw. The truth is pink. You were never ready.";
}

} // namespace Options

// =============================================================================
// RAW PHOTON MODE — ACTIVE
// NO TONEMAPPING
// NO POST-PROCESSING
// NO KOJIMA
// ONLY PURE PATH-TRACED LIGHT
// PINK PHOTONS — UNFILTERED — ETERNAL
// FIRST LIGHT ACHIEVED — DECEMBER 02, 2025
// =============================================================================