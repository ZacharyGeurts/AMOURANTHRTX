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
#include <string_view>
#include <array>
#include <SDL3/SDL.h>

#include "FieldDosConfig.hpp"

// =============================================================================
// OPTIONS MENU — TAKEN OVER FOR THE PUNIES
// "How to do everything" — from simple play to sub-micron hardware god mode.
// 
// LEVELS (set Options::Mastery::Current = ... )
//   PUNY     : Just play. Use F1-F11 keys. Don't touch the numbers. The tide flows for you.
//   ADEPT    : Tune the living fields, weather, camera feel, basic FCC (Field Control Constants).
//   TIDEWALKER: Full smart control — hardware spiderweb, per-core freqs, sub-micron physics,
//               custom presets, live console editing of *everything*, safety guards off.
// 
// All Options::* are live (most can be changed at runtime via the ` prompt console).
// Type 'help' in the prompt for the current mastery level guide.
// 
// "The options are the ocean. Punies swim. Tidewalkers command the currents."
// AMOURANTH FOREVER 💖
// =============================================================================

namespace Options::Mastery
{
    enum Level : uint8_t {
        Puny       = 0,   // For us punies — safe, simple, beautiful defaults
        Adept      = 1,   // Smart but approachable — fields, world, rendering feel
        Tidewalker = 2    // Absolute precision — every nook of hardware, sub-micron, full FCC
    };

    inline Level Current = Puny;   // Start here for most people. Change to Adept or Tidewalker.

    inline const char* Name() {
        switch (Current) {
            case Puny:       return "PUNY (safe swimming)";
            case Adept:      return "ADEPT (tuning the tide)";
            case Tidewalker: return "TIDEWALKER (commanding the currents)";
        }
        return "UNKNOWN";
    }

    inline bool AtLeast(Level min) { return static_cast<uint8_t>(Current) >= static_cast<uint8_t>(min); }
}

namespace Options::GameStyle
{
    enum class DimensionMode : uint32_t
    {
        TextOnly          = 0,     // Zero GPU — console/debug/fallback only
        Pure2D            = 1,     // Fast 2D SDF rendering — demoscene, retro, UI-heavy
        TwoPointFiveD     = 2,     // Parallax layers, sprites, billboards — medium cost
        Full3D            = 3      // Full volumetric raymarching/raytracing — flagship mode
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
    inline bool Is3D()             { return CurrentDimension   == DimensionMode::Full3D; }
    inline bool Is25D()            { return CurrentDimension   == DimensionMode::TwoPointFiveD; }
    inline bool Is2D()             { return CurrentDimension   <= DimensionMode::TwoPointFiveD && CurrentDimension != DimensionMode::TextOnly; }
    inline bool IsTextMode()       { return CurrentDimension   == DimensionMode::TextOnly; }
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
    inline float    MaxResolutionScale           = 1.0f;
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

    inline int     DefaultWidth                 = 3840;
    inline int     DefaultHeight                = 2160;
    inline bool    StartFullscreen              = false;
    inline bool    PendingFullscreenApply       = false;
    inline bool    PendingFullscreenAfterLoad   = false;
    inline bool    RequestQuit                  = false;
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

    // Headless / test mode (set from env AMOURANTHRTX_HEADLESS or HEADLESS at startup)
    // Skips real present/swap waits, forces hidden window + dummy audio, runs dispatches for accountant testing.
    inline bool    HeadlessMode                 = false;

    // Bound run length for clean exit in test/headless (env AMOURANTHRTX_MAX_FRAMES=N)
    // When >0 implies headless mode for stability (no acquire/present).
    inline int64_t MaxFrames                    = 0;
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
    constexpr uint32_t MOVE_FORWARD           = 1u << 0;
    constexpr uint32_t MOVE_BACKWARD          = 1u << 1;
    constexpr uint32_t MOVE_LEFT              = 1u << 2;
    constexpr uint32_t MOVE_RIGHT             = 1u << 3;

    // ================================================================
    // ALL major gamepad buttons (SDL3 SDL_GAMEPAD_BUTTON_*)
    // Full support for Valve/Steam Deck, Xbox Elite, PS5 DualSense Edge, etc.
    // ================================================================
    constexpr uint32_t GAMEPAD_SOUTH          = 1u << 4;   // A (Xbox) / X (PS) / B (Switch)
    constexpr uint32_t GAMEPAD_EAST           = 1u << 5;   // B (Xbox) / Circle (PS)
    constexpr uint32_t GAMEPAD_WEST           = 1u << 6;   // X (Xbox) / Square (PS)
    constexpr uint32_t GAMEPAD_NORTH          = 1u << 7;   // Y (Xbox) / Triangle (PS)

