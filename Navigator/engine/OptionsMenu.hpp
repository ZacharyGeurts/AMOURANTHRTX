#pragma once

// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial (gzac5314@gmail.com)
// AMOURANTH FOREVER 💖
// =============================================================================

#include <glm/glm.hpp>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// Game Style & Perspective
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::GameStyle
{
    enum class DimensionMode : uint32_t
    {
        TextOnly       = 0,
        Pure2D         = 1,
        TwoPointFiveD  = 2,
        Full3D         = 3
    };

    enum class CameraPerspective : uint32_t
    {
        FirstPerson       = 0,
        ThirdPerson       = 1,
        TopDown           = 2,
        Isometric         = 3,
        SideScroller      = 4,
        Orthographic2D    = 5,
        TextAdventure     = 6
    };

    enum class GenrePreset : uint32_t
    {
        None              = 0,
        FPS               = 1,
        ThirdPersonAction = 2,
        Platformer        = 3,
        Metroidvania      = 4,
        TopDownRPG        = 5,
        TwinStickShooter  = 6,
        SurvivalHorror    = 7,
        Roguelike         = 8,
        TextAdventure     = 9,
        Shmup             = 10,
        Racing            = 11,
        Puzzle            = 12,
        Fighting          = 13,
        Sports            = 14,
        Simulation        = 15,
        Strategy          = 16,
        MMORPG            = 17,
        PartyGame         = 18
    };

    inline DimensionMode       CurrentDimension     = DimensionMode::Full3D;
    inline CameraPerspective   CurrentPerspective   = CameraPerspective::FirstPerson;
    inline GenrePreset         CurrentGenre         = GenrePreset::FPS;

    inline bool Is3D()          { return CurrentDimension == DimensionMode::Full3D; }
    inline bool Is25D()         { return CurrentDimension == DimensionMode::TwoPointFiveD; }
    inline bool Is2D()          { return CurrentDimension <= DimensionMode::TwoPointFiveD && CurrentDimension != DimensionMode::TextOnly; }
    inline bool IsTextMode()    { return CurrentDimension == DimensionMode::TextOnly; }
    inline bool IsFirstPerson() { return CurrentPerspective == CameraPerspective::FirstPerson; }
    inline bool IsThirdPerson() { return CurrentPerspective == CameraPerspective::ThirdPerson; }
    inline bool IsTopDown()     { return CurrentPerspective == CameraPerspective::TopDown; }
    inline bool IsIsometric()   { return CurrentPerspective == CameraPerspective::Isometric; }
    inline bool IsSideScroller(){ return CurrentPerspective == CameraPerspective::SideScroller; }
    inline bool IsOrthographic2D() { return CurrentPerspective == CameraPerspective::Orthographic2D; }
    inline bool IsTextAdventureMode() { return CurrentPerspective == CameraPerspective::TextAdventure; }
}

