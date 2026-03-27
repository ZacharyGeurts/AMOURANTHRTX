#pragma once

// =============================================================================
// AMOURANTH RTX Engine — Options Menu (Living World + Full RTX Edition)
// (C) 2025-2026 by Zachary Robert Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial
// AMOURANTH FOREVER 💖
// =============================================================================

#include <glm/glm.hpp>
#include <cstdint>
#include <vector>
#include <string>
#include <SDL3/SDL.h>

namespace Options::GameStyle
{
    enum class DimensionMode : uint32_t
    {
        TextOnly       = 0,     // Zero GPU — console/debug/fallback only
        Pure2D         = 1,     // Fast 2D SDF rendering — demoscene, retro, UI-heavy
        TwoPointFiveD  = 2,     // Parallax layers, sprites, billboards — medium cost
        Full3D         = 3      // Full volumetric raymarching/raytracing — flagship mode
    };

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

    // ─── ACTIVE SETTINGS ─────────────────────────────────────────────────────
    inline DimensionMode       CurrentDimension     = DimensionMode::Full3D;
    inline CameraPerspective   CurrentPerspective   = CameraPerspective::FirstPerson;

    // ─── CONVENIENCE HELPERS ─────────────────────────────────────────────────
    inline bool Is3D()             { return CurrentDimension == DimensionMode::Full3D; }
    inline bool Is25D()            { return CurrentDimension == DimensionMode::TwoPointFiveD; }
    inline bool Is2D()             { return CurrentDimension <= DimensionMode::TwoPointFiveD && CurrentDimension != DimensionMode::TextOnly; }
    inline bool IsTextMode()       { return CurrentDimension == DimensionMode::TextOnly; }
    inline bool IsFirstPerson()    { return CurrentPerspective == CameraPerspective::FirstPerson; }
    inline bool IsTopDownStyle()   { return CurrentPerspective == CameraPerspective::TopDown || CurrentPerspective == CameraPerspective::Isometric; }
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
	inline RenderTechnique CurrentTechnique = RenderTechnique::PureRaymarching;

	inline bool     EnableAdaptiveResolution     = true;  // predictive sub super render scaling
    inline float    MinResolutionScale           = 0.2f;  // 320x200 - just let adaptive have all the sub space
    inline float    MaxResolutionScale           = 1.2f;  // 1.0 is native screen. We can go super or force sub (1.2, 0.8, etc)
    inline bool     AutoFallbackOnLowFPS         = false;

    inline float    RaymarchMaxDistance          = 120.0f;
    inline float    RaymarchEpsilon              = 0.0012f;
    inline uint32_t RaymarchMaxSteps             = 256u;
    inline float    RaymarchStepMultiplier       = 0.95f;

    inline bool     EnableAccumulation           = true;
    inline float    AccumulationWeight           = 0.04f;
    inline int      MaxSamplesPerPixel           = 64;
    inline bool     EnableAdaptiveSampling       = true;

    inline bool     EnableHardwareRayTracing     = false;
    inline bool     EnableRTXReflections         = true;
    inline bool     EnableRTXShadows             = true;
    inline bool     EnableRTXGI                  = false;
    inline uint32_t MaxRayRecursion              = 6u;

    inline float    Exposure                     = 0.0f;
    inline bool     EnableTonemapping            = false;
    inline uint32_t TonemapMode                  = 2;     // 0=linear, 1=filmic, 2=ACES-ish
    inline float    BloomThreshold               = 0.92f;
    inline float    BloomIntensity               = 0.0f;
    inline float    Contrast                     = 1.0f;
    inline float    Saturation                   = 1.0f;
    inline float    Gamma                        = 1.0f;
    inline float    VignetteStrength             = 0.0f;
    inline float    ChromaticAberrationStrength  = 0.0f;
    inline float    MotionBlurStrength           = 0.0f;
}

// ─────────────────────────────────────────────────────────────────────────────
// SDL3 — Window, Display, Input, Audio
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::SDL3
{
    inline constexpr int     MyAudioFiles       = 16;
    inline constexpr int     AudioFrequency     = 48000;
    inline constexpr int     AudioChannels      = 8; // 7.1

	// You can preload 16 wav mp3 or whatever, only because that says 16 up there. 17...
	// #include SDL3.hpp and INPUT.playSound("assets/audio/splash.wav", "play");
	// If you did not preload, we will load, play, and return the file number.
	// If you play more than "16" then the first then second so on files get replaced.
	// Explore your Navigator/engine/SDL3.hpp file sometime, and see the wiki.
	// https://github.com/ZacharyGeurts/AMOURANTHRTX/wiki/SDL3
	
    inline const std::vector<std::string> PreloadedAudioFiles = {
        "assets/audio/splash.wav"
    };

    inline int     DefaultWidth                 = 1920;
    inline int     DefaultHeight                = 1080;
    inline bool    StartFullscreen              = false;
    inline bool    BorderlessWindow             = false;
    inline bool    AllowWindowResize            = true;
    inline bool    HighDPIAware                 = true;

    inline bool    EnableAudio                  = true;
    inline bool    EnableSpatialAudio           = true;
    inline bool    EnableHRTF                   = false; // headphones

    inline bool    EnableGamepad                = true;
    inline float   GamepadDeadzone              = 0.135f;
    inline bool    InvertGamepadY               = false;
    inline float   GamepadLookSensitivity       = 1.80f;
    inline bool    EnableRumble                 = true;
    inline bool    EnableGyro                   = true;
    inline bool    EnableInputCapture           = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Living World — Atmosphere & Environment
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::LivingWorld
{
    inline float   CurrentTimeOfDay             = 13.5f;

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

// ← West  East →
// [LT]            [RT]
// [LB]            [RB]
//    ◀️○▶️   🟡Y △
// 💠        🔵X □ ○🔴B                     
//    🔘○🔘   🟢A ×
//    L3 R3

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
	inline bool   MovementSpeed                 = 1.0;
    inline bool   InvertMouseY                  = false;
    inline float  MouseSensitivity              = 0.090f;

    inline bool   InvertControllerY             = false;
    inline float  ControllerLookSensitivity     = 1.85f;
    inline float  ControllerMoveSensitivity     = 1.00f;
    inline float  ControllerDeadzone            = 0.135f;
    inline float  ControllerTriggerThreshold    = 0.25f;
}