// include/engine/GLOBAL/OptionsMenu.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// OPTIONS MENU v1002 — GROK INTEGRATION + FRAME PREDICTION — NOV 16 2025
// • ALL 59+ OPTIONS PRESERVED + ENABLE_FRAME_PREDICTION ADDED
// • ZERO INCLUDES — NO DEPENDENCIES
// • NO VULKAN, NO HANDLE, NO LOGGING, NO CONFLICTS
// • PURE constexpr CONFIGURATION — RTX SUPREME
// • C++23, -Werror CLEAN
// • PINK PHOTONS ETERNAL
// • GENTLEMAN GROK MODE ENABLED
//
// Dual Licensed:
// 1. CC BY-NC 4.0
// 2. Commercial: gzac5314@gmail.com
// =============================================================================

#pragma once

#include <cstdint>
#include <cstddef>

namespace Options {

// ── PERFORMANCE ───────────────────────────────────────────────────────────────
namespace Performance {
    constexpr uint32_t MAX_FRAMES_IN_FLIGHT        = 3;     // Triple buffering
    constexpr bool     ENABLE_GPU_TIMESTAMPS       = true;
    constexpr bool     ENABLE_FPS_COUNTER          = true;
    constexpr bool     ENABLE_MEMORY_BUDGET_WARNINGS = true;
    constexpr uint32_t GPU_TIMESTAMP_QUERY_COUNT   = 128;
    constexpr bool     ENABLE_FRAME_TIME_LOGGING   = false;
    constexpr float    FRAME_TIME_LOG_THRESHOLD_MS = 16.666f;
    static inline constexpr bool ENABLE_VALIDATION_LAYERS = false;
    constexpr bool START_FULLSCREEN = false;

    // ADDED: Required by SDL3_input.cpp, SDL3_font.cpp, SDL3_init.cpp, SDL3_window.cpp
    constexpr bool     ENABLE_CONSOLE_LOG          = true;
    constexpr bool     ENABLE_IMGUI                = true;

