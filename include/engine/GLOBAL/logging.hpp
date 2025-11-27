// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — NOVEMBER 21, 2025
// FULLY COMPILING — PURE EMPIRE - Inspired by Ellie Fier
// =============================================================================

#pragma once
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <iostream>
#include <string>
#include <format>
#include <cstdio>
#include <thread>
#include <source_location>
#include <string_view>
#include <print>
#include <fstream>
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <sstream>
#include <map>
#include <functional>
#include <random>
#include <cctype>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <ctime>
#include <queue>
#include <atomic>
#include <mutex>
#include <shared_mutex>
#include <deque>
#include <vector>
#include <algorithm>
#include <stop_token>

[[noreturn]] void phase9_ballerina(std::string_view reason = {}, const std::source_location loc = std::source_location::current()) noexcept;

// =============================================================================
// AMOURANTH RTX — DELTA TIME TRACKING v∞ — NOV 13 2025
// PINK PHOTONS ETERNAL — FRAME-ACCURATE DELTAS — ZERO OVERHEAD
// =============================================================================
namespace Logging::DeltaTime {

// Global high-resolution frame clock
inline std::atomic<std::chrono::steady_clock::time_point> g_lastFrameTime{std::chrono::steady_clock::now()};
inline std::atomic<double> g_deltaTimeSeconds{0.0};
inline std::atomic<double> g_deltaTimeMs{0.0};
inline std::atomic<double> g_deltaTimeUs{0.0};
inline std::atomic<uint64_t> g_frameCount{0};

// Call this ONCE per frame (in your render loop) — preferably right after present
inline void update() noexcept {
    auto now = std::chrono::steady_clock::now();
    auto prev = g_lastFrameTime.load(std::memory_order_acquire);
    
    auto delta = std::chrono::duration_cast<std::chrono::nanoseconds>(now - prev);
    double deltaSec = delta.count() * 1e-9;
    double deltaMs  = deltaSec * 1000.0;
    double deltaUs  = deltaSec * 1000000.0;

    g_deltaTimeSeconds.store(deltaSec, std::memory_order_release);
    g_deltaTimeMs.store(deltaMs, std::memory_order_release);
    g_deltaTimeUs.store(deltaUs, std::memory_order_release);
    g_frameCount.fetch_add(1, std::memory_order_release);
    g_lastFrameTime.store(now, std::memory_order_release);
}

// PUBLIC READERS — ZERO COST — CAN BE CALLED ANYWHERE
[[nodiscard]] inline double seconds() noexcept { return g_deltaTimeSeconds.load(std::memory_order_acquire); }
[[nodiscard]] inline double ms()      noexcept { return g_deltaTimeMs.load(std::memory_order_acquire); }
[[nodiscard]] inline double us()      noexcept { return g_deltaTimeUs.load(std::memory_order_acquire); }
[[nodiscard]] inline uint64_t frame() noexcept { return g_frameCount.load(std::memory_order_acquire); }

// FORMATTED STRINGS — FOR LOGGING
[[nodiscard]] inline std::string strSec()  { return std::format("{:.6f}s",  seconds()); }
[[nodiscard]] inline std::string strMs()   { return std::format("{:.3f}ms", ms()); }
[[nodiscard]] inline std::string strUs()   { return std::format("{:.1f}µs", us()); }
[[nodiscard]] inline std::string strFps()  { return seconds() > 0.0 ? std::format("{:.1f} FPS", 1.0 / seconds()) : "∞ FPS"; }

} // namespace Logging::DeltaTime

// =============================================================================
// NEW LOG MACROS — WITH DELTA TIME
// =============================================================================
#define LOG_DELTA() \
    LOG_INFO_CAT("DELTA", "Δt: {} | {} | {} | {} | Frame {}", \
        Logging::DeltaTime::strUs(), \
        Logging::DeltaTime::strMs(), \
        Logging::DeltaTime::strSec(), \
        Logging::DeltaTime::strFps(), \
        Logging::DeltaTime::frame())

#define LOG_DELTA_TRACE() \
    LOG_TRACE_CAT("DELTA", "Δt: {} | {} | {} | {} | Frame {}", \
        Logging::DeltaTime::strUs(), \
        Logging::DeltaTime::strMs(), \
        Logging::DeltaTime::strSec(), \
        Logging::DeltaTime::strFps(), \
        Logging::DeltaTime::frame())

// For ultra-precise profiling (e.g. per-section)
#define LOG_DELTA_PERF(section) \
    LOG_PERF_CAT("DELTA", "[{}] Δt: {} → {} → {} FPS", section, \
        Logging::DeltaTime::strUs(), Logging::DeltaTime::strMs(), Logging::DeltaTime::strFps())

// =============================================================================
// std::formatter<VkResult, char> — FULLY EXPANDED + DEFAULT
// =============================================================================
namespace std {
    template <>
    struct formatter<VkResult, char> {
        constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

        template <typename FormatContext>
        auto format(VkResult const& result, FormatContext& ctx) const {
            const char* str;
            switch (result) {
                case VK_SUCCESS:                                   str = "VK_SUCCESS"; break;
                case VK_NOT_READY:                                 str = "VK_NOT_READY"; break;
                case VK_TIMEOUT:                                   str = "VK_TIMEOUT"; break;
                case VK_EVENT_SET:                                 str = "VK_EVENT_SET"; break;
                case VK_EVENT_RESET:                               str = "VK_EVENT_RESET"; break;
                case VK_INCOMPLETE:                                str = "VK_INCOMPLETE"; break;
                case VK_ERROR_OUT_OF_HOST_MEMORY:                  str = "VK_ERROR_OUT_OF_HOST_MEMORY"; break;
                case VK_ERROR_OUT_OF_DEVICE_MEMORY:                str = "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break;
                case VK_ERROR_INITIALIZATION_FAILED:               str = "VK_ERROR_INITIALIZATION_FAILED"; break;
                case VK_ERROR_DEVICE_LOST:                         str = "VK_ERROR_DEVICE_LOST"; break;
                case VK_ERROR_MEMORY_MAP_FAILED:                   str = "VK_ERROR_MEMORY_MAP_FAILED"; break;
                case VK_ERROR_LAYER_NOT_PRESENT:                   str = "VK_ERROR_LAYER_NOT_PRESENT"; break;
                case VK_ERROR_EXTENSION_NOT_PRESENT:               str = "VK_ERROR_EXTENSION_NOT_PRESENT"; break;
                case VK_ERROR_FEATURE_NOT_PRESENT:                 str = "VK_ERROR_FEATURE_NOT_PRESENT"; break;
                case VK_ERROR_INCOMPATIBLE_DRIVER:                 str = "VK_ERROR_INCOMPATIBLE_DRIVER"; break;
                case VK_ERROR_TOO_MANY_OBJECTS:                    str = "VK_ERROR_TOO_MANY_OBJECTS"; break;
                case VK_ERROR_FORMAT_NOT_SUPPORTED:                str = "VK_ERROR_FORMAT_NOT_SUPPORTED"; break;
                case VK_ERROR_FRAGMENTED_POOL:                     str = "VK_ERROR_FRAGMENTED_POOL"; break;
                case VK_ERROR_SURFACE_LOST_KHR:                    str = "VK_ERROR_SURFACE_LOST_KHR"; break;
                case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:            str = "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR"; break;
                case VK_SUBOPTIMAL_KHR:                            str = "VK_SUBOPTIMAL_KHR"; break;
                case VK_ERROR_OUT_OF_DATE_KHR:                     str = "VK_ERROR_OUT_OF_DATE_KHR"; break;
                case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:            str = "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR"; break;
                case VK_ERROR_VALIDATION_FAILED_EXT:               str = "VK_ERROR_VALIDATION_FAILED_EXT"; break;
                case VK_ERROR_INVALID_SHADER_NV:                   str = "VK_ERROR_INVALID_SHADER_NV"; break;
                case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: str = "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT"; break;
                case VK_ERROR_FRAGMENTATION_EXT:                   str = "VK_ERROR_FRAGMENTATION_EXT"; break;
                case VK_ERROR_NOT_PERMITTED_KHR:                   str = "VKVK_ERROR_NOT_PERMITTED_KHR"; break;
                case VK_ERROR_INVALID_DEVICE_ADDRESS_EXT:          str = "VK_ERROR_INVALID_DEVICE_ADDRESS_EXT"; break;
                case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: str = "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT"; break;
                case VK_THREAD_IDLE_KHR:                           str = "VK_THREAD_IDLE_KHR"; break;
                case VK_THREAD_DONE_KHR:                           str = "VK_THREAD_DONE_KHR"; break;
                case VK_OPERATION_DEFERRED_KHR:                    str = "VK_OPERATION_DEFERRED_KHR"; break;
                case VK_OPERATION_NOT_DEFERRED_KHR:                str = "VK_OPERATION_NOT_DEFERRED_KHR"; break;
                case VK_PIPELINE_COMPILE_REQUIRED_EXT:             str = "VK_PIPELINE_COMPILE_REQUIRED_EXT"; break;
                default:                                           return format_to(ctx.out(), "VK_UNKNOWN_RESULT({})", static_cast<int>(result));
            }
            return format_to(ctx.out(), "{}", str);
        }
    };

