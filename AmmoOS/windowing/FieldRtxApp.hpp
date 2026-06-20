#pragma once

// RTX application shell — dark GPU widgets, no VGA text chrome.

#include "FieldDosChrome.hpp"
#include "FieldDosViewport.hpp"
#include "FieldRtxMouse.hpp"
#include "FieldRtxWidgets.hpp"
#include "FieldWinApp.hpp"

#include <algorithm>
#include <cstdint>

namespace FieldRtxApp {

inline FieldRtxWidgets::Ui& ui = FieldRtxWidgets::g;
inline int scrollTop = 0;
inline int scrollMax = 0;
inline int hoverWidget = -1;
inline std::uint8_t focusedAppId = 0;

inline void resetUi() noexcept {
    ui.clear();
    scrollTop = scrollMax = 0;
    hoverWidget = -1;
    focusedAppId = 0;
}

inline void begin(std::uint8_t* ram, std::uint8_t appId) noexcept {
    if (!ram) return;
    FieldWinApp::reset();
    FieldWinApp::begin(ram, "", false, nullptr);
    ui.clear();
    ui.appId = appId;
    if (focusedAppId != appId) {
        scrollTop = scrollMax = 0;
        hoverWidget = -1;
    }
    focusedAppId = appId;
}

inline void finish(std::uint8_t* ram) noexcept {
    ui.scrollTop = scrollTop;
    ui.scrollMax = scrollMax;
    ui.hoverId = hoverWidget;
    ui.pack(ram);
}

inline int hitWidget(float nx, float ny) noexcept {
    for (int i = static_cast<int>(ui.widgets.size()) - 1; i >= 0; --i) {
        const auto& w = ui.widgets[static_cast<std::size_t>(i)];
        const float x0 = w.x0 / 1024.f, y0 = w.y0 / 1024.f;
        const float x1 = w.x1 / 1024.f, y1 = w.y1 / 1024.f;
        if (nx >= x0 && nx < x1 && ny >= y0 && ny < y1) return i;
    }
    return -1;
}

inline bool contentPointerNorm(float& u, float& v) noexcept {
    if (!FieldWinApp::useGpuChrome()) return false;
    const float px = FieldDosChrome::lastPixelX;
    const float py = FieldDosChrome::lastPixelY;
    const FieldDosViewport::Rect win = FieldDosChrome::chromeUsesRenderSpace()
        ? FieldDosViewport::panelRectRender() : FieldDosViewport::panelRect();
    if (!win.contains(px, py)) return false;
    const float winLocalX = px - win.x0;
    const float winLocalY = py - win.y0;
    const float titleH = FieldDosViewport::chromeTitleH;
    const float hudH = FieldDosViewport::DOS_HUD_H;
    const float left = 4.f;
    const float right = win.w() - 18.f;
    const float top = titleH + 2.f;
    const float bot = win.h() - hudH - 2.f;
    const float cw = std::max(right - left, 1.f);
    const float ch = std::max(bot - top, 1.f);
    u = std::clamp((winLocalX - left) / cw, 0.f, 1.f);
    v = std::clamp((winLocalY - top) / ch, 0.f, 1.f);
    return true;
}

inline bool pumpScroll(int action) noexcept {
    if (action == 301) { scrollTop = 0; return true; }
    if (action == 302) { scrollTop = scrollMax; return true; }
    if (action == 303) { scrollTop = std::clamp(scrollTop + 1, 0, scrollMax); return true; }
    if (action == 304) { scrollTop = std::clamp(scrollTop - 1, 0, scrollMax); return true; }
    return false;
}

inline bool pumpMouse(std::uint8_t* ram, int& outAction) noexcept {
    outAction = 0;
    if (!ram) return false;

    float nx = 0.f, ny = 0.f;
    bool haveNorm = false;
    bool leftClick = false;
    bool visible = false;

    if (FieldWinApp::useGpuChrome()) {
        float u = 0.f, v = 0.f;
        if (contentPointerNorm(u, v)) {
            nx = u;
            ny = v;
            haveNorm = true;
            visible = true;
        }
        const std::uint8_t btn = FieldDosChrome::lastButtons;
        leftClick = (btn & 1u) != 0u && (FieldDosChrome::prevButtons & 1u) == 0u;
        FieldDosChrome::prevButtons = btn;
    }

    if (!haveNorm) {
        const FieldRtxMouse::Frame m = FieldRtxMouse::capture();
        if (!m.visible) return false;
        const auto& L = FieldWinApp::layout;
        const float cw = static_cast<float>(std::max(1, L.clientCols));
        const float ch = static_cast<float>(std::max(1, L.clientRows));
        nx = (static_cast<float>(m.col) - L.clientC0) / cw;
        ny = (static_cast<float>(m.row) - L.clientR0) / ch;
        leftClick = m.leftClick;
        visible = true;
    }

    if (!visible) return false;

    if (leftClick) {
        const int hit = hitWidget(nx, ny);
        if (hit >= 0) {
            const auto& w = ui.widgets[static_cast<std::size_t>(hit)];
            if (w.type == FieldRtxWidgets::Type::Button)
                outAction = w.state > 0 ? static_cast<int>(w.state) : 100 + hit;
            else if (w.type == FieldRtxWidgets::Type::Checkbox)
                outAction = 200 + hit;
            else if (w.type == FieldRtxWidgets::Type::Dropdown)
                outAction = 300 + hit;
            else if (w.type == FieldRtxWidgets::Type::VScroll)
                outAction = 303;
            hoverWidget = hit;
            ui.hoverId = hit;
            return true;
        }
    }
    hoverWidget = hitWidget(nx, ny);
    ui.hoverId = hoverWidget;

    if (!FieldWinApp::useGpuChrome()) {
        int wmAction = 0;
        if (FieldWinApp::pumpMouse(ram, wmAction)) {
            outAction = wmAction;
            return true;
        }
    }
    return leftClick;
}

} // namespace FieldRtxApp