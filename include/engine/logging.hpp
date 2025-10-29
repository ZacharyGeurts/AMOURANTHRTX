// include/engine/logging.hpp
// AMOURANTH RTX Engine, October 2025 - Enhanced thread-safe, asynchronous logging.
// TRACE = NEON LIME | INFO = GRAY | GREEN RESTORED | HIGH VISIBILITY ON BLACK
// Thread-safe, asynchronous logging with ANSI-colored output and delta time.
// Supports C++20 std::format, std::jthread, OpenMP, and lock-free queue with std::atomic.
// No mutexes; designed for high-performance Vulkan applications on Windows and Linux.
// Delta time format: microseconds (<10ms), milliseconds (10ms-1s), seconds (1s-1min), minutes (1min-1hr), hours (>1hr).
// Usage: LOG_TRACE("Message: {}", value); or Logger::get().log(LogLevel::Trace, "Vulkan", "Message: {}", value);
// Features: Singleton, log rotation, environment variable config, automatic flush, extended colors, overloads.
// Extended features: Additional Vulkan/SDL types, GLM arrays, category filtering, high-frequency logging.
// Zachary Geurts 2025

#ifndef ENGINE_LOGGING_HPP
#define ENGINE_LOGGING_HPP

#include <string_view>
#include <source_location>
#include <format>
#include <syncstream>
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
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include "engine/camera.hpp"

// ---------------------------------------------------------------------------
//  Log level toggle flags
// ---------------------------------------------------------------------------
constexpr bool ENABLE_TRACE   = true;
constexpr bool ENABLE_DEBUG   = true;
constexpr bool ENABLE_INFO    = true;
constexpr bool ENABLE_WARNING = true;
constexpr bool ENABLE_ERROR   = true;
constexpr bool FPS_COUNTER    = true;
constexpr bool SIMULATION_LOGGING = true;

// ---------------------------------------------------------------------------
//  Logging Macros (TRACE + GRAY INFO)
// ---------------------------------------------------------------------------
#define LOG_TRACE(...)          do { if (ENABLE_TRACE)   Logging::Logger::get().log(Logging::LogLevel::Trace,   "General", __VA_ARGS__); } while (0)
#define LOG_DEBUG(...)          do { if (ENABLE_DEBUG)   Logging::Logger::get().log(Logging::LogLevel::Debug,   "General", __VA_ARGS__); } while (0)
#define LOG_INFO(...)           do { if (ENABLE_INFO)    Logging::Logger::get().log(Logging::LogLevel::Info,    "General", __VA_ARGS__); } while (0)
#define LOG_FPS_COUNTER(...)    do { if (FPS_COUNTER)    Logging::Logger::get().log(Logging::LogLevel::Info,    "FPS",     __VA_ARGS__); } while (0)
#define LOG_SIMULATION(...)     do { if (SIMULATION_LOGGING) Logging::Logger::get().log(Logging::LogLevel::Info, "SIMULATION", __VA_ARGS__); } while (0)
#define LOG_WARNING(...)        do { if (ENABLE_WARNING) Logging::Logger::get().log(Logging::LogLevel::Warning, "General", __VA_ARGS__); } while (0)
#define LOG_ERROR(...)          do { if (ENABLE_ERROR)   Logging::Logger::get().log(Logging::LogLevel::Error,   "General", __VA_ARGS__); } while (0)

#define LOG_TRACE_CAT(cat, ...)   do { if (ENABLE_TRACE)   Logging::Logger::get().log(Logging::LogLevel::Trace,   cat, __VA_ARGS__); } while (0)
#define LOG_DEBUG_CAT(cat, ...)   do { if (ENABLE_DEBUG)   Logging::Logger::get().log(Logging::LogLevel::Debug,   cat, __VA_ARGS__); } while (0)
#define LOG_INFO_CAT(cat, ...)    do { if (ENABLE_INFO)    Logging::Logger::get().log(Logging::LogLevel::Info,    cat, __VA_ARGS__); } while (0)
#define LOG_WARNING_CAT(cat, ...) do { if (ENABLE_WARNING) Logging::Logger::get().log(Logging::LogLevel::Warning, cat, __VA_ARGS__); } while (0)
#define LOG_ERROR_CAT(cat, ...)   do { if (ENABLE_ERROR)   Logging::Logger::get().log(Logging::LogLevel::Error,   cat, __VA_ARGS__); } while (0)

