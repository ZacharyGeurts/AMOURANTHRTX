#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Options Menu (Living World + Full RTX Edition)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
//
// This file defines ALL user-configurable runtime options for the engine.
// Values are pushed directly to shaders via PushConstants and can be changed
// live through the in-game options menu, hotkeys, config files, or debug console.
//
// Every single line is commented for clarity.
// Updated to include SDL3 constants used in SDL3.hpp (MaxAudioSlots, AudioFrequency, etc.)
// No vestigial fields (no Window namespace, no ACCUMULATION, no MaxGPULoadPercent, no START_POSITION, etc.)
// No demo/scene cycling — treated as build-time only
// =============================================================================

#include <glm/glm.hpp>
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// Game Style & Perspective
// Controls overall visual dimension feel, camera framing, and genre-driven defaults
// Influences shader branching, FOV/movement defaults, lighting tone, UI layout
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::GameStyle
{
    // DimensionMode — selects the core rendering dimension / visual style
    // Used to auto-set sensible defaults for FOV, movement speed, lighting complexity
    enum class DimensionMode : uint32_t
    {
        TextOnly       = 0,     // Console/text output only — zero GPU usage, debug/fallback mode
        Pure2D         = 1,     // Fast 2D SDF canvas — classic demoscene/arcade style
        TwoPointFiveD  = 2,     // Layered 2.5D with parallax/billboards — medium GPU cost
        Full3D         = 3      // Full 3D raymarched/raytraced world — highest immersion & GPU cost
    };

    // CameraPerspective — defines framing style and navigation behavior
    enum class CameraPerspective : uint32_t
    {
        FirstPerson       = 0,  // Immersive eye-level view — best for raymarched depth
        ThirdPerson       = 1,  // Orbiting/over-shoulder follow — good for character focus
        TopDown           = 2,  // Overhead map view — strategy/RPG, low distortion
        Isometric         = 3,  // Tilted 2.5D classic — no perspective scaling
        SideScroller      = 4,  // Side-view platformer — 2D/2.5D only
        Orthographic2D    = 5,  // Flat ortho projection — clean UI visuals
        TextAdventure     = 6   // No camera — static text narrative only
    };

    // GenrePreset — high-level theme/style that suggests default settings
    enum class GenrePreset : uint32_t
    {
        None              = 0,   // Custom / manual control
        FPS               = 1,   // Fast-paced first-person — high FOV, responsive movement
        ThirdPersonAction = 2,   // Adventure/action — orbiting camera, character emphasis
        Platformer        = 3,   // Precision platforming — clean side/2.5D view
        Metroidvania      = 4,   // Exploration/progression — layered world, secrets
        TopDownRPG        = 5,   // Classic overhead RPG — maps, quests, party
        TwinStickShooter  = 6,   // Dual-stick action — independent move/aim
        SurvivalHorror    = 7,   // Tense atmosphere — heavy fog, dim lighting, audio focus
        Roguelike         = 8,   // Procedural dungeons — high replay, turn-based or real-time
        TextAdventure     = 9,   // Narrative-driven — minimal visuals, text focus
        Shmup             = 10,  // Bullet hell shooter — scrolling patterns, fast movement
        Racing            = 11,  // Speed/time trials — motion blur, vehicle physics
        Puzzle            = 12,  // Logic/pattern solving — clean, distraction-free visuals
        Fighting          = 13,  // Arena combat — tight controls, close camera
        Sports            = 14,  // Simulated sports — physics, teams, multiplayer focus
        Simulation        = 15,  // Life/building/management — detailed open-ended world
        Strategy          = 16,  // Tactical/resource — maps, units, top-down view
        MMORPG            = 17,  // Massive persistent world — social, large scale
        PartyGame         = 18,  // Casual multiplayer fun — bright, simple, social
        DemosceneRetro    = 19,  // 80s/90s chunky low-res demoscene aesthetic
        DemosceneModern   = 20   // Modern RTX-era volumetric/raytraced showcase
    };

    // Active runtime values — drive shader logic, camera behavior, preset defaults
    inline DimensionMode       CurrentDimension     = DimensionMode::Full3D;
    inline CameraPerspective   CurrentPerspective   = CameraPerspective::FirstPerson;
    inline GenrePreset         CurrentGenre         = GenrePreset::DemosceneModern;

    // Helper predicates — used in shader conditionals, UI logic, behavior checks
    inline bool Is3D()             { return CurrentDimension == DimensionMode::Full3D; }
    inline bool Is25D()            { return CurrentDimension == DimensionMode::TwoPointFiveD; }
    inline bool Is2D()             { return CurrentDimension <= DimensionMode::TwoPointFiveD && CurrentDimension != DimensionMode::TextOnly; }
    inline bool IsTextMode()       { return CurrentDimension == DimensionMode::TextOnly; }
    inline bool IsFirstPerson()    { return CurrentPerspective == CameraPerspective::FirstPerson; }
    inline bool IsRetroDemoscene() { return CurrentGenre == GenrePreset::DemosceneRetro; }
}

