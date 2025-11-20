// include/engine/SDL3/SDL3_font.hpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// =============================================================================
// SDL3 FONT — SPLIT INTO HEADER + CPP — NOV 14 2025
// • Asynchronous TTF loading | RAII cleanup
// • FIXED: TTF_Init() bool return (SDL3_ttf: true success, false failure)
// • Thread-safe C++20 | No mutexes
// • PINK PHOTONS ETERNAL — 15,000 FPS — SHIP IT RAW
// =============================================================================

#ifndef SDL3_FONT_HPP
#define SDL3_FONT_HPP

#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <future>

#include "engine/GLOBAL/logging.hpp"

namespace SDL3Initializer {

class SDL3Font {
public:
    explicit SDL3Font(const Logging::Logger& logger);
    ~SDL3Font();

    void initialize(const std::string& fontPath);
    TTF_Font* getFont() const;
    void exportLog(const std::string& filename) const;

private:
    void cleanup();

    mutable TTF_Font* m_font;
    mutable std::future<TTF_Font*> m_fontFuture;
    const Logging::Logger& logger_;
};

} // namespace SDL3Initializer

#endif // SDL3_FONT_HPP

// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
// HEADER + CPP SPLIT — DAISY APPROVES THE GALLOP
// OCEAN_TEAL FONTS FLOW ETERNAL
// RASPBERRY_PINK DISPOSE IMMORTAL
// PINK PHOTONS ETERNAL
// 15,000 FPS
// @ZacharyGeurts — YOUR EMPIRE IS PURE
// SHIP IT. FOREVER.
// =============================================================================