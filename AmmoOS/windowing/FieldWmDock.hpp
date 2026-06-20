#pragma once

// Window docking — top-left cascade, taskbar-aware bounds (no center placement).

#include "FieldDosViewport.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

namespace FieldAmouranthOs {
struct Program;
extern std::vector<Program> programs;
float uiScale() noexcept;
float scaledTaskbarH() noexcept;
float desktopTopInset() noexcept;
float chromeViewportW() noexcept;
float chromeViewportH() noexcept;
} // fwd

namespace FieldWmDock {

constexpr float MARGIN_PX = 32.f;
constexpr float CASCADE_PX = 28.f;
constexpr float DATA_CENTER_WIDTH_FRAC = 0.5f;
constexpr float MAX_PANEL_SCALE      = 4.0f;
constexpr float MIN_VIEWPORT_PCT     = 0.10f;
constexpr float MAX_VIEWPORT_PCT     = 4.00f;

inline float desktopHeight() noexcept {
    return std::max(1.f, FieldAmouranthOs::chromeViewportH()
        - FieldAmouranthOs::desktopTopInset() - FieldAmouranthOs::scaledTaskbarH());
}

inline float defaultPanelScale(float widthFrac = DATA_CENTER_WIDTH_FRAC) noexcept {
    const float sw = FieldAmouranthOs::chromeViewportW();
    const float margin = MARGIN_PX * FieldAmouranthOs::uiScale();
    const float targetW = (sw - margin * 2.f) * widthFrac;
    const float targetH = desktopHeight() - margin * 2.f;
    const float saved = FieldDosViewport::wmPanelScale;
    FieldDosViewport::wmPanelScale = 1.f;
    const float baseW = FieldDosViewport::panelOuterW();
    const float baseH = FieldDosViewport::panelOuterH();
    FieldDosViewport::wmPanelScale = saved;
    return std::clamp(
        std::min(targetW / std::max(baseW, 1.f), targetH / std::max(baseH, 1.f)),
        0.55f, MAX_PANEL_SCALE);
}

inline int slotIndexFor(int progId) noexcept {
    int slot = 0;
    for (const auto& p : FieldAmouranthOs::programs) {
        if (!p.running) continue;
        if (p.id == progId) return slot;
        ++slot;
    }
    return slot;
}

inline void clampToDesktop(float& ox, float& oy, float pw, float ph) noexcept {
    const float sw = FieldAmouranthOs::chromeViewportW();
    const float deskH = desktopHeight();
    const float margin = MARGIN_PX * FieldAmouranthOs::uiScale();
    ox = std::clamp(ox, margin, std::max(margin, sw - pw - margin));
    oy = std::clamp(oy, FieldAmouranthOs::desktopTopInset() + margin,
        std::max(FieldAmouranthOs::desktopTopInset() + margin, deskH - ph - margin));
}

inline void dockProgram(FieldAmouranthOs::Program& pr) noexcept {
    const float s = FieldAmouranthOs::uiScale();
    const float margin = MARGIN_PX * s;
    const float cascade = CASCADE_PX * s;
    const int slot = slotIndexFor(pr.id);
    const float pw = FieldDosViewport::panelOuterW();
    const float ph = FieldDosViewport::panelOuterH();
    pr.panelOx = margin + cascade * static_cast<float>(slot % 6);
    pr.panelOy = FieldAmouranthOs::desktopTopInset() + margin + cascade * static_cast<float>(slot % 6);
    clampToDesktop(pr.panelOx, pr.panelOy, pw, ph);
}

inline void applyToViewport(const FieldAmouranthOs::Program& pr) noexcept {
    FieldDosViewport::panelOx = pr.panelOx;
    FieldDosViewport::panelOy = pr.panelOy;
    FieldDosViewport::panelPositioned = true;
    if (pr.panelScale > 0.f) {
        FieldDosViewport::wmPanelScale = pr.panelScale;
    }
    const float pw = FieldDosViewport::panelOuterW();
    const float ph = FieldDosViewport::panelOuterH();
    float ox = FieldDosViewport::panelOx;
    float oy = FieldDosViewport::panelOy;
    clampToDesktop(ox, oy, pw, ph);
    FieldDosViewport::panelOx = ox;
    FieldDosViewport::panelOy = oy;
}

} // namespace FieldWmDock
