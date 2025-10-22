// AMOURANTH RTX Engine, October 2025 - Enhanced thread-safe, asynchronous logging.
// Thread-safe, asynchronous logging with ANSI-colored output and delta time.
// Supports C++20 std::format, std::jthread, OpenMP, and lock-free queue with std::atomic.
// No mutexes for queue; designed for high-performance Vulkan applications on Windows and Linux.
// Delta time format: microseconds (<10ms), milliseconds (10ms-1s), seconds (1s-1min), minutes (1min-1hr), hours (>1hr).
// Usage: LOG_INFO("Message: {}", value); or Logger::get().log(LogLevel::Info, "Vulkan", "Message: {}", value);
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
#include <mutex>
#include <set>
#include <map>
#include <span>
#include <glm/glm.hpp>
#include <glm/gtx/string_cast.hpp>
#include <vulkan/vulkan.h>
#include <SDL3/SDL.h>
#include "engine/camera.hpp" // For Camera base class

// Log level toggle flags (enable/disable specific log levels)
constexpr bool ENABLE_DEBUG = true;   // Debug logs disabled by default
constexpr bool ENABLE_INFO = true;     // Info logs enabled by default
constexpr bool ENABLE_WARNING = true; // Warning logs disabled by default
constexpr bool ENABLE_ERROR = true;    // Error logs enabled by default
constexpr bool FPS_COUNTER = true;     // FPS counter logging enabled by default
constexpr bool SIMULATION_LOGGING = true; // Simulation logging enabled by default

#define LOG_DEBUG(...) do { if (ENABLE_DEBUG) Logging::Logger::get().log(Logging::LogLevel::Debug, "General", __VA_ARGS__); } while (0)
#define LOG_INFO(...) do { if (ENABLE_INFO) Logging::Logger::get().log(Logging::LogLevel::Info, "General", __VA_ARGS__); } while (0)
#define LOG_FPS_COUNTER(...) do { if (FPS_COUNTER) Logging::Logger::get().log(Logging::LogLevel::Info, "FPS", __VA_ARGS__); } while (0)
#define LOG_SIMULATION(...) do { if (SIMULATION_LOGGING) Logging::Logger::get().log(Logging::LogLevel::Info, "SIMULATION", __VA_ARGS__); } while (0)
#define LOG_WARNING(...) do { if (ENABLE_WARNING) Logging::Logger::get().log(Logging::LogLevel::Warning, "General", __VA_ARGS__); } while (0)
#define LOG_ERROR(...) do { if (ENABLE_ERROR) Logging::Logger::get().log(Logging::LogLevel::Error, "General", __VA_ARGS__); } while (0)
#define LOG_DEBUG_CAT(category, ...) do { if (ENABLE_DEBUG) Logging::Logger::get().log(Logging::LogLevel::Debug, category, __VA_ARGS__); } while (0)
#define LOG_INFO_CAT(category, ...) do { if (ENABLE_INFO) Logging::Logger::get().log(Logging::LogLevel::Info, category, __VA_ARGS__); } while (0)
#define LOG_WARNING_CAT(category, ...) do { if (ENABLE_WARNING) Logging::Logger::get().log(Logging::LogLevel::Warning, category, __VA_ARGS__); } while (0)
#define LOG_ERROR_CAT(category, ...) do { if (ENABLE_ERROR) Logging::Logger::get().log(Logging::LogLevel::Error, category, __VA_ARGS__); } while (0)

namespace Logging {

enum class LogLevel { Debug, Info, Warning, Error };

// ANSI color codes
inline constexpr std::string_view RESET = "\033[0m";
inline constexpr std::string_view CYAN = "\033[1;36m";
inline constexpr std::string_view GREEN = "\033[1;32m";
inline constexpr std::string_view YELLOW = "\033[1;33m";
inline constexpr std::string_view MAGENTA = "\033[1;35m";
inline constexpr std::string_view BLUE = "\033[1;34m";
inline constexpr std::string_view RED = "\033[1;31m";
inline constexpr std::string_view WHITE = "\033[1;37m";
inline constexpr std::string_view PURPLE = "\033[1;35m";
inline constexpr std::string_view ORANGE = "\033[38;5;208m";
inline constexpr std::string_view TEAL = "\033[38;5;51m";
inline constexpr std::string_view YELLOW_GREEN = "\033[38;5;154m";
inline constexpr std::string_view BRIGHT_MAGENTA = "\033[38;5;201m";
inline constexpr std::string_view GOLDEN_BROWN = "\033[38;5;138m";

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