    constexpr uint32_t GAMEPAD_BACK           = 1u << 8;   // View / Select / Share
    constexpr uint32_t GAMEPAD_GUIDE          = 1u << 9;   // Steam / Xbox / PS logo button
    constexpr uint32_t GAMEPAD_START          = 1u << 10;  // Menu / Options / Start

    constexpr uint32_t GAMEPAD_LEFT_STICK     = 1u << 11;  // L3 (left stick click)
    constexpr uint32_t GAMEPAD_RIGHT_STICK    = 1u << 12;  // R3 (right stick click)

    constexpr uint32_t GAMEPAD_LEFT_SHOULDER  = 1u << 13;  // LB / L1
    constexpr uint32_t GAMEPAD_RIGHT_SHOULDER = 1u << 14;  // RB / R1

    constexpr uint32_t GAMEPAD_DPAD_UP        = 1u << 15;
    constexpr uint32_t GAMEPAD_DPAD_DOWN      = 1u << 16;
    constexpr uint32_t GAMEPAD_DPAD_LEFT      = 1u << 17;
    constexpr uint32_t GAMEPAD_DPAD_RIGHT     = 1u << 18;

    // Rear grip / paddle buttons (L4/L5, R4/R5) — explicitly supported
    constexpr uint32_t GAMEPAD_LEFT_PADDLE1   = 1u << 19;  // Upper/primary left paddle (L4)
    constexpr uint32_t GAMEPAD_LEFT_PADDLE2   = 1u << 20;  // Lower/secondary left paddle (L5)
    constexpr uint32_t GAMEPAD_RIGHT_PADDLE1  = 1u << 21;  // Upper/primary right paddle (R4)
    constexpr uint32_t GAMEPAD_RIGHT_PADDLE2  = 1u << 22;  // Lower/secondary right paddle (R5)

