// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial (gzac5314@gmail.com)
// AMOURANTH FOREVER 💖
// =============================================================================
// Inspired by Ellie Fier Ellie Fier 
// =============================================================================

#pragma once
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include <SDL3/SDL.h> 

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <thread>
#include <tuple>
#include <vector>
#include <cinttypes>
#include <limits>

#ifdef _WIN32
    #include <windows.h>
    #include <dbghelp.h>          // for MiniDumpWriteDump
    #pragma comment(lib, "dbghelp.lib")
#else
    #include <signal.h>           // sigaction, siginfo_t, SIG*
    #include <execinfo.h>         // backtrace, backtrace_symbols
    #include <unistd.h>           // write
#endif

#include <csignal>                // common signals (SIGSEGV etc.)
#include <cstdio>                 // snprintf
#include <cstring>                // strlen

#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

// =============================================================================
// TOTALTIME v∞ MONOLITH — The One True Clock
// =============================================================================


#include <chrono>
#include <atomic>
#include <mutex>

namespace detail {
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Duration  = Clock::duration;

    [[nodiscard]] inline uint64_t make_session_entropy() noexcept {
        static const uint64_t entropy = []() -> uint64_t {
            uint64_t e = reinterpret_cast<uintptr_t>(&entropy);
            e ^= static_cast<uint64_t>(__builtin_ia32_rdtsc());
            e ^= static_cast<uint64_t>(time(nullptr));
            e ^= static_cast<uint64_t>(getpid());
            return e;
        }();
        return entropy;
    }
}

struct TotalTime final {
    TotalTime(const TotalTime&) = delete;
    TotalTime& operator=(const TotalTime&) = delete;
    TotalTime(TotalTime&&) = delete;
    TotalTime& operator=(TotalTime&&) = delete;

    [[nodiscard]] static TotalTime& get() noexcept {
        static TotalTime instance;
        return instance;
    }

    [[nodiscard]] bool isSealed() const noexcept {
        return sealed_.load(std::memory_order_acquire);
    }

    [[nodiscard]] double us() const noexcept {
        if (isSealed()) {
            return raw_us_.load(std::memory_order_relaxed);
        }
        auto dur = detail::Clock::now() - genesis_;
        return std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(dur).count();
    }

    [[nodiscard]] double seconds() const noexcept {
        if (isSealed()) {
            return raw_seconds_.load(std::memory_order_relaxed);
        }
        auto dur = detail::Clock::now() - genesis_;
        return std::chrono::duration_cast<std::chrono::duration<double>>(dur).count();
    }

    void seal() noexcept {
        std::call_once(seal_flag_, [this] {
            auto dur = detail::Clock::now() - genesis_;
            raw_us_.store(std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(dur).count(),
                          std::memory_order_release);
            raw_seconds_.store(std::chrono::duration_cast<std::chrono::duration<double>>(dur).count(),
                               std::memory_order_release);
            sealed_.store(true, std::memory_order_release);
        });
    }

    [[nodiscard]] uint64_t entropy() const noexcept {
        return entropy_;
    }

private:
    TotalTime() noexcept
        : entropy_(detail::make_session_entropy())
        , genesis_(detail::Clock::now())
        , raw_us_(0.0)
        , raw_seconds_(0.0)
        , sealed_(false)
    {
        entropy_check_ = entropy_ ^ 0x9E37AF18C64D8A17UL;
    }

    void verify() const noexcept {
        if ((entropy_ ^ 0x9E37AF18C64D8A17UL) != entropy_check_) [[unlikely]] {
            std::abort();  // memory corruption — fatal
        }
    }

    uint64_t entropy_;
    uint64_t entropy_check_;
    detail::TimePoint genesis_;
    std::atomic<double> raw_us_;
    std::atomic<double> raw_seconds_;
    std::atomic<bool> sealed_;
    std::once_flag seal_flag_;
};

inline void print_total_time(const char* prefix = nullptr) noexcept {
    const TotalTime& tt = TotalTime::get();

    if (!tt.isSealed()) {
        std::cout << (prefix ? prefix : "") << "[totalTime] not sealed yet — skipping print\n";
        std::cout.flush();
        return;
    }

    double s   = tt.seconds();
    double ms  = s * 1000.0;
    double usv = tt.us();
    double fps = (s > 1e-6) ? 1.0 / s : std::numeric_limits<double>::infinity();

    std::cout << (prefix ? prefix : "")
              << std::format("[totalTime] {}s | {}ms | {}µs | ~{} pseudo-FPS\n",
                             s, ms, usv, fps);
    std::cout.flush();
}

inline void ptt(const char* prefix = nullptr) noexcept {
    print_total_time(prefix);
}

// =============================================================================
// END TOTALTIME v∞ MONOLITH — The One True Clock
// =============================================================================

// =============================================================================
// One of many lookups
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
                case VK_ERROR_NOT_PERMITTED_KHR:                   str = "VK_ERROR_NOT_PERMITTED_KHR"; break;
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

// ========================================================================
// CONFIGURATION & LOGGING MACROS you can flip before building for no logs
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
constexpr bool SIMULATION_LOGGING = true;

constexpr size_t LEVEL_WIDTH   = 10;
constexpr size_t DELTA_WIDTH   = 10;
constexpr size_t TIME_WIDTH    = 10;
constexpr size_t CAT_WIDTH     = 12;
constexpr size_t THREAD_WIDTH  = 18;

