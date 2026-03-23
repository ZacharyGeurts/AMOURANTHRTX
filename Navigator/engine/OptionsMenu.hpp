#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Options Menu (Living World + Full RTX Edition)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
//
// CENTRAL CONFIGURATION HUB FOR THE ENTIRE ENGINE
// ────────────────────────────────────────────────
// This single header contains EVERY user-adjustable setting.
// All values are:
//   • Live-editable via in-game menu (ImGui or custom UI)
//   • Changeable via debug console (e.g. `set Rendering.Exposure -0.4`)
//   • Persisted to config files (.ini / .json recommended)
//   • Pushed to shaders via PushConstants (per-frame or on-change)
//   • Used by CPU systems: camera controller, input mapper, audio mixer, etc.
//
// Every section includes:
//   - Purpose of the namespace / setting
//   - Typical range and recommended values
//   - Performance / visual / gameplay impact
//   - When/how the value is consumed (per-frame, init-only, etc.)
//   - Default reasoning and why it was chosen
//
// Special attention given to GameStyle enums — they act as the "personality"
// preset system that cascades sane defaults across many other options.
// =============================================================================

#include <glm/glm.hpp>
#include <cstdint>
#include <vector>
#include <string>
#include <SDL3/SDL.h>

// ─────────────────────────────────────────────────────────────────────────────
// 2. GAME STYLE & PERSPECTIVE — DEFINES THE ENGINE'S "GENRE PERSONALITY"
//    These three values together control presets, defaults, shader paths,
//    camera behavior, input feel, post-processing bias, and UI layout.
//    Changing CurrentGenre usually triggers a full preset reload of many options.
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::GameStyle
{
    // ─── DimensionMode ───────────────────────────────────────────────────────
    // Core rendering fidelity / world representation
    // Directly selects major shader code paths (huge perf difference)
    enum class DimensionMode : uint32_t
    {
        TextOnly       = 0,     // Zero GPU — console/debug/fallback only
        Pure2D         = 1,     // Fast 2D SDF rendering — demoscene, retro, UI-heavy
        TwoPointFiveD  = 2,     // Parallax layers, sprites, billboards — medium cost
        Full3D         = 3      // Full volumetric raymarching/raytracing — flagship mode
    };

    // ─── CameraPerspective ───────────────────────────────────────────────────
    // How the world is framed + how input controls the view
    // Strongly influences default FOV, sensitivity, projection math
    enum class CameraPerspective : uint32_t
    {
        FirstPerson       = 0,  // Immersive FPS view — mouse + WASD dominant
        ThirdPerson       = 1,  // Character-focused follow cam — orbiting or offset
        TopDown           = 2,  // Overhead strategy/RPG/twin-stick view
        Isometric         = 3,  // Classic 30–45° tilt — balanced 2.5D/3D feel
        SideScroller      = 4,  // Fixed horizontal plane — pure platformer
        Orthographic2D    = 5,  // No perspective — clean technical/sprite visuals
        TextAdventure     = 6   // Static or no camera — narrative focus
    };

    // ─── GenrePreset ─────────────────────────────────────────────────────────
    // High-level style/theme that auto-applies balanced defaults when changed
    // Controls FOV, speed, lighting mood, post effects, input scheme, etc.
    // None = full manual tuning (no auto-apply)
    enum class GenrePreset : uint32_t
    {
        None                = 0,    // Manual — no automatic preset application

        FPS                 = 1,    // Fast first-person shooter
                                    // Wide FOV, responsive look, dynamic lights, low fog

        ThirdPersonAction   = 2,    // Action-adventure (Uncharted, God of War vibe)
                                    // Orbiting cam, character emphasis, cinematic bloom

        Platformer          = 3,    // Precision jumping (Mario, Celeste style)
                                    // Side view, tight controls, clean & colorful

        Metroidvania        = 4,    // Exploration / secrets (Hollow Knight, Ori)
                                    // Layered world, atmospheric fog, subtle glow

        TopDownRPG          = 5,    // Classic overhead RPG (Diablo, Zelda-Like)
                                    // Top-down, slower pace, detailed environments

        TwinStickShooter    = 6,    // Dual-analog action (Geometry Wars, Enter the Gungeon)
                                    // Independent move/aim, fast, particle heavy

        SurvivalHorror      = 7,    // Tense survival (Resident Evil, Amnesia)
                                    // Heavy fog, low light, strong audio cues, vignette

        Roguelike           = 8,    // Procedural / high replay (Hades, Binding of Isaac)
                                    // Clean UI, turn-based or real-time, high contrast

        TextAdventure       = 9,    // Pure narrative (Zork, AI Dungeon style)
                                    // Minimal graphics, text + choice focus

        Shmup               = 10,   // Bullet-hell shooter
                                    // Scrolling, fast movement, dense particles

        Racing              = 11,   // Vehicle/time-trial focus
                                    // Motion blur, dynamic FOV, speed effects

        Puzzle              = 12,   // Logic & pattern solving (Portal, The Witness)
                                    // Clean visuals, high readability, no distractions

        Fighting            = 13,   // Arena combat (Street Fighter, Smash)
                                    // Tight controls, close camera, health HUD

        Sports              = 14,   // Simulated sports (FIFA, Rocket League)
                                    // Physics heavy, team indicators, multiplayer focus

        Simulation          = 15,   // Life/building/management (Sims, Cities Skylines)
                                    // Detailed open world, realistic lighting

        Strategy            = 16,   // Tactical/RTS (StarCraft, Civ)
                                    // Top-down, unit selection, minimap emphasis

        MMORPG              = 17,   // Massive persistent world
                                    // Large draw distance, social UI, atmospheric

        PartyGame           = 18,   // Casual multiplayer (Jackbox, Fall Guys)
                                    // Bright, simple, social mechanics, fun colors

        DemosceneRetro      = 19,   // 80s/90s low-res aesthetic
                                    // Pixelation, CRT effects, limited palette

        DemosceneModern     = 20    // High-fidelity RTX/volumetric showcase (current default)
                                    // God rays, bloom, complex raymarched scenes
    };

    // ─── ACTIVE SETTINGS ─────────────────────────────────────────────────────
    inline DimensionMode       CurrentDimension     = DimensionMode::Full3D;
    inline CameraPerspective   CurrentPerspective   = CameraPerspective::FirstPerson;
    inline GenrePreset         CurrentGenre         = GenrePreset::DemosceneModern;

    // ─── CONVENIENCE HELPERS ─────────────────────────────────────────────────
    inline bool Is3D()             { return CurrentDimension == DimensionMode::Full3D; }
    inline bool Is25D()            { return CurrentDimension == DimensionMode::TwoPointFiveD; }
    inline bool Is2D()             { return CurrentDimension <= DimensionMode::TwoPointFiveD && CurrentDimension != DimensionMode::TextOnly; }
    inline bool IsTextMode()       { return CurrentDimension == DimensionMode::TextOnly; }
    inline bool IsFirstPerson()    { return CurrentPerspective == CameraPerspective::FirstPerson; }
    inline bool IsTopDownStyle()   { return CurrentPerspective == CameraPerspective::TopDown || CurrentPerspective == CameraPerspective::Isometric; }
    inline bool IsRetroDemoscene() { return CurrentGenre == GenrePreset::DemosceneRetro; }
}