    template<typename... Args>
    void log(LogLevel level, std::string_view category, std::string_view message, const Args&... args) const {
        if (!shouldLog(level, category)) return;

        std::string formatted;
        try {
            bool argsValid = true;
            (
                [&] {
                    if constexpr (std::is_pointer_v<std::decay_t<Args>>) {
                        if (args == nullptr) {
                            argsValid = false;
                            formatted = std::string(message) + " [Invalid argument: nullptr detected]";
                        }
                    } else if constexpr (std::is_same_v<std::decay_t<Args>, std::string> ||
                                        std::is_same_v<std::decay_t<Args>, std::string_view>) {
                        if (args.empty()) {
                            argsValid = false;
                            formatted = std::string(message) + " [Invalid argument: empty string]";
                        }
                    } else if constexpr (std::is_same_v<std::decay_t<Args>, uint64_t>) {
                        if (args == UINT64_MAX) {
                            argsValid = false;
                            formatted = std::string(message) + " [Invalid argument: UINT64_MAX detected]";
                        }
                    }
                }(),
                ...
            );

            if (argsValid) {
                formatted = std::vformat(message, std::make_format_args(args...));
            }
        } catch (const std::format_error& e) {
            formatted = std::string(message) + " [Format error: " + e.what() + "]";
        }

        if (formatted.empty()) {
            formatted = "Empty log message";
        }

        enqueueMessage(level, message, category, formatted, std::source_location::current());
    }

