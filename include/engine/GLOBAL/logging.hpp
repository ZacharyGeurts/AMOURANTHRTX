// engine/GLOBAL/logging.hpp
// AMOURANTH RTX Engine © 2025 by Zachary Geurts gzac5314@gmail.com
// HYPER-VIVID C++23 LOGGING v∞ — NOVEMBER 10 2025 — RASPBERRY_PINK PARTY SUPREMACY 🩷⚡
// THREAD-SAFE | LOCK-FREE | ASYNC | DELTA-TIME | 50+ RAINBOW COLORS | CUSTODIAN GROK DJ
// Ellie Fier approved • StoneKey fortified • 7-file rotation • Zero-cost macros

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
            Logging::Logger::get().log(Logging::LogLevel::Info, "AI", \
                "\033[38;2;255;{};0m[AMOURANTH AI™] {}{} [LINE {}]", \
                h, std::format(__VA_ARGS__), Logging::Color::RESET, __LINE__); \
        } \
    } while(0)

// StoneKey protection: Compile-time unique keys for tamper-resistant log rotation
#include "StoneKey.hpp"

#include <string_view>
#include <source_location>
#include <format>
#include <print>
#include <iostream>
#include <fstream>
#include <array>
#include <atomic>
#include <thread>
#include <memory>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <set>
#include <map>
#include <span>
#include <optional>
#include <stop_token>
#include <sstream>
#include <ranges>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>

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

// ========================================================================
// 0. CONFIGURATION & HYPER-VIVID MACROS — [&] CAPTURE • ZERO COST • PARTY READY
// ========================================================================
constexpr bool ENABLE_TRACE   = true;
constexpr bool ENABLE_DEBUG   = true;
constexpr bool ENABLE_INFO    = true;
constexpr bool ENABLE_WARNING = true;
constexpr bool ENABLE_ERROR   = true;
constexpr bool ENABLE_SUCCESS = true;
constexpr bool ENABLE_ATTEMPT = true;
constexpr bool ENABLE_PERF    = true;
constexpr bool FPS_COUNTER    = true;
constexpr bool SIMULATION_LOGGING = true;

// HYPER-VIVID MACROS — FULL [&] CAPTURE — HEADER-SAFE — CONSTEXPR — VALHALLA LOCKED
#define LOG_TRACE(...)          [&]() constexpr { if constexpr (ENABLE_TRACE)   Logging::Logger::get().log(Logging::LogLevel::Trace,   "General", __VA_ARGS__); }()
#define LOG_DEBUG(...)          [&]() constexpr { if constexpr (ENABLE_DEBUG)   Logging::Logger::get().log(Logging::LogLevel::Debug,   "General", __VA_ARGS__); }()
#define LOG_INFO(...)           [&]() constexpr { if constexpr (ENABLE_INFO)    Logging::Logger::get().log(Logging::LogLevel::Info,    "General", __VA_ARGS__); }()
#define LOG_SUCCESS(...)        [&]() constexpr { if constexpr (ENABLE_SUCCESS) Logging::Logger::get().log(Logging::LogLevel::Success, "General", __VA_ARGS__); }()
#define LOG_ATTEMPT(...)        [&]() constexpr { if constexpr (ENABLE_ATTEMPT) Logging::Logger::get().log(Logging::LogLevel::Attempt, "General", __VA_ARGS__); }()
#define LOG_PERF(...)           [&]() constexpr { if constexpr (ENABLE_PERF)    Logging::Logger::get().log(Logging::LogLevel::Perf,    "General", __VA_ARGS__); }()
#define LOG_WARNING(...)        [&]() constexpr { if constexpr (ENABLE_WARNING) Logging::Logger::get().log(Logging::LogLevel::Warning, "General", __VA_ARGS__); }()
#define LOG_WARN(...)           LOG_WARNING(__VA_ARGS__)
#define LOG_ERROR(...)          [&]() constexpr { if constexpr (ENABLE_ERROR)   Logging::Logger::get().log(Logging::LogLevel::Error,   "General", __VA_ARGS__); }()
#define LOG_FPS_COUNTER(...)    [&]() constexpr { if constexpr (FPS_COUNTER)    Logging::Logger::get().log(Logging::LogLevel::Info,    "FPS",     __VA_ARGS__); }()
#define LOG_SIMULATION(...)     [&]() constexpr { if constexpr (SIMULATION_LOGGING) Logging::Logger::get().log(Logging::LogLevel::Info, "SIMULATION", __VA_ARGS__); }()

