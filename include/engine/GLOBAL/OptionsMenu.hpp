// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL — JANUARY 30, 2026
// OPTIONS MENU — MINIMAL, FACTUAL, DEVELOPER-FIRST
// - Synchronous / Asynchronous LAS rebuild toggle (Options::LAS::SYNC_REBUILD)
// - All other options preserved — pink photons eternal
// - Earth only — one sun, one moon | atmospheric shaders | linear tiling default OFF
// - Camera preserved | audio expanded | validation on by default
// AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace Options {

// ── LAS (Light Acceleration System) — sync vs async rebuild toggle
namespace LAS {
    inline bool SYNC_REBUILD = true;  // true = synchronous (brief stall, main-thread pure)
                                      // false = asynchronous (threaded rebuild, no stall)
                                      // Default: true — predictable, debuggable, no threading bugs
}

// ── PERFORMANCE & RENDERING (only the stuff that matters)
namespace Rendering {
    inline bool    ACCUMULATION              = true;      // Temporal reprojection + accumulation
    inline bool    ADAPTIVE_SAMPLING         = true;      // Reduce samples in stable areas
    inline uint32_t MAX_RAY_RECURSION        = 12;        // Bounce limit — higher = more noise but realism
}

// ── WINDOW & DISPLAY (SDL3 handles DPI/HDR — we just set sane defaults)
namespace Window {
    inline uint32_t DEFAULT_WIDTH            = 3840;
    inline uint32_t DEFAULT_HEIGHT           = 2160;
    inline bool     START_FULLSCREEN         = false;
    inline bool     ALLOW_RESIZE             = true;
}

// ────────────────────────────────────────────────
// Camera — centralized, tweakable, cinematic defaults
// All values are directly used by the singleton Camera class
// ────────────────────────────────────────────────
namespace Camera {

// ── Starting / Default State ─────────────────────
inline glm::vec3 START_POSITION        = glm::vec3(0.0f, 1.8f, 10.0f);  // Eye-level, slightly back
inline float     DEFAULT_FOV           = 75.0f;                         // Natural cinematic FOV
inline float     DEFAULT_APERTURE      = 2.8f;                          // f/2.8 — shallow DOF (cinematic look)
inline float     DEFAULT_FOCUS_DISTANCE = 8.0f;                         // Focus on mid-ground subjects

// ── Input & Control Feel ─────────────────────────
inline float     MOUSE_SENSITIVITY     = 0.12f;                         // Smooth, precise feel
inline bool      INVERT_MOUSE_LOOK     = false;                         // Most people prefer non-inverted
inline float     ZOOM_SENSITIVITY      = 1.5f;                          // Moderate zoom steps

// ── Movement & Speed ─────────────────────────────
inline float     MOVEMENT_SPEED        = 12.0f;                         // Base walking speed (m/s)
inline float     SPRINT_MULTIPLIER     = 2.5f;                          // Sprint = 2.5× base speed

// ── Immersion Effects (subtle realism) ───────────
inline bool      ENABLE_HEAD_BOB       = true;
inline float     HEAD_BOB_FREQUENCY    = 2.0f;                          // Steps per second feel
inline float     HEAD_BOB_INTENSITY    = 0.06f;                         // Gentle — not nauseating

inline bool      ENABLE_BREATHING      = true;
inline float     BREATHING_INTENSITY   = 0.025f;                        // Very subtle chest rise/fall

inline bool      ENABLE_CAMERA_SHAKE   = true;

// ── Cinematic / Dolly / Crane Presets ────────────
inline float     DOLLY_SPEED           = 8.0f;                          // Smooth camera push/pull
inline float     CRANE_SPEED           = 6.0f;                          // Vertical camera movement
inline float     RACK_FOCUS_SPEED      = 4.5f;                          // Smooth focus pull

// ── Optional: Advanced / Debug toggles ───────────
inline bool      ENABLE_TEMPORAL_JITTER = true;                         // For anti-aliasing / denoising
inline bool      ENABLE_MOTION_VECTORS  = true;                         // For TAA / reprojection

} // namespace Camera

