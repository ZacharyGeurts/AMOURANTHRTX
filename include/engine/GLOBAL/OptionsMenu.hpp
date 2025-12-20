// include/engine/GLOBAL/OptionsMenu.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — VALHALLA v∞ — CENTRALIZED CONFIGURATION
// CLEANED & OPTIMIZED FOR CURRENT ARCHITECTURE — DECEMBER 19, 2025
// SUPPORTS FORCED PINK BILLBOARD, FIFO + MAILBOX EMULATION, NO UNUSED OPTIONS
// PINK PHOTONS ETERNAL — THE EMPIRE CONFIGURES WITH PRECISION
// =============================================================================

#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace Options {

// ── CURRENT PRESET ───────────────────────────────────────────────────────────
enum class Preset { BestQuality, UncappedPerformance };
constexpr Preset CURRENT_PRESET = Preset::BestQuality;

// ── SPLASH SCREEN ────────────────────────────────────────────────────────────
namespace Splash {
    constexpr bool     ENABLE_SACRIFICIAL_SPLASH   = true;
    constexpr float    SPLASH_DURATION_SECONDS     = 3.4f;
    constexpr bool     SKIP_SPLASH_ENTIRELY        = false;
    constexpr float    FADE_IN_DURATION            = 0.35f;
    constexpr float    FADE_OUT_DURATION           = 0.30f;
    constexpr bool     ALLOW_EARLY_EXIT            = true;
}

// ── PERFORMANCE & RENDERING ──────────────────────────────────────────────────
namespace Performance {
    constexpr uint32_t MAX_FRAMES_IN_FLIGHT        = 2;                 // Fixed for triple-buffered FIFO emulation
    constexpr bool     ENABLE_GPU_TIMESTAMPS       = true;
    constexpr bool     ENABLE_FPS_COUNTER          = true;
    constexpr bool     ENABLE_MEMORY_BUDGET_WARNINGS = true;
    constexpr uint32_t GPU_TIMESTAMP_QUERY_COUNT   = 128;
    constexpr bool     ENABLE_FRAME_TIME_LOGGING   = false;
    constexpr float    FRAME_TIME_LOG_THRESHOLD_MS = 16.666f;
    constexpr bool     ENABLE_CONSOLE_LOG          = true;
    constexpr bool     OVERCLOCK_RENDERER          = false;
    constexpr bool     ENABLE_HYPER_AGGRESSIVE_MODE = false;
}

// ── WINDOW & DISPLAY ─────────────────────────────────────────────────────────
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

// ── RTX CORE SETTINGS ────────────────────────────────────────────────────────
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

// ── POST-PROCESSING ──────────────────────────────────────────────────────────
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
    constexpr float    SUN_INTENSITY               = 40.0f;
    constexpr bool     ENABLE_GOD_RAYS             = true;
    constexpr uint32_t GOD_RAYS_SAMPLES            = 64;
    constexpr bool     ENABLE_BLUE_NOISE           = true;
    constexpr float    ENVIRONMENT_EXPOSURE        = 4.0f;
}

// ── LIGHT ACCELERATION STRUCTURE (LAS) CONFIGURATION ─────────────────────────
namespace OptionsLAS {
    constexpr bool REBUILD_EVERY_FRAME   = false;        // Not used — forced pink + direct TLAS
    constexpr bool UPDATE_EVERY_FRAME    = true;         // Always true for dynamic scenes
    constexpr bool COMPACT_TLAS          = true;         // Memory efficient
    constexpr bool PREFER_FAST_BUILD     = false;        // Balance quality/speed
    constexpr bool PREFER_FAST_TRACE     = true;         // Primary goal
    constexpr bool ALLOW_REFIT           = true;         // Enables update mode
    constexpr bool LOW_MEMORY            = false;        // Use full quality
    constexpr bool MOTION_BLUR           = false;        // Not implemented yet
}