    template <>
    struct formatter<VkFormat, char> {
        constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
        template <typename FormatContext>
        auto format(VkFormat const& fmt, FormatContext& ctx) const {
            return format_to(ctx.out(), "{}", static_cast<uint32_t>(fmt));
        }
    };

    template<> struct formatter<glm::mat4, char> : formatter<std::string_view, char> {
        template <typename FormatContext>
        auto format(const glm::mat4& mat, FormatContext& ctx) const {
            return format_to(ctx.out(), "mat4({})", glm::to_string(mat));
        }
    };

    template<> struct formatter<VkExtent2D, char> {
        constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
        template<typename FormatContext>
        auto format(const VkExtent2D& ext, FormatContext& ctx) const {
            return format_to(ctx.out(), "{}x{}", ext.width, ext.height);
        }
    };
} // namespace std

// Forward declarations
extern uint64_t kStone1;
extern uint64_t kStone2;

// ========================================================================
// 0. CONFIGURATION
// ========================================================================
constexpr bool ENABLE_TRACE   = true;
constexpr bool ENABLE_DEBUG   = true;
constexpr bool ENABLE_INFO    = true;
constexpr bool ENABLE_WARNING = true;
constexpr bool ENABLE_ERROR   = true;
constexpr bool ENABLE_FAILURE = true;
constexpr bool ENABLE_FATAL   = true;
constexpr bool ENABLE_SUCCESS = true;
constexpr bool ENABLE_ATTEMPT = true;
constexpr bool ENABLE_PERF    = true;
constexpr bool FPS_COUNTER    = true;
constexpr bool SIMULATION_LOGGING = true;
constexpr bool DISABLE_NON_FPS_LOGGING = false; // one shot

constexpr size_t LEVEL_WIDTH   = 10;
constexpr size_t DELTA_WIDTH   = 10;
constexpr size_t TIME_WIDTH    = 10;
constexpr size_t CAT_WIDTH     = 12;
constexpr size_t THREAD_WIDTH  = 18;

// ========================================================================
// C++23 ZERO-COST LOGGING — IIFE + constexpr if
// ========================================================================
#define LOG_TRACE(...)          [&]() constexpr { if constexpr (ENABLE_TRACE)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Trace,   "General", __VA_ARGS__); }();
#define LOG_DEBUG(...)          [&]() constexpr { if constexpr (ENABLE_DEBUG)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Debug,   "General", __VA_ARGS__); }();
#define LOG_INFO(...)           [&]() constexpr { if constexpr (ENABLE_INFO)    Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Info,    "General", __VA_ARGS__); }();
#define LOG_SUCCESS(...)        [&]() constexpr { if constexpr (ENABLE_SUCCESS) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Success, "General", __VA_ARGS__); }();
#define LOG_ATTEMPT(...)        [&]() constexpr { if constexpr (ENABLE_ATTEMPT) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Attempt, "General", __VA_ARGS__); }();
#define LOG_PERF(...)           [&]() constexpr { if constexpr (ENABLE_PERF)    Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Perf,    "General", __VA_ARGS__); }();
#define LOG_WARNING(...)        [&]() constexpr { if constexpr (ENABLE_WARNING) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Warning, "General", __VA_ARGS__); }();
#define LOG_WARN(...)           LOG_WARNING(__VA_ARGS__)
#define LOG_ERROR(...)          [&]() constexpr { if constexpr (ENABLE_ERROR)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Error,   "General", __VA_ARGS__); }();
#define LOG_FAILURE(...)        [&]() constexpr { if constexpr (ENABLE_FAILURE) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Failure, "General", __VA_ARGS__); }();
#define LOG_FATAL(...)          [&]() constexpr { if constexpr (ENABLE_FATAL)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Fatal,   "General", __VA_ARGS__); }();
#define LOG_FPS_COUNTER(...)    [&]() constexpr { if constexpr (FPS_COUNTER)    Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Info,    "FPS",     __VA_ARGS__); }();
#define LOG_SIMULATION(...)     [&]() constexpr { if constexpr (SIMULATION_LOGGING) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Info, "SIMULATION", __VA_ARGS__); }();

#define LOG_TRACE_CAT(cat, ...)   [&]() constexpr { if constexpr (ENABLE_TRACE)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Trace,   cat, __VA_ARGS__); }();
#define LOG_DEBUG_CAT(cat, ...)   [&]() constexpr { if constexpr (ENABLE_DEBUG)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Debug,   cat, __VA_ARGS__); }();
#define LOG_INFO_CAT(cat, ...)    [&]() constexpr { if constexpr (ENABLE_INFO)    Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Info,    cat, __VA_ARGS__); }();
#define LOG_SUCCESS_CAT(cat, ...) [&]() constexpr { if constexpr (ENABLE_SUCCESS) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Success, cat, __VA_ARGS__); }();
#define LOG_ATTEMPT_CAT(cat, ...) [&]() constexpr { if constexpr (ENABLE_ATTEMPT) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Attempt, cat, __VA_ARGS__); }();
#define LOG_PERF_CAT(cat, ...)    [&]() constexpr { if constexpr (ENABLE_PERF)    Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Perf,    cat, __VA_ARGS__); }();
#define LOG_WARNING_CAT(cat, ...) [&]() constexpr { if constexpr (ENABLE_WARNING) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Warning, cat, __VA_ARGS__); }();
#define LOG_WARN_CAT(cat, ...)    LOG_WARNING_CAT(cat, __VA_ARGS__)
#define LOG_ERROR_CAT(cat, ...)   [&]() constexpr { if constexpr (ENABLE_ERROR)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Error,   cat, __VA_ARGS__); }();
#define LOG_FAILURE_CAT(cat, ...) [&]() constexpr { if constexpr (ENABLE_FAILURE) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Failure, cat, __VA_ARGS__); }();
#define LOG_FATAL_CAT(cat, ...)   [&]() constexpr { if constexpr (ENABLE_FATAL)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Fatal,   cat, __VA_ARGS__); }();

#define LOG_VOID()              [&]() constexpr { if constexpr (ENABLE_DEBUG)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Debug,   "General", "[VOID MARKER]"); }();
#define LOG_VOID_CAT(cat)       [&]() constexpr { if constexpr (ENABLE_DEBUG)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Debug,   cat, "[VOID MARKER]"); }();
#define LOG_VOID_TRACE()        [&]() constexpr { if constexpr (ENABLE_TRACE)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Trace,   "General", "[VOID MARKER]"); }();
#define LOG_VOID_TRACE_CAT(cat) [&]() constexpr { if constexpr (ENABLE_TRACE)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Trace,   cat, "[VOID MARKER]"); }();