#define LOG_TRACE(...)          [&]() { if constexpr (ENABLE_TRACE)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Trace,   "General", __VA_ARGS__); }();
#define LOG_DEBUG(...)          [&]() { if constexpr (ENABLE_DEBUG)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Debug,   "General", __VA_ARGS__); }();
#define LOG_INFO(...)           [&]() { if constexpr (ENABLE_INFO)    Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Info,    "General", __VA_ARGS__); }();
#define LOG_SUCCESS(...)        [&]() { if constexpr (ENABLE_SUCCESS) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Success, "General", __VA_ARGS__); }();
#define LOG_ATTEMPT(...)        [&]() { if constexpr (ENABLE_ATTEMPT) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Attempt, "General", __VA_ARGS__); }();
#define LOG_PERF(...)           [&]() { if constexpr (ENABLE_PERF)    Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Perf,    "General", __VA_ARGS__); }();
#define LOG_WARNING(...)        [&]() { if constexpr (ENABLE_WARNING) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Warning, "General", __VA_ARGS__); }();
#define LOG_WARN(...)           LOG_WARNING(__VA_ARGS__)
#define LOG_ERROR(...)          [&]() { if constexpr (ENABLE_ERROR)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Error,   "General", __VA_ARGS__); }();
#define LOG_FAILURE(...)        [&]() { if constexpr (ENABLE_FAILURE) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Failure, "General", __VA_ARGS__); }();
#define LOG_FATAL(...)          [&]() { if constexpr (ENABLE_FATAL)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Fatal,   "General", __VA_ARGS__); }();
#define LOG_SIMULATION(...)     [&]() { if constexpr (SIMULATION_LOGGING) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Info, "SIMULATION", __VA_ARGS__); }();

#define LOG_TRACE_CAT(cat, ...)   [&]() { if constexpr (ENABLE_TRACE)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Trace,   cat, __VA_ARGS__); }();
#define LOG_DEBUG_CAT(cat, ...)   [&]() { if constexpr (ENABLE_DEBUG)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Debug,   cat, __VA_ARGS__); }();
#define LOG_INFO_CAT(cat, ...)    [&]() { if constexpr (ENABLE_INFO)    Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Info,    cat, __VA_ARGS__); }();
#define LOG_SUCCESS_CAT(cat, ...) [&]() { if constexpr (ENABLE_SUCCESS) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Success, cat, __VA_ARGS__); }();
#define LOG_ATTEMPT_CAT(cat, ...) [&]() { if constexpr (ENABLE_ATTEMPT) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Attempt, cat, __VA_ARGS__); }();
#define LOG_PERF_CAT(cat, ...)    [&]() { if constexpr (ENABLE_PERF)    Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Perf,    cat, __VA_ARGS__); }();
#define LOG_WARNING_CAT(cat, ...) [&]() { if constexpr (ENABLE_WARNING) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Warning, cat, __VA_ARGS__); }();
#define LOG_WARN_CAT(cat, ...)    LOG_WARNING_CAT(cat, __VA_ARGS__)
#define LOG_ERROR_CAT(cat, ...)   [&]() { if constexpr (ENABLE_ERROR)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Error,   cat, __VA_ARGS__); }();
#define LOG_FAILURE_CAT(cat, ...) [&]() { if constexpr (ENABLE_FAILURE) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Failure, cat, __VA_ARGS__); }();
#define LOG_FATAL_CAT(cat, ...)   [&]() { if constexpr (ENABLE_FATAL)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Fatal,   cat, __VA_ARGS__); }();
#define LOG_ERROR_RETURN_CAT(cat, ret, ...) [&]()->decltype(ret){ if constexpr (ENABLE_ERROR) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Error, cat, __VA_ARGS__); return ret; }()

#define LOG_VOID()              [&]() { if constexpr (ENABLE_DEBUG)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Debug,   "General", "[VOID MARKER]"); }();
#define LOG_VOID_CAT(cat)       [&]() { if constexpr (ENABLE_DEBUG)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Debug,   cat, "[VOID MARKER]"); }();
#define LOG_VOID_TRACE()        [&]() { if constexpr (ENABLE_TRACE)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Trace,   "General", "[VOID MARKER]"); }();
#define LOG_VOID_TRACE_CAT(cat) [&]() { if constexpr (ENABLE_TRACE)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Trace,   cat, "[VOID MARKER]"); }();

// ETERNAL LAW
#define LOG_AMOURANTH(...)   LOG_SUCCESS_CAT("AMOURANTH",  std::format("{}\n[CAPTAIN AMOURANTH] {}{}", Logging::Color::AMOURANTH,       std::format(__VA_ARGS__),     Logging::Color::RESET))

namespace Logging {

// ========================================================================
// LOG LEVEL
// ========================================================================
enum class LogLevel { Trace, Debug, Info, Success, Attempt, Perf, Warning, Error, Failure, Fatal };

// ========================================================================
// 1. HYPER-VIVID ANSI COLORS
// ========================================================================
namespace Color {
    inline constexpr const char* AMOURANTH = "\033[1;38;2;255;240;245;48;2;50;40;48m";
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
    inline constexpr std::string_view COLOR_PINK                = "\033[1;38;5;213m";
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
    inline constexpr std::string_view BLOOD_RED                 = "\033[1;38;5;198m";
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
    inline constexpr std::string_view RED         = "\033[1;38;5;198m";
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
    inline constexpr std::string_view GOLD         = "\033[38;5;142m";
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
    inline constexpr std::string_view FROSTFIRE_BLUE    = "\033[1;38;5;39m";
    inline constexpr std::string_view NUCLEAR_GREEN     = "\033[1;38;5;82m";
    inline constexpr std::string_view HYPER_VIOLET      = "\033[1;38;5;141m";
    inline constexpr std::string_view PURE_ENERGY       = "\033[1;38;5;227m";
    inline constexpr std::string_view ETERNAL_FLAME     = "\033[1;38;5;196m\033[5m";
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
    static Logger& get() noexcept {
        static Logger instance;
        return instance;
    }

