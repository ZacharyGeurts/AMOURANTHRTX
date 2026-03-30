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

    inline float     OrthoZoom                = 1.0f;                  // Base zoom factor (1.0 = normal scale)
    inline float     MinOrthoZoom             = 0.25f;                 // Widest view (max zoom out)
    inline float     MaxOrthoZoom             = 8.0f;                  // Tightest view (max zoom in)

    // Depth-of-field parameters (used when EnableDoF = true)
    inline float     Aperture                 = 2.8f;                  // f-stop value (lower = shallower DoF)
    inline float     FocusDistance            = 3.5f;                  // Distance to sharp focus plane (meters)
    inline bool      EnableDoF                = false;                 // Master DoF toggle (GPU expensive)
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

    inline RenderTechnique CurrentTechnique      = RenderTechnique::PureRaymarching;

    inline bool     EnableAdaptiveResolution     = true;
    inline float    MinResolutionScale           = 0.2f;
    inline float    MaxResolutionScale           = 1.2f;
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

    // Preloaded audio files (expand as needed)
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
// INPUT BITFLAGS — MUST MATCH SHADER PushConstants::controllerInput
// These are the ONLY input signals the shaders can receive from the CPU.
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Input::Flags
{
    // ================================================================
    // Movement (WASD + left analog stick)
    // ================================================================
    constexpr uint32_t MOVE_FORWARD  = 1u << 0;
    constexpr uint32_t MOVE_BACKWARD = 1u << 1;
    constexpr uint32_t MOVE_LEFT     = 1u << 2;
    constexpr uint32_t MOVE_RIGHT    = 1u << 3;

    // ================================================================
    // ALL major gamepad buttons (SDL3 SDL_GAMEPAD_BUTTON_*)
    // Full support for Valve/Steam Deck, Xbox Elite, PS5 DualSense Edge, etc.
    // ================================================================
    constexpr uint32_t GAMEPAD_SOUTH         = 1u << 4;   // A (Xbox) / X (PS) / B (Switch)
    constexpr uint32_t GAMEPAD_EAST          = 1u << 5;   // B (Xbox) / Circle (PS)
    constexpr uint32_t GAMEPAD_WEST          = 1u << 6;   // X (Xbox) / Square (PS)
    constexpr uint32_t GAMEPAD_NORTH         = 1u << 7;   // Y (Xbox) / Triangle (PS)

    constexpr uint32_t GAMEPAD_BACK          = 1u << 8;   // View / Select / Share
    constexpr uint32_t GAMEPAD_GUIDE         = 1u << 9;   // Steam / Xbox / PS logo button
    constexpr uint32_t GAMEPAD_START         = 1u << 10;  // Menu / Options / Start

    constexpr uint32_t GAMEPAD_LEFT_STICK    = 1u << 11;  // L3 (left stick click)
    constexpr uint32_t GAMEPAD_RIGHT_STICK   = 1u << 12;  // R3 (right stick click)

    constexpr uint32_t GAMEPAD_LEFT_SHOULDER = 1u << 13;  // LB / L1
    constexpr uint32_t GAMEPAD_RIGHT_SHOULDER= 1u << 14;  // RB / R1

    constexpr uint32_t GAMEPAD_DPAD_UP       = 1u << 15;
    constexpr uint32_t GAMEPAD_DPAD_DOWN     = 1u << 16;
    constexpr uint32_t GAMEPAD_DPAD_LEFT     = 1u << 17;
    constexpr uint32_t GAMEPAD_DPAD_RIGHT    = 1u << 18;

    // Rear grip / paddle buttons (L4/L5, R4/R5) — explicitly supported
    constexpr uint32_t GAMEPAD_LEFT_PADDLE1  = 1u << 19;  // Upper/primary left paddle (L4)
    constexpr uint32_t GAMEPAD_LEFT_PADDLE2  = 1u << 20;  // Lower/secondary left paddle (L5)
    constexpr uint32_t GAMEPAD_RIGHT_PADDLE1 = 1u << 21;  // Upper/primary right paddle (R4)
    constexpr uint32_t GAMEPAD_RIGHT_PADDLE2 = 1u << 22;  // Lower/secondary right paddle (R5)

    constexpr uint32_t GAMEPAD_MISC1         = 1u << 23;  // Share / Capture button
    constexpr uint32_t GAMEPAD_TOUCHPAD      = 1u << 24;  // PS touchpad click

    // ================================================================
    // Mouse buttons
    // ================================================================
    constexpr uint32_t MOUSE_LEFT    = 1u << 25;
    constexpr uint32_t MOUSE_RIGHT   = 1u << 26;
    constexpr uint32_t MOUSE_MIDDLE  = 1u << 27;

    // ================================================================
    // Common keyboard scancodes (SDL_SCANCODE_*)
    // ================================================================
    constexpr uint32_t KEY_ESCAPE    = 1u << 28;
    constexpr uint32_t KEY_RETURN    = 1u << 29;   // Enter
    constexpr uint32_t KEY_SPACE     = 1u << 30;
    constexpr uint32_t KEY_TAB       = 1u << 31;   // Last bit in uint32_t
}

// ← West  East → TODO: Valve R4 R5 etc.
// [LT]            [RT]
// [LB]            [RB]
//    ◀️○▶️   🟡Y △
// 💠        🔵X □ ○🔴B                     
//    🔘○🔘   🟢A ×
//    L3 R3
namespace Options::Input
{
    // ================================================================
    // Keyboard bindings (real SDL_Scancode)
    // ================================================================
    inline constexpr SDL_Scancode MoveForward   = SDL_SCANCODE_W;
    inline constexpr SDL_Scancode MoveBackward  = SDL_SCANCODE_S;
    inline constexpr SDL_Scancode MoveLeft      = SDL_SCANCODE_A;
    inline constexpr SDL_Scancode MoveRight     = SDL_SCANCODE_D;

