#pragma once

// =============================================================================
// AMOURANTHRTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial (gzac5314@gmail.com)
// AMOURANTH FOREVER 💖
// =============================================================================

#include <glm/glm.hpp>

// ─────────────────────────────────────────────────────────────────────────────
// Window & Display
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Window {
    inline int     DEFAULT_WIDTH            = 1920;
    inline int     DEFAULT_HEIGHT           = 1080;
    inline bool    START_FULLSCREEN         = false;
    inline bool    ALLOW_RESIZE             = true;
    inline bool    VSYNC                    = false;      // FIFO vs IMMEDIATE
    inline bool    BORDERLESS               = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Camera — all defaults & feel
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Camera {
    inline glm::vec3 START_POSITION         = glm::vec3(0.0f, 1.8f, 10.0f);
    inline float     DEFAULT_FOV            = 75.0f;
    inline float     DEFAULT_NEAR           = 0.1f;
    inline float     DEFAULT_FAR            = 1000.0f;

    inline float     DEFAULT_APERTURE       = 2.8f;       // f-stop
    inline float     DEFAULT_FOCUS_DISTANCE = 8.0f;       // meters

    inline float     MOUSE_SENSITIVITY      = 0.12f;
    inline bool      INVERT_MOUSE_Y         = false;
    inline float     MOVEMENT_SPEED         = 12.0f;      // m/s
    inline float     SPRINT_MULTIPLIER      = 2.5f;
    inline float     ZOOM_SENSITIVITY       = 1.5f;

    inline bool      ENABLE_HEAD_BOB        = true;
    inline float     HEAD_BOB_INTENSITY     = 0.06f;
    inline float     HEAD_BOB_FREQUENCY     = 2.0f;

    inline bool      ENABLE_BREATHING       = true;
    inline float     BREATHING_INTENSITY    = 0.025f;

    inline bool      ENABLE_CAMERA_SHAKE    = true;

    inline float     DOLLY_SPEED            = 8.0f;
    inline float     CRANE_SPEED            = 6.0f;
    inline float     RACK_FOCUS_SPEED       = 4.5f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Rendering & Performance
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Rendering {
    inline bool    ACCUMULATION             = true;      // temporal reprojection
    inline bool    ADAPTIVE_SAMPLING        = true;
    inline int     MAX_RAY_RECURSION        = 8;
    inline float   EXPOSURE                 = 0.2f;      // HDR → screen brightness
    inline bool    ENABLE_TONEMAP           = true;      // Reinhard / ACES
    inline int     DISPATCH_GROUP_SIZE      = 16;        // compute local size
}

// ─────────────────────────────────────────────────────────────────────────────
// Sky & Day/Night Cycle (Earth-realistic)
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Sky {
    inline bool    ENABLE_DAY_NIGHT_CYCLE   = true;
    inline float   DAY_LENGTH_SECONDS       = 1200.0f;   // 20 min full cycle
    inline float   CYCLE_SPEED              = 1.0f;      // multiplier
    inline float   START_TIME_OF_DAY        = 12.0f;     // noon

    inline bool    SUN_ENABLED              = true;
    inline glm::vec3 SUN_COLOR              = glm::vec3(1.0f, 0.96f, 0.88f);
    inline float   SUN_INTENSITY_DAY        = 12.0f;
    inline float   SUN_INTENSITY_NIGHT      = 0.1f;

    inline bool    MOON_ENABLED             = true;
    inline glm::vec3 MOON_COLOR             = glm::vec3(0.9f, 0.95f, 1.0f);
    inline float   MOON_INTENSITY           = 2.0f;

    inline float   FOG_DENSITY              = 0.0008f;
    inline float   CLOUD_DENSITY            = 0.4f;

    inline glm::vec4 SKY_ZENITH_DAY         = glm::vec4(0.3f, 0.55f, 1.0f, 1.0f);
    inline glm::vec4 SKY_HORIZON_DAY        = glm::vec4(0.6f, 0.8f, 1.0f, 1.0f);
    inline glm::vec4 SKY_ZENITH_NIGHT       = glm::vec4(0.01f, 0.02f, 0.05f, 1.0f);
    inline glm::vec4 SKY_HORIZON_NIGHT      = glm::vec4(0.03f, 0.03f, 0.08f, 1.0f);

    inline glm::vec4 GROUND_COLOR_DAY       = glm::vec4(0.18f, 0.35f, 0.12f, 1.0f);
    inline glm::vec4 GROUND_COLOR_NIGHT     = glm::vec4(0.08f, 0.12f, 0.18f, 1.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// LAS (Acceleration Structures)
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::LAS {
    inline bool    SYNC_REBUILD             = true;      // true = block main thread
                                                          // false = async thread (future)
    inline bool    ENABLE_TLAS              = true;
    inline bool    ENABLE_BLAS              = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio — matches SDL3.hpp usage exactly
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Audio {
    inline bool    ENABLE_HAPTICS_FEEDBACK  = true;      // controller rumble (added)
    inline int     SAMPLE_RATE              = 48000;     
    inline int     CHANNELS                 = 2;         
    inline int     BUFFER_SIZE              = 2048;      // latency vs stability
}

// ─────────────────────────────────────────────────────────────────────────────
// Debug & Tools
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Debug {
    inline bool    ENABLE_VALIDATION_LAYERS = true;
    inline bool    ENABLE_VERBOSE_LOGGING   = false;
    inline bool    DRAW_WIREFRAME           = false;
}