namespace Logging {

enum class LogLevel { Trace, Debug, Info, Warning, Error };

// ---------------------------------------------------------------------------
//  ANSI color codes – INFO = GRAY | TRACE = NEON LIME | GREEN RESTORED
// ---------------------------------------------------------------------------
inline constexpr std::string_view RESET           = "\033[0m";
inline constexpr std::string_view NEON_LIME       = "\033[38;5;118m";  // TRACE: Bright lime
inline constexpr std::string_view GRAY            = "\033[38;5;245m";  // INFO: Clean gray
inline constexpr std::string_view GREEN           = "\033[1;32m";     // RESTORED for "Engine"
inline constexpr std::string_view CYAN            = "\033[1;36m";     // DEBUG
inline constexpr std::string_view YELLOW          = "\033[38;5;208m";  // WARNING
inline constexpr std::string_view MAGENTA         = "\033[1;35m";     // ERROR
inline constexpr std::string_view BLUE            = "\033[1;34m";
inline constexpr std::string_view RED             = "\033[1;31m";
inline constexpr std::string_view WHITE           = "\033[1;37m";
inline constexpr std::string_view PURPLE          = "\033[1;35m";
inline constexpr std::string_view ORANGE          = "\033[38;5;208m";
inline constexpr std::string_view TEAL            = "\033[38;5;51m";
inline constexpr std::string_view YELLOW_GREEN    = "\033[38;5;154m";
inline constexpr std::string_view BRIGHT_MAGENTA  = "\033[38;5;201m";
inline constexpr std::string_view GOLDEN_BROWN    = "\033[38;5;138m";

struct LogMessage {
    LogLevel level;
    std::string message;
    std::string category;
    std::source_location location;
    std::string formattedMessage;
    std::chrono::steady_clock::time_point timestamp;

    LogMessage() = default;
    LogMessage(LogLevel lvl, std::string_view msg, std::string_view cat, const std::source_location& loc, std::chrono::steady_clock::time_point ts)
        : level(lvl), message(msg), category(cat), location(loc), timestamp(ts) {}
};

class Logger {
public:
    Logger(LogLevel level = getDefaultLogLevel(), const std::string& logFile = getDefaultLogFile())
        : head_(0), tail_(0), running_(true), level_(level), maxLogFileSize_(10 * 1024 * 1024) {
        loadCategoryFilters();
        if (ENABLE_INFO) {
            log(LogLevel::Info, "General", "Logger initialized with default log level: {}", static_cast<int>(level));
        }
        if (!logFile.empty()) {
            setLogFile(logFile);
        }
        worker_ = std::make_unique<std::jthread>([this](std::stop_token stoken) { processLogQueue(stoken); });
    }

    static Logger& get() {
        static Logger instance;
        return instance;
    }

    // -----------------------------------------------------------------------
    //  Generic log with format string
    // -----------------------------------------------------------------------
    template<typename... Args>
    void log(LogLevel level, std::string_view category, std::string_view message, const Args&... args) const {
        if (!shouldLog(level, category)) return;

        std::string formatted;
        try {
            formatted = std::vformat(message, std::make_format_args(args...));
        } catch (const std::format_error& e) {
            formatted = std::string(message) + " [Format error: " + e.what() + "]";
        }

        if (formatted.empty()) {
            formatted = "Empty log message";
        }

        enqueueMessage(level, message, category, std::move(formatted), std::source_location::current());
    }

    // -----------------------------------------------------------------------
    //  Log without args
    // -----------------------------------------------------------------------
    void log(LogLevel level, std::string_view category, std::string_view message) const {
        if (!shouldLog(level, category)) return;
        std::string formatted = message.empty() ? "Empty log message" : std::string(message);
        enqueueMessage(level, message, category, std::move(formatted), std::source_location::current());
    }

