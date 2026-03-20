#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Options Menu (Living World + Full RTX Edition)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
//
// This file centralizes ALL user-configurable runtime options.
// Every value here can be changed live via the in-game options menu,
// hotkeys, config files (.ini/.json), or debug console commands.
// Most values are directly pushed to shaders via PushConstants.
// All fields are documented with purpose, default reasoning, range,
// and notes on visual/performance impact.
// =============================================================================

#include <glm/glm.hpp>
#include <cstdint>
#include <vector>
#include <string>

// ─────────────────────────────────────────────────────────────────────────────
// Game Style & Perspective
// High-level presets that influence default camera behavior, FOV, lighting,
// movement feel, UI layout, and shader branching logic.
// Changing these can dramatically alter the perceived genre and rendering cost.
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::GameStyle
{
    // DimensionMode — Core rendering dimension / visual fidelity level
    // Controls whether the engine runs in 2D, 2.5D layered, or full 3D ray-based mode.
    // Directly affects which shader paths are active and GPU workload.
    enum class DimensionMode : uint32_t
    {
        TextOnly       = 0,     // Pure console/text output — zero GPU usage, debug/fallback mode
        Pure2D         = 1,     // Fast 2D SDF-based canvas — classic demoscene/arcade style, very low GPU cost
        TwoPointFiveD  = 2,     // Layered 2.5D with parallax layers/billboards — medium GPU cost, good for retro-modern hybrid
        Full3D         = 3      // Full 3D raymarched or raytraced world — highest immersion and GPU demand
    };

    // CameraPerspective — Defines framing style and navigation paradigm
    // Influences default FOV, movement controls, look-at behavior, and distortion handling.
    // Used by camera controller and shader to select projection matrix and input interpretation.
    enum class CameraPerspective : uint32_t
    {
        FirstPerson       = 0,  // Immersive eye-level view — ideal for depth perception in raymarched/RT scenes
        ThirdPerson       = 1,  // Orbiting or over-shoulder follow — emphasizes character model and environment
        TopDown           = 2,  // Overhead map-style view — strategy/RPG, minimal perspective distortion
        Isometric         = 3,  // Classic tilted 2.5D angle — balanced visibility, no perspective scaling
        SideScroller      = 4,  // Strict side-view platformer — 2D/2.5D only, fixed horizontal plane
        Orthographic2D    = 5,  // Flat orthographic projection — clean UI, sprite, or technical visuals
        TextAdventure     = 6   // No active camera — static background or pure text narrative
    };

    // GenrePreset — Thematic high-level style that auto-suggests sane defaults
    // Used at startup or preset load to configure FOV, speed, lighting, post-effects, etc.
    // Set to None for full manual control.
    enum class GenrePreset : uint32_t
    {
        None              = 0,   // No preset — full manual control, no auto-defaults applied
        FPS               = 1,   // Fast first-person shooter — wide FOV, responsive movement, dynamic lighting
        ThirdPersonAction = 2,   // Action-adventure — orbiting camera, character-focused framing
        Platformer        = 3,   // Precision platforming — clean side/2.5D view, tight controls
        Metroidvania      = 4,   // Exploration-driven — layered world, secrets, atmospheric tone
        TopDownRPG        = 5,   // Classic overhead RPG — maps, quests, party-based gameplay
        TwinStickShooter  = 6,   // Dual-stick action — independent movement & aiming
        SurvivalHorror    = 7,   // Tense survival — heavy fog, dim lighting, strong audio reliance
        Roguelike         = 8,   // Procedural dungeons — high replayability, often turn-based
        TextAdventure     = 9,   // Narrative focus — minimal or no 3D visuals, text-driven
        Shmup             = 10,  // Bullet-hell shooter — scrolling patterns, fast movement
        Racing            = 11,  // Vehicle/time-trial focus — motion blur, speed effects
        Puzzle            = 12,  // Logic/pattern solving — clean visuals, no distractions
        Fighting          = 13,  // Arena combat — tight controls, close-up camera
        Sports            = 14,  // Simulated sports — physics, teams, multiplayer emphasis
        Simulation        = 15,  // Life/building/management — detailed open world
        Strategy          = 16,  // Tactical/resource management — maps, units, top-down
        MMORPG            = 17,  // Massive persistent world — social features, large scale
        PartyGame         = 18,  // Casual multiplayer fun — bright, simple, social mechanics
        DemosceneRetro    = 19,  // 80s/90s low-res aesthetic — chunky pixels, limited palette
        DemosceneModern   = 20   // Modern RTX/volumetric showcase — high-fidelity ray effects
    };

    // Currently active values — drive shader conditionals, camera logic, preset application
    inline DimensionMode       CurrentDimension     = DimensionMode::Full3D;       // Active rendering dimension
    inline CameraPerspective   CurrentPerspective   = CameraPerspective::FirstPerson; // Active camera framing style
    inline GenrePreset         CurrentGenre         = GenrePreset::DemosceneModern;   // Active genre/theme preset

    // Helper predicates — used in shaders, UI, and controller logic for branching
    inline bool Is3D()             { return CurrentDimension == DimensionMode::Full3D; }
    inline bool Is25D()            { return CurrentDimension == DimensionMode::TwoPointFiveD; }
    inline bool Is2D()             { return CurrentDimension <= DimensionMode::TwoPointFiveD && CurrentDimension != DimensionMode::TextOnly; }
    inline bool IsTextMode()       { return CurrentDimension == DimensionMode::TextOnly; }
    inline bool IsFirstPerson()    { return CurrentPerspective == CameraPerspective::FirstPerson; }
    inline bool IsRetroDemoscene() { return CurrentGenre == GenrePreset::DemosceneRetro; }
}

