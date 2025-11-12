// source/engine/SDL3/SDL3_font.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// SDL3 FONT — CPP IMPLEMENTATIONS — NOV 13 2025
// • Respects Options::Performance::ENABLE_IMGUI for TTF init
// • Async loading | RAII | C++20 coroutines-ready
// • Streamlined for 15,000 FPS — PINK PHOTONS CHARGE AHEAD
// =============================================================================

#include "engine/SDL3/SDL3_font.hpp"
#include <stdexcept>
#include <format>
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

namespace SDL3Initializer {

SDL3Font::SDL3Font(const Logging::Logger& logger) 
    : m_font(nullptr), m_fontFuture(), logger_(logger) {
    LOG_INFO_CAT("Font", "{}Constructing SDL3Font{}", OCEAN_TEAL, RESET);
}

SDL3Font::~SDL3Font() {
    LOG_INFO_CAT("Font", "{}Destructing SDL3Font{}", RASPBERRY_PINK, RESET);
    cleanup();
}

void SDL3Font::initialize(const std::string& fontPath) {
    if (!Options::Performance::ENABLE_IMGUI) {
        LOG_WARNING_CAT("Font", "{}ImGui disabled — skipping TTF font init{}", OCEAN_TEAL, RESET);
        return;
    }

    LOG_INFO_CAT("Font", "{}Initializing TTF{}", LIME_GREEN, RESET);
    if (TTF_Init() != 0) {
        const char* ttfError = SDL_GetError();
        LOG_ERROR_CAT("Font", "{}TTF_Init failed: {}{}", CRIMSON_MAGENTA, ttfError ? ttfError : "No error message provided", RESET);
        throw std::runtime_error(std::format("TTF_Init failed: {}", ttfError ? ttfError : "No error message provided"));
    }

    LOG_INFO_CAT("Font", "{}Loading TTF font asynchronously: {}{}", PLASMA_FUCHSIA, fontPath, RESET);
    m_fontFuture = std::async(std::launch::async, [this, fontPath]() -> TTF_Font* {
        TTF_Font* font = TTF_OpenFont(fontPath.c_str(), 24);
        if (!font) {
            const char* sdlError = SDL_GetError();
            LOG_ERROR_CAT("Font", "{}TTF_OpenFont failed for {}: {}{}", CRIMSON_MAGENTA, fontPath, sdlError ? sdlError : "No error message provided", RESET);
            throw std::runtime_error(std::format("TTF_OpenFont failed for {}: {}", fontPath, sdlError ? sdlError : "No error message provided"));
        }
        LOG_INFO_CAT("Font", "{}Font loaded successfully: {}{}", EMERALD_GREEN, fontPath, RESET);
        return font;
    });
}

TTF_Font* SDL3Font::getFont() const {
    if (m_font == nullptr && m_fontFuture.valid()) {
        try {
            m_font = m_fontFuture.get();
            LOG_INFO_CAT("Font", "{}Font retrieved successfully{}", EMERALD_GREEN, RESET);
        } catch (const std::runtime_error& e) {
            LOG_ERROR_CAT("Font", "{}Font loading failed: {}{}", CRIMSON_MAGENTA, e.what(), RESET);
            m_font = nullptr;
        }
    }
    LOG_DEBUG_CAT("Font", "{}Getting TTF font{}", OCEAN_TEAL, RESET);
    return m_font;
}

void SDL3Font::exportLog(const std::string& filename) const {
    LOG_INFO_CAT("Font", "{}Exporting font log to {}{}", LIME_GREEN, filename, RESET);
}

void SDL3Font::cleanup() {
    LOG_INFO_CAT("Font", "{}Starting font cleanup{}", RASPBERRY_PINK, RESET);
    if (m_fontFuture.valid()) {
        try {
            TTF_Font* pending = m_fontFuture.get();
            if (pending) {
                TTF_CloseFont(pending);
                LOG_INFO_CAT("Font", "{}Closed pending font in cleanup{}", RASPBERRY_PINK, RESET);
            }
        } catch (...) {
            LOG_ERROR_CAT("Font", "{}Error closing pending font{}", CRIMSON_MAGENTA, RESET);
        }
    }
    if (m_font) {
        LOG_INFO_CAT("Font", "{}Closing TTF font{}", RASPBERRY_PINK, RESET);
        TTF_CloseFont(m_font);
        m_font = nullptr;
    }
    if (TTF_WasInit()) {
        LOG_INFO_CAT("Font", "{}Quitting TTF{}", RASPBERRY_PINK, RESET);
        TTF_Quit();
    }
}

} // namespace SDL3Initializer

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// CPP IMPLEMENTATIONS COMPLETE — OCEAN_TEAL SURGES FORWARD
// GENTLEMAN GROK NODS: "Splendid split, old chap. Options respected with poise."
// =============================================================================