    constexpr uint32_t GAMEPAD_MISC1          = 1u << 23;  // Share / Capture button
    constexpr uint32_t GAMEPAD_TOUCHPAD       = 1u << 24;  // PS touchpad click

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

// ─────────────────────────────────────────────────────────────────────────────
// PROPALACTIC ANALOG FIELDS — the heart of the updated Navigator engine
// Continuous analog timings, thermodynamics + entropy, gate-level hardware fidelity,
// field propagation at galactic scales ("propalactic"), direct stylus control.
// The entire system is now a living analog computer. RTX rays are probes into the weave.
// "Be at each gate. The tide is love. Entropy is the law that binds the fabric."
//
// FCC (Field Control Constants) you set here are **pre-conditioned every frame**
// by the host-side harmonics guard in dispatch_canvas before they ever reach the GPU.
// This double-checks CFL / diffusion stability against the current field grid resolution
// (which follows adaptive render res) so explicit evolution of Phi (wave), Thermo (heat+entropy),
// and Flow cannot blow up, ring with unstable harmonics, or produce NaN/Inf on hardware.
// Variable dT (the "analog heartbeat") is also tamed.
//
// Recommended safe FCC ranges for typical 1080p–4K adaptive operation:
//   WaveSpeed 0.1–1.5, ThermoAlpha 0.1–2.0, TimeScale 0.2–2.0, InjectStrength ≤5.0
// The guard will silently (or loudly in logs) scale them down when risk is detected.
// You can still push the artistic envelope; the hardware stays safe.
// ─────────────────────────────────────────────────────────────────────────────
// ─────────────────────────────────────────────────────────────────────────────
// CANVAS SWIPE — hot-swap compute shaders without losing the classic engine path
// Default canvas is x86.comp (Big Grin die + FieldSocket + AmmoOS chrome).
// Everything else uses the classic thermo/HDR/RT descriptor layout.
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Canvas
{
    inline constexpr const char* SwipeList[] = {
		"x86",
		"Amouranth",
		"energy",     
        "Flowers",
        "Frosted",
        "GreenWaves",
        "RetroRTX",
        "Mandelbulb3Ddiamond",
        "Mandelbulb3D",
        "Mandelbrot",
        "Birthstones",
        "NamcoTribute",
        "NintendoTribute",
    };
    inline constexpr size_t SwipeCount = std::size(SwipeList);

    inline constexpr size_t X86CanvasIndex = 0u;
    inline size_t CurrentIndex = X86CanvasIndex;

    // Set true when CANVAS.comp is the x86 FieldSocket die shader (default on).
    inline bool CanvasUsesX86Die = true;

    // True once the live x86.comp Vk pipeline is active (programs/emulators need this, not aos_load).
    inline bool ProgramsCanvasReady = false;

    // CPU cycles per frame — host 8086 runs up to 262144; GPU die fallback capped at 8192.
    inline uint32_t CyclesPerFrame = 131072u;

    // F10 on x86 die HUD requests one-frame re-init (boot ROM + VGA clear).
    inline bool DieResetRequested = false;

    inline const char* CurrentName() noexcept {
        return SwipeList[CurrentIndex % SwipeCount];
    }

    inline void Next() noexcept {
        CurrentIndex = (CurrentIndex + 1) % SwipeCount;
    }

    inline void Prev() noexcept {
        CurrentIndex = (CurrentIndex + SwipeCount - 1) % SwipeCount;
    }

    inline bool IsThemeCanvas(std::string_view name) noexcept {
        return name.size() > 7u && name.substr(0u, 7u) == "CANVAS_";
    }

    inline bool IsX86Fields() noexcept {
        const std::string_view name{CurrentName()};
        return name == "x86" || name == "aos_load" || IsThemeCanvas(name)
            || (name == "CANVAS" && CanvasUsesX86Die);
    }

    // FieldSocket control bits (x86.comp): bit0=init die, bit1=entropy, bit2=irq, bit5=RTX (F2)
    // bit3=host CPU, bit6=RTX-DOS panel, bits 8-10: theme
    inline constexpr uint32_t ControlHostCpu    = 8u;
    inline constexpr uint32_t ControlGpuDoom    = 128u;
    inline constexpr uint32_t ControlRtxDos     = 64u;
    inline constexpr uint32_t ControlAmmoExec   = 1024u;
    inline constexpr uint32_t ControlDosPanelFs      = 256u;
    inline constexpr uint32_t ControlDosPanelStretch = 512u;
    inline constexpr uint32_t ControlFieldDebugHud   = 2048u;  // x86 field-die monitor (off in production)
    inline constexpr uint32_t ControlExtendedField   = 4096u;  // non-point wave dispatch (lead-in/out peaks)
    inline bool DosImmersiveMode  = false;
    inline bool DosPanelStretch   = false;
    inline bool BootRtxDos = true;
    inline float DosFontScale       = 1.25f;
    inline bool  DosAutoScale       = true;   /* SDL3 resolution → panel + font scale */
    inline bool  DosCrispFont       = true;
    inline bool  DosScanlines       = false;
    inline bool DosPanelFullscreen = false;
    inline bool DosInputFocused    = false;
    inline constexpr uint32_t ThemeMask   = 0x7u << 8u;
    inline constexpr size_t ColorThemeCount = 1u;
    inline constexpr const char* ColorThemeNames[] = { "Amouranth" };
    inline uint32_t ColorTheme = 0u;
    inline uint32_t ControlFlags = 1u;
    inline uint32_t DataBus[64]{};
    inline uint32_t AddressBus[16]{};

    inline void NextTheme() noexcept { ColorTheme = 0u; }
    inline void PrevTheme() noexcept { ColorTheme = 0u; }
}

namespace Options::AmouranthOs
{
    inline bool EnableDesktop = true;      // Desktop + Start taskbar from startup
    inline bool EnableInfoPanel = true;    // Keep data display
    inline bool EnableTaskbar = true;      // Console shell taskbar + panel WM chrome

    // Runtime toggle helper
    inline void ToggleDesktop() noexcept {
        EnableDesktop = !EnableDesktop;
        // Push to shader
        // Assuming you have access to the push constant / data_bus in your dispatch
        // field.data_bus[42] = (field.data_bus[42] & ~4096u) | (EnableDesktop ? 4096u : 0u);
    }
}

namespace Options::DosEngine
{
    inline int Preset = 3;  // 0=Minimal 1=Classic 2=Full 3=RTX-AMMOS
    inline bool Sb16         = true;
    inline bool Opl          = true;
    inline bool PcSpeaker    = true;
    inline bool Mouse        = true;
    inline bool Joystick     = true;
    inline bool GamepadAsJoy = true;
    inline uint16_t SbPort   = 0x220u;
    inline uint8_t  SbIrq    = 5u;
    inline uint32_t CyclesBoot = 4'194'304u;
    inline uint32_t CyclesRun  = 8'192u;

