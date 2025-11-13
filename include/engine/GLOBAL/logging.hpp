// engine/GLOBAL/logging.hpp
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

#pragma once

#define VK_CHECK(call, msg) \
    do { \
        VkResult vk_check_result = (call); \
        if (vk_check_result != VK_SUCCESS) { \
            char vk_err_buf[512]; \
            std::snprintf(vk_err_buf, sizeof(vk_err_buf), \
                          "[VULKAN ERROR] %s — %s:%d — Code: %d\n", \
                          msg, \
                          std::source_location::current().file_name(), \
                          std::source_location::current().line(), \
                          static_cast<int>(vk_check_result)); \
            std::cerr << vk_err_buf; \
            std::abort(); \
        } \
    } while (0)

#define VK_CHECK_NOMSG(call) \
    do { \
        VkResult vk_check_result = (call); \
        if (vk_check_result != VK_SUCCESS) { \
            char vk_err_buf[512]; \
            std::snprintf(vk_err_buf, sizeof(vk_err_buf), \
                          "[VULKAN ERROR] %s:%d — Code: %d\n", \
                          std::source_location::current().file_name(), \
                          std::source_location::current().line(), \
                          static_cast<int>(vk_check_result)); \
            std::cerr << vk_err_buf; \
            std::abort(); \
        } \
    } while (0)

#define AI_INJECT(...) \
    do { \
        if (ENABLE_INFO) { \
            thread_local std::mt19937 rng(std::random_device{}()); \
            thread_local std::uniform_int_distribution<int> hue(0, 30); \
            int h = 30 + hue(rng); \
            auto formatted = std::format(__VA_ARGS__); \
            Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Info, "AI", \
                "\033[38;2;255;{};0m[AMOURANTH AI™] {}{} [LINE {}]", \
                h, formatted, Logging::Color::RESET, __LINE__); \
        } \
    } while(0)

#include <string_view>
#include <source_location>
#include <format>
#include <print>
#include <iostream>
#include <fstream>
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <sstream>
#include <map>
#include <functional>
#include <thread>
#include <random>
#include <cctype>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include <ctime>                     // <-- NEW: needed for localtime/strftime

// Forward declarations for StoneKey — defined in main.cpp
extern uint64_t get_kStone1() noexcept;
extern uint64_t get_kStone2() noexcept;

