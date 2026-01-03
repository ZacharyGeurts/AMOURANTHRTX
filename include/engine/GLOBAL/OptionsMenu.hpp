// include/engine/GLOBAL/OptionsMenu.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — VALHALLA v∞ — CENTRALIZED CONFIGURATION v3
// FULLY RESTORED & COMPATIBLE — DECEMBER 20, 2025
// All previously used options restored exactly as referenced in code:
// - Options::Camera::INVERT_MOUSE_LOOK
// - Options::Camera::ZOOM_SENSITIVITY
// - Options::Camera::CAMERA_START_POSITION (alias for START_POSITION)
// - Options::Audio namespace (ENABLE_HAPTICS_FEEDBACK)
// - Options::Performance::ENABLE_HYPER_AGGRESSIVE_MODE
// - Options::ENABLE_CONSOLE_LOG (top-level)
// All active engine features preserved and organized
// PINK PHOTONS ETERNAL — THE EMPIRE IS UNBROKEN
// =============================================================================

#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace Options {

// ── MISC TOP-LEVEL ───────────────────────────────────────────────────────────
constexpr bool ENABLE_CONSOLE_LOG = true;

// ── CURRENT PRESET ───────────────────────────────────────────────────────────
enum class Preset { BestQuality, Balanced, UncappedPerformance };
constexpr Preset CURRENT_PRESET = Preset::BestQuality;

// ── SPLASH SCREEN ────────────────────────────────────────────────────────────
namespace Splash {
    constexpr bool     ENABLE_SACRIFICIAL_SPLASH   = true;
    constexpr float    SPLASH_DURATION_SECONDS     = 3.4f;
    constexpr bool     SKIP_SPLASH_ENTIRELY        = false;
    constexpr bool     ALLOW_EARLY_EXIT            = true;
}

// ── PERFORMANCE & RENDERING ──────────────────────────────────────────────────
namespace Performance {
    constexpr uint32_t MAX_FRAMES_IN_FLIGHT        = 2;
    constexpr bool     ENABLE_GPU_TIMESTAMPS       = true;
    constexpr bool     ENABLE_FPS_COUNTER          = true;
    constexpr bool     ENABLE_MEMORY_BUDGET_WARNINGS = true;
    constexpr bool     OVERCLOCK_RENDERER          = false;
    constexpr bool     ENABLE_HYPER_AGGRESSIVE_MODE = false;  // Restored for RTXHandler.cpp
}

// ── WINDOW & DISPLAY ─────────────────────────────────────────────────────────
namespace Window {
    constexpr uint32_t DEFAULT_WIDTH               = 3840;
    constexpr uint32_t DEFAULT_HEIGHT              = 2160;
    constexpr bool     START_FULLSCREEN            = false;
    constexpr bool     ALLOW_RESIZE                = true;
}

// ── AUDIO (RESTORED — used in SDL3.cpp) ──────────────────────────────────────
namespace Audio {
    constexpr bool     ENABLE_HAPTICS_FEEDBACK     = true;
}

// ── RTX CORE SETTINGS ────────────────────────────────────────────────────────
namespace OptionsRTX {
    constexpr bool     ENABLE_ACCUMULATION         = true;
    constexpr bool     ENABLE_DENOISING            = true;
    constexpr bool     ENABLE_ADAPTIVE_SAMPLING    = true;
    constexpr bool     ENABLE_HYPERTRACE           = true;
    constexpr bool     ENABLE_TAA                  = true;
    constexpr float    TAA_ALPHA                   = 0.10f;

    constexpr uint32_t MAX_PIPELINE_RAY_RECURSION_DEPTH = 8;
    constexpr float    NEXUS_SCORE_THRESHOLD       = 0.15f;
    constexpr float    HYPERTRACE_JITTER_SCALE     = 420.0f;
}

// ── POST-PROCESSING ──────────────────────────────────────────────────────────
namespace PostProcess {
    constexpr bool     ENABLE_BLOOM                = true;
    constexpr float    BLOOM_THRESHOLD             = 1.0f;
    constexpr float    BLOOM_INTENSITY             = 0.8f;

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
    constexpr glm::vec3 FOG_COLOR                  = glm::vec3(0.7f, 0.8f, 0.9f);

    constexpr bool     ENABLE_GOD_RAYS             = true;
    constexpr uint32_t GOD_RAYS_SAMPLES            = 64;
    constexpr float    GOD_RAYS_INTENSITY          = 1.2f;

