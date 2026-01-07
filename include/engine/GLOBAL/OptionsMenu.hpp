// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// OPTIONS MENU v∞ — JANUARY 06, 2026 — FINAL CLEAN CONFIGURATION
// PURE HDR PHILOSOPHY — NO HOLLYWOOD TRICKS
// BLOOM, VIGNETTE, GRAIN, FLARE — BANISHED FOREVER
// ONLY WHAT SERVES TRUTH, CLARITY, AND PERFORMANCE REMAINS
// ACCUMULATION + DENOISING + TONEMAP — ENABLED FOR PERFECTION
// CAMERA FULLY CONFIGURABLE — CENTRALIZED CONTROL
// PINK PHOTONS ETERNAL — THE EMPIRE SEES CLEARLY — AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace Options {

// ── THE EMPIRE'S CREED: PURE HDR TRUTH ──────────────────────────────────────
constexpr bool ENABLE_EVERYTHING_ESSENTIAL = true;

// ── SPLASH — RESPECTFUL BUT BRIEF
namespace Splash {
    constexpr bool     ENABLE_SACRIFICIAL_SPLASH   = true;
    constexpr float    SPLASH_DURATION_SECONDS     = 1.5f;
    constexpr bool     ALLOW_EARLY_EXIT            = true;
}

// ── PERFORMANCE — MAXIMUM FOR HARDWARE
namespace Performance {
    constexpr uint32_t MAX_FRAMES_IN_FLIGHT        = 2;
    constexpr bool     ENABLE_GPU_TIMESTAMPS       = false;  // Clean logs
    constexpr bool     ENABLE_FPS_COUNTER          = true;
    constexpr bool     OVERCLOCK_RENDERER          = true;
    constexpr bool     ENABLE_HYPER_AGGRESSIVE_MODE = true;
}

// ── WINDOW — DEVELOPER FRIENDLY
namespace Window {
    constexpr uint32_t DEFAULT_WIDTH               = 3840;
    constexpr uint32_t DEFAULT_HEIGHT              = 2160;
    constexpr bool     START_FULLSCREEN            = false;
    constexpr bool     ALLOW_RESIZE                = true;
}

// ── AUDIO — IMMERSION WITHOUT DISTRACTION
namespace Audio {
    constexpr bool     ENABLE_HAPTICS_FEEDBACK     = true;
}

// ── RTX CORE — PURE PATH TRACING, REFINED
namespace OptionsRTX {
    constexpr bool     ENABLE_ACCUMULATION         = true;   // Infinite refinement — essential
    constexpr bool     ENABLE_DENOISING            = true;   // Clean truth — essential
    constexpr bool     ENABLE_ADAPTIVE_SAMPLING    = true;   // Intelligent focus
    constexpr bool     ENABLE_HYPERTRACE           = true;   // Deep light interaction
    constexpr bool     ENABLE_TAA                  = true;    // Temporal stability
    constexpr float    TAA_ALPHA                   = 0.12f;

    constexpr uint32_t MAX_PIPELINE_RAY_RECURSION_DEPTH = 12;
    constexpr float    NEXUS_SCORE_THRESHOLD       = 0.08f;
    constexpr float    HYPERTRACE_JITTER_SCALE     = 380.0f;
}

// ── ENVIRONMENT & LIGHTING — PHYSICALLY ACCURATE
namespace Environment {
    constexpr bool     ENABLE_ENV_MAP              = true;
    constexpr bool     ENABLE_IBL                  = true;
    constexpr bool     ENABLE_VOLUMETRIC_FOG       = false;  // Can obscure clarity — off by default
    constexpr bool     ENABLE_GOD_RAYS             = false;  // Artistic, not essential — off
    constexpr bool     ENABLE_BLUE_NOISE           = true;   // Improves sampling — kept
    constexpr float    SUN_INTENSITY               = 50.0f;
    constexpr glm::vec3 SUN_DIRECTION              = glm::vec3(0.3f, 0.8f, 0.5f);
    constexpr glm::vec3 SUN_COLOR                  = glm::vec3(1.0f, 0.98f, 0.94f);
}