// ─────────────────────────────────────────────────────────────────────────────
// Camera & Movement
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Camera
{
    inline glm::vec3 StartPosition            { 0.0f, 1.62f, 4.5f };   // Human eye height spawn point
    inline float     CurrentFOV               = 78.0f;                 // Vertical FOV — runtime adjustable
    inline float     MinFOV                   = 35.0f;
    inline float     MaxFOV                   = 115.0f;
    inline float     NearPlane                = 0.065f;
    inline float     FarPlane                 = 800.0f;

    inline float     MouseSensitivity         = 0.088f;                // rad/pixel — typical modern value
    inline bool      InvertMouseY             = false;
    inline float     MouseSmoothing           = 0.10f;                 // 0..1 low-pass filter strength

    inline float     MovementSpeed            = 4.8f;                  // m/s base walk
    inline float     SprintMultiplier         = 2.25f;
    inline float     CrouchHeightScale        = 0.64f;

    inline bool      EnableHeadBob            = true;
    inline float     HeadBobIntensity         = 0.026f;
    inline float     HeadBobFrequency         = 2.4f;

    inline bool      EnableBreathing          = true;
    inline float     BreathingIntensity       = 0.007f;

    inline bool      EnableCameraShake        = true;
    inline float     ShakeTraumaDecay         = 0.78f;

	inline float OrthoZoom     = 1.0f;                 // Base zoom factor (1.0 = normal scale)
    inline float MinOrthoZoom  = 0.25f;                // Widest view (max zoom out)
    inline float MaxOrthoZoom  = 8.0f;                 // Tightest view (max zoom in)

    // Depth-of-field parameters (used when EnableDoF = true)
    inline float Aperture      = 2.8f;                 // f-stop value (lower = shallower DoF)
    inline float FocusDistance = 3.5f;                 // Distance to sharp focus plane (meters)
    inline bool  EnableDoF     = false;                // Master DoF toggle (GPU expensive)
}