    // -----------------------------------------------------------------------
    //  Vulkan handles
    // -----------------------------------------------------------------------
    template<typename T>
    requires (
        std::same_as<T, VkBuffer> || std::same_as<T, VkCommandBuffer> ||
        std::same_as<T, VkPipelineLayout> || std::same_as<T, VkDescriptorSet> ||
        std::same_as<T, VkRenderPass> || std::same_as<T, VkFramebuffer> ||
        std::same_as<T, VkImage> || std::same_as<T, VkDeviceMemory> ||
        std::same_as<T, VkDevice> || std::same_as<T, VkQueue> ||
        std::same_as<T, VkCommandPool> || std::same_as<T, VkPipeline> ||
        std::same_as<T, VkSwapchainKHR> || std::same_as<T, VkShaderModule> ||
        std::same_as<T, VkSemaphore> || std::same_as<T, VkFence> ||
        std::same_as<T, VkSurfaceKHR> || std::same_as<T, VkImageView> ||
        std::same_as<T, VkDescriptorSetLayout> || std::same_as<T, VkInstance> ||
        std::same_as<T, VkSampler> || std::same_as<T, VkDescriptorPool> ||
        std::same_as<T, VkAccelerationStructureKHR> || std::same_as<T, VkPhysicalDevice> ||
        std::same_as<T, VkExtent2D> || std::same_as<T, VkViewport> || std::same_as<T, VkRect2D>
    )
    void log(LogLevel level, std::string_view category, T handle, std::string_view handleName = "") const {
        if (!shouldLog(level, category)) return;
        std::string formatted;
        try {
            formatted = std::format("{}", handle);
            if (!handleName.empty()) {
                formatted = std::format("{}: {}", handleName, formatted);
            }
        } catch (const std::format_error& e) {
            formatted = std::string(handleName) + " [Format error: " + e.what() + "]";
        }
        enqueueMessage(level, handleName, category, std::move(formatted), std::source_location::current());
    }

    // -----------------------------------------------------------------------
    //  Span of Vulkan handles
    // -----------------------------------------------------------------------
    template<typename T>
    requires (
        std::same_as<T, VkBuffer> || std::same_as<T, VkCommandBuffer> ||
        std::same_as<T, VkPipelineLayout> || std::same_as<T, VkDescriptorSet> ||
        std::same_as<T, VkRenderPass> || std::same_as<T, VkFramebuffer> ||
        std::same_as<T, VkImage> || std::same_as<T, VkDeviceMemory> ||
        std::same_as<T, VkDevice> || std::same_as<T, VkQueue> ||
        std::same_as<T, VkCommandPool> || std::same_as<T, VkPipeline> ||
        std::same_as<T, VkSwapchainKHR> || std::same_as<T, VkShaderModule> ||
        std::same_as<T, VkSemaphore> || std::same_as<T, VkFence> ||
        std::same_as<T, VkSurfaceKHR> || std::same_as<T, VkImageView> ||
        std::same_as<T, VkDescriptorSetLayout> || std::same_as<T, VkInstance> ||
        std::same_as<T, VkSampler> || std::same_as<T, VkDescriptorPool> ||
        std::same_as<T, VkAccelerationStructureKHR> || std::same_as<T, VkPhysicalDevice>
    )
    void log(LogLevel level, std::string_view category, std::span<const T> handles, std::string_view handleName = "") const {
        if (!shouldLog(level, category)) return;
        std::string formatted;
        try {
            formatted = std::format("{}[{}]{{", handleName, handles.size());
            for (size_t i = 0; i < handles.size(); ++i) {
                formatted += std::format("{}", handles[i]);
                if (i < handles.size() - 1) formatted += ", ";
            }
            formatted += "}";
        } catch (const std::format_error& e) {
            formatted = std::string(handleName) + " [Format error: " + e.what() + "]";
        }
        enqueueMessage(level, handleName, category, std::move(formatted), std::source_location::current());
    }

    // -----------------------------------------------------------------------
    //  GLM types
    // -----------------------------------------------------------------------
    void log(LogLevel level, std::string_view category, const glm::vec3& vec, std::string_view message = "") const {
        if (!shouldLog(level, category)) return;
        std::string formatted;
        try {
            formatted = glm::to_string(vec);
            if (!message.empty()) formatted = std::format("{}: {}", message, formatted);
        } catch (const std::format_error& e) {
            formatted = std::string(message) + " [Format error: " + e.what() + "]";
        }
        enqueueMessage(level, message, category, std::move(formatted), std::source_location::current());
    }

