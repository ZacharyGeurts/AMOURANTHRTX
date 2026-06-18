#pragma once

// Legacy include — chrome actions moved to FieldDosChrome.hpp.

#include "FieldDosChrome.hpp"

namespace FieldDosImmersive {

inline bool toggle(SDL_Window* /*window*/) noexcept {
    if (Options::Canvas::DosPanelStretch)
        FieldDosChrome::setStamp();
    else
        FieldDosChrome::setZoom();
    return Options::Canvas::DosPanelStretch;
}

} // namespace FieldDosImmersive