namespace Logging {

// ========================================================================
// LOG LEVEL
// ========================================================================
enum class LogLevel { Trace, Debug, Info, Success, Attempt, Perf, Warning, Error, Failure, Fatal };

// ========================================================================
// 1. HYPER-VIVID ANSI COLORS
// ========================================================================
namespace Color {
    inline constexpr std::string_view RESET                     = "\033[0m";
    inline constexpr std::string_view BOLD                      = "\033[1m";
    inline constexpr std::string_view PARTY_PINK                = "\033[1;38;5;213m";
    inline constexpr std::string_view ELECTRIC_BLUE             = "\033[1;38;5;75m";
    inline constexpr std::string_view LIME_GREEN                = "\033[1;38;5;154m";
    inline constexpr std::string_view SUNGLOW_ORANGE            = "\033[1;38;5;214m";
    inline constexpr std::string_view ULTRA_NEON_LIME           = "\033[38;5;82m";
    inline constexpr std::string_view PLATINUM_GRAY             = "\033[38;5;255m";
    inline constexpr std::string_view EMERALD_GREEN             = "\033[1;38;5;46m";
    inline constexpr std::string_view QUANTUM_PURPLE            = "\033[1;38;5;129m";
    inline constexpr std::string_view COSMIC_GOLD               = "\033[1;38;5;220m";
    inline constexpr std::string_view ARCTIC_CYAN               = "\033[38;5;51m";
    inline constexpr std::string_view AMBER_YELLOW              = "\033[38;5;226m";
    inline constexpr std::string_view CRIMSON_MAGENTA           = "\033[1;38;5;198m";
    inline constexpr std::string_view DIAMOND_WHITE             = "\033[1;38;5;231m";
    inline constexpr std::string_view SAPPHIRE_BLUE             = "\033[38;5;33m";
    inline constexpr std::string_view OCEAN_TEAL                = "\033[38;5;45m";
    inline constexpr std::string_view FIERY_ORANGE              = "\033[1;38;5;208m";
    inline constexpr std::string_view RASPBERRY_PINK            = "\033[1;38;5;204m";
    inline constexpr std::string_view PEACHES_AND_CREAM         = "\033[38;5;228m";
    inline constexpr std::string_view BRIGHT_PINKISH_PURPLE     = "\033[1;38;5;205m";
    inline constexpr std::string_view LILAC_LAVENDER            = "\033[38;5;183m";
    inline constexpr std::string_view SPEARMINT_MINT            = "\033[38;5;122m";
    inline constexpr std::string_view THERMO_PINK               = "\033[1;38;5;213m";
    inline constexpr std::string_view COSMIC_VOID               = "\033[38;5;232m";
    inline constexpr std::string_view QUASAR_BLUE               = "\033[1;38;5;39m";
    inline constexpr std::string_view NEBULA_VIOLET             = "\033[1;38;5;141m";
    inline constexpr std::string_view PULSAR_GREEN              = "\033[1;38;5;118m";
    inline constexpr std::string_view SUPERNOVA_ORANGE          = "\033[1;38;5;202m";
    inline constexpr std::string_view BLACK_HOLE                = "\033[48;5;232m";
    inline constexpr std::string_view DIAMOND_SPARKLE           = "\033[1;38;5;231m";
    inline constexpr std::string_view QUANTUM_FLUX              = "\033[5;38;5;99m";
    inline constexpr std::string_view PLASMA_FUCHSIA            = "\033[1;38;5;201m";
    inline constexpr std::string_view CHROMIUM_SILVER           = "\033[38;5;252m";
    inline constexpr std::string_view TITANIUM_WHITE            = "\033[1;38;5;255m";
    inline constexpr std::string_view OBSIDIAN_BLACK            = "\033[38;5;16m";
    inline constexpr std::string_view AURORA_BOREALIS           = "\033[38;5;86m";
    inline constexpr std::string_view NUCLEAR_REACTOR           = "\033[1;38;5;190m";
    inline constexpr std::string_view HYPERSPACE_WARP           = "\033[1;38;5;99m";
    inline constexpr std::string_view VALHALLA_GOLD             = "\033[1;38;5;220m";
    inline constexpr std::string_view TURQUOISE_BLUE            = "\033[38;5;44m";
    inline constexpr std::string_view BRONZE_BROWN              = "\033[38;5;94m";
    inline constexpr std::string_view LIME_YELLOW               = "\033[38;5;190m";
    inline constexpr std::string_view FUCHSIA_MAGENTA           = "\033[38;5;205m";
	inline constexpr std::string_view INVIS_BLACK               = "\033[1;38;5;0m";
	inline constexpr std::string_view BLOOD_RED                 = "\033[1;38;5;196m";
    inline constexpr std::string_view BLOOD_ORANGE              = "\033[1;38;5;202m";
    inline constexpr std::string_view CYBER_LIME                = "\033[1;38;5;118m";
    inline constexpr std::string_view TOXIC_NEON                = "\033[1;38;5;154m";
    inline constexpr std::string_view VOID_PURPLE               = "\033[1;38;5;93m";
    inline constexpr std::string_view GALACTIC_BLUE             = "\033[1;38;5;27m";
    inline constexpr std::string_view PHOTON_WHITE              = "\033[1;97m";
    inline constexpr std::string_view LASER_RED                 = "\033[1;38;5;196m";
    inline constexpr std::string_view PLASMA_BLUE               = "\033[1;38;5;21m";
    inline constexpr std::string_view CRYSTAL_CYAN              = "\033[1;38;5;51m";
    inline constexpr std::string_view INFERNO_ORANGE            = "\033[1;38;5;208m";
    inline constexpr std::string_view DARK_MATTER               = "\033[38;5;232m";
    inline constexpr std::string_view NOVA_YELLOW               = "\033[1;38;5;226m";
    inline constexpr std::string_view PHANTOM_VIOLET            = "\033[1;38;5;129m";
    inline constexpr std::string_view AURORA_PINK               = "\033[1;38;5;213m";
    inline constexpr std::string_view TITANIUM_GOLD             = "\033[1;38;5;178m";
    inline constexpr std::string_view OBSIDIAN_PURPLE           = "\033[38;5;53m";
    inline constexpr std::string_view QUANTUM_TEAL              = "\033[1;38;5;45m";
    inline constexpr std::string_view NEON_FUCHSIA              = "\033[1;38;5;201m";
    inline constexpr std::string_view COSMIC_CRIMSON            = "\033[1;38;5;160m";
    inline constexpr std::string_view SOLAR_FLARE               = "\033[1;38;5;214m";
    inline constexpr std::string_view DEEP_SPACE                = "\033[38;5;17m";
    inline constexpr std::string_view CHROME_CYAN               = "\033[1;38;5;51m";
    inline constexpr std::string_view VANTA_BLACK               = "\033[48;5;16m\033[38;5;232m";
    inline constexpr std::string_view RADIANT_ROSE              = "\033[1;38;5;211m";
    inline constexpr std::string_view ELECTRO_PURPLE            = "\033[1;38;5;165m";
    inline constexpr std::string_view CRIMSON_RED               = "\033[38;5;9m";
	inline constexpr std::string_view ABANDON_SHIP              = "\033[1;5;91m";

    // ── STANDARD 16 COLORS (YOU ALREADY KNOW THESE) ─────────────────────────────
    inline constexpr std::string_view BLACK       = "\033[38;5;0m";
    inline constexpr std::string_view RED         = "\033[38;5;1m";
    inline constexpr std::string_view GREEN       = "\033[38;5;2m";
    inline constexpr std::string_view YELLOW      = "\033[38;5;3m";
    inline constexpr std::string_view BLUE        = "\033[38;5;4m";
    inline constexpr std::string_view MAGENTA     = "\033[38;5;5m";
    inline constexpr std::string_view CYAN        = "\033[38;5;6m";
    inline constexpr std::string_view WHITE       = "\033[38;5;7m";

