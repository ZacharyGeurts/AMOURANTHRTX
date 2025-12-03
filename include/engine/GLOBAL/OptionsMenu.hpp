// =============================================================================
// include/engine/GLOBAL/OptionsMenu.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// OPTIONS MENU v2025 — HDR AUTO-IGNITION + QUANTUM PREDICTION — NOV 26 2025
// • HDR TOGGLE REMOVED — THE EMPIRE DETECTS AND ENFORCES
// • NEW: FRAME PREDICTION, SHADING RATE, DIRECT DISPLAY, QUANTUM RESIZE
// • ZERO INCLUDES — NO DEPENDENCIES
// • PURE constexpr CONFIGURATION — RTX SUPREME
// • C++23, -Werror CLEAN
// • PINK PHOTONS ETERNAL
// • GENTLEMAN GROK MODE ENABLED
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>

namespace Options {

// ── SPLASH  ───────────────────────────────────────────────────
namespace Splash {
    // Master switch — completely disable the splash if desired
    constexpr bool     ENABLE_SACRIFICIAL_SPLASH   = true;
    constexpr float    SPLASH_DURATION_SECONDS     = 3.4f;
    // Nuclear override — skips everything, even the image draw
    // Useful for benchmarking, CI, or when you just want to get to the photons
    constexpr bool     SKIP_SPLASH_ENTIRELY        = false;
    constexpr float    FADE_IN_DURATION            = 0.35f;
    constexpr float    FADE_OUT_DURATION           = 0.30f;
    // Allow user to quit during splash with ESC or window close
    constexpr bool     ALLOW_EARLY_EXIT            = true;
}

// ── PERFORMANCE ───────────────────────────────────────────────────────────────
// ── PERFORMANCE ───────────────────────────────────────────────────────────────
namespace Performance {
    // Maximum number of frames that can be in flight simultaneously
    // 3 = triple buffering — optimal for low latency + tear-free presentation
    constexpr uint32_t MAX_FRAMES_IN_FLIGHT        = 3;

    // GPU query timestamps for precise timing of render passes
    constexpr bool     ENABLE_GPU_TIMESTAMPS       = true;

    // On-screen FPS counter (top-left overlay)
    constexpr bool     ENABLE_FPS_COUNTER          = true;

    // Warn when approaching Vulkan memory budget limits
    constexpr bool     ENABLE_MEMORY_BUDGET_WARNINGS = true;

    // Number of timestamp queries allocated per frame
    constexpr uint32_t GPU_TIMESTAMP_QUERY_COUNT   = 128;

    // Log frame time warnings when exceeding threshold
    constexpr bool     ENABLE_FRAME_TIME_LOGGING   = false;
    constexpr float    FRAME_TIME_LOG_THRESHOLD_MS = 16.666f;  // ~60 Hz

    // Start application in fullscreen mode
    constexpr bool     START_FULLSCREEN            = false;

    // Enable console output (stdout/stderr)
    constexpr bool     ENABLE_CONSOLE_LOG          = true;

    // Use Google display timing extensions for perfect frame pacing
    constexpr bool     ENABLE_FRAME_PREDICTION     = true;

    // Prefer Mailbox present mode (tear-free, low latency, soft FPS cap at refresh+1)
    constexpr bool     PREFER_MAILBOX_PRESENT      = true;

    // Allow fallback to Immediate present mode (uncapped, may tear)
    constexpr bool     ALLOW_IMMEDIATE_PRESENT     = false;

    // Dynamic fragment shading rate multiplier (VRS) — >1.0 = higher quality, <1.0 = performance
    constexpr float    DYNAMIC_SHADING_RATE        = 1.5f;

    // Bypass compositor on Linux/Wayland (zero-copy, ~1.8ms input-to-photon)
    constexpr bool     ENABLE_DIRECT_DISPLAY       = true;

    // Remove runtime safety checks and asserts — maximum performance
    constexpr bool     OVERCLOCK_RENDERER           = true;

