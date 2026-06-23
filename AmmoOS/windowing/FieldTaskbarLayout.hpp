#pragma once

// Left-docked taskbar layout — mirrors aos_taskbar_layout.inc for C++ hit tests.

#include <algorithm>
#include <cmath>

namespace FieldAmouranthOs {
extern float uiScale() noexcept;
} // fwd

namespace FieldTaskbarLayout {

constexpr float TASKBAR_H_PX   = 56.f;
constexpr float START_W_PX     = 156.f;
constexpr float QUICK_BTN_W_PX = 44.f;
constexpr float TAB_W_PX       = 40.f;
constexpr float CLOCK_W_PX     = 200.f;
constexpr int   QUICK_LAUNCH_N = 4;

struct Layout {
    float vpW = 3840.f;
    float vpH = 2160.f;
    float scale = 1.f;
    float taskH = 56.f;
    float pad = 6.f;
    float lift = 22.f;
    float startX = 6.f;
    float startW = 156.f;
    float startH = 72.f;
    float quickX = 0.f;
    float quickW = 44.f;
    float quickGap = 4.f;
    float tabX = 0.f;
    float tabW = 40.f;
    float tabGap = 4.f;
    float tabY = 0.f;
    float tabH = 46.f;
    float clockW = 168.f;
    float clockX0 = 0.f;
    float clockX1 = 0.f;
    float barY0 = 0.f;
    float barY1 = 0.f;
};

inline Layout compute(float vpW, float vpH, float uiSc) noexcept {
    Layout L{};
    L.vpW = std::max(vpW, 1.f);
    L.vpH = std::max(vpH, 1.f);
    L.scale = uiSc;
    L.taskH = TASKBAR_H_PX * uiSc;
    L.pad = 6.f * uiSc;
    L.lift = 22.f * uiSc;
    L.barY1 = L.vpH;
    L.barY0 = L.vpH - L.taskH;
    L.startX = L.pad;
    L.startW = START_W_PX * uiSc;
    L.startH = L.taskH - L.pad * 2.f + L.lift;
    L.quickGap = 4.f * uiSc;
    L.quickW = QUICK_BTN_W_PX * uiSc;
    L.quickX = L.startX + L.startW + L.quickGap;
    L.tabX = L.quickX + static_cast<float>(QUICK_LAUNCH_N) * (L.quickW + L.quickGap) + 6.f * uiSc;
    L.tabW = TAB_W_PX * uiSc;
    L.tabGap = 4.f * uiSc;
    L.tabY = L.barY0 + 5.f * uiSc;
    L.tabH = L.taskH - 10.f * uiSc;
    L.clockW = CLOCK_W_PX * uiSc;
    L.clockX1 = L.vpW - L.pad;
    L.clockX0 = L.clockX1 - L.clockW;
    return L;
}

inline float quickLaunchX(const Layout& L, int idx) noexcept {
    return L.quickX + static_cast<float>(idx) * (L.quickW + L.quickGap);
}

inline float tabX(const Layout& L, int slot) noexcept {
    return L.tabX + static_cast<float>(slot) * (L.tabW + L.tabGap);
}

} // namespace FieldTaskbarLayout