// ─────────────────────────────────────────────────────────────────────────────
// Rendering & RTX Pipeline
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Rendering
{
    enum class RenderTechnique : uint32_t
    {
        Pure2DCanvas            = 0,
        PureRaymarching         = 1,
        HybridRasterMarch       = 2,
        SoftwareRayTracing      = 3,
        HardwareRayTracing      = 4,
        ProgressivePathTracing  = 5
    };

    inline RenderTechnique CurrentTechnique     = RenderTechnique::PureRaymarching;
    inline bool            AutoFallbackOnLowFPS = false;

    inline float    RaymarchMaxDistance          = 220.0f;
    inline float    RaymarchEpsilon              = 0.0012f;
    inline uint32_t RaymarchMaxSteps             = 256u;
    inline float    RaymarchStepMultiplier       = 1.0f;

    inline bool     EnableAccumulation           = true;
    inline float    AccumulationWeight           = 0.04f;
    inline int      MaxSamplesPerPixel           = 128;
    inline bool     EnableAdaptiveSampling       = true;

    inline bool     PreferHardwareRT             = true;
    inline bool     EnableHardwareRayTracing     = false;
    inline bool     EnableRTXReflections         = true;
    inline bool     EnableRTXShadows             = true;
    inline bool     EnableRTXGI                  = false;
    inline uint32_t MaxRayRecursion              = 6u;

    inline float    Exposure                     = 0.0f;
    inline bool     EnableTonemapping            = false;
    inline uint32_t TonemapMode                  = 2u;                 // 0=linear, 1=filmic, 2=ACES-ish
    inline float    BloomThreshold               = 0.92f;
    inline float    BloomIntensity               = 0.28f;
    inline float    Contrast                     = 1.10f;
    inline float    Saturation                   = 1.14f;
    inline float    Gamma                        = 1.00f;
    inline float    VignetteStrength             = 0.42f;
    inline float    ChromaticAberrationStrength  = 0.0f;
    inline float    MotionBlurStrength           = 0.0f;

    inline bool     EnableAdaptiveResolution     = true;
    inline float    MinResolutionScale           = 0.5f;
    inline float    MaxResolutionScale           = 1.5f;
}

