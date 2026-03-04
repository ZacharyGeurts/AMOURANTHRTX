#pragma once

// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial (gzac5314@gmail.com)
// AMOURANTH FOREVER 💖
// =============================================================================

#include <glm/glm.hpp>

namespace Options::GameStyle
{
    enum class DimensionMode : uint32_t
    {
        TextOnly       = 0,     // ASCII / terminal style (very cheap, fun easter egg)
        Pure2D         = 1,     // side-scroller / classic pixel art
        TwoPointFiveD  = 2,     // 3D models but locked movement plane (e.g. Trine, LittleBigPlanet layers)
        Full3D         = 3      // free 6DoF movement & camera
    };

    enum class CameraPerspective : uint32_t
    {
        FirstPerson       = 0, // player's perspective
        ThirdPerson       = 1, // camera follows behind the character
        TopDown           = 2, // classic RTS / twin-stick
        Isometric         = 3, // angled view creating depth
        SideScroller      = 4, // fixed side view (2D or 2.5D)
        Orthographic2D    = 5, // pure 2D ortho projection
        TextAdventure     = 6  // no real camera, narrative mode
    };

    enum class GenrePreset : uint32_t
    {
        None              = 0, // no specific genre
        FPS               = 1, // fast movement, first-person, shooting focus
        ThirdPersonAction = 2, // over-shoulder / behind character perspective
        Platformer        = 3, // jump-heavy, precise movement
        Metroidvania      = 4, // exploration + backtracking
        TopDownRPG        = 5, // top-down role-playing game
        TwinStickShooter  = 6, // shooter controlled with dual sticks
        SurvivalHorror    = 7, // horror themes with survival elements
        Roguelike         = 8, // randomized levels and permadeath
        TextAdventure     = 9, // narrative-based games without graphic elements
        Shmup             = 10, // shoot 'em up (vertical/horizontal scrolling)
        Racing            = 11, // racing focused gameplay
        Puzzle            = 12, // gameplay centered around solving puzzles
        Fighting          = 13, // combat between characters
        Sports            = 14, // gameplay focused on sports
        Simulation        = 15, // life simulation or business simulation genres
        Strategy          = 16, // logical reasoning and planning focused games
        MMORPG            = 17, // massive multiplayer online role-playing games
        PartyGame         = 18  // games meant for multiple players at once
        // Additional genres can be added as needed
    };

    // runtime values (these will be modified by menu / config)
    inline DimensionMode       CurrentDimension     = DimensionMode::Full3D;
    inline CameraPerspective   CurrentPerspective   = CameraPerspective::FirstPerson;
    inline GenrePreset         CurrentGenre         = GenrePreset::FPS;

    // helper flags derived from above (used in shader & C++ logic)
    inline bool Is3D()          { return CurrentDimension == DimensionMode::Full3D; }
    inline bool Is25D()         { return CurrentDimension == DimensionMode::TwoPointFiveD; }
    inline bool Is2D()          { return CurrentDimension <= DimensionMode::TwoPointFiveD && CurrentDimension != DimensionMode::TextOnly; }
    inline bool IsTextMode()    { return CurrentDimension == DimensionMode::TextOnly; }
    inline bool IsFirstPerson() { return CurrentPerspective == CameraPerspective::FirstPerson; }
    inline bool IsThirdPerson() { return CurrentPerspective == CameraPerspective::ThirdPerson; }
    inline bool IsTopDown()     { return CurrentPerspective == CameraPerspective::TopDown; }
    inline bool IsIsometric()    { return CurrentPerspective == CameraPerspective::Isometric; }
    inline bool IsSideScroller() { return CurrentPerspective == CameraPerspective::SideScroller; }
    inline bool IsOrthographic2D() { return CurrentPerspective == CameraPerspective::Orthographic2D; }
    inline bool IsTextAdventureMode() { return CurrentPerspective == CameraPerspective::TextAdventure; }
    inline bool IsGenreFPS()        { return CurrentGenre == GenrePreset::FPS; }
    inline bool IsGenrePlatformer() { return CurrentGenre == GenrePreset::Platformer; }
    // Add more genre checks as needed
}