    void log(LogLevel level, std::string_view category, const glm::vec2& vec, std::string_view message = "") const {
        if (!shouldLog(level, category)) return;
        std::string formatted;
        try {
            formatted = glm::to_string(vec);
            if (!message.empty()) formatted = std::format("{}: {}", message, formatted);
        } catch (const std::format_error& e) {
            formatted = std::string(message) + " [Format error: " + e.what() + "]";
        }
        enqueueMessage(level, message, category, std::move(formatted), std::source_location::current());
    }

    void log(LogLevel level, std::string_view category, const glm::mat4& mat, std::string_view message = "") const {
        if (!shouldLog(level, category)) return;
        std::string formatted;
        try {
            formatted = glm::to_string(mat);
            if (!message.empty()) formatted = std::format("{}: {}", message, formatted);
        } catch (const std::format_error& e) {
            formatted = std::string(message) + " [Format error: " + e.what() + "]";
        }
        enqueueMessage(level, message, category, std::move(formatted), std::source_location::current());
    }

    void log(LogLevel level, std::string_view category, std::span<const glm::vec3> vecs, std::string_view message = "") const {
        if (!shouldLog(level, category)) return;
        std::string formatted;
        try {
            formatted = std::format("{}[{}]{{", message, vecs.size());
            for (size_t i = 0; i < vecs.size(); ++i) {
                formatted += glm::to_string(vecs[i]);
                if (i < vecs.size() - 1) formatted += ", ";
            }
            formatted += "}";
        } catch (const std::format_error& e) {
            formatted = std::string(message) + " [Format error: " + e.what() + "]";
        }
        enqueueMessage(level, message, category, std::move(formatted), std::source_location::current());
    }

    // -----------------------------------------------------------------------
    //  Camera
    // -----------------------------------------------------------------------
    void log(LogLevel level, std::string_view category, const Camera& camera, std::string_view message = "") const {
        if (!shouldLog(level, category)) return;
        std::string formatted;
        try {
            formatted = std::format("Camera{{position: {}, viewMatrix: {}}}", 
                                   glm::to_string(camera.getViewMatrix()[3]), 
                                   glm::to_string(camera.getViewMatrix()));
            if (!message.empty()) {
                formatted = std::format("{}: {}", message, formatted);
            }
        } catch (const std::format_error& e) {
            formatted = std::string(message) + " [Format error: " + e.what() + "]";
        }
        enqueueMessage(level, message, category, std::move(formatted), std::source_location::current());
    }

    // -----------------------------------------------------------------------
    //  Configuration
    // -----------------------------------------------------------------------
    void setLogLevel(LogLevel level) {
        level_.store(level, std::memory_order_relaxed);
        if (ENABLE_INFO) {
            log(LogLevel::Info, "General", "Log level set to: {}", static_cast<int>(level));
        }
    }

    bool setLogFile(const std::string& filename, size_t maxSizeBytes = 10 * 1024 * 1024) {
        if (logFile_.is_open()) logFile_.close();
        logFile_.open(filename, std::ios::out | std::ios::app);
        if (!logFile_.is_open()) {
            if (ENABLE_ERROR) log(LogLevel::Error, "General", "Failed to open log file: {}", filename);
            return false;
        }
        logFilePath_ = filename;
        maxLogFileSize_ = maxSizeBytes;
        if (ENABLE_INFO) log(LogLevel::Info, "General", "Log file set to: {}", filename);
        return true;
    }

    void setCategoryFilter(std::string_view category, bool enable) {
        if (enable) enabledCategories_.insert(std::string(category));
        else enabledCategories_.erase(std::string(category));
        if (ENABLE_INFO) log(LogLevel::Info, "General", "Category {} {}", category, (enable ? "enabled" : "disabled"));
    }

    void stop() {
        if (running_.exchange(false)) {
            worker_->request_stop();
            worker_->join();
            flushQueue();
        }
    }

private:
    static constexpr size_t QueueSize = 1024;
    static constexpr size_t MaxFiles = 5;
    static constexpr size_t AggressiveDropThreshold = QueueSize / 2;