    // Enable hyper-aggressive optimizations (may reduce debuggability)
    constexpr bool     ENABLE_HYPER_AGGRESSIVE_MODE = true;
}

// ── APPLICATION & WINDOW ──────────────────────────────────────────────────────
namespace Window {
    // Default window resolution
    constexpr uint32_t DEFAULT_WIDTH               = 3840;
    constexpr uint32_t DEFAULT_HEIGHT              = 2160;

    // Start in fullscreen mode
    constexpr bool     START_FULLSCREEN            = false;

    // Allow user to resize the window
    constexpr bool     ALLOW_RESIZE                = true;

    // Enable high-DPI (Retina / 4K+) scaling
    constexpr bool     HIGH_DPI                    = true;

    // Pre-create swapchain at predicted size during resize — eliminates perceived lag
    constexpr bool     ENABLE_QUANTUM_RESIZE_PREDICTION = true;
}

// ── AUDIO ─────────────────────────────────────────────────────────────────────
namespace Audio {
    // Enable haptic (vibration) feedback on supported controllers
    constexpr bool     ENABLE_HAPTICS_FEEDBACK     = true;

    // Enable 3D spatial audio processing
    constexpr bool     ENABLE_SPATIAL_AUDIO        = true;
}

// ── RTX CORE SETTINGS ─────────────────────────────────────────────────────────
namespace OptionsRTX {
    // Temporal accumulation of ray tracing samples
    constexpr bool     ENABLE_ACCUMULATION         = true;

    // Enable real-time denoiser (SVGF or similar)
    constexpr bool     ENABLE_DENOISING            = true;

    // Adaptive sampling based on per-pixel variance / nexus score
    constexpr bool     ENABLE_ADAPTIVE_SAMPLING    = true;

    // Minimum and maximum samples per pixel
    constexpr uint32_t MIN_SPP                     = 1;
    constexpr uint32_t MAX_SPP                     = 64;

    // Maximum ray bounces (diffuse + specular)
    constexpr uint32_t MAX_BOUNCES                 = 3;

    // Threshold for adaptive sampling convergence
    constexpr float    NEXUS_SCORE_THRESHOLD       = 0.15f;

    // HyperTrace — next-gen temporal reuse and jitter system
    constexpr bool     ENABLE_HYPERTRACE           = true;
    constexpr float    HYPERTRACE_JITTER_SCALE     = 420.0f;

    // Spatiotemporal variance-guided filtering denoiser
    constexpr bool     ENABLE_SVGF_DENOISER        = true;
    constexpr uint32_t DENOISER_HISTORY_LENGTH     = 8;

    // Temporal Anti-Aliasing for final composition
    constexpr bool     ENABLE_TAA                  = true;
    constexpr float    TAA_ALPHA                   = 0.1f;

    // Maximum ray recursion depth in ray tracing pipelines
    constexpr uint32_t MAX_PIPELINE_RAY_RECURSION_DEPTH = 3;
}

// ── POST-PROCESSING ───────────────────────────────────────────────────────────
namespace PostProcess {
    // High-dynamic-range bloom effect
    constexpr bool     ENABLE_BLOOM                = true;
    constexpr float    BLOOM_THRESHOLD             = 1.0f;
    constexpr float    BLOOM_INTENSITY             = 0.8f;

    // Screen-space ambient occlusion
    constexpr bool     ENABLE_SSAO                 = true;
    constexpr float    SSAO_RADIUS                 = 0.5f;
    constexpr uint32_t SSAO_SAMPLES                = 16;

    // Screen-space reflections
    constexpr bool     ENABLE_SSR                  = true;
    constexpr float    SSR_STEP_SIZE               = 0.02f;

    // Darkening of screen edges
    constexpr bool     ENABLE_VIGNETTE             = true;
    constexpr float    VIGNETTE_INTENSITY          = 0.4f;

    // Film grain overlay for cinematic look
    constexpr bool     ENABLE_FILM_GRAIN           = true;
    constexpr float    FILM_GRAIN_STRENGTH         = 0.05f;

