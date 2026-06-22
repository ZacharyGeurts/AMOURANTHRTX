#pragma once

// AmouranthOS GUI program launcher — paints framed panels, never raw C:\> text.

#include "FieldAmouranthFileCmd.hpp"
#include "FieldAmouranthLaunch.hpp"
#include "FieldAmouranthOs.hpp"
#include "FieldAmmoCode.hpp"
#include "FieldAmmoExec.hpp"
#include "FieldAmmoText.hpp"
#include "FieldRtxBoot.hpp"
#include "FieldRtxShell.hpp"
#include "FieldRtxThemePicker.hpp"
#include "FieldWebPanel.hpp"
#include "FieldAmmoBrowser.hpp"
#include "FieldBrowserHook.hpp"
#include "FieldAmmoEmuCommon.hpp"
#include "FieldAmmoNes.hpp"
#include "FieldAmmoA2600.hpp"
#include "FieldAmmoSms.hpp"
#include "FieldAmmoGenesis.hpp"
#include "FieldAmmoSnes.hpp"
#include "FieldPadTest.hpp"
#include "FieldRtxBasicIde.hpp"
#include "FieldRtxConsoleGui.hpp"
#include "FieldRtxGui.hpp"
#include "FieldRtxThemes.hpp"
#include "FieldRuntimeInfo.hpp"
#include "FieldRtxMouse.hpp"
#include "FieldAosAppIdentity.hpp"
#include "FieldAosAppJournal.hpp"
#include "FieldAosAppSnapshot.hpp"
#include "FieldAosMonitor.hpp"
#include "FieldRtxApp.hpp"
#include "FieldRtxWidgets.hpp"
#include "FieldWinApp.hpp"
#include "FieldWinFrame.hpp"
#include "FieldAmouranthExitConfirm.hpp"
#include "FieldAmouranthShutdown.hpp"
#include "OptionsMenu.hpp"

#include <SDL3/SDL.h>

#include <cstring>
#include <string>
#include <vector>