// ─────────────────────────────────────────────────────────────────────────────
// Camera & Movement
// All parameters that affect camera position, orientation, projection, and motion feel.
// Directly influences view/projection matrices and player control responsiveness.
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Camera
{
    inline glm::vec3 StartPosition            { 0.0f, 1.6f, 5.0f };   // Default spawn position (eye height ~1.6 m for human scale)

    inline float     CurrentFOV               = 75.0f;                // Active vertical field of view in degrees — runtime adjustable
    inline float     MinFOV                   = 30.0f;                // Lowest allowed FOV (prevents extreme fisheye)
    inline float     MaxFOV                   = 120.0f;               // Highest allowed FOV (prevents distortion/nausea)

    inline float     NearPlane                = 0.05f;                // Near clipping distance — small values prevent z-fighting close-up
    inline float     FarPlane                 = 500.0f;               // Far clipping distance — larger = draw more distant objects, but precision loss

    inline float     MouseSensitivity         = 0.10f;                // Mouse look sensitivity multiplier (higher = faster turning)
    inline bool      InvertMouseY             = false;                // Invert vertical mouse axis (true = flight-sim style)

    inline float     MovementSpeed            = 4.8f;                 // Base walking speed in world units per second
    inline float     SprintMultiplier         = 2.2f;                 // Speed multiplier when sprinting is active
    inline float     CrouchHeightScale        = 0.65f;                // Eye-height scale factor when crouched (affects collision & view)

    inline bool      EnableHeadBob            = true;                 // Enable vertical camera bobbing during movement
    inline float     HeadBobIntensity         = 0.028f;               // Max vertical displacement from head bob (meters)
    inline float     HeadBobFrequency         = 2.3f;                 // Cycles per second when walking (higher = faster steps feel)

    inline bool      EnableBreathing          = true;                 // Enable subtle idle up/down breathing motion
    inline float     BreathingIntensity       = 0.008f;               // Max vertical offset from breathing (meters)

    inline bool      EnableCameraShake        = true;                 // Enable trauma-based impulsive camera shake
    inline float     ShakeTraumaDecay         = 0.75f;                // Multiplier applied each frame to reduce shake intensity

    inline float     Aperture                 = 2.8f;                 // Depth-of-field f-stop value (lower = shallower depth of field)
    inline float     FocusDistance            = 3.5f;                 // Distance to plane of sharp focus (meters)
    inline bool      EnableDoF                = false;                // Master toggle for depth-of-field blur (adds significant GPU cost)

    // Orthographic / 2D / Isometric specific controls
    inline float     OrthoZoom                = 1.0f;                 // Base zoom factor for orthographic projection (1.0 = normal scale)
    inline float     MinOrthoZoom             = 0.25f;                // Minimum zoom allowed (maximum zoom-out / widest view)
    inline float     MaxOrthoZoom             = 8.0f;                 // Maximum zoom allowed (maximum zoom-in / tightest view)
}

