#pragma once

// Exit confirmation — Yes/No modal before shutting down AmmoOS.

#include "FieldAmouranthMenu.hpp"
#include "FieldAmouranthFilesMenu.hpp"
#include "FieldAmouranthShutdown.hpp"
#include "FieldX86Emu.hpp"
#include "FieldRtxApp.hpp"
#include "FieldRtxWidgets.hpp"
#include "FieldWinApp.hpp"
#include "OptionsMenu.hpp"

#include <cstdio>

namespace FieldAmouranthOs {
void prepareExitConfirmUi() noexcept;
void applyShutdownState() noexcept;
}

namespace FieldAmouranthExitConfirm {

inline bool open = false;

inline bool isOpen() noexcept { return open; }
inline void forceClose() noexcept { open = false; }

inline void completeShutdown(std::uint8_t* ram) noexcept {
    if (!ram) ram = FieldX86Emu::ramHost;
    open = false;
    FieldAmouranthOs::panelVisible = false;
    FieldAmouranthOs::hideDosPanel();
    FieldAmouranthShutdown::closeAllGuestApps(ram);
    FieldAmouranthLaunch::clear();
    if (ram) FieldRtxWidgets::clearRam(ram);
    FieldRtxApp::resetUi();
    FieldWinApp::reset();
    FieldAmouranthOs::applyShutdownState();
    Options::Canvas::DosInputFocused = false;
    Options::SDL3::RequestQuit = true;
    std::fprintf(stderr, "[AMOURANTHOS] Graceful shut down — all guest apps closed\n");
}

inline void paint(std::uint8_t* ram) noexcept {
    if (!ram || !open) return;
    FieldRtxApp::begin(ram, 20u);
    auto& ui = FieldRtxApp::ui;

    ui.panel(8, 8, 1016, 1016, 0);
    ui.panel(272, 340, 752, 684, 0);
    ui.label(292, 360, 732, 400, "Exit AmmoOS?", 1);
    ui.label(292, 412, 732, 444,
        "Close all open programs and shut down gracefully.", 0);
    ui.button(312, 520, 472, 564, "Yes", 1);
    ui.button(552, 520, 712, 564, "No", 2);
    ui.label(292, 600, 732, 632, "Esc = No", 0);

    FieldRtxApp::finish(ram);
}

inline void show() noexcept {
    open = true;
    FieldAmouranthMenu::closeMenuFocus();
    FieldAmouranthFilesMenu::hover = -1;
    FieldAmouranthOs::prepareExitConfirmUi();
    std::uint8_t* ram = FieldX86Emu::ramHost;
    if (ram) paint(ram);
}

inline void dismiss(std::uint8_t* ram) noexcept {
    open = false;
    if (ram) FieldRtxWidgets::clearRam(ram);
    FieldRtxApp::resetUi();
    FieldWinApp::reset();
    FieldAmouranthOs::hideDosPanel();
    FieldAmouranthOs::syncDesktopState();
}

inline void confirmYes(std::uint8_t* ram) noexcept {
    open = false;
    completeShutdown(ram);
}

inline bool pumpMouse(std::uint8_t* ram, int& outAction) noexcept {
    if (!open || !ram) return false;
    FieldRtxApp::pumpMouse(ram, outAction);
    paint(ram);
    if (outAction == 1) { confirmYes(ram); return true; }
    if (outAction == 2) { dismiss(ram); return true; }
    return outAction != 0;
}

inline bool onKey(std::uint16_t key, std::uint8_t* ram) noexcept {
    if (!open || !ram) return false;
    if (key == 0x011Bu || key == 0x3B00u) {
        dismiss(ram);
        return true;
    }
    if (key == 0x1C0Du || key == 0x0D00u) {
        confirmYes(ram);
        return true;
    }
    return false;
}

} // namespace FieldAmouranthExitConfirm