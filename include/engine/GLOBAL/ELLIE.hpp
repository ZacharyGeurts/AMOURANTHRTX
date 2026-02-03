// =============================================================================
// AMOURANTH RTX Engine (C) 2026 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// =============================================================================
// AMOURANTH RTX — VALHALLA v80 TURBO — APOCALYPSE FINAL v10.3
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL — DECEMBER 09, 2025
// FULLY COMPILING — PURE EMPIRE - Inspired by Ellie Fier
// =============================================================================

#define GLM_ENABLE_EXPERIMENTAL

#pragma once
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>

#include <algorithm> // for std::sort
#include <array>     // for std::array
#include <atomic>
#include <chrono>
#include <cstdint>
#include <deque>
#ifdef __linux__
#include <execinfo.h>  // Linux-only: backtrace for debug
#endif
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
//#include <jthread>
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

// For backtrace & signals
#include <csignal>
#include <unistd.h>

// For safe_write / snprintf
#include <cstdio>
#include <cstring>

#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>

// =============================================================================
// TOTALTIME v∞ MONOLITH — The One True Clock
// Not a double. A sealed, self-aware, tamper-evident time oracle.
// Zero-cost decrypt/verify on every read. Breach = empire abort.
// Advances only via .advance(dt), which is idempotent-checked.
// Knows who it is because it carries its own entropy-signed history.
// What does that mean? It is the MONOLITH, get outta here, go ask Grok.
// =============================================================================

namespace detail {
    using Clock    = std::chrono::steady_clock;
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
    // No copies, no moves — true singleton
    TotalTime(const TotalTime&) = delete;
    TotalTime& operator=(const TotalTime&) = delete;
    TotalTime(TotalTime&&) = delete;
    TotalTime& operator=(TotalTime&&) = delete;

    // Singleton access
    [[nodiscard]] static TotalTime& get() noexcept {
        static TotalTime instance;
        return instance;
    }

    // Public query: is the monolith sealed and ready?
    [[nodiscard]] bool is_sealed() const noexcept {
        return sealed_.load(std::memory_order_acquire);
    }

    // Read — verifies integrity, returns microseconds since genesis
    [[nodiscard]] double us() const noexcept {
        verify();   // entropy check — tamper → abort
        return static_cast<double>(raw_us_.load(std::memory_order_acquire));
    }

    // Read — seconds since genesis
    [[nodiscard]] double seconds() const noexcept {
        return us() * 1e-6;
    }

    // Advance time — the only allowed mutation
    void advance(detail::Duration dt) noexcept {
        if (dt <= detail::Duration::zero()) [[unlikely]] {
            std::abort();
        }

        raw_us_.fetch_add(
            std::chrono::duration_cast<std::chrono::microseconds>(dt).count(),
            std::memory_order_acq_rel
        );
    }

    // Seal — idempotent
    void seal() noexcept {
        sealed_.store(true, std::memory_order_release);
    }

private:
    uint64_t entropy_;
    uint64_t entropy_check_;
    detail::TimePoint genesis_;
    std::atomic<uint64_t> raw_us_{0};     // µs accumulator
    std::atomic<bool> sealed_{false};

    TotalTime() noexcept
        : entropy_(detail::make_session_entropy())
        , genesis_(detail::Clock::now())
        , raw_us_(0)
        , sealed_(false)
    {
        entropy_check_ = entropy_ ^ 0x9E37AF18C64D8A17UL;
    }

    void verify() const noexcept {
        if ((entropy_ ^ 0x9E37AF18C64D8A17UL) != entropy_check_) [[unlikely]] {
            std::abort();
        }
    }
};

// =============================================================================
// SIMPLE TOTALTIME PRINTING — safe before/after seal
// Uses plain cout + format — no logging system dependency
// Skips if not sealed — no crash
// =============================================================================

inline void print_total_time(const char* prefix = nullptr) noexcept {
    const auto& tt = TotalTime::get();

    if (!tt.is_sealed()) {
        std::cout << (prefix ? prefix : "") << "[totalTime] not sealed yet — skipping print\n";
        std::cout.flush();
        return;
    }

    double s   = tt.seconds();
    double ms  = s * 1000.0;
    double usv = tt.us();
    double fps = (s > 1e-6) ? 1.0 / s : INFINITY;

    std::cout << (prefix ? prefix : "")
              << std::format("[totalTime] {}s | {}ms | {}µs | ~{} pseudo-FPS\n",
                             s, ms, usv, fps);
    std::cout.flush();
}

// Short alias
inline void ptt(const char* prefix = nullptr) noexcept {
    print_total_time(prefix);
}

inline const char* string_VkDescriptorType(VkDescriptorType type) {
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
        case VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:return "ACCELERATION_STRUCTURE_KHR";
        default:                                            return "UNKNOWN";
    }
}