// ─────────────────────────────────────────────────────────────────────────────
// Window & Display
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Window {
    inline constexpr int     DEFAULT_WIDTH            = 1920;
    inline constexpr int     DEFAULT_HEIGHT           = 1080;
    inline constexpr bool    START_FULLSCREEN         = false;
    inline constexpr bool    ALLOW_RESIZE             = true;
    inline constexpr bool    VSYNC                    = false;
    inline constexpr bool    BORDERLESS               = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Camera
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Camera {

    inline constexpr glm::vec3 START_POSITION    { 0.0f, 1.68f, 0.0f };
    inline constexpr float     DEFAULT_FOV       = 90.0f;
    inline constexpr float     DEFAULT_NEAR      = 0.05f;
    inline constexpr float     DEFAULT_FAR       = 5000.0f;

    inline constexpr float     DEFAULT_APERTURE       = 2.8f;
    inline constexpr float     DEFAULT_FOCUS_DISTANCE = 3.0f;

    inline constexpr bool  ENABLE_HEAD_BOB        = true;
    inline constexpr float HEAD_BOB_INTENSITY     = 0.035f;
    inline constexpr float HEAD_BOB_FREQUENCY     = 2.1f;

    inline constexpr bool  ENABLE_BREATHING       = true;
    inline constexpr float BREATHING_INTENSITY    = 0.012f;
    inline constexpr float BREATHING_FREQUENCY    = 0.18f;

    inline constexpr bool  ENABLE_CAMERA_SHAKE    = true;

    inline constexpr float SPRINT_FOV_BOOST       = 5.0f;
    inline constexpr float CROUCH_EYE_DROP        = 0.45f;

    inline float MOUSE_SENSITIVITY   = 0.11f;
    inline bool  INVERT_MOUSE_Y      = false;

    inline float MOVEMENT_SPEED      = 5.2f;
    inline float SPRINT_MULTIPLIER   = 1.8f;

    inline float ZOOM_SENSITIVITY    = 1.0f;

    inline constexpr float VIEWMODEL_FOV          = 70.0f;
    inline constexpr float VIEWMODEL_SCALE        = 0.85f;
    inline constexpr float WEAPON_SWAY_INTENSITY  = 0.8f;

    inline constexpr float DOLLY_SPEED       = 4.0f;
    inline constexpr float CRANE_SPEED       = 3.0f;
    inline constexpr float RACK_FOCUS_SPEED  = 2.5f;

} // namespace Options::Camera

// ─────────────────────────────────────────────────────────────────────────────
// Rendering & Performance
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Rendering {
    inline constexpr int     INTERNAL_WIDTH           = 1920;
    inline constexpr int     INTERNAL_HEIGHT          = 1080;

    inline constexpr bool    ACCUMULATION             = true;
    inline constexpr bool    ADAPTIVE_SAMPLING        = true;
    inline constexpr int     MAX_RAY_RECURSION        = 8;
    inline constexpr float   EXPOSURE                 = 0.2f;
    inline constexpr bool    ENABLE_TONEMAP           = true;
    inline constexpr int     DISPATCH_GROUP_SIZE      = 16;
}

// ─────────────────────────────────────────────────────────────────────────────
// Sky & Day/Night Cycle
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Sky {
    inline constexpr bool    ENABLE_DAY_NIGHT_CYCLE   = true;
    inline constexpr float   DAY_LENGTH_SECONDS       = 1200.0f;
    inline constexpr float   CYCLE_SPEED              = 1.0f;
    inline constexpr float   START_TIME_OF_DAY        = 12.0f;

    inline constexpr bool    SUN_ENABLED              = true;
    inline constexpr glm::vec3 SUN_COLOR              = glm::vec3(1.0f, 0.96f, 0.88f);
    inline constexpr float   SUN_INTENSITY_DAY        = 12.0f;
    inline constexpr float   SUN_INTENSITY_NIGHT      = 0.1f;

    inline constexpr bool    MOON_ENABLED             = true;
    inline constexpr glm::vec3 MOON_COLOR             = glm::vec3(0.9f, 0.95f, 1.0f);
    inline constexpr float   MOON_INTENSITY           = 2.0f;

    inline constexpr float   FOG_DENSITY              = 0.0008f;
    inline constexpr float   CLOUD_DENSITY            = 0.4f;

    inline constexpr glm::vec4 SKY_ZENITH_DAY         = glm::vec4(0.3f, 0.55f, 1.0f, 1.0f);
    inline constexpr glm::vec4 SKY_HORIZON_DAY        = glm::vec4(0.6f, 0.8f, 1.0f, 1.0f);
    inline constexpr glm::vec4 SKY_ZENITH_NIGHT       = glm::vec4(0.01f, 0.02f, 0.05f, 1.0f);
    inline constexpr glm::vec4 SKY_HORIZON_NIGHT      = glm::vec4(0.03f, 0.03f, 0.08f, 1.0f);

    inline constexpr glm::vec4 GROUND_COLOR_DAY       = glm::vec4(0.18f, 0.35f, 0.12f, 1.0f);
    inline constexpr glm::vec4 GROUND_COLOR_NIGHT     = glm::vec4(0.08f, 0.12f, 0.18f, 1.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// LAS (Acceleration Structures)
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::LAS {
    inline constexpr bool    SYNC_REBUILD             = true;
    inline constexpr bool    ENABLE_TLAS              = true;
    inline constexpr bool    ENABLE_BLAS              = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Audio {
    inline constexpr bool    ENABLE_HAPTICS_FEEDBACK  = true;
    inline constexpr int     SAMPLE_RATE              = 48000;
    inline constexpr int     CHANNELS                 = 2;
    inline constexpr int     BUFFER_SIZE              = 2048;
}

// ─────────────────────────────────────────────────────────────────────────────
// Debug & Tools
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Debug {
    inline constexpr bool    ENABLE_VALIDATION_LAYERS = true;
    inline constexpr bool    ENABLE_VERBOSE_LOGGING   = false;
    inline constexpr bool    DRAW_WIREFRAME           = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Input Controls — remappable in menu (keys handled in InputManager)
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Input {
    inline bool INVERT_MOUSE_Y        = false;
    inline float MOUSE_SENSITIVITY    = 0.11f;
    inline float MOVEMENT_SPEED       = 5.2f;
    inline float SPRINT_MULTIPLIER    = 1.8f;
}