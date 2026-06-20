#pragma once

// AmouranthOS DevKit info panel — version, quality, host clock (packed to data_bus for GPU HUD).

#include "FieldDosDisplay.hpp"
#include "FieldDosViewport.hpp"
#include "FieldRuntimeInfo.hpp"
#include "OptionsMenu.hpp"

#include <chrono>
#include <cstdio>
#include <ctime>

namespace FieldAmouranthInfo {

inline bool visible = false;
inline int  clockHour = 0, clockMin = 0, clockSec = 0;
inline int  clockDay = 1, clockMonth = 1, clockYear = 2026;
inline int  clockWday = 0;
inline std::uint32_t qualityFlags = 0u;
inline std::uint32_t displayScaleQ = 256u;

inline void refreshClock() noexcept {
    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif
    clockHour = tm.tm_hour;
    clockMin = tm.tm_min;
    clockSec = tm.tm_sec;
    clockDay = tm.tm_mday;
    clockMonth = tm.tm_mon + 1;
    clockYear = tm.tm_year + 1900;
    clockWday = tm.tm_wday;
}

inline void formatDateLine(char* buf, std::size_t len) noexcept {
    if (!buf || len < 8u) return;
    static const char* kDays[] = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };
    static const char* kMonths[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
    const char* dow = (clockWday >= 0 && clockWday < 7) ? kDays[clockWday] : "---";
    const char* mon = (clockMonth >= 1 && clockMonth <= 12) ? kMonths[clockMonth - 1] : "---";
    std::snprintf(buf, len, "%s %d %s %d", dow, clockDay, mon, clockYear);
}

inline void refreshQuality() noexcept {
    FieldRuntimeInfo::refresh();
    qualityFlags = 0u;
    if (FieldDosViewport::subpixelFont) qualityFlags |= 1u;
    if (FieldDosViewport::autoZoom4K) qualityFlags |= 2u;
    if (FieldDosViewport::panelStretch) qualityFlags |= 4u;
    if (Options::Rendering::EnableHardwareRayTracing) qualityFlags |= 8u;
    if (Options::Canvas::DosCrispFont) qualityFlags |= 16u;
    if (FieldDosViewport::scanlines) qualityFlags |= 32u;
    displayScaleQ = static_cast<std::uint32_t>(FieldDosViewport::fontScale * 256.f);
    if (displayScaleQ == 0u) displayScaleQ = 256u;
}

inline void tick() noexcept {
    refreshClock();
    refreshQuality();
}

inline void packDataBus(std::uint32_t* bus) noexcept {
    if (!bus) return;
    tick();
    bus[29] = (static_cast<std::uint32_t>(clockHour) << 16u)
            | (static_cast<std::uint32_t>(clockMin) << 8u)
            | static_cast<std::uint32_t>(clockSec);
    bus[30] = qualityFlags;
    bus[31] = displayScaleQ;
}

} // namespace FieldAmouranthInfo