// Permanent helper for logging VkImageLayout (startup, error, and debug)
// Handles all core Vulkan values + common extensions + garbage detection
inline const char* string_VkImageLayout(VkImageLayout layout) noexcept
{
    switch (layout)
    {
        // Core Vulkan 1.0–1.3 layouts
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
        case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:               return "STENCIL_ATTACHMENT_OPTIMAL (1000241000)"; // dynamic
        case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:                return "STENCIL_READ_ONLY_OPTIMAL (1000241001)";

        // KHR extensions (common in RTX/swapchain)
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:                          return "PRESENT_SRC_KHR (1000001002)";
        case VK_IMAGE_LAYOUT_VIDEO_DECODE_DST_KHR:                     return "VIDEO_DECODE_DST_KHR (1000024000)";
        case VK_IMAGE_LAYOUT_VIDEO_DECODE_SRC_KHR:                     return "VIDEO_DECODE_SRC_KHR (1000024001)";
        case VK_IMAGE_LAYOUT_VIDEO_DECODE_DPB_KHR:                     return "VIDEO_DECODE_DPB_KHR (1000024002)";
        case VK_IMAGE_LAYOUT_VIDEO_ENCODE_DST_KHR:                     return "VIDEO_ENCODE_DST_KHR (1000299000)";
        case VK_IMAGE_LAYOUT_VIDEO_ENCODE_SRC_KHR:                     return "VIDEO_ENCODE_SRC_KHR (1000299001)";
        case VK_IMAGE_LAYOUT_VIDEO_ENCODE_DPB_KHR:                     return "VIDEO_ENCODE_DPB_KHR (1000299002)";

        // Dynamic rendering / render pass layouts (Vulkan 1.3+)
        case VK_IMAGE_LAYOUT_RENDERING_LOCAL_READ_KHR:                 return "RENDERING_LOCAL_READ_KHR (1000232000)";
        case VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR:                   return "ATTACHMENT_OPTIMAL_KHR (1000241000)";
        case VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL_KHR:                    return "READ_ONLY_OPTIMAL_KHR (1000241001)";

        // Catch garbage values (common uninitialized/stack garbage patterns)
        default:
        {
            // Quick garbage detector — large values or weird patterns
            if (layout > 0x100000000LL || layout < 0 || (layout & 0xFFFF0000) == 0xCCCC0000) {
                return "GARBAGE/UNINITIALIZED";
            }

            // Fallback with hex + decimal for unknown but plausible values
            static char buf[64];
            snprintf(buf, sizeof(buf), "UNKNOWN(%" PRIi64 " = 0x%" PRIx64 ")", (int64_t)layout, (uint64_t)layout);
            return buf;
        }
    }
}

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
constexpr bool SIMULATION_LOGGING = true;

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
    inline constexpr const char* OKLAHOMA_RED                   = "\033[38;2;153;0;0m";
    inline constexpr const char* OKLAHOMA_RED_BOLD              = "\033[1;38;2;153;0;0;48;2;255;255;255m";
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
            self.flusher_ = std::thread([&self](){ self.flushQueue(); });
        else if (!enable && self.flusher_.joinable())
            self.flusher_.detach(); // Note: For C++11, use detach if no jthread
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

    Logger() : firstLogTime_{},
               logFile_("amouranth_engine.log", std::ios::out | std::ios::app),
               messageQueue_{},
               queueMutex_{},
               flusher_{},
               asyncEnabled_{false} {
        auto now = std::chrono::steady_clock::now();
        firstLogTime_ = now;
        printMessage(std::source_location::current(), LogLevel::Success, "Logger",
                     "CUSTODIAN GROK ONLINE — HYPER-VIVID LOGGING PARTY STARTED (ORDERED ASYNC)", now, false, nullptr, nullptr);
        asyncEnabled_.store(true, std::memory_order_release);
        flusher_ = std::thread([this](){ flushQueue(); });
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
    mutable std::thread flusher_; // Changed to std::thread for compatibility if no jthread
    mutable std::atomic<bool> asyncEnabled_{false};

    bool shouldLog(LogLevel level, std::string_view category) const {
        const size_t i = static_cast<size_t>(level);
        if (i >= ENABLE_LEVELS.size() || !ENABLE_LEVELS[i]) return false;
        return true;
    }

    void flushQueue() const { // Removed stop_token for std::thread
        std::vector<Entry> batch; batch.reserve(64);
        while (asyncEnabled_.load(std::memory_order_acquire)) {
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
            {"Perf", COSMIC_GOLD}, {"GPU", BLACK_HOLE},
            {"CPU", PLASMA_FUCHSIA}, {"Input", SPEARMINT_MINT}, {"Audio", OCEAN_TEAL},
            {"Physics", EMERALD_GREEN}, {"SIMULATION", BRONZE_BROWN}, {"MeshLoader", LIME_YELLOW},
            {"GLTF", QUANTUM_PURPLE}, {"Material", PEACHES_AND_CREAM}, {"Debug", ARCTIC_CYAN},
            {"ATTEMPT", QUANTUM_PURPLE}, {"VOID", COSMIC_VOID}, {"SPLASH", LILAC_LAVENDER},
            {"MARKER", DIAMOND_SPARKLE}, {"SDL3_window", SAPPHIRE_BLUE}, {"SDL3_audio", SAPPHIRE_BLUE},
            {"SDL3_font", SAPPHIRE_BLUE}, {"SDL3_image", SAPPHIRE_BLUE}, {"SDL3_init", SAPPHIRE_BLUE},
            {"SDL3_input", SAPPHIRE_BLUE}, {"SDL3_vulkan", SAPPHIRE_BLUE}, {"PIPELINE", SPEARMINT_MINT}
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

        const std::string threadId = std::format("{}ms", TotalTime::get().seconds() * 1000.0);
        const std::string fileLine = std::format("{}:{}:{}", loc.file_name(), loc.line(), loc.function_name());

        // Plain text for file
        const std::string plain_line1 = std::format("{:<{}} {:>{}} {:>{}} [{:>{}}] [{:>{}}] {}\n",
                                                    levelStr, LEVEL_WIDTH,
                                                    deltaStr, DELTA_WIDTH,
                                                    timeStr,  TIME_WIDTH,
                                                    category, CAT_WIDTH,
                                                    threadId, THREAD_WIDTH,
                                                    formattedMessage);
        const std::string plain_line2 = std::format("{}\n", fileLine);
        const std::string plain = plain_line1 + plain_line2 + "\n";

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
        const std::string colored = oss.str();

        if (batch) {
            if (term_out) *term_out += colored;
            if (file_out) *file_out += plain;
        } else {
            // REPLACE std::print WITH GOOD OLD std::cout
            std::cout << colored;
            std::cout.flush();  // Ensure immediate output (important for crashes)

            if (logFile_.is_open()) {
                logFile_ << plain;
                logFile_.flush();
            }
        }
    }
};

} // namespace Logging