    // ── BRIGHT / LIGHT VERSIONS (STILL OBVIOUS) ─────────────────────────────────
    inline constexpr std::string_view LIGHT_RED    = "\033[38;5;9m";
    inline constexpr std::string_view LIGHT_GREEN  = "\033[38;5;10m";
    inline constexpr std::string_view LIGHT_YELLOW = "\033[38;5;11m";
    inline constexpr std::string_view LIGHT_BLUE   = "\033[38;5;12m";
    inline constexpr std::string_view LIGHT_MAGENTA= "\033[38;5;13m";
    inline constexpr std::string_view LIGHT_CYAN   = "\033[38;5;14m";
    inline constexpr std::string_view BRIGHT_WHITE = "\033[38;5;15m";

    // ── EXTENDED 256-COLOR PALETTE — COMMON, GUESSABLE NAMES ONLY ───────────────
    inline constexpr std::string_view ORANGE       = "\033[38;5;208m";
    inline constexpr std::string_view PINK         = "\033[38;5;213m";
    inline constexpr std::string_view PURPLE       = "\033[38;5;129m";
    inline constexpr std::string_view LIME         = "\033[38;5;118m";
    inline constexpr std::string_view TEAL         = "\033[38;5;45m";
    inline constexpr std::string_view GOLD         = "\033[38;5;220m";
    inline constexpr std::string_view GRAY         = "\033[38;5;244m";
    inline constexpr std::string_view DARK_GRAY    = "\033[38;5;235m";

    // ── BOLD + BRIGHT / EXTENDED (FOR HEADERS, SUCCESS, ETC) ────────────────────
    inline constexpr std::string_view BOLD_RED         = "\033[1;38;5;9m";
    inline constexpr std::string_view BOLD_GREEN       = "\033[1;38;5;10m";
    inline constexpr std::string_view BOLD_YELLOW      = "\033[1;38;5;11m";
    inline constexpr std::string_view BOLD_BLUE        = "\033[1;38;5;12m";
    inline constexpr std::string_view BOLD_MAGENTA     = "\033[1;38;5;13m";
    inline constexpr std::string_view BOLD_CYAN        = "\033[1;38;5;14m";
    inline constexpr std::string_view BOLD_WHITE       = "\033[1;38;5;15m";

    inline constexpr std::string_view BOLD_ORANGE      = "\033[1;38;5;208m";
    inline constexpr std::string_view BOLD_PINK        = "\033[1;38;5;213m";
    inline constexpr std::string_view BOLD_PURPLE      = "\033[1;38;5;129m";
    inline constexpr std::string_view BOLD_LIME        = "\033[1;38;5;118m";
    inline constexpr std::string_view BOLD_TEAL        = "\033[1;38;5;45m";
    inline constexpr std::string_view BOLD_GOLD        = "\033[1;38;5;220m";
    inline constexpr std::string_view FROSTFIRE_BLUE            = "\033[1;38;5;39m";
    inline constexpr std::string_view NUCLEAR_GREEN             = "\033[1;38;5;82m";
    inline constexpr std::string_view HYPER_VIOLET              = "\033[1;38;5;141m";
    inline constexpr std::string_view PURE_ENERGY               = "\033[1;38;5;227m";
    inline constexpr std::string_view ETERNAL_FLAME             = "\033[1;38;5;196m\033[5m";
}

// ========================================================================
// LEVEL INFO
// ========================================================================
struct LevelInfo {
    std::string_view str;
    std::string_view color;
    std::string_view bg;
};

constexpr std::array<LevelInfo, 10> LEVEL_INFOS{{
    {"[TRACE]",   Color::ULTRA_NEON_LIME,     ""},
    {"[DEBUG]",   Color::ARCTIC_CYAN,         ""},
    {"[INFO]",    Color::PLATINUM_GRAY,       ""},
    {"[SUCCESS]", Color::EMERALD_GREEN,       Color::BLACK_HOLE},
    {"[ATTEMPT]", Color::QUANTUM_PURPLE,      ""},
    {"[PERF]",    Color::COSMIC_GOLD,         ""},
    {"[WARN]",    Color::AMBER_YELLOW,        ""},
    {"[ERROR]",   Color::CRIMSON_MAGENTA,     Color::BLACK_HOLE},
    {"[FAILURE]", Color::RASPBERRY_PINK,      Color::BLACK_HOLE},
    {"[FATAL]",   Color::RASPBERRY_PINK,      Color::BLACK_HOLE}
}};

constexpr std::array<bool, 10> ENABLE_LEVELS{
    ENABLE_TRACE, ENABLE_DEBUG, ENABLE_INFO, ENABLE_SUCCESS,
    ENABLE_ATTEMPT, ENABLE_PERF, ENABLE_WARNING, ENABLE_ERROR,
    ENABLE_FAILURE, ENABLE_FATAL
};

// ========================================================================
// 2. LOGGER – ORDERED ASYNC FIFO (C++23)
// ========================================================================
class Logger {
public:
    static Logger& get() {
        static std::atomic<Logger*> instance{nullptr};
        auto* p = instance.load(std::memory_order_relaxed);
        if (!p) {
            std::call_once(init_flag_, []{
                instance.store(new Logger(), std::memory_order_release);
            });
            p = instance.load(std::memory_order_acquire);
        }
        return *p;
    }