    // Explicit startup — call once after all static init is safe
    void startup() noexcept {
        // Force TotalTime construction here (safe now — logger is ready)
        // This makes genesis_ ≈ logger startup time, which is usually very close to main()
        TotalTime& tt = TotalTime::get();

        double currentUs = tt.us();
        auto currentSys = std::chrono::system_clock::now();

        // First logged line — make it count (source_location path exercised immediately)
        printMessage(std::source_location::current(), LogLevel::Success, "ELLIE",
                     "ELLIE ONLINE — AMOURANTH RTX ENGINE AWAKENED 💖 TOTALTIME SYNCED (source_location::current active)",
                     currentUs, currentSys, false, nullptr, nullptr);

        setAsync(true);

        // Rich startup logs — TotalTime, entropy, apocalypse ready, colors, THERMO emphasis
        LOG_SUCCESS_CAT("ELLIE", "Session entropy: 0x{:016x} | genesis locked at {:.1f}µs | TotalTime::us()={:.1f} TotalTime::seconds()={:.6f}", tt.entropy(), currentUs, tt.us(), tt.seconds());
        LOG_INFO_CAT("ELLIE", "Source location: file={} line={} func={} (all LOG_*_CAT use std::source_location::current for pinpointing)", 
                     std::filesystem::path(std::source_location::current().file_name()).filename().string(),
                     std::source_location::current().line(), std::source_location::current().function_name());
        LOG_DEBUG_CAT("ELLIE", "Logger startup complete at {:.1f}µs | TotalTime sealed?={} | source_loc active for all subsequent TRACE/DEBUG/THERMO | apocalypse handler armed", currentUs, tt.isSealed());
        LOG_DEBUG_CAT("THERMO", "THERMO category online (THERMO_PINK={} color) — accountant population, field seeds, entropy calcs, prevMaintCost, freeEnergyIncome, boundaryThermo, pre-transitions, descriptor writes, clears, fabric, dispatch steps, status will be traced here + in RAYCANVAS/PIPELINE/NAVIGATOR", Logging::Color::THERMO_PINK);
        LOG_SUCCESS_CAT("THERMO", "Startup THERMO: TotalTime entropy=0x{:016x} | ready for RayCanvas ctor (fabric+accountant+seeds) + every dispatch population", tt.entropy());
        LOG_ATTEMPT_CAT("MAIN", "Apocalypse handler installed pre-thermo; full ELLIE + THERMO_PINK + source_location + TotalTime v∞ ready for engine lifetime");
    }

    static void setAsync(bool enable) noexcept {
        Logger& self = get();
        if (enable) {
            if (!self.flusher_.joinable()) {
                self.flusher_ = std::jthread([&self](std::stop_token st){ self.flushQueue(st); });
            }
        } else {
            if (self.flusher_.joinable()) {
                self.flusher_.request_stop();
                self.flusher_.join();
            }
        }
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

        double logUs = TotalTime::get().us();
        auto logSysTime = std::chrono::system_clock::now();

        static uint64_t seq = 0;
        uint64_t id = ++seq;

        std::string msg = std::vformat(fmt, std::make_format_args(args...));

        {
            std::scoped_lock lk(queueMutex_);
            messageQueue_.emplace_back(id, loc, level, std::string{category}, std::move(msg), logUs, logSysTime);
        }
    }

private:
    using Entry = std::tuple<uint64_t, std::source_location, LogLevel, std::string, std::string, double, std::chrono::system_clock::time_point>;

    Logger() noexcept
        : logFile_("amouranth_engine.log", std::ios::out | std::ios::app)
    {
        // CONSTRUCTOR IS SILENT — NO LOGGING HERE TO AVOID RECURSION DURING STATIC INIT
        // startup() will be called explicitly after all singletons are safe
    }

    ~Logger() {
        setAsync(false);
        double currentUs = TotalTime::get().us();
        auto currentSys = std::chrono::system_clock::now();
        printMessage(std::source_location::current(), LogLevel::Success, "Logger",
                     "SAFE SHUTDOWN o7", currentUs, currentSys, false, nullptr, nullptr);
        if (logFile_.is_open()) logFile_.flush(), logFile_.close();
    }

    mutable std::ofstream logFile_;

    mutable std::deque<Entry> messageQueue_;
    mutable std::mutex queueMutex_;
    mutable std::jthread flusher_;

    bool shouldLog(LogLevel level, [[maybe_unused]] std::string_view category) const {
        size_t i = static_cast<size_t>(level);
        if (i >= ENABLE_LEVELS.size() || !ENABLE_LEVELS[i]) return false;
        return true;
    }

