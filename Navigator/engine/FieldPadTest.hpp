#pragma once

// PADTEST — RTX dark-theme gamepad tester (GPU widgets, no VGA terminal).

#include "FieldAosAppIdentity.hpp"
#include "FieldAosAppSnapshot.hpp"

#include "FieldInput.hpp"
#include "FieldRtxApp.hpp"
#include "FieldRuntimeInfo.hpp"

#include <cstdio>

namespace FieldPadTest {

inline bool active = false;
inline bool showRaw = false;
inline int  deviceSel = 0;

inline void paint(std::uint8_t* ram) noexcept {
    const auto& gp = FieldInput::state.gamepad;
    FieldRtxApp::begin(ram, 5u);
    auto& ui = FieldRtxApp::ui;

    ui.panel(8, 8, 1016, 1016, 0);
    ui.label(24, 20, 640, 52, "PADTEST — Field Input Lab", 1);
    ui.dropdown(660, 18, 1000, 52,
        gp.connected ? (gp.name[0] ? gp.name : "SDL Gamepad") : "(no device)", deviceSel);

    const FieldAosAppIdentity::Copy info =
        FieldAosAppIdentity::copyFor(FieldAosAppIdentity::AppId::PadTest);
    ui.label(24, 60, 1000, 88, info.tagline, 0);

    char status[96];
    std::snprintf(status, sizeof status, "%s", FieldRuntimeInfo::masterStatusLine());
    ui.label(24, 94, 720, 122, status, 0);
    ui.checkbox(24, 128, 300, 156, "Show raw axis values", showRaw);
    ui.button(860, 124, 1000, 156, "Rescan", 1);

    ui.panel(16, 164, 352, 620, 0);
    ui.label(32, 172, 336, 200, "Buttons", 1);

    const int rowH = 32;
    int y = 208;
    auto btnRow = [&](const char* name, bool on) {
        ui.checkbox(32, y, 336, y + rowH, name, on);
        y += rowH;
    };

    btnRow("A / South",      (gp.buttons & FieldInput::GP_SOUTH) != 0);
    btnRow("B / East",       (gp.buttons & FieldInput::GP_EAST) != 0);
    btnRow("X / West",       (gp.buttons & FieldInput::GP_WEST) != 0);
    btnRow("Y / North",      (gp.buttons & FieldInput::GP_NORTH) != 0);
    btnRow("LB",             (gp.buttons & FieldInput::GP_LSHOULDER) != 0);
    btnRow("RB",             (gp.buttons & FieldInput::GP_RSHOULDER) != 0);
    btnRow("Back / View",    (gp.buttons & FieldInput::GP_BACK) != 0);
    btnRow("Start / Menu",   (gp.buttons & FieldInput::GP_START) != 0);
    btnRow("Guide / Xbox",   (gp.buttons & FieldInput::GP_GUIDE) != 0);
    btnRow("L3",             (gp.buttons & FieldInput::GP_LSTICK) != 0);
    btnRow("R3",             (gp.buttons & FieldInput::GP_RSTICK) != 0);

    ui.panel(368, 164, 1000, 620, 0);

    std::uint32_t dpad = 0;
    if (gp.buttons & FieldInput::GP_DUP)    dpad |= 1u;
    if (gp.buttons & FieldInput::GP_DDOWN)  dpad |= 2u;
    if (gp.buttons & FieldInput::GP_DLEFT)  dpad |= 4u;
    if (gp.buttons & FieldInput::GP_DRIGHT) dpad |= 8u;
    ui.dpad(400, 184, 540, 324, dpad);
    ui.label(400, 332, 540, 360, "D-Pad", 0);

    ui.stick(568, 184, 728, 364, gp.lx, gp.ly, "Left Stick");
    ui.stick(752, 184, 912, 364, gp.rx, gp.ry, "Right Stick");

    ui.progress(568, 388, 912, 420, gp.lt, "LT");
    ui.progress(568, 432, 912, 464, gp.rt, "RT");

    if (showRaw) {
        char ax[96];
        std::snprintf(ax, sizeof ax, "LX %+.2f  LY %+.2f  RX %+.2f  RY %+.2f",
            gp.lx, gp.ly, gp.rx, gp.ry);
        ui.label(32, 640, 992, 672, ax, 0);
    }

    ui.label(32, 960, 800, 992,
        gp.connected ? "Connected — press buttons to test" : "Plug Xbox360 / SDL gamepad", 0);

    FieldAosAppSnapshot::refresh(FieldAosAppIdentity::AppId::PadTest);
    FieldRtxApp::finish(ram);
}

inline void close(std::uint8_t* ram) noexcept {
    active = false;
    FieldRtxWidgets::clearRam(ram);
    FieldRtxApp::resetUi();
    FieldWinApp::reset();
}

inline void open(std::uint8_t* ram) noexcept {
    active = true;
    showRaw = false;
    FieldAosAppSnapshot::apply(FieldAosAppIdentity::AppId::PadTest);
    deviceSel = 0;
    paint(ram);
}

inline void pump(std::uint8_t* ram, std::uint16_t key, bool keyDown) noexcept {
    if (!active) return;
    int action = 0;
    if (FieldRtxApp::pumpMouse(ram, action)) {
        if (action == 109) { close(ram); return; }
        if (action == 1) { /* rescan */ }
        if (action >= 200) {
            const int wid = action - 200;
            if (wid >= 0 && wid < static_cast<int>(FieldRtxApp::ui.widgets.size())) {
                auto& w = FieldRtxApp::ui.widgets[static_cast<std::size_t>(wid)];
                if (w.type == FieldRtxWidgets::Type::Checkbox && w.labelOff > 0) {
                    const char* lbl = FieldRtxApp::ui.strings.c_str() + w.labelOff;
                    if (std::strstr(lbl, "raw axis")) showRaw = !showRaw;
                }
            }
        }
    }
    paint(ram);
    if (keyDown && (key == 0x011Bu || key == 0x3B00u))
        close(ram);
}

} // namespace FieldPadTest