// ─────────────────────────────────────────────────────────────────────────────
// Camera & Movement
// Controls position, FOV, sensitivity, bob/shake, DoF parameters
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Camera
{
    inline glm::vec3 StartPosition            { 0.0f, 1.6f, 5.0f };   // Initial world-space camera position
    inline float     CurrentFOV               = 75.0f;                // Active vertical FOV (degrees) — runtime adjustable
    inline float     MinFOV                   = 30.0f;                // Minimum allowed FOV value
    inline float     MaxFOV                   = 120.0f;               // Maximum allowed FOV value

    inline float     NearPlane                = 0.05f;                // Near clipping plane — prevents z-fighting
    inline float     FarPlane                 = 500.0f;               // Far clipping plane — culling distance

    inline float     MouseSensitivity         = 0.10f;                // Mouse look sensitivity multiplier
    inline bool      InvertMouseY             = false;                // Invert vertical mouse axis

    inline float     MovementSpeed            = 4.8f;                 // Base movement speed (units/sec)
    inline float     SprintMultiplier         = 2.2f;                 // Speed multiplier when sprinting
    inline float     CrouchHeightScale        = 0.65f;                // Height scale when crouched

    inline bool      EnableHeadBob            = true;                 // Enable camera head bobbing while moving
    inline float     HeadBobIntensity         = 0.028f;               // Strength of head bob effect
    inline float     HeadBobFrequency         = 2.3f;                 // Frequency of head bob cycle

    inline bool      EnableBreathing          = true;                 // Enable subtle idle breathing motion
    inline float     BreathingIntensity       = 0.008f;               // Strength of breathing effect

    inline bool      EnableCameraShake        = true;                 // Enable camera shake (trauma-based)
    inline float     ShakeTraumaDecay         = 0.75f;                // How quickly shake trauma decays

    inline float     Aperture                 = 2.8f;                 // DoF f-stop value (lower = more blur)
    inline float     FocusDistance            = 3.5f;                 // DoF focus plane distance
    inline bool      EnableDoF                = false;                // Enable depth of field (GPU cost increases)
}