    static LogLevel getDefaultLogLevel() {
        if (const char* levelStr = std::getenv("AMOURANTH_LOG_LEVEL")) {
            std::string level(levelStr);
            if (level == "Trace") return LogLevel::Trace;
            if (level == "Debug") return LogLevel::Debug;
            if (level == "Info") return LogLevel::Info;
            if (level == "Warning") return LogLevel::Warning;
            if (level == "Error") return LogLevel::Error;
        }
        return LogLevel::Info;
    }

    static std::string getDefaultLogFile() {
        if (const char* file = std::getenv("AMOURANTH_LOG_FILE")) return std::string(file);
        return "";
    }

    static std::string_view getCategoryColor(std::string_view category) {
        static const std::map<std::string_view, std::string_view, std::less<>> categoryColors = {
            {"General", WHITE},
            {"Vulkan", BLUE},
            {"SIMULATION", GOLDEN_BROWN},
            {"Renderer", ORANGE},
            {"Engine", GREEN},
            {"Audio", TEAL},
            {"Image", YELLOW_GREEN},
            {"Input", BRIGHT_MAGENTA},
            {"FPS", BRIGHT_MAGENTA},
            {"BufferMgr", TEAL},
            {"MeshLoader", YELLOW_GREEN}
        };
        auto it = categoryColors.find(category);
        return it != categoryColors.end() ? it->second : WHITE;
    }

    bool shouldLog(LogLevel level, std::string_view category) const {
        switch (level) {
            case LogLevel::Trace:   if (!ENABLE_TRACE) return false; break;
            case LogLevel::Debug:   if (!ENABLE_DEBUG) return false; break;
            case LogLevel::Info:    if (!ENABLE_INFO) return false; break;
            case LogLevel::Warning: if (!ENABLE_WARNING) return false; break;
            case LogLevel::Error:   if (!ENABLE_ERROR) return false; break;
        }
        if (static_cast<int>(level) < static_cast<int>(level_.load(std::memory_order_relaxed))) return false;
        return enabledCategories_.empty() || enabledCategories_.contains(std::string(category));
    }

    void loadCategoryFilters() {
        if (const char* categories = std::getenv("AMOURANTH_LOG_CATEGORIES")) {
            std::string cats(categories);
            std::string_view catsView(cats);
            size_t start = 0, end;
            while ((end = catsView.find(',', start)) != std::string_view::npos) {
                std::string_view cat = catsView.substr(start, end - start);
                cat.remove_prefix(std::min(cat.find_first_not_of(" "), cat.size()));
                cat.remove_suffix(std::min(cat.size() - cat.find_last_not_of(" ") - 1, cat.size()));
                if (!cat.empty()) enabledCategories_.insert(std::string(cat));
                start = end + 1;
            }
            std::string_view cat = catsView.substr(start);
            cat.remove_prefix(std::min(cat.find_first_not_of(" "), cat.size()));
            cat.remove_suffix(std::min(cat.size() - cat.find_last_not_of(" ") - 1, cat.size()));
            if (!cat.empty()) enabledCategories_.insert(std::string(cat));
        }
    }

    void enqueueMessage(LogLevel level, std::string_view message, std::string_view category,
                        std::string formatted, const std::source_location& location) const {
        size_t currentHead = head_.load(std::memory_order_relaxed);
        size_t currentTail = tail_.load(std::memory_order_acquire);
        size_t nextHead = (currentHead + 1) % QueueSize;
        size_t currentSize = (currentHead >= currentTail) ? (currentHead - currentTail) : (QueueSize - currentTail + currentHead);

        if (nextHead == currentTail || currentSize >= QueueSize) {
            size_t dropCount = std::max(static_cast<size_t>(1), currentSize / 2);
            size_t newTail = (currentTail + dropCount) % QueueSize;
            tail_.store(newTail, std::memory_order_release);
            if (ENABLE_ERROR) {
                std::osyncstream(std::cerr) << RED << "[ERROR] [0.000us] [Logger] Log queue overwhelmed, dropping " << dropCount << " messages" << RESET << std::endl;
            }
            currentTail = newTail;
        }

        auto now = std::chrono::steady_clock::now();
        logQueue_[currentHead] = LogMessage(level, message, category, location, now);
        logQueue_[currentHead].formattedMessage = std::move(formatted);
        if (!firstLogTime_.has_value()) firstLogTime_ = now;
        head_.store(nextHead, std::memory_order_release);
    }

