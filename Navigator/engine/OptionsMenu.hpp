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
// Controls visual complexity, rendering dimension, camera view, and genre preset.
// Most GPU damage: Full3D + complex camera effects
// Least GPU damage: TextOnly or Pure2D
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::GameStyle
{
    enum class DimensionMode : uint32_t
    {
        TextOnly       = 0,     // pure text/console-style rendering (minimal GPU use)
        Pure2D         = 1,     // flat 2D sprites, pixel art, no depth (low GPU load)
        TwoPointFiveD  = 2,     // 2.5D with parallax, billboards, layered sprites in 3D space
        Full3D         = 3      // full 3D geometry, ray tracing/compute rendering (highest GPU demand)
    };

    enum class CameraPerspective : uint32_t
    {
        FirstPerson       = 0,  // immersive player-eye view (wide FOV possible, high immersion)
        ThirdPerson       = 1,  // over-shoulder or orbiting character follow camera
        TopDown           = 2,  // overhead map-style view (common in strategy/RPGs)
        Isometric         = 3,  // classic tilted 2.5D angle (diagonal view, no perspective distortion)
        SideScroller      = 4,  // 2D side-view platformer camera
        Orthographic2D    = 5,  // flat orthographic projection (no perspective scaling)
        TextAdventure     = 6   // no camera — static text description only
    };

    enum class GenrePreset : uint32_t
    {
        None              = 0,   // no preset — custom / uncategorized
        FPS               = 1,   // first-person shooter (fast movement, high precision aiming)
        ThirdPersonAction = 2,   // action-adventure with third-person character control
        Platformer        = 3,   // 2D/2.5D jumping, precision platforming
        Metroidvania      = 4,   // exploration-focused platformer with progression gating
        TopDownRPG        = 5,   // overhead RPG with party/quests/world map
        TwinStickShooter  = 6,   // dual-stick shooting (move + aim independently)
        SurvivalHorror    = 7,   // tense resource management + horror atmosphere
        Roguelike         = 8,   // turn-based, procedural dungeons, permadeath
        TextAdventure     = 9,   // narrative-driven, command-based interaction
        Shmup             = 10,  // shoot 'em up (bullet hell, scrolling shooters)
        Racing            = 11,  // vehicle-based speed/time trials
        Puzzle            = 12,  // logic, physics, or pattern-solving focus
        Fighting          = 13,  // one-on-one or arena combat (fighters, brawlers)
        Sports            = 14,  // simulated real-world sports (teams, physics)
        Simulation        = 15,  // life/building/management sim (open-ended)
        Strategy          = 16,  // tactical/resource management (RTS or turn-based)
        MMORPG            = 17,  // massive multiplayer online role-playing
        PartyGame          = 18  // multiplayer casual/minigames (local/online fun)
    };

    inline DimensionMode       CurrentDimension     = DimensionMode::Full3D;     // current rendering dimension mode
    inline CameraPerspective   CurrentPerspective   = CameraPerspective::ThirdPerson; // active camera view style
    inline GenrePreset         CurrentGenre         = GenrePreset::Simulation;   // genre preset (affects UI hints, defaults, or atmosphere)

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
// Controls output resolution and presentation mode.
// Most GPU damage: High resolution + fullscreen
// Least GPU damage: Low resolution + windowed
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Window {
    inline constexpr int     DEFAULT_WIDTH            = 1920;           // default window width in screen pixels
    inline constexpr int     DEFAULT_HEIGHT           = 1080;           // default window height in screen pixels
    inline constexpr bool    START_FULLSCREEN         = false;          // start the application in fullscreen mode?
    inline constexpr bool    ALLOW_RESIZE             = true;           // allow user to resize the window manually?
    inline constexpr bool    BORDERLESS               = false;          // use borderless fullscreen window (if fullscreen)
}

// ─────────────────────────────────────────────────────────────────────────────
// Camera — Cinematic orbiting + adjustable parameters
// ─────────────────────────────────────────────────────────────────────────────
// Controls view, movement, and cinematic effects.
// Most GPU damage: High FOV + DoF enabled + heavy headbob/breathing/shake
// Least GPU damage: No DoF, no headbob/breathing/shake, low FOV
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Camera {

    inline constexpr glm::vec3 START_POSITION         { 0.0f, 4.5f, 0.0f }; // initial camera position in world space
    inline constexpr float     DEFAULT_FOV            = 75.0f;             // default vertical field of view in degrees
    inline constexpr float     DEFAULT_NEAR           = 0.05f;             // near clipping plane distance (prevents z-fighting)
    inline constexpr float     DEFAULT_FAR            = 5000.0f;           // far clipping plane distance (cull distant objects)

    inline constexpr float     DEFAULT_APERTURE       = 2.8f;              // default lens aperture for depth-of-field blur
    inline constexpr float     DEFAULT_FOCUS_DISTANCE = 3.0f;              // default focus distance in meters for DoF

    inline constexpr float     BASE_HEIGHT            = 4.5f;              // vertical center height for cinematic orbit
    inline constexpr float     HEIGHT_SWING            = 2.8f;             // vertical bobbing amplitude (head movement)
    inline constexpr float     HEIGHT_FREQ             = 0.11f;            // vertical bobbing frequency (cycles per second)

    inline constexpr float     BASE_DISTANCE          = 20.0f;             // base orbit distance from look target
    inline constexpr float     DISTANCE_SWING         = 4.5f;              // distance variation amplitude (push/pull)
    inline constexpr float     DISTANCE_FREQ          = 0.08f;             // distance variation frequency

    inline constexpr float     LOOK_AT_Y_OFFSET       = 0.0f;              // vertical offset for look-at point (look down at ground)

    inline float               CurrentFOV             = DEFAULT_FOV;       // current runtime field of view
    inline float               MinFOV                 = 30.0f;            // minimum allowed FOV (prevents extreme zoom-in)
    inline float               MaxFOV                 = 120.0f;           // maximum allowed FOV (prevents fisheye distortion)

    inline float               MouseSensitivity       = 0.11f;            // mouse look rotation speed multiplier
    inline bool                InvertMouseY           = false;             // invert vertical mouse axis direction?

    inline float               MovementSpeed          = 5.2f;              // base character/camera movement speed (units/sec)
    inline float               SprintMultiplier       = 1.8f;              // speed multiplier when holding sprint

    inline bool                EnableHeadBob          = true;              // enable realistic head bobbing while moving?
    inline float               HeadBobIntensity       = 0.035f;            // head bob vertical/horizontal strength
    inline float               HeadBobFrequency       = 2.1f;              // head bob cycle speed

    inline bool                EnableBreathing        = true;              // enable subtle idle breathing motion?
    inline float               BreathingIntensity     = 0.012f;            // breathing sway strength
    inline float               BreathingFrequency     = 0.18f;             // breathing cycle speed

    inline bool                EnableCameraShake      = true;              // enable dynamic camera shake (explosions, impacts)?

    inline float               Aperture               = DEFAULT_APERTURE;  // current depth-of-field f-stop value
    inline float               FocusDistance          = DEFAULT_FOCUS_DISTANCE; // current DoF focus distance in meters

    inline float               ZoomSensitivity        = 1.0f;              // zoom in/out speed multiplier
    inline float               DollySpeed             = 4.0f;              // forward/backward camera dolly speed
    inline float               CraneSpeed             = 3.0f;              // vertical camera crane (up/down) speed
    inline float               RackFocusSpeed         = 2.5f;              // focus pull (change focus distance) speed

    inline float               OrbitSpeedMultiplier   = 1.0f;              // cinematic orbit rotation speed multiplier
}

// ─────────────────────────────────────────────────────────────────────────────
// Rendering & Performance
// ─────────────────────────────────────────────────────────────────────────────
// Core GPU workload settings — biggest impact on performance.
// Most GPU damage: High INTERNAL res + accumulation + high samples/recursion + supersampling
// Least GPU damage: Low INTERNAL cap + accumulation off + low samples/recursion + subsampling
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Rendering {
    inline int     INTERNAL_WIDTH           = 4915;                        // maximum allowed internal render width (pixels)
    inline int     INTERNAL_HEIGHT          = 2592;                        // maximum allowed internal render height (pixels)

    inline constexpr bool    ACCUMULATION             = false;             // enable temporal frame accumulation for denoising?
    inline constexpr bool    ADAPTIVE_SAMPLING        = true;              // enable per-pixel quality adaptation?
    inline constexpr int     MAX_SAMPLES_PER_PIXEL    = 4;                 // max adaptive samples per pixel (higher = cleaner, slower)
    inline constexpr int     MAX_RAY_RECURSION        = 8;                 // maximum ray bounces per path (higher = more realistic lighting)
    inline constexpr float   EXPOSURE                 = 0.0f;              // manual exposure compensation (EV stops)
    inline constexpr bool    ENABLE_TONEMAP           = false;             // apply HDR-to-LDR tonemapping operator?
    inline constexpr int     DISPATCH_GROUP_SIZE      = 16;                // compute shader local workgroup size (must match shader)

    inline constexpr float   MaxGPULoadPercent        = 90.0f;             // maximum acceptable GPU utilization before downscaling

    inline bool    EnableAdaptiveResolution   = true;                      // enable dynamic resolution scaling based on load?
    inline float   MinResolutionScale         = 0.10f;                     // lowest allowed render scale (heavy subsampling fallback)
    inline float   MaxResolutionScale         = 1.2f;                      // highest allowed supersampling when under load
    inline float   ResolutionStepSize         = 0.01f;                     // smallest increment/decrement for scale changes

    inline float   ResolutionAdjustHysteresis = 0.10f;                     // minimum scale difference needed to apply change (anti-oscillation)
    inline float   AggressiveDownscaleThreshold = 1.35f;                   // frametime multiplier that triggers stronger downscale
    inline float   HeadroomForUpscale         = 0.10f;                     // minimum spare GPU time fraction required to upscale

    inline float   TemporalBlendStrength      = 0.0f;                      // strength of temporal reprojection blending (0 = disabled)
}

// ─────────────────────────────────────────────────────────────────────────────
// Living World — Environment & Weather Controls
// ─────────────────────────────────────────────────────────────────────────────
// Controls atmosphere, lighting, and vegetation effects.
// Most GPU damage: Heavy fog + high precipitation + dense clouds + grass sway
// Least GPU damage: Disable day/night, sun/moon off, no fog/clouds/grass
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::LivingWorld {

    inline bool              EnableDayNightCycle      = true;              // enable continuous day/night time progression?
    inline float             DayLengthSeconds         = 60.0f;             // real-time seconds for one full day/night cycle
    inline float             CycleSpeedMultiplier     = 1.0f;              // time acceleration factor (1.0 = real-time speed)
    inline float             CurrentTimeOfDay         = 24.0f;             // current simulated time of day (0–24 hours)

    inline bool              SunEnabled               = true;              // enable sun as a light source?
    inline glm::vec3         SunColor                 = glm::vec3(1.0f, 0.96f, 0.88f); // daytime sunlight tint
    inline float             SunIntensityDay          = 12.0f;             // sun strength during daytime
    inline float             SunIntensityNight        = 0.1f;              // fallback light level at night

    inline bool              MoonEnabled              = true;              // enable moon as a light source?
    inline glm::vec3         MoonColor                = glm::vec3(0.9f, 0.95f, 1.0f); // moonlight tint
    inline float             MoonIntensity            = 2.0f;              // moon light strength

    inline float             FogDensity               = 0.0008f;           // atmospheric fog thickness (km⁻¹)
    inline float             CloudCoverage            = 0.4f;              // fraction of sky covered by clouds (0–1)
    inline float             CloudAnimationSpeed      = 0.08f;             // speed of cloud movement across sky

    inline float             WindStrength             = 0.6f;              // wind force affecting vegetation/particles
    inline glm::vec3         WindDirection            = glm::normalize(glm::vec3(0.7f, 0.0f, 0.3f)); // normalized wind direction vector

    inline float             TemperatureC             = 22.0f;             // ambient temperature in Celsius (affects tint/sky)
    inline float             Humidity                 = 0.65f;             // atmospheric humidity (0–1, affects fog/wetness)
    inline float             PrecipitationFactor      = 0.0f;              // rain/snow intensity (0–1)
    inline float             AirPressureKPa           = 101.3f;            // air pressure in kPa (weather system tuning)

    inline bool              EnableGrassSway          = true;              // enable wind-driven grass animation?
    inline float             GrassSwayAmplitude       = 0.12f;             // grass bending strength
    inline float             GrassWetShineBoost       = 1.8f;              // extra specular highlight when wet
    inline float             TemperatureColorShift    = 0.4f;              // strength of temperature-based color tinting

    inline uint32_t          DebugFlags               = 0;                 // bitmask for shader debug visualization modes
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio
// ─────────────────────────────────────────────────────────────────────────────
// Audio engine settings — mostly CPU-side, low GPU impact.
// Most GPU damage: None (audio is negligible for GPU)
// Least GPU damage: All settings (GPU unaffected)
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Audio {
    inline constexpr bool    ENABLE_HAPTICS_FEEDBACK  = true;              // enable vibration/force feedback on controllers?
    inline constexpr int     SAMPLE_RATE              = 48000;             // audio output sample rate in Hz
    inline constexpr int     CHANNELS                 = 2;                 // number of audio channels (2 = stereo)
    inline constexpr int     BUFFER_SIZE              = 2048;              // audio buffer size in samples (latency vs stability)
}

// ─────────────────────────────────────────────────────────────────────────────
// Debug & Tools
// ─────────────────────────────────────────────────────────────────────────────
// Debug visualization and validation layers.
// Most GPU damage: Validation layers ON + wireframe ON
// Least GPU damage: All off
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Debug {
    inline constexpr bool    ENABLE_VALIDATION_LAYERS = true;              // enable Vulkan API validation and error checking?
    inline constexpr bool    ENABLE_VERBOSE_LOGGING   = false;             // output detailed debug information to console?
    inline constexpr bool    DRAW_WIREFRAME           = false;             // force wireframe rendering for debugging geometry?
}

// ─────────────────────────────────────────────────────────────────────────────
// Input Controls — remappable in menu (handled by GlobalInputManager)
// ─────────────────────────────────────────────────────────────────────────────
// All keyboard + gamepad bindings and state are managed by GlobalInputManager.
// These values are defaults/initial values — user can remap via menu.
// Menu UI should call INPUT.bindAction("action_name", newScancode) for keys,
// or update controller mappings/logic as needed.
// Most GPU damage: None (pure input, zero render impact)
// Least GPU damage: All settings (GPU unaffected)
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Input {

    // ─── Default keyboard bindings (synced with InputManager init) ───────────
    inline constexpr SDL_Scancode DEFAULT_MOVE_FORWARD  = SDL_SCANCODE_W;      // primary forward movement key
    inline constexpr SDL_Scancode DEFAULT_MOVE_BACKWARD = SDL_SCANCODE_S;      // primary backward movement key
    inline constexpr SDL_Scancode DEFAULT_MOVE_LEFT     = SDL_SCANCODE_A;      // strafe left
    inline constexpr SDL_Scancode DEFAULT_MOVE_RIGHT    = SDL_SCANCODE_D;      // strafe right
    inline constexpr SDL_Scancode DEFAULT_SPRINT        = SDL_SCANCODE_LSHIFT; // hold to run faster
    inline constexpr SDL_Scancode DEFAULT_CROUCH        = SDL_SCANCODE_LCTRL;  // hold to crouch/lower stance
    inline constexpr SDL_Scancode DEFAULT_JUMP          = SDL_SCANCODE_SPACE;  // jump / ascend
    inline constexpr SDL_Scancode DEFAULT_INTERACT      = SDL_SCANCODE_E;      // use/activate objects
    inline constexpr SDL_Scancode DEFAULT_SHOOT         = SDL_SCANCODE_LCTRL;  // primary fire / attack

    // ─── Default controller bindings (Xbox-style layout, queried in InputManager) ───
    inline constexpr SDL_GamepadButton DEFAULT_CONTROLLER_JUMP    = SDL_GAMEPAD_BUTTON_SOUTH;       // A button = jump
    inline constexpr SDL_GamepadButton DEFAULT_CONTROLLER_SHOOT   = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER; // RB = shoot/attack
    inline constexpr SDL_GamepadButton DEFAULT_CONTROLLER_CROUCH  = SDL_GAMEPAD_BUTTON_EAST;        // B button = crouch
    inline constexpr SDL_GamepadButton DEFAULT_CONTROLLER_INTERACT = SDL_GAMEPAD_BUTTON_WEST;       // X button = interact/use
    inline constexpr SDL_GamepadButton DEFAULT_CONTROLLER_SPRINT  = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER; // LB = sprint (hold)
    inline constexpr SDL_GamepadAxis   DEFAULT_CONTROLLER_MOVE_X  = SDL_GAMEPAD_AXIS_LEFTX;         // left stick horizontal (strafe)
    inline constexpr SDL_GamepadAxis   DEFAULT_CONTROLLER_MOVE_Y  = SDL_GAMEPAD_AXIS_LEFTY;         // left stick vertical (forward/back)
    inline constexpr SDL_GamepadAxis   DEFAULT_CONTROLLER_LOOK_X  = SDL_GAMEPAD_AXIS_RIGHTX;        // right stick horizontal (look/turn)
    inline constexpr SDL_GamepadAxis   DEFAULT_CONTROLLER_LOOK_Y  = SDL_GAMEPAD_AXIS_RIGHTY;        // right stick vertical (look up/down)
    inline constexpr SDL_GamepadAxis   DEFAULT_CONTROLLER_TRIGGER_SPRINT = SDL_GAMEPAD_AXIS_RIGHT_TRIGGER; // right trigger analog sprint

    // ─── Runtime adjustable values ──────────────────────────────────────────
    inline bool   INVERT_MOUSE_Y              = false;                         // invert vertical mouse look axis?
    inline float  MOUSE_SENSITIVITY           = 0.11f;                         // mouse look rotation speed multiplier (lower = slower)

    inline bool   INVERT_CONTROLLER_Y         = false;                         // invert vertical right-stick look axis?
    inline float  CONTROLLER_LOOK_SENSITIVITY = 1.8f;                          // right-stick look rotation speed multiplier
    inline float  CONTROLLER_MOVE_SENSITIVITY = 1.0f;                          // left-stick movement speed multiplier
    inline float  CONTROLLER_DEADZONE         = 0.15f;                         // analog stick deadzone (0.0–1.0, prevents drift)
    inline float  CONTROLLER_TRIGGER_THRESHOLD = 0.3f;                         // min trigger press (0–1) to register sprint/fire

    inline float  MOVEMENT_SPEED              = 5.2f;                          // base walking speed (world units per second)
    inline float  SPRINT_MULTIPLIER           = 1.8f;                          // speed multiplier when sprint input active (keyboard or trigger)

    // ─── Menu remapping helpers (called when user changes binding) ──────────
    // For keyboard: INPUT.bindAction("sprint", newScancode);
    // For controller: Update logic in InputManager or add custom remapping (e.g., change button enums at runtime if supported)
    // Example: if user remaps jump → update DEFAULT_CONTROLLER_JUMP and re-query in getMovementVector equivalents
    // ────────────────────────────────────────────────────────────────────────
}