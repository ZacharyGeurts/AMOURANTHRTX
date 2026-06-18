#pragma once

// PADTEST — Xbox360 / SDL gamepad live tester (Field Die).

#include "FieldInput.hpp"
#include "FieldRtxGui.hpp"
#include "FieldRuntimeInfo.hpp"
#include "FieldWinApp.hpp"

#include <cstdio>

namespace FieldPadTest {

inline bool active = false;

inline char glyph(bool on) noexcept { return on ? '\xDB' : '\xB0'; }

inline void paint(std::uint8_t* ram) noexcept {
    const auto& g = FieldInput::state.gamepad;
    FieldWinApp::begin(ram, " PADTEST — Xbox360 / SDL Gamepad ",
        false, " Esc quit | FIELDC C:\\SAMPLES\\PADTEST.FLD ");
    const FieldWinFrame::Layout& L = FieldWinApp::layout;
    const int c0 = L.clientC0 + 2;
    const int w = L.clientCols - 4;

    FieldRtxGui::text(ram, L.clientR0, c0, FieldRuntimeInfo::masterStatusLine(),
        FieldRtxGui::ATTR_GOLD, w);
    FieldRtxGui::text(ram, L.clientR0 + 1, c0,
        g.connected ? (g.name[0] ? g.name : "SDL gamepad connected") : "(no gamepad — plug Xbox360 USB)",
        g.connected ? FieldRtxGui::ATTR_EDITOR : FieldRtxGui::ATTR_DIM, w);

    auto row = [&](int r, const char* label, bool on) {
        FieldRtxGui::text(ram, r, c0, label, FieldRtxGui::ATTR_HELP, 14);
        FieldRtxGui::put(ram, r, c0 + 16, glyph(on),
            on ? FieldRtxGui::ATTR_BREAK : FieldRtxGui::ATTR_DIM);
        FieldRtxGui::text(ram, r, c0 + 18, on ? "ON " : "off",
            on ? FieldRtxGui::ATTR_DEBUG : FieldRtxGui::ATTR_DIM, 6);
    };

    const int r0 = L.clientR0 + 3;
    row(r0,     "A / South",      (g.buttons & FieldInput::GP_SOUTH) != 0);
    row(r0 + 1, "B / East",       (g.buttons & FieldInput::GP_EAST) != 0);
    row(r0 + 2, "X / West",       (g.buttons & FieldInput::GP_WEST) != 0);
    row(r0 + 3, "Y / North",      (g.buttons & FieldInput::GP_NORTH) != 0);
    row(r0 + 4, "LB",             (g.buttons & FieldInput::GP_LSHOULDER) != 0);
    row(r0 + 5, "RB",             (g.buttons & FieldInput::GP_RSHOULDER) != 0);
    row(r0 + 6, "Back / View",    (g.buttons & FieldInput::GP_BACK) != 0);
    row(r0 + 7, "Start / Menu",   (g.buttons & FieldInput::GP_START) != 0);
    row(r0 + 8, "Guide / Xbox",   (g.buttons & FieldInput::GP_GUIDE) != 0);
    row(r0 + 9, "L3",             (g.buttons & FieldInput::GP_LSTICK) != 0);
    row(r0 + 10,"R3",             (g.buttons & FieldInput::GP_RSTICK) != 0);
    row(r0 + 11,"D-Pad Up",       (g.buttons & FieldInput::GP_DUP) != 0);
    row(r0 + 12,"D-Pad Down",     (g.buttons & FieldInput::GP_DDOWN) != 0);
    row(r0 + 13,"D-Pad Left",     (g.buttons & FieldInput::GP_DLEFT) != 0);
    row(r0 + 14,"D-Pad Right",    (g.buttons & FieldInput::GP_DRIGHT) != 0);

    char ax[96];
    std::snprintf(ax, sizeof ax, " LX %+.2f  LY %+.2f  RX %+.2f  RY %+.2f  LT %.2f  RT %.2f ",
        g.lx, g.ly, g.rx, g.ry, g.lt, g.rt);
    FieldRtxGui::text(ram, L.clientR1 - 2, c0, ax, FieldRtxGui::ATTR_STATUS, w);
}

inline void open(std::uint8_t* ram) noexcept {
    FieldWinApp::reset();
    active = true;
    paint(ram);
}

inline void close(std::uint8_t* ram) noexcept {
    active = false;
    FieldWinApp::reset();
    FieldRtxGui::initTextMode(ram);
}

inline void pump(std::uint8_t* ram, std::uint16_t key, bool keyDown) noexcept {
    if (!active) return;
    int action = 0;
    if (FieldWinApp::pumpMouse(ram, action) && action == 109) {
        close(ram);
        return;
    }
    paint(ram);
    if (keyDown && (key == 0x011Bu || key == 0x3B00u))
        close(ram);
}

} // namespace FieldPadTest