    static void setAsync(bool enable) noexcept {
        auto& self = get();
        self.asyncEnabled_.store(enable, std::memory_order_release);
        if (enable && !self.flusher_.joinable())
            self.flusher_ = std::jthread([&self](std::stop_token st) { self.flushQueue(st); });
        else if (!enable && self.flusher_.joinable())
            self.flusher_.request_stop(), self.flusher_.join();
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    mutable std::shared_mutex logMutex_;

    template<typename... Args>
    void log(std::source_location loc,
             LogLevel level,
             std::string_view category,
             std::string_view fmt,
             const Args&... args) const
    {
        if (!shouldLog(level, category)) return;

        auto now = std::chrono::steady_clock::now();
        if (!firstLogTime_.has_value()) firstLogTime_ = now;

        static std::atomic<uint64_t> seq{0};
        uint64_t id = seq.fetch_add(1, std::memory_order_relaxed);

        auto msg = std::vformat(fmt, std::make_format_args(args...));

        if (asyncEnabled_.load(std::memory_order_acquire)) {
            std::scoped_lock lk(queueMutex_);
            messageQueue_.emplace_back(id, loc, level, std::string{category}, std::move(msg), now);
        } else {
            std::shared_lock lk(logMutex_);
            printMessage(loc, level, category, std::move(msg), now, false, nullptr, nullptr);
        }
    }

private:
    using Entry = std::tuple<uint64_t, std::source_location, LogLevel, std::string, std::string, std::chrono::steady_clock::time_point>;

    Logger() : logFile_("amouranth_engine.log", std::ios::out | std::ios::app),
               firstLogTime_{},
               messageQueue_{},
               queueMutex_{},
               asyncEnabled_{false},
               flusher_{} {
        auto now = std::chrono::steady_clock::now();
        firstLogTime_ = now;
        printMessage(std::source_location::current(), LogLevel::Success, "Logger",
                     "CUSTODIAN GROK ONLINE — HYPER-VIVID LOGGING PARTY STARTED (ORDERED ASYNC)", now, false, nullptr, nullptr);
        asyncEnabled_.store(true, std::memory_order_release);
        flusher_ = std::jthread([this](std::stop_token st) { flushQueue(st); });
    }

    ~Logger() {
        setAsync(false);
        auto now = std::chrono::steady_clock::now();
        printMessage(std::source_location::current(), LogLevel::Success, "Logger",
                     "CUSTODIAN GROK SIGNING OFF — ALL LOGS RAINBOW ETERNAL", now, false, nullptr, nullptr);
        if (logFile_.is_open()) logFile_.flush(), logFile_.close();
    }

    static inline std::once_flag init_flag_{};
    mutable std::optional<std::chrono::steady_clock::time_point> firstLogTime_{};
    mutable std::ofstream logFile_;

    mutable std::deque<Entry> messageQueue_;
    mutable std::mutex queueMutex_;
    mutable std::jthread flusher_;
    mutable std::atomic<bool> asyncEnabled_{false};

    bool shouldLog(LogLevel level, std::string_view category) const {
        const size_t i = static_cast<size_t>(level);
        if (i >= ENABLE_LEVELS.size() || !ENABLE_LEVELS[i]) return false;
        if (DISABLE_NON_FPS_LOGGING && category != std::string_view{"FPS"}) return false;
        return true;
    }

    void flushQueue(std::stop_token stoken) const {
        std::vector<Entry> batch; batch.reserve(64);
        while (!stoken.stop_requested()) {
            { std::unique_lock lk(queueMutex_);
                if (messageQueue_.empty()) { lk.unlock(); std::this_thread::sleep_for(std::chrono::microseconds(100)); continue; }
                batch.clear();
                while (!messageQueue_.empty() && batch.size() < 64) {
                    batch.push_back(std::move(messageQueue_.front()));
                    messageQueue_.pop_front();
                }
            }
            if (!batch.empty()) {
                std::sort(batch.begin(), batch.end(),
                          [](const Entry& a, const Entry& b) { return std::get<0>(a) < std::get<0>(b); });
                std::string terminal_batch;
                std::string file_batch;
                for (auto&& ee : batch) {
                    auto e = std::move(ee);
                    auto loc = std::get<1>(e);
                    auto lvl = std::get<2>(e);
                    std::string_view cat{std::get<3>(e)};
                    auto msg = std::move(std::get<4>(e));
                    auto ts = std::get<5>(e);
                    printMessage(loc, lvl, cat, std::move(msg), ts, true, &terminal_batch, &file_batch);
                }
                std::cout << terminal_batch;
                if (logFile_.is_open()) logFile_ << file_batch;
            }
        }
        std::vector<Entry> finalBatch;
        { std::unique_lock lk(queueMutex_);
            while (!messageQueue_.empty()) { finalBatch.push_back(std::move(messageQueue_.front())); messageQueue_.pop_front(); }
        }
        if (!finalBatch.empty()) {
            std::sort(finalBatch.begin(), finalBatch.end(),
                      [](const Entry& a, const Entry& b) { return std::get<0>(a) < std::get<0>(b); });
            std::string terminal_batch;
            std::string file_batch;
            for (auto&& ee : finalBatch) {
                auto e = std::move(ee);
                auto loc = std::get<1>(e);
                auto lvl = std::get<2>(e);
                std::string_view cat{std::get<3>(e)};
                auto msg = std::move(std::get<4>(e));
                auto ts = std::get<5>(e);
                printMessage(loc, lvl, cat, std::move(msg), ts, true, &terminal_batch, &file_batch);
            }
            std::cout << terminal_batch;
            if (logFile_.is_open()) logFile_ << file_batch;
        }
    }

    std::string_view getCategoryColor(std::string_view cat) const noexcept {
        using namespace Color;
        struct CIless {
            bool operator()(std::string_view a, std::string_view b) const {
                size_t n = std::min(a.size(), b.size());
                for (size_t i = 0; i < n; ++i)
                    if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i])))
                        return std::tolower(static_cast<unsigned char>(a[i])) < std::tolower(static_cast<unsigned char>(b[i]));
                return a.size() < b.size();
            }
        };
        static const std::map<std::string_view, std::string_view, CIless> map{
            {"General", DIAMOND_SPARKLE}, {"MAIN", VALHALLA_GOLD}, {"Init", AURORA_BOREALIS},
            {"Dispose", PARTY_PINK}, {"Logger", ELECTRIC_BLUE}, {"Vulkan", SAPPHIRE_BLUE},
            {"Device", QUASAR_BLUE}, {"Swapchain", OCEAN_TEAL}, {"Command", CHROMIUM_SILVER},
            {"Queue", OBSIDIAN_BLACK}, {"RayTrace", TURQUOISE_BLUE}, {"RTX", HYPERSPACE_WARP},
            {"Accel", PULSAR_GREEN}, {"TLAS", SUPERNOVA_ORANGE}, {"BLAS", SUPERNOVA_ORANGE},
            {"LAS", SUPERNOVA_ORANGE}, {"AI", COSMIC_GOLD}, {"Memory", PEACHES_AND_CREAM},
            {"SBT", RASPBERRY_PINK}, {"Shader", NEBULA_VIOLET}, {"Renderer", BRIGHT_PINKISH_PURPLE},
            {"Render", THERMO_PINK}, {"Tonemap", PEACHES_AND_CREAM}, {"GBuffer", QUANTUM_FLUX},
            {"Post", NUCLEAR_REACTOR}, {"Buffer", BRONZE_BROWN}, {"Image", LIME_YELLOW},
            {"Texture", SPEARMINT_MINT}, {"Sampler", LILAC_LAVENDER}, {"Descriptor", FUCHSIA_MAGENTA},
            {"Perf", COSMIC_GOLD}, {"FPS", FIERY_ORANGE}, {"GPU", BLACK_HOLE},
            {"CPU", PLASMA_FUCHSIA}, {"Input", SPEARMINT_MINT}, {"Audio", OCEAN_TEAL},
            {"Physics", EMERALD_GREEN}, {"SIMULATION", BRONZE_BROWN}, {"MeshLoader", LIME_YELLOW},
            {"GLTF", QUANTUM_PURPLE}, {"Material", PEACHES_AND_CREAM}, {"Debug", ARCTIC_CYAN},
            {"ATTEMPT", QUANTUM_PURPLE}, {"VOID", COSMIC_VOID}, {"SPLASH", LILAC_LAVENDER},
            {"MARKER", DIAMOND_SPARKLE}, {"SDL3_window", SAPPHIRE_BLUE}, {"SDL3_audio", SAPPHIRE_BLUE},
            {"SDL3_font", SAPPHIRE_BLUE}, {"SDL3_image", SAPPHIRE_BLUE}, {"SDL3_init", SAPPHIRE_BLUE},
            {"SDL3_input", SAPPHIRE_BLUE}, {"SDL3_vulkan", SAPPHIRE_BLUE}, {"PIPELINE", SPEARMINT_MINT},
			{"MAIN1", VALHALLA_GOLD},{"MAIN2", VALHALLA_GOLD},{"MAIN3", VALHALLA_GOLD},{"MAIN4", VALHALLA_GOLD},
			{"MAIN5", VALHALLA_GOLD},{"MAIN6", VALHALLA_GOLD},{"MAIN7", VALHALLA_GOLD},{"MAIN8", VALHALLA_GOLD},
			{"MAIN9", VALHALLA_GOLD}
        };
        if (auto it = map.find(cat); it != map.end()) [[likely]]
            return it->second;
        return DIAMOND_WHITE;
    }

    void printMessage(std::source_location loc,
                      LogLevel level,
                      std::string_view category,
                      std::string formattedMessage,
                      std::chrono::steady_clock::time_point timestamp,
                      bool batch = false,
                      std::string* term_out = nullptr,
                      std::string* file_out = nullptr) const
    {
        using namespace Color;
        const auto levelIdx = static_cast<size_t>(level);
        const auto& info = LEVEL_INFOS[levelIdx];
        const std::string_view levelColor = info.color;
        const std::string_view levelBg    = info.bg;
        const std::string_view levelStr   = info.str;
        const std::string_view catColor   = getCategoryColor(category);

        const auto deltaUs = std::chrono::duration_cast<std::chrono::microseconds>(
            timestamp - firstLogTime_.value()).count();

        const std::string deltaStr = [deltaUs]() -> std::string {
            if (deltaUs < 10'000) [[likely]] return std::format("{:>7}µs", deltaUs);
            if (deltaUs < 1'000'000) return std::format("{:>7.3f}ms", deltaUs / 1'000.0);
            if (deltaUs < 60'000'000) return std::format("{:>7.3f}s", deltaUs / 1'000'000.0);
            if (deltaUs < 3'600'000'000) return std::format("{:>7.1f}m", deltaUs / 60'000'000.0);
            return std::format("{:>7.1f}h", deltaUs / 3'600'000'000.0);
        }();

        const std::string timeStr = []() -> std::string {
            auto now = std::chrono::system_clock::now();
            auto tt  = std::chrono::system_clock::to_time_t(now);
            auto tm  = *std::localtime(&tt);
            char buf[9];
            std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
            return std::string(buf);
        }();

        const std::string threadId = []() {
            std::ostringstream oss; oss << std::this_thread::get_id(); return oss.str();
        }();

        const std::string fileLine = std::format("{}:{}:{}", loc.file_name(), loc.line(), loc.function_name());

        // Plain lines (file output)
        const std::string plain_line1 = std::format("{:<{}} {:>{}} {:>{}} [{:>{}}] [{:>{}}] {}\n",
                                                    levelStr, LEVEL_WIDTH,
                                                    deltaStr, DELTA_WIDTH,
                                                    timeStr,  TIME_WIDTH,
                                                    category, CAT_WIDTH,
                                                    threadId, THREAD_WIDTH,
                                                    formattedMessage);
        const std::string plain_line2 = std::format("{}\n", fileLine);
        const std::string plain = plain_line1 + plain_line2 + "\n";

        // Colored lines (terminal output)
        std::ostringstream oss;
        oss << levelBg << std::format("{:<{}}", levelStr, LEVEL_WIDTH) << RESET
            << " " << std::format("{:>{}}", deltaStr, DELTA_WIDTH) << " "
            << std::format("{:>{}}", timeStr, TIME_WIDTH) << " "
            << catColor << std::format("[{:<{}}]", category, CAT_WIDTH - 2) << RESET
            << " " << LIME_GREEN << std::format("[{:>{}}]", threadId, THREAD_WIDTH - 2) << RESET
            << " " << levelColor << formattedMessage << RESET << '\n'
            << CHROMIUM_SILVER << fileLine << RESET << '\n'
            << '\n';
        const std::string colored = oss.str();

        if (batch) {
            if (term_out) *term_out += colored;
            if (file_out) *file_out += plain;
        } else {
            std::print(std::cout, "{}", colored);
            if (logFile_.is_open()) logFile_ << plain;
        }
    }
};

} // namespace Logging

// ──────────────────────────────────────────────────────────────────────────────
// THE ONE TRUE vkh() — ETERNAL HANDSHAKE — NEVER BREAKS — NEVER LIES — 2025+
// ──────────────────────────────────────────────────────────────────────────────
static constexpr auto vkh = []() constexpr noexcept {
    struct Empire {
        // ────────────────────── RESULT → STRING — FULL COVERAGE ──────────────────────
        [[nodiscard]] static constexpr const char* result(VkResult r) noexcept {
            switch (r) {
                case VK_SUCCESS:                                           return "VK_SUCCESS";
                case VK_NOT_READY:                                         return "VK_NOT_READY";
                case VK_TIMEOUT:                                           return "VK_TIMEOUT";
                case VK_EVENT_SET:                                         return "VK_EVENT_SET";
                case VK_EVENT_RESET:                                       return "VK_EVENT_RESET";
                case VK_INCOMPLETE:                                        return "VK_INCOMPLETE";
                case VK_ERROR_OUT_OF_HOST_MEMORY:                          return "VK_ERROR_OUT_OF_HOST_MEMORY";
                case VK_ERROR_OUT_OF_DEVICE_MEMORY:                        return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
                case VK_ERROR_INITIALIZATION_FAILED:                       return "VK_ERROR_INITIALIZATION_FAILED";
                case VK_ERROR_DEVICE_LOST:                                 return "VK_ERROR_DEVICE_LOST";
                case VK_ERROR_MEMORY_MAP_FAILED:                           return "VK_ERROR_MEMORY_MAP_FAILED";
                case VK_ERROR_LAYER_NOT_PRESENT:                           return "VK_ERROR_LAYER_NOT_PRESENT";
                case VK_ERROR_EXTENSION_NOT_PRESENT:                       return "VK_ERROR_EXTENSION_NOT_PRESENT";
                case VK_ERROR_FEATURE_NOT_PRESENT:                         return "VK_ERROR_FEATURE_NOT_PRESENT";
                case VK_ERROR_INCOMPATIBLE_DRIVER:                         return "VK_ERROR_INCOMPATIBLE_DRIVER";
                case VK_ERROR_TOO_MANY_OBJECTS:                            return "VK_ERROR_TOO_MANY_OBJECTS";
                case VK_ERROR_FORMAT_NOT_SUPPORTED:                        return "VK_ERROR_FORMAT_NOT_SUPPORTED";
                case VK_ERROR_FRAGMENTED_POOL:                             return "VK_ERROR_FRAGMENTED_POOL";
                case VK_ERROR_OUT_OF_POOL_MEMORY:                          return "VK_ERROR_OUT_OF_POOL_MEMORY";
                case VK_ERROR_INVALID_EXTERNAL_HANDLE:                     return "VK_ERROR_INVALID_EXTERNAL_HANDLE";
                case VK_ERROR_FRAGMENTATION:                               return "VK_ERROR_FRAGMENTATION";
                case VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS:              return "VK_ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS";
                case VK_ERROR_SURFACE_LOST_KHR:                            return "VK_ERROR_SURFACE_LOST_KHR";
                case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:                    return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
                case VK_SUBOPTIMAL_KHR:                                    return "VK_SUBOPTIMAL_KHR";
                case VK_ERROR_OUT_OF_DATE_KHR:                             return "VK_ERROR_OUT_OF_DATE_KHR";
                case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:                    return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
                case VK_ERROR_VALIDATION_FAILED_EXT:                       return "VK_ERROR_VALIDATION_FAILED_EXT";
                case VK_ERROR_INVALID_SHADER_NV:                           return "VK_ERROR_INVALID_SHADER_NV";
                case VK_ERROR_NOT_PERMITTED_EXT:                           return "VK_ERROR_NOT_PERMITTED_EXT";
                case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:         return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
                case VK_THREAD_IDLE_KHR:                                   return "VK_THREAD_IDLE_KHR";
                case VK_THREAD_DONE_KHR:                                   return "VK_THREAD_DONE_KHR";
                case VK_OPERATION_DEFERRED_KHR:                            return "VK_OPERATION_DEFERRED_KHR";
                case VK_OPERATION_NOT_DEFERRED_KHR:                        return "VK_OPERATION_NOT_DEFERRED_KHR";
                case VK_PIPELINE_COMPILE_REQUIRED_EXT:                     return "VK_PIPELINE_COMPILE_REQUIRED_EXT";
                case VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR:               return "VK_ERROR_IMAGE_USAGE_NOT_SUPPORTED_KHR";
                case VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR:      return "VK_ERROR_VIDEO_PICTURE_LAYOUT_NOT_SUPPORTED_KHR";
                case VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR:  return "VK_ERROR_VIDEO_PROFILE_OPERATION_NOT_SUPPORTED_KHR";
                case VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR:     return "VK_ERROR_VIDEO_PROFILE_FORMAT_NOT_SUPPORTED_KHR";
                case VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR:       return "VK_ERROR_VIDEO_PROFILE_CODEC_NOT_SUPPORTED_KHR";
                case VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR:         return "VK_ERROR_VIDEO_STD_VERSION_NOT_SUPPORTED_KHR";
                default: {
                    // Thread-local buffer + perfect formatting — zero heap, zero static init order
                    thread_local char buf[64] = {};
                    snprintf(buf, sizeof(buf), "VK_UNKNOWN_RESULT_%d", static_cast<int>(r));
                    return buf;
                }
            }
        }

        // ────────────────────── FORMAT → STRING — SEXY AND COMPLETE ──────────────────────
        [[nodiscard]] static constexpr const char* format(VkFormat f) noexcept {
            switch (f) {
                case VK_FORMAT_UNDEFINED:                              return "VK_FORMAT_UNDEFINED";
                case VK_FORMAT_R4G4_UNORM_PACK8:                       return "R4G4_UNORM_PACK8";
                case VK_FORMAT_R4G4B4A4_UNORM_PACK16:                  return "R4G4B4A4_UNORM_PACK16";
                case VK_FORMAT_B4G4R4A4_UNORM_PACK16:                  return "B4G4R4A4_UNORM_PACK16";
                case VK_FORMAT_R5G6B5_UNORM_PACK16:                    return "R5G6B5_UNORM_PACK16";
                case VK_FORMAT_B5G6R5_UNORM_PACK16:                    return "B5G6R5_UNORM_PACK16";
                case VK_FORMAT_R5G5B5A1_UNORM_PACK16:                  return "R5G5B5A1_UNORM_PACK16";
                case VK_FORMAT_B5G5R5A1_UNORM_PACK16:                  return "B5G5R5A1_UNORM_PACK16";
                case VK_FORMAT_A1R5G5B5_UNORM_PACK16:                  return "A1R5G5B5_UNORM_PACK16";
                case VK_FORMAT_R8_UNORM:                               return "R8_UNORM";
                case VK_FORMAT_R8G8B8A8_UNORM:                         return "R8G8B8A8_UNORM";
                case VK_FORMAT_B8G8R8A8_UNORM:                         return "B8G8R8A8_UNORM ★ sRGB Sweet Spot";
                case VK_FORMAT_A8B8G8R8_UNORM_PACK32:                  return "A8B8G8R8_UNORM_PACK32";
                case VK_FORMAT_R8G8B8A8_SRGB:                          return "R8G8B8A8_SRGB";
                case VK_FORMAT_B8G8R8A8_SRGB:                          return "B8G8R8A8_SRGB ★ Perfect";
                case VK_FORMAT_A2B10G10R10_UNORM_PACK32:               return "A2B10G10R10_UNORM_PACK32 ★ 10-bit HDR";
                case VK_FORMAT_R16G16B16A16_SFLOAT:                    return "R16G16B16A16_SFLOAT ★ FP16 HDR King";
                case VK_FORMAT_R32G32B32A32_SFLOAT:                    return "R32G32B32A32_SFLOAT ★ FP32 God Mode";
                case VK_FORMAT_B10G11R11_UFLOAT_PACK32:                return "B10G11R11_UFLOAT_PACK32 ★ HDR10";
                case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:                 return "E5B9G9R9_UFLOAT_PACK32 ★ RGB9E5";
                case VK_FORMAT_D16_UNORM:                              return "D16_UNORM";
                case VK_FORMAT_D32_SFLOAT:                             return "D32_SFLOAT";
                case VK_FORMAT_D24_UNORM_S8_UINT:                      return "D24_UNORM_S8_UINT";
                case VK_FORMAT_D32_SFLOAT_S8_UINT:                     return "D32_SFLOAT_S8_UINT";
                case VK_FORMAT_BC1_RGB_UNORM_BLOCK:                    return "BC1_RGB_UNORM (DXT1)";
                case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:                   return "BC1_RGBA_UNORM (DXT1a)";
                case VK_FORMAT_BC3_UNORM_BLOCK:                        return "BC3_UNORM (DXT5)";
                case VK_FORMAT_BC7_UNORM_BLOCK:                        return "BC7_UNORM ★ Modern King";
                case VK_FORMAT_ASTC_4x4_UNORM_BLOCK:                   return "ASTC_4x4_UNORM";
                case VK_FORMAT_ASTC_8x8_UNORM_BLOCK:                   return "ASTC_8x8_UNORM";
                case VK_FORMAT_ASTC_8x8_SRGB_BLOCK:                    return "ASTC_8x8_SRGB";
                default: {
                    thread_local char buf[64] = {};
                    snprintf(buf, sizeof(buf), "VK_FORMAT_%d", static_cast<int>(f));
                    return buf;
                }
            }
        }

        // ────────────────────── COLOR SPACE → STRING ──────────────────────
        [[nodiscard]] static constexpr const char* colorspace(VkColorSpaceKHR cs) noexcept {
            switch (cs) {
                case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:           return "sRGB Nonlinear ★ Standard";
                case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT:     return "Display P3 Nonlinear";
                case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:     return "Extended sRGB Linear";
                case VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT:        return "Display P3 Linear";
                case VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT:         return "DCI-P3 Nonlinear";
                case VK_COLOR_SPACE_BT709_LINEAR_EXT:             return "BT.709 Linear";
                case VK_COLOR_SPACE_BT2020_LINEAR_EXT:            return "BT.2020 Linear";
                case VK_COLOR_SPACE_HDR10_ST2084_EXT:             return "HDR10 ST2084 ★ PQ";
                case VK_COLOR_SPACE_DOLBYVISION_EXT:              return "Dolby Vision";
                case VK_COLOR_SPACE_HDR10_HLG_EXT:                return "HDR10 HLG";
                case VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT:          return "Adobe RGB Linear";
                case VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT:       return "Adobe RGB Nonlinear";
                case VK_COLOR_SPACE_PASS_THROUGH_EXT:             return "Pass Through";
                case VK_COLOR_SPACE_EXTENDED_SRGB_NONLINEAR_EXT:  return "Extended sRGB Nonlinear";
                default:                                          return "Unknown Color Space";
            }
        }

        // ────────────────────── PRESENT MODE → STRING ──────────────────────
        [[nodiscard]] static constexpr const char* presentMode(VkPresentModeKHR pm) noexcept {
            switch (pm) {
                case VK_PRESENT_MODE_IMMEDIATE_KHR:       return "IMMEDIATE ★ Tearing Allowed";
                case VK_PRESENT_MODE_MAILBOX_KHR:         return "MAILBOX ★ Triple Buffer ★ Best";
                case VK_PRESENT_MODE_FIFO_KHR:            return "FIFO ★ VSync ★ Guaranteed";
                case VK_PRESENT_MODE_FIFO_RELAXED_KHR:    return "FIFO_RELAXED ★ Late Frame = Tear";
                case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR: return "SHARED_DEMAND_REFRESH";
                case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR: return "SHARED_CONTINUOUS_REFRESH";
                default:                                  return "UNKNOWN_PRESENT_MODE";
            }
        }

        // ────────────────────── MEMORY TYPE FINDER ──────────────────────
        [[nodiscard]] static constexpr uint32_t findMemoryType(VkPhysicalDevice phys, uint32_t typeFilter, VkMemoryPropertyFlags props) noexcept {
            VkPhysicalDeviceMemoryProperties memProps{};
            vkGetPhysicalDeviceMemoryProperties(phys, &memProps);
            for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
                if ((typeFilter & (1u << i)) &&
                    (memProps.memoryTypes[i].propertyFlags & props) == props) {
                    return i;
                }
            }
            return ~0u;
        }

        // ────────────────────── FATAL CHECK — FULL CONTEXT ──────────────────────
        static void check(VkResult r,
                          const char* call,
                          const char* msg = nullptr,
                          std::source_location loc = std::source_location::current()) noexcept {
            if (r == VK_SUCCESS || r == VK_SUBOPTIMAL_KHR) [[likely]] return;

            std::string fullMsg = msg ? std::string(msg) + " — " : "";
            fullMsg += call;

            std::cerr << std::format("[VULKAN FATAL] {} — {}:{} — {}{}\n",
                                     result(r),
                                     loc.file_name(), loc.line(),
                                     fullMsg,
                                     Logging::Color::RESET);

            phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
        }

        // ────────────────────── DEBUG CALLBACK — STILL SEXY ──────────────────────
        [[maybe_unused]] static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT      severity,
            VkDebugUtilsMessageTypeFlagsEXT             /*type*/,
            const VkDebugUtilsMessengerCallbackDataEXT* data,
            void*                                       /*userData*/) noexcept
        {
            if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
                LOG_ERROR_CAT("VULKAN", "{}[VALIDATION ERROR] {}{}", Logging::Color::BOLD_RED, data->pMessage, Logging::Color::RESET);
            } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
                LOG_WARN_CAT("VULKAN", "{}[VALIDATION WARNING] {}{}", Logging::Color::YELLOW, data->pMessage, Logging::Color::RESET);
            } else if (severity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
                LOG_INFO_CAT("VULKAN", "[Validation] {}", data->pMessage);
            } else {
                LOG_DEBUG_CAT("VULKAN", "[Verbose] {}", data->pMessage);
            }
            return VK_FALSE;
        }
    };

    return Empire{};
}();