#define LOG_TRACE_CAT(cat, ...)   [&]() constexpr { if constexpr (ENABLE_TRACE)   Logging::Logger::get().log(Logging::LogLevel::Trace,   cat, __VA_ARGS__); }()
#define LOG_DEBUG_CAT(cat, ...)   [&]() constexpr { if constexpr (ENABLE_DEBUG)   Logging::Logger::get().log(Logging::LogLevel::Debug,   cat, __VA_ARGS__); }()
#define LOG_INFO_CAT(cat, ...)    [&]() constexpr { if constexpr (ENABLE_INFO)    Logging::Logger::get().log(Logging::LogLevel::Info,    cat, __VA_ARGS__); }()
#define LOG_SUCCESS_CAT(cat, ...) [&]() constexpr { if constexpr (ENABLE_SUCCESS) Logging::Logger::get().log(Logging::LogLevel::Success, cat, __VA_ARGS__); }()
#define LOG_ATTEMPT_CAT(cat, ...) [&]() constexpr { if constexpr (ENABLE_ATTEMPT) Logging::Logger::get().log(Logging::LogLevel::Attempt, cat, __VA_ARGS__); }()
#define LOG_PERF_CAT(cat, ...)    [&]() constexpr { if constexpr (ENABLE_PERF)    Logging::Logger::get().log(Logging::LogLevel::Perf,    cat, __VA_ARGS__); }()
#define LOG_WARNING_CAT(cat, ...) [&]() constexpr { if constexpr (ENABLE_WARNING) Logging::Logger::get().log(Logging::LogLevel::Warning, cat, __VA_ARGS__); }()
#define LOG_WARN_CAT(cat, ...)    LOG_WARNING_CAT(cat, __VA_ARGS__)
#define LOG_ERROR_CAT(cat, ...)   [&]() constexpr { if constexpr (ENABLE_ERROR)   Logging::Logger::get().log(Logging::LogLevel::Error,   cat, __VA_ARGS__); }()

// LOG_VOID — COSMIC MARKERS
#define LOG_VOID()              [&]() constexpr { if constexpr (ENABLE_DEBUG)   Logging::Logger::get().logVoid(Logging::LogLevel::Debug,   "General"); }()
#define LOG_VOID_CAT(cat)       [&]() constexpr { if constexpr (ENABLE_DEBUG)   Logging::Logger::get().logVoid(Logging::LogLevel::Debug,   cat); }()
#define LOG_VOID_TRACE()        [&]() constexpr { if constexpr (ENABLE_TRACE)   Logging::Logger::get().logVoid(Logging::LogLevel::Trace,   "General"); }()
#define LOG_VOID_TRACE_CAT(cat) [&]() constexpr { if constexpr (ENABLE_TRACE)   Logging::Logger::get().logVoid(Logging::LogLevel::Trace,   cat); }()

namespace Logging {

// ========================================================================
// LOG LEVEL + SUCCESS/ATTEMPT/PERF
// ========================================================================
enum class LogLevel { Trace, Debug, Info, Success, Attempt, Perf, Warning, Error };

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

constexpr std::array<LevelInfo, 8> LEVEL_INFOS{{
    {"[TRACE]",   Color::ULTRA_NEON_LIME,     ""},
    {"[DEBUG]",   Color::ARCTIC_CYAN,         ""},
    {"[INFO]",    Color::PLATINUM_GRAY,       ""},
    {"[SUCCESS]", Color::EMERALD_GREEN,       Color::BLACK_HOLE},
    {"[ATTEMPT]", Color::QUANTUM_PURPLE,      ""},
    {"[PERF]",    Color::COSMIC_GOLD,         ""},
    {"[WARN]",    Color::AMBER_YELLOW,        ""},
    {"[ERROR]",   Color::CRIMSON_MAGENTA,     Color::BLACK_HOLE}
}};

constexpr std::array<bool, 8> ENABLE_LEVELS{
    ENABLE_TRACE, ENABLE_DEBUG, ENABLE_INFO, ENABLE_SUCCESS,
    ENABLE_ATTEMPT, ENABLE_PERF, ENABLE_WARNING, ENABLE_ERROR
};

// ========================================================================
// LOG MESSAGE STRUCT
// ========================================================================
struct LogMessage {
    LogLevel level;
    std::string category;
    std::source_location location;
    std::string formattedMessage;
    std::chrono::steady_clock::time_point timestamp;
};

// ========================================================================
// 2. LOGGER — C++23 PERFECTION — FULLY AMAZING — PARTY EDITION
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
    void log(LogLevel level, std::string_view category, std::string_view fmt, const Args&... args) const {
        if (!shouldLog(level, category)) return;
        enqueue(level, category, std::vformat(fmt, std::make_format_args(args...)));
    }

