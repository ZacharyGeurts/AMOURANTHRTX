// include/engine/GLOBAL/OptionsMenu.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2026 — VALHALLA v∞ TURBO — APOCALYPSE FINAL — JANUARY 07, 2026
// OPTIONS MENU — MINIMAL & PURE | FOCUSED ON RTX REALM & CELESTIAL SYSTEM
// CAMERA FULLY PRESERVED | AUDIO SETTINGS RESTORED | NO HOLLYWOOD POST
// PINK PHOTONS ETERNAL — EMPIRE UNBROKEN — AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include <cstdint>
#include <glm/glm.hpp>

namespace Options {

// ── SPLASH SCREEN
namespace Splash {
    constexpr bool  ENABLE_SACRIFICIAL_SPLASH = true;
    constexpr float SPLASH_DURATION_SECONDS  = 2.0f;
    constexpr bool  ALLOW_EARLY_EXIT         = true;
}

// ── PERFORMANCE
namespace Performance {
    constexpr uint32_t MAX_FRAMES_IN_FLIGHT  = 2;
    constexpr bool     ENABLE_FPS_COUNTER    = true;
    constexpr bool     OVERCLOCK_RENDERER    = true;
}

// ── WINDOW
namespace Window {
    constexpr uint32_t DEFAULT_WIDTH         = 3840;
    constexpr uint32_t DEFAULT_HEIGHT        = 2160;
    constexpr bool     START_FULLSCREEN      = false;
    constexpr bool     ALLOW_RESIZE          = true;
}

// ── AUDIO — RESTORED FOR HAPTICS
namespace Audio {
    constexpr bool ENABLE_HAPTICS_FEEDBACK   = true;  // Controller rumble on connect
}

// ── RTX CORE
namespace RTX {
    constexpr bool ENABLE_ACCUMULATION       = true;
    constexpr bool ENABLE_ADAPTIVE_SAMPLING  = true;
    constexpr uint32_t MAX_RAY_RECURSION     = 12;
}

// ── CELESTIAL SYSTEM — FULLY CONFIGURABLE
namespace Sky {
    constexpr bool  ENABLE_DAY_NIGHT_CYCLE   = true;
    constexpr float CYCLE_SPEED              = 0.05f;
    constexpr float START_TIME_OF_DAY        = 12.0f;

    constexpr uint32_t MAX_SUNS              = 4;
    constexpr bool     SUN_ENABLED[MAX_SUNS] = { true, false, false, false };
    constexpr glm::vec3 SUN_DIRECTION[MAX_SUNS] = {
        glm::vec3(0.3f, 0.8f, 0.5f),
        glm::vec3(-0.5f, 0.6f, -0.3f),
        glm::vec3(0.0f),
        glm::vec3(0.0f)
    };
    constexpr glm::vec3 SUN_COLOR[MAX_SUNS] = {
        glm::vec3(1.0f, 0.95f, 0.85f) * 12.0f,
        glm::vec3(1.0f, 0.6f, 0.4f) * 8.0f,
        glm::vec3(0.0f),
        glm::vec3(0.0f)
    };
    constexpr float    SUN_INTENSITY[MAX_SUNS] = { 12.0f, 8.0f, 0.0f, 0.0f };

    constexpr uint32_t MAX_MOONS             = 4;
    constexpr bool     MOON_ENABLED[MAX_MOONS] = { true, false, false, false };
    constexpr glm::vec3 MOON_DIRECTION[MAX_MOONS] = {
        glm::vec3(-0.3f, -0.8f, -0.5f),
        glm::vec3(0.4f, -0.7f, 0.6f),
        glm::vec3(0.0f),
        glm::vec3(0.0f)
    };
    constexpr glm::vec3 MOON_COLOR[MAX_MOONS] = {
        glm::vec3(0.8f, 0.85f, 1.0f) * 2.0f,
        glm::vec3(1.0f, 0.8f, 0.6f) * 1.5f,
        glm::vec3(0.0f),
        glm::vec3(0.0f)
    };
    constexpr float    MOON_INTENSITY[MAX_MOONS] = { 2.0f, 1.5f, 0.0f, 0.0f };
    constexpr float    MOON_SIZE[MAX_MOONS] = { 0.15f, 0.12f, 0.0f, 0.0f };

    constexpr bool     ENABLE_STARS          = true;
    constexpr float    STAR_DENSITY          = 0.0012f;
    constexpr float    STAR_BASE_INTENSITY   = 0.8f;
    constexpr glm::vec3 STAR_COLOR_BASE      = glm::vec3(1.0f);
    constexpr float    STAR_TWINKLE_INTENSITY = 0.6f;
    constexpr float    STAR_TWINKLE_SPEED    = 0.8f;
    constexpr int      TWINKLE_LAYERS        = 3;

    constexpr glm::vec3 SKY_ZENITH_DAY       = glm::vec3(0.3f, 0.55f, 1.0f);
    constexpr glm::vec3 SKY_HORIZON_DAY      = glm::vec3(0.6f, 0.8f, 1.0f);
    constexpr glm::vec3 SKY_ZENITH_NIGHT     = glm::vec3(0.01f, 0.02f, 0.05f);
    constexpr glm::vec3 SKY_HORIZON_NIGHT    = glm::vec3(0.03f, 0.03f, 0.08f);
}

// ── CAMERA — FULLY PRESERVED (DO NOT TOUCH)
namespace Camera {
    constexpr glm::vec3 START_POSITION       = glm::vec3(0.0f, 5.0f, 15.0f);
    constexpr float    DEFAULT_FOV           = 80.0f;
    constexpr float    DEFAULT_APERTURE      = 16.0f;
    constexpr float    DEFAULT_FOCUS_DISTANCE = 10.0f;

    constexpr float    MOUSE_SENSITIVITY     = 0.14f;
    constexpr bool     INVERT_MOUSE_LOOK     = false;
    constexpr float    ZOOM_SENSITIVITY      = 1.1f;

    constexpr float    MOVEMENT_SPEED        = 14.0f;
    constexpr float    SPRINT_MULTIPLIER     = 2.8f;

    constexpr bool     ENABLE_HEAD_BOB       = true;
    constexpr float    HEAD_BOB_FREQUENCY    = 2.2f;
    constexpr float    HEAD_BOB_INTENSITY    = 0.08f;

    constexpr bool     ENABLE_BREATHING      = true;
    constexpr float    BREATHING_INTENSITY   = 0.03f;

    constexpr bool     ENABLE_CAMERA_SHAKE   = true;
}

// ── VR SUPPORT — FUTURE-PROOF
namespace VR {
    constexpr bool     ENABLE_VR             = false;
    constexpr bool     ENABLE_STEREO_RENDERING = false;
    constexpr float    IPD                   = 0.064f; // Inter-pupillary distance (m)
    constexpr bool     ENABLE_ASYMMETRIC_PROJECTION = false;
}

// ── DEBUG
namespace Debug {
    constexpr bool SHOW_FPS_OVERLAY          = true;
    constexpr bool ENABLE_VALIDATION_LAYERS  = false;
}

} // namespace Options

// =============================================================================
// FINAL CONFIGURATION — JANUARY 07, 2026
// CAMERA FULLY PRESERVED — DOF, head bob, breathing, shake, zoom
// AUDIO SETTINGS RESTORED — haptics feedback
// FULL CELESTIAL SYSTEM — suns, moons, stars, day/night
// VR READY — toggle when implemented
// Empire complete — pink photons eternal — AMOURANTH FOREVER 💖
// =============================================================================