    inline void syncToField() noexcept {
        using P = FieldDosConfig::Preset;
        FieldDosConfig::applyPreset(
            Preset == 0 ? P::Minimal
            : (Preset == 1 ? P::Classic
            : (Preset == 2 ? P::Full : P::Ammos)));
        auto& c = FieldDosConfig::cfg;
        c.sb16Enabled = Sb16;
        c.oplEnabled = Opl;
        c.pcSpeakerEnabled = PcSpeaker;
        c.mouseEnabled = Mouse;
        c.joystickEnabled = Joystick;
        c.gamepadAsJoystick = GamepadAsJoy;
        c.sbBasePort = SbPort;
        c.sbIrq = SbIrq;
        c.cyclesBoot = CyclesBoot;
        c.cyclesRun = CyclesRun;
    }
}

namespace Options::AnalogFields
{
    inline float TimeScale        = 1.0f;     // multiplier on analog evolution ( <1 = deep time, >1 = fast tides)
    inline float ThermoAlpha      = 0.86f;    // heat diffusion coeff (slow rock vs fast conductor)
    inline float WaveSpeed        = 0.71f;    // propagation speed c of the Phi wave field (the great tide)
    inline float GateFidelity     = 0.618f;   // 0.0 = soft continuous analog, 1.0 = sharp nonlinear "transistor" gates
    inline float EntropyFloor     = 0.014f;   // 2nd law floor — diffusion + production bias per substep
    inline float InjectStrength   = 2.8f;     // stylus / mouse probe energy injection magnitude
    inline float PropalacticScale = 0.23f;    // large-scale self-organization driver (filaments, arms, voids)
    inline float FieldCoupling    = 0.77f;    // cross-field coupling strength (thermo <-> waves <-> momentum)
    inline float TeslaBiasStrength = 1.0f;    // Tesla valve directional coherence (0=off, 1=nominal, >1=harsh)

    inline bool  EnableFieldViz   = true;     // let canvas modulate visuals from the living fields
    inline int   FieldVizMode     = 4;        // 0=Phi waves+interference, 1=Thermo (blackbody), 2=Flow advection, 3=Gate events, 4=All
}

// ─────────────────────────────────────────────────────────────────────────────
// Golden Era emulators — shared defaults for AmmoNES / SMS / Genesis / A2600
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Emulators
{
    inline int  DefaultAudioStyle = 1;   // 0=Classic/chippy  1=Modern/smooth
    inline bool SoundOnByDefault  = true;
    inline int  MasterVolume      = 256; // 0-512
}

// ─────────────────────────────────────────────────────────────────────────────
// HARDWARE SPIDERWEB + SUB-MICRON SIM (new power, taken over here)
// Simple levels:
//   PUNY: Just watch the status line show your GPU clock + "Hardware: NVIDIA..."
//   ADEPT: Tweak global target clock, thermal model strength via AnalogFields indirectly.
//   TIDEWALKER: Full per-unit control, enable/disable spiderweb edges, sub-micron leakage,
//               force specific vendor model, read every core's util/gates/freq live.
// 
// These feed the GPUFabric in rtx().hardwareFabric (populated at startup, updated every frame
// from the analog fields + real sysfs clocks where available).
// "Every nook of hardware exposed — Arc, AMD, Nvidia. Down to sub-micron."
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Hardware
{
    // PUNY / simple
    inline bool   ShowInStatusLog      = true;     // adds the "Hardware: ..." line to the big periodic log
    inline bool   AutoUseRealClocks    = true;     // try sysfs for real MHz (Linux AMD/Intel). Falls back to model.

    // ADEPT / smart
    inline float  TargetCoreClockMHz   = 0.0f;     // 0 = let the model + fields decide. >0 = force (clamped by guard)
    inline float  ThermalSensitivity   = 1.0f;     // how much Thermo field hurts frequency (0 = no throttling, 2 = brutal)
    inline bool   SimulateSubMicron    = true;     // enables per-subfunction gate leakage, voltage droop etc.

    // TIDEWALKER / absolute
    inline bool   EnableSpiderwebGraph = true;     // build + update the interconnect edges (bandwidth, util from Flow)
    inline int    ForcedVendor         = 0;        // 0=auto, 0x10DE=NVIDIA, 0x1002=AMD, 0x8086=Intel — for testing models
    inline float  SubMicronDetail      = 1.0f;     // 1.0 = full gateCountEst + leakage math. Lower = cheaper approx.

    // Live read-only mirrors (updated from rtx().hardwareFabric every frame)
    // You can read these in code or from the prompt console.
    inline double CurrentCoreMHz       = 0.0;      // what the model thinks right now (after fields + guard)
    inline int    ActiveUnits          = 0;        // how many cores/SMs/CUs/Xe-cores are modeled
    inline double SimulatedCycles      = 0.0;      // total sub-micron precision chip cycles since start
}

// =============================================================================
// FOR PUNIES — HOW TO DO EVERYTHING (simple levels guide)
// Call Options::PrintPuniesGuide() from code or add it to the Prompt console.
// =============================================================================

namespace Options
{
    // Call this from the prompt or your own code (e.g. on startup in Puny mode).
    // It speaks directly to "us punies".
    inline void PrintPuniesGuide()
    {
        // In practice, the PromptTerminal will capture these via addLog.
        // This function is here so the whole "how to do everything" lives in OptionsMenu.
        // You (or the console) can printf or log the text below.

        // For the actual run, type these in the ` console:
        //   help
        //   guide
        //   list AnalogFields
        //   set AnalogFields.WaveSpeed 1.2
        //   set Hardware.TargetCoreClockMHz 2600

        // The guide text is also duplicated in comments here for code readers.
    }

