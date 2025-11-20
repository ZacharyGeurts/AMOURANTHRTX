// =============================================================================
// source/engine/SDL3/SDL3_font.cpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 15, 2025 — APOCALYPSE v3.2
// IMGUI & TTF SUBSYSTEM EXTERMINATED — PINK PHOTONS ONLY — MAXIMUM VELOCITY
// =============================================================================

#include "engine/SDL3/SDL3_font.hpp"
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

namespace SDL3Initializer {

SDL3Font::SDL3Font(const Logging::Logger& logger)
    : m_font(nullptr), logger_(logger) {
    LOG_SUCCESS_CAT("SDL3_font", "{}IMGUI & TTF SUBSYSTEM DISABLED — FONT RAII BYPASSED — PINK PHOTONS ASCENDANT{}", 
                    COSMIC_GOLD, RESET);
}

SDL3Font::~SDL3Font() {
    cleanup();
    LOG_SUCCESS_CAT("SDL3_font", "{}SDL3Font RAII destroyed — no TTF ever touched{}", COSMIC_GOLD, RESET);
}

void SDL3Font::initialize(const std::string&) {
    // ImGui and TTF completely removed — this function is now a pure no-op
    LOG_INFO_CAT("SDL3_font", "{}Font initialization skipped — ImGui/TTF subsystem purged for performance{}", 
                 EMERALD_GREEN, RESET);
}

TTF_Font* SDL3Font::getFont() const {
    LOG_DEBUG_CAT("SDL3_font", "{}getFont() called — returning nullptr (ImGui/TTF disabled){}", 
                  CRIMSON_MAGENTA, RESET);
    return nullptr;
}

void SDL3Font::exportLog(const std::string&) const {
    // No-op — logging export not needed without fonts
}

void SDL3Font::cleanup() {
    // Nothing to clean — TTF never initialized
    if (m_font) {
        m_font = nullptr;  // Defensive (should never be non-null)
    }
    LOG_INFO_CAT("SDL3_font", "{}Font cleanup complete — zero allocations ever made{}", 
                 SAPPHIRE_BLUE, RESET);
}

} // namespace SDL3Initializer

// =============================================================================
// IMGUI & TTF ARE DEAD — LONG LIVE THE PINK PHOTONS
// STONEKEY v∞ PREVAILS — NO OVERLAYS, NO FONTS, NO BLOAT
// =============================================================================