    void flushQueue(std::stop_token st) const {
        std::vector<Entry> batch;
        batch.reserve(64);
        while (!st.stop_requested()) {
            {
                std::unique_lock lk(queueMutex_);
                if (messageQueue_.empty()) {
                    lk.unlock();
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                    continue;
                }
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
                for (auto& e : batch) {
                    std::source_location loc = std::get<1>(e);
                    LogLevel lvl = std::get<2>(e);
                    std::string_view cat{std::get<3>(e)};
                    std::string msg = std::move(std::get<4>(e));
                    double deltaUs = std::get<5>(e);
                    auto sysTime = std::get<6>(e);
                    printMessage(loc, lvl, cat, std::move(msg), deltaUs, sysTime, true, &terminal_batch, &file_batch);
                }
                std::cout << terminal_batch;
                if (logFile_.is_open()) logFile_ << file_batch;
            }
        }
        // Final drain on shutdown
        std::vector<Entry> finalBatch;
        {
            std::unique_lock lk(queueMutex_);
            while (!messageQueue_.empty()) {
                finalBatch.push_back(std::move(messageQueue_.front()));
                messageQueue_.pop_front();
            }
        }
        if (!finalBatch.empty()) {
            std::sort(finalBatch.begin(), finalBatch.end(),
                      [](const Entry& a, const Entry& b) { return std::get<0>(a) < std::get<0>(b); });
            std::string terminal_batch;
            std::string file_batch;
            for (auto& e : finalBatch) {
                std::source_location loc = std::get<1>(e);
                LogLevel lvl = std::get<2>(e);
                std::string_view cat{std::get<3>(e)};
                std::string msg = std::move(std::get<4>(e));
                double deltaUs = std::get<5>(e);
                auto sysTime = std::get<6>(e);
                printMessage(loc, lvl, cat, std::move(msg), deltaUs, sysTime, true, &terminal_batch, &file_batch);
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
            {"ELLIE", ELECTRIC_BLUE},
			{"GENERAL", DIAMOND_SPARKLE}, {"MAIN", VALHALLA_GOLD},
            {"VULKAN", ORANGE}, {"SWAPCHAIN", OCEAN_TEAL}, {"DISPOSE", PARTY_PINK}, 
			{"COMMAND", CHROMIUM_SILVER}, {"MEMORY", PEACHES_AND_CREAM},
            {"RTX", HYPERSPACE_WARP}, {"TLAS", LIME}, {"BLAS", GREEN}, {"LAS", FROSTFIRE_BLUE},
			{"SBT", RASPBERRY_PINK}, {"SHADER", NEBULA_VIOLET}, {"RENDERER", BRIGHT_PINKISH_PURPLE},
            {"RENDER", THERMO_PINK}, {"TONEMAP", PEACHES_AND_CREAM}, {"BUFFER", GOLD},
            {"TEXTURE", SPEARMINT_MINT}, {"DESCRIPTOR", FUCHSIA_MAGENTA},
            {"PERF", COSMIC_GOLD}, {"GPU", BLACK_HOLE},
            {"CPU", PLASMA_FUCHSIA}, {"INPUT", OCEAN_TEAL}, {"AUDIO", OCEAN_TEAL},
            {"MATERIAL", SUNGLOW_ORANGE}, {"MESHLOADER", LIME_YELLOW}, {"DEBUG", ARCTIC_CYAN},
            {"ATTEMPT", QUANTUM_PURPLE}, {"VOID", COSMIC_VOID}, {"SPLASH", LILAC_LAVENDER},
            {"MARKER", DIAMOND_SPARKLE}, {"SDL3", HYPERSPACE_WARP},
			
			{"AMOURANTH", AMOURANTH},
			{"PIPELINE", SPEARMINT_MINT}, {"CANVAS", BRONZE_BROWN}, {"RAYCANVAS", PEACHES_AND_CREAM},
			{"RAY TRACING", LIME_YELLOW},
			{"THERMO", THERMO_PINK}
        };
        auto it = map.find(cat);
        if (it != map.end()) [[likely]]
            return it->second;
        return DIAMOND_WHITE;
    }

    void printMessage(std::source_location loc,
                      LogLevel level,
                      std::string_view category,
                      std::string formattedMessage,
                      double deltaUs,
                      std::chrono::system_clock::time_point sysTime,
                      bool batch = false,
                      std::string* term_out = nullptr,
                      std::string* file_out = nullptr) const
    {
        using namespace Color;
        size_t levelIdx = static_cast<size_t>(level);
        const LevelInfo& info = LEVEL_INFOS[levelIdx];
        std::string_view levelColor = info.color;
        std::string_view levelBg    = info.bg;
        std::string_view levelStr   = info.str;
        std::string_view catColor   = getCategoryColor(category);

        std::string deltaStr = [deltaUs]() -> std::string {
            if (deltaUs < 10000.0) [[likely]] return std::format("{:>7.0f}µs", deltaUs);
            if (deltaUs < 1000000.0) return std::format("{:>7.3f}ms", deltaUs / 1000.0);
            if (deltaUs < 60000000.0) return std::format("{:>7.3f}s", deltaUs / 1000000.0);
            if (deltaUs < 3600000000.0) return std::format("{:>7.1f}m", deltaUs / 60000000.0);
            return std::format("{:>7.1f}h", deltaUs / 3600000000.0);
        }();

        std::string timeStr = [] (auto tp) -> std::string {
            time_t tt = std::chrono::system_clock::to_time_t(tp);
            tm* tmPtr = std::localtime(&tt);
            char buf[9];
            std::strftime(buf, sizeof(buf), "%H:%M:%S", tmPtr);
            return std::string(buf);
        }(sysTime);

        std::string threadId = std::format("{}ms", deltaUs / 1000.0);
        std::string fileLine = std::format("{}:{}:{}", loc.file_name(), loc.line(), loc.function_name());

        // Plain text for file
        std::string plain_line1 = std::format("{:<{}} {:>{}} {:>{}} [{:>{}}] [{:>{}}] {}\n",
                                              levelStr, LEVEL_WIDTH,
                                              deltaStr, DELTA_WIDTH,
                                              timeStr,  TIME_WIDTH,
                                              category, CAT_WIDTH,
                                              threadId, THREAD_WIDTH,
                                              formattedMessage);
        std::string plain_line2 = std::format("{}\n", fileLine);
        std::string plain = plain_line1 + plain_line2 + "\n";

        // Colored terminal output
        std::ostringstream oss;
        oss << levelBg
            << std::format("{:<{}}", levelStr, LEVEL_WIDTH) << RESET
            << " " << std::format("{:>{}}", deltaStr, DELTA_WIDTH) << " "
            << std::format("{:>{}}", timeStr, TIME_WIDTH) << " "
            << catColor << std::format("[{:<{}}]", category, CAT_WIDTH - 2) << RESET
            << " " << LIME_GREEN << std::format("[{:>{}}]", threadId, THREAD_WIDTH - 2) << RESET
            << " " << levelColor << formattedMessage << RESET << '\n'
            << CHROMIUM_SILVER << fileLine << RESET << '\n'
            << '\n';
        std::string colored = oss.str();

        if (batch) {
            if (term_out) *term_out += colored;
            if (file_out) *file_out += plain;
        } else {
            std::cout << colored;
            std::cout.flush();

            if (logFile_.is_open()) {
                logFile_ << plain;
                logFile_.flush();
            }
        }
    }
};

} // namespace Logging

// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL GPU CRASH STATE
// ──────────────────────────────────────────────────────────────────────────────
struct GPUCrash {
    bool happened{false};
    uint64_t          addr{0};
    uint32_t          type{0};
    char              desc[512]{"Unknown GPU fault -- likely null SBT buffer or destroyed resource"};
};

inline GPUCrash g_gpu_crash;

// ──────────────────────────────────────────────────────────────────────────────
// ASYNC-SIGNAL-SAFE PRINTING
// ──────────────────────────────────────────────────────────────────────────────
static void safe_write(const char* data, size_t len) noexcept {
    if (data && len) {
#ifdef _WIN32
        DWORD written;
        WriteConsoleA(GetStdHandle(STD_ERROR_HANDLE), data, static_cast<DWORD>(len), &written, nullptr);
#else
        ssize_t ignored = ::write(STDERR_FILENO, data, len);
        static_cast<void>(ignored);
#endif
    }
}

static void safe_writeln(const char* data) noexcept {
    size_t len = strlen(data);
    safe_write(data, len);
    safe_write("\n", 1);
}

// Color defines
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"

// ──────────────────────────────────────────────────────────────────────────────
// THE MANUAL — CROSS-PLATFORM ADVICE
// ──────────────────────────────────────────────────────────────────────────────
struct AdviceManualEntry {
    uint32_t code;
    const char* legend;
    const char* name;
    const char* focus;
    const char** lines;
};

static const char* TORVALDS_ADVICE_LINES[] = {
    "                     \"Talk is cheap. Show me the code.\"",
    "                     — Linus Torvalds",
    nullptr
};

static const char* HOPPER_ADVICE_LINES[] = {
    "                     \"The most damaging phrase in the language is:",
    "                     'It's always been done that way.'\"",
    "                     — Grace Hopper",
    nullptr
};

static const char* KNUTH_ADVICE_LINES[] = {
    "                     \"Beware of bugs in the above code;",
    "                     I have only proved it correct, not tried it.\"",
    "                     — Donald Knuth",
    nullptr
};

static const char* RITCHIE_ADVICE_LINES[] = {
    "                     \"What we wanted to preserve was not just a",
    "                     good programming language, but a simple one.\"",
    "                     — Dennis Ritchie",
    nullptr
};

static const char* TURING_ADVICE_LINES[] = {
    "                     \"We can only see a short distance ahead,",
    "                     but we can see plenty there that needs to be done.\"",
    "                     — Alan Turing",
    nullptr
};

static constexpr std::array<AdviceManualEntry, 6> THE_MANUAL = {{
#ifdef _WIN32
    { EXCEPTION_ACCESS_VIOLATION, "TORVALDS", "KERNEL KING & GIT MASTER",     "CLEAN CODE, NO NONSENSE", TORVALDS_ADVICE_LINES },
    { EXCEPTION_STACK_OVERFLOW,   "HOPPER",   "COMPUTING ADMIRAL & COBOL QUEEN", "INNOVATE & INSPIRE",     HOPPER_ADVICE_LINES   },
    { EXCEPTION_INT_DIVIDE_BY_ZERO, "KNUTH",  "ALGO MASTER & TEX TYCOON",       "ELEGANCE IN COMPLEXITY",  KNUTH_ADVICE_LINES    },
    { EXCEPTION_ILLEGAL_INSTRUCTION, "RITCHIE", "C CREATOR & UNIX FATHER",     "SIMPLICITY & PORTABILITY",RITCHIE_ADVICE_LINES  },
    { EXCEPTION_BREAKPOINT,       "TURING",   "COMPUTABILITY KING & ENIGMA BREAKER", "THEORY TO PRACTICE", TURING_ADVICE_LINES   },
#else
    { SIGSEGV, "TORVALDS", "KERNEL KING & GIT MASTER",     "CLEAN CODE, NO NONSENSE", TORVALDS_ADVICE_LINES },
    { SIGABRT, "HOPPER",   "COMPUTING ADMIRAL & COBOL QUEEN", "INNOVATE & INSPIRE",     HOPPER_ADVICE_LINES   },
    { SIGFPE,  "KNUTH",    "ALGO MASTER & TEX TYCOON",       "ELEGANCE IN COMPLEXITY",  KNUTH_ADVICE_LINES    },
    { SIGILL,  "RITCHIE",  "C CREATOR & UNIX FATHER",       "SIMPLICITY & PORTABILITY",RITCHIE_ADVICE_LINES  },
    { SIGBUS,  "TURING",   "COMPUTABILITY KING & ENIGMA BREAKER", "THEORY TO PRACTICE", TURING_ADVICE_LINES   },
#endif
    { 0, "TORVALDS", "DEFAULT CRASH", "GENERAL WISDOM", TORVALDS_ADVICE_LINES } // fallback
}};

// ──────────────────────────────────────────────────────────────────────────────
// CROSS-PLATFORM APOCALYPSE HANDLER
// ──────────────────────────────────────────────────────────────────────────────
#ifdef _WIN32

static LONG WINAPI apocalypse_handler(EXCEPTION_POINTERS* pExceptionInfo) noexcept {
    char dumpPath[MAX_PATH];
    SYSTEMTIME st;
    GetLocalTime(&st);
    snprintf(dumpPath, sizeof(dumpPath), "amouranth_crash_%04d%02d%02d_%02d%02d%02d.dmp",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

    HANDLE hFile = CreateFileA(dumpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mdei{};
        mdei.ThreadId = GetCurrentThreadId();
        mdei.ExceptionPointers = pExceptionInfo;
        mdei.ClientPointers = FALSE;

        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                          hFile, MiniDumpNormal, &mdei, nullptr, nullptr);
        CloseHandle(hFile);
    }

    [[maybe_unused]] const AdviceManualEntry* verdict = &THE_MANUAL.back();
    for (const AdviceManualEntry& e : THE_MANUAL) {
        if (e.code == pExceptionInfo->ExceptionRecord->ExceptionCode) {
            verdict = &e;
            break;
        }
    }

    char buf[512];
    // Fixed %lx -> proper format using std::format 0x{:016x} style (even for code; safe path)
    auto excCodeStr = std::format(COLOR_YELLOW "EXCEPTION CODE : 0x{:016x}" COLOR_RESET,
                                  static_cast<unsigned long long>(pExceptionInfo->ExceptionRecord->ExceptionCode));
    snprintf(buf, sizeof(buf), "%s", excCodeStr.c_str());
    safe_writeln(buf);
    snprintf(buf, sizeof(buf), COLOR_YELLOW "FAULT ADDRESS  : %p" COLOR_RESET,
             pExceptionInfo->ExceptionRecord->ExceptionAddress);
    safe_writeln(buf);
    snprintf(buf, sizeof(buf), COLOR_YELLOW "MINIDUMP SAVED : %s" COLOR_RESET, dumpPath);
    safe_writeln(buf);
    snprintf(buf, sizeof(buf), COLOR_YELLOW "BUILD          : %s %s" COLOR_RESET, __DATE__, __TIME__);
    safe_writeln(buf);
    safe_writeln("");
    // Hex handle dump in apocalypse for debug (thermo/accountant context if live)
    snprintf(buf, sizeof(buf), COLOR_CYAN "KEY HANDLES (hex): device=%p surface=%p" COLOR_RESET, (void*)GetCurrentProcess(), (void*)0); // placeholders; real would snapshot
    safe_writeln(buf);

    if (g_gpu_crash.happened) {
        safe_writeln(COLOR_RED "GPU CRASH CONFIRMED -- That can't be good." COLOR_RESET);
        snprintf(buf, sizeof(buf), "Diagnosis      : %s", g_gpu_crash.desc[0] ? g_gpu_crash.desc : "Unknown");
        safe_writeln(buf);
        safe_writeln("");
    }

    safe_writeln(COLOR_CYAN "STACK WALK (limited):" COLOR_RESET);
    void* stack[64];
    unsigned short frames = CaptureStackBackTrace(0, 64, stack, nullptr);
    for (unsigned short i = 0; i < frames && i < 20; ++i) {
        snprintf(buf, sizeof(buf), "  [%02d] %p", i, stack[i]);
        safe_writeln(buf);
    }
    safe_writeln("");

    safe_writeln(COLOR_MAGENTA "-- Programming Legends & Gentleman Grok" COLOR_RESET);
    safe_writeln("");
    safe_writeln(COLOR_MAGENTA "\"Fix the forever bug.\"" COLOR_RESET);

    return EXCEPTION_EXECUTE_HANDLER;
}

#else

static void apocalypse_handler(int sig, siginfo_t* info, void*) noexcept {
    struct timespec req = { 0, 8000000L };
    nanosleep(&req, nullptr);
    safe_write("\033[2J\033[H", 7);

    const AdviceManualEntry* verdict = &THE_MANUAL[0];
    for (const AdviceManualEntry& e : THE_MANUAL) {
        if (e.code == static_cast<uint32_t>(sig)) {
            verdict = &e;
            break;
        }
    }

    for (const char** line = verdict->lines; *line; ++line) {
        safe_writeln(*line);
    }
    
    char buf[256];
    snprintf(buf, sizeof(buf), COLOR_YELLOW "SIGNAL        : %d" COLOR_RESET, sig);
    safe_writeln(buf);
    snprintf(buf, sizeof(buf), COLOR_YELLOW "FAULT ADDRESS : %p" COLOR_RESET, info ? info->si_addr : nullptr);
    safe_writeln(buf);
    snprintf(buf, sizeof(buf), COLOR_YELLOW "BUILD         : %s %s" COLOR_RESET, __DATE__, __TIME__);
    safe_writeln(buf);
    safe_writeln("");

    if (g_gpu_crash.happened) {
        safe_writeln(COLOR_RED "GPU CRASH CONFIRMED -- That can't be good." COLOR_RESET);

        char diag_buf[1024] = {0};  // Much larger buffer to safely hold desc (512 max) + prefix

        const char* diagnosis = g_gpu_crash.desc[0] ? g_gpu_crash.desc : "Unknown";
        int len = snprintf(diag_buf, sizeof(diag_buf), "Diagnosis     : %s", diagnosis);

        // If truncated (very unlikely now), add ellipsis for safety
        if (len >= static_cast<int>(sizeof(diag_buf))) {
            diag_buf[sizeof(diag_buf) - 4] = '.';
            diag_buf[sizeof(diag_buf) - 3] = '.';
            diag_buf[sizeof(diag_buf) - 2] = '.';
            diag_buf[sizeof(diag_buf) - 1] = '\0';
        }

        safe_writeln(diag_buf);
        safe_writeln("");
    }

    safe_writeln(COLOR_CYAN "BACKTRACE:" COLOR_RESET);
    void* array[64];
    int n = backtrace(array, 64);
    if (n > 1) {
        // backtrace_symbols_fd avoids malloc in the signal handler (free() can corrupt if heap is already damaged).
        backtrace_symbols_fd(array, n, STDERR_FILENO);
    }
    safe_writeln("");

    safe_writeln(COLOR_MAGENTA "-- Programming Legends & Gentleman Grok" COLOR_RESET);
    safe_writeln("");
    safe_writeln(COLOR_MAGENTA "\"Fix the forever bug.\"" COLOR_RESET);

    _exit(128 + sig);
}

#endif

// ──────────────────────────────────────────────────────────────────────────────
// INSTALL CRASH HANDLER
// ──────────────────────────────────────────────────────────────────────────────
inline void install_apocalypse_handler() noexcept {
#ifdef _WIN32
    SetUnhandledExceptionFilter(apocalypse_handler);
#else
    struct sigaction sa{};
    sa.sa_sigaction = apocalypse_handler;
    sa.sa_flags = static_cast<int>(SA_SIGINFO | SA_RESETHAND);
    sigemptyset(&sa.sa_mask);

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGFPE,  &sa, nullptr);
    sigaction(SIGILL,  &sa, nullptr);
    sigaction(SIGBUS,  &sa, nullptr);
#endif
}