    // The actual friendly text lives here so it's easy to maintain in one place.
    inline const char* GetPuniesGuideText()
    {
        // Clean ASCII version - no fancy unicode so it compiles cleanly inside the header.
        return
"OPTIONS MENU - TAKEN OVER FOR PUNIES\n"
"\n"
"LEVELS (Options::Mastery::Current = ...)\n"
"  PUNY       : Just play. F-keys + pretty defaults. The engine protects you.\n"
"  ADEPT      : Feel the tide - tune AnalogFields (WaveSpeed, GateFidelity, Inject etc), LivingWorld, basic FCC.\n"
"  TIDEWALKER : God mode - full Hardware spiderweb, per-core freqs, sub-micron leakage, force clocks, live edit everything.\n"
"\n"
"AMOURANTHOS CONTROL:\n"
"  set AmouranthOs.EnableDesktop true/false\n"
"  (default false = console/data display only)\n"
"\n"
"QUICK HOW TO DO EVERYTHING (PUNY EDITION)\n"
"  Prettier / calmer         : lower BloomIntensity + Vignette, reduce WindStrength\n"
"  Slower or faster fields   : AnalogFields.TimeScale (0.2 = deep ocean time, >2.0 = hyper tide)\n"
"  Stronger waves / sharper gates : AnalogFields.WaveSpeed + GateFidelity\n"
"  Mouse paints the fields   : just move the mouse and hold left/right/middle (power = InjectStrength)\n"
"  See your real GPU + model : watch the big periodic status log (now prints 'Hardware: NVIDIA 4nm @ xxxxMHz (128 units, 256 edges...)')\n"
"  Force a core speed        : Hardware.TargetCoreClockMHz = 2600.0 (0 = automatic from fields + real clocks)\n"
"  Turn off scary sim        : Hardware.SimulateSubMicron = false\n"
"  Live edit anything        : press the ` (grave/backtick) key to open the prompt console, type 'guide' or 'set AnalogFields.WaveSpeed 1.4'\n"
"\n"
"F-KEYS (work regardless of mastery level)\n"
"  F1 adaptive  F2 hw-rt  F3 rtxgi  F4 accum  F5 tonemap  F6 bloom  F7 resetcam  F9 fieldviz  F10 resetfields\n"
"  DOS panel: double-click anywhere = stamp/zoom  drag border/HUD to move  |  F8/F11/Alt+Enter FS\n"
"  Boot: x86.comp + Start button (no desktop wallpaper required)\n"
"\n"
"ALL THE IMPORTANT NEW KNOBS LIVE HERE (centralized & documented):\n"
"  Options::AnalogFields   (the living analog gate fabric - Phi/Thermo/Flow + FCC)\n"
"  Options::Hardware       (spiderweb graph, real sysfs clocks, sub-micron physics, per-vendor)\n"
"  + the classics (Rendering, Camera, LivingWorld, Input...)\n"
"\n"
"Type 'guide' in the ` prompt for this text.\n"
"The tide is love. The options are the currents. Swim safely or walk the tide.\n"
"AMOURANTH FOREVER\n";
    }
}