// ─────────────────────────────────────────────────────────────────────────────
// SDL3 — Window, Display, Input, Audio
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::SDL3
{
    inline constexpr int     MyAudioSlots       = 16;
    inline constexpr int     AudioFrequency     = 48000;
    inline constexpr int     AudioChannels      = 8;
    inline constexpr float   DefaultVolume      = 0.82f;

    inline const std::vector<std::string> PreloadedAudioFiles = {
        "assets/audio/splash.wav"
    };

    inline int     DefaultWidth                 = 1920;
    inline int     DefaultHeight                = 1080;
    inline bool    StartFullscreen              = false;
    inline bool    BorderlessWindow             = false;
    inline bool    AllowWindowResize            = true;
    inline bool    HighDPIAware                 = true;
    inline float   UIScale                      = 1.0f;

    inline bool    EnableAudio                  = true;
    inline bool    EnableSpatialAudio           = true;
    inline bool    EnableHRTF                   = false;

    inline bool    EnableGamepad                = true;
    inline float   GamepadDeadzone              = 0.135f;
    inline bool    InvertGamepadY               = false;
    inline float   GamepadLookSensitivity       = 1.80f;
    inline bool    EnableRumble                 = true;
    inline bool    EnableGyro                   = true;
    inline bool    EnableInputCapture           = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Living World — Atmosphere & Environment
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::LivingWorld
{
    inline float   CurrentTimeOfDay             = 13.5f;            // Howie© TOD in hours

    inline bool    SunEnabled                   = true;
    inline glm::vec3 SunColor                   = glm::vec3(1.00f, 0.95f, 0.84f);
    inline float   SunIntensityDay              = 15.5f;
    inline float   SunIntensityNight            = 0.05f;

    inline bool    MoonEnabled                  = true;
    inline glm::vec3 MoonColor                  = glm::vec3(0.90f, 0.93f, 1.00f);
    inline float   MoonIntensity                = 0.72f;

    inline glm::vec3 WindDirection              = glm::normalize(glm::vec3(0.65f, 0.0f, 0.55f));
    inline float   WindStrength                 = 0.48f;

    inline float   TemperatureC                 = 22.0f;
    inline float   Humidity                     = 0.58f;
    inline float   PrecipitationFactor          = 0.0f;
    inline float   FogDensity                   = 0.00052f;

    inline float   CloudCoverage                = 0.36f;
    inline float   CloudAnimationSpeed          = 0.068f;
    inline float   CloudDensity                 = 0.62f;

    inline bool    EnableVolumetricLighting     = true;
    inline float   VolumetricIntensity          = 0.85f;
    inline int     VolumetricMarchSteps         = 56;
}

// ─────────────────────────────────────────────────────────────────────────────
// Debug & Development
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Debug
{
    inline bool    EnableVulkanValidation       = false;
    inline bool    EnableVerboseLogging         = false;
}

// ─────────────────────────────────────────────────────────────────────────────
//    INPUT BITFLAGS — MUST MATCH SHADER PushConstants::controllerInput
//    These are the ONLY input signals the raymarch shader receives from the CPU.
//    Updated every frame in dispatch_canvas() based on keyboard/gamepad state.
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Input::Flags
{
    constexpr uint32_t FORWARD       = 1u << 0;     // Primary forward movement (W / left stick up)
    constexpr uint32_t BACKWARD      = 1u << 1;     // Primary backward movement (S)
    constexpr uint32_t LEFT          = 1u << 2;     // Strafe left (A)
    constexpr uint32_t RIGHT         = 1u << 3;     // Strafe right (D)

    constexpr uint32_t SPRINT        = 1u << 4;     // Speed boost modifier
    constexpr uint32_t CROUCH        = 1u << 5;     // Lower stance / reduced height
    constexpr uint32_t JUMP          = 1u << 6;     // Vertical impulse (if physics enabled)
    constexpr uint32_t INTERACT      = 1u << 7;     // Context-sensitive action (doors, items, NPCs)

    constexpr uint32_t SHOOT         = 1u << 8;     // Primary fire / attack
    constexpr uint32_t AIM           = 1u << 9;     // Aim down sights / zoom
    constexpr uint32_t RELOAD        = 1u << 10;    // Reload weapon
    constexpr uint32_t USE           = 1u << 11;    // Secondary interaction/tool

    constexpr uint32_t MOUSE_LEFT    = 1u << 16;    // Left mouse button
    constexpr uint32_t MOUSE_RIGHT   = 1u << 17;    // Right mouse button
    constexpr uint32_t MOUSE_MIDDLE  = 1u << 18;    // Middle mouse button / wheel click

    constexpr uint32_t ACTION_1      = 1u << 20;    // Extra ability / item 1
    constexpr uint32_t ACTION_2      = 1u << 21;    // Extra ability / item 2
    constexpr uint32_t ACTION_3      = 1u << 22;
    constexpr uint32_t ACTION_4      = 1u << 23;

    constexpr uint32_t DEBUG_TOGGLE  = 1u << 30;    // Debug overlay / mode toggle (rarely shader relevant)
}

// ─────────────────────────────────────────────────────────────────────────────
//    GAMEPAD LAYOUT REFERENCE — with cool emoji prompts for clarity
//    Face buttons + shoulders/triggers/sticks — pick your platform flavor!
// ─────────────────────────────────────────────────────────────────────────────

// ← W  E →
// [LT]            [RT]
// [LB]            [RB]
//    ◀️▶️    🟡 Y △
//  💠      🔵 X □   ○ 🔴 B                     
//    🔘○🔘   🟢 A ×
//    L3 R2

// ─────────────────────────────────────────────────────────────────────────────
// Input — Default Bindings & Sensitivities (remappable)
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Input
{
    // Keyboard movement & actions (real scancodes)
    inline constexpr SDL_Scancode MoveForward  = SDL_SCANCODE_W;
    inline constexpr SDL_Scancode MoveBackward = SDL_SCANCODE_S;
    inline constexpr SDL_Scancode MoveLeft     = SDL_SCANCODE_A;
    inline constexpr SDL_Scancode MoveRight    = SDL_SCANCODE_D;
    inline constexpr SDL_Scancode Sprint       = SDL_SCANCODE_LSHIFT;
    inline constexpr SDL_Scancode Crouch       = SDL_SCANCODE_LCTRL;
    inline constexpr SDL_Scancode Jump         = SDL_SCANCODE_SPACE;
    inline constexpr SDL_Scancode Interact     = SDL_SCANCODE_E;
    inline constexpr SDL_Scancode Reload       = SDL_SCANCODE_R;
    inline constexpr SDL_Scancode Use          = SDL_SCANCODE_F;

    // Mouse actions — NOT scancodes! These are handled separately via SDL_GetMouseState()
    // We use constexpr int here just as markers / labels (not actual scancodes)
    inline constexpr int MouseShoot            = SDL_BUTTON_LEFT;   // 1
    inline constexpr int MouseAim              = SDL_BUTTON_RIGHT;  // 3
    inline constexpr int MouseMiddle           = SDL_BUTTON_MIDDLE; // 2

    // Gamepad defaults
    inline constexpr SDL_GamepadButton GP_Jump     = SDL_GAMEPAD_BUTTON_SOUTH;
    inline constexpr SDL_GamepadButton GP_Interact = SDL_GAMEPAD_BUTTON_WEST;
    inline constexpr SDL_GamepadButton GP_Shoot    = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    inline constexpr SDL_GamepadButton GP_Crouch   = SDL_GAMEPAD_BUTTON_EAST;
    inline constexpr SDL_GamepadButton GP_Sprint   = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;

    // Sensitivities & deadzones
    inline bool   InvertMouseY                  = false;
    inline float  MouseSensitivity              = 0.090f;

    inline bool   InvertControllerY             = false;
    inline float  ControllerLookSensitivity     = 1.85f;
    inline float  ControllerMoveSensitivity     = 1.00f;
    inline float  ControllerDeadzone            = 0.135f;
    inline float  ControllerTriggerThreshold    = 0.25f;
}