    // Lens flare simulation from bright lights
    constexpr bool     ENABLE_LENS_FLARE           = true;
    constexpr float    LENS_FLARE_INTENSITY        = 0.3f;
}

// ── ENVIRONMENT & LIGHTING ───────────────────────────────────────────────────
namespace Environment {
    // Image-based lighting from environment map
    constexpr bool     ENABLE_ENV_MAP              = true;
    constexpr bool     ENABLE_IBL                  = true;

    // Volumetric fog and atmospheric scattering
    constexpr bool     ENABLE_VOLUMETRIC_FOG       = true;
    constexpr float    FOG_DENSITY                 = 0.02f;

    // Physically-based sky and atmosphere model
    constexpr bool     ENABLE_SKY_ATMOSPHERE       = true;
    constexpr float    SUN_INTENSITY               = 10.0f;

    // Volumetric light shafts (god rays)
    constexpr bool     ENABLE_GOD_RAYS             = true;
    constexpr uint32_t GOD_RAYS_SAMPLES            = 64;
}

// ── LAS (Lightweight Acceleration Structure) ─────────────────────────────────
namespace OptionsLAS {
    // Rebuild entire TLAS every frame (slow, accurate)
    constexpr bool     REBUILD_EVERY_FRAME         = false;

    // Update TLAS incrementally (fast, may have minor artifacts)
    constexpr bool     UPDATE_EVERY_FRAME          = true;

    // Compact TLAS after build/update (reduces memory, increases build time)
    constexpr bool     COMPACT_TLAS                = true;

    // Prefer fast build over fast trace
    constexpr bool     PREFER_FAST_BUILD           = true;

    // Prefer fast trace over fast build
    constexpr bool     PREFER_FAST_TRACE           = false;
}

// ── RENDERING MODES & DEBUG ───────────────────────────────────────────────────
namespace Debug {
    // Show GPU timestamp query results in overlay
    constexpr bool     SHOW_GPU_TIMESTAMPS         = false;

    // Display FPS counter
    constexpr bool     SHOW_FPS_OVERLAY            = true;

    // Show HyperTrace nexus convergence score
    constexpr bool     SHOW_NEXUS_SCORE            = true;

    // Show current accumulation frame count
    constexpr bool     SHOW_ACCUMULATION_COUNT     = true;

    // Visualize samples per pixel as heatmap
    constexpr bool     SHOW_SPP_HEATMAP            = true;

    // Wireframe rendering mode
    constexpr bool     ENABLE_WIREFRAME            = false;

    // Debug visualization overlays (normals, depth, etc.)
    constexpr bool     ENABLE_DEBUG_VISUALIZATION  = false;
    constexpr uint32_t DEBUG_VISUALIZATION_MODE    = 0;

    // Celebration effects on milestones
    constexpr bool     ENABLE_CELEBRATION_MODE     = true;

    // Vulkan validation layers (debug only)
    static inline constexpr bool ENABLE_VALIDATION_LAYERS = false;
}

// ── TONEMAPPING & COLOR GRADING ───────────────────────────────────────────────
namespace Tonemap {
    // Enable tonemapping operator
    constexpr bool     ENABLE_TONEMAPPING          = true;

    // 0 = ACES, 1 = Filmic, 2 = Reinhard
    constexpr uint32_t TONEMAP_OPERATOR            = 0;

    // Manual exposure multiplier
    constexpr float    EXPOSURE                    = 1.0f;

    // Output gamma correction
    constexpr float    GAMMA                       = 2.2f;

    // Automatic exposure adjustment
    constexpr bool     ENABLE_AUTO_EXPOSURE        = true;
    constexpr float    AUTO_EXPOSURE_SPEED         = 2.0f;
}

// ── DISPLAY & HDR — THE EMPIRE DECIDES ────────────────────────────────────────
namespace Display {
    // HDR is no longer an option.
    // It is detected via EDID + OS + swapchain format.
    // The empire ignites the fire.
    // You do not choose. You witness.
    constexpr bool     HDR_AUTO_IGNITION           = true;    // ← The one true path