    void log(LogLevel level, std::string_view category, std::string_view message) const {
        if (!shouldLog(level, category)) return;
        std::string formatted = message.empty() ? "Empty log message" : std::string(message);
        enqueueMessage(level, message, category, formatted, std::source_location::current());
    }

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
        enqueueMessage(level, message, category, formatted, std::source_location::current());
    }

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
        std::same_as<T, VkExtent2D> || std::same_as<T, VkViewport> ||
        std::same_as<T, VkRect2D>
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
        enqueueMessage(level, handleName, category, formatted, std::source_location::current());
    }

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
                if (i < handles.size() - 1) {
                    formatted += ", ";
                }
            }
            formatted += "}";
        } catch (const std::format_error& e) {
            formatted = std::string(handleName) + " [Format error: " + e.what() + "]";
        }
        enqueueMessage(level, handleName, category, formatted, std::source_location::current());
    }

    void log(LogLevel level, std::string_view category, const glm::vec3& vec, std::string_view message = "") const {
        if (!shouldLog(level, category)) return;
        std::string formatted;
        try {
            formatted = glm::to_string(vec);
            if (!message.empty()) {
                formatted = std::format("{}: {}", message, formatted);
            }
        } catch (const std::format_error& e) {
            formatted = std::string(message) + " [Format error: " + e.what() + "]";
        }
        enqueueMessage(level, message, category, formatted, std::source_location::current());
    }

    void log(LogLevel level, std::string_view category, const glm::vec2& vec, std::string_view message = "") const {
        if (!shouldLog(level, category)) return;
        std::string formatted;
        try {
            formatted = glm::to_string(vec);
            if (!message.empty()) {
                formatted = std::format("{}: {}", message, formatted);
            }
        } catch (const std::format_error& e) {
            formatted = std::string(message) + " [Format error: " + e.what() + "]";
        }
        enqueueMessage(level, message, category, formatted, std::source_location::current());
    }

    void log(LogLevel level, std::string_view category, const glm::mat4& mat, std::string_view message = "") const {
        if (!shouldLog(level, category)) return;
        std::string formatted;
        try {
            formatted = glm::to_string(mat);
            if (!message.empty()) {
                formatted = std::format("{}: {}", message, formatted);
            }
        } catch (const std::format_error& e) {
            formatted = std::string(message) + " [Format error: " + e.what() + "]";
        }
        enqueueMessage(level, message, category, formatted, std::source_location::current());
    }

    void log(LogLevel level, std::string_view category, std::span<const glm::vec3> vecs, std::string_view message = "") const {
        if (!shouldLog(level, category)) return;
        std::string formatted;
        try {
            formatted = std::format("{}[{}]{{", message, vecs.size());
            for (size_t i = 0; i < vecs.size(); ++i) {
                formatted += glm::to_string(vecs[i]);
                if (i < vecs.size() - 1) {
                    formatted += ", ";
                }
            }
            formatted += "}";
        } catch (const std::format_error& e) {
            formatted = std::string(message) + " [Format error: " + e.what() + "]";
        }
        enqueueMessage(level, message, category, formatted, std::source_location::current());
    }

    void setLogLevel(LogLevel level) {
        level_.store(level, std::memory_order_relaxed);
        if (ENABLE_INFO) {
            log(LogLevel::Info, "General", "Log level set to: {}", static_cast<int>(level));
        }
    }

    bool setLogFile(const std::string& filename, size_t maxSizeBytes = 10 * 1024 * 1024) {
        std::lock_guard<std::mutex> lock(fileMutex_);
        if (logFile_.is_open()) {
            logFile_.close();
        }
        logFile_.open(filename, std::ios::out | std::ios::app);
        if (!logFile_.is_open()) {
            if (ENABLE_ERROR) {
                log(LogLevel::Error, "General", "Failed to open log file: {}", filename);
            }
            return false;
        }
        logFilePath_ = filename;
        maxLogFileSize_ = maxSizeBytes;
        if (ENABLE_INFO) {
            log(LogLevel::Info, "General", "Log file set to: {}", filename);
        }
        return true;
    }

    void setCategoryFilter(std::string_view category, bool enable) {
        std::lock_guard<std::mutex> lock(categoryMutex_);
        if (enable) {
            enabledCategories_.insert(std::string(category));
        } else {
            enabledCategories_.erase(std::string(category));
        }
        if (ENABLE_INFO) {
            log(LogLevel::Info, "General", "Category {} {}", category, (enable ? "enabled" : "disabled"));
        }
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
    static constexpr size_t AggressiveDropThreshold = QueueSize / 2; // 50% of queue size

    static LogLevel getDefaultLogLevel() {
        if (const char* levelStr = std::getenv("AMOURANTH_LOG_LEVEL")) {
            std::string level(levelStr);
            if (level == "Debug") return LogLevel::Debug;
            if (level == "Info") return LogLevel::Info;
            if (level == "Warning") return LogLevel::Warning;
            if (level == "Error") return LogLevel::Error;
        }
        return LogLevel::Info; // Default to Info
    }

    static std::string getDefaultLogFile() {
        if (const char* file = std::getenv("AMOURANTH_LOG_FILE")) {
            return std::string(file);
        }
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
            {"FPS", BRIGHT_MAGENTA}
        };
        auto it = categoryColors.find(category);
        return it != categoryColors.end() ? it->second : WHITE;
    }

    bool shouldLog(LogLevel level, std::string_view category) const {
        // Check toggle flags first
        switch (level) {
            case LogLevel::Debug:   if (!ENABLE_DEBUG) return false; break;
            case LogLevel::Info:    if (!ENABLE_INFO) return false; break;
            case LogLevel::Warning: if (!ENABLE_WARNING) return false; break;
            case LogLevel::Error:   if (!ENABLE_ERROR) return false; break;
        }

        // Then check dynamic log level and category filters
        if (static_cast<int>(level) < static_cast<int>(level_.load(std::memory_order_relaxed))) {
            return false;
        }
        std::lock_guard<std::mutex> lock(categoryMutex_);
        return enabledCategories_.empty() || enabledCategories_.contains(std::string(category));
    }

    void loadCategoryFilters() {
        if (const char* categories = std::getenv("AMOURANTH_LOG_CATEGORIES")) {
            std::string cats(categories);
            std::string_view catsView(cats);
            size_t start = 0;
            size_t end;
            while ((end = catsView.find(',', start)) != std::string_view::npos) {
                std::string_view cat = catsView.substr(start, end - start);
                cat.remove_prefix(std::min(cat.find_first_not_of(" "), cat.size()));
                cat.remove_suffix(std::min(cat.size() - cat.find_last_not_of(" ") - 1, cat.size()));
                if (!cat.empty()) {
                    enabledCategories_.insert(std::string(cat));
                }
                start = end + 1;
            }
            std::string_view cat = catsView.substr(start);
            cat.remove_prefix(std::min(cat.find_first_not_of(" "), cat.size()));
            cat.remove_suffix(std::min(cat.size() - cat.find_last_not_of(" ") - 1, cat.size()));
            if (!cat.empty()) {
                enabledCategories_.insert(std::string(cat));
            }
        }
    }

    void enqueueMessage(LogLevel level, std::string_view message, std::string_view category,
                        std::string formatted, const std::source_location& location) const {
        std::lock_guard<std::mutex> lock(queueMutex_); // Added mutex for thread-safe enqueue
        size_t currentHead = head_.load(std::memory_order_relaxed);
        size_t currentTail = tail_.load(std::memory_order_acquire);
        size_t nextHead = (currentHead + 1) % QueueSize;
        
        // Calculate current queue size
        size_t currentSize = (currentHead >= currentTail) ? (currentHead - currentTail) : (QueueSize - currentTail + currentHead);
        
        if (nextHead == currentTail || currentSize >= QueueSize) {
            // Queue is full or would overflow; aggressively drop 50% of the queue
            size_t dropCount = std::max(static_cast<size_t>(1), currentSize / 2);
            size_t newTail = (currentTail + dropCount) % QueueSize;
            tail_.store(newTail, std::memory_order_release);
            
            if (ENABLE_ERROR) {
                std::osyncstream(std::cerr) << RED << "[ERROR] [0.000us] [Logger] Log queue overwhelmed, zealously dropping " << dropCount << " (" << (dropCount * 100 / currentSize) << "%) oldest messages to free space" << RESET << std::endl;
            }
            
            // Update currentTail for this enqueue
            currentTail = newTail;
            currentSize -= dropCount;
        }

        auto now = std::chrono::steady_clock::now();
        logQueue_[currentHead] = LogMessage(level, message, category, location, now);
        logQueue_[currentHead].formattedMessage = std::move(formatted);
        if (!firstLogTime_.has_value()) {
            firstLogTime_ = now;
        }
        head_.store(nextHead, std::memory_order_release);
    }

    void processLogQueue(std::stop_token stoken) {
        while (running_.load(std::memory_order_relaxed) || head_.load(std::memory_order_acquire) != tail_.load(std::memory_order_acquire)) {
            if (stoken.stop_requested() && head_.load(std::memory_order_acquire) == tail_.load(std::memory_order_acquire)) {
                break;
            }

            std::vector<LogMessage> batch;
            {
                std::lock_guard<std::mutex> lock(queueMutex_); // Added mutex for thread-safe dequeue
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

            if (logFile_.is_open()) {
                std::lock_guard<std::mutex> lock(fileMutex_);
                if (std::filesystem::file_size(logFilePath_) > maxLogFileSize_) {
                    logFile_.close();
                    rotateLogFile();
                    logFile_.open(logFilePath_, std::ios::out | std::ios::app);
                }
            }

            for (const auto& msg : batch) {
                std::string_view categoryColor = getCategoryColor(msg.category);
                std::string_view levelStr;
                std::string_view levelColor;
                switch (msg.level) {
                    case LogLevel::Debug:   levelStr = "[DEBUG]"; levelColor = CYAN; break;
                    case LogLevel::Info:    levelStr = "[INFO]";  levelColor = GREEN; break;
                    case LogLevel::Warning: levelStr = "[WARN]";  levelColor = YELLOW; break;
                    case LogLevel::Error:   levelStr = "[ERROR]"; levelColor = MAGENTA; break;
                }

                auto delta = std::chrono::duration_cast<std::chrono::microseconds>(msg.timestamp - *firstLogTime_).count();
                std::string timeStr;
                if (delta < 10000) {
                    timeStr = std::format("{:>6}us", delta);
                } else if (delta < 1000000) {
                    timeStr = std::format("{:>6.3f}ms", delta / 1000.0);
                } else if (delta < 60000000) {
                    timeStr = std::format("{:>6.3f}s", delta / 1000000.0);
                } else if (delta < 3600000000LL) {
                    timeStr = std::format("{:>6.3f}m", delta / 60000000.0);
                } else {
                    timeStr = std::format("{:>6.3f}h", delta / 3600000000.0);
                }

                std::string output;
                try {
                    output = std::format("{}{} [{}] {}[{}]{} {}{}",
                                         levelColor, levelStr, timeStr, categoryColor, msg.category, RESET,
                                         msg.formattedMessage.empty() ? "[Empty message]" : msg.formattedMessage, RESET);
                } catch (const std::format_error& e) {
                    output = std::format("[ERROR] [{}] [Logger] Format error in log output: {}", timeStr, e.what());
                }

                std::osyncstream(std::cout) << output << std::endl;
                if (logFile_.is_open()) {
                    std::lock_guard<std::mutex> lock(fileMutex_);
                    std::osyncstream(logFile_) << output << std::endl;
                }
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
            std::lock_guard<std::mutex> lock(queueMutex_);
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
            std::string_view levelStr;
            std::string_view levelColor;
            switch (msg.level) {
                case LogLevel::Debug:   levelStr = "[DEBUG]"; levelColor = CYAN; break;
                case LogLevel::Info:    levelStr = "[INFO]";  levelColor = GREEN; break;
                case LogLevel::Warning: levelStr = "[WARN]";  levelColor = YELLOW; break;
                case LogLevel::Error:   levelStr = "[ERROR]"; levelColor = MAGENTA; break;
            }

            auto delta = std::chrono::duration_cast<std::chrono::microseconds>(msg.timestamp - *firstLogTime_).count();
            std::string timeStr;
            if (delta < 10000) {
                timeStr = std::format("{:>6}us", delta);
            } else if (delta < 1000000) {
                timeStr = std::format("{:>6.3f}ms", delta / 1000.0);
            } else if (delta < 60000000) {
                timeStr = std::format("{:>6.3f}s", delta / 1000000.0);
            } else if (delta < 3600000000LL) {
                timeStr = std::format("{:>6.3f}m", delta / 60000000.0);
            } else {
                timeStr = std::format("{:>6.3f}h", delta / 3600000000.0);
            }

            std::string output;
            try {
                output = std::format("{}{} [{}] {}[{}]{} {}{}",
                                     levelColor, levelStr, timeStr, categoryColor, msg.category, RESET,
                                     msg.formattedMessage.empty() ? "[Empty message]" : msg.formattedMessage, RESET);
            } catch (const std::format_error& e) {
                output = std::format("[ERROR] [{}] [Logger] Format error in log output: {}", timeStr, e.what());
            }

            std::osyncstream(std::cout) << output << std::endl;
            if (logFile_.is_open()) {
                std::lock_guard<std::mutex> lock(fileMutex_);
                std::osyncstream(logFile_) << output << std::endl;
            }
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
    mutable std::mutex fileMutex_;
    mutable std::mutex categoryMutex_;
    mutable std::mutex queueMutex_; // Added for thread-safe queue access
    std::set<std::string> enabledCategories_;
    std::unique_ptr<std::jthread> worker_;
    mutable std::optional<std::chrono::steady_clock::time_point> firstLogTime_;
};

} // namespace Logging

namespace std {
// Formatter for std::source_location
template<>
struct formatter<std::source_location, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const std::source_location& loc, FormatContext& ctx) const {
        return format_to(ctx.out(), "{}:{}:{}", loc.file_name(), loc.line(), loc.function_name());
    }
};

// Formatter for GLM vector
template<glm::length_t L, typename T, glm::qualifier Q>
struct formatter<glm::vec<L, T, Q>, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const glm::vec<L, T, Q>& vec, FormatContext& ctx) const {
        return format_to(ctx.out(), "{}", glm::to_string(vec));
    }
};