// ─────────────────────────────────────────────────────────────────────────────
// Rendering & RTX Pipeline
// Controls active render path, quality settings, post-processing stack, debug modes
// Most values directly affect shader branching and performance cost
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Rendering
{
    // Primary render technique — selects which shader/pipeline is executed
    enum class RenderTechnique : uint32_t
    {
        Pure2DCanvas            = 0,    // Fast 2D signed-distance-field canvas — lowest GPU cost
        PureRaymarching         = 1,    // Classic SDF raymarching — good for volumetrics/fractals
        HybridRasterMarch       = 2,    // Raster G-buffer + raymarched effects (AO/SSR/volumetrics)
        SoftwareRayTracing      = 3,    // Compute-based ray queries — no hardware RT required
        HardwareRayTracing      = 4,    // VK_KHR_ray_tracing_pipeline — reflections/shadows/GI
        ProgressivePathTracing  = 5     // Monte Carlo path tracing with temporal accumulation
    };

    inline RenderTechnique CurrentTechnique     = RenderTechnique::PureRaymarching; // Active render algorithm
    inline bool            AutoFallbackOnLowFPS = false;            // Auto-drop technique if average FPS < threshold

    // Raymarching quality (used in PureRaymarching & Hybrid modes)
    inline float   RaymarchMaxDistance          = 80.0f;           // Max ray travel distance before termination
    inline float   RaymarchEpsilon              = 0.0015f;          // Minimum hit distance threshold (smaller = sharper)
    inline uint32_t RaymarchMaxSteps            = 80u;             // Max marching steps per ray (main cost driver)
    inline float   RaymarchStepMultiplier       = 1.0f;             // Global step size scale (lower = chunkier look)

    // Path tracing & temporal accumulation settings
    inline bool    EnableAccumulation           = false;            // Enable temporal reprojection/denoising
    inline float   AccumulationWeight           = 0.05f;            // Blend factor for new samples vs history
    inline int     MaxSamplesPerPixel           = 64;               // Target SPP for path tracing convergence
    inline bool    EnableAdaptiveSampling       = true;             // Spend more samples in high-variance areas

    // Hardware ray tracing toggles (RenderTechnique::HardwareRayTracing)
    inline bool    PreferHardwareRT             = true;             // Try hardware RT when extensions are present
    inline bool    EnableHardwareRayTracing     = true;             // Master toggle for RT pipeline
    inline bool    EnableRTXReflections         = true;             // Ray-traced screen-space reflections
    inline bool    EnableRTXShadows             = true;             // Ray-traced hard/soft shadows
    inline bool    EnableRTXGI                  = true;             // Ray-traced global illumination (expensive)
    inline uint32_t MaxRayRecursion             = 6u;               // Max ray bounces (higher = more realism, slower)

    // Post-processing stack controls
    inline float   Exposure                     = 0.0f;             // HDR exposure compensation in EV stops
    inline bool    EnableTonemapping            = false;            // Apply filmic or ACES tonemapping
    inline float   BloomThreshold               = 0.0f;             // Brightness level to trigger bloom
    inline float   BloomIntensity               = 0.0f;             // Strength of bloom glow
    inline float   Contrast                     = 1.0f;             // Contrast multiplier (1.0 = neutral)
    inline float   Saturation                   = 1.0f;             // Color saturation multiplier (1.0 = neutral)

    inline float   VignetteStrength             = 0.0f;             // Edge darkening amount
    inline float   ChromaticAberrationStrength  = 0.0f;             // Lens color fringing strength
    inline float   MotionBlurStrength           = 0.0f;             // Per-pixel motion blur intensity

    inline bool    EnableAdaptiveResolution     = true;             // Dynamically scale render resolution to hit target FPS
    inline float   MinResolutionScale           = 0.02f;            // Lowest allowed internal render scale 320x200 (very blurry)
    inline float   MaxResolutionScale           = 8.0f;             // Highest supersampling scale (16K sharp)

    // Debug visualization modes — override final output for inspection
    enum DebugVisMode : uint32_t
    {
        None                = 0,    // Normal final image
        Depth               = 1,    // Linear depth buffer visualization
        Normals             = 2,    // Surface normal colors
        HeatmapSteps        = 3,    // Raymarching step count heatmap (red = expensive)
        RayHits             = 4,    // Hit/miss visualization
        PathTraceVariance   = 5,    // Noise/variance map for path tracing
        RenderModeOverlay   = 6     // Text overlay showing current technique/settings
    };

    inline DebugVisMode DebugVisualization      = DebugVisMode::None; // Active debug view mode
    inline uint32_t     DebugFlags              = 0u;               // Bitfield for extra debug toggles (shader-specific)
}