namespace FieldAmouranthExec {

inline FieldWinFrame::MenuBarState shellGuiMenuSt;
inline FieldWinFrame::Options shellGuiOpt;

inline void paintRtxShellGui(std::uint8_t* ram) noexcept {
    FieldRtxConsoleGui::close();
    FieldAmouranthOs::consoleShell = false;
    FieldAosAppSnapshot::apply(FieldAosAppIdentity::AppId::Shell);
    FieldRtxApp::begin(ram, 1u);
    auto& ui = FieldRtxApp::ui;
    using namespace FieldRtxWidgets;
    ui.panel(8, 8, 1016, 1016, 1);
    ui.panel(16, uiRow(0) - 4, 1008, uiRow(0) + UI_ROW_H + 12, 1);
    ui.label(28, uiRow(0), 520, uiRow(0) + UI_LABEL_H, "RTX Shell", 1);
    ui.label(28, uiRow(0) + 30, 500, uiRow(0) + 30 + UI_LABEL_H,
        "AmouranthOS 7.0 — Dark Chrome", 0);
    ui.dropdown(744, uiRow(0) + 6, 996, uiRow(0) + 6 + UI_ROW_H, "Programs", 0);
    FieldAosAppIdentity::paintAbout(ui, FieldAosAppIdentity::AppId::Shell, uiRow(1), uiRow(4));
    ui.label(32, uiRow(4), 340, uiRow(4) + UI_LABEL_H, "Available programs", 1);
    ui.checkbox(32, uiRow(5), 360, uiRow(5) + UI_ROW_H, "Show program log", shellGuiOpt.logPanel);
    ui.button(388, uiRow(5), 528, uiRow(5) + UI_ROW_H, "Launch selected", 2);
    ui.button(540, uiRow(5), 700, uiRow(5) + UI_ROW_H, "DOS Console", 3);

    static const char* kApps[] = {
        "AmmoCode IDE", "Field Commander", "QBASIC", "Field Compiler",
        "PADTEST", "AmmoNES", "Field Web", "Shareware Doom"
    };
    constexpr int kAppCount = static_cast<int>(sizeof kApps / sizeof kApps[0]);
    FieldRtxApp::scrollMax = std::max(0, kAppCount - 6);
    FieldRtxApp::scrollTop = std::clamp(shellGuiScroll, 0, FieldRtxApp::scrollMax);
    shellGuiScroll = FieldRtxApp::scrollTop;

    int y = uiRow(6);
    for (int i = 0; i < 6; ++i) {
        const int di = FieldRtxApp::scrollTop + i;
        if (di >= kAppCount) break;
        ui.button(40, y, 528, y + UI_ROW_H, kApps[di], 10 + di);
        y += UI_ROW_H;
    }

    FieldAosAppJournal::paintRecent(ui, FieldAosAppJournal::journalProgId, uiRow(12), uiRow(14));

    ui.panel(544, uiRow(4), 1000, uiRow(14), 1);
    ui.label(560, uiRow(4) + 10, 760, uiRow(4) + 10 + UI_LABEL_H, "Runtime status", 1);
    char status[120];
    std::snprintf(status, sizeof status, "%s", FieldRuntimeInfo::masterStatusLine());
    ui.label(560, uiRow(5) + 4, 984, uiRow(5) + 4 + UI_LABEL_H, status, 0);
    ui.label(560, uiRow(6) + 4, 984, uiRow(6) + 4 + UI_LABEL_H,
        "Use Start menu to open games and tools.", 0);
    ui.label(560, uiRow(7) + 4, 984, uiRow(7) + 4 + UI_LABEL_H,
        "Launch selected opens the highlighted app here.", 0);
    ui.label(560, uiRow(8) + 4, 984, uiRow(8) + 4 + UI_LABEL_H,
        "DOS Console opens C:\\> with HELP and scrollback.", 0);
    ui.label(560, uiRow(9) + 4, 984, uiRow(9) + 4 + UI_LABEL_H,
        "Taskbar Terminal also opens the DOS shell.", 0);

    ui.vscroll(992, uiRow(6), 1016, uiRow(17), FieldRtxApp::scrollTop, kAppCount);
    FieldAosAppSnapshot::refresh(FieldAosAppIdentity::AppId::Shell);
    FieldRtxApp::finish(ram);
}

inline void paintFieldCompilerGui(std::uint8_t* ram) noexcept {
    FieldRtxApp::begin(ram, 9u);
    auto& ui = FieldRtxApp::ui;
    using namespace FieldRtxWidgets;
    ui.panel(8, 8, 1016, 1016);
    ui.label(24, uiRow(0), 700, uiRow(0) + UI_LABEL_H, "Field Compiler v4 - FIELDC", 1);
    ui.dropdown(720, uiRow(0), 1000, uiRow(0) + UI_ROW_H, "Build", 0);
    FieldAosAppIdentity::paintAbout(ui, FieldAosAppIdentity::AppId::FieldC, uiRow(1), uiRow(5));
    FieldAosAppJournal::paintRecent(ui, FieldAosAppJournal::journalProgId, uiRow(5), uiRow(7));
    ui.button(860, uiRow(0), 1000, uiRow(0) + UI_ROW_H, "Compile", 2);
    ui.label(24, uiRow(7), 980, uiRow(7) + UI_LABEL_H, "FIELDC C:\\SAMPLES\\PADTEST.FLD", 0);
    ui.label(24, uiRow(8), 980, uiRow(8) + UI_LABEL_H, "BUILD PADTEST", 0);
    ui.label(24, uiRow(9), 980, uiRow(9) + UI_LABEL_H, "Syntax: print return field era any", 0);
    ui.label(24, uiRow(10), 980, uiRow(10) + UI_LABEL_H, "Shell: BUILD file.ASM  |  FIELDC /? for help topics", 0);
    ui.checkbox(24, uiRow(11), 420, uiRow(11) + UI_ROW_H, "Emit debug symbols", false);
    ui.checkbox(24, uiRow(12), 420, uiRow(12) + UI_ROW_H, "Optimize passes", true);
    char status[120];
    std::snprintf(status, sizeof status, "%s", FieldRuntimeInfo::masterStatusLine());
    ui.label(24, uiRow(17), 980, uiRow(17) + UI_LABEL_H, status, 0);
    FieldRtxApp::finish(ram);
}

inline void paintBrowserHookGui(std::uint8_t* ram) noexcept {
    FieldRtxApp::begin(ram, 8u);
    auto& ui = FieldRtxApp::ui;
    using namespace FieldRtxWidgets;
    ui.panel(8, 8, 1016, 1016);
    ui.label(24, uiRow(0), 700, uiRow(0) + UI_LABEL_H, "Field Web - OS Browser Hook", 1);
    ui.dropdown(720, uiRow(0), 1000, uiRow(0) + UI_ROW_H,
        FieldBrowserHook::hooked ? FieldBrowserHook::browserLabel.c_str() : "Detecting...", 0);
    FieldAosAppIdentity::paintAbout(ui, FieldAosAppIdentity::AppId::Browser, uiRow(1), uiRow(5));
    FieldAosAppJournal::paintRecent(ui, FieldAosAppJournal::journalProgId, uiRow(5), uiRow(7));
    char line[120];
    if (FieldBrowserHook::hooked) {
        std::snprintf(line, sizeof line, "Engine: %s (%s)",
            FieldBrowserHook::browserLabel.c_str(), FieldBrowserHook::browserId.c_str());
        ui.label(24, uiRow(7), 980, uiRow(7) + UI_LABEL_H, line, 0);
        std::snprintf(line, sizeof line, "URL: %.80s", FieldBrowserHook::currentUrl.c_str());
        ui.label(24, uiRow(8), 980, uiRow(8) + UI_LABEL_H, line, 0);
    } else {
        ui.label(24, uiRow(7), 980, uiRow(7) + UI_LABEL_H,
            "No browser hooked yet - click Refresh, then Open when ready.", 0);
    }
    ui.button(24, uiRow(9), 280, uiRow(9) + UI_ROW_H, "Open Amouranth.com", 3);
    ui.button(296, uiRow(9), 520, uiRow(9) + UI_ROW_H, "Refresh hook", 4);
    FieldAosAppSnapshot::refresh(FieldAosAppIdentity::AppId::Browser);
    FieldRtxApp::finish(ram);
}

inline void clearPanel(std::uint8_t* ram, std::uint8_t attr = 0x0Fu) noexcept {
    if (!ram) return;
    FieldRtxGui::initTextMode(ram);
    FieldWinFrame::clearScreen(ram, attr);
}

inline void paintEmptyPanel(std::uint8_t* ram) noexcept {
    if (!ram) return;
    FieldRtxGui::initTextMode(ram);
    FieldWinFrame::clearScreen(ram, 0x07u);
}

inline void suspendAllGuestApps(std::uint8_t* ram) noexcept {
    FieldAmouranthShutdown::closeAllGuestApps(ram);
}

inline void launchDosConsole(std::uint8_t* ram) noexcept {
    if (!ram) return;
    suspendAllGuestApps(ram);
    FieldAmouranthOs::consoleShell = true;
    FieldDosViewport::panelStretch = true;
    Options::Canvas::DosPanelStretch = true;
    Options::Canvas::ControlFlags |= Options::Canvas::ControlDosPanelStretch;
    bool haveShell = false;
    for (const auto& p : FieldAmouranthOs::programs) {
        if (p.app == FieldAmouranthOs::AppId::Shell && p.running) {
            haveShell = true;
            FieldAmouranthOs::focusProgram(p.id, false);
            break;
        }
    }
    if (!haveShell)
        FieldAmouranthOs::openNewWindow(FieldAmouranthOs::AppId::Shell);
    FieldAmouranthOs::panelVisible = true;
    FieldAmouranthOs::pendingEmptyPanel = false;
    Options::Canvas::DosInputFocused = true;
    FieldRtxShell::paintTerminalShell(ram);
    FieldAmouranthOs::syncDesktopState();
}

inline void closeActiveProgram(std::uint8_t* ram) noexcept {
    if (!ram) return;
    if (FieldAmouranthOs::focusedApp == FieldAmouranthOs::AppId::Shell)
        FieldAmouranthOs::consoleShell = false;
    suspendAllGuestApps(ram);
    FieldRtxConsoleGui::close();
    FieldAmouranthOs::removeTopProgram();
    if (FieldAmouranthOs::focusedProgId > 0)
        FieldAmouranthOs::focusProgram(FieldAmouranthOs::focusedProgId);
    else if (FieldAmouranthOs::active)
        clearPanel(ram, 0x07u);
    else
        paintEmptyPanel(ram);
    FieldAmouranthOs::maybeDismissMemoryIdle(ram);
    FieldAmouranthOs::syncDesktopState();
}

inline bool pumpEmuMouse(std::uint8_t* ram, FieldAmouranthOs::AppId app, int action) noexcept;

inline void handleWmMenuAction(int action, std::uint8_t* ram) noexcept {
    if (!ram || action <= 0) return;
    if (action == 109) {
        closeActiveProgram(ram);
        return;
    }
    if (action == 304) {
        FieldRtxThemePicker::open();
        return;
    }
    const auto app = FieldAmouranthOs::focusedApp;
    if (app == FieldAmouranthOs::AppId::Shell) {
        if (action == 301) shellGuiScroll = 0;
        else if (action == 302) shellGuiScroll = 999;
        else if (action == 303) shellGuiOpt.logPanel = !shellGuiOpt.logPanel;
        else if (FieldRtxApp::pumpScroll(action)) shellGuiScroll = FieldRtxApp::scrollTop;
        paintRtxShellGui(ram);
        return;
    }
    if (app == FieldAmouranthOs::AppId::FileCmd && action == 101)
        FieldAmouranthFileCmd::open();
    if (app == FieldAmouranthOs::AppId::Nes
            || app == FieldAmouranthOs::AppId::A2600
            || app == FieldAmouranthOs::AppId::Sms
            || app == FieldAmouranthOs::AppId::Genesis
            || app == FieldAmouranthOs::AppId::Snes)
        pumpEmuMouse(ram, app, action);
}

inline void handleRtxShellAction(int action, std::uint8_t* ram) noexcept {
    if (action == 109) {
        closeActiveProgram(ram);
        return;
    }
    if (action == 3) {
        launchDosConsole(ram);
        return;
    }
    if (action == 2) {
        static const int kMap[] = {1, 11, 3, 4, 5, 6, 15, 7};
        const int di = std::clamp(shellGuiScroll, 0, 7);
        FieldAmouranthOs::dispatchAction(kMap[di]);
    } else if (action >= 10 && action <= 17) {
        static const int kMap[] = {1, 11, 3, 4, 5, 6, 15, 7};
        const int di = action - 10;
        if (di >= 0 && di < 8)
            FieldAmouranthOs::dispatchAction(kMap[di]);
    } else if (action >= 200) {
        const int wid = action - 200;
        if (wid >= 0 && wid < static_cast<int>(FieldRtxApp::ui.widgets.size())) {
            auto& w = FieldRtxApp::ui.widgets[static_cast<std::size_t>(wid)];
            if (w.type == FieldRtxWidgets::Type::Checkbox) {
                const char* lbl = FieldRtxApp::ui.strings.c_str() + w.labelOff;
                if (std::strstr(lbl, "program log"))
                    shellGuiOpt.logPanel = !shellGuiOpt.logPanel;
            }
        }
    } else if (FieldRtxApp::pumpScroll(action)) {
        shellGuiScroll = FieldRtxApp::scrollTop;
    }
    paintRtxShellGui(ram);
}

inline bool pumpEmuMouse(std::uint8_t* ram, FieldAmouranthOs::AppId app, int action) noexcept {
    if (FieldEmuFileDialog::active) {
        if (FieldAmmoEmu::pumpFileDialogMouse(ram, action))
            return true;
    }
    if (action == 0) return false;
    switch (app) {
    case FieldAmouranthOs::AppId::Nes:
        if (FieldNes::active && FieldNes::handleMenu(action, ram)) return true;
        if (action == FieldWinFrame::A_FILE_EXIT) {
            closeActiveProgram(ram);
            return true;
        }
        break;
    case FieldAmouranthOs::AppId::Snes:
        if (FieldSnes::active && FieldSnes::handleMenu(action, ram)) return true;
        if (action == FieldWinFrame::A_FILE_EXIT) { closeActiveProgram(ram); return true; }
        break;
    case FieldAmouranthOs::AppId::Genesis:
        if (FieldGenesis::active && FieldGenesis::handleMenu(action, ram)) return true;
        if (action == FieldWinFrame::A_FILE_EXIT) { closeActiveProgram(ram); return true; }
        break;
    case FieldAmouranthOs::AppId::Sms:
        if (FieldSms::active && FieldSms::handleMenu(action, ram)) return true;
        if (action == FieldWinFrame::A_FILE_EXIT) { closeActiveProgram(ram); return true; }
        break;
    case FieldAmouranthOs::AppId::A2600:
        if (FieldA2600::active && FieldA2600::handleMenu(action, ram)) return true;
        if (action == FieldWinFrame::A_FILE_EXIT) { closeActiveProgram(ram); return true; }
        break;
    default:
        break;
    }
    return false;
}

inline bool pumpFocusedKeyboard(std::uint8_t* ram, std::uint16_t key, bool keyDown) noexcept {
    if (!ram || !keyDown || key == 0) return false;
    const auto app = FieldAmouranthOs::focusedApp;
    if (app == FieldAmouranthOs::AppId::PadTest && FieldPadTest::active) {
        if (key == 0x011Bu || key == 0x3B00u) {
            closeActiveProgram(ram);
            return true;
        }
        FieldPadTest::pump(ram, key, true);
        return true;
    }
    if (app == FieldAmouranthOs::AppId::Nes && FieldNes::active) {
        FieldNes::pump(ram, key, true);
        if (!FieldNes::active) closeActiveProgram(ram);
        return true;
    }
    if (app == FieldAmouranthOs::AppId::Snes && FieldSnes::active) {
        FieldSnes::pump(ram, key, true);
        if (!FieldSnes::active) closeActiveProgram(ram);
        return true;
    }
    if (app == FieldAmouranthOs::AppId::Genesis && FieldGenesis::active) {
        FieldGenesis::pump(ram, key, true);
        if (!FieldGenesis::active) closeActiveProgram(ram);
        return true;
    }
    if (app == FieldAmouranthOs::AppId::Sms && FieldSms::active) {
        FieldSms::pump(ram, key, true);
        if (!FieldSms::active) closeActiveProgram(ram);
        return true;
    }
    if (app == FieldAmouranthOs::AppId::A2600 && FieldA2600::active) {
        FieldA2600::pump(ram, key, true);
        if (!FieldA2600::active) closeActiveProgram(ram);
        return true;
    }
    return false;
}

inline void pumpWinGuiMouse(std::uint8_t* ram) noexcept {
    if (!ram) return;
    if (FieldAmouranthExitConfirm::open) {
        int action = 0;
        FieldAmouranthExitConfirm::pumpMouse(ram, action);
        return;
    }
    if (!FieldAmouranthOs::panelVisible) return;
    int action = 0;
    const auto app = FieldAmouranthOs::focusedApp;

    if (app == FieldAmouranthOs::AppId::Nes
            || app == FieldAmouranthOs::AppId::A2600
            || app == FieldAmouranthOs::AppId::Sms
            || app == FieldAmouranthOs::AppId::Genesis
            || app == FieldAmouranthOs::AppId::Snes) {
        if (FieldEmuFileDialog::active) {
            FieldRtxApp::pumpMouse(ram, action);
            pumpEmuMouse(ram, app, action);
            return;
        }
        if (FieldWinApp::pumpMouse(ram, action))
            pumpEmuMouse(ram, app, action);
        if (app == FieldAmouranthOs::AppId::Nes && FieldNes::optionsOpen)
            FieldAmmoNesSetup::paint(ram);
        else if (app == FieldAmouranthOs::AppId::A2600 && FieldA2600::optionsOpen)
            FieldAmmoA2600Setup::paint(ram);
        else if (app == FieldAmouranthOs::AppId::Sms && FieldSms::optionsOpen)
            FieldAmmoSmsSetup::paint(ram);
        else if (app == FieldAmouranthOs::AppId::Genesis && FieldGenesis::optionsOpen)
            FieldAmmoGenesisSetup::paint(ram);
        else if (app == FieldAmouranthOs::AppId::Snes && FieldSnes::optionsOpen)
            FieldAmmoSnesSetup::paint(ram);
        return;
    }
    if (app == FieldAmouranthOs::AppId::FileCmd) {
        FieldAmouranthFileCmd::handleMouseFrame(ram);
        return;
    }
    if (app == FieldAmouranthOs::AppId::PadTest) {
        if (!FieldPadTest::active) return;
        FieldRtxApp::pumpMouse(ram, action);
        if (action == 109)
            closeActiveProgram(ram);
        else
            FieldPadTest::paint(ram);
        return;
    }
    if (app == FieldAmouranthOs::AppId::NesSetup) {
        FieldRtxApp::pumpMouse(ram, action);
        if (action == 109)
            closeActiveProgram(ram);
        else
            FieldAmmoNesSetup::paint(ram);
        return;
    }
    if (app == FieldAmouranthOs::AppId::A2600Setup) {
        FieldRtxApp::pumpMouse(ram, action);
        if (action == 109) closeActiveProgram(ram);
        else FieldAmmoA2600Setup::paint(ram);
        return;
    }
    if (app == FieldAmouranthOs::AppId::SmsSetup) {
        FieldRtxApp::pumpMouse(ram, action);
        if (action == 109) closeActiveProgram(ram);
        else FieldAmmoSmsSetup::paint(ram);
        return;
    }
    if (app == FieldAmouranthOs::AppId::GenesisSetup) {
        FieldRtxApp::pumpMouse(ram, action);
        if (action == 109) closeActiveProgram(ram);
        else FieldAmmoGenesisSetup::paint(ram);
        return;
    }
    if (app == FieldAmouranthOs::AppId::SnesSetup) {
        FieldRtxApp::pumpMouse(ram, action);
        if (action == 109) closeActiveProgram(ram);
        else FieldAmmoSnesSetup::paint(ram);
        return;
    }
    if (app == FieldAmouranthOs::AppId::Shell && FieldAmouranthOs::active) {
        FieldRtxApp::pumpMouse(ram, action);
        if (action != 0)
            handleRtxShellAction(action, ram);
        else
            paintRtxShellGui(ram);
        return;
    }
    if (app == FieldAmouranthOs::AppId::FieldC
            || app == FieldAmouranthOs::AppId::Browser
            || app == FieldAmouranthOs::AppId::Monitor) {
        FieldRtxApp::pumpMouse(ram, action);
        if (action == 109) {
            closeActiveProgram(ram);
            return;
        }
        if (app == FieldAmouranthOs::AppId::Browser && action == 3) {
            FieldAosAppJournal::recordAction(FieldAosAppIdentity::AppId::Browser,
                "Field Web", "open amouranth.com");
            FieldBrowserHook::open("https://amouranth.com", nullptr);
        } else if (app == FieldAmouranthOs::AppId::Browser && action == 4)
            FieldBrowserHook::refreshManifest();
        if (app == FieldAmouranthOs::AppId::Monitor)
            FieldAosMonitor::paint(ram);
        else if (app == FieldAmouranthOs::AppId::FieldC)
            paintFieldCompilerGui(ram);
        else
            paintBrowserHookGui(ram);
    }
}

inline void launchNesGui(std::uint8_t* ram) noexcept {
    FieldNesImport::ensureImported();
    const bool wantOpts = FieldAmouranthLaunch::pendingOpenOptions;
    const bool padOnly = FieldAmouranthLaunch::pendingNesPadOnly;
    FieldNes::open(ram, nullptr, nullptr, wantOpts, padOnly);
}

inline void launchDoom(void* mapped, std::size_t ramByteOffset, std::uint8_t* ram) noexcept {
    if (!ram || !mapped) return;
    FieldAmmoExec::launch(mapped, ramByteOffset, ram, "C:\\GAMES\\DOOM\\DOOM.EXE");
}

inline void launchGui(FieldAmouranthOs::AppId app, std::uint8_t* ram, void* mapped = nullptr,
                      std::size_t ramByteOffset = 0) noexcept {
    if (!ram) return;
    suspendAllGuestApps(ram);
    FieldAmouranthOs::applyAppViewport(app);
    const bool gfxApp = app == FieldAmouranthOs::AppId::Nes
        || app == FieldAmouranthOs::AppId::Doom
        || app == FieldAmouranthOs::AppId::A2600
        || app == FieldAmouranthOs::AppId::Sms
        || app == FieldAmouranthOs::AppId::Genesis
        || app == FieldAmouranthOs::AppId::Snes;
    if (!gfxApp) {
        if (app == FieldAmouranthOs::AppId::Shell && FieldAmouranthOs::consoleShell
                && !FieldAmouranthOs::active)
            clearPanel(ram, FieldRtxBoot::TERM_ATTR);
        else
            clearPanel(ram, 0x07u);
    }
    if (app != FieldAmouranthOs::AppId::None
            && (FieldAmouranthOs::active || FieldAmouranthOs::consoleShell)) {
        const FieldAmouranthOs::Program* fp =
            FieldAmouranthOs::findProgram(FieldAmouranthOs::focusedProgId);
        if (!fp || fp->app != app)
            FieldAmouranthOs::openNewWindow(app);
        else {
            FieldAmouranthOs::panelVisible = true;
            FieldAmouranthOs::pendingEmptyPanel = false;
            Options::Canvas::DosInputFocused = true;
        }
    }
    switch (app) {
    case FieldAmouranthOs::AppId::Shell:
        if (FieldAmouranthOs::consoleShell)
            FieldRtxShell::paintTerminalShell(ram);
        else
            paintRtxShellGui(ram);
        break;
    case FieldAmouranthOs::AppId::AmmoCode:
        FieldAmmoCode::open(ram, FieldAmmoCode::Lang::Asm, "C:\\AMMOCODE\\MAIN.ASM");
        break;
    case FieldAmouranthOs::AppId::QBasic:
        FieldRtxBasic::startIde(ram);
        break;
    case FieldAmouranthOs::AppId::FieldC:
        paintFieldCompilerGui(ram);
        break;
    case FieldAmouranthOs::AppId::PadTest:
        FieldPadTest::open(ram);
        break;
    case FieldAmouranthOs::AppId::Nes:
        launchNesGui(ram);
        break;
    case FieldAmouranthOs::AppId::NesSetup:
        FieldAmmoNesSetup::open(false, false);
        FieldAmmoNesSetup::paint(ram);
        break;
    case FieldAmouranthOs::AppId::A2600:
        FieldA2600::open(ram, nullptr, FieldAmouranthLaunch::pendingOpenOptions);
        break;
    case FieldAmouranthOs::AppId::A2600Setup:
        FieldA2600::open(ram, nullptr, true);
        break;
    case FieldAmouranthOs::AppId::Sms:
        FieldSms::open(ram, nullptr, FieldAmouranthLaunch::pendingOpenOptions);
        break;
    case FieldAmouranthOs::AppId::SmsSetup:
        FieldSms::open(ram, nullptr, true);
        break;
    case FieldAmouranthOs::AppId::Genesis:
        FieldGenesis::open(ram, nullptr, FieldAmouranthLaunch::pendingOpenOptions);
        break;
    case FieldAmouranthOs::AppId::GenesisSetup:
        FieldGenesis::open(ram, nullptr, true);
        break;
    case FieldAmouranthOs::AppId::Snes:
        FieldSnes::open(ram, nullptr, FieldAmouranthLaunch::pendingOpenOptions);
        break;
    case FieldAmouranthOs::AppId::SnesSetup:
        FieldSnes::open(ram, nullptr, true);
        break;
    case FieldAmouranthOs::AppId::Browser:
        FieldBrowserHook::refreshManifest();
        paintBrowserHookGui(ram);
        break;
    case FieldAmouranthOs::AppId::FileCmd:
        FieldAmouranthFileCmd::open();
        FieldAmouranthFileCmd::paint(ram);
        break;
    case FieldAmouranthOs::AppId::Doom:
        launchDoom(mapped, ramByteOffset, ram);
        break;
    case FieldAmouranthOs::AppId::Monitor:
        FieldAosMonitor::open(ram);
        break;
    default:
        paintRtxShellGui(ram);
        break;
    }
    if (app != FieldAmouranthOs::AppId::None
            && (FieldAmouranthOs::active || FieldAmouranthOs::consoleShell)) {
        FieldAmouranthOs::showDosPanelDocked();
        FieldAmouranthOs::needsProgramCanvas = true;
        FieldAmouranthOs::syncDesktopState();
    }
    FieldAmouranthOs::pendingEmptyPanel = false;
}

inline FieldAmouranthOs::AppId appFromLaunch(FieldAmouranthLaunch::GuiApp g) noexcept {
    switch (g) {
    case FieldAmouranthLaunch::GuiApp::Shell:    return FieldAmouranthOs::AppId::Shell;
    case FieldAmouranthLaunch::GuiApp::AmmoCode: return FieldAmouranthOs::AppId::AmmoCode;
    case FieldAmouranthLaunch::GuiApp::QBasic:   return FieldAmouranthOs::AppId::QBasic;
    case FieldAmouranthLaunch::GuiApp::FieldC:   return FieldAmouranthOs::AppId::FieldC;
    case FieldAmouranthLaunch::GuiApp::PadTest:  return FieldAmouranthOs::AppId::PadTest;
    case FieldAmouranthLaunch::GuiApp::Nes:      return FieldAmouranthOs::AppId::Nes;
    case FieldAmouranthLaunch::GuiApp::NesSetup: return FieldAmouranthOs::AppId::NesSetup;
    case FieldAmouranthLaunch::GuiApp::Browser:  return FieldAmouranthOs::AppId::Browser;
    case FieldAmouranthLaunch::GuiApp::FileCmd:  return FieldAmouranthOs::AppId::FileCmd;
    case FieldAmouranthLaunch::GuiApp::Doom:     return FieldAmouranthOs::AppId::Doom;
    case FieldAmouranthLaunch::GuiApp::Monitor:  return FieldAmouranthOs::AppId::Monitor;
    case FieldAmouranthLaunch::GuiApp::A2600:      return FieldAmouranthOs::AppId::A2600;
    case FieldAmouranthLaunch::GuiApp::A2600Setup: return FieldAmouranthOs::AppId::A2600Setup;
    case FieldAmouranthLaunch::GuiApp::Sms:        return FieldAmouranthOs::AppId::Sms;
    case FieldAmouranthLaunch::GuiApp::SmsSetup:   return FieldAmouranthOs::AppId::SmsSetup;
    case FieldAmouranthLaunch::GuiApp::Genesis:    return FieldAmouranthOs::AppId::Genesis;
    case FieldAmouranthLaunch::GuiApp::GenesisSetup: return FieldAmouranthOs::AppId::GenesisSetup;
    case FieldAmouranthLaunch::GuiApp::Snes:       return FieldAmouranthOs::AppId::Snes;
    case FieldAmouranthLaunch::GuiApp::SnesSetup:  return FieldAmouranthOs::AppId::SnesSetup;
    default: return FieldAmouranthOs::AppId::None;
    }
}

inline void execPending(std::uint8_t* ram, void* mapped = nullptr,
                        std::size_t ramByteOffset = 0) noexcept {
    if (Options::SDL3::RequestQuit) return;
    if (FieldAmouranthLaunch::deferGuiFrames > 0) {
        --FieldAmouranthLaunch::deferGuiFrames;
        return;
    }
    if (FieldAmouranthLaunch::pendingDosConsole) {
        if (!ram) return;
        FieldAmouranthLaunch::pendingDosConsole = false;
        launchDosConsole(ram);
        return;
    }
    if (FieldAmouranthLaunch::pendingGuiApp != FieldAmouranthLaunch::GuiApp::None) {
        if (!ram) return;
        launchGui(appFromLaunch(FieldAmouranthLaunch::pendingGuiApp), ram, mapped, ramByteOffset);
        FieldAmouranthLaunch::clear();
        return;
    }
    if (!FieldAmouranthLaunch::pendingShellCmd.empty()) {
        FieldRtxShell::execLine(FieldAmouranthLaunch::pendingShellCmd.c_str(), ram,
            FieldRtxShell::echoChar, FieldRtxShell::defaultNewline, FieldRtxShell::defaultPrompt);
        FieldAmouranthLaunch::clear();
    }
}

inline bool screenHasGuiMarker(const std::uint8_t* ram, FieldAmouranthOs::AppId app) noexcept {
    if (!ram) return false;
    char buf[80 * 25 + 1]{};
    for (int i = 0; i < 80 * 25; ++i)
        buf[i] = static_cast<char>(ram[0xB8000u + static_cast<std::uint32_t>(i * 2)]);
    switch (app) {
    case FieldAmouranthOs::AppId::Shell:
        return std::strstr(buf, "RTX Shell") != nullptr
            || FieldRtxWidgets::g.appId == 1u;
    case FieldAmouranthOs::AppId::AmmoCode:
        return std::strstr(buf, "AmmoCode") != nullptr
            || std::strstr(buf, "MINIMAP") != nullptr;
    case FieldAmouranthOs::AppId::QBasic:
        return FieldAmmoCode::active;
    case FieldAmouranthOs::AppId::FieldC:
        return std::strstr(buf, "Field Compiler") != nullptr
            || FieldRtxWidgets::g.appId == 9u;
    case FieldAmouranthOs::AppId::Monitor:
        return FieldRtxWidgets::g.appId == 12u;
    case FieldAmouranthOs::AppId::PadTest:
        return FieldPadTest::active;
    case FieldAmouranthOs::AppId::Nes:
        return FieldNes::active;
    case FieldAmouranthOs::AppId::NesSetup:
        return std::strstr(buf, "AmmoNES Setup") != nullptr
            || FieldRtxWidgets::g.appId == 11u;
    case FieldAmouranthOs::AppId::A2600:
        return FieldA2600::active;
    case FieldAmouranthOs::AppId::A2600Setup:
        return FieldAmmoA2600Setup::active || FieldRtxWidgets::g.appId == 13u;
    case FieldAmouranthOs::AppId::Sms:
        return FieldSms::active;
    case FieldAmouranthOs::AppId::SmsSetup:
        return FieldAmmoSmsSetup::active || FieldRtxWidgets::g.appId == 14u;
    case FieldAmouranthOs::AppId::Genesis:
        return FieldGenesis::active;
    case FieldAmouranthOs::AppId::GenesisSetup:
        return FieldAmmoGenesisSetup::active || FieldRtxWidgets::g.appId == 15u;
    case FieldAmouranthOs::AppId::Snes:
        return FieldSnes::active;
    case FieldAmouranthOs::AppId::SnesSetup:
        return FieldAmmoSnesSetup::active || FieldRtxWidgets::g.appId == 16u;
    case FieldAmouranthOs::AppId::Browser:
        return FieldAmmoBrowser::isActive()
            || FieldRtxWidgets::g.appId == 8u;
    case FieldAmouranthOs::AppId::FileCmd:
        return std::strstr(buf, "Field Commander") != nullptr
            || FieldRtxWidgets::g.appId == 7u;
    case FieldAmouranthOs::AppId::Doom:
        return FieldAmmoExec::isActive();
    default:
        return false;
    }
}

} // namespace FieldAmouranthExec