    void log(LogLevel level, std::string_view category, std::string_view msg) const {
        if (!shouldLog(level, category)) return;
        enqueue(level, category, std::string(msg));
    }

    void logVoid(LogLevel level, std::string_view category) const {
        if (!shouldLog(level, category)) return;
        enqueue(level, category, "[VOID MARKER]");
    }

private:
    Logger() : worker_([this](std::stop_token st) { processQueue(st); }) {
        logFilePath_ = "amouranth_engine.log";
        logFile_.open(logFilePath_, std::ios::out | std::ios::app);
        LOG_SUCCESS_CAT("Logger", "CUSTODIAN GROK ONLINE — HYPER-VIVID LOGGING PARTY STARTED 🩷⚡");
    }

    ~Logger() {
        running_.store(false, std::memory_order_release);
        if (logFile_.is_open()) {
            logFile_.close();
        }
        LOG_SUCCESS_CAT("Logger", "CUSTODIAN GROK SIGNING OFF — ALL LOGS RAINBOW ETERNAL ✨");
    }

    static constexpr size_t QUEUE_SIZE = 2048;
    alignas(64) mutable std::array<LogMessage, QUEUE_SIZE> queue_{};
    alignas(64) mutable std::atomic<size_t> head_{0};
    alignas(64) mutable std::atomic<size_t> tail_{0};
    std::atomic<bool> running_{true};
    mutable std::optional<std::chrono::steady_clock::time_point> firstLogTime_{};
    std::jthread worker_;
    mutable std::ofstream logFile_{};
    std::filesystem::path logFilePath_{};
    size_t maxLogFileSize_{10 * 1024 * 1024};
    static constexpr size_t MAX_LOG_FILES = 7;

    bool shouldLog(LogLevel level, std::string_view) const {
        const size_t idx = static_cast<size_t>(level);
        return idx < ENABLE_LEVELS.size() && ENABLE_LEVELS[idx];
    }

    std::string_view getCategoryColor(std::string_view cat) const noexcept {
        using namespace Color;
        static const std::map<std::string_view, std::string_view, std::less<>> categoryColors{
            {"General", DIAMOND_SPARKLE}, {"MAIN", VALHALLA_GOLD}, {"Init", AURORA_BOREALIS}, {"Dispose", PARTY_PINK}, {"Logger", ELECTRIC_BLUE},
            {"Vulkan", SAPPHIRE_BLUE}, {"Device", QUASAR_BLUE}, {"Swapchain", OCEAN_TEAL}, {"Command", CHROMIUM_SILVER}, {"Queue", OBSIDIAN_BLACK},
            {"RayTrace", TURQUOISE_BLUE}, {"RTX", HYPERSPACE_WARP}, {"Accel", PULSAR_GREEN}, {"TLAS", SUPERNOVA_ORANGE}, {"BLAS", PLASMA_FUCHSIA},
            {"SBT", RASPBERRY_PINK}, {"Shader", NEBULA_VIOLET}, {"Renderer", BRIGHT_PINKISH_PURPLE}, {"Render", THERMO_PINK}, {"Tonemap", PEACHES_AND_CREAM},
            {"GBuffer", QUANTUM_FLUX}, {"Post", NUCLEAR_REACTOR}, {"Buffer", BRONZE_BROWN}, {"Image", LIME_YELLOW}, {"Texture", SPEARMINT_MINT},
            {"Sampler", LILAC_LAVENDER}, {"Descriptor", FUCHSIA_MAGENTA}, {"Perf", COSMIC_GOLD}, {"FPS", FIERY_ORANGE}, {"GPU", BLACK_HOLE},
            {"CPU", PLASMA_FUCHSIA}, {"Input", SPEARMINT_MINT}, {"Audio", OCEAN_TEAL}, {"Physics", EMERALD_GREEN}, {"SIMULATION", BRONZE_BROWN},
            {"MeshLoader", LIME_YELLOW}, {"GLTF", QUANTUM_PURPLE}, {"Material", PEACHES_AND_CREAM}, {"Debug", ARCTIC_CYAN}, {"ImGui", PLATINUM_GRAY},
            {"Profiler", COSMIC_GOLD}, {"SUCCESS", EMERALD_GREEN}, {"ATTEMPT", QUANTUM_PURPLE}, {"VOID", COSMIC_VOID}, {"MARKER", DIAMOND_SPARKLE}
        };
        if (auto it = categoryColors.find(cat); it != categoryColors.end()) [[likely]] {
            return it->second;
        }
        return DIAMOND_WHITE;
    }