// ─────────────────────────────────────────────────────────────────────────────
// SDL3 — Window, Display, Input, Audio, Gamepad + Audio Constants
// All SDL3-related configuration and constants used in SDL3.hpp
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::SDL3
{
    // Audio engine constants — used for mixer creation and track management
    inline constexpr int     MyAudioSlots       = 16;               // Recommended maximum concurrent playing sounds (soft limit)
    inline constexpr int     AudioFrequency     = 48000;            // Mixer sample rate in Hz (44100 or 48000 common)
    inline constexpr int     AudioChannels      = 8;                // Mixer output channels (1=mono, 2=stereo, up to 8 for surround)
    inline constexpr float   DefaultVolume      = 0.8f;             // Default per-track gain (0.0–1.0 range)

    // Audio files preloaded at engine startup into fixed slots for low-latency playback
    inline const std::vector<std::string> PreloadedAudioFiles = {
        "assets/audio/splash.wav",
        // Add more paths here as needed (relative to assets root or absolute)
    };

    // Window & display settings
    inline int     DefaultWidth                 = 1920; // Must be 1920x1080 for adaptive to work. We know 4k is 4x that.
    inline int     DefaultHeight                = 1080; // You can turn adaptive off and bake your own
    inline bool    StartFullscreen              = false;            // Launch in exclusive fullscreen mode
    inline bool    BorderlessWindow             = false;            // Use borderless fullscreen windowed mode
    inline bool    AllowWindowResize            = true;             // Allow user to resize window manually

    // Audio master toggles
    inline bool    EnableAudio                  = true;             // Master audio enable/disable
    inline bool    EnableSpatialAudio           = true;             // Enable 3D positional audio processing
    inline bool    EnableHRTF                   = false;            // Enable headphone HRTF spatialization (if supported)

    // Input & controller settings
    inline bool    EnableGamepad                = true;             // Enable gamepad/controller detection & usage
    inline float   GamepadDeadzone              = 0.14f;            // Analog stick deadzone threshold (0.0–1.0)
    inline bool    InvertGamepadY               = false;            // Invert right stick vertical axis
    inline float   GamepadLookSensitivity       = 1.65f;            // Look sensitivity multiplier for gamepad sticks
    inline bool    EnableRumble                 = true;             // Enable haptic feedback/vibration
    inline bool    EnableGyro                   = true;             // Enable gyroscopic aiming (if controller supports)
    inline bool    EnableInputCapture           = true;             // Capture mouse/keyboard when window is focused (relative mode)
}

