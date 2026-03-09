#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Options Menu (Living World Edition)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
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
    inline CameraPerspective   CurrentPerspective   = CameraPerspective::ThirdPerson;
    inline GenrePreset         CurrentGenre         = GenrePreset::Simulation;

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
// Camera — Cinematic orbiting + adjustable parameters
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Camera {

    // ─── Core camera constants ──────────────────────────────────────────────
    inline constexpr glm::vec3 START_POSITION         { 0.0f, 4.5f, 20.0f };  // default orbit start
    inline constexpr float     DEFAULT_FOV            = 75.0f;               // vertical FOV (degrees)
    inline constexpr float     DEFAULT_NEAR           = 0.05f;
    inline constexpr float     DEFAULT_FAR            = 5000.0f;

    inline constexpr float     DEFAULT_APERTURE       = 2.8f;                 // f-stop for DoF
    inline constexpr float     DEFAULT_FOCUS_DISTANCE = 3.0f;                 // meters

    // ─── Cinematic orbiting settings ────────────────────────────────────────
    inline constexpr float     BASE_HEIGHT            = 4.5f;                 // vertical orbit center
    inline constexpr float     HEIGHT_SWING            = 2.8f;                 // vertical bob amplitude
    inline constexpr float     HEIGHT_FREQ             = 0.11f;                // vertical bob frequency

    inline constexpr float     BASE_DISTANCE          = 20.0f;                // distance from look target
    inline constexpr float     DISTANCE_SWING         = 4.5f;                 // distance variation amplitude
    inline constexpr float     DISTANCE_FREQ          = 0.08f;                // distance variation frequency

    inline constexpr float     LOOK_AT_Y_OFFSET       = 0.0f;               // look below horizon (toward ground/water)

    // ─── Runtime adjustable settings ────────────────────────────────────────
    inline float               CurrentFOV             = DEFAULT_FOV;
    inline float               MinFOV                 = 30.0f;
    inline float               MaxFOV                 = 120.0f;

    inline float               MouseSensitivity       = 0.11f;
    inline bool                InvertMouseY           = false;

    inline float               MovementSpeed          = 5.2f;
    inline float               SprintMultiplier       = 1.8f;

    inline bool                EnableHeadBob          = true;
    inline float               HeadBobIntensity       = 0.035f;
    inline float               HeadBobFrequency       = 2.1f;

    inline bool                EnableBreathing        = true;
    inline float               BreathingIntensity     = 0.012f;
    inline float               BreathingFrequency     = 0.18f;

    inline bool                EnableCameraShake      = true;

    // Depth of field / cinematic
    inline float               Aperture               = DEFAULT_APERTURE;
    inline float               FocusDistance          = DEFAULT_FOCUS_DISTANCE;

    // Movement speeds & sensitivities
    inline float               ZoomSensitivity        = 1.0f;
    inline float               DollySpeed             = 4.0f;
    inline float               CraneSpeed             = 3.0f;
    inline float               RackFocusSpeed         = 2.5f;

    // Cinematic orbiting speed multiplier (affects day/night sync too)
    inline float               OrbitSpeedMultiplier   = 1.0f;  // 1.0 = normal, >1 = faster orbit
}

// ─────────────────────────────────────────────────────────────────────────────
// Rendering & Performance
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Rendering {
    inline int     INTERNAL_WIDTH           = 1920;
    inline int     INTERNAL_HEIGHT          = 1080;

    inline constexpr bool    ACCUMULATION             = true;
    inline constexpr bool    ADAPTIVE_SAMPLING        = true;
    inline constexpr int     MAX_RAY_RECURSION        = 8;
    inline constexpr float   EXPOSURE                 = 0.2f;
    inline constexpr bool    ENABLE_TONEMAP           = true;
    inline constexpr int     DISPATCH_GROUP_SIZE      = 16;

    // Adaptive quality controls
    inline bool    EnableAdaptiveQuality    = true;   // checkbox
    inline int     MaxSamplesPerPixel       = 4;      // slider: 1–8
    inline int     MaxRayRecursion          = 10;     // slider: 4–16
    inline float   QualityHeadroomThreshold = 0.75f;  // 0.6–0.9 (fraction of frame budget under which we boost)
    inline float   MaxGPULoadPercent        = 95.0f;  // never exceed this % of target frame time

    // Temporal accumulation strength (0.0 = no accumulation, 1.0 = freeze)
    inline float   TemporalBlendStrength    = 0.92f;  // slider: 0.0–1.0
}

// ─────────────────────────────────────────────────────────────────────────────
// Living World — Environment & Weather Controls
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::LivingWorld {

    // Day/Night Cycle
    inline bool              EnableDayNightCycle      = true;
    inline float             DayLengthSeconds         = 60.0f;   // full day in real seconds 1200
    inline float             CycleSpeedMultiplier     = 1.0f;      // 1.0 = real-time, >1 = faster, <1 = slower
    inline float             CurrentTimeOfDay         = 12.0f;     // 0..24 hours (updated by sim)

    // Sun & Moon
    inline bool              SunEnabled               = true;
    inline glm::vec3         SunColor                 = glm::vec3(1.0f, 0.96f, 0.88f);
    inline float             SunIntensityDay          = 12.0f;
    inline float             SunIntensityNight        = 0.1f;

    inline bool              MoonEnabled              = true;
    inline glm::vec3         MoonColor                = glm::vec3(0.9f, 0.95f, 1.0f);
    inline float             MoonIntensity            = 2.0f;

    // Atmosphere & Weather
    inline float             FogDensity               = 0.0008f;   // km⁻¹ baseline
    inline float             CloudCoverage            = 0.4f;
    inline float             CloudAnimationSpeed      = 0.08f;

    inline float             WindStrength             = 0.6f;      // 0..1 — affects grass sway, clouds
    inline glm::vec3         WindDirection            = glm::normalize(glm::vec3(0.7f, 0.0f, 0.3f));

    inline float             TemperatureC             = 22.0f;     // -50..50 — tints grass/sky
    inline float             Humidity                 = 0.65f;     // 0..1 — wetness, fog density
    inline float             PrecipitationFactor      = 0.0f;      // 0..1 — rain/snow intensity
    inline float             AirPressureKPa           = 101.3f;    // ~90..110 — weather pressure

    // Ground / Vegetation
    inline bool              EnableGrassSway          = true;
    inline float             GrassSwayAmplitude       = 0.12f;
    inline float             GrassWetShineBoost       = 1.8f;
    inline float             TemperatureColorShift    = 0.4f;

    // Debug & Visualization
    inline uint32_t          DebugFlags               = 0;         // bitfield — passed to shader
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