#pragma once

// Display metrics for DOS Field Die — window pixels, HiDPI scale, auto font sizing.
// SDL3 polls live in RayCanvas/SDL3System; this module stays header-only for QA tests.

#include "FieldDosViewport.hpp"
#include "OptionsMenu.hpp"

#include <algorithm>
#include <cstdio>

namespace FieldDosDisplay {

inline int pixelW = 0;
inline int pixelH = 0;
inline int logicalW = 0;
inline int logicalH = 0;
inline float displayScale = 1.f;
inline int displayPixelW = 3840;
inline int displayPixelH = 2160;

inline void setWindowMetrics(int w, int h, float scale, int dispW, int dispH,
                             int logW = 0, int logH = 0) noexcept {
    if (w > 0) pixelW = w;
    if (h > 0) pixelH = h;
    if (logW > 0) logicalW = logW;
    if (logH > 0) logicalH = logH;
    if (scale > 0.f) displayScale = scale;
    if (dispW > 0) displayPixelW = dispW;
    if (dispH > 0) displayPixelH = dispH;
}

inline void syncViewport(int fallbackW, int fallbackH, int renderW = 0, int renderH = 0) noexcept {
    const float winW = static_cast<float>(pixelW > 0 ? pixelW : fallbackW);
    const float winH = static_cast<float>(pixelH > 0 ? pixelH : fallbackH);

    FieldDosViewport::winW = winW;
    FieldDosViewport::winH = winH;
    FieldDosViewport::renderW = static_cast<float>(renderW > 0 ? renderW : fallbackW);
    FieldDosViewport::renderH = static_cast<float>(renderH > 0 ? renderH : fallbackH);
    FieldDosViewport::displayScale = displayScale;
    FieldDosViewport::windowAspect = winW / std::max(winH, 1.f);
    FieldDosViewport::applyDesktopDefaults();

    if (Options::Canvas::DosAutoScale) {
        const FieldDosViewport::Rect r = FieldDosViewport::contentRect();
        FieldDosViewport::fontScale = FieldDosViewport::computeFontScale(r);
        Options::Canvas::DosFontScale = FieldDosViewport::fontScale;
    }
}

inline void formatStatus(char* buf, std::size_t len) noexcept {
    const FieldDosViewport::Rect r = FieldDosViewport::panelRect();
    std::snprintf(buf, len,
        "%.0fx%.0f window render %.0fx%.0f (display %dx%d) DOS %s %.0fx%.0f scale %.1fx subpx=%s%s",
        FieldDosViewport::winW, FieldDosViewport::winH,
        FieldDosViewport::renderW, FieldDosViewport::renderH,
        displayPixelW, displayPixelH,
        FieldDosViewport::panelStretch ? "zoom" : "stamp",
        r.w(), r.h(),
        FieldDosViewport::fontScale,
        FieldDosViewport::subpixelFont ? "on" : "off",
        Options::Canvas::DosAutoScale ? " auto" : "");
}

} // namespace FieldDosDisplay