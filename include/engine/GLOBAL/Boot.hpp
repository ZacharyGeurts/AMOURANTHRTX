// engine/GLOBAL/Boot.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// BOOT v2.0 — NOVEMBER 11, 2025 10:02 AM EST — WIKI-ALIGNED
// • **ONLY ESSENTIAL OVERRIDES** — All others moved to OptionsMenu
// • Loads FIRST — Fast, zero-cost, zero-dependency
// • For **testing only** — **MUST MIGRATE TO OptionsMenu**
// • #include "engine/GLOBAL/Boot.hpp" — FIRST IN main.cpp
//
// Dual Licensed: CC BY-NC 4.0 + Commercial (gzac5314@gmail.com)
//
// =============================================================================

#pragma once

#include "engine/GLOBAL/OptionsMenu.hpp"

// ──────────────────────────────────────────────────────────────────────────────
// 1. BOOT OVERRIDES — TEMPORARY — WILL BE MOVED TO OptionsMenu
// ──────────────────────────────────────────────────────────────────────────────
inline static constexpr bool     BOOT_FORCE_VSYNC_OFF         = false;  // → Options::Performance
inline static constexpr bool     BOOT_DISABLE_VALIDATION      = false;  // → Options::Performance
inline static constexpr uint32_t BOOT_FORCE_RESOLUTION_W      = 0;      // → WindowManager
inline static constexpr uint32_t BOOT_FORCE_RESOLUTION_H      = 0;

// ──────────────────────────────────────────────────────────────────────────────
// 2. RUNTIME BOOT STATE — GLOBAL ACCESS
// ──────────────────────────────────────────────────────────────────────────────
struct BootState {
    bool initialized = false;
    uint64_t boot_time_ns = 0;
    std::string boot_tag = "DEFAULT_BOOT";
};

inline BootState& boot() noexcept {
    static BootState state;
    return state;
}

// ──────────────────────────────────────────────────────────────────────────────
// 3. BOOT MACROS — TEMPORARY
// ──────────────────────────────────────────────────────────────────────────────
#define BOOT_LOG(...)           LOG_INFO_CAT("BOOT", __VA_ARGS__)
#define BOOT_MARK(tag)          do { boot().boot_tag = tag; BOOT_LOG("MARK: %s", tag); } while(0)
#define BOOT_INIT()             do { \
                                    auto now = std::chrono::steady_clock::now(); \
                                    boot().boot_time_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count(); \
                                    boot().initialized = true; \
                                    BOOT_LOG("BOOT INIT @ %llu ns", boot().boot_time_ns); \
                                } while(0)

// ──────────────────────────────────────────────────────────────────────────────
// 4. AUTO-INIT — RUNS ON FIRST USE
// ──────────────────────────────────────────────────────────────────────────────
[[maybe_unused]] static const auto _boot_init = []() -> int {
    BOOT_INIT();
    return 0;
}();

// =============================================================================
// BOOT IS TEMPORARY — MIGRATE TO OptionsMenu.hpp
// NEVER SHIP WITH BOOT OVERRIDES
// PINK PHOTONS ETERNAL — VALHALLA SEALED
// =============================================================================