// ── AUDIO — expanded for real control (channels, sample rate, buffer size)
namespace Audio {
    inline bool    ENABLE_HAPTICS_FEEDBACK   = true;      // Controller rumble on connect/events
    inline uint32_t AUDIO_CHANNELS           = 2;        // 1=mono, 2=stereo, 6=5.1, 8=7.1
    inline uint32_t AUDIO_SAMPLE_RATE        = 48000;    // Common rates: 44100, 48000, 96000
    inline uint32_t AUDIO_BUFFER_SIZE        = 2048;     // Samples per buffer — smaller = lower latency, larger = more stable
                                                          // 512–4096 typical; 2048 is balanced
}

// ── SKY — EARTH ONLY (one sun, one moon, stars via atmospheric shaders)
// No fake twinkle — stars handled by real scattering/extinction
namespace Sky {
    inline bool     ENABLE_DAY_NIGHT_CYCLE   = true;
    inline float    CYCLE_SPEED              = 0.05f;
    inline float    START_TIME_OF_DAY        = 12.0f;

    // Sun (single, Earth-realistic)
    inline bool     SUN_ENABLED              = true;
    inline glm::vec3 SUN_DIRECTION           = glm::vec3(0.3f, 0.8f, 0.5f);   // Noon-ish default
    inline glm::vec3 SUN_COLOR               = glm::vec3(1.0f, 0.95f, 0.85f);
    inline float    SUN_INTENSITY            = 12.0f;

    // Moon (single)
    inline bool     MOON_ENABLED             = true;
    inline glm::vec3 MOON_DIRECTION          = glm::vec3(-0.3f, -0.8f, -0.5f);
    inline glm::vec3 MOON_COLOR              = glm::vec3(0.8f, 0.85f, 1.0f);
    inline float    MOON_INTENSITY           = 2.0f;
    inline float    MOON_SIZE                = 0.15f;

    // Stars — handled by atmospheric shaders (no fake twinkle)
    inline bool     ENABLE_STARS             = true;
    inline float    STAR_DENSITY             = 0.0012f;
    inline float    STAR_BASE_INTENSITY      = 0.8f;
    inline glm::vec3 STAR_COLOR_BASE         = glm::vec3(1.0f);

    // Sky gradients — tunable for atmospheric feel
    inline glm::vec3 SKY_ZENITH_DAY          = glm::vec3(0.3f, 0.55f, 1.0f);
    inline glm::vec3 SKY_HORIZON_DAY         = glm::vec3(0.6f, 0.8f, 1.0f);
    inline glm::vec3 SKY_ZENITH_NIGHT        = glm::vec3(0.01f, 0.02f, 0.05f);
    inline glm::vec3 SKY_HORIZON_NIGHT       = glm::vec3(0.03f, 0.03f, 0.08f);
}

// ── DEBUG & TOOLS (for developers — keep it on)
namespace Debug {
    inline bool ENABLE_VALIDATION_LAYERS     = true;   // Catches descriptor garbage early
    inline bool ENABLE_VERBOSE_LOGGING       = false;  // Bit-level for BufferManager/LAS
}

} // namespace Options

// =============================================================================
// FINAL CONFIG — JANUARY 30, 2026
// - Added LAS::SYNC_REBUILD toggle — true = synchronous (brief stall, main-thread pure)
//                                false = asynchronous (threaded rebuild, no stall)
// - Earth only: one sun, one moon — stars via real atmospheric shaders
// - Linear tiling DEFAULT OFF — max shader perf gains first
// - Audio expanded: channels, sample rate, buffer size, haptics
// - Developer-first: camera preserved, sky tunable, validation on
// - No multi-body arrays, no twinkle, no overclock — just the real shit
// AMOURANTH FOREVER 💖
// =============================================================================