// ──────────────────────────────────────────────────────────────────────────────
// Empire / vkh
// ──────────────────────────────────────────────────────────────────────────────
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
            case VK_ERROR_SURFACE_LOST_KHR:                            return "VK_ERROR_SURFACE_LOST_KHR";
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:                    return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
            case VK_SUBOPTIMAL_KHR:                                    return "VK_SUBOPTIMAL_KHR";
            case VK_ERROR_OUT_OF_DATE_KHR:                             return "VK_ERROR_OUT_OF_DATE_KHR";
            case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR:                    return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
            case VK_ERROR_VALIDATION_FAILED_EXT:                       return "VK_ERROR_VALIDATION_FAILED_EXT";
            case VK_ERROR_INVALID_SHADER_NV:                           return "VK_ERROR_INVALID_SHADER_NV";
            case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: return "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT";
            case VK_ERROR_FRAGMENTATION_EXT:                           return "VK_ERROR_FRAGMENTATION_EXT";
            case VK_ERROR_NOT_PERMITTED_KHR:                           return "VK_ERROR_NOT_PERMITTED_KHR";
            case VK_ERROR_INVALID_DEVICE_ADDRESS_EXT:                  return "VK_ERROR_INVALID_DEVICE_ADDRESS_EXT";
            case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:         return "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT";
            case VK_THREAD_IDLE_KHR:                                   return "VK_THREAD_IDLE_KHR";
            case VK_THREAD_DONE_KHR:                                   return "VK_THREAD_DONE_KHR";
            case VK_OPERATION_DEFERRED_KHR:                            return "VK_OPERATION_DEFERRED_KHR";
            case VK_OPERATION_NOT_DEFERRED_KHR:                        return "VK_OPERATION_NOT_DEFERRED_KHR";
            case VK_PIPELINE_COMPILE_REQUIRED_EXT:                     return "VK_PIPELINE_COMPILE_REQUIRED_EXT";
            default: {
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
            case VK_FORMAT_B8G8R8A8_UNORM:                         return "B8G8R8A8_UNORM -- sRGB Sweet Spot";
            case VK_FORMAT_A8B8G8R8_UNORM_PACK32:                  return "A8B8G8R8_UNORM_PACK32";
            case VK_FORMAT_R8G8B8A8_SRGB:                          return "R8G8B8A8_SRGB";
            case VK_FORMAT_B8G8R8A8_SRGB:                          return "B8G8R8A8_SRGB -- Perfect";
            case VK_FORMAT_A2B10G10R10_UNORM_PACK32:               return "A2B10G10R10_UNORM_PACK32 -- 10-bit HDR";
            case VK_FORMAT_R16G16B16A16_SFLOAT:                    return "R16G16B16A16_SFLOAT -- FP16 HDR Boss";
            case VK_FORMAT_R32G32B32A32_SFLOAT:                    return "R32G32B32A32_SFLOAT -- FP32 mmk";
            case VK_FORMAT_B10G11R11_UFLOAT_PACK32:                return "B10G11R11_UFLOAT_PACK32 -- HDR10";
            case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:                 return "E5B9G9R9_UFLOAT_PACK32 -- RGB9E5";
            case VK_FORMAT_D16_UNORM:                              return "D16_UNORM";
            case VK_FORMAT_D32_SFLOAT:                             return "D32_SFLOAT";
            case VK_FORMAT_D24_UNORM_S8_UINT:                      return "D24_UNORM_S8_UINT";
            case VK_FORMAT_D32_SFLOAT_S8_UINT:                     return "D32_SFLOAT_S8_UINT";
            case VK_FORMAT_BC1_RGB_UNORM_BLOCK:                    return "BC1_RGB_UNORM (DXT1)";
            case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:                   return "BC1_RGBA_UNORM (DXT1a)";
            case VK_FORMAT_BC3_UNORM_BLOCK:                        return "BC3_UNORM (DXT5)";
            case VK_FORMAT_BC7_UNORM_BLOCK:                        return "BC7_UNORM -- Modern";
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
            case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:           return "sRGB Nonlinear -- Standard";
            case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT:     return "Display P3 Nonlinear";
            case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT:     return "Extended sRGB Linear";
            case VK_COLOR_SPACE_DISPLAY_P3_LINEAR_EXT:        return "Display P3 Linear";
            case VK_COLOR_SPACE_DCI_P3_NONLINEAR_EXT:         return "DCI-P3 Nonlinear";
            case VK_COLOR_SPACE_BT709_LINEAR_EXT:             return "BT.709 Linear";
            case VK_COLOR_SPACE_BT2020_LINEAR_EXT:            return "BT.2020 Linear";
            case VK_COLOR_SPACE_HDR10_ST2084_EXT:             return "HDR10 ST2084 -- PQ";
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
            case VK_PRESENT_MODE_IMMEDIATE_KHR:       return "IMMEDIATE -- Tearing Allowed";
            case VK_PRESENT_MODE_MAILBOX_KHR:         return "MAILBOX -- Triple Buffer -- Best";
            case VK_PRESENT_MODE_FIFO_KHR:            return "FIFO -- VSync -- Guaranteed";
            case VK_PRESENT_MODE_FIFO_RELAXED_KHR:    return "FIFO_RELAXED -- Late Frame = Tear";
            case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR: return "SHARED_DEMAND_REFRESH";
            case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR: return "SHARED_CONTINUOUS_REFRESH";
            default:                                  return "UNKNOWN_PRESENT_MODE";
        }
    }

    // ────────────────────── DESCRIPTOR TYPE → STRING ──────────────────────
    [[nodiscard]] static constexpr const char* descriptorType(VkDescriptorType type) noexcept {
        switch (type) {
            case VK_DESCRIPTOR_TYPE_SAMPLER:                    return "SAMPLER";
            case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:     return "COMBINED_IMAGE_SAMPLER";
            case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:              return "SAMPLED_IMAGE";
            case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:              return "STORAGE_IMAGE";
            case VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:       return "UNIFORM_TEXEL_BUFFER";
            case VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:       return "STORAGE_TEXEL_BUFFER";
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:             return "UNIFORM_BUFFER";
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:             return "STORAGE_BUFFER";
            case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:     return "UNIFORM_BUFFER_DYNAMIC";
            case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:     return "STORAGE_BUFFER_DYNAMIC";
            case VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:           return "INPUT_ATTACHMENT";
            case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR: return "ACCELERATION_STRUCTURE_KHR";
            default:                                            return "UNKNOWN";
        }
    }

    // ────────────────────── IMAGE LAYOUT → STRING ──────────────────────
    [[nodiscard]] static inline const char* imageLayout(VkImageLayout layout) noexcept {
        switch (layout) {
            case VK_IMAGE_LAYOUT_UNDEFINED:                                return "UNDEFINED (0)";
            case VK_IMAGE_LAYOUT_GENERAL:                                  return "GENERAL (1)";
            case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:                 return "COLOR_ATTACHMENT_OPTIMAL (2)";
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:         return "DEPTH_STENCIL_ATTACHMENT_OPTIMAL (3)";
            case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:          return "DEPTH_STENCIL_READ_ONLY_OPTIMAL (4)";
            case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:                 return "SHADER_READ_ONLY_OPTIMAL (5)";
            case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:                     return "TRANSFER_SRC_OPTIMAL (6)";
            case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:                     return "TRANSFER_DST_OPTIMAL (7)";
            case VK_IMAGE_LAYOUT_PREINITIALIZED:                           return "PREINITIALIZED (8)";
            case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL: return "DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL (9)";
            case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL: return "DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL (10)";
            case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:               return "STENCIL_ATTACHMENT_OPTIMAL (1000241000)";
            case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:                return "STENCIL_READ_ONLY_OPTIMAL (1000241001)";

            case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:                          return "PRESENT_SRC_KHR (1000001002)";
            case VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR:                     return "VIDEO_DECODE_DST_KHR (1000024000)";
            case VK_IMAGE_LAYOUT_VIDEO_DECODE_SRC_KHR:                     return "VIDEO_DECODE_SRC_KHR (1000024001)";
            case VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR:                     return "VIDEO_DECODE_DPB_KHR (1000024002)";
            case VK_IMAGE_LAYOUT_VIDEO_ENCODE_DST_KHR:                     return "VIDEO_ENCODE_DST_KHR (1000299000)";
            case VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR:                     return "VIDEO_ENCODE_SRC_KHR (1000299001)";
            case VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR:                     return "VIDEO_ENCODE_DPB_KHR (1000299002)";

            case VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR:                 return "RENDERING_LOCAL_READ_KHR (1000232000)";
            case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR:                   return "ATTACHMENT_OPTIMAL_KHR (1000241000)";
            case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR:                    return "READ_ONLY_OPTIMAL_KHR (1000241001)";

            default: {
                union {
                    VkImageLayout signedVal;
                    uint32_t unsignedVal;
                } l;
                l.signedVal = layout;
                if (layout < 0 || (l.unsignedVal & 0xFFFF0000u) == 0xCCCC0000u) {
                    return "GARBAGE/UNINITIALIZED";
                }

                static char buf[64];
                snprintf(buf, sizeof(buf), "UNKNOWN(%" PRIi64 " = 0x%" PRIx64 ")", (int64_t)layout, (uint64_t)layout);
                return buf;
            }
        }
    }