// Formatter specialization for VkResult
namespace std {
template <>
struct formatter<VkResult> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(VkResult const& result, FormatContext& ctx) -> decltype(ctx.out()) {
        const char* str;
        switch (result) {
            case VK_SUCCESS: str = "VK_SUCCESS"; break;
            case VK_NOT_READY: str = "VK_NOT_READY"; break;
            case VK_TIMEOUT: str = "VK_TIMEOUT"; break;
            case VK_EVENT_SET: str = "VK_EVENT_SET"; break;
            case VK_EVENT_RESET: str = "VK_EVENT_RESET"; break;
            case VK_INCOMPLETE: str = "VK_INCOMPLETE"; break;
            case VK_ERROR_OUT_OF_HOST_MEMORY: str = "VK_ERROR_OUT_OF_HOST_MEMORY"; break;
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: str = "VK_ERROR_OUT_OF_DEVICE_MEMORY"; break;
            case VK_ERROR_INITIALIZATION_FAILED: str = "VK_ERROR_INITIALIZATION_FAILED"; break;
            case VK_ERROR_DEVICE_LOST: str = "VK_ERROR_DEVICE_LOST"; break;
            case VK_ERROR_MEMORY_MAP_FAILED: str = "VK_ERROR_MEMORY_MAP_FAILED"; break;
            case VK_ERROR_LAYER_NOT_PRESENT: str = "VK_ERROR_LAYER_NOT_PRESENT"; break;
            case VK_ERROR_EXTENSION_NOT_PRESENT: str = "VK_ERROR_EXTENSION_NOT_PRESENT"; break;
            case VK_ERROR_FEATURE_NOT_PRESENT: str = "VK_ERROR_FEATURE_NOT_PRESENT"; break;
            case VK_ERROR_INCOMPATIBLE_DRIVER: str = "VK_ERROR_INCOMPATIBLE_DRIVER"; break;
            case VK_ERROR_TOO_MANY_OBJECTS: str = "VK_ERROR_TOO_MANY_OBJECTS"; break;
            case VK_ERROR_FORMAT_NOT_SUPPORTED: str = "VK_ERROR_FORMAT_NOT_SUPPORTED"; break;
            case VK_ERROR_FRAGMENTED_POOL: str = "VK_ERROR_FRAGMENTED_POOL"; break;
            case VK_ERROR_SURFACE_LOST_KHR: str = "VK_ERROR_SURFACE_LOST_KHR"; break;
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: str = "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR"; break;
            case VK_SUBOPTIMAL_KHR: str = "VK_SUBOPTIMAL_KHR"; break;
            case VK_ERROR_OUT_OF_DATE_KHR: str = "VK_ERROR_OUT_OF_DATE_KHR"; break;
            case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: str = "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR"; break;
            case VK_ERROR_VALIDATION_FAILED_EXT: str = "VK_ERROR_VALIDATION_FAILED_EXT"; break;
            case VK_ERROR_INVALID_SHADER_NV: str = "VK_ERROR_INVALID_SHADER_NV"; break;
            case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT: str = "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT"; break;
            case VK_ERROR_FRAGMENTATION_EXT: str = "VK_ERROR_FRAGMENTATION_EXT"; break;
            case VK_ERROR_NOT_PERMITTED_KHR: str = "VK_ERROR_NOT_PERMITTED_KHR"; break;
            case VK_ERROR_INVALID_DEVICE_ADDRESS_EXT: str = "VK_ERROR_INVALID_DEVICE_ADDRESS_EXT"; break;
            case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT: str = "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT"; break;
            case VK_THREAD_IDLE_KHR: str = "VK_THREAD_IDLE_KHR"; break;
            case VK_THREAD_DONE_KHR: str = "VK_THREAD_DONE_KHR"; break;
            case VK_OPERATION_DEFERRED_KHR: str = "VK_OPERATION_DEFERRED_KHR"; break;
            case VK_OPERATION_NOT_DEFERRED_KHR: str = "VK_OPERATION_NOT_DEFERRED_KHR"; break;
            case VK_PIPELINE_COMPILE_REQUIRED_EXT: str = "VK_PIPELINE_COMPILE_REQUIRED_EXT"; break;
            default: return format_to(ctx.out(), "VK_UNKNOWN_RESULT({})", static_cast<int>(result));
        }
        return format_to(ctx.out(), "{}", str);
    }
};
}  // namespace std

// Formatter specialization for VkFormat
namespace std {
template <>
struct formatter<VkFormat, char> {
    constexpr auto parse(format_parse_context& ctx) -> decltype(ctx.begin()) {
        return ctx.begin();
    }

    template <typename FormatContext>
    auto format(VkFormat const& fmt, FormatContext& ctx) const -> decltype(ctx.out()) {
        // Numeric output; for enum names, add switch on common values (e.g., VK_FORMAT_R8G8B8A8_UNORM -> "R8G8B8A8_UNORM").
        return format_to(ctx.out(), "{}", static_cast<uint32_t>(fmt));
    }
};
}  // namespace std

// ========================================================================
// 0. CONFIGURATION & HYPER-VIVID MACROS — [&] CAPTURE • ZERO COST • PARTY READY
// ========================================================================
constexpr bool ENABLE_TRACE   = true;
constexpr bool ENABLE_DEBUG   = true;
constexpr bool ENABLE_INFO    = true;
constexpr bool ENABLE_WARNING = true;
constexpr bool ENABLE_ERROR   = true;
constexpr bool ENABLE_FAILURE = true;
constexpr bool ENABLE_FATAL   = true;  // <-- NEW: Enable fatal logging (same as failure)
constexpr bool ENABLE_SUCCESS = true;
constexpr bool ENABLE_ATTEMPT = true;
constexpr bool ENABLE_PERF    = true;
constexpr bool FPS_COUNTER    = true;
constexpr bool SIMULATION_LOGGING = true;