// ─────────────────────────────────────────────────────────────────────────────
// Window & Display
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Window {
    inline constexpr int     DEFAULT_WIDTH            = 1920;
    inline constexpr int     DEFAULT_HEIGHT           = 1080;
    inline constexpr bool    START_FULLSCREEN         = false;
    inline constexpr bool    ALLOW_RESIZE             = true;
    inline constexpr bool    VSYNC                    = false;      // FIFO vs IMMEDIATE
    inline constexpr bool    BORDERLESS               = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Camera — all defaults & feel
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Camera {
    inline constexpr glm::vec3 START_POSITION         = glm::vec3(0.0f, 1.8f, 10.0f);
    inline constexpr float     DEFAULT_FOV            = 75.0f;
    inline constexpr float     DEFAULT_NEAR           = 0.1f;
    inline constexpr float     DEFAULT_FAR            = 1000.0f;

    inline constexpr float     DEFAULT_APERTURE       = 2.8f;       // f-stop
    inline constexpr float     DEFAULT_FOCUS_DISTANCE = 8.0f;       // meters

    inline constexpr float     MOUSE_SENSITIVITY      = 0.12f;
    inline constexpr bool      INVERT_MOUSE_Y         = false;
    inline constexpr float     MOVEMENT_SPEED         = 12.0f;      // m/s
    inline constexpr float     SPRINT_MULTIPLIER      = 2.5f;
    inline constexpr float     ZOOM_SENSITIVITY       = 1.5f;

    inline constexpr bool      ENABLE_HEAD_BOB        = true;
    inline constexpr float     HEAD_BOB_INTENSITY     = 0.06f;
    inline constexpr float     HEAD_BOB_FREQUENCY     = 2.0f;

    inline constexpr bool      ENABLE_BREATHING       = true;
    inline constexpr float     BREATHING_INTENSITY    = 0.025f;

    inline constexpr bool      ENABLE_CAMERA_SHAKE    = true;

    inline constexpr float     DOLLY_SPEED            = 8.0f;
    inline constexpr float     CRANE_SPEED            = 6.0f;
    inline constexpr float     RACK_FOCUS_SPEED       = 4.5f;
}

// ─────────────────────────────────────────────────────────────────────────────
// Rendering & Performance
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Rendering {
    // Internal render resolution (ray tracing / compute target size)
    // This is fixed — swapchain/window size can be different (upscaled via blit)
    inline constexpr int     INTERNAL_WIDTH           = 1920;   // ← Change to your preferred quality/perf balance
    inline constexpr int     INTERNAL_HEIGHT          = 1080;   //   2560×1440, 3840×2160, etc. are great choices

    inline constexpr bool    ACCUMULATION             = true;      // temporal reprojection / accumulation
    inline constexpr bool    ADAPTIVE_SAMPLING        = true;
    inline constexpr int     MAX_RAY_RECURSION        = 8;
    inline constexpr float   EXPOSURE                 = 0.2f;     // HDR → screen brightness
    inline constexpr bool    ENABLE_TONEMAP           = true;      // Reinhard / ACES
    inline constexpr int     DISPATCH_GROUP_SIZE      = 16;        // compute local workgroup size
}

// ─────────────────────────────────────────────────────────────────────────────
// Sky & Day/Night Cycle (Earth-realistic)
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Sky {
    inline constexpr bool    ENABLE_DAY_NIGHT_CYCLE   = true;
    inline constexpr float   DAY_LENGTH_SECONDS       = 1200.0f;   // 20 min full cycle
    inline constexpr float   CYCLE_SPEED              = 1.0f;      // multiplier
    inline constexpr float   START_TIME_OF_DAY        = 12.0f;     // noon

    inline constexpr bool    SUN_ENABLED              = true;
    inline constexpr glm::vec3 SUN_COLOR              = glm::vec3(1.0f, 0.96f, 0.88f);
    inline constexpr float   SUN_INTENSITY_DAY        = 12.0f;
    inline constexpr float   SUN_INTENSITY_NIGHT      = 0.1f;

    inline constexpr bool    MOON_ENABLED             = true;
    inline constexpr glm::vec3 MOON_COLOR             = glm::vec3(0.9f, 0.95f, 1.0f);
    inline constexpr float   MOON_INTENSITY           = 2.0f;

    inline constexpr float   FOG_DENSITY              = 0.0008f;
    inline constexpr float   CLOUD_DENSITY            = 0.4f;

    inline constexpr glm::vec4 SKY_ZENITH_DAY         = glm::vec4(0.3f, 0.55f, 1.0f, 1.0f);
    inline constexpr glm::vec4 SKY_HORIZON_DAY        = glm::vec4(0.6f, 0.8f, 1.0f, 1.0f);
    inline constexpr glm::vec4 SKY_ZENITH_NIGHT       = glm::vec4(0.01f, 0.02f, 0.05f, 1.0f);
    inline constexpr glm::vec4 SKY_HORIZON_NIGHT      = glm::vec4(0.03f, 0.03f, 0.08f, 1.0f);

    inline constexpr glm::vec4 GROUND_COLOR_DAY       = glm::vec4(0.18f, 0.35f, 0.12f, 1.0f);
    inline constexpr glm::vec4 GROUND_COLOR_NIGHT     = glm::vec4(0.08f, 0.12f, 0.18f, 1.0f);
}

// ─────────────────────────────────────────────────────────────────────────────
// LAS (Acceleration Structures)
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::LAS {
    inline constexpr bool    SYNC_REBUILD             = true;      // true = block main thread
                                                                   // false = async thread (future)
    inline constexpr bool    ENABLE_TLAS              = true;
    inline constexpr bool    ENABLE_BLAS              = true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Audio — matches SDL3.hpp usage exactly
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Audio {
    inline constexpr bool    ENABLE_HAPTICS_FEEDBACK  = true;      // controller rumble
    inline constexpr int     SAMPLE_RATE              = 48000;
    inline constexpr int     CHANNELS                 = 2;
    inline constexpr int     BUFFER_SIZE              = 2048;      // latency vs stability
}

// ─────────────────────────────────────────────────────────────────────────────
// Debug & Tools
// ─────────────────────────────────────────────────────────────────────────────
namespace Options::Debug {
    inline constexpr bool    ENABLE_VALIDATION_LAYERS = true;
    inline constexpr bool    ENABLE_VERBOSE_LOGGING   = false;
    inline constexpr bool    DRAW_WIREFRAME           = false;
}