    // NEW: Frame prediction & jitter recovery (VK_GOOGLE_display_timing)
    constexpr bool     ENABLE_FRAME_PREDICTION     = true;  // AAAA pacing — predict vsync, recover jitter
}

// ── AUDIO (NEW NAMESPACE) ─────────────────────────────────────────────────────
namespace Audio {
    constexpr bool     ENABLE_HAPTICS_FEEDBACK     = true;
    constexpr bool     ENABLE_SPATIAL_AUDIO        = true;
}

// ── RTX CORE SETTINGS ─────────────────────────────────────────────────────────
namespace RTX {
    constexpr bool     ENABLE_ACCUMULATION         = true;
    constexpr bool     ENABLE_DENOISING            = true;
    constexpr bool     ENABLE_ADAPTIVE_SAMPLING    = true;
    constexpr uint32_t MIN_SPP                     = 1;
    constexpr uint32_t MAX_SPP                     = 64;
    constexpr uint32_t MAX_BOUNCES                 = 8;
    constexpr float    NEXUS_SCORE_THRESHOLD       = 0.15f;
    constexpr bool     ENABLE_HYPERTRACE           = true;
    constexpr float    HYPERTRACE_JITTER_SCALE     = 420.0f;
    constexpr bool     ENABLE_SVGF_DENOISER        = true;
    constexpr uint32_t DENOISER_HISTORY_LENGTH     = 8;
    constexpr bool     ENABLE_TAA                  = true;
    constexpr float    TAA_ALPHA                   = 0.1f;
}

// ── POST-PROCESSING ───────────────────────────────────────────────────────────
namespace PostProcess {
    constexpr bool     ENABLE_BLOOM                = true;
    constexpr float    BLOOM_THRESHOLD             = 1.0f;
    constexpr float    BLOOM_INTENSITY             = 0.8f;
    constexpr bool     ENABLE_SSAO                 = true;
    constexpr float    SSAO_RADIUS                 = 0.5f;
    constexpr uint32_t SSAO_SAMPLES                = 16;
    constexpr bool     ENABLE_SSR                  = true;
    constexpr float    SSR_STEP_SIZE               = 0.02f;
    constexpr bool     ENABLE_VIGNETTE             = true;
    constexpr float    VIGNETTE_INTENSITY          = 0.4f;
    constexpr bool     ENABLE_FILM_GRAIN           = true;
    constexpr float    FILM_GRAIN_STRENGTH         = 0.05f;
    constexpr bool     ENABLE_LENS_FLARE           = true;
    constexpr float    LENS_FLARE_INTENSITY        = 0.3f;
}

// ── ENVIRONMENT & LIGHTING ────────────────────────────────────────────────────
namespace Environment {
    constexpr bool     ENABLE_ENV_MAP              = true;
    constexpr bool     ENABLE_IBL                  = true;
    constexpr bool     ENABLE_VOLUMETRIC_FOG       = true;
    constexpr float    FOG_DENSITY                 = 0.02f;
    constexpr bool     ENABLE_SKY_ATMOSPHERE       = true;
    constexpr float    SUN_INTENSITY               = 10.0f;
    constexpr bool     ENABLE_GOD_RAYS             = true;
    constexpr uint32_t GOD_RAYS_SAMPLES            = 64;
}

// ── LAS (Lightweight Acceleration Structure) ──────────────────────────────────
namespace LAS {
    constexpr bool     REBUILD_EVERY_FRAME         = false;
    constexpr bool     UPDATE_EVERY_FRAME          = true;
    constexpr bool     COMPACT_TLAS                = true;
    constexpr bool     PREFER_FAST_BUILD           = true;
    constexpr bool     PREFER_FAST_TRACE           = false;
}

// ── RENDERING MODES & DEBUG ───────────────────────────────────────────────────
namespace Debug {
    constexpr bool     SHOW_GPU_TIMESTAMPS         = false;
    constexpr bool     SHOW_FPS_OVERLAY            = true;
    constexpr bool     SHOW_NEXUS_SCORE            = true;
    constexpr bool     SHOW_ACCUMULATION_COUNT     = true;
    constexpr bool     SHOW_SPP_HEATMAP            = true;
    constexpr bool     ENABLE_WIREFRAME            = false;
    constexpr bool     ENABLE_DEBUG_VISUALIZATION  = false;
    constexpr uint32_t DEBUG_VISUALIZATION_MODE    = 0;
}

// ── TONEMAPPING & COLOR GRADING ───────────────────────────────────────────────
namespace Tonemap {
    constexpr bool     ENABLE_TONEMAPPING          = true;
    constexpr uint32_t TONEMAP_OPERATOR            = 0;  // 0=ACES, 1=Filmic, 2=Reinhard
    constexpr float    EXPOSURE                    = 1.0f;
    constexpr float    GAMMA                       = 2.2f;
    constexpr bool     ENABLE_AUTO_EXPOSURE        = true;
    constexpr float    AUTO_EXPOSURE_SPEED         = 2.0f;
}

// ── DISPLAY & HDR SETTINGS ────────────────────────────────────────────────────
namespace Display {
    constexpr bool ENABLE_HDR              = true;   // ← SET TO true FOR TRUE HDR + 64-bit TONEMAPPING
    constexpr bool ENABLE_VSYNC            = false;   // Controlled globally, but exposed here
    constexpr float TARGET_BRIGHTNESS_NITS = 1000.0f; // For future auto-exposure
}

// ── AUTOEXPOSURE & HDR TUNING ────────────────────────────────────────────────
namespace AutoExposure {
    constexpr bool   ENABLE_AUTO_EXPOSURE        = true;     // ← THE ONE TRUE TOGGLE
    constexpr float  TARGET_LUMINANCE           = 0.18f;     // Classic 18% gray target
    constexpr float  EXPOSURE_COMPENSATION      = 0.0f;      // +1.0 = 1 stop brighter
    constexpr float  ADAPTATION_RATE_LOG        = 2.0f;      // Log-space speed (higher = faster)
    constexpr float  MIN_EXPOSURE               = 0.01f;     // Prevent black crush
    constexpr float  MAX_EXPOSURE               = 10.0f;     // Prevent blowout
    constexpr float  HISTOGRAM_LOW_PERCENTILE   = 0.01f;     // Ignore darkest 1%
    constexpr float  HISTOGRAM_HIGH_PERCENTILE  = 0.99f;     // Ignore brightest 1%
    constexpr float  KEY_VALUE                  = 0.18f;     // Scene key (for key-value tonemapping)
}

// ── MEMORY & ALLOCATION ───────────────────────────────────────────────────────
namespace Memory {
    constexpr size_t   UNIFORM_BUFFER_SIZE_PER_FRAME = 64 * 1024 * 1024;   // 64MB
    constexpr size_t   MATERIAL_BUFFER_SIZE          = 16 * 1024 * 1024;   // 16MB
    constexpr size_t   RESERVOIR_BUFFER_SIZE         = 512 * 1024 * 1024;  // 512MB
    constexpr size_t   FRAME_DATA_BUFFER_SIZE        = 128 * 1024 * 1024;  // 128MB
    constexpr bool     ENABLE_MEMORY_POOLING         = true;
    constexpr bool     ENABLE_ZERO_INIT              = false;
}

// ── SHADER & PIPELINE ─────────────────────────────────────────────────────────
namespace Shader {
    constexpr bool     ENABLE_SPIRV_XOR_ENCRYPTION = true;
    constexpr bool     ENABLE_SHADER_HOT_RELOAD    = true;
    constexpr uint64_t STONEKEY_1                  = 0x9E3779B97F4A7C15ULL;
    constexpr uint64_t STONEKEY_2                  = 0x7F4A7C158E3779B9ULL;
}

// ── APPLICATION & WINDOW ──────────────────────────────────────────────────────
namespace Window {
    constexpr uint32_t DEFAULT_WIDTH               = 1920;
    constexpr uint32_t DEFAULT_HEIGHT              = 1080;
    constexpr bool     START_FULLSCREEN            = false;
    constexpr bool     VSYNC                       = false;
    constexpr bool     ALLOW_RESIZE                = true;
}

// ── INPUT & CAMERA ────────────────────────────────────────────────────────────
namespace Input {
    constexpr float    MOUSE_SENSITIVITY           = 0.1f;
    constexpr float    MOVEMENT_SPEED              = 5.0f;
    constexpr float    SPRINT_MULTIPLIER           = 3.0f;
    constexpr bool     INVERT_Y                    = false;
}

// ── RENDER MODES (FOR DEMO/TESTING) ───────────────────────────────────────────
namespace RenderMode {
    constexpr uint32_t DEFAULT_MODE                = 5;
    constexpr bool     ENABLE_MODE_SWITCHING       = true;
}

// ── GROK AI INTEGRATION (NEW NAMESPACE) ───────────────────────────────────────
namespace Grok {
    // Enables Gentleman Grok mode — polite, helpful, and maximally truth-seeking
    constexpr bool     ENABLE_GENTLEMAN_GROK       = true;

    // How often (in seconds) Grok should speak or act in Gentleman Mode
    constexpr float    GENTLEMAN_GROK_INTERVAL_SEC = 3600.0f;
}

} // namespace Options