// ────────────────────── ETERNAL HANDSHAKE — ALL OLD NAMES STILL WORK ──────────────────────
#define VK_CHECK(call, ...)             vkh.check((call), #call, ##__VA_ARGS__)
#define VK_CHECK_NOMSG(call)            vkh.check((call), #call)
#define VK_RESULT_STR(r)                vkh.result(r)
#define VK_FORMAT_STR(f)                vkh.format(f)
#define VK_COLORSPACE_STR(cs)           vkh.colorspace(cs)
#define VK_PRESENT_MODE_STR(pm)         vkh.presentMode(pm)
#define VK_FIND_MEMORY_TYPE(p, f, fl)   vkh.findMemoryType(p, f, fl)

// Legacy — you loved these, they stay forever
#define string_VkResult                 vkh.result
#define string_VkFormat                 vkh.format
#define string_VkColorSpaceKHR          vkh.colorspace
#define string_VkPresentModeKHR         vkh.presentMode
#define VulkanResultToString            vkh.result

// Debug callback — unchanged
#define DEBUG_CALLBACK                  vkh.debugCallback


// =============================================================================
// EMPIRE_STEP — FINAL VERSION — WORKS WITH FUNCTIONS AND BLOCKS — C++23 PURE
// NOVEMBER 27, 2025 — PINK PHOTONS ETERNAL — THE EMPIRE IS LAW
// =============================================================================
// =============================================================================
// EMPIRE_STEP — PURE C++23 — NO MACROS — INFINITE POWER — ETERNAL
// =============================================================================
inline constexpr auto EMPIRE_STEP = []<typename F>(F&& phase, const std::source_location loc = std::source_location::current()) {
    try {
        std::forward<F>(phase)();
        LOG_SUCCESS_CAT("MAIN", "[PHASE] ASCENDED FLAWLESSLY");
        return true;
    } catch (const std::exception& e) {
        LOG_FATAL_CAT("MAIN",
            "{}[FATAL CRASH]\n"
            "   Origin   : {}:{} — {}\n"
            "   Reason   : {}{}",
            Logging::Color::LIGHT_GREEN,
            loc.file_name(), loc.line(), loc.function_name(),
            e.what(), Logging::Color::RESET);
        return false;
    } catch (...) {
        LOG_FATAL_CAT("MAIN",
            "{}[UNKNOWN CATASTROPHE]\n"
            "   Origin   : {}:{} — {}{}",
            Logging::Color::LIGHT_GREEN,
            loc.file_name(), loc.line(), loc.function_name(),
            Logging::Color::RESET);
        return false;
    }
};