// Formatter for GLM matrix
template<glm::length_t C, glm::length_t R, typename T, glm::qualifier Q>
struct formatter<glm::mat<C, R, T, Q>, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const glm::mat<C, R, T, Q>& mat, FormatContext& ctx) const {
        return format_to(ctx.out(), "{}", glm::to_string(mat));
    }
};

// Formatter for Vulkan handles
template<typename T>
requires (
    std::same_as<T, VkBuffer> ||
    std::same_as<T, VkCommandBuffer> ||
    std::same_as<T, VkPipelineLayout> ||
    std::same_as<T, VkDescriptorSet> ||
    std::same_as<T, VkRenderPass> ||
    std::same_as<T, VkFramebuffer> ||
    std::same_as<T, VkImage> ||
    std::same_as<T, VkDeviceMemory> ||
    std::same_as<T, VkDevice> ||
    std::same_as<T, VkQueue> ||
    std::same_as<T, VkCommandPool> ||
    std::same_as<T, VkPipeline> ||
    std::same_as<T, VkSwapchainKHR> ||
    std::same_as<T, VkShaderModule> ||
    std::same_as<T, VkSemaphore> ||
    std::same_as<T, VkFence> ||
    std::same_as<T, VkSurfaceKHR> ||
    std::same_as<T, VkImageView> ||
    std::same_as<T, VkDescriptorSetLayout> ||
    std::same_as<T, VkInstance> ||
    std::same_as<T, VkSampler> ||
    std::same_as<T, VkDescriptorPool> ||
    std::same_as<T, VkAccelerationStructureKHR> ||
    std::same_as<T, VkPhysicalDevice>
)
struct formatter<T, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(T ptr, FormatContext& ctx) const {
        if (ptr == VK_NULL_HANDLE) {
            return format_to(ctx.out(), "VK_NULL_HANDLE");
        }
        return format_to(ctx.out(), "{:p}", static_cast<const void*>(static_cast<void*>(const_cast<T*>(&ptr))));
    }
};