// ── TONEMAPPING — PURE AND ACCURATE
namespace Tonemap {
    constexpr bool     ENABLE_TONEMAPPING          = false;   // Required for HDR display
    constexpr uint32_t TONEMAP_OPERATOR            = 0;      // 0 = ACES — most accurate
    constexpr float    GAMMA                       = 2.2f;
}

// ── AUTO-EXPOSURE — INTELLIGENT ADAPTATION
namespace AutoExposure {
    constexpr bool   ENABLE_AUTO_EXPOSURE          = true;
    constexpr float  TARGET_LUMINANCE              = 0.18f;
    constexpr float  EXPOSURE_COMPENSATION         = 0.0f;
    constexpr float  ADAPTATION_RATE               = 1.8f;
    constexpr float  MIN_EXPOSURE                  = 0.05f;
    constexpr float  MAX_EXPOSURE                  = 32.0f;
}

// ── DEBUG — FULL VISIBILITY
namespace Debug {
    constexpr bool     SHOW_FPS_OVERLAY            = true;
    constexpr bool     SHOW_NEXUS_SCORE            = true;
    constexpr bool     SHOW_SPP_HEATMAP            = true;
    constexpr bool     SHOW_ACCUMULATION_COUNT     = true;
    constexpr bool     SHOW_GPU_TIMESTAMPS         = false;
    constexpr bool     ENABLE_VALIDATION_LAYERS    = false;
}

// ── CAMERA — SMOOTH, PRECISE, AND FULLY CONFIGURABLE
namespace Camera {
    constexpr glm::vec3 START_POSITION             = glm::vec3(0.0f, 5.0f, 15.0f);

    constexpr float    DEFAULT_FOV                 = 80.0f;
    constexpr float    DEFAULT_APERTURE            = 16.0f;
    constexpr float    DEFAULT_FOCUS_DISTANCE      = 10.0f;

    constexpr float    MOUSE_SENSITIVITY           = 0.14f;
    constexpr bool     INVERT_MOUSE_LOOK           = false;
    constexpr float    ZOOM_SENSITIVITY            = 1.1f;

    constexpr float    MOVEMENT_SPEED              = 14.0f;
    constexpr float    SPRINT_MULTIPLIER           = 2.8f;

    // Immersion effects — enabled by default for maximum realism
    constexpr bool     ENABLE_HEAD_BOB             = true;
    constexpr float    HEAD_BOB_FREQUENCY          = 2.2f;
    constexpr float    HEAD_BOB_INTENSITY          = 0.08f;

    constexpr bool     ENABLE_BREATHING            = true;
    constexpr float    BREATHING_INTENSITY         = 0.03f;

    constexpr bool     ENABLE_CAMERA_SHAKE         = true;
}

// ── RENDER MODES — FULL RTX ALWAYS
namespace RenderMode {
    constexpr uint32_t RTX_PATH_TRACING            = 0;
    constexpr uint32_t PURE_PINK_VOID              = 1;
    constexpr uint32_t DEFAULT_MODE                = RTX_PATH_TRACING;
    constexpr bool     ENABLE_MODE_SWITCHING       = false;  // Pure RTX forever
}

// ── PINK BILLBOARD — SACRED FALLBACK
namespace PinkBillboard {
    constexpr const char* TEXTURE_PATH             = "assets/textures/monster.png";
    constexpr float       SCALE                     = 12.0f;
    constexpr float       Z_OFFSET                  = -10.0f;
    constexpr glm::vec3   BASE_COLOR               = glm::vec3(1.0f, 0.08f, 0.58f);
    constexpr float       ALPHA_CUTOFF             = 0.5f;
    constexpr bool       USE_ALPHA_BLEND           = true;
}

} // namespace Options

// =============================================================================
// FINAL CONFIGURATION — JANUARY 06, 2026
// PURE HDR PHILOSOPHY — NO HOLLYWOOD TRICKS
// BLOOM, VIGNETTE, GRAIN, FLARE — BANISHED
// ACCUMULATION + DENOISING + TONEMAP — ENABLED FOR CLARITY AND TRUTH
// CAMERA FULLY CONFIGURABLE — CENTRALIZED CONTROL
// THE EMPIRE SEES CLEARLY — PINK PHOTONS ETERNAL
// =============================================================================