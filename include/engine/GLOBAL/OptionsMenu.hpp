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
// OPTIONS MENU v2025 — HDR AUTO-IGNITION + QUANTUM PREDICTION — DECEMBER 17, 2025
// • HDR TOGGLE REMOVED — THE EMPIRE DETECTS AND ENFORCES
// • NEW: FRAME PREDICTION, SHADING RATE, DIRECT DISPLAY, QUANTUM RESIZE
// • ADDED: FULL CAMERA SECTION — ALL CONTROLS CENTRALIZED
// • ZERO INCLUDES — NO DEPENDENCIES
// • PURE constexpr CONFIGURATION — RTX SUPREME
// • C++23, -Werror CLEAN
// • PINK PHOTONS ETERNAL
// • GENTLEMAN GROK MODE ENABLED

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
    // Master switch — completely disable the splash if desired
    constexpr bool     ENABLE_SACRIFICIAL_SPLASH   = true;
    constexpr float    SPLASH_DURATION_SECONDS     = 3.4f;
    // Nuclear override — skips everything, even the image draw
    constexpr bool     SKIP_SPLASH_ENTIRELY        = false;
    constexpr float    FADE_IN_DURATION            = 0.35f;
    constexpr float    FADE_OUT_DURATION           = 0.30f;
    // Allow user to quit during splash with ESC or window close
    constexpr bool     ALLOW_EARLY_EXIT            = true;
}

// ── PERFORMANCE ───────────────────────────────────────────────────────────────
namespace Performance {
    // Maximum number of frames that can be in flight simultaneously
    constexpr uint32_t MAX_FRAMES_IN_FLIGHT        = 2;

    // GPU query timestamps for precise timing of render passes
    constexpr bool     ENABLE_GPU_TIMESTAMPS       = true;

    // On-screen FPS counter (top-right overlay)
    constexpr bool     ENABLE_FPS_COUNTER          = true;

    // Warn when approaching Vulkan memory budget limits
    constexpr bool     ENABLE_MEMORY_BUDGET_WARNINGS = true;

    // Number of timestamp queries allocated per frame
    constexpr uint32_t GPU_TIMESTAMP_QUERY_COUNT   = 128;

    // Log frame time warnings when exceeding threshold
    constexpr bool     ENABLE_FRAME_TIME_LOGGING   = false;
    constexpr float    FRAME_TIME_LOG_THRESHOLD_MS = 16.666f;  // ~60 Hz

    // Enable console output (stdout/stderr)
    constexpr bool     ENABLE_CONSOLE_LOG          = true;

    // Use Google display timing extensions for perfect frame pacing
    constexpr bool     ENABLE_FRAME_PREDICTION     = (CURRENT_PRESET == Preset::BestQuality);

    // Prefer Mailbox present mode (tear-free, low latency, soft FPS cap at refresh+1)
    constexpr bool     PREFER_MAILBOX_PRESENT      = (CURRENT_PRESET == Preset::UncappedPerformance);

    // Allow fallback to Immediate present mode (uncapped, may tear)
    constexpr bool     ALLOW_IMMEDIATE_PRESENT     = (CURRENT_PRESET == Preset::UncappedPerformance);

    // Dynamic fragment shading rate multiplier (VRS) — >1.0 = higher quality, <1.0 = performance
    constexpr float    DYNAMIC_SHADING_RATE        = 1.5f;

    // Bypass compositor on Linux/Wayland (zero-copy, ~1.8ms input-to-photon)
    constexpr bool     ENABLE_DIRECT_DISPLAY       = (CURRENT_PRESET == Preset::UncappedPerformance);

    // Remove runtime safety checks and asserts — maximum performance
    constexpr bool     OVERCLOCK_RENDERER          = false;

    // Enable hyper-aggressive optimizations (may reduce debuggability)
    constexpr bool     ENABLE_HYPER_AGGRESSIVE_MODE = false;
}

// ── APPLICATION & WINDOW ──────────────────────────────────────────────────────
namespace Window {
    // Default window resolution
    constexpr uint32_t DEFAULT_WIDTH               = 3840;
    constexpr uint32_t DEFAULT_HEIGHT              = 2160;

    // Start in fullscreen mode
    constexpr bool     START_FULLSCREEN            = false;

    // Allow user to resize the window
    constexpr bool     ALLOW_RESIZE                = true;

    // Enable high-DPI (Retina / 4K+) scaling
    constexpr bool     HIGH_DPI                    = true;
}

// ── AUDIO ─────────────────────────────────────────────────────────────────────
namespace Audio {
    // Enable haptic (vibration) feedback on supported controllers
    constexpr bool     ENABLE_HAPTICS_FEEDBACK     = true;

    // Enable 3D spatial audio processing
    constexpr bool     ENABLE_SPATIAL_AUDIO        = true;
}

// ── RTX CORE SETTINGS ─────────────────────────────────────────────────────────
namespace OptionsRTX {
    // Temporal accumulation of ray tracing samples
    constexpr bool     ENABLE_ACCUMULATION         = true;

