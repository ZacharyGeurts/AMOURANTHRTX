#pragma once

// AmmoNES ROM repair confirmation — Yes/No modal before writing fixed image.

#include "FieldNesRomQuality.hpp"
#include "FieldRtxApp.hpp"
#include "FieldRtxWidgets.hpp"
#include "FieldWinApp.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace FieldNesRomFix {

inline bool active = false;
inline std::string pendingPath;
inline std::vector<std::uint8_t> pendingData;
inline FieldNesRomQuality::Report pendingReport;
inline std::string fixSummary;

inline void close(std::uint8_t* ram) noexcept {
    active = false;
    pendingPath.clear();
    pendingData.clear();
    pendingReport = {};
    fixSummary.clear();
    if (ram) FieldRtxWidgets::clearRam(ram);
    FieldRtxApp::resetUi();
    FieldWinApp::reset();
}

inline void paint(std::uint8_t* ram) noexcept {
    if (!ram || !active) return;
    FieldRtxApp::begin(ram, 21u);
    auto& ui = FieldRtxApp::ui;
    ui.panel(8, 8, 1016, 1016, 0);
    ui.panel(200, 280, 824, 744, 0);
    ui.label(220, 300, 804, 340, "Repair NES ROM header?", 1);
    ui.label(220, 352, 804, 380,
        "AmmoNES found quality issues (no NES 2.0 header or bad metadata).", 0);
    ui.label(220, 388, 804, 416, "Programmatic fixes (better than legacy GoodNES):", 0);

    int y = 424;
    const int maxLines = 6;
    int shown = 0;
    for (const auto& e : pendingReport.issues) {
        if (!e.fixable) continue;
        if (shown >= maxLines) break;
        char line[96];
        std::snprintf(line, sizeof line, "  - %s", e.text);
        ui.label(228, y, 796, y + 28, line, 0);
        y += 30;
        ++shown;
    }
    if (shown == 0)
        ui.label(228, y, 796, y + 28, "  - Promote to NES 2.0 and normalize layout", 0);

    ui.label(220, y + 12, 804, y + 40, "Apply fix and save corrected ROM?", 0);
    ui.button(240, y + 56, 420, y + 100, "Yes — fix && save", 1);
    ui.button(460, y + 56, 640, y + 100, "No — load as-is", 2);
    ui.label(220, y + 112, 804, y + 140, "Enter=Yes  Esc=No", 0);
    FieldRtxApp::finish(ram);
}

inline void open(const char* path, std::vector<std::uint8_t>&& data,
                 const FieldNesRomQuality::Report& report) noexcept {
    active = true;
    pendingPath = path ? path : "";
    pendingData = std::move(data);
    pendingReport = report;
    fixSummary.clear();
}

inline bool pump(std::uint8_t* ram, std::uint16_t key, bool keyDown,
                 bool& choseFix, std::vector<std::uint8_t>& outData,
                 std::string& outPath) noexcept {
    if (!active || !ram) return false;
    if (keyDown) {
        if (key == 0x011Bu || key == 0x3B00u) {
            choseFix = false;
            outData = pendingData;
            outPath = pendingPath;
            close(ram);
            return true;
        }
        if (key == 0x1C0Du || key == 0x0D00u) {
            choseFix = true;
            outData = pendingData;
            outPath = pendingPath;
            FieldNesRomQuality::applyFixes(outData, pendingReport, fixSummary);
            close(ram);
            return true;
        }
    }
    int action = 0;
    FieldRtxApp::pumpMouse(ram, action);
    paint(ram);
    if (action == 1) {
        choseFix = true;
        outData = pendingData;
        outPath = pendingPath;
        FieldNesRomQuality::applyFixes(outData, pendingReport, fixSummary);
        close(ram);
        return true;
    }
    if (action == 2) {
        choseFix = false;
        outData = pendingData;
        outPath = pendingPath;
        close(ram);
        return true;
    }
    return true;
}

} // namespace FieldNesRomFix