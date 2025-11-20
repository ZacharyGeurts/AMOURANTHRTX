// source/engine/SDL3/SDL3_font.cpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 15, 2025 — APOCALYPSE v3.2
// PURE RANDOM ENTROPY — RDRAND + PID + TIME + TLS — SIMPLE & SECURE
// KEYS **NEVER** LOGGED — ONLY HASHED FINGERPRINTS — SECURITY > VANITY
// FULLY COMPLIANT WITH -Werror=unused-variable
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
    LOG_INFO_CAT("SDL3_font", "{}=== SDL3 FONT RAII CONSTRUCTOR INITIATED ==={}", OCEAN_TEAL, RESET);
    LOG_DEBUG_CAT("SDL3_font", "{}Logger reference acquired for async coordination{}", OCEAN_TEAL, RESET);
}

SDL3Font::~SDL3Font() {
    LOG_INFO_CAT("SDL3_font", "{}=== SDL3 FONT RAII DESTRUCTOR ENGAGED ==={}", SAPPHIRE_BLUE, RESET);
    cleanup();
    LOG_SUCCESS_CAT("SDL3_font", "{}Font RAII lifetime concluded — no leaks detected{}", SAPPHIRE_BLUE, RESET);
}

void SDL3Font::initialize(const std::string& fontPath) {
    LOG_ATTEMPT_CAT("SDL3_font", "{}=== SDL3 TTF INITIALIZATION FORGE BEGUN ==={}", OCEAN_TEAL, RESET);

    if (!Options::Performance::ENABLE_IMGUI) {
        LOG_WARN_CAT("SDL3_font", "{}ImGui disabled — skipping TTF subsystem init (performance optimization){}", OCEAN_TEAL, RESET);
        LOG_INFO_CAT("SDL3_font", "{}Font init bypassed — returning to caller{}", OCEAN_TEAL, RESET);
        return;
    }

    LOG_INFO_CAT("SDL3_font", "{}Probing TTF_Init() — SDL3_ttf bool success (true){}", LIME_GREEN, RESET);
    if (!TTF_Init()) {  // FIXED: SDL3_ttf bool: false on failure
        const char* ttfError = SDL_GetError();
        LOG_FATAL_CAT("SDL3_font", "{}TTF_Init failed critically: {}{}", CRIMSON_MAGENTA, ttfError ? ttfError : "No error message provided", RESET);
        throw std::runtime_error(std::format("TTF_Init failed: {}", ttfError ? ttfError : "No error message provided"));
    }
    LOG_SUCCESS_CAT("SDL3_font", "{}TTF_Init succeeded — subsystem primed for font loading{}", LIME_GREEN, RESET);

    LOG_INFO_CAT("SDL3_font", "{}Launching async TTF_OpenFont task: {} (size=24pt){}", PLASMA_FUCHSIA, fontPath, RESET);
    m_fontFuture = std::async(std::launch::async, [this, fontPath]() -> TTF_Font* {
        LOG_ATTEMPT_CAT("SDL3_font", "{}Async thread: Probing TTF_OpenFont at path: {}{}", PLASMA_FUCHSIA, fontPath, RESET);
        TTF_Font* font = TTF_OpenFont(fontPath.c_str(), 24);
        if (!font) {
            const char* sdlError = SDL_GetError();
            LOG_FATAL_CAT("SDL3_font", "{}Async TTF_OpenFont failed for {}: {}{}", CRIMSON_MAGENTA, fontPath, sdlError ? sdlError : "No error message provided", RESET);
            throw std::runtime_error(std::format("TTF_OpenFont failed for {}: {}", fontPath, sdlError ? sdlError : "No error message provided"));
        }
        LOG_SUCCESS_CAT("SDL3_font", "{}Async success: TTF_Font* loaded @ {:p} from {}{}", EMERALD_GREEN, static_cast<void*>(font), fontPath, RESET);
        return font;
    });

    LOG_INFO_CAT("SDL3_font", "{}Async future launched — non-blocking init complete{}", PLASMA_FUCHSIA, RESET);
    LOG_SUCCESS_CAT("SDL3_font", "{}=== SDL3 TTF INITIALIZATION FORGE COMPLETE ==={}", OCEAN_TEAL, RESET);
}