// Formatter for VkExtent2D
template<>
struct formatter<VkExtent2D, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const VkExtent2D& extent, FormatContext& ctx) const {
        return format_to(ctx.out(), "{{width: {}, height: {}}}", extent.width, extent.height);
    }
};

// Formatter for VkViewport
template<>
struct formatter<VkViewport, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const VkViewport& viewport, FormatContext& ctx) const {
        return format_to(ctx.out(), "{{x: {:.1f}, y: {:.1f}, width: {:.1f}, height: {:.1f}, minDepth: {:.1f}, maxDepth: {:.1f}}}",
                        viewport.x, viewport.y, viewport.width, viewport.height, viewport.minDepth, viewport.maxDepth);
    }
};

// Formatter for VkRect2D
template<>
struct formatter<VkRect2D, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const VkRect2D& rect, FormatContext& ctx) const {
        return format_to(ctx.out(), "{{offset: {{x: {}, y: {}}}, extent: {{width: {}, height: {}}}}}",
                        rect.offset.x, rect.offset.y, rect.extent.width, rect.extent.height);
    }
};

// Formatter for VkFormat
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

// Formatter for VkResult
template<>
struct formatter<VkResult, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(VkResult result, FormatContext& ctx) const {
        switch (result) {
            case VK_SUCCESS: return format_to(ctx.out(), "VK_SUCCESS");
            case VK_NOT_READY: return format_to(ctx.out(), "VK_NOT_READY");
            case VK_TIMEOUT: return format_to(ctx.out(), "VK_TIMEOUT");
            case VK_EVENT_SET: return format_to(ctx.out(), "VK_EVENT_SET");
            case VK_EVENT_RESET: return format_to(ctx.out(), "VK_EVENT_RESET");
            case VK_INCOMPLETE: return format_to(ctx.out(), "VK_INCOMPLETE");
            case VK_ERROR_OUT_OF_HOST_MEMORY: return format_to(ctx.out(), "VK_ERROR_OUT_OF_HOST_MEMORY");
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: return format_to(ctx.out(), "VK_ERROR_OUT_OF_DEVICE_MEMORY");
            case VK_ERROR_INITIALIZATION_FAILED: return format_to(ctx.out(), "VK_ERROR_INITIALIZATION_FAILED");
            case VK_ERROR_DEVICE_LOST: return format_to(ctx.out(), "VK_ERROR_DEVICE_LOST");
            case VK_ERROR_MEMORY_MAP_FAILED: return format_to(ctx.out(), "VK_ERROR_MEMORY_MAP_FAILED");
            case VK_ERROR_LAYER_NOT_PRESENT: return format_to(ctx.out(), "VK_ERROR_LAYER_NOT_PRESENT");
            case VK_ERROR_EXTENSION_NOT_PRESENT: return format_to(ctx.out(), "VK_ERROR_EXTENSION_NOT_PRESENT");
            case VK_ERROR_FEATURE_NOT_PRESENT: return format_to(ctx.out(), "VK_ERROR_FEATURE_NOT_PRESENT");
            case VK_ERROR_INCOMPATIBLE_DRIVER: return format_to(ctx.out(), "VK_ERROR_INCOMPATIBLE_DRIVER");
            case VK_ERROR_TOO_MANY_OBJECTS: return format_to(ctx.out(), "VK_ERROR_TOO_MANY_OBJECTS");
            case VK_ERROR_FORMAT_NOT_SUPPORTED: return format_to(ctx.out(), "VK_ERROR_FORMAT_NOT_SUPPORTED");
            case VK_ERROR_FRAGMENTED_POOL: return format_to(ctx.out(), "VK_ERROR_FRAGMENTED_POOL");
            case VK_ERROR_OUT_OF_POOL_MEMORY: return format_to(ctx.out(), "VK_ERROR_OUT_OF_POOL_MEMORY");
            case VK_ERROR_INVALID_EXTERNAL_HANDLE: return format_to(ctx.out(), "VK_ERROR_INVALID_EXTERNAL_HANDLE");
            case VK_ERROR_SURFACE_LOST_KHR: return format_to(ctx.out(), "VK_ERROR_SURFACE_LOST_KHR");
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return format_to(ctx.out(), "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR");
            case VK_SUBOPTIMAL_KHR: return format_to(ctx.out(), "VK_SUBOPTIMAL_KHR");
            case VK_ERROR_OUT_OF_DATE_KHR: return format_to(ctx.out(), "VK_ERROR_OUT_OF_DATE_KHR");
            case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return format_to(ctx.out(), "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR");
            case VK_ERROR_VALIDATION_FAILED_EXT: return format_to(ctx.out(), "VK_ERROR_VALIDATION_FAILED_EXT");
            case VK_ERROR_INVALID_SHADER_NV: return format_to(ctx.out(), "VK_ERROR_INVALID_SHADER_NV");
            case VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT:
                return format_to(ctx.out(), "VK_ERROR_INVALID_DRM_FORMAT_MODIFIER_PLANE_LAYOUT_EXT");
            case VK_ERROR_FRAGMENTATION_EXT: return format_to(ctx.out(), "VK_ERROR_FRAGMENTATION_EXT");
            case VK_ERROR_NOT_PERMITTED_EXT: return format_to(ctx.out(), "VK_ERROR_NOT_PERMITTED_EXT");
            case VK_ERROR_INVALID_DEVICE_ADDRESS_EXT: return format_to(ctx.out(), "VK_ERROR_INVALID_DEVICE_ADDRESS_EXT");
            case VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT:
                return format_to(ctx.out(), "VK_ERROR_FULL_SCREEN_EXCLUSIVE_MODE_LOST_EXT");
            case VK_ERROR_UNKNOWN: return format_to(ctx.out(), "VK_ERROR_UNKNOWN");
            default: return format_to(ctx.out(), "VkResult({})", static_cast<int>(result));
        }
    }
};