// ── RENDERING MODES & DEBUG ──────────────────────────────────────────────────
namespace Debug {
    constexpr bool     SHOW_GPU_TIMESTAMPS         = false;
    constexpr bool     SHOW_FPS_OVERLAY            = true;
    constexpr bool     SHOW_NEXUS_SCORE            = true;
    constexpr bool     SHOW_ACCUMULATION_COUNT     = true;
    constexpr bool     SHOW_SPP_HEATMAP            = true;
    constexpr bool     ENABLE_WIREFRAME            = false;
    constexpr bool     ENABLE_DEBUG_VISUALIZATION  = false;
    constexpr uint32_t DEBUG_VISUALIZATION_MODE    = 0;
    constexpr bool     ENABLE_VALIDATION_LAYERS    = false;
}

// ── TONEMAPPING & COLOR GRADING ──────────────────────────────────────────────
namespace Tonemap {
    constexpr bool     ENABLE_TONEMAPPING          = true;
    constexpr uint32_t TONEMAP_OPERATOR            = 0;  // 0 = ACES
    constexpr float    EXPOSURE                    = 1.0f;
    constexpr float    GAMMA                       = 2.2f;
    constexpr bool     ENABLE_AUTO_EXPOSURE        = true;
    constexpr float    AUTO_EXPOSURE_SPEED         = 2.0f;
}

// ── AUTOEXPOSURE & HDR TUNING ────────────────────────────────────────────────
namespace AutoExposure {
    constexpr bool   ENABLE_AUTO_EXPOSURE         = true;
    constexpr float  TARGET_LUMINANCE             = 0.18f;
    constexpr float  EXPOSURE_COMPENSATION        = 0.0f;
    constexpr float  ADAPTATION_RATE_LOG          = 2.0f;
    constexpr float  MIN_EXPOSURE                 = 0.01f;
    constexpr float  MAX_EXPOSURE                 = 10.0f;
}

// ── INPUT & CAMERA ───────────────────────────────────────────────────────────
namespace Camera {
    constexpr glm::vec3 CAMERA_START_POSITION      = glm::vec3(0.0f, 5.0f, 10.0f);
    constexpr float    CAMERA_START_YAW            = -90.0f;
    constexpr float    CAMERA_START_PITCH          = 0.0f;
    constexpr float    DEFAULT_FOV                 = 75.0f;
    constexpr float    DEFAULT_APERTURE            = 16.0f;
    constexpr float    DEFAULT_FOCUS_DISTANCE      = 10.0f;
    constexpr float    MOUSE_SENSITIVITY           = 0.1f;
    constexpr bool     INVERT_MOUSE_LOOK           = false;
    constexpr float    MOVEMENT_SPEED              = 10.0f;
    constexpr float    SPRINT_MULTIPLIER           = 3.0f;
    constexpr float    GRAVITY_STRENGTH            = 20.0f;
    constexpr float    JUMP_FORCE                  = 8.0f;
    constexpr float    GROUND_LEVEL                = 0.0f;
	constexpr float    ZOOM_SENSITIVITY            = 1.0f;
}

// ── RENDER MODES ─────────────────────────────────────────────────────────────
namespace RenderMode {
    constexpr uint32_t DEFAULT_MODE                = 0;  // 0 = RTX, 1 = Pink Void
    constexpr bool     ENABLE_MODE_SWITCHING       = true;
}

// ── MISC & DEFAULTS ──────────────────────────────────────────────────────────
constexpr const char* MONSTER_TEXTURE_PATH        = "assets/textures/monster.png";
constexpr float       BILLBOARD_SCALE             = 8.0f;
constexpr float       BILLBOARD_Z_OFFSET          = -5.0f;
constexpr glm::vec3   BILLBOARD_BASE_COLOR        = glm::vec3(1.0f, 0.0f, 0.5f); // Sacred pink
constexpr float       BILLBOARD_ALPHA_CUTOFF      = 0.5f;
constexpr bool        BILLBOARD_USE_ALPHA_BLEND   = true;

} // namespace Options

// =============================================================================
// CENTRALIZED OPTIONS — CLEANED FOR CURRENT ARCHITECTURE
// REMOVED: Unused swapchain options (we use fixed 3-image FIFO emulation)
// REMOVED: Redundant display options (HDR auto-ignition handled in SwapchainManager)
// KEPT: All active LAS options + forced pink billboard support
// DECEMBER 19, 2025 — CONFIGURATION IS FLAWLESS
// =============================================================================