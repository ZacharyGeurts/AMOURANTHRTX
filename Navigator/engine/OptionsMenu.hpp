#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Options Menu (Living World Edition)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
//
// This file defines all user-configurable runtime options for the engine.
// Values are pushed directly to shaders via PushConstants and can be changed
// live through the in-game options menu, hotkeys, or config files.
//
// Comments explain:
// - What each setting does
// - Typical use cases
// - Performance impact (GPU/CPU cost)
// - How it interacts with rendering modes (2D canvas vs raymarched 3D vs hybrid)
// - Default values and why they were chosen
// =============================================================================

#include <glm/glm.hpp>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// Game Style & Perspective
// ─────────────────────────────────────────────────────────────────────────────
// Core visual and gameplay dimension + camera style.
// Directly affects whether the shader uses 2D SDF canvas, hybrid layering,
// or full raymarched 3D path.
// Also sets default UI layouts, controls feel, and atmosphere tone.
//
// Performance ranking:
// • Full3D + FirstPerson → highest GPU load (raymarching, complex lighting)
// • Pure2D + TopDown/Ortho → lowest GPU load (fast SDF painting)
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::GameStyle
{
    // DimensionMode — selects rendering pipeline path
    // Controls whether we run pure 2D SDF, hybrid 2D+3D, or full raymarched 3D.
    // Also influences geometry complexity, lighting model, and post-processing.
    enum class DimensionMode : uint32_t
    {
        TextOnly       = 0,     // Console/text output only — zero GPU usage, debug/fallback mode
        Pure2D         = 1,     // Fast 2D SDF canvas (classic Pac-Man style, very low GPU cost)
        TwoPointFiveD  = 2,     // Layered 2.5D with parallax/billboards (medium GPU cost)
        Full3D         = 3      // Full raymarched 3D world (highest GPU cost)
    };

    // CameraPerspective — how the world is framed and navigated
    // Influences FOV defaults, head movement, and whether raymarching makes sense.
    enum class CameraPerspective : uint32_t
    {
        FirstPerson       = 0,  // Immersive eye-level view — best for raymarched 3D depth
        ThirdPerson       = 1,  // Orbiting/over-shoulder follow — good for character focus
        TopDown           = 2,  // Overhead map view — common in strategy/RPG, works well in 2D/2.5D
        Isometric         = 3,  // Classic tilted 2.5D angle — no perspective distortion
        SideScroller      = 4,  // Side-view platformer — 2D/2.5D only
        Orthographic2D    = 5,  // Flat ortho projection — no perspective scaling
        TextAdventure     = 6   // No camera — static text narrative only
    };

    // GenrePreset — high-level style/theme hint
    // Used to auto-suggest defaults for lighting, sound, UI, controls, and atmosphere.
    // Does NOT directly affect rendering cost — only presets and feel.
    enum class GenrePreset : uint32_t
    {
        None              = 0,   // Custom / no preset — full manual control
        FPS               = 1,   // Fast-paced shooting — suggests high FOV, responsive input
        ThirdPersonAction = 2,   // Adventure/action — orbiting camera, character focus
        Platformer        = 3,   // Precision jumping — side-view or 2.5D
        Metroidvania      = 4,   // Exploration + progression — large world, secrets
        TopDownRPG        = 5,   // Classic overhead RPG — quests, party, maps
        TwinStickShooter  = 6,   // Dual-stick action — independent move/aim
        SurvivalHorror    = 7,   // Tense atmosphere — fog, dim lighting, sound emphasis
        Roguelike         = 8,   // Procedural dungeons — turn-based or real-time
        TextAdventure     = 9,   // Narrative focus — minimal visuals
        Shmup             = 10,  // Bullet hell shooter — scrolling, patterns
        Racing            = 11,  // Speed/time trials — vehicle physics
        Puzzle            = 12,  // Logic/pattern solving — clean visuals
        Fighting          = 13,  // Arena combat — tight controls
        Sports            = 14,  // Simulated sports — teams, physics
        Simulation        = 15,  // Life/building/management — open-ended
        Strategy          = 16,  // Tactical/resource — maps, units
        MMORPG            = 17,  // Massive online world — social, persistent
        PartyGame         = 18   // Casual multiplayer fun — bright, simple
    };

    // Active runtime values — these drive shader branching and camera logic
    inline DimensionMode       CurrentDimension     = DimensionMode::Full3D;
    inline CameraPerspective   CurrentPerspective   = CameraPerspective::ThirdPerson;
    inline GenrePreset         CurrentGenre         = GenrePreset::Simulation;

    // Helper predicates — used in shader conditionals and UI logic
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
// Controls output resolution, presentation mode, and window behavior.
// Most GPU impact: High resolution + fullscreen + borderless
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Window {
    inline constexpr int     DEFAULT_WIDTH            = 1920;           // Default window width in pixels
    inline constexpr int     DEFAULT_HEIGHT           = 1080;           // Default window height in pixels
    inline constexpr bool    START_FULLSCREEN         = false;          // Launch in fullscreen?
    inline constexpr bool    ALLOW_RESIZE             = true;           // Allow user resizing?
    inline constexpr bool    BORDERLESS               = false;          // Borderless fullscreen mode (if fullscreen)
}

// ─────────────────────────────────────────────────────────────────────────────
// Camera — Cinematic orbiting, movement, effects & controls
// Most GPU impact: High FOV + DoF + heavy bob/shake + rack focus
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Camera {

    inline constexpr glm::vec3 START_POSITION         { 0.0f, 4.5f, 0.0f }; // Initial world-space position
    inline constexpr float     DEFAULT_FOV            = 72.0f;             // Vertical FOV in degrees
    inline constexpr float     DEFAULT_NEAR           = 0.05f;             // Near plane (prevents z-fighting)
    inline constexpr float     DEFAULT_FAR            = 5000.0f;           // Far plane (culling distance)

    inline constexpr float     DEFAULT_APERTURE       = 2.8f;              // DoF f-stop (lower = more blur)
    inline constexpr float     DEFAULT_FOCUS_DISTANCE = 3.0f;              // DoF focus plane distance

    inline constexpr float     BASE_HEIGHT            = 4.5f;              // Orbit center height
    inline constexpr float     HEIGHT_SWING            = 2.8f;             // Vertical bob amplitude
    inline constexpr float     HEIGHT_FREQ             = 0.11f;            // Vertical bob frequency

    inline constexpr float     BASE_DISTANCE          = 20.0f;             // Orbit radius base
    inline constexpr float     DISTANCE_SWING         = 4.5f;              // Orbit push/pull amplitude
    inline constexpr float     DISTANCE_FREQ          = 0.08f;             // Orbit variation frequency

    inline constexpr float     LOOK_AT_Y_OFFSET       = 0.0f;              // Vertical look-at offset

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

    inline float               Aperture               = DEFAULT_APERTURE;
    inline float               FocusDistance          = DEFAULT_FOCUS_DISTANCE;

    inline float               ZoomSensitivity        = 1.0f;
    inline float               DollySpeed             = 4.0f;
    inline float               CraneSpeed             = 3.0f;
    inline float               RackFocusSpeed         = 2.5f;

    inline float               OrbitSpeedMultiplier   = 1.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Rendering & Performance
// Core GPU workload + hybrid raymarching toggles
// Most expensive: PureRaymarched3D + high steps + high recursion + accumulation
// Cheapest: Pure2DCanvas + low samples + no temporal
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Rendering {

    // Core resolution & dispatch settings
    inline int     INTERNAL_WIDTH           = 4915;                        // Max internal render width
    inline int     INTERNAL_HEIGHT          = 4592;                        // Max internal render height

    // Temporal & sampling quality
    inline constexpr bool    ACCUMULATION             = true;             // Temporal reprojection/accumulation (denoising)
    inline constexpr bool    ADAPTIVE_SAMPLING        = true;              // Per-pixel quality scaling
    inline constexpr int     MAX_SAMPLES_PER_PIXEL    = 4;                 // Adaptive max samples (higher = cleaner)
    inline constexpr int     MAX_RAY_RECURSION        = 8;                 // Max ray bounces (only in raymarched mode)
    inline constexpr float   EXPOSURE                 = 0.0f;              // Manual HDR exposure compensation (EV)
    inline constexpr bool    ENABLE_TONEMAP           = false;             // Apply HDR → LDR tonemapping
    inline constexpr int     DISPATCH_GROUP_SIZE      = 16;                // Compute workgroup size (must match shader)

    inline constexpr double  MaxGPULoadPercent        = 95.0;             // Target max GPU utilization before downscaling

    // Dynamic resolution scaling (DRS)
    inline bool    EnableAdaptiveResolution   = true;
    inline float   MinResolutionScale         = 0.10f;                     // Lowest allowed scale (heavy fallback)
    inline float   MaxResolutionScale         = 1.2f;                      // Highest supersampling allowed
    inline float   ResolutionStepSize         = 0.1f;

    inline float   ResolutionAdjustHysteresis = 0.9f;                     // Anti-oscillation threshold do not move deadzone
    inline float   AggressiveDownscaleThreshold = 1.35f;                   // Frametime multiplier for strong downscale
    inline float   HeadroomForUpscale         = 0.10f;                     // affects MaxGPULoadPercent

    inline float   TemporalBlendStrength      = 0.0f;                      // Frame blend, motionbluresque

    // ─── Hybrid / Raymarching Mode Selection ───────────────────────────────
    // Controls shader path: 2D SDF canvas vs full 3D raymarching vs hybrid blend.
    enum class RenderMode : uint32_t
    {
        Pure2DCanvas      = 0u,   // Fast 2D SDF painting (lowest GPU cost, classic look)
        PureRaymarched3D  = 1u,   // Full 3D raymarched world (crystal/gems, highest GPU cost)
        Hybrid            = 2u    // 2D canvas base + raymarched 3D effects on entities (balanced)
    };

    inline RenderMode CurrentRenderMode       = RenderMode::Pure2DCanvas;

    // Raymarching quality & performance tunables (only active in raymarched/hybrid modes)
    inline float      RaymarchMaxDistance     = 25.0f;      // Max ray travel before termination
    inline float      RaymarchEpsilon         = 0.004f;     // Hit detection threshold (smaller = sharper detail)
    inline uint32_t   RaymarchMaxSteps        = 140u;       // Max marching iterations per ray

    // Fallback & safety
    inline bool       EnableRaymarchFallback  = true;       // In hybrid mode, drop to 2D if GPU load too high
    inline float      RaymarchLoadThreshold   = 0.85f;      // GPU utilization % above which hybrid falls back
}

// ─────────────────────────────────────────────────────────────────────────────
// Living World — Environment, Weather, Atmosphere
// Controls day/night, lighting, fog, clouds, wind, temperature, etc.
// Most GPU impact: High fog + dense clouds + precipitation + heavy grass sway
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::LivingWorld {

    inline bool              EnableDayNightCycle      = true;              // Continuous time progression?
    inline float             DayLengthSeconds         = 60.0f;             // Real seconds per full day/night
    inline float             CycleSpeedMultiplier     = 1.0f;              // Time acceleration (1.0 = real-time)

    inline float             CurrentTimeOfDay         = 24.0f;             // Current simulated hour (0–24)

    inline bool              SunEnabled               = true;
    inline glm::vec3         SunColor                 = glm::vec3(1.0f, 0.96f, 0.88f);
    inline float             SunIntensityDay          = 12.0f;
    inline float             SunIntensityNight        = 0.1f;

    inline bool              MoonEnabled              = true;
    inline glm::vec3         MoonColor                = glm::vec3(0.9f, 0.95f, 1.0f);
    inline float             MoonIntensity            = 2.0f;

    inline float             FogDensity               = 0.0008f;           // Atmospheric fog thickness (km⁻¹)
    inline float             CloudCoverage            = 0.4f;              // Sky cloud fraction (0–1)
    inline float             CloudAnimationSpeed      = 0.08f;             // Cloud drift speed

    inline float             WindStrength             = 0.6f;
    inline glm::vec3         WindDirection            = glm::normalize(glm::vec3(0.7f, 0.0f, 0.3f));

    inline float             TemperatureC             = 22.0f;
    inline float             Humidity                 = 0.65f;
    inline float             PrecipitationFactor      = 0.0f;
    inline float             AirPressureKPa           = 101.3f;

    inline bool              EnableGrassSway          = true;
    inline float             GrassSwayAmplitude       = 0.12f;
    inline float             GrassWetShineBoost       = 1.8f;
    inline float             TemperatureColorShift    = 0.4f;

    inline uint32_t          DebugFlags               = 0;                 // Shader debug visualization bitmask
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio
// Audio engine settings (mostly CPU, negligible GPU impact)
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Audio {
    inline constexpr bool    ENABLE_HAPTICS_FEEDBACK  = true;
    inline constexpr int     SAMPLE_RATE              = 48000;
    inline constexpr int     CHANNELS                 = 2;
    inline constexpr int     BUFFER_SIZE              = 2048;
}

// ─────────────────────────────────────────────────────────────────────────────
// Debug & Tools
// Validation, logging, visualization toggles
// Most GPU impact: Validation layers + wireframe
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Debug {
    inline constexpr bool    ENABLE_VALIDATION_LAYERS = true;              // Vulkan API validation
    inline constexpr bool    ENABLE_VERBOSE_LOGGING   = false;             // Detailed console output
    inline constexpr bool    DRAW_WIREFRAME           = false;             // Force wireframe mode
}

// ─────────────────────────────────────────────────────────────────────────────
// Input Controls — Remappable via menu
// All bindings handled by GlobalInputManager — these are defaults
// GPU impact: None
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Input {

    // Keyboard defaults
    inline constexpr SDL_Scancode DEFAULT_MOVE_FORWARD  = SDL_SCANCODE_W;
    inline constexpr SDL_Scancode DEFAULT_MOVE_BACKWARD = SDL_SCANCODE_S;
    inline constexpr SDL_Scancode DEFAULT_MOVE_LEFT     = SDL_SCANCODE_A;
    inline constexpr SDL_Scancode DEFAULT_MOVE_RIGHT    = SDL_SCANCODE_D;
    inline constexpr SDL_Scancode DEFAULT_SPRINT        = SDL_SCANCODE_LSHIFT;
    inline constexpr SDL_Scancode DEFAULT_CROUCH        = SDL_SCANCODE_LCTRL;
    inline constexpr SDL_Scancode DEFAULT_JUMP          = SDL_SCANCODE_SPACE;
    inline constexpr SDL_Scancode DEFAULT_INTERACT      = SDL_SCANCODE_E;
    inline constexpr SDL_Scancode DEFAULT_SHOOT         = SDL_SCANCODE_LCTRL;

    // Controller defaults (Xbox layout)
    inline constexpr SDL_GamepadButton DEFAULT_CONTROLLER_JUMP    = SDL_GAMEPAD_BUTTON_SOUTH;
    inline constexpr SDL_GamepadButton DEFAULT_CONTROLLER_SHOOT   = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    inline constexpr SDL_GamepadButton DEFAULT_CONTROLLER_CROUCH  = SDL_GAMEPAD_BUTTON_EAST;
    inline constexpr SDL_GamepadButton DEFAULT_CONTROLLER_INTERACT = SDL_GAMEPAD_BUTTON_WEST;
    inline constexpr SDL_GamepadButton DEFAULT_CONTROLLER_SPRINT  = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;
    inline constexpr SDL_GamepadAxis   DEFAULT_CONTROLLER_MOVE_X  = SDL_GAMEPAD_AXIS_LEFTX;
    inline constexpr SDL_GamepadAxis   DEFAULT_CONTROLLER_MOVE_Y  = SDL_GAMEPAD_AXIS_LEFTY;
    inline constexpr SDL_GamepadAxis   DEFAULT_CONTROLLER_LOOK_X  = SDL_GAMEPAD_AXIS_RIGHTX;
    inline constexpr SDL_GamepadAxis   DEFAULT_CONTROLLER_LOOK_Y  = SDL_GAMEPAD_AXIS_RIGHTY;
    inline constexpr SDL_GamepadAxis   DEFAULT_CONTROLLER_TRIGGER_SPRINT = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER;

    // Runtime input tuning
    inline bool   INVERT_MOUSE_Y              = false;
    inline float  MOUSE_SENSITIVITY           = 0.11f;

    inline bool   INVERT_CONTROLLER_Y         = false;
    inline float  CONTROLLER_LOOK_SENSITIVITY = 1.8f;
    inline float  CONTROLLER_MOVE_SENSITIVITY = 1.0f;
    inline float  CONTROLLER_DEADZONE         = 0.15f;
    inline float  CONTROLLER_TRIGGER_THRESHOLD = 0.3f;

    inline float  MOVEMENT_SPEED              = 5.2f;
    inline float  SPRINT_MULTIPLIER           = 1.8f;
}