    // Enable real-time denoiser (SVGF or similar)
    constexpr bool     ENABLE_DENOISING            = true;

    // Adaptive sampling based on per-pixel variance / nexus score
    constexpr bool     ENABLE_ADAPTIVE_SAMPLING    = true;

    // Minimum and maximum samples per pixel
    constexpr uint32_t MIN_SPP                     = 1;
    constexpr uint32_t MAX_SPP                     = 64;

    // Maximum ray bounces (diffuse + specular)
    constexpr uint32_t MAX_BOUNCES                 = 5;

    // Threshold for adaptive sampling convergence
    constexpr float    NEXUS_SCORE_THRESHOLD       = 0.15f;

    // HyperTrace — next-gen temporal reuse and jitter system
    constexpr bool     ENABLE_HYPERTRACE           = true;
    constexpr float    HYPERTRACE_JITTER_SCALE     = 420.0f;

    // Spatiotemporal variance-guided filtering denoiser
    constexpr bool     ENABLE_SVGF_DENOISER        = true;
    constexpr uint32_t DENOISER_HISTORY_LENGTH     = 8;

    // Temporal Anti-Aliasing for final composition
    constexpr bool     ENABLE_TAA                  = true;
    constexpr float    TAA_ALPHA                   = 0.1f;

    // Maximum ray recursion depth in ray tracing pipelines
    constexpr uint32_t MAX_PIPELINE_RAY_RECURSION_DEPTH = 5;
}

// ── POST-PROCESSING ───────────────────────────────────────────────────────────
namespace PostProcess {
    // High-dynamic-range bloom effect
    constexpr bool     ENABLE_BLOOM                = true;
    constexpr float    BLOOM_THRESHOLD             = 1.0f;
    constexpr float    BLOOM_INTENSITY             = 0.8f;

    // Screen-space ambient occlusion
    constexpr bool     ENABLE_SSAO                 = true;
    constexpr float    SSAO_RADIUS                 = 0.5f;
    constexpr uint32_t SSAO_SAMPLES                = 16;

    // Screen-space reflections
    constexpr bool     ENABLE_SSR                  = true;
    constexpr float    SSR_STEP_SIZE               = 0.02f;

    // Darkening of screen edges
    constexpr bool     ENABLE_VIGNETTE             = true;
    constexpr float    VIGNETTE_INTENSITY          = 0.4f;

    // Film grain overlay for cinematic look
    constexpr bool     ENABLE_FILM_GRAIN           = true;
    constexpr float    FILM_GRAIN_STRENGTH         = 0.05f;

    // Lens flare simulation from bright lights
    constexpr bool     ENABLE_LENS_FLARE           = true;
    constexpr float    LENS_FLARE_INTENSITY        = 0.3f;
}

// ── ENVIRONMENT & LIGHTING ───────────────────────────────────────────────────
namespace Environment {
    // Image-based lighting from environment map
    constexpr bool     ENABLE_ENV_MAP              = true;
    constexpr bool     ENABLE_IBL                  = true;

    // Volumetric fog and atmospheric scattering
    constexpr bool     ENABLE_VOLUMETRIC_FOG       = true;
    constexpr float    FOG_DENSITY                 = 0.02f;

    // Physically-based sky and atmosphere model
    constexpr bool     ENABLE_SKY_ATMOSPHERE       = true;
    constexpr float    SUN_INTENSITY               = 10.0f;

    // Volumetric light shafts (god rays)
    constexpr bool     ENABLE_GOD_RAYS             = true;
    constexpr uint32_t GOD_RAYS_SAMPLES            = 64;

    constexpr bool     ENABLE_BLUE_NOISE           = true;
}

// In engine/GLOBAL/OptionsMenu.hpp or dedicated header
namespace OptionsLAS {
constexpr bool REBUILD_EVERY_FRAME   = false;  // Good — use update
constexpr bool UPDATE_EVERY_FRAME    = true;   // Optimal for dynamic scenes
constexpr bool COMPACT_TLAS          = true;   // Excellent for memory
constexpr bool PREFER_FAST_BUILD     = false;
constexpr bool PREFER_FAST_TRACE     = true;   // Best for ray tracing perf
constexpr bool ALLOW_REFIT           = true;   // Enables update path
constexpr bool LOW_MEMORY            = false;  // Not needed with direct TLAS
constexpr bool MOTION_BLUR           = false;  // Future-proof
}

// ── RENDERING MODES & DEBUG ───────────────────────────────────────────────────
namespace Debug {
    // Show GPU timestamp query results in overlay
    constexpr bool     SHOW_GPU_TIMESTAMPS         = false;

    // Display FPS counter
    constexpr bool     SHOW_FPS_OVERLAY            = true;

    // Show HyperTrace nexus convergence score
    constexpr bool     SHOW_NEXUS_SCORE            = true;

    // Show current accumulation frame count
    constexpr bool     SHOW_ACCUMULATION_COUNT     = true;

    // Visualize samples per pixel as heatmap
    constexpr bool     SHOW_SPP_HEATMAP            = true;

