// =============================================================================
// include/engine/options.hpp
// GORILLAZ RTX ENGINE - PLASTIC BEACH v∞ - RUNTIME OPTIONS & CONFIGURATION
// DECEMBER 17, 2025 - FULLY CONFIGURABLE PRESENTATION MODES
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
// BILLBOARD BEHAVIOR
// =============================================================================
inline bool   EnableBillboard         = true;
inline float  BillboardScale          = 30.0f;
inline float  BillboardZOffset        = 15.0f;
inline bool   BillboardDoubleSided    = true;
inline float  BillboardAlphaCutoff    = 0.5f;
inline bool   BillboardFaceCamera     = true;
inline float  BillboardAutoRotateY    = 0.0f;

// =============================================================================
// ENVIRONMENT / SKY
// =============================================================================
inline bool   UseEnvironmentAsSky     = true;
inline float  EnvironmentExposure     = 0.2f;
inline float  EnvironmentRotationY    = 0.0f;
inline bool   FlipEnvironmentV        = false;
inline float  SkyIntensity            = 1.0f;

// =============================================================================
// RENDERING & PERFORMANCE
// =============================================================================
inline bool   EnableJitterAA          = true;
inline float  JitterStrength          = 1.0f;
inline bool   ForceEnvironmentOnly    = false;
inline uint32_t AccumulationFrames    = 0;
inline bool   DenoiseAfterAccumulation = false;

// =============================================================================
// PRESENTATION & SWAPCHAIN - FULLY CONFIGURABLE
// =============================================================================
inline bool   ForceVSync              = true;  // true = hard vsync (FIFO - capped to refresh rate, no tearing)
                                                // false = uncapped rendering

inline bool   PreferMailboxForNoTearing = true; // When ForceVSync = false:
                                                // true = use MAILBOX if available (uncapped + no tearing)
                                                // false = use IMMEDIATE (uncapped + possible tearing, max compatibility)

inline uint32_t SwapchainImageCount   = 2;      // Base count - auto-increased to 3 when MAILBOX is used

    // =============================================================================
    // CAMERA START POSITION & LOOK
    // =============================================================================
    inline glm::vec3 CameraStartPosition  = glm::vec3(0.0f, 0.0f, 8.0f);  // Base position at ground level
    inline float     CameraStartYaw       = 180.0f;                      // Face forward toward the billboard (along -Z)
    inline float     CameraStartPitch     = 0.0f;                        // Look straight ahead (horizontal)
    inline float     CameraLookSensitivity= 0.07f;
    inline bool      InvertMouseLook      = true;
    constexpr float  CameraEyeHeight      = 14.0f;                        // Eye height above ground (standard human height)

    // =============================================================================
    // FPS PHYSICS & MOVEMENT
    // =============================================================================
    inline float GravityStrength          = 20.0f;                       // Positive value (gravity pulls down)
    inline float JumpForce                = 1128.5f;                        // Positive upward force
    inline float GroundLevel              = 0.0f;
    inline float FPSSpeed                 = 27.0f;
    inline float SprintMultiplier         = 1.6f;

// =============================================================================
// DEBUG & VISUALIZATION
// =============================================================================
inline bool   ShowHotPinkOnHit        = false;
inline bool   ShowNormals             = false;
inline bool   ShowUVs                 = false;
inline bool   ShowWireframe           = false;
inline bool   DebugPrintSpp           = true;

// =============================================================================
// MATERIAL OVERRIDES (FOR BILLBOARD)
// =============================================================================
inline glm::vec3 BillboardBaseColor    = glm::vec3(1.0f);
inline float  BillboardRoughness      = 0.8f;
inline float  BillboardMetallic       = 0.0f;
inline bool   BillboardUseAlphaBlend  = false;

// =============================================================================
// FUTURE FEATURES
// =============================================================================
inline bool   EnableReflections       = false;
inline bool   EnableShadows           = false;
inline bool   EnableVolumetrics       = false;
inline uint32_t MaxRayDepth           = 4;
inline uint32_t SamplesPerPixel       = 1;

} // namespace RTX::Options