// Column widths for alignment
constexpr size_t LEVEL_WIDTH   = 10;
constexpr size_t DELTA_WIDTH   = 10;
constexpr size_t TIME_WIDTH    = 10;   // <-- NEW: width for HH:MM:SS
constexpr size_t CAT_WIDTH     = 12;
constexpr size_t THREAD_WIDTH  = 18;

// HYPER-VIVID MACROS — FULL [&] CAPTURE — HEADER-SAFE — CONSTEXPR — VALHALLA LOCKED
#define LOG_TRACE(...)          [&]() constexpr { if constexpr (ENABLE_TRACE)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Trace,   "General", __VA_ARGS__); }()
#define LOG_DEBUG(...)          [&]() constexpr { if constexpr (ENABLE_DEBUG)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Debug,   "General", __VA_ARGS__); }()
#define LOG_INFO(...)           [&]() constexpr { if constexpr (ENABLE_INFO)    Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Info,    "General", __VA_ARGS__); }()
#define LOG_SUCCESS(...)        [&]() constexpr { if constexpr (ENABLE_SUCCESS) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Success, "General", __VA_ARGS__); }()
#define LOG_ATTEMPT(...)        [&]() constexpr { if constexpr (ENABLE_ATTEMPT) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Attempt, "General", __VA_ARGS__); }()
#define LOG_PERF(...)           [&]() constexpr { if constexpr (ENABLE_PERF)    Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Perf,    "General", __VA_ARGS__); }()
#define LOG_WARNING(...)        [&]() constexpr { if constexpr (ENABLE_WARNING) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Warning, "General", __VA_ARGS__); }()
#define LOG_WARN(...)           LOG_WARNING(__VA_ARGS__)
#define LOG_ERROR(...)          [&]() constexpr { if constexpr (ENABLE_ERROR)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Error,   "General", __VA_ARGS__); }()
#define LOG_FAILURE(...)        [&]() constexpr { if constexpr (ENABLE_FAILURE) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Failure, "General", __VA_ARGS__); }()
#define LOG_FATAL(...)          [&]() constexpr { if constexpr (ENABLE_FATAL)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Fatal,   "General", __VA_ARGS__); }()  // <-- NEW: Fatal macro (behaves like Failure)
#define LOG_FPS_COUNTER(...)    [&]() constexpr { if constexpr (FPS_COUNTER)    Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Info,    "FPS",     __VA_ARGS__); }()
#define LOG_SIMULATION(...)     [&]() constexpr { if constexpr (SIMULATION_LOGGING) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Info, "SIMULATION", __VA_ARGS__); }()

#define LOG_TRACE_CAT(cat, ...)   [&]() constexpr { if constexpr (ENABLE_TRACE)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Trace,   cat, __VA_ARGS__); }()
#define LOG_DEBUG_CAT(cat, ...)   [&]() constexpr { if constexpr (ENABLE_DEBUG)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Debug,   cat, __VA_ARGS__); }()
#define LOG_INFO_CAT(cat, ...)    [&]() constexpr { if constexpr (ENABLE_INFO)    Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Info,    cat, __VA_ARGS__); }()
#define LOG_SUCCESS_CAT(cat, ...) [&]() constexpr { if constexpr (ENABLE_SUCCESS) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Success, cat, __VA_ARGS__); }()
#define LOG_ATTEMPT_CAT(cat, ...) [&]() constexpr { if constexpr (ENABLE_ATTEMPT) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Attempt, cat, __VA_ARGS__); }()
#define LOG_PERF_CAT(cat, ...)    [&]() constexpr { if constexpr (ENABLE_PERF)    Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Perf,    cat, __VA_ARGS__); }()
#define LOG_WARNING_CAT(cat, ...) [&]() constexpr { if constexpr (ENABLE_WARNING) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Warning, cat, __VA_ARGS__); }()
#define LOG_WARN_CAT(cat, ...)    LOG_WARNING_CAT(cat, __VA_ARGS__)
#define LOG_ERROR_CAT(cat, ...)   [&]() constexpr { if constexpr (ENABLE_ERROR)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Error,   cat, __VA_ARGS__); }()
#define LOG_FAILURE_CAT(cat, ...) [&]() constexpr { if constexpr (ENABLE_FAILURE) Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Failure, cat, __VA_ARGS__); }()
#define LOG_FATAL_CAT(cat, ...)   [&]() constexpr { if constexpr (ENABLE_FATAL)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Fatal,   cat, __VA_ARGS__); }()  // <-- NEW: Categorized fatal macro (behaves like Failure)