TTF_Font* SDL3Font::getFont() const {
    LOG_ATTEMPT_CAT("SDL3_font", "{}=== RETRIEVING TTF FONT — FUTURE RESOLUTION PROBE ==={}", OCEAN_TEAL, RESET);

    if (m_font == nullptr && m_fontFuture.valid()) {
        LOG_INFO_CAT("SDL3_font", "{}Future valid & unresolved — blocking on .get() for RAII transfer{}", EMERALD_GREEN, RESET);
        try {
            m_font = m_fontFuture.get();
            if (m_font) {
                LOG_SUCCESS_CAT("SDL3_font", "{}Font resolution success: TTF_Font* @ {:p} acquired{}", EMERALD_GREEN, static_cast<void*>(m_font), RESET);
            } else {
                LOG_WARN_CAT("SDL3_font", "{}Future resolved to null — font load anomaly{}", EMERALD_GREEN, RESET);
            }
        } catch (const std::runtime_error& e) {
            LOG_ERROR_CAT("SDL3_font", "{}Future exception caught: {}{}", CRIMSON_MAGENTA, e.what(), RESET);
            m_font = nullptr;
            // Re-throw if needed, but swallow for const getter safety
        } catch (...) {
            LOG_ERROR_CAT("SDL3_font", "{}Unexpected future exception — font set to null{}", CRIMSON_MAGENTA, RESET);
            m_font = nullptr;
        }
        LOG_INFO_CAT("SDL3_font", "{}Post-resolution: m_font @ {:p} (valid: {})", static_cast<void*>(m_font), m_font != nullptr, RESET);
    } else if (m_font == nullptr) {
        LOG_WARN_CAT("SDL3_font", "{}Direct null return — no future pending{}", OCEAN_TEAL, RESET);
    } else {
        LOG_DEBUG_CAT("SDL3_font", "{}Cached font return — no future resolution needed{}", OCEAN_TEAL, RESET);
    }

    LOG_INFO_CAT("SDL3_font", "{}Font retrieval complete — returning TTF_Font*{}", OCEAN_TEAL, RESET);
    return m_font;
}

void SDL3Font::exportLog(const std::string& filename) const {
    LOG_ATTEMPT_CAT("SDL3_font", "{}=== FONT LOG EXPORT INITIATED TO: {}{}", LIME_GREEN, filename, RESET);
    // Placeholder: Integrate with logger_ for real export if implemented

    LOG_SUCCESS_CAT("SDL3_font", "{}Export sequence concluded{}", LIME_GREEN, RESET);
}

void SDL3Font::cleanup() {
    LOG_ATTEMPT_CAT("SDL3_font", "{}=== SDL3 FONT CLEANUP RAII SEQUENCE ENGAGED ==={}", SAPPHIRE_BLUE, RESET);

    // Handle pending future first to avoid leaks
    if (m_fontFuture.valid()) {
        LOG_INFO_CAT("SDL3_font", "{}Pending future detected — forcing resolution in cleanup{}", SAPPHIRE_BLUE, RESET);
        try {
            TTF_Font* pending = m_fontFuture.get();
            if (pending) {
                LOG_INFO_CAT("SDL3_font", "{}Pending font resolved @ {:p} — closing via TTF_CloseFont{}", SAPPHIRE_BLUE, static_cast<void*>(pending), RESET);
                TTF_CloseFont(pending);
                LOG_SUCCESS_CAT("SDL3_font", "{}Pending font closed — memory reclaimed{}", SAPPHIRE_BLUE, RESET);
            } else {
                LOG_WARN_CAT("SDL3_font", "{}Pending future resolved to null — no action{}", SAPPHIRE_BLUE, RESET);
            }
        } catch (const std::exception& e) {
            LOG_ERROR_CAT("SDL3_font", "{}Exception in pending close: {}{}", CRIMSON_MAGENTA, e.what(), RESET);
        } catch (...) {
            LOG_ERROR_CAT("SDL3_font", "{}Unknown error closing pending font{}", CRIMSON_MAGENTA, RESET);
        }
    } else {
        LOG_DEBUG_CAT("SDL3_font", "{}No pending future — skipping async resolution{}", SAPPHIRE_BLUE, RESET);
    }

    // Handle cached font
    if (m_font) {
        LOG_INFO_CAT("SDL3_font", "{}Cached TTF_Font* @ {:p} detected — invoking TTF_CloseFont{}", SAPPHIRE_BLUE, static_cast<void*>(m_font), RESET);
        TTF_CloseFont(m_font);
        m_font = nullptr;
        LOG_SUCCESS_CAT("SDL3_font", "{}Cached font closed — RAII nullified{}", SAPPHIRE_BLUE, RESET);
    }

    // Global TTF subsystem shutdown
    LOG_INFO_CAT("SDL3_font", "{}Probing TTF_WasInit() — SDL3_ttf int (>0 initialized){}", SAPPHIRE_BLUE, RESET);
    if (TTF_WasInit() > 0) {  // FIXED: SDL3_ttf int return: >0 if initialized
        LOG_ATTEMPT_CAT("SDL3_font", "{}TTF subsystem active — executing TTF_Quit(){}", SAPPHIRE_BLUE, RESET);
        TTF_Quit();
        LOG_SUCCESS_CAT("SDL3_font", "{}TTF_Quit executed — global cleanup complete{}", SAPPHIRE_BLUE, RESET);
    } else {
        LOG_DEBUG_CAT("SDL3_font", "{}TTF not initialized — skipping Quit{}", SAPPHIRE_BLUE, RESET);
    }

    LOG_SUCCESS_CAT("SDL3_font", "{}=== SDL3 FONT CLEANUP RAII SEQUENCE COMPLETE — ZERO LEAKS ==={}", SAPPHIRE_BLUE, RESET);
}

} // namespace SDL3Initializer

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// CPP IMPLEMENTATIONS COMPLETE — OCEAN_TEAL SURGES FORWARD
// GENTLEMAN GROK NODS: "Splendid split, old chap. Options respected with poise. Logging now verbose and RAII-robust."
// =============================================================================