// ─────────────────────────────────────────────────────────────────────────────
// Rendering & RTX Pipeline
// Core GPU workload configuration — raymarching, ray tracing, path tracing, retro emulation
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Rendering
{
    // Primary render technique — selects shader path / algorithm
    enum class RenderTechnique : uint32_t
    {
        Pure2DCanvas            = 0,    // Fast 2D SDF painting — lowest GPU cost, classic look
        PureRaymarching         = 1,    // Classic SDF raymarching — volumetrics, fractals, fire
        HybridRasterMarch       = 2,    // Raster G-buffer base + raymarched effects (AO, SSR, volumetrics)
        SoftwareRayTracing      = 3,    // Compute-based ray queries / brute-force tracing
        HardwareRayTracing      = 4,    // Hardware-accelerated ray tracing (VK_KHR_ray_tracing_pipeline)
        ProgressivePathTracing  = 5     // Monte Carlo path tracing with temporal accumulation
    };

    inline RenderTechnique CurrentTechnique     = RenderTechnique::PureRaymarching;
    inline bool            AutoFallbackOnLowFPS = false;             // Automatically drop to lower technique if FPS too low

    // Raymarching quality controls (used in modes 1 & 2)
    inline float   RaymarchMaxDistance          = 120.0f;           // Maximum ray travel distance before termination
    inline float   RaymarchEpsilon              = 0.0015f;          // Hit detection threshold — smaller = sharper detail
    inline uint32_t RaymarchMaxSteps            = 120u;             // Maximum marching steps per ray — primary cost driver
    inline float   RaymarchStepMultiplier       = 1.0f;             // Global step scale — lower = chunkier / retro feel

    // Path tracing & temporal accumulation
    inline bool    EnableAccumulation           = false;            // Enable temporal reprojection / denoising
    inline float   AccumulationWeight           = 0.05f;            // Blend factor for progressive refinement
    inline int     MaxSamplesPerPixel           = 64;               // Target samples per pixel for path tracing
    inline bool    EnableAdaptiveSampling       = true;             // Spend more samples in noisy areas

    // Hardware ray tracing toggles (mode 4)
    inline bool    PreferHardwareRT             = true;             // Attempt hardware RT when extensions available
    inline bool    EnableRTXReflections         = false;             // Ray-traced reflections - will need the shaders
    inline bool    EnableRTXShadows             = false;             // Ray-traced shadows - hit miss chit
    inline bool    EnableRTXGI                  = false;            // Ray-traced global illumination — very expensive
    inline uint32_t MaxRayRecursion             = 6u;               // Maximum ray bounces in hardware RT

    // Post-processing & quality
    inline float   Exposure                     = 0.0f;             // HDR exposure compensation (EV stops)
    inline bool    EnableTonemapping            = false;             // Apply HDR → LDR tonemapping
    inline bool    EnableBloom                  = false;             // Enable bloom / emissive glow
    inline float   BloomThreshold               = 1.2f;             // Brightness threshold for bloom
    inline float   BloomIntensity               = 0.45f;            // Strength of bloom effect

    inline bool    EnableVignette               = false;             // Apply screen vignette (dark edges)
    inline float   VignetteStrength             = 0.35f;            // Vignette darkness amount

    inline bool    EnableChromaticAberration    = false;            // Lens chromatic aberration effect
    inline float   ChromaticAberrationStrength  = 0.6f;             // Strength of color fringing

    inline bool    EnableMotionBlur             = false;            // Per-pixel motion blur
    inline float   MotionBlurStrength           = 0.4f;             // Blur intensity

    inline bool    EnableAdaptiveResolution     = true;             // Dynamically scale render resolution
    inline float   MinResolutionScale           = 0.2f;             // Lowest allowed render scale
    inline float   MaxResolutionScale           = 1.2f;             // Highest supersampling scale

    // Debug visualization modes
    enum DebugVisMode : uint32_t
    {
        None                = 0,
        Depth               = 1,
        Normals             = 2,
        HeatmapSteps        = 3,
        RayHits             = 4,
        PathTraceVariance   = 5,
        RenderModeOverlay   = 6
    };

    inline DebugVisMode DebugVisualization      = DebugVisMode::None;
    inline uint32_t     DebugFlags              = 0u;               // Additional bitfield debug toggles
}

// ─────────────────────────────────────────────────────────────────────────────
// SDL3 — Window, Display, Input, Audio, Gamepad + Audio Constants
// All SDL3-related options and constants used in SDL3.hpp are centralized here
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::SDL3
{
    // Audio constants (used in SDL3.hpp for desired spec, mixer setup, track gain, slot count)
    inline constexpr int     MaxAudioSlots      = 16;               // Number of simultaneous audio tracks/slots (MAX_SLOTS)
    inline constexpr int     AudioFrequency     = 48000;            // Default audio sample rate in Hz (AUDIO_FREQ)
    inline constexpr int     AudioChannels      = 8;                // Default channels (1=mono, 2=stereo) (AUDIO_CHANNELS)
    inline constexpr float   DefaultVolume      = 0.8f;             // Default track gain (0.0 = silent, 1.0 = full) (DEFAULT_VOLUME)

    // Window & Presentation
    inline int     DefaultWidth                 = 1920;             // Logical startup width in pixels
    inline int     DefaultHeight                = 1080;             // Logical startup height in pixels
    inline bool    StartFullscreen              = false;             // Launch in exclusive fullscreen
    inline bool    BorderlessWindow             = false;             // Use borderless fullscreen windowed mode
    inline bool    AllowWindowResize            = true;             // Allow user to resize window
    inline bool    HighDPIAware                 = true;             // Use high-DPI pixel backing size (SDL_GetWindowSizeInPixels)
    inline float   UIScale                      = 1.0f;             // Global UI scaling factor (for 4K/Retina)

    // Audio
    inline bool    EnableAudio                  = true;             // Master audio toggle
    inline bool    EnableSpatialAudio           = true;             // Enable 3D positional audio (if supported)
    inline bool    EnableHRTF                   = false;            // Enable head-related transfer function (headphone spatialization)

    // Input & Gamepad
    inline bool    EnableGamepad                = true;             // Enable controller/gamepad support
    inline float   GamepadDeadzone              = 0.14f;            // Analog stick deadzone threshold
    inline bool    InvertGamepadY               = false;            // Invert right stick vertical axis
    inline float   GamepadLookSensitivity       = 1.65f;            // Look sensitivity multiplier for gamepad
    inline bool    EnableRumble                 = true;             // Enable haptic feedback/vibration
    inline bool    EnableGyro                   = true;             // Enable gyro aiming (if controller supports)
    inline bool    EnableInputCapture           = false;            // Capture mouse/keyboard when focused (relative mouse mode)
}