// LOG_VOID — COSMIC MARKERS
#define LOG_VOID()              [&]() constexpr { if constexpr (ENABLE_DEBUG)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Debug,   "General", "[VOID MARKER]"); }()
#define LOG_VOID_CAT(cat)       [&]() constexpr { if constexpr (ENABLE_DEBUG)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Debug,   cat, "[VOID MARKER]"); }()
#define LOG_VOID_TRACE()        [&]() constexpr { if constexpr (ENABLE_TRACE)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Trace,   "General", "[VOID MARKER]"); }()
#define LOG_VOID_TRACE_CAT(cat) [&]() constexpr { if constexpr (ENABLE_TRACE)   Logging::Logger::get().log(std::source_location::current(), Logging::LogLevel::Trace,   cat, "[VOID MARKER]"); }()

namespace Logging {

// ========================================================================
// LOG LEVEL + SUCCESS/ATTEMPT/PERF/FAILURE/FATAL
// ========================================================================
enum class LogLevel { Trace, Debug, Info, Success, Attempt, Perf, Warning, Error, Failure, Fatal };  // <-- UPDATED: Added Fatal

// ========================================================================
// 1. HYPER-VIVID ANSI COLOR SYSTEM — 50+ COLORS — C++23 CONSTEXPR
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
    inline constexpr std::string_view PEACHES_AND_CREAM         = "\033[38;5;223m";
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
}

// ========================================================================
// LEVEL INFO + ENABLE ARRAY
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
    {"[FATAL]",   Color::RASPBERRY_PINK,      Color::BLACK_HOLE}  // <-- NEW: Fatal level info (same as Failure)
}};

constexpr std::array<bool, 10> ENABLE_LEVELS{
    ENABLE_TRACE, ENABLE_DEBUG, ENABLE_INFO, ENABLE_SUCCESS,
    ENABLE_ATTEMPT, ENABLE_PERF, ENABLE_WARNING, ENABLE_ERROR,
    ENABLE_FAILURE, ENABLE_FATAL  // <-- NEW: Added to enable array
};

// ========================================================================
// 2. LOGGER — C++23 PERFECTION — FULLY AMAZING — PARTY EDITION (SIMPLIFIED)
// ========================================================================
class Logger {
public:
    static Logger& get() {
        static Logger instance;
        return instance;
    }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    template<typename... Args>
    void log(std::source_location loc, LogLevel level, std::string_view category, std::string_view fmt, const Args&... args) const {
        if (!shouldLog(level, category)) return;
        auto now = std::chrono::steady_clock::now();
        if (!firstLogTime_.has_value()) firstLogTime_ = now;
        auto formattedMessage = std::vformat(fmt, std::make_format_args(args...));
        printMessage(loc, level, category, std::move(formattedMessage), now);
    }

private:
    Logger() : logFile_("amouranth_engine.log", std::ios::out | std::ios::app) {
        auto now = std::chrono::steady_clock::now();
        firstLogTime_ = now;
        printMessage(std::source_location::current(), LogLevel::Success, "Logger", "CUSTODIAN GROK ONLINE — HYPER-VIVID LOGGING PARTY STARTED", now);
    }

