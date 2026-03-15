#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Options Menu (Living World Edition)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
//
// This file defines ALL user-configurable runtime options for the engine.
// Values are pushed directly to shaders via PushConstants and can be changed
// live through the in-game options menu, hotkeys, or config files.
//
// Comments explain EVERY option:
// - What it does
// - Typical use cases / gameplay feel
// - Performance impact (GPU/CPU cost)
// - How it interacts with rendering modes (2D canvas vs raymarched 3D vs hybrid)
// - Default value and why it was chosen
// =============================================================================

#include <glm/glm.hpp>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// Game Style & Perspective
// Controls overall rendering path, camera behavior, and atmosphere tone.
// Directly selects shader branching (2D SDF vs full raymarched 3D vs hybrid).
// Affects default lighting, controls, UI layout, and world feel.
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::GameStyle
{
    // DimensionMode — selects the core rendering pipeline
    // Determines whether we use fast 2D SDF canvas, layered 2.5D, or full 3D raymarching.
    // High impact: Full3D is most GPU-expensive (raymarching, bounces, complex lighting).
    // Low impact: Pure2D is fastest (simple fragment shading).
    enum class DimensionMode : uint32_t
    {
        TextOnly       = 0,     // Console/text output only — zero GPU usage, debug/fallback mode
        Pure2D         = 1,     // Fast 2D SDF canvas (classic arcade style, very low GPU cost)
        TwoPointFiveD  = 2,     // Layered 2.5D with parallax/billboards (medium GPU cost)
        Full3D         = 3      // Full raymarched 3D world (highest GPU cost, deep immersion)
    };

    // CameraPerspective — defines framing and navigation style
    // Influences FOV defaults, head movement, and whether raymarching depth makes sense.
    // FirstPerson → best for 3D raymarched depth and immersion
    // TopDown/Isometric → works well in 2D/2.5D, low perspective distortion
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

    // GenrePreset — high-level style/theme hint for auto-defaults
    // Used to suggest lighting, sound, UI, controls, and atmosphere presets.
    // Does NOT directly affect rendering cost — only sets sensible defaults.
    // Example: FPS suggests high FOV, fast input; SurvivalHorror suggests fog + dim lighting.
    enum class GenrePreset : uint32_t
    {
        None              = 0,   // Custom / no preset — full manual control
        FPS               = 1,   // Fast-paced shooting — high FOV, responsive input, dynamic lighting
        ThirdPersonAction = 2,   // Adventure/action — orbiting camera, character focus, medium complexity
        Platformer        = 3,   // Precision jumping — side-view or 2.5D, clean visuals
        Metroidvania      = 4,   // Exploration + progression — large world, secrets, layered visuals
        TopDownRPG        = 5,   // Classic overhead RPG — quests, party, maps, low perspective
        TwinStickShooter  = 6,   // Dual-stick action — independent move/aim, fast pacing
        SurvivalHorror    = 7,   // Tense atmosphere — fog, dim lighting, sound emphasis
        Roguelike         = 8,   // Procedural dungeons — turn-based or real-time, high replay
        TextAdventure     = 9,   // Narrative focus — minimal visuals, text-driven
        Shmup             = 10,  // Bullet hell shooter — scrolling, patterns, fast movement
        Racing            = 11,  // Speed/time trials — vehicle physics, motion blur
        Puzzle            = 12,  // Logic/pattern solving — clean visuals, no heavy effects
        Fighting          = 13,  // Arena combat — tight controls, close camera
        Sports            = 14,  // Simulated sports — teams, physics, multiplayer focus
        Simulation        = 15,  // Life/building/management — open-ended, detailed world
        Strategy          = 16,  // Tactical/resource — maps, units, top-down view
        MMORPG            = 17,  // Massive online world — social, persistent, large scale
        PartyGame         = 18   // Casual multiplayer fun — bright, simple, social
    };

    // Active runtime values — drive shader branching, camera logic, and presets
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
// High impact: High resolution + fullscreen + borderless
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Window {
    inline constexpr int     DEFAULT_WIDTH            = 1920;           // Default logical width in pixels
    inline constexpr int     DEFAULT_HEIGHT           = 1080;           // Default logical height in pixels
}

// ─────────────────────────────────────────────────────────────────────────────
// Camera — Cinematic orbiting, movement, effects & controls
// High impact: High FOV + DoF + heavy bob/shake + rack focus
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
// High impact: PureRaymarched3D + high steps + accumulation
// Low impact: Pure2DCanvas + low samples + no temporal
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Rendering {
    inline int64_t     INTERNAL_WIDTH           = 6000;                        // Max internal render width
    inline int64_t     INTERNAL_HEIGHT          = 6000;                        // Max internal render height

    inline constexpr bool    ACCUMULATION             = false;             // Temporal reprojection/accumulation (denoising)
    inline constexpr bool    ADAPTIVE_SAMPLING        = true;              // Per-pixel quality scaling
    inline constexpr int     MAX_SAMPLES_PER_PIXEL    = 4;                 // Adaptive max samples (higher = cleaner)
    inline constexpr int     MAX_RAY_RECURSION        = 8;                 // Max ray bounces (only in raymarched mode)
    inline constexpr float   EXPOSURE                 = 0.0f;              // Manual HDR exposure compensation (EV)
    inline constexpr bool    ENABLE_TONEMAP           = false;             // Apply HDR → LDR tonemapping
    inline constexpr int     DISPATCH_GROUP_SIZE      = 16;                // Compute workgroup size (must match shader)

    inline constexpr double  MaxGPULoadPercent        = 95.0;             // Target max GPU utilization before downscaling

    inline bool    EnableAdaptiveResolution   = true;
    inline float   MinResolutionScale         = 0.10f;                     // Lowest allowed scale (heavy fallback)
    inline float   MaxResolutionScale         = 20.0f;                      // Highest supersampling allowed
    inline float   ResolutionStepSize         = 1.2f;

    inline float   ResolutionAdjustHysteresis = 0.9f;                     // Anti-oscillation threshold
    inline float   AggressiveDownscaleThreshold = 1.35f;                   // Frametime multiplier for strong downscale
    inline float   HeadroomForUpscale         = 0.10f;                     // Affects MaxGPULoadPercent

    inline float   TemporalBlendStrength      = 0.0f;                      // Frame blend, motionblur-like

    enum class RenderMode : uint32_t
    {
        Pure2DCanvas      = 0u,   // Fast 2D SDF painting (lowest GPU cost, classic look)
        PureRaymarched3D  = 1u,   // Full 3D raymarched world (highest GPU cost, deep immersion)
        Hybrid            = 2u    // 2D canvas base + raymarched 3D effects on entities (balanced)
    };

    inline RenderMode CurrentRenderMode       = RenderMode::Pure2DCanvas;

    inline float      RaymarchMaxDistance     = 120.0f;      // Max ray travel before termination
    inline float      RaymarchEpsilon         = 0.004f;     // Hit detection threshold (smaller = sharper detail)
    inline uint32_t   RaymarchMaxSteps        = 120u;       // Max marching iterations per ray

    inline bool       EnableRaymarchFallback  = false;       // In hybrid mode, drop to 2D if GPU load too high
    inline float      RaymarchLoadThreshold   = 0.85f;      // GPU utilization % above which hybrid falls back
}

// ─────────────────────────────────────────────────────────────────────────────
// SDL3 Options (all exposed in menu)
// Controls SDL3 video, audio, input, gamepad behavior
// Low GPU impact — mostly CPU/window/input related
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::SDL3
{
    inline bool    EnableFullscreen           = false;             // Start/force fullscreen
    inline bool    AllowWindowResize          = true;              // Can user resize window?
    inline bool    BorderlessWindow           = false;             // Borderless mode (if not fullscreen)
    inline bool    HighDPIAware               = true;              // Use pixel backing size (SDL_GetWindowSizeInPixels)

    // Audio
    inline bool    EnableAudio                = true;              // Master audio toggle
    inline float   MasterVolume               = 1.0f;              // 0.0 = mute, 1.0 = full
    inline int     AudioFrequency             = 48000;             // Sample rate (Hz)
    inline int     AudioChannels              = 2;                 // 1 = mono, 2 = stereo
    inline bool    EnableSpatialAudio         = true;              // 3D positional audio (if supported)
    inline bool    EnableHRTF                 = false;             // Head-related transfer function (headphone spatialization)

    // Input & Gamepad
    inline bool    EnableGamepad              = true;              // Enable controller support
    inline float   GamepadDeadzone            = 0.15f;             // Analog stick deadzone
    inline bool    InvertGamepadY             = false;             // Invert right stick Y axis
    inline float   GamepadLookSensitivity     = 1.8f;              // Look sensitivity multiplier
    inline bool    EnableRumble               = true;              // Haptic feedback
    inline bool    EnableGyro                 = true;              // Gyro aiming (if controller supports)

    // SDL Performance & Debug
    inline bool    EnableHighPerformanceMode  = false;             // Hint SDL for high perf GPU
    inline bool    EnableEventLogging         = false;             // Log all SDL events (debug only)
    inline bool    EnableInputCapture         = false;              // Capture mouse/keyboard when focused
}

// ─────────────────────────────────────────────────────────────────────────────
// Living World — Environment & Atmosphere (used in Pipeline)
// Controls day/night, sun/moon, wind, fog, clouds, grass sway, etc.
// Medium GPU impact: fog, clouds, wind-affected vegetation
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::LivingWorld
{
    inline float   CurrentTimeOfDay           = 12.0f;               // 0.0–24.0 (hours) — drives day/night cycle

    inline bool    SunEnabled                 = true;                // Toggle sun rendering
    inline glm::vec3 SunColor                 = glm::vec3(1.00f, 0.96f, 0.88f);  // Warm daylight
    inline float   SunIntensityDay            = 12.0f;               // Full daylight intensity
    inline float   SunIntensityNight          = 0.05f;               // Nighttime fallback

    inline bool    MoonEnabled                = true;                // Toggle moon rendering
    inline glm::vec3 MoonColor                = glm::vec3(0.90f, 0.95f, 1.00f);  // Cool moonlight
    inline float   MoonIntensity              = 0.8f;                // Moonlight strength

    inline float   WindStrength               = 0.6f;                // 0 = calm, 1 = strong breeze — affects grass/trees
    inline glm::vec3 WindDirection            = glm::normalize(glm::vec3(0.7f, 0.0f, 0.3f));

    inline float   TemperatureC               = 22.0f;               // Ambient temperature (°C) — tint shift
    inline float   Humidity                   = 0.65f;               // Relative humidity [0–1] — fog/cloud influence
    inline float   PrecipitationFactor        = 0.0f;                // Rain/snow intensity [0–1]
    inline float   AirPressureKPa             = 101.3f;              // Sea-level pressure (kPa) — subtle atmosphere

    inline float   FogDensity                 = 0.0008f;             // km⁻¹ (~1.25 km visibility)
    inline float   CloudCoverage              = 0.45f;               // 0 = clear, 1 = overcast
    inline float   CloudAnimationSpeed        = 0.08f;               // Cloud drift speed

    inline bool    EnableGrassSway            = true;                // Wind-affected grass movement
    inline float   GrassSwayAmplitude         = 0.12f;
    inline float   GrassWetShineBoost         = 1.8f;                // Wet grass specular boost
    inline float   TemperatureColorShift      = 0.4f;                // Warm/cool tint shift based on temperature

    inline Uint32  DebugFlags                 = 0;                   // Bitflags for debug overlays (e.g., wireframe, normals)
}

// ─────────────────────────────────────────────────────────────────────────────
// Debug & Tools
// Validation, logging, visualization toggles
// Most GPU impact: Validation layers + wireframe
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Debug {
    inline constexpr bool    ENABLE_VALIDATION_LAYERS = false;              // Vulkan API validation
    inline constexpr bool    ENABLE_VERBOSE_LOGGING   = false;             // Detailed console output
    inline constexpr bool    DRAW_WIREFRAME           = false;             // Force wireframe mode
}

// ─────────────────────────────────────────────────────────────────────────────
// Input Controls — Remappable via menu
// All bindings handled by GlobalInputManager — these are defaults
// GPU impact: None
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Input {
    inline constexpr SDL_Scancode DEFAULT_MOVE_FORWARD  = SDL_SCANCODE_W;
    inline constexpr SDL_Scancode DEFAULT_MOVE_BACKWARD = SDL_SCANCODE_S;
    inline constexpr SDL_Scancode DEFAULT_MOVE_LEFT     = SDL_SCANCODE_A;
    inline constexpr SDL_Scancode DEFAULT_MOVE_RIGHT    = SDL_SCANCODE_D;
    inline constexpr SDL_Scancode DEFAULT_SPRINT        = SDL_SCANCODE_LSHIFT;
    inline constexpr SDL_Scancode DEFAULT_CROUCH        = SDL_SCANCODE_LCTRL;
    inline constexpr SDL_Scancode DEFAULT_JUMP          = SDL_SCANCODE_SPACE;
    inline constexpr SDL_Scancode DEFAULT_INTERACT      = SDL_SCANCODE_E;
    inline constexpr SDL_Scancode DEFAULT_SHOOT         = SDL_SCANCODE_LCTRL;

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