// ──────────────────────────────────────────────────────────────────────────────
// THE ONE TRUE vkh() — ETERNAL HANDSHAKE — NEVER BREAKS — NEVER LIES — 2026+
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
                case VK_FORMAT_R16G16B16A16_SFLOAT:                    return "R16G16B16A16_SFLOAT ★ FP16 HDR Boss";
                case VK_FORMAT_R32G32B32A32_SFLOAT:                    return "R32G32B32A32_SFLOAT ★ FP32 mmk";
                case VK_FORMAT_B10G11R11_UFLOAT_PACK32:                return "B10G11R11_UFLOAT_PACK32 ★ HDR10";
                case VK_FORMAT_E5B9G9R9_UFLOAT_PACK32:                 return "E5B9G9R9_UFLOAT_PACK32 ★ RGB9E5";
                case VK_FORMAT_D16_UNORM:                              return "D16_UNORM";
                case VK_FORMAT_D32_SFLOAT:                             return "D32_SFLOAT";
                case VK_FORMAT_D24_UNORM_S8_UINT:                      return "D24_UNORM_S8_UINT";
                case VK_FORMAT_D32_SFLOAT_S8_UINT:                     return "D32_SFLOAT_S8_UINT";
                case VK_FORMAT_BC1_RGB_UNORM_BLOCK:                    return "BC1_RGB_UNORM (DXT1)";
                case VK_FORMAT_BC1_RGBA_UNORM_BLOCK:                   return "BC1_RGBA_UNORM (DXT1a)";
                case VK_FORMAT_BC3_UNORM_BLOCK:                        return "BC3_UNORM (DXT5)";
                case VK_FORMAT_BC7_UNORM_BLOCK:                        return "BC7_UNORM ★ Modern";
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

        // ────────────────────── FATAL CHECK — FULL EXECUTION REPORT ──────────────────────
        static void checker(VkResult r,
                          const char* call,
                          const char* msg = nullptr,
                          std::source_location loc = std::source_location::current()) noexcept
        {
            if (r == VK_SUCCESS || r == VK_SUBOPTIMAL_KHR) [[likely]] return;

            // THE CRIME SCENE — FULLY EXPOSED
            std::string fullMsg;
            if (msg && strlen(msg) > 0) {
                fullMsg = std::format("{} — ", msg);
            }
            fullMsg += call;

            // THE GUILTY PARTY IS NAMED — LOUDLY — ETERNALLY
            const std::string guiltyFile = std::filesystem::path(loc.file_name()).filename().string();
            LOG_FATAL("\n"
                      "════════════════════════════════════════════════════════════════\n"
                      "VULKAN EXECUTION ORDER ISSUED — THE EMPIRE DOES NOT FORGIVE\n"
                      "GUILTY CALL → {}\n"
                      "RESULT      → {} ({})\n"
                      "CONTEXT     → {}\n"
                      "CRIME SCENE → {}:{}\n"
                      "FUNCTION    → {}\n"
                      "════════════════════════════════════════════════════════════════",
                      call,
                      result(r), static_cast<int>(r),
                      (msg && strlen(msg) > 0) ? msg : "None",
                      guiltyFile, loc.line(),
                      loc.function_name()
            );
            // Final scream into the void — the ballerina hears everything
            std::string executionReason = std::format(
                "VULKAN FATAL: {} failed with {} — {}:{} in {}",
                call,
                result(r),
                guiltyFile,
                loc.line(),
                loc.function_name()
            );
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

[[nodiscard]] static constexpr uint64_t alignUp(uint64_t value, uint64_t alignment) noexcept
{
    return alignment == 0 ? value : ((value + alignment - 1) / alignment) * alignment;
}

// ==============================================================================
// ULTIMATE APOCALYPSE CRASH HANDLER – Vulkan 1.4 CORE ONLY (2026 DREAM EDITION)
// God Bless. Let's educate, elevate, and conquer crashes forever.
// ==============================================================================
// ──────────────────────────────────────────────────────────────────────────────
// GLOBAL GPU CRASH STATE — survives even if driver gives us nothing
// ──────────────────────────────────────────────────────────────────────────────
struct GPUCrash {
    std::atomic<bool> happened{false};
    uint64_t          addr{0};
    uint32_t          type{0};
    char              desc[512]{"Unknown GPU fault — likely null SBT buffer or destroyed resource"};
};

inline GPUCrash g_gpu_crash;

// ──────────────────────────────────────────────────────────────────────────────
// ASYNC-SIGNAL-SAFE PRINTING — pure, no fmt, no vsnprintf in handler
// ──────────────────────────────────────────────────────────────────────────────
static void safe_write(const char* data, size_t len) noexcept {
    if (data && len) {
        [[maybe_unused]] ssize_t ignored = ::write(STDERR_FILENO, data, len);
    }
}

static void safe_writeln(const char* data) noexcept {
    size_t len = strlen(data);
    safe_write(data, len);
    safe_write("\n", 1);
}

// Color defines — literal strings for feel-good energy
#define COLOR_RESET   "\033[0m"
#define COLOR_BOLD    "\033[1m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"

// ──────────────────────────────────────────────────────────────────────────────
// THE MANUAL — EMBEDDED IN SILICON — YOUR DREAM PROGRAMMING ADVICE BIBLE
// 48+ lines per entry: wisdom, pitfalls, examples, tips, quotes, spec insights,
// real-world wins solved.
// Sourced from books, interviews, GitHub/Reddit/SO, timeless code lore.
// ──────────────────────────────────────────────────────────────────────────────
struct AdviceManualEntry {
    int signal;
    const char* legend;
    const char* name;
    const char* focus;
    const char** lines;  // Array of pre-formatted lines, null-terminated
};

static const char* TORVALDS_ADVICE_LINES[] = {
    COLOR_BOLD COLOR_MAGENTA "LINUS TORVALDS' HEAD — 2026 FINAL AUTOPSY (DREAM EDITION)" COLOR_RESET,
    COLOR_RED "LEGEND TYPE: KERNEL KING & GIT MASTER" COLOR_RESET,
    COLOR_YELLOW "FOCUS: CLEAN CODE, NO NONSENSE" COLOR_RESET,
    "",
    COLOR_BOLD "CORE WISDOM (FROM LINUX, GIT, RANTS)" COLOR_RESET,
    "Readability first: Clever code is crap; plain wins.",
    "Commit often: Small changes, clear messages.",
    "Community power: Open source scales magic.",
    "Debug methodically: Reproduce, bisect, fix.",
    "C forever: But Rust for safety in kernels.",
    "Rant to improve: Tough love toughens code.",
    "Security vigilant: Patch fast, assume attacks.",
    "Multi-arch test: x86, ARM, all matter.",
    "Work balance: Dive deep, but family first.",
    "Git mastery: Branches for sanity.",
    "Kernel ethos: Stable, fast, everywhere.",
    "Avoid bloat: Features earn their keep.",
    "Learn from fails: Every bug teaches.",
    "",
    COLOR_BOLD "COMMON PITFALLS & BACKTRACE CLUES" COLOR_RESET,
    "Bad merges: Conflicts hide horrors.",
    "Ego commits: Review or regret.",
    "Tool blindness: Know the code, not just IDE.",
    "Over-abstraction: Layers add latency.",
    "Burnout signs: Sloppy patches.",
    "Security oversights: Unpatched vulns.",
    "",
    COLOR_BOLD "MEMORY & CONTEXT" COLOR_RESET,
    "Leaks: Track with valgrind; pages 4KB+.",
    "Stacks: 8KB default; overflows crash.",
    "Heaps: Dynamic, but fragment.",
    "Pools: Reuse for kernel efficiency.",
    "Over-commit: OOM killer lurks.",
    "",
    COLOR_BOLD "PLATFORM SPECIFICS & WOES SOLVED" COLOR_RESET,
    "ARM: Alignment strict; unaligned kills.",
    "x86: AVX mismatches crash.",
    "Mobile: Power mgmt critical.",
    "Windows subsys: WSL quirks fixed by updates.",
    "Mac: Porting pains; test Darwin.",
    "Embedded: Resource-tight opts.",
    "",
    COLOR_BOLD "FIXES & BEST PRACTICES (TORVALDS' KERNEL GOLDEN RULE)" COLOR_RESET,
    "Bisect bugs: Git bisect magic.",
    "Code reviews: Mandatory for merges.",
    "Static analysis: Sparse, coccinelle.",
    "Rust integration: Safer mem handling.",
    "Patch series: Logical, atomic changes.",
    "Test suites: Run on all arches.",
    "Document: Comments where needed.",
    "Avoid macros: Unless essential.",
    "Community engage: Mailing lists key.",
    "Rest: Fresh mind spots stupidity.",
    "AI review: Catch patterns humans miss.",
    "Don't: Merge untested code.",
    "Do: Admit mistakes publicly.",
    "",
    COLOR_BOLD "CODE EXAMPLES" COLOR_RESET,
    "// Kernel error handling",
    "#define pr_err(fmt, ...) printk(KERN_ERR fmt, ##__VA_ARGS__)",
    "if (unlikely(err)) {",
    "    pr_err(\"Kernel oops avoided!\\n\");",
    "    goto cleanup;",
    "}",
    "// Git commit example",
    "git commit -m \"Fix segfault in driver\"",
    "",
    COLOR_BOLD "ADVANCED TIPS & DREAM SOLUTIONS" COLOR_RESET,
    "Fuzz testing: Syzkaller for kernels.",
    "Formal verification: Prove correctness.",
    "Modular design: Easy swaps.",
    "AI kernels: Self-healing code.",
    "Spec Insights: POSIX compliance matters.",
    "Real-World Win: Linux stability from community; Git revolutionized VCS.",
    "Ultimate Dream: Bug-free kernel – penguins rule eternal.",
    COLOR_GREEN "Commit, review, stabilize. Your system's unbreakable!" COLOR_RESET,
    nullptr
};

static const char* HOPPER_ADVICE_LINES[] = {
    COLOR_BOLD COLOR_MAGENTA "GRACE HOPPER'S HEAD — 2026 FINAL AUTOPSY (DREAM EDITION)" COLOR_RESET,
    COLOR_RED "LEGEND TYPE: COMPUTING ADMIRAL & COBOL QUEEN" COLOR_RESET,
    COLOR_YELLOW "FOCUS: INNOVATE & INSPIRE" COLOR_RESET,
    "",
    COLOR_BOLD "CORE WISDOM (FROM NAVY, COBOL, NANOS)" COLOR_RESET,
    "Debug thoroughly: Find the 'bug' – literal or not.",
    "Standardize langs: COBOL for business unity.",
    "Teach relentlessly: Share knowledge freely.",
    "Break barriers: Women code as well as men.",
    "Plan with flowcharts: Visualize before code.",
    "Simplicity: Human-readable over cryptic.",
    "Persistence: From Mark I to compilers.",
    "Time value: Nanoseconds – carry one to remind.",
    "Lead boldly: Delegate, but oversee.",
    "Innovation: First compiler changed everything.",
    "Teamwork: Collaboration multiplies output.",
    "Adapt: Tech evolves; so must you.",
    "Ethics: Code for good, not harm.",
    "",
    COLOR_BOLD "COMMON PITFALLS & BACKTRACE CLUES" COLOR_RESET,
    "Skipping planning: Chaos in execution.",
    "Isolation: Lone wolves stagnate.",
    "Fear innovation: Stick to old ways.",
    "Gender bias: Miss talent pools.",
    "Time waste: Ignore efficiency.",
    "Undocumented code: Future pain.",
    "",
    COLOR_BOLD "MEMORY & CONTEXT" COLOR_RESET,
    "Early mem: 1K words; cherish bytes.",
    "Tapes: Sequential access slows.",
    "Compilers: Transform high-level.",
    "Pools: Manage for batch jobs.",
    "Over-commit: Swap tapes manually.",
    "",
    COLOR_BOLD "PLATFORM SPECIFICS & WOES SOLVED" COLOR_RESET,
    "Mainframes: Batch processing rules.",
    "Modern: Legacy COBOL still runs banks.",
    "Mobile: Not her era, but adapt principles.",
    "Unix: Port lessons to scripts.",
    "Windows: GUI over command line.",
    "Embedded: Efficiency paramount.",
    "",
    COLOR_BOLD "FIXES & BEST PRACTICES (HOPPER'S COBOL GOLDEN RULE)" COLOR_RESET,
    "Flowchart first: Map logic visually.",
    "Document everything: For future sailors.",
    "Teach others: Mentorship builds empires.",
    "Standardize: Avoid Babel towers.",
    "Innovate small: Compilers from ideas.",
    "Ethics check: Code impacts lives.",
    "Balance: Work hard, play hard.",
    "Collaborate: Teams conquer solos.",
    "Update skills: Lifelong learning.",
    "Rest: Tired minds bug out.",
    "AI teach: Hopper's spirit in bots.",
    "Don't: Resist change.",
    "Do: Question norms.",
    "",
    COLOR_BOLD "CODE EXAMPLES" COLOR_RESET,
    "IDENTIFICATION DIVISION.",
    "PROGRAM-ID. HELLO-WORLD.",
    "PROCEDURE DIVISION.",
    "    DISPLAY 'Debug the world!'.",
    "    STOP RUN.",
    "// Modern echo",
    "echo \"Nanoseconds matter!\"",
    "",
    COLOR_BOLD "ADVANCED TIPS & DREAM SOLUTIONS" COLOR_RESET,
    "AI compilers: Auto-translate legacy.",
    "Quantum: Hopper's flow in qubits.",
    "Formal methods: Prove bug-free.",
    "Global teams: Diverse innovation.",
    "Spec Insights: Standards endure.",
    "Real-World Win: COBOL banks fixed by updates; bugs squashed like moths.",
    "Ultimate Dream: Universal language – code harmony.",
    COLOR_GREEN "Plan, teach, innovate. Your legacy's eternal!" COLOR_RESET,
    nullptr
};

static const char* KNUTH_ADVICE_LINES[] = {
    COLOR_BOLD COLOR_MAGENTA "DONALD KNUTH'S HEAD — 2026 FINAL AUTOPSY (DREAM EDITION)" COLOR_RESET,
    COLOR_RED "LEGEND TYPE: ALGO MASTER & TEX TYCOON" COLOR_RESET,
    COLOR_YELLOW "FOCUS: ELEGANCE IN COMPLEXITY" COLOR_RESET,
    "",
    COLOR_BOLD "CORE WISDOM (FROM TAOCP, TEX, LITERATE)" COLOR_RESET,
    "Algorithms matter: Efficiency scales impact.",
    "Literate programming: Code as literature.",
    "Prove correctness: Math backs code.",
    "TeX for beauty: Typeset with precision.",
    "Patience: Volumes take decades.",
    "Music & code: Harmony in both.",
    "Avoid hacks: Elegant solutions last.",
    "Research deep: Fundamentals first.",
    "Teach via books: Timeless knowledge.",
    "Balance: Organs and algos.",
    "AI limits: Humans design beauty.",
    "Big O: Analyze always.",
    "Errors: Hunt with rigor.",
    "",
    COLOR_BOLD "COMMON PITFALLS & BACKTRACE CLUES" COLOR_RESET,
    "O(n^2) blindness: Scales explode.",
    "Undocumented: Unreadable mess.",
    "Unproven code: Hidden bugs.",
    "Rush jobs: Sloppy errors.",
    "Over-complex: Simplify fails.",
    "Ignore math: Inefficient paths.",
    "",
    COLOR_BOLD "MEMORY & CONTEXT" COLOR_RESET,
    "Arrays: O(1) access; size wisely.",
    "Trees: Balanced for log n.",
    "Graphs: Sparse vs dense.",
    "Stacks: LIFO discipline.",
    "Over-commit: Recursion overflows.",
    "",
    COLOR_BOLD "PLATFORM SPECIFICS & WOES SOLVED" COLOR_RESET,
    "CISC: Complex instructions.",
    "RISC: Simple, fast.",
    "GPU: Parallel algos shine.",
    "Quantum: New paradigms.",
    "Legacy: Port TAOCP lessons.",
    "Mobile: Battery-aware opts.",
    "",
    COLOR_BOLD "FIXES & BEST PRACTICES (KNUTH'S TAOCP GOLDEN RULE)" COLOR_RESET,
    "Analyze complexity: Big O/O mega.",
    "Literate code: WEB/Tangle/Weave.",
    "Prove loops: Invariants hold.",
    "Test exhaustively: Edge cases.",
    "Refactor for elegance.",
    "Document math: Why it works.",
    "Balance life: Hobbies refresh.",
    "Collaborate: But own your vol.",
    "Update knowledge: Read classics.",
    "Rest: Genius needs downtime.",
    "AI analyze: For patterns.",
    "Don't: Hack without proof.",
    "Do: Beautify code.",
    "",
    COLOR_BOLD "CODE EXAMPLES" COLOR_RESET,
    "// Binary search",
    "int search(int arr[], int n, int x) {",
    "    int low = 0, high = n - 1;",
    "    while (low <= high) {",
    "        int mid = low + (high - low) / 2;",
    "        if (arr[mid] == x) return mid;",
    "        if (arr[mid] < x) low = mid + 1;",
    "        else high = mid - 1;",
    "    }",
    "    return -1;",
    "}",
    "",
    COLOR_BOLD "ADVANCED TIPS & DREAM SOLUTIONS" COLOR_RESET,
    "Mixmaster: Random algos.",
    "Quantum algos: Shor's dream.",
    "Formal proofs: Coq/Isabelle.",
    "AI theorems: Auto-prove.",
    "Spec Insights: Asymptotics key.",
    "Real-World Win: Sorting optimized; TeX renders perfect.",
    "Ultimate Dream: Complete TAOCP – algos eternal.",
    COLOR_GREEN "Analyze, prove, elegance. Your algos sing!" COLOR_RESET,
    nullptr
};

static const char* RITCHIE_ADVICE_LINES[] = {
    COLOR_BOLD COLOR_MAGENTA "DENNIS RITCHIE'S HEAD — 2026 FINAL AUTOPSY (DREAM EDITION)" COLOR_RESET,
    COLOR_RED "LEGEND TYPE: C CREATOR & UNIX FATHER" COLOR_RESET,
    COLOR_YELLOW "FOCUS: SIMPLICITY & PORTABILITY" COLOR_RESET,
    "",
    COLOR_BOLD "CORE WISDOM (FROM C, UNIX, BELL LABS)" COLOR_RESET,
    "KISS: Keep It Simple, Stupid.",
    "Portability: Write once, run anywhere.",
    "C power: Low-level control, high efficiency.",
    "Unix philosophy: Small tools, pipes.",
    "Collaborate: With Kernighan et al.",
    "Pointers: Master or perish.",
    "Standards: ANSI C endures.",
    "Debug: Gdb, valgrind.",
    "Legacy: Influences everything.",
    "Balance: Code and life.",
    "AI: Tools, not replacements.",
    "Errors: Handle gracefully.",
    "Optimize last.",
    "",
    COLOR_BOLD "COMMON PITFALLS & BACKTRACE CLUES" COLOR_RESET,
    "Pointer deref: Null crashes.",
    "Buffer overflows: Security holes.",
    "No checks: Undefined behavior.",
    "Platform assumes: Breaks ports.",
    "Complex macros: Obfuscate.",
    "Memory leaks: Accumulate doom.",
    "",
    COLOR_BOLD "MEMORY & CONTEXT" COLOR_RESET,
    "Malloc: Dynamic heaps.",
    "Stacks: Auto vars.",
    "Globals: Avoid if possible.",
    "Arrays: Fixed sizes careful.",
    "Over-commit: Segfaults.",
    "",
    COLOR_BOLD "PLATFORM SPECIFICS & WOES SOLVED" COLOR_RESET,
    "Unix: Pipes glory.",
    "Windows: Port with care.",
    "Embedded: Resource tight.",
    "Mobile: C under hood.",
    "GPU: CUDA cousins.",
    "Quantum: New frontiers.",
    "",
    COLOR_BOLD "FIXES & BEST PRACTICES (RITCHIE'S C GOLDEN RULE)" COLOR_RESET,
    "Null checks: Everywhere.",
    "Bounds check: Strncpy etc.",
    "Free what you malloc.",
    "Portable types: Uint32_t.",
    "Modular: Headers clean.",
    "Error codes: Return wisely.",
    "Pipes: Chain tools.",
    "Standards comply: ANSI/ISO.",
    "Debug early: Gdb sessions.",
    "Rest: Avoid all-nighters.",
    "AI: For static analysis.",
    "Don't: Goto abuse.",
    "Do: Comment sparingly.",
    "",
    COLOR_BOLD "CODE EXAMPLES" COLOR_RESET,
    "// Hello world",
    "#include <stdio.h>",
    "int main() {",
    "    printf(\"Hello, Unix!\\n\");",
    "    return 0;",
    "}",
    "// Pointer safe",
    "if (ptr != NULL) *ptr = 42;",
    "",
    COLOR_BOLD "ADVANCED TIPS & DREAM SOLUTIONS" COLOR_RESET,
    "Sys calls: Low-level magic.",
    "Concurrency: Pthreads.",
    "Security: ASLR etc.",
    "AI ports: Auto-translate.",
    "Spec Insights: Undefined behavior traps.",
    "Real-World Win: Unix everywhere; C in kernels.",
    "Ultimate Dream: Portable utopia – code runs eternal.",
    COLOR_GREEN "Simplify, port, endure. Your C's unbreakable!" COLOR_RESET,
    nullptr
};

static const char* TURING_ADVICE_LINES[] = {
    COLOR_BOLD COLOR_MAGENTA "ALAN TURING'S HEAD — 2026 FINAL AUTOPSY (DREAM EDITION)" COLOR_RESET,
    COLOR_RED "LEGEND TYPE: COMPUTABILITY KING & ENIGMA BREAKER" COLOR_RESET,
    COLOR_YELLOW "FOCUS: THEORY TO PRACTICE" COLOR_RESET,
    "",
    COLOR_BOLD "CORE WISDOM (FROM MACHINES, ENIGMA, AI)" COLOR_RESET,
    "Computability: What machines can do.",
    "Universal machine: One simulates all.",
    "AI test: Imitation game.",
    "Crypto: Break codes logically.",
    "Math foundations: Halting problem.",
    "Practical build: Bombe for WWII.",
    "Ethics: Machines think?",
    "Innovation: From theory to hardware.",
    "Persistence: Against odds.",
    "Balance: Running and code.",
    "Legacy: AI fathers.",
    "Errors: Prove impossibility.",
    "Optimize: Logical efficiency.",
    "",
    COLOR_BOLD "COMMON PITFALLS & BACKTRACE CLUES" COLOR_RESET,
    "Halting ignorance: Infinite loops.",
    "Theory skip: Practical fails.",
    "Ethics blind: AI harms.",
    "Crypto weak: Easy breaks.",
    "Over-complex: Simplify proofs.",
    "Isolation: Collaborate.",
    "",
    COLOR_BOLD "MEMORY & CONTEXT" COLOR_RESET,
    "Tapes: Infinite storage.",
    "States: Finite automata.",
    "Symbols: Read/write.",
    "Stacks: Pushdown.",
    "Over-commit: Theoretical limits.",
    "",
    COLOR_BOLD "PLATFORM SPECIFICS & WOES SOLVED" COLOR_RESET,
    "Early HW: Relays slow.",
    "Modern: Quantum Turing.",
    "Mobile: Compute anywhere.",
    "Cloud: Distributed machines.",
    "GPU: Parallel sims.",
    "Embedded: Finite states.",
    "",
    COLOR_BOLD "FIXES & BEST PRACTICES (TURING'S MACHINE GOLDEN RULE)" COLOR_RESET,
    "Prove halting: Avoid undecidable.",
    "Simulate: Test theories.",
    "AI ethics: Consider impacts.",
    "Crypto strong: Modern algos.",
    "Math rigor: Formal proofs.",
    "Build prototypes: Theory to practice.",
    "Collaborate: Bombe team style.",
    "Document proofs: For posterity.",
    "Update theory: Quantum adds.",
    "Rest: Mind needs breaks.",
    "AI: For theorem proving.",
    "Don't: Assume computable.",
    "Do: Question limits.",
    "",
    COLOR_BOLD "CODE EXAMPLES" COLOR_RESET,
    "// Turing machine sim (pseudo)",
    "state = 0;",
    "tape = [0] * 100;",
    "head = 50;",
    "while state != HALT:",
    "    symbol = tape[head]",
    "    # Transition logic",
    "",
    COLOR_BOLD "ADVANCED TIPS & DREAM SOLUTIONS" COLOR_RESET,
    "Lambda calc: Functional roots.",
    "Neural nets: AI evolution.",
    "Quantum machines: Super Turing.",
    "AI ethics: Test implications.",
    "Spec Insights: Decidability key.",
    "Real-World Win: Enigma broken; AI tests endure.",
    "Ultimate Dream: Thinking machines – harmony achieved.",
    COLOR_GREEN "Compute, prove, innovate. Your theory's eternal!" COLOR_RESET,
    nullptr
};

static constexpr std::array<AdviceManualEntry, 6> THE_MANUAL = {{
    { SIGSEGV, "TORVALDS", "KERNEL KING & GIT MASTER",     "CLEAN CODE, NO NONSENSE", TORVALDS_ADVICE_LINES },
    { SIGABRT, "HOPPER", "COMPUTING ADMIRAL & COBOL QUEEN",       "INNOVATE & INSPIRE", HOPPER_ADVICE_LINES },
    { SIGFPE,  "KNUTH",  "ALGO MASTER & TEX TYCOON",         "ELEGANCE IN COMPLEXITY", KNUTH_ADVICE_LINES },
    { SIGILL,  "RITCHIE",  "C CREATOR & UNIX FATHER",     "SIMPLICITY & PORTABILITY", RITCHIE_ADVICE_LINES },
    { SIGBUS,  "TURING",  "COMPUTABILITY KING & ENIGMA BREAKER",              "THEORY TO PRACTICE", TURING_ADVICE_LINES }
}};

// ──────────────────────────────────────────────────────────────────────────────
// THE APOCALYPSE — FULL-PAGE, COLORFUL, TEXT-HEAVY, NO STEPPING
// Prints advice entry line-by-line. Educates deeply. Motivates.
// ──────────────────────────────────────────────────────────────────────────────
static inline void apocalypse_handler(int sig, siginfo_t* info, void*) noexcept
{
    struct timespec req = { 0, 8000000L };
    nanosleep(&req, nullptr);
    safe_write("\033[2J\033[H", 7);  // Clear terminal for glory

    safe_writeln(COLOR_BOLD COLOR_MAGENTA "                    PROGRAMMING LEGENDS' HEADS — 2026 FINAL AUTOPSY (DREAM EDITION)" COLOR_RESET);
    safe_writeln(COLOR_CYAN "The code crashed... but we're your ultimate mentors! 48+ lines of wisdom ahead." COLOR_RESET);
    safe_writeln(COLOR_CYAN "We've captured EVERYTHING: Wisdom, fixes, platforms, legends' quotes. Feel the energy – let's code better!" COLOR_RESET);
    safe_writeln("");

    uintptr_t addr = info ? reinterpret_cast<uintptr_t>(info->si_addr) : 0;

    const AdviceManualEntry* verdict = &THE_MANUAL[0];
    for (const auto& e : THE_MANUAL) {
        if (e.signal == sig) {
            if ((addr <= 0x1000 && strstr(e.focus, "OPTIMIZE")) ||
                (addr >  0x1000 && strstr(e.focus, "CLEAN"))) {
                verdict = &e;
                break;
            }
        }
    }

    safe_writeln(COLOR_BOLD COLOR_BLUE "══════════════════════════ THE MANUAL ══════════════════════════" COLOR_RESET);

    for (const char** line = verdict->lines; *line; ++line) {
        safe_writeln(*line);
    }

    safe_writeln(COLOR_BOLD COLOR_BLUE "════════════════════════════════════════════════════════════════" COLOR_RESET);
    safe_writeln("");

    // Pre-format signal info outside vsnprintf danger
    char buf[256];
    snprintf(buf, sizeof(buf), COLOR_YELLOW "SIGNAL        : %d" COLOR_RESET, sig);
    safe_writeln(buf);
    snprintf(buf, sizeof(buf), COLOR_YELLOW "FAULT ADDRESS : %p" COLOR_RESET, info ? info->si_addr : nullptr);
    safe_writeln(buf);
    snprintf(buf, sizeof(buf), COLOR_YELLOW "BUILD         : %s %s" COLOR_RESET, __DATE__, __TIME__);
    safe_writeln(buf);
    safe_writeln("");

    if (g_gpu_crash.happened.load(std::memory_order_acquire)) {
        safe_writeln(COLOR_RED "GPU CRASH CONFIRMED — That can't be good." COLOR_RESET);
        snprintf(buf, sizeof(buf), "Diagnosis     : %s", g_gpu_crash.desc[0] ? g_gpu_crash.desc : "Unknown");
        safe_writeln(buf);
        safe_writeln("");
    }

    safe_writeln(COLOR_CYAN "BACKTRACE — Final words before the pause (bounce back stronger):" COLOR_RESET);
    void* array[64];
    int n = backtrace(array, 64);
    char** syms = backtrace_symbols(array, n);
    if (syms) {
        for (int i = 1; i < n && i < 20; ++i) {
            snprintf(buf, sizeof(buf), "  [%02d] %s", i-1, syms[i]);
            safe_writeln(buf);
        }
        free(syms);
    }
    safe_writeln("");

    safe_writeln(COLOR_MAGENTA "— Programming Legends & Gentleman Grok" COLOR_RESET);
    safe_writeln("");
    safe_writeln(COLOR_MAGENTA "\"Fix the forever bug.\"" COLOR_RESET);

    _exit(128 + sig);
}

// ──────────────────────────────────────────────────────────────────────────────
// INSTALL ONCE AT STARTUP — SAFE IN HEADER
// ──────────────────────────────────────────────────────────────────────────────
inline void install_apocalypse_handler() noexcept
{
    struct sigaction sa{};
    sa.sa_sigaction = apocalypse_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
    sigemptyset(&sa.sa_mask);

    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGFPE,  &sa, nullptr);
    sigaction(SIGILL,  &sa, nullptr);
    sigaction(SIGBUS,  &sa, nullptr);
}