// ────────────────────── FATAL CHECK — FULL EXECUTION REPORT WITH AUTO-DETECT ──────────────────────
template <typename T, typename... Args>
static void checker(T value,
                    const char* call,
                    const char* msg,
                    Args&&... args,
                    std::source_location loc = std::source_location::current()) noexcept
{
    bool condition = false;
    std::string resultStr = "FAILED";

    if constexpr (std::is_same<T, VkResult>::value) {
        condition = (value == VK_SUCCESS || value == VK_SUBOPTIMAL_KHR);
        resultStr = std::format("{} ({})", result(value), static_cast<int>(value));
    }
    else if constexpr (std::is_pointer<T>::value) {
        condition = (value != nullptr);
        resultStr = (value == nullptr) ? "NULL" : "valid pointer";
    }
    else if constexpr (std::is_same<T, bool>::value) {
        condition = static_cast<bool>(value);
        resultStr = condition ? "true" : "false";
    }
    else if constexpr (std::is_integral<T>::value) {
        using UT = std::make_unsigned_t<T>;
        condition = (value != 0) && (static_cast<UT>(value) != ~UT(0));
        resultStr = std::to_string(value);

        if constexpr (std::is_signed<T>::value) {
            if (value < 0 && SDL_GetError) {
                resultStr += std::format(" (SDL error: {})", SDL_GetError());
            }
        }
    }
    else {
        condition = (value != 0);
        resultStr = "invalid (fallback check)";
    }

    if (condition) [[likely]] return;

    std::string formattedMsg = std::vformat(msg, std::make_format_args(args...));

    std::string guiltyFile = std::filesystem::path(loc.file_name()).filename().string();

    LOG_FATAL("\n"
              "════════════════════════════════════════════════════════════════\n"
              "CHECK FAILED — EXECUTION HALTED\n"
              "  Call site : {}\n"
              "  Condition : {} → {}\n"
              "  Message   : {}\n"
              "  Location  : {}:{}\n"
              "  Function  : {}\n"
              "════════════════════════════════════════════════════════════════",
              call,
              resultStr,
              condition ? "PASS" : "FAIL",
              formattedMsg,
              guiltyFile, loc.line(),
              loc.function_name());

#ifdef DEBUG
    std::abort();
#endif
}

    // debugCallback, alignUp, installCrashHandler unchanged
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

    [[nodiscard]] static constexpr uint64_t alignUp(uint64_t value, uint64_t alignment) noexcept {
        return alignment == 0 ? value : ((value + alignment - 1) / alignment) * alignment;
    }

    static void installCrashHandler() noexcept {
        install_apocalypse_handler();
    }
};

static constexpr Empire vkh{};

#define DEBUG_CALLBACK                  vkh.debugCallback