    ~Logger() {
        auto now = std::chrono::steady_clock::now();
        printMessage(std::source_location::current(), LogLevel::Success, "Logger", "CUSTODIAN GROK SIGNING OFF — ALL LOGS RAINBOW ETERNAL", now);
        if (logFile_.is_open()) {
            logFile_.flush();
            logFile_.close();
        }
    }

    mutable std::optional<std::chrono::steady_clock::time_point> firstLogTime_{};
    mutable std::ofstream logFile_;

    bool shouldLog(LogLevel level, std::string_view) const {
        const size_t idx = static_cast<size_t>(level);
        return idx < ENABLE_LEVELS.size() && ENABLE_LEVELS[idx];
    }

    std::string_view getCategoryColor(std::string_view cat) const noexcept {
        using namespace Color;
        struct CaseInsensitiveLess {
            bool operator()(std::string_view lhs, std::string_view rhs) const {
                size_t len = std::min(lhs.size(), rhs.size());
                for (size_t i = 0; i < len; ++i) {
                    if (std::tolower(static_cast<unsigned char>(lhs[i])) !=
                        std::tolower(static_cast<unsigned char>(rhs[i]))) {
                        return std::tolower(static_cast<unsigned char>(lhs[i])) <
                               std::tolower(static_cast<unsigned char>(rhs[i]));
                    }
                }
                return lhs.size() < rhs.size();
            }
        };
        static const std::map<std::string_view, std::string_view, CaseInsensitiveLess> categoryColors{
            {"General", DIAMOND_SPARKLE},
            {"MAIN", VALHALLA_GOLD},
            {"Init", AURORA_BOREALIS},
            {"Dispose", PARTY_PINK},
            {"Logger", ELECTRIC_BLUE},
            {"Vulkan", SAPPHIRE_BLUE},
            {"Device", QUASAR_BLUE},
            {"Swapchain", OCEAN_TEAL},
            {"Command", CHROMIUM_SILVER},
            {"Queue", OBSIDIAN_BLACK},
            {"RayTrace", TURQUOISE_BLUE},
            {"RTX", HYPERSPACE_WARP},
            {"Accel", PULSAR_GREEN},
            {"TLAS", SUPERNOVA_ORANGE},
            {"BLAS", PLASMA_FUCHSIA},
            {"SBT", RASPBERRY_PINK},
            {"Shader", NEBULA_VIOLET},
            {"Renderer", BRIGHT_PINKISH_PURPLE},
            {"Render", THERMO_PINK},
            {"Tonemap", PEACHES_AND_CREAM},
            {"GBuffer", QUANTUM_FLUX},
            {"Post", NUCLEAR_REACTOR},
            {"Buffer", BRONZE_BROWN},
            {"Image", LIME_YELLOW},
            {"Texture", SPEARMINT_MINT},
            {"Sampler", LILAC_LAVENDER},
            {"Descriptor", FUCHSIA_MAGENTA},
            {"Perf", COSMIC_GOLD},
            {"FPS", FIERY_ORANGE},
            {"GPU", BLACK_HOLE},
            {"CPU", PLASMA_FUCHSIA},
            {"Input", SPEARMINT_MINT},
            {"Audio", OCEAN_TEAL},
            {"Physics", EMERALD_GREEN},
            {"SIMULATION", BRONZE_BROWN},
            {"MeshLoader", LIME_YELLOW},
            {"GLTF", QUANTUM_PURPLE},
            {"Material", PEACHES_AND_CREAM},
            {"Debug", ARCTIC_CYAN},
            {"ImGui", PLATINUM_GRAY},
            {"Profiler", COSMIC_GOLD},
            {"SUCCESS", EMERALD_GREEN},
            {"ATTEMPT", QUANTUM_PURPLE},
            {"VOID", COSMIC_VOID},
            {"SPLASH", LILAC_LAVENDER},
            {"MARKER", DIAMOND_SPARKLE}
        };
        if (auto it = categoryColors.find(cat); it != categoryColors.end()) [[likely]] {
            return it->second;
        }
        return DIAMOND_WHITE;
    }