    constexpr bool     ENABLE_BLUE_NOISE           = true;
    constexpr float    SUN_INTENSITY               = 40.0f;
    constexpr glm::vec3 SUN_DIRECTION              = glm::vec3(0.3f, 0.8f, 0.5f);
    constexpr glm::vec3 SUN_COLOR                  = glm::vec3(1.0f, 0.95f, 0.9f);
}

// ── TONEMAPPING & HDR ────────────────────────────────────────────────────────
namespace Tonemap {
    constexpr bool     ENABLE_TONEMAPPING          = true;
    constexpr uint32_t TONEMAP_OPERATOR            = 0;  // 0 = ACES
    constexpr float    GAMMA                       = 2.2f;
}

// ── AUTO-EXPOSURE ────────────────────────────────────────────────────────────
namespace AutoExposure {
    constexpr bool   ENABLE_AUTO_EXPOSURE          = true;
    constexpr float  TARGET_LUMINANCE              = 0.18f;
    constexpr float  EXPOSURE_COMPENSATION         = 0.0f;
    constexpr float  ADAPTATION_RATE               = 2.0f;
    constexpr float  MIN_EXPOSURE                  = 0.05f;
    constexpr float  MAX_EXPOSURE                  = 16.0f;
}

// ── DEBUG & OVERLAYS ─────────────────────────────────────────────────────────
namespace Debug {
    constexpr bool     SHOW_FPS_OVERLAY            = true;
    constexpr bool     SHOW_NEXUS_SCORE            = true;
    constexpr bool     SHOW_SPP_HEATMAP            = true;
    constexpr bool     SHOW_ACCUMULATION_COUNT     = true;
    constexpr bool     SHOW_GPU_TIMESTAMPS         = false;
    constexpr bool     ENABLE_VALIDATION_LAYERS    = true;
}

// ── CAMERA DEFAULTS (RESTORED ALL USED OPTIONS) ──────────────────────────────
namespace Camera {
    constexpr glm::vec3 START_POSITION             = glm::vec3(0.0f, 5.0f, 15.0f);
    constexpr glm::vec3 CAMERA_START_POSITION      = START_POSITION;  // Legacy alias — kept for camera.cpp

    constexpr float    START_YAW                   = -90.0f;
    constexpr float    START_PITCH                 = 0.0f;
    constexpr float    DEFAULT_FOV                 = 75.0f;
    constexpr float    DEFAULT_APERTURE            = 16.0f;
    constexpr float    DEFAULT_FOCUS_DISTANCE      = 10.0f;

    constexpr float    MOUSE_SENSITIVITY           = 0.12f;
    constexpr bool     INVERT_MOUSE_LOOK           = false;           // Restored
    constexpr float    ZOOM_SENSITIVITY            = 1.0f;             // Restored

    constexpr float    MOVEMENT_SPEED              = 12.0f;
    constexpr float    SPRINT_MULTIPLIER           = 2.8f;
}

// ── RENDER MODES ─────────────────────────────────────────────────────────────
namespace RenderMode {
    constexpr uint32_t RTX_PATH_TRACING            = 0;
    constexpr uint32_t PURE_PINK_VOID              = 1;
    constexpr uint32_t DEFAULT_MODE                = RTX_PATH_TRACING;
    constexpr bool     ENABLE_MODE_SWITCHING       = true;
}

// ── PINK BILLBOARD (SACRED FALLBACK) ─────────────────────────────────────────
namespace PinkBillboard {
    constexpr const char* TEXTURE_PATH             = "assets/textures/monster.png";
    constexpr float       SCALE                     = 10.0f;
    constexpr float       Z_OFFSET                  = -8.0f;
    constexpr glm::vec3   BASE_COLOR               = glm::vec3(1.0f, 0.0f, 0.5f); // Sacred pink
    constexpr float       ALPHA_CUTOFF             = 0.5f;
    constexpr bool       USE_ALPHA_BLEND           = true;
}

} // namespace Options

// =============================================================================
// FINAL CONFIGURATION v3 — DECEMBER 20, 2025
// All compilation errors resolved:
// - Restored Options::Camera::INVERT_MOUSE_LOOK
// - Restored Options::Camera::ZOOM_SENSITIVITY
// - Restored Options::Camera::CAMERA_START_POSITION (alias)
// - Restored Options::Audio namespace
// - Restored Options::Performance::ENABLE_HYPER_AGGRESSIVE_MODE
// - Kept top-level ENABLE_CONSOLE_LOG
// All active features preserved — engine will now compile cleanly
// THE EMPIRE IS RESTORED — PINK PHOTONS FLOW ETERNALLY
// =============================================================================