// ─────────────────────────────────────────────────────────────────────────────
// Living World — Atmosphere & Environment
// Controls dynamic sky, lighting, weather, and volumetric effects
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::LivingWorld
{
    inline float   CurrentTimeOfDay             = 12.0f;            // Current time of day in hours (0.0 = midnight, 12.0 = noon)

    inline bool    SunEnabled                   = true;             // Enable sun disk & direct sunlight contribution
    inline glm::vec3 SunColor                   = glm::vec3(1.00f, 0.94f, 0.82f); // Sunlight tint color (warm white)
    inline float   SunIntensityDay              = 14.0f;            // Full daylight direct light strength
    inline float   SunIntensityNight            = 0.04f;            // Nighttime fallback direct light (moon compensation)

    inline bool    MoonEnabled                  = true;             // Enable moon disk & moonlight contribution
    inline glm::vec3 MoonColor                  = glm::vec3(0.88f, 0.92f, 1.00f); // Moonlight tint color (cool blue-white)
    inline float   MoonIntensity                = 0.7f;             // Moonlight direct strength

    inline glm::vec3 WindDirection              = glm::normalize(glm::vec3(0.6f, 0.0f, 0.8f)); // Normalized wind vector (x/z plane)
    inline float   WindStrength                 = 0.5f;            // Wind force scalar [0–1+] — affects foliage/particles

    inline float   TemperatureC                 = 21.0f;            // Ambient temperature (°C) — subtle tint & fog influence
    inline float   Humidity                     = 0.60f;            // Relative humidity [0–1] — affects fog/cloud density
    inline float   PrecipitationFactor          = 0.0f;             // Rain/snow intensity [0–1] — particle & audio trigger
    inline float   AirPressureKPa               = 101.3f;           // Sea-level pressure (kPa) — minor atmosphere tuning
    inline float   FogDensity                   = 0.0006f;          // Exponential fog density coefficient (per km)

    inline float   CloudCoverage                = 0.4f;             // Cloud coverage fraction [0–1]
    inline float   CloudAnimationSpeed          = 0.07f;            // Cloud movement speed multiplier
    inline float   CloudDensity                 = 0.65f;            // Cloud thickness/opacity

    inline bool    EnableVolumetricLighting     = true;             // Enable god rays/light shafts/volumetric fog
    inline float   VolumetricIntensity          = 0.8f;             // Strength of volumetric light scattering
    inline int     VolumetricMarchSteps         = 48;               // Marching steps for volumetric rays (cost vs quality)
}

// ─────────────────────────────────────────────────────────────────────────────
// Input Controls — Default Bindings (remappable via menu)
// Physical key/gamepad defaults — can be rebound in-game
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Input
{
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

    inline bool   INVERT_MOUSE_Y              = false;                // Invert vertical mouse look axis
    inline float  MOUSE_SENSITIVITY           = 0.11f;                // Mouse look sensitivity multiplier

    inline bool   INVERT_CONTROLLER_Y         = false;                // Invert vertical right-stick look axis
    inline float  CONTROLLER_LOOK_SENSITIVITY = 1.8f;                 // Gamepad look sensitivity multiplier
    inline float  CONTROLLER_MOVE_SENSITIVITY = 1.0f;                 // Gamepad movement stick sensitivity
    inline float  CONTROLLER_DEADZONE         = 0.15f;                // Analog stick deadzone threshold
    inline float  CONTROLLER_TRIGGER_THRESHOLD = 0.3f;                // Trigger activation threshold (0–1)

    inline float  MOVEMENT_SPEED              = 5.2f;                 // Base character movement speed (units/sec)
    inline float  SPRINT_MULTIPLIER           = 1.8f;                 // Sprint speed multiplier when active
}