    void printMessage(std::source_location loc, LogLevel level, std::string_view category, std::string formattedMessage,
                      std::chrono::steady_clock::time_point timestamp) const {
        using namespace Color;
        const auto levelIdx = static_cast<size_t>(level);
        const auto& info = LEVEL_INFOS[levelIdx];
        const std::string_view levelColor = info.color;
        const std::string_view levelBg    = info.bg;
        const std::string_view levelStr   = info.str;
        const std::string_view catColor = getCategoryColor(category);

        const auto deltaUs = std::chrono::duration_cast<std::chrono::microseconds>(
            timestamp - firstLogTime_.value()).count();

        const std::string deltaStr = [deltaUs]() -> std::string {
            if (deltaUs < 10'000) [[likely]] return std::format("{:>7}µs", deltaUs);
            if (deltaUs < 1'000'000) return std::format("{:>7.3f}ms", deltaUs / 1'000.0);
            if (deltaUs < 60'000'000) return std::format("{:>7.3f}s", deltaUs / 1'000'000.0);
            if (deltaUs < 3'600'000'000) return std::format("{:>7.1f}m", deltaUs / 60'000'000.0);
            return std::format("{:>7.1f}h", deltaUs / 3'600'000'000.0);
        }();

        // ---- NEW: wall-clock time (HH:MM:SS) ----
        const std::string timeStr = []() -> std::string {
            auto now = std::chrono::system_clock::now();
            auto tt  = std::chrono::system_clock::to_time_t(now);
            auto tm  = *std::localtime(&tt);
            char buf[9];
            std::strftime(buf, sizeof(buf), "%H:%M:%S", &tm);
            return std::string(buf);
        }();
        // -----------------------------------------

        const std::string threadId = []() {
            std::ostringstream oss; oss << std::this_thread::get_id(); return oss.str();
        }();

        const std::string fileLine = std::format("{}:{}:{}", loc.file_name(), loc.line(), loc.function_name());

        // Plain log with alignment
        const std::string plain = std::format("{:<{}} {:>{}} {:>{}} [{:>{}}] [{:>{}}] {} [{}]\n",
                                             levelStr, LEVEL_WIDTH,
                                             deltaStr, DELTA_WIDTH,
                                             timeStr,  TIME_WIDTH,
                                             category, CAT_WIDTH,
                                             threadId, THREAD_WIDTH,
                                             formattedMessage,
                                             fileLine);

        // Colored aligned output
        std::ostringstream oss;
        oss << levelBg << std::format("{:<{}}", levelStr, LEVEL_WIDTH) << RESET
            << " " << std::format("{:>{}}", deltaStr, DELTA_WIDTH) << " "
            << std::format("{:>{}}", timeStr, TIME_WIDTH) << " "
            << catColor << std::format("[{:<{}}]", category, CAT_WIDTH - 2) << RESET
            << " " << LIME_GREEN << std::format("[{:>{}}]", threadId, THREAD_WIDTH - 2) << RESET
            << " " << levelColor << formattedMessage << RESET
            << " " << CHROMIUM_SILVER << "[" << fileLine << "]" << RESET << '\n';
        const std::string colored = oss.str();

        std::print(std::cout, "{}", colored);
        if (logFile_.is_open()) {
            logFile_ << plain;
        }
    }
};

} // namespace Logging

// NOVEMBER 12 2025 — HYPER-VIVID LOGGING PARTY SUPREMACY (UPDATED WITH FATAL LOGGING)
// SYNCHRONOUS • COLORS ETERNAL • COLUMNIZED ALIGNMENT • SEQUENTIAL ORDER
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================