// ─────────────────────────────────────────────────────────────────────────────
// Living World — Atmosphere & Environment
// Controls time-of-day, sun/moon, wind, fog, clouds, volumetric lighting
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::LivingWorld
{
    inline float   CurrentTimeOfDay             = 12.0f;            // Current time of day in hours (0.0–24.0)

    inline bool    SunEnabled                   = true;             // Enable sun rendering & lighting
    inline glm::vec3 SunColor                   = glm::vec3(1.00f, 0.94f, 0.82f); // Sunlight tint
    inline float   SunIntensityDay              = 14.0f;            // Full daylight intensity
    inline float   SunIntensityNight            = 0.04f;            // Nighttime fallback intensity

    inline bool    MoonEnabled                  = true;             // Enable moon rendering & lighting
    inline glm::vec3 MoonColor                  = glm::vec3(0.88f, 0.92f, 1.00f); // Moonlight tint
    inline float   MoonIntensity                = 0.7f;             // Moonlight strength

    inline glm::vec3 WindDirection              = glm::normalize(glm::vec3(0.6f, 0.0f, 0.8f)); // Normalized wind vector
    inline float   WindStrength                 = 0.5f;            // Wind force [0–1+] — affects vegetation, particles

    inline float   TemperatureC                 = 21.0f;            // Ambient temperature in Celsius — affects tint & fog
    inline float   Humidity                     = 0.60f;            // Relative humidity [0–1] — influences fog/cloud density
    inline float   PrecipitationFactor          = 0.0f;             // Rain/snow intensity [0–1]
    inline float   AirPressureKPa               = 101.3f;           // Sea-level pressure (kPa) — subtle atmosphere
    inline float   FogDensity                   = 0.0006f;          // Exponential fog density (km⁻¹)

    inline float   CloudCoverage                = 0.4f;             // Cloud cover fraction [0–1]
    inline float   CloudAnimationSpeed          = 0.07f;            // Cloud movement speed
    inline float   CloudDensity                 = 0.65f;            // Cloud thickness / opacity

    inline bool    EnableVolumetricLighting     = true;             // Enable god rays, light shafts, volumetric fog
    inline float   VolumetricIntensity          = 0.8f;             // Strength of volumetric effects
    inline int     VolumetricMarchSteps         = 48;               // Marching steps for volumetric lighting
}

// ─────────────────────────────────────────────────────────────────────────────
// Debug & Development
// Validation, logging, visualization, time control
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Debug
{
    inline bool    EnableVulkanValidation       = false;            // Enable Vulkan API validation layers
    inline bool    EnableVerboseLogging         = false;            // Detailed logging to console
    inline bool    ShowFPSOverlay               = true;             // Display FPS & frametime overlay
    inline bool    ShowGPUStats                 = true;             // Show GPU memory usage, etc.
    inline bool    ForceWireframe               = false;            // Force wireframe rendering (if supported)
    inline float   TimeScale                    = 1.0f;             // Time multiplier (0.1 = slow, 10 = fast)
}

// ─────────────────────────────────────────────────────────────────────────────
// Input Controls — Default Bindings (remappable via menu)
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