    void processLogQueue(std::stop_token stoken) {
        while (running_.load(std::memory_order_relaxed) || head_.load(std::memory_order_acquire) != tail_.load(std::memory_order_acquire)) {
            if (stoken.stop_requested() && head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire)) break;

            std::vector<LogMessage> batch;
            {
                size_t currentTail = tail_.load(std::memory_order_relaxed);
                size_t currentHead = head_.load(std::memory_order_acquire);
                size_t batchSize = (currentHead >= currentTail) ? (currentHead - currentTail) : (QueueSize - currentTail + currentHead);
                batchSize = std::min(batchSize, static_cast<size_t>(100));
                batch.reserve(batchSize);
                for (size_t i = 0; i < batchSize; ++i) {
                    batch.push_back(std::move(logQueue_[currentTail]));
                    currentTail = (currentTail + 1) % QueueSize;
                }
                tail_.store(currentTail, std::memory_order_release);
            }

            if (logFile_.is_open() && std::filesystem::file_size(logFilePath_) > maxLogFileSize_) {
                logFile_.close();
                rotateLogFile();
                logFile_.open(logFilePath_, std::ios::out | std::ios::app);
            }

            for (const auto& msg : batch) {
                std::string_view categoryColor = getCategoryColor(msg.category);
                std::string_view levelStr, levelColor;
                switch (msg.level) {
                    case LogLevel::Trace:   levelStr = "[TRACE]"; levelColor = NEON_LIME; break;
                    case LogLevel::Debug:   levelStr = "[DEBUG]"; levelColor = CYAN; break;
                    case LogLevel::Info:    levelStr = "[INFO]";  levelColor = GRAY; break;
                    case LogLevel::Warning: levelStr = "[WARN]";  levelColor = YELLOW; break;
                    case LogLevel::Error:   levelStr = "[ERROR]"; levelColor = MAGENTA; break;
                }

                auto delta = std::chrono::duration_cast<std::chrono::microseconds>(msg.timestamp - *firstLogTime_).count();
                std::string timeStr;
                if (delta < 10000) timeStr = std::format("{:>6}us", delta);
                else if (delta < 1000000) timeStr = std::format("{:>6.3f}ms", delta / 1000.0);
                else if (delta < 60000000) timeStr = std::format("{:>6.3f}s", delta / 1000000.0);
                else if (delta < 3600000000LL) timeStr = std::format("{:>6.3f}m", delta / 60000000.0);
                else timeStr = std::format("{:>6.3f}h", delta / 3600000000.0);

                std::string output;
                try {
                    output = std::format("{}{} [{}] {}[{}]{} {}{}",
                                         levelColor, levelStr, timeStr, categoryColor, msg.category, RESET,
                                         msg.formattedMessage.empty() ? "[Empty message]" : msg.formattedMessage, RESET);
                } catch (const std::format_error& e) {
                    output = std::format("[ERROR] [{}] [Logger] Format error: {}", timeStr, e.what());
                }

                std::osyncstream(std::cout) << output << std::endl;
                if (logFile_.is_open()) std::osyncstream(logFile_) << output << std::endl;
            }
        }
    }

    void rotateLogFile() {
        std::time_t now = std::time(nullptr);
        char timestamp[20];
        std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", std::localtime(&now));
        std::string newFile = std::format("{}.{}.log", logFilePath_.stem().string(), timestamp);
        std::filesystem::rename(logFilePath_, logFilePath_.parent_path() / newFile);

        std::vector<std::filesystem::path> logs;
        std::string stem = logFilePath_.stem().string();
        for (const auto& entry : std::filesystem::directory_iterator(logFilePath_.parent_path())) {
            if (entry.path().extension() == ".log" && entry.path().stem().string().rfind(stem, 0) == 0) {
                logs.push_back(entry.path());
            }
        }
        std::sort(logs.begin(), logs.end(), [](const auto& a, const auto& b) {
            return std::filesystem::last_write_time(a) < std::filesystem::last_write_time(b);
        });
        while (logs.size() > MaxFiles) {
            std::filesystem::remove(logs.front());
            logs.erase(logs.begin());
        }
    }

