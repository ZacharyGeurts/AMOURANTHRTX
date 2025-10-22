#ifndef SDL3_FONT_HPP
#define SDL3_FONT_HPP

// Font handling for AMOURANTH RTX Engine, October 2025
// Manages asynchronous TTF font loading and cleanup using RAII.
// Thread-safe with C++20 features; no mutexes required.
// Dependencies: SDL3_ttf, C++20 standard library, logging.hpp.
// Usage: Initialize with font path, use getFont() for rendering, and export logs.
// Zachary Geurts 2025

#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <future>
#include <stdexcept>
#include "engine/logging.hpp"

namespace SDL3Initializer {

class SDL3Font {
public:
    explicit SDL3Font(const Logging::Logger& logger) : m_font(nullptr), m_fontFuture(), logger_(logger) {
        logger_.log(Logging::LogLevel::Info, "Constructing SDL3Font", std::source_location::current());
    }

    ~SDL3Font() {
        logger_.log(Logging::LogLevel::Info, "Destructing SDL3Font", std::source_location::current());
        cleanup();
    }

    void initialize(const std::string& fontPath) {
        logger_.log(Logging::LogLevel::Info, "Initializing TTF", std::source_location::current());
        const char* ttfError = nullptr;
        if (TTF_Init() == 0) {
            ttfError = SDL_GetError();
            logger_.log(Logging::LogLevel::Error, "TTF_Init failed: '{}'", 
                        std::source_location::current(), ttfError ? ttfError : "No error message provided");
            throw std::runtime_error(std::format("TTF_Init failed: {}", ttfError ? ttfError : "No error message provided"));
        }

        logger_.log(Logging::LogLevel::Info, "Loading TTF font asynchronously: {}", 
                    std::source_location::current(), fontPath);
        m_fontFuture = std::async(std::launch::async, [this, fontPath]() -> TTF_Font* {
            TTF_Font* font = TTF_OpenFont(fontPath.c_str(), 24);
            if (!font) {
                const char* sdlError = SDL_GetError();
                logger_.log(Logging::LogLevel::Error, "TTF_OpenFont failed for {}: '{}'", 
                            std::source_location::current(), fontPath, sdlError ? sdlError : "No error message provided");
                throw std::runtime_error(std::format("TTF_OpenFont failed for {}: {}", fontPath, sdlError ? sdlError : "No error message provided"));
            }
            logger_.log(Logging::LogLevel::Info, "Font loaded successfully: {}", 
                        std::source_location::current(), fontPath);
            return font;
        });
    }

    TTF_Font* getFont() const {
        if (m_font == nullptr && m_fontFuture.valid()) {
            try {
                m_font = m_fontFuture.get();
                logger_.log(Logging::LogLevel::Info, "Font retrieved successfully", 
                            std::source_location::current());
            } catch (const std::runtime_error& e) {
                logger_.log(Logging::LogLevel::Error, "Font loading failed: {}", 
                            std::source_location::current(), e.what());
                m_font = nullptr;
            }
        }
        logger_.log(Logging::LogLevel::Debug, "Getting TTF font", std::source_location::current());
        return m_font;
    }

    void exportLog(const std::string& filename) const {
        logger_.log(Logging::LogLevel::Info, "Exporting font log to {}", 
                    std::source_location::current(), filename);
    }

private:
    void cleanup() {
        logger_.log(Logging::LogLevel::Info, "Starting font cleanup", std::source_location::current());
        if (m_fontFuture.valid()) {
            try {
                TTF_Font* pending = m_fontFuture.get();
                if (pending) {
                    TTF_CloseFont(pending);
                    logger_.log(Logging::LogLevel::Info, "Closed pending font in cleanup", 
                                std::source_location::current());
                }
            } catch (...) {
                logger_.log(Logging::LogLevel::Error, "Error closing pending font", 
                            std::source_location::current());
            }
        }
        if (m_font) {
            logger_.log(Logging::LogLevel::Info, "Closing TTF font", std::source_location::current());
            TTF_CloseFont(m_font);
            m_font = nullptr;
        }
        if (TTF_WasInit()) {
            logger_.log(Logging::LogLevel::Info, "Quitting TTF", std::source_location::current());
            TTF_Quit();
        }
    }

    mutable TTF_Font* m_font;
    mutable std::future<TTF_Font*> m_fontFuture;
    const Logging::Logger& logger_;
};

} // namespace SDL3Initializer

#endif // SDL3_FONT_HPP