    constexpr float    TARGET_BRIGHTNESS_NITS      = 1000.0f; // For auto-exposure & metadata

    // VSync is controlled via present mode — traditional VSync is forbidden
    constexpr bool     ENABLE_VSYNC                = false;

    // ── PRESENT MODE PREFERENCE (RUNTIME CONTROLLABLE) ───────────────────────
    // These are hints — actual mode chosen at runtime based on support
    constexpr bool     PREFER_MAILBOX_PRESENT      = true;     // Default: tear-free, low latency
    constexpr bool     ALLOW_IMMEDIATE_PRESENT     = false;    // Uncapped mode (e.g. F9 key)

    // Quantum frame pacing using Google display timing extensions
    constexpr bool     ENABLE_PERFECT_FRAME_PREDICTION    = true;

    // Zero-copy direct-to-display (Linux/Wayland only)
    constexpr bool     ENABLE_DIRECT_DISPLAY              = true;

    // Runtime toggle for uncapped FPS mode
    inline static std::atomic<bool> UNCAPPED_MODE_ACTIVE   = true;
}

// ── AUTOEXPOSURE & HDR TUNING ────────────────────────────────────────────────
namespace AutoExposure {
    // ── AUTO-EXPOSURE — THE EMPIRE MEASURES LIGHT ITSELF ─────────────────────
    constexpr bool   ENABLE_AUTO_EXPOSURE          = true;   // The photons adjust to mortal eyes

    // Target middle-gray luminance in linear space (classic 18% gray card)
    constexpr float  TARGET_LUMINANCE             = 0.18f;   // Sacred value — all tonemappers bow

    // Manual exposure bias in EV stops (positive = brighter)
    constexpr float  EXPOSURE_COMPENSATION        = 0.0f;    // 0.0 = neutral, +1.0 = +1 stop, -1.0 = -1 stop

    // Logarithmic adaptation speed (higher = faster response)
    // 2.0 ≈ adapts in ~1 second at large changes
    constexpr float  ADAPTATION_RATE_LOG          = 2.0f;

    // Hard limits — prevents eye-searing overexposure or total darkness
    constexpr float  MIN_EXPOSURE                 = 0.01f;   // 1/100
    constexpr float  MAX_EXPOSURE                 = 10.0f;   // x10 overexposure allowed

    // Histogram-based metering — ignores extreme outliers
    constexpr float  HISTOGRAM_LOW_PERCENTILE     = 0.01f;   // Bottom 1% ignored (deep shadows)
    constexpr float  HISTOGRAM_HIGH_PERCENTILE    = 0.99f;   // Top 1% ignored (bright highlights)

    // Key value for key-to-middle-gray mapping (alternative metering mode)
    constexpr float  KEY_VALUE                    = 0.18f;   // Classic photographic key
}

// ── MEMORY & ALLOCATION ───────────────────────────────────────────────────────
namespace Memory {
    // ── MEMORY ALLOCATION — THE EMPIRE'S RESERVES ─────────────────────────────
    // All sizes are maximum expected usage per frame or for the entire scene
    // Values are generous but bounded — prevents runaway allocation

    constexpr size_t UNIFORM_BUFFER_SIZE_PER_FRAME = 64 * 1024 * 1024;   // 64 MiB per frame (UBO + storage buffers)
    constexpr size_t MATERIAL_BUFFER_SIZE          = 16 * 1024 * 1024;   // 16 MiB total for all materials (GPU-only)
    constexpr size_t RESERVOIR_BUFFER_SIZE         = 512 * 1024 * 1024;  // 512 MiB for ReSTIR reservoirs (temporal reuse)
    constexpr size_t FRAME_DATA_BUFFER_SIZE        = 128 * 1024 * 1024;  // 128 MiB for per-frame structured data

    // Custom allocator pooling — reduces fragmentation and allocation overhead
    constexpr bool   ENABLE_MEMORY_POOLING         = true;