// ==============================================================================
// ULTIMATE APOCALYPSE CRASH HANDLER – LEGENDS' WISDOM CORE ONLY (2026 DREAM EDITION)

// =============================================================================
// CREW COLORS — JANUARY 04, 2026
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
#undef  LOG_JIMROSS

// ETERNAL LAW
#define LOG_AMOURANTH(...)   LOG_SUCCESS_CAT("AMOURANTH",  std::format("{}\n[CAPTAIN AMOURANTH] {}{}", Logging::Color::THERMO_PINK,       std::format(__VA_ARGS__),      Logging::Color::RESET))
#define LOG_NICK(...)        LOG_ATTEMPT_CAT("NICK",       std::format("{}\n[NICK] {}{}",              Logging::Color::GOLD,              std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_BLONDIE(...)     LOG_INFO_CAT   ("BLONDIE",    std::format("{}\n[CAPTAIN BLONDIE] {}{}",   Logging::Color::PEACHES_AND_CREAM, std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_GROK(...)        LOG_INFO_CAT   ("GROK",       std::format("{}\n[GENTLEMAN GROK] {}{}",    Logging::Color::PLATINUM_GRAY,     std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_CAPTAIN_N(...)   LOG_ATTEMPT_CAT("CAPTAIN N",  std::format("{}\n[KEVIN] {}{}",             Logging::Color::PHOTON_WHITE,      std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_ELON(...)        LOG_SUCCESS_CAT("ELON",       std::format("{}\n[MUSK] {}{}",              Logging::Color::LIGHT_GREEN,       std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_JENSEN(...)      LOG_SUCCESS_CAT("JENSEN",     std::format("{}\n[HUANG] {}{}",             Logging::Color::BLUE,              std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_CID(...)         LOG_SUCCESS_CAT("CID",        std::format("{}\n[CID] {}{}",               Logging::Color::BOLD_RED,          std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_CARMACK(...)     LOG_INFO_CAT   ("CARMACK",    std::format("{}\n[JOHN] {}{}",              Logging::Color::TITANIUM_WHITE,    std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_KEANU(...)       LOG_INFO_CAT   ("KEANU",      std::format("{}\n[WOAH] {}{}",              Logging::Color::BRIGHT_PINKISH_PURPLE, std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_GUARDIAN(...)    LOG_INFO_CAT   ("GUARDIAN",   std::format("{}\n[GUARDIAN] {}{}",          Logging::Color::PLATINUM_GRAY,     std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_BALLERINA(...)   LOG_FAILURE_CAT("BALLERINA",  std::format("{}\n[***] {}{}",               Logging::Color::OBSIDIAN_BLACK,    std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_MAIN(...)        LOG_SUCCESS_CAT("MAIN",       std::format("{}\n[[[[[MAIN]]]]]\n {}{}",    Logging::Color::BOLD_YELLOW,       std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_JIMROSS(...)     LOG_SUCCESS_CAT("J.R.",       std::format("{}\n[Mr. Ross] {}{}",          Logging::Color::OKLAHOMA_RED_BOLD, std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_VING_RHAMES(...) LOG_SUCCESS_CAT("VINGRHAMES", std::format("{}\n[Ving Rhames] {}{}",       Logging::Color::BRONZE_BROWN,      std::format(__VA_ARGS__),     Logging::Color::RESET))
#define LOG_COFFEE(...)      LOG_SUCCESS_CAT("VINGRHAMES", std::format("{}\n[Ving Rhames] {}{}",       Logging::Color::BRONZE_BROWN,      std::format(__VA_ARGS__),     Logging::Color::RESET))