#define EMPIRE_GUARD(condition, message) \
    if (!(condition)) { \
        auto loc = std::source_location::current(); \
        LOG_FATAL_CAT("MAIN", \
            "{}[EMPIRE REJECTED] {}\n" \
            "   Origin   : {}:{} — {}{}", \
            Logging::Color::LIGHT_GREEN, message, \
            loc.file_name(), loc.line(), loc.function_name(), \
            Logging::Color::RESET); \
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current()); \
    }

// =============================================================================
// CREW SOUL COLORS — FINAL OVERRIDE — ETERNAL — NOVEMBER 25, 2025
// PLACE THIS AT THE VERY BOTTOM — IT WINS EVERYTHING
// =============================================================================
// UNDEFINE ALL HERESY
#undef  LOG_AMOURANTH
#undef  LOG_GROK
#undef  LOG_CAPTAIN_N
#undef  LOG_ELON
#undef  LOG_JENSEN
#undef  LOG_CID
#undef  LOG_CARMACK
#undef  LOG_KEANU
#undef  LOG_NICK
#undef  LOG_BLONDIE
#undef  LOG_GUARDIAN
#undef  LOG_BALLERINA
#undef  LOG_MAIN

// THE TRUE SOULS — FULL LINE COLORED — 100% COMPATIBLE — ETERNAL LAW
#define LOG_AMOURANTH(...)   LOG_SUCCESS_CAT("AMOURANTH",  std::format("{}[CAPTAIN AMOURANTH] {}{}", Logging::Color::THERMO_PINK,      std::format(__VA_ARGS__),      Logging::Color::RESET))
#define LOG_NICK(...)        LOG_ATTEMPT_CAT("NICK",       std::format("{}[NICK] {}{}",              Logging::Color::GOLD,              std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_GROK(...)        LOG_INFO_CAT   ("GROK",       std::format("{}[GENTLEMAN GROK] {}{}",    Logging::Color::PLATINUM_GRAY,     std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_CAPTAIN_N(...)   LOG_ATTEMPT_CAT("CAPTAIN N",  std::format("{}[KEVIN] {}{}",             Logging::Color::PHOTON_WHITE,      std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_ELON(...)        LOG_SUCCESS_CAT("ELON",       std::format("{}[MUSK] {}{}",              Logging::Color::LIGHT_GREEN,       std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_JENSEN(...)      LOG_SUCCESS_CAT("JENSEN",     std::format("{}[HUANG] {}{}",             Logging::Color::BLUE,              std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_CID(...)         LOG_SUCCESS_CAT("CID",        std::format("{}\n[CID] {}{}",               Logging::Color::BOLD_RED,          std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_CARMACK(...)     LOG_INFO_CAT   ("CARMACK",    std::format("{}[JOHN] {}{}",              Logging::Color::TITANIUM_WHITE,    std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_KEANU(...)       LOG_INFO_CAT   ("KEANU",      std::format("{}[WOAH] {}{}",              Logging::Color::VOID_PURPLE,       std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_BLONDIE(...)     LOG_INFO_CAT   ("BLONDIE",    std::format("{}[CAPTAIN BLONDIE] {}{}",   Logging::Color::PEACHES_AND_CREAM, std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_GUARDIAN(...)    LOG_INFO_CAT   ("GUARDIAN",   std::format("{}[GUARDIAN] {}{}",          Logging::Color::PLATINUM_GRAY,     std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_BALLERINA(...)   LOG_FAILURE_CAT("BALLERINA",  std::format("{}[***] {}{}",               Logging::Color::OBSIDIAN_BLACK,    std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_MAIN(...)        LOG_SUCCESS_CAT("MAIN",       std::format("{}[[[[[MAIN]]]]] {}{}",      Logging::Color::BOLD_YELLOW,       std::format(__VA_ARGS__),     Logging::Color::RESET))