    inline constexpr SDL_Scancode Jump          = SDL_SCANCODE_SPACE;
    inline constexpr SDL_Scancode Crouch        = SDL_SCANCODE_LCTRL;
    inline constexpr SDL_Scancode Sprint        = SDL_SCANCODE_LSHIFT;
    inline constexpr SDL_Scancode Interact      = SDL_SCANCODE_E;
    inline constexpr SDL_Scancode Reload        = SDL_SCANCODE_R;
    inline constexpr SDL_Scancode Use           = SDL_SCANCODE_F;

    // Extra common keys
    inline constexpr SDL_Scancode Escape        = SDL_SCANCODE_ESCAPE;
    inline constexpr SDL_Scancode Tab           = SDL_SCANCODE_TAB;
    inline constexpr SDL_Scancode Return        = SDL_SCANCODE_RETURN;

    // ================================================================
    // Mouse buttons (NOT scancodes — handled via SDL_GetMouseState)
    // ================================================================
    inline constexpr int MousePrimary   = SDL_BUTTON_LEFT;    // Usually fire / attack
    inline constexpr int MouseSecondary = SDL_BUTTON_RIGHT;   // Usually aim / alternate
    inline constexpr int MouseMiddle    = SDL_BUTTON_MIDDLE;  // Middle click

    // ================================================================
    // Gamepad / Controller bindings (SDL_GamepadButton)
    // Full support for Valve (Steam Deck), Xbox Elite, PS5 Edge, etc.
    // ================================================================
    inline constexpr SDL_GamepadButton GP_South         = SDL_GAMEPAD_BUTTON_SOUTH;      // A / X / B
    inline constexpr SDL_GamepadButton GP_East          = SDL_GAMEPAD_BUTTON_EAST;       // B / Circle
    inline constexpr SDL_GamepadButton GP_West          = SDL_GAMEPAD_BUTTON_WEST;       // X / Square
    inline constexpr SDL_GamepadButton GP_North         = SDL_GAMEPAD_BUTTON_NORTH;      // Y / Triangle

    inline constexpr SDL_GamepadButton GP_Back          = SDL_GAMEPAD_BUTTON_BACK;       // View / Select
    inline constexpr SDL_GamepadButton GP_Guide         = SDL_GAMEPAD_BUTTON_GUIDE;      // Steam / Xbox / PS logo
    inline constexpr SDL_GamepadButton GP_Start         = SDL_GAMEPAD_BUTTON_START;      // Menu / Options

    inline constexpr SDL_GamepadButton GP_LeftStick     = SDL_GAMEPAD_BUTTON_LEFT_STICK; // L3
    inline constexpr SDL_GamepadButton GP_RightStick    = SDL_GAMEPAD_BUTTON_RIGHT_STICK;// R3

    inline constexpr SDL_GamepadButton GP_LeftShoulder  = SDL_GAMEPAD_BUTTON_LEFT_SHOULDER;  // LB / L1
    inline constexpr SDL_GamepadButton GP_RightShoulder = SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER; // RB / R1

    inline constexpr SDL_GamepadButton GP_DPad_Up       = SDL_GAMEPAD_BUTTON_DPAD_UP;
    inline constexpr SDL_GamepadButton GP_DPad_Down     = SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    inline constexpr SDL_GamepadButton GP_DPad_Left     = SDL_GAMEPAD_BUTTON_DPAD_LEFT;
    inline constexpr SDL_GamepadButton GP_DPad_Right    = SDL_GAMEPAD_BUTTON_DPAD_RIGHT;

    // Rear grip / paddle buttons (L4/L5, R4/R5) — explicitly supported
    inline constexpr SDL_GamepadButton GP_LeftPaddle1   = SDL_GAMEPAD_BUTTON_LEFT_PADDLE1;  // Upper left paddle (L4)
    inline constexpr SDL_GamepadButton GP_LeftPaddle2   = SDL_GAMEPAD_BUTTON_LEFT_PADDLE2;  // Lower left paddle (L5)
    inline constexpr SDL_GamepadButton GP_RightPaddle1  = SDL_GAMEPAD_BUTTON_RIGHT_PADDLE1; // Upper right paddle (R4)
    inline constexpr SDL_GamepadButton GP_RightPaddle2  = SDL_GAMEPAD_BUTTON_RIGHT_PADDLE2; // Lower right paddle (R5)

    inline constexpr SDL_GamepadButton GP_Misc1         = SDL_GAMEPAD_BUTTON_MISC1;         // Share / Capture button
    inline constexpr SDL_GamepadButton GP_Touchpad      = SDL_GAMEPAD_BUTTON_TOUCHPAD;      // PS touchpad click

    // ================================================================
    // Input sensitivities & deadzones
    // ================================================================
    inline float MovementSpeed               = 1.0f;
    inline bool  InvertMouseY                = false;
    inline float MouseSensitivity            = 0.090f;

    inline bool  InvertControllerY           = false;
    inline float ControllerLookSensitivity   = 1.85f;
    inline float ControllerMoveSensitivity   = 1.00f;
    inline float ControllerDeadzone          = 0.135f;
    inline float ControllerTriggerThreshold  = 0.25f;
};