    void flushQueue() {
        std::vector<LogMessage> batch;
        {
            size_t currentTail = tail_.load(std::memory_order_relaxed);
            size_t currentHead = head_.load(std::memory_order_acquire);
            size_t batchSize = (currentHead >= currentTail) ? (currentHead - currentTail) : (QueueSize - currentTail + currentHead);
            batch.reserve(batchSize);
            for (size_t i = 0; i < batchSize; ++i) {
                batch.push_back(std::move(logQueue_[currentTail]));
                currentTail = (currentTail + 1) % QueueSize;
            }
            tail_.store(currentTail, std::memory_order_release);
        }
        for (const auto& msg : batch) {
            std::string_view categoryColor = getCategoryColor(msg.category);
            std::string_view levelStr, levelColor;
            switch (msg.level) {
                case LogLevel::Trace:   levelStr = "[TRACE]"; levelColor = NEON_LIME; break;
                case LogLevel::Debug:   levelStr = "[DEBUG]"; levelColor = CYAN; break;
                case LogLevel::Info:    levelStr = "[INFO]";  levelColor = GRAY; break;
                case LogLevel::Warning: levelStr = "[WARN]";  levelColor = YELLOW; break;
                case LogLevel::Error:   levelStr = "[ERROR]"; levelColor = MAGENTA; break;
            }
            auto delta = std::chrono::duration_cast<std::chrono::microseconds>(msg.timestamp - *firstLogTime_).count();
            std::string timeStr;
            if (delta < 10000) timeStr = std::format("{:>6}us", delta);
            else if (delta < 1000000) timeStr = std::format("{:>6.3f}ms", delta / 1000.0);
            else if (delta < 60000000) timeStr = std::format("{:>6.3f}s", delta / 1000000.0);
            else if (delta < 3600000000LL) timeStr = std::format("{:>6.3f}m", delta / 60000000.0);
            else timeStr = std::format("{:>6.3f}h", delta / 3600000000.0);
            std::string output;
            try {
                output = std::format("{}{} [{}] {}[{}]{} {}{}",
                                     levelColor, levelStr, timeStr, categoryColor, msg.category, RESET,
                                     msg.formattedMessage.empty() ? "[Empty message]" : msg.formattedMessage, RESET);
            } catch (const std::format_error& e) {
                output = std::format("[ERROR] [{}] [Logger] Format error: {}", timeStr, e.what());
            }
            std::osyncstream(std::cout) << output << std::endl;
            if (logFile_.is_open()) std::osyncstream(logFile_) << output << std::endl;
        }
    }

    mutable std::array<LogMessage, QueueSize> logQueue_;
    mutable std::atomic<size_t> head_;
    mutable std::atomic<size_t> tail_;
    std::atomic<bool> running_;
    std::atomic<LogLevel> level_;
    mutable std::ofstream logFile_;
    std::filesystem::path logFilePath_;
    size_t maxLogFileSize_;
    std::set<std::string> enabledCategories_;
    std::unique_ptr<std::jthread> worker_;
    mutable std::optional<std::chrono::steady_clock::time_point> firstLogTime_;
};

} // namespace Logging

// ---------------------------------------------------------------------------
//  std::formatters
// ---------------------------------------------------------------------------
namespace std {

template<>
struct formatter<std::source_location, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const std::source_location& loc, FormatContext& ctx) const {
        return format_to(ctx.out(), "{}:{}:{}", loc.file_name(), loc.line(), loc.function_name());
    }
};

template<glm::length_t L, typename T, glm::qualifier Q>
struct formatter<glm::vec<L, T, Q>, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const glm::vec<L, T, Q>& vec, FormatContext& ctx) const {
        return format_to(ctx.out(), "{}", glm::to_string(vec));
    }
};

template<glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
struct formatter<glm::mat<C, R, T, Q>, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const glm::mat<C, R, T, Q>& mat, FormatContext& ctx) const {
        return format_to(ctx.out(), "{}", glm::to_string(mat));
    }
};