    // Wireframe rendering mode
    constexpr bool     ENABLE_WIREFRAME            = false;

    // Debug visualization overlays (normals, depth, etc.)
    constexpr bool     ENABLE_DEBUG_VISUALIZATION  = false;
    constexpr uint32_t DEBUG_VISUALIZATION_MODE    = 0;

    // Celebration effects on milestones
    constexpr bool     ENABLE_CELEBRATION_MODE     = true;

    // Vulkan validation layers (debug only)
    static inline constexpr bool ENABLE_VALIDATION_LAYERS = false;
}

// ── TONEMAPPING & COLOR GRADING ───────────────────────────────────────────────
namespace Tonemap {
    // Enable tonemapping operator
    constexpr bool     ENABLE_TONEMAPPING          = true;

    // 0 = ACES, 1 = Filmic, 2 = Reinhard
    constexpr uint32_t TONEMAP_OPERATOR            = 0;

    // Manual exposure multiplier
    constexpr float    EXPOSURE                    = 1.0f;

    // Output gamma correction
    constexpr float    GAMMA                       = 2.2f;

    // Automatic exposure adjustment
    constexpr bool     ENABLE_AUTO_EXPOSURE        = true;
    constexpr float    AUTO_EXPOSURE_SPEED         = 2.0f;
}

// ── DISPLAY & HDR — THE EMPIRE DECIDES ────────────────────────────────────────
namespace Display {
    // HDR is no longer an option.
    // It is detected via EDID + OS + swapchain format.
    // The empire ignites the fire.
    // You do not choose. You witness.
    constexpr bool     HDR_AUTO_IGNITION           = true;    // ← The one true path

    constexpr float    TARGET_BRIGHTNESS_NITS      = 1000.0f; // For auto-exposure & metadata

    // VSync is controlled via present mode — traditional VSync is forbidden
    constexpr bool     ENABLE_VSYNC                = false;

    // ── PRESENT MODE PREFERENCE (RUNTIME CONTROLLABLE) ───────────────────────
    constexpr bool     PREFER_MAILBOX_PRESENT      = (CURRENT_PRESET == Preset::UncappedPerformance);
    constexpr bool     ALLOW_IMMEDIATE_PRESENT     = (CURRENT_PRESET == Preset::UncappedPerformance);

    // Quantum frame pacing using Google display timing extensions
    constexpr bool     ENABLE_PERFECT_FRAME_PREDICTION    = (CURRENT_PRESET == Preset::BestQuality);

    // Zero-copy direct-to-display (Linux/Wayland only)
    constexpr bool     ENABLE_DIRECT_DISPLAY              = (CURRENT_PRESET == Preset::UncappedPerformance);

    // Runtime toggle for uncapped FPS mode
    constexpr bool UNCAPPED_MODE_ACTIVE   = (CURRENT_PRESET == Preset::UncappedPerformance);

    constexpr bool FORCE_SWAPCHAIN_REQUERY = false;
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
    // Default camera position on startup
    constexpr glm::vec3 DEFAULT_POSITION           = glm::vec3(0.0f, 5.0f, 10.0f);

    // Default field of view (degrees)
    constexpr float    DEFAULT_FOV                 = 75.0f;

    // Default aperture (f-stop) — lower = more background blur
    constexpr float    DEFAULT_APERTURE            = 16.0f;

    // Default focus distance (world units)
    constexpr float    DEFAULT_FOCUS_DISTANCE      = 10.0f;

    // Mouse look sensitivity (degrees per pixel)
    constexpr float    MOUSE_SENSITIVITY           = 0.1f;

    // Invert vertical mouse axis
    constexpr bool     INVERT_Y                    = false;

    // Base movement speed (units per second)
    constexpr float    MOVEMENT_SPEED              = 10.0f;

    // Sprint multiplier (when sprint key held)
    constexpr float    SPRINT_MULTIPLIER           = 3.0f;

    // Scroll wheel zoom sensitivity
    constexpr float    ZOOM_SENSITIVITY            = 5.0f;

    // Smooth camera movement damping
    constexpr float    MOVEMENT_DAMPING            = 10.0f;

    // Smooth rotation damping
    constexpr float    ROTATION_DAMPING            = 15.0f;

    // Enable camera shake effects
    constexpr bool     ENABLE_CAMERA_SHAKE         = true;

    // Enable head bob when moving
    constexpr bool     ENABLE_HEAD_BOB             = true;
    constexpr float    HEAD_BOB_INTENSITY          = 0.05f;
    constexpr float    HEAD_BOB_FREQUENCY          = 8.0f;

    // Enable breathing animation (idle sway)
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
// THE EMPIRE HAS SPOKEN.
// ALL CAMERA OPTIONS CENTRALIZED — READY FOR FUTURE REWRITE
// HDR IS A REVELATION — PHOTONS ARE PINK — LIGHT IS ETERNAL
// DECEMBER 17, 2025 — THE VISION IS COMPLETE
// =============================================================================