// Formatter for VkPhysicalDeviceProperties
template<>
struct formatter<VkPhysicalDeviceProperties, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const VkPhysicalDeviceProperties& props, FormatContext& ctx) const {
        return format_to(ctx.out(), "VkPhysicalDeviceProperties{{deviceName: {}}}", props.deviceName ? props.deviceName : "null");
    }
};

// Formatter for VkSurfaceCapabilitiesKHR
template<>
struct formatter<VkSurfaceCapabilitiesKHR, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(const VkSurfaceCapabilitiesKHR& caps, FormatContext& ctx) const {
        return format_to(ctx.out(), "VkSurfaceCapabilitiesKHR{{minImageCount: {}, maxImageCount: {}, currentExtent: {}, currentTransform: {}}}",
                        caps.minImageCount, caps.maxImageCount, caps.currentExtent, static_cast<uint32_t>(caps.currentTransform));
    }
};

// Formatter for SDL_Window*
template<>
struct formatter<SDL_Window*, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(SDL_Window* window, FormatContext& ctx) const {
        if (window == nullptr) {
            return format_to(ctx.out(), "SDL_Window(nullptr)");
        }
        const char* title = SDL_GetWindowTitle(window);
        return format_to(ctx.out(), "SDL_Window{{title: {}}}", title ? title : "unknown");
    }
};

// Formatter for SDL_EventType
template<>
struct formatter<SDL_EventType, char> {
    constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }
    template<typename FormatContext>
    auto format(SDL_EventType type, FormatContext& ctx) const {
        switch (type) {
            case SDL_EVENT_QUIT: return format_to(ctx.out(), "SDL_EVENT_QUIT");
            default: return format_to(ctx.out(), "SDL_EventType({})", static_cast<uint32_t>(type));
        }
    }
};

} // namespace std

#endif // ENGINE_LOGGING_HPP