template<typename T>
requires (
    std::same_as<T, VkBuffer> || std::same_as<T, VkCommandBuffer> ||
    std::same_as<T, VkPipelineLayout> || std::same_as<T, VkDescriptorSet> ||
    std::same_as<T, VkRenderPass> || std::same_as<T, VkFramebuffer> ||
    std::same_as<T, VkImage> || std::same_as<T, VkDeviceMemory> ||
    std::same_as<T, VkDevice> || std::same_as<T, VkQueue> ||
    std::same_as<T, VkCommandPool> || std::same_as<T, VkPipeline> ||
    std::same_as<T, VkSwapchainKHR> || std::same_as<T, VkShaderModule> ||
    std::same_as<T, VkSemaphore> || std::same_as<T, VkFence> ||
    std::same_as<T, VkSurfaceKHR> || std::same_as<T, VkImageView> ||
    std::same_as<T, VkDescriptorSetLayout> || std::same_as<T, VkInstance> ||
    std::same_as<T, VkSampler> || std::same_as<T, VkDescriptorPool> ||
    std::same_as<T, VkAccelerationStructureKHR> || std::same_as<T, VkPhysicalDevice>
)
struct formatter<T, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(T ptr, FormatContext& ctx) const {
        if (ptr == VK_NULL_HANDLE) return format_to(ctx.out(), "VK_NULL_HANDLE");
        return format_to(ctx.out(), "{:p}", static_cast<const void*>(reinterpret_cast<const void*>(ptr)));
    }
};

template<>
struct formatter<VkExtent2D, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const VkExtent2D& extent, FormatContext& ctx) const {
        return format_to(ctx.out(), "{{width: {}, height: {}}}", extent.width, extent.height);
    }
};

template<>
struct formatter<VkViewport, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const VkViewport& viewport, FormatContext& ctx) const {
        return format_to(ctx.out(), "{{x: {:.1f}, y: {:.1f}, width: {:.1f}, height: {:.1f}, minDepth: {:.1f}, maxDepth: {:.1f}}}",
                        viewport.x, viewport.y, viewport.width, viewport.height, viewport.minDepth, viewport.maxDepth);
    }
};

template<>
struct formatter<VkRect2D, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const VkRect2D& rect, FormatContext& ctx) const {
        return format_to(ctx.out(), "{{offset: {{x: {}, y: {}}}, extent: {{width: {}, height: {}}}}}",
                        rect.offset.x, rect.offset.y, rect.extent.width, rect.extent.height);
    }
};

template<>
struct formatter<VkFormat, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(VkFormat format, FormatContext& ctx) const {
        switch (format) {
            case VK_FORMAT_UNDEFINED: return format_to(ctx.out(), "VK_FORMAT_UNDEFINED");
            case VK_FORMAT_R8G8B8A8_UNORM: return format_to(ctx.out(), "VK_FORMAT_R8G8B8A8_UNORM");
            case VK_FORMAT_B8G8R8A8_SRGB: return format_to(ctx.out(), "VK_FORMAT_B8G8R8A8_SRGB");
            default: return format_to(ctx.out(), "VkFormat({})", static_cast<int>(format));
        }
    }
};

template<>
struct formatter<VkResult, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(VkResult result, FormatContext& ctx) const {
        switch (result) {
            case VK_SUCCESS: return format_to(ctx.out(), "VK_SUCCESS");
            case VK_ERROR_OUT_OF_HOST_MEMORY: return format_to(ctx.out(), "VK_ERROR_OUT_OF_HOST_MEMORY");
            case VK_ERROR_DEVICE_LOST: return format_to(ctx.out(), "VK_ERROR_DEVICE_LOST");
            default: return format_to(ctx.out(), "VkResult({})", static_cast<int>(result));
        }
    }
};

template<>
struct formatter<std::thread::id, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(std::thread::id id, FormatContext& ctx) const {
        std::ostringstream oss;
        oss << id;
        return format_to(ctx.out(), "{}", oss.str());
    }
};

template<>
struct formatter<SDL_Scancode, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(SDL_Scancode sc, FormatContext& ctx) const {
        const char* name = SDL_GetScancodeName(sc);
        return format_to(ctx.out(), "{} ({})", static_cast<int>(sc), name ? name : "UNKNOWN");
    }
};

} // namespace std

#endif // ENGINE_LOGGING_HPP