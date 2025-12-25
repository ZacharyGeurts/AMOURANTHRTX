// =============================================================================
// include/engine/options.hpp
// GORILLAZ RTX ENGINE - PLASTIC BEACH v∞ - RUNTIME OPTIONS & CONFIGURATION
// DECEMBER 25, 2025 - MAXIMUM CONFIGURABILITY FOR FUTURE-PROOF MAIN.CPP
// =============================================================================

#pragma once

#include <string>
#include <glm/glm.hpp>

namespace RTX::Options {

// =============================================================================
// PATHS & ASSETS
// =============================================================================
inline std::string MonsterTexturePath = "assets/textures/monster.webp";
inline std::string EnvironmentMapPath = "assets/textures/envmap.hdr";

// =============================================================================
// BILLBOARD / MUG BEHAVIOR
// =============================================================================
inline bool   EnableBillboard         = true;
inline float  BillboardScale          = 1.0f;
inline float  BillboardZOffset        = 15.0f;
inline bool   BillboardDoubleSided    = true;
inline float  BillboardAlphaCutoff    = 0.5f;
inline bool   BillboardFaceCamera     = true;
inline float  BillboardAutoRotateY    = 5.0f;   // degrees per second - windmill rotation
inline glm::vec3 BillboardBaseColor   = glm::vec3(1.0f);
inline bool   BillboardUseAlphaBlend  = true;

// NEW: Mug tipping control (for standing upright or lying on its side)
inline bool   MugTipEnabled           = true;   // Set to true to tip the mug sideways
inline float  MugTipAngleDegrees      = 270.0f;   // Angle of roll around X-axis (90° = fully on side)
inline float  MugTipGroundOffsetY     = 1.5f;    // Extra Y translation after tipping to keep it resting on ground (tweak per model scale)

// =============================================================================
// ENVIRONMENT / SKY / ATMOSPHERE
// =============================================================================
inline bool   UseEnvironmentAsSky     = false;
inline float  EnvironmentExposure     = 0.8f;
inline float  EnvironmentRotationY    = 1.0f;   // Driven by main.cpp for day-night cycle
inline bool   FlipEnvironmentV        = false;
inline float  SkyIntensity            = 1.0f;   // Base intensity - pulsed for fog/mood
inline float  SkyRotationSpeed        = 0.2f;   // degrees per second for day-night cycle
inline float  FogPulseSpeed           = 0.8f;   // speed of volumetric mood pulse
inline float  FogPulseAmount          = 0.3f;   // intensity variation of sky/fog pulse

// =============================================================================
// RENDERING & PATH TRACING
// =============================================================================
inline bool   EnableJitterAA          = true;
inline float  JitterStrength          = 0.7f;
inline bool   ForceEnvironmentOnly    = false;
inline uint32_t AccumulationFrames    = 0;      // 0 = real-time progressive
inline bool   DenoiseAfterAccumulation = true;
inline uint32_t SamplesPerPixel       = 2;
inline uint32_t MaxRayDepth           = 31;

// =============================================================================
// RAY TRACING FEATURES
// =============================================================================
inline bool   EnableReflections       = true;
inline bool   EnableShadows           = true;
inline bool   EnableVolumetrics       = true;

// =============================================================================
// PRESENTATION & SWAPCHAIN
// =============================================================================
inline bool   ForceVSync              = false;
inline bool   PreferMailboxForNoTearing = true;
inline bool   ForceImmediateForMaxFps = false;

// =============================================================================
// CAMERA & MOVEMENT
// =============================================================================
inline glm::vec3 CameraStartPosition  = glm::vec3(0.0f, 0.0f, 8.0f);
inline float     CameraStartYaw       = 0.0f;
inline float     CameraStartPitch     = 0.0f;
inline float     CameraLookSensitivity= 0.07f;
inline bool      InvertMouseLook      = true;
constexpr float  CameraEyeHeight      = 1.7f;    // meters - standard eye height

inline float GravityStrength          = 9.81f;
inline float JumpForce                = 8.0f;
inline float GroundLevel              = 0.0f;
inline float FPSSpeed                 = 5.0f;    // walking speed in m/s
inline float SprintMultiplier         = 2.0f;    // shift key

// Camera head bobble
inline bool   EnableCameraBobble      = true;
inline float  CameraBobbleFrequency   = 2.5f;   // cycles per second when moving
inline float  CameraBobbleAmplitude   = 0.008f;  // vertical offset in meters

// =============================================================================
// POINT LIGHTS ANIMATION
// =============================================================================
inline float  LightBobSpeed           = 0.3f;   // vertical bobbing frequency
inline float  LightBobAmplitude       = 0.028f;
inline float  LightOrbitSpeed         = 0.3f;   // horizontal orbit frequency
inline float  LightOrbitAmplitude     = 0.005f;
inline float  LightColorPulseSpeed    = 0.5f;   // color tint pulse
inline float  LightColorPulseAmount   = 0.01f;

// =============================================================================
// DEBUG & VISUALIZATION (F1 carousel in main.cpp)
// =============================================================================
inline bool   ShowHotPinkOnHit        = false;
inline bool   ShowNormals             = false;
inline bool   ShowUVs                 = false;
inline bool   ShowWireframe           = false;
inline bool   DebugPrintSpp           = false;

// =============================================================================
// INPUT & UI
// =============================================================================
inline bool   StartWithMouseCapture   = true;
inline bool   StartFullscreen         = true;

// =============================================================================
// PERFORMANCE & LOGGING
// =============================================================================
inline float  StatusLogInterval       = 1.0f;   // seconds between status prints

} // namespace RTX::Options