#pragma once

// System Monitor — RTX dark-theme host/runtime dashboard (GPU widgets).

#include "FieldAosAppIdentity.hpp"
#include "FieldAosAppJournal.hpp"
#include "FieldAmouranthInfo.hpp"

#include "FieldDosViewport.hpp"
#include "FieldRtxApp.hpp"
#include "FieldRuntimeInfo.hpp"
#include "OptionsMenu.hpp"

#include <cstdio>

namespace FieldAosMonitor {

inline bool active = false;

inline void paint(std::uint8_t* ram) noexcept {
    using namespace FieldRtxWidgets;
    FieldAmouranthInfo::tick();
    FieldRtxApp::begin(ram, 12u);
    auto& ui = FieldRtxApp::ui;

    ui.panel(8, 8, 1016, 1016, 0);
    ui.label(24, uiRow(0), 640, uiRow(0) + UI_LABEL_H, "System Monitor - AmouranthOS", 1);
    ui.dropdown(720, uiRow(0), 1000, uiRow(0) + UI_ROW_H, "Live", 0);
    FieldAosAppIdentity::paintAbout(ui, FieldAosAppIdentity::AppId::Monitor, uiRow(1), uiRow(5));
    FieldAosAppJournal::paintRecent(ui, FieldAosAppJournal::journalProgId, uiRow(5), uiRow(7));

    char line[120];
    std::snprintf(line, sizeof line, "%s", FieldRuntimeInfo::masterStatusLine());
    ui.label(24, uiRow(7), 1000, uiRow(7) + UI_LABEL_H, line, 0);

    std::snprintf(line, sizeof line, "GPU  %s", FieldRuntimeInfo::gpuShort);
    ui.label(32, uiRow(8), 500, uiRow(8) + UI_LABEL_H, line, 0);
    std::snprintf(line, sizeof line, "Vulkan  %s", FieldRuntimeInfo::VULKAN_API_LABEL);
    ui.label(32, uiRow(9), 500, uiRow(9) + UI_LABEL_H, line, 0);
    std::snprintf(line, sizeof line, "SDL  %s", FieldRuntimeInfo::sdlShort);
    ui.label(32, uiRow(10), 500, uiRow(10) + UI_LABEL_H, line, 0);
    std::snprintf(line, sizeof line, "Field Compiler  %s", FieldRuntimeInfo::FIELD_COMPILER_VER);
    ui.label(32, uiRow(11), 500, uiRow(11) + UI_LABEL_H, line, 0);

    std::snprintf(line, sizeof line, "Clock  %02d:%02d:%02d",
        FieldAmouranthInfo::clockHour, FieldAmouranthInfo::clockMin, FieldAmouranthInfo::clockSec);
    ui.label(540, uiRow(8), 980, uiRow(8) + UI_LABEL_H, line, 0);
    FieldAmouranthInfo::formatDateLine(line, sizeof line);
    ui.label(540, uiRow(9), 980, uiRow(9) + UI_LABEL_H, line, 0);
    std::snprintf(line, sizeof line, "Display scale  %.2fx",
        static_cast<float>(FieldAmouranthInfo::displayScaleQ) / 256.f);
    ui.label(540, uiRow(10), 980, uiRow(10) + UI_LABEL_H, line, 0);

    const std::uint32_t qf = FieldAmouranthInfo::qualityFlags;
    ui.checkbox(32, uiRow(12), 480, uiRow(12) + UI_ROW_H, "Subpixel font", (qf & 1u) != 0u);
    ui.checkbox(32, uiRow(13), 480, uiRow(13) + UI_ROW_H, "Auto zoom 4K", (qf & 2u) != 0u);
    ui.checkbox(32, uiRow(14), 480, uiRow(14) + UI_ROW_H, "Panel stretch", (qf & 4u) != 0u);
    ui.checkbox(32, uiRow(15), 480, uiRow(15) + UI_ROW_H, "Hardware ray tracing", (qf & 8u) != 0u);
    ui.checkbox(32, uiRow(16), 480, uiRow(16) + UI_ROW_H, "Crisp DOS font", (qf & 16u) != 0u);
    ui.checkbox(32, uiRow(17), 480, uiRow(17) + UI_ROW_H, "CRT scanlines", (qf & 32u) != 0u);

    ui.checkbox(540, uiRow(12), 980, uiRow(12) + UI_ROW_H, "Subpixel viewport", FieldDosViewport::subpixelFont);
    ui.checkbox(540, uiRow(13), 980, uiRow(13) + UI_ROW_H, "Panel stretch", FieldDosViewport::panelStretch);
    ui.checkbox(540, uiRow(14), 980, uiRow(14) + UI_ROW_H, "Scanlines", FieldDosViewport::scanlines);

    std::snprintf(line, sizeof line, "Viewport  %.0f x %.0f",
        FieldDosViewport::winW, FieldDosViewport::winH);
    ui.label(32, uiRow(18), 980, uiRow(18) + UI_LABEL_H, line, 0);

    ui.button(860, uiRow(17), 1000, uiRow(17) + UI_ROW_H, "Refresh", 1);
    FieldRtxApp::finish(ram);
}

inline void open(std::uint8_t* ram) noexcept {
    active = true;
    paint(ram);
}

inline void close(std::uint8_t* ram) noexcept {
    active = false;
    FieldRtxWidgets::clearRam(ram);
    FieldWinApp::reset();
}

inline void pump(std::uint8_t* ram) noexcept {
    if (!active) return;
    int action = 0;
    if (FieldRtxApp::pumpMouse(ram, action)) {
        if (action == 109) { close(ram); return; }
        if (action == 1) paint(ram);
    }
    paint(ram);
}

} // namespace FieldAosMonitor