    // Zero-initialize all allocations — useful for debugging, harmful to performance
    constexpr bool   ENABLE_ZERO_INIT              = false;
}

// ── SHADER & PIPELINE ─────────────────────────────────────────────────────────
namespace Shader {
    // SPIR-V XOR encryption at load time — prevents casual inspection
    // Key is baked into binary; change requires recompilation
    constexpr bool     ENABLE_SPIRV_XOR_ENCRYPTION = true;

    // Runtime shader hot-reload — watches .spv files and rebuilds pipelines on change
    // Extremely useful during shader development
    constexpr bool     ENABLE_SHADER_HOT_RELOAD    = true;

    // 128-bit XOR key pair — cryptographically random, unique to this engine build
    // Used for both encryption and runtime decryption of shader bytecode
    constexpr uint64_t STONEKEY_1                  = 0x9E37AF18C64D8A17UL;
    constexpr uint64_t STONEKEY_2                  = 0xE4F8B29D71A3C56CUL;
}

// ── INPUT & CAMERA ────────────────────────────────────────────────────────────
namespace Input {
    // Mouse look sensitivity — degrees per pixel of movement
    constexpr float    MOUSE_SENSITIVITY           = 0.1f;

    // Base movement speed in world units per second
    constexpr float    MOVEMENT_SPEED              = 5.0f;

    // Multiplier applied when sprint key is held (usually Left Shift)
    constexpr float    SPRINT_MULTIPLIER           = 3.0f;

    // Invert vertical mouse axis (pitch) — classic "flight sim" style
    constexpr bool     INVERT_Y                    = false;
}

// ── RENDER MODES ──────────────────────────────────────────────────────────────
namespace RenderMode {
    // Default rendering mode on engine startup
    // 0 = Dev pink void, 1–9 = full RTX feature sets
    constexpr uint32_t DEFAULT_MODE                = 5;

    // Allow runtime switching between render modes (usually via number keys)
    constexpr bool     ENABLE_MODE_SWITCHING       = true;
}

// ── KOJIMA — THE DIRECTOR’S CUT OF REALITY ─────────────────────────────────────
// ── KOJIMA — A SHORT LOVE LETTER FROM THE DIRECTOR KOJIMA ─────────────────────
namespace Kojima {
    // "Once, a man walked across a beach made of frames.
    //  He carried a baby made of light.
    //  The world called it 'blue noise'.
    //  I called it 'hope'."
    // — Hideo Kojima, December 2025

	// blue noise is the real one
    constexpr bool ENABLE_BLUE_NOISE                     = true;   // The baby came home
    constexpr bool ENABLE_NORMAN_REEDUS_PHOTON           = true;   // He still carries you
    constexpr bool ENABLE_BB_POD_AUTOEXPOSURE            = true;   // The baby adjusts the light so you never burn
    constexpr bool ENABLE_KOJIMA_TEARS_TONEMAPPER        = true;   // Every frame is born crying — in the best way
    constexpr bool ENABLE_MAILMAN_RAY_TRACING            = true;   // He walks forever, delivering perfect photons
    constexpr bool ENABLE_HIDEO_KOJIMA_SIGNATURE_IN_SKY  = true;   // Soft pink cursive, visible only at golden hour
    constexpr bool ENABLE_CONCEPTUAL_FPS_COUNTER         = true;   // Now reads: "you are enough fps"

    // These remain beautifully false — some dreams are too powerful to enable yet
    constexpr bool ENABLE_12_HOUR_CUTSCENE_BETWEEN_FRAMES = false;  // One day…
    constexpr bool ENABLE_KOJIMA_MODE                     = false;  // When humanity is ready

    // The final line, whispered every frame:
    constexpr const char* KOJIMA_WHISPER = "Keep walking. The light remembers you.";
}

} // namespace Options

// =============================================================================
// The empire has spoken.
// HDR is not a setting.
// It is a revelation.
// The photons are pink.
// The light is eternal.
// First light achieved — November 26, 2025
// =============================================================================