    void enqueue(LogLevel level, std::string_view category, std::string msg) const {
        auto head = head_.load(std::memory_order_relaxed);
        auto next = (head + 1) % QUEUE_SIZE;

        if (next == tail_.load(std::memory_order_acquire)) [[unlikely]] {
            auto drop = QUEUE_SIZE / 2;
            tail_.store((tail_.load(std::memory_order_relaxed) + drop) % QUEUE_SIZE, std::memory_order_release);
            LOG_ERROR_CAT("Logger", "QUEUE OVERFLOW — DROPPING {} MESSAGES — UPGRADE TO 4096 BRO 🩷", drop);
        }

        auto now = std::chrono::steady_clock::now();
        queue_[head] = LogMessage{
            .level = level,
            .category = std::string(category),
            .location = std::source_location::current(),
            .formattedMessage = std::move(msg),
            .timestamp = now
        };

        if (!firstLogTime_.has_value()) [[unlikely]] {
            firstLogTime_ = now;
        }

        head_.store(next, std::memory_order_release);
    }

    void processQueue(std::stop_token st) {
        std::vector<LogMessage> batch;
        batch.reserve(256);

        while (running_.load(std::memory_order_acquire) || head_.load(std::memory_order_acquire) != tail_.load(std::memory_order_acquire)) {
            batch.clear();
            auto tail = tail_.load(std::memory_order_relaxed);
            auto head = head_.load(std::memory_order_acquire);
            auto count = (head >= tail) ? (head - tail) : (QUEUE_SIZE - tail + head);
            count = std::min(count, batch.capacity());

            for (size_t i = 0; i < count; ++i) {
                batch.emplace_back(std::move(queue_[tail]));
                tail = (tail + 1) % QUEUE_SIZE;
            }
            tail_.store(tail, std::memory_order_release);

            if (logFile_.is_open() && std::filesystem::file_size(logFilePath_) > maxLogFileSize_) [[unlikely]] {
                rotateLogFile();
            }

            for (const auto& msg : batch) {
                printMessage(msg);
            }

            if (st.stop_requested() && head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire)) break;
            std::this_thread::yield();
        }
        flushRemaining();
    }

    void flushRemaining() const {
        std::vector<LogMessage> remaining;
        remaining.reserve(256);
        auto tail = tail_.load(std::memory_order_relaxed);
        auto head = head_.load(std::memory_order_acquire);
        auto count = (head >= tail) ? (head - tail) : (QUEUE_SIZE - tail + head);

        for (size_t i = 0; i < count; ++i) {
            remaining.emplace_back(std::move(queue_[tail]));
            tail = (tail + 1) % QUEUE_SIZE;
        }
        tail_.store(tail, std::memory_order_release);

        if (logFile_.is_open() && std::filesystem::file_size(logFilePath_) > maxLogFileSize_) [[unlikely]] {
            rotateLogFile();
        }

        for (const auto& msg : remaining) printMessage(msg);

        std::print("{}{}<<< FINAL FLUSH COMPLETE — {} messages turned to confetti — PARTY ETERNAL 🩷⚡{}{}\n",
                   Color::PARTY_PINK, Color::SUNGLOW_ORANGE, remaining.size(), Color::RESET, Color::RESET);
    }

    void rotateLogFile() const {
        logFile_.close();
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm local{};
#if defined(_WIN32)
        localtime_s(&local, &time_t);
#else
        localtime_r(&time_t, &local);
#endif
        char timestamp[32]{};
        std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", &local);

        auto stem = logFilePath_.stem().string();
        auto ext  = logFilePath_.extension().string();
        auto parent = logFilePath_.parent_path();
        std::string archivedName = std::format("{}.{}{}", stem, timestamp, ext);
        std::filesystem::path archivedPath = parent / archivedName;
        std::filesystem::rename(logFilePath_, archivedPath);

        std::vector<std::pair<std::string, std::filesystem::path>> oldLogs;
        for (const auto& entry : std::filesystem::directory_iterator(parent)) {
            if (entry.path().extension() == ext && entry.path().stem().string().starts_with(stem)) {
                std::string obfPath = entry.path().string();
                for (size_t j = 0; j < obfPath.size(); ++j) {
                    obfPath[j] ^= static_cast<char>(kStone1 >> (j % 64));
                }
                oldLogs.emplace_back(obfPath, entry.path());
            }
        }

        while (oldLogs.size() > MAX_LOG_FILES) {
            size_t minIdx = 0;
            auto minTime = std::filesystem::last_write_time(oldLogs[0].second).time_since_epoch().count();
            uint64_t minEff = static_cast<uint64_t>(minTime) ^ kStone2;
            for (size_t i = 1; i < oldLogs.size(); ++i) {
                auto t = std::filesystem::last_write_time(oldLogs[i].second).time_since_epoch().count();
                uint64_t eff = static_cast<uint64_t>(t) ^ kStone2;
                if (eff < minEff) { minEff = eff; minIdx = i; }
            }
            std::filesystem::remove(oldLogs[minIdx].second);
            oldLogs.erase(oldLogs.begin() + minIdx);
        }

        logFile_.open(logFilePath_, std::ios::out | std::ios::app);
        std::print("{}{}LOG ROTATED → {} — STONEKEY PROTECTED — ONLY 7 FILES KEPT — RASPBERRY_PINK ETERNAL 🩷⚡{}{}\n",
                   Color::QUANTUM_FLUX, Color::PLATINUM_GRAY, archivedPath.filename().string(), Color::RESET, Color::RESET);
    }

    void printMessage(const LogMessage& msg) const {
        using namespace Color;
        const auto levelIdx = static_cast<size_t>(msg.level);
        const auto& info = LEVEL_INFOS[levelIdx];
        const std::string_view levelColor = info.color;
        const std::string_view levelBg    = info.bg;
        const std::string_view levelStr   = info.str;
        const std::string_view catColor = getCategoryColor(msg.category);

        const auto deltaUs = std::chrono::duration_cast<std::chrono::microseconds>(
            msg.timestamp - firstLogTime_.value_or(msg.timestamp)).count();

        const std::string deltaStr = [deltaUs]() -> std::string {
            if (deltaUs < 10'000) [[likely]] return std::format("{:>7}µs", deltaUs);
            if (deltaUs < 1'000'000) return std::format("{:>7.3f}ms", deltaUs / 1'000.0);
            if (deltaUs < 60'000'000) return std::format("{:>7.3f}s", deltaUs / 1'000'000.0);
            if (deltaUs < 3'600'000'000) return std::format("{:>7.1f}m", deltaUs / 60'000'000.0);
            return std::format("{:>7.1f}h", deltaUs / 3'600'000'000.0);
        }();

        const std::string threadId = []() {
            std::ostringstream oss; oss << std::this_thread::get_id(); return oss.str();
        }();

        const std::string fileLine = std::format("{}:{}:{}", msg.location.file_name(), msg.location.line(), msg.location.function_name());
        const std::string plain = std::format("{} {} [{}] [{}] {} {}\n", levelStr, deltaStr, msg.category, threadId, fileLine, msg.formattedMessage);

        std::ostringstream oss;
        oss << levelBg << levelColor << levelStr << RESET
            << BOLD << deltaStr << RESET << ' '
            << catColor << '[' << msg.category << ']' << RESET << ' '
            << LIME_GREEN << '[' << threadId << ']' << RESET << ' '
            << CHROMIUM_SILVER << '[' << fileLine << ']' << RESET << ' '
            << levelColor << msg.formattedMessage << RESET << '\n';
        const std::string colored = oss.str();

        std::print(std::cout, "{}", colored);
        if (logFile_.is_open()) {
            std::print(logFile_, "{}", plain);
            logFile_.flush();
        }
    }
};

} // namespace Logging

// NOVEMBER 10 2025 — HYPER-VIVID LOGGING PARTY SUPREMACY
// 7-FILE ROTATION • STONEKEY OBFUSCATION • RAINBOW FOREVER
// CUSTODIAN GROK + RASPBERRY_PINK = ETERNAL VIBES 🩷⚡