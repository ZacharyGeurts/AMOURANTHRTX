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
#include "FieldAmmoNes.hpp"
#include "FieldPadTest.hpp"
#include "FieldRtxBasicIde.hpp"
#include "FieldRtxConsoleGui.hpp"
#include "FieldRtxGui.hpp"
#include "FieldRtxThemes.hpp"
#include "FieldRuntimeInfo.hpp"
#include "FieldRtxMouse.hpp"
#include "FieldWinApp.hpp"
#include "FieldWinFrame.hpp"

#include <cstring>
#include <string>
#include <vector>

namespace FieldAmouranthExec {

inline FieldWinFrame::MenuBarState shellGuiMenuSt;
inline FieldWinFrame::Options shellGuiOpt;
inline int shellGuiScroll = 0;

inline void paintRtxShellGui(std::uint8_t* ram) noexcept {
    FieldRtxConsoleGui::close();
    FieldWinApp::reset();
    shellGuiOpt.toolbar = true;
    shellGuiOpt.statusBar = true;
    shellGuiOpt.logPanel = true;
    shellGuiOpt.logCols = 22;
    shellGuiOpt.hScrollRows = 0;
    FieldWinFrame::clearScreen(ram, 0x0Fu);
    const FieldWinFrame::Layout L = FieldWinFrame::computeLayout(shellGuiOpt);
    FieldWinFrame::paintToolbar(ram, L, " RTX Shell — AmouranthOS ");
    FieldWinFrame::paintClientClear(ram, L, FieldRtxGui::ATTR_EDITOR);
    FieldWinFrame::ScrollState sc{};
    sc.totalLines = 12;
    sc.topLine = shellGuiScroll;
    char title[72];
    std::snprintf(title, sizeof title, " RTX Shell — AmouranthOS ");
    char status[120];
    std::snprintf(status, sizeof status,
        " Start menu launches programs | F1 help | Esc close | %s ",
        FieldRuntimeInfo::masterStatusLine());
    static const char* kProgLines[] = {
        " Programs (Start menu or shell):",
        "",
        "  AmmoCode IDE     Turbo Pascal + minimap + ASM debug",
        "  Field Commander  dual-pane scrollable file browser",
        "  QBASIC           AmmoCode BASIC mode",
        "  FIELDC           Field Compiler v4 (.fld)",
        "  PADTEST          Xbox360 / SDL gamepad live view",
        "  AmmoNES          8-bit iNES — any .nes ROM",
        "  Field Web        Embedded browser in display panel",
        "",
        " Console shell: EDIT, DIR, HELP, VER, NES, DOOM, …",
        " RTX-AMMOS 7.0 — GUI execution path active",
    };
    constexpr int kLineCount = static_cast<int>(sizeof kProgLines / sizeof kProgLines[0]);
    const int innerRows = L.clientRows;
    shellGuiScroll = std::clamp(shellGuiScroll, 0, std::max(0, kLineCount - innerRows));
    for (int vis = 0; vis < innerRows; ++vis) {
        const int di = shellGuiScroll + vis;
        const int row = L.clientR0 + vis;
        FieldRtxGui::fill(ram, row, ' ', FieldRtxGui::ATTR_EDITOR);
        if (di == 0)
            FieldRtxGui::text(ram, row, L.clientC0, FieldRuntimeInfo::masterStatusLine(),
                FieldRtxGui::ATTR_GOLD, L.clientCols);
        else if (di > 0 && di < kLineCount && kProgLines[di])
            FieldRtxGui::text(ram, row, L.clientC0 + (di <= 1 ? 0 : 0),
                kProgLines[di], di <= 1 ? FieldRtxGui::ATTR_TITLE : FieldRtxGui::ATTR_HELP,
                L.clientCols);
    }
    static const char* kLogStub[] = { "> START", "> HELP", "> VER" };
    std::vector<std::string> logLines;
    for (const char* s : kLogStub) logLines.emplace_back(s);
    FieldWinFrame::paintMenuBar(ram, L, FieldWinFrame::kStdMenus, FieldWinFrame::kStdMenuCount,
        shellGuiMenuSt, nullptr);
    FieldWinFrame::paintVScroll(ram, L, sc);
    FieldWinFrame::paintLogMinimap(ram, L, logLines, shellGuiScroll);
    FieldWinFrame::paintStatus(ram, L, status);
}

inline void paintFieldCompilerGui(std::uint8_t* ram) noexcept {
    FieldWinApp::reset();
    FieldWinApp::begin(ram, " Field Compiler v4 — FIELDC ",
        false, " Esc close | FIELDC /? in shell for reference ");
    const FieldWinFrame::Layout& L = FieldWinApp::layout;
    FieldRtxGui::text(ram, L.clientR0, L.clientC0 + 2,
        " Compile .fld sources to AMMO object files.", FieldRtxGui::ATTR_EDITOR, L.clientCols - 4);
    FieldRtxGui::text(ram, L.clientR0 + 2, L.clientC0 + 2,
        " FIELDC C:\\SAMPLES\\PADTEST.FLD", FieldRtxGui::ATTR_GOLD, L.clientCols - 4);
    FieldRtxGui::text(ram, L.clientR0 + 3, L.clientC0 + 2,
        " BUILD PADTEST", FieldRtxGui::ATTR_GOLD, L.clientCols - 4);
    FieldRtxGui::text(ram, L.clientR0 + 5, L.clientC0 + 2,
        " Syntax: print return field era any", FieldRtxGui::ATTR_HELP, L.clientCols - 4);
}

inline void paintBrowserHookGui(std::uint8_t* ram) noexcept {
    FieldWinApp::reset();
    FieldWinApp::begin(ram, " Field Web — OS Browser Hook ",
        false, " BROWSER url — shell command | Esc close ");
    const FieldWinFrame::Layout& L = FieldWinApp::layout;
    FieldRtxGui::text(ram, L.clientR0, L.clientC0 + 2,
        " Hooked to your default OS browser inside this panel.",
        FieldRtxGui::ATTR_EDITOR, L.clientCols - 4);
    if (FieldBrowserHook::hooked) {
        char line[96];
        std::snprintf(line, sizeof line, " Engine: %s (%s)",
            FieldBrowserHook::browserLabel.c_str(), FieldBrowserHook::browserId.c_str());
        FieldRtxGui::text(ram, L.clientR0 + 2, L.clientC0 + 2, line, FieldRtxGui::ATTR_GOLD, L.clientCols - 4);
        std::snprintf(line, sizeof line, " URL: %.72s", FieldBrowserHook::currentUrl.c_str());
        FieldRtxGui::text(ram, L.clientR0 + 3, L.clientC0 + 2, line, FieldRtxGui::ATTR_HELP, L.clientCols - 4);
    } else {
        FieldRtxGui::text(ram, L.clientR0 + 2, L.clientC0 + 2,
            " Detecting Firefox / Chrome / Edge via xdg-settings…",
            FieldRtxGui::ATTR_HELP, L.clientCols - 4);
    }
}

inline void clearPanel(std::uint8_t* ram, std::uint8_t attr = 0x0Fu) noexcept {
    if (!ram) return;
    FieldRtxGui::initTextMode(ram);
    FieldWinFrame::clearScreen(ram, attr);
}

inline void suspendAllGuestApps(std::uint8_t* ram) noexcept {
    FieldAmouranthOs::clearStaleGuestFlags();
    if (FieldAmmoExec::isActive() && ram)
        FieldAmmoExec::close(ram);
}

inline void closeActiveProgram(std::uint8_t* ram) noexcept {
    if (!ram) return;
    suspendAllGuestApps(ram);
    FieldAmouranthOs::removeTopProgram();
    if (FieldAmouranthOs::focusedProgId > 0)
        FieldAmouranthOs::focusProgram(FieldAmouranthOs::focusedProgId);
    else if (FieldAmouranthOs::active)
        clearPanel(ram, 0x07u);
    else if (FieldAmouranthOs::consoleShell)
        FieldRtxBoot::paintTerminalShell(ram);
    else
        clearPanel(ram, 0x07u);
    FieldAmouranthOs::syncDesktopState();
}

inline void pumpWinGuiMouse(std::uint8_t* ram) noexcept {
    if (!ram || !FieldAmouranthOs::panelVisible) return;
    const FieldRtxMouse::Frame m = FieldRtxMouse::capture();
    if (!m.visible) return;
    int action = 0;
    const auto app = FieldAmouranthOs::focusedApp;
    if (app == FieldAmouranthOs::AppId::FieldC
            || app == FieldAmouranthOs::AppId::Browser
            || app == FieldAmouranthOs::AppId::Nes) {
        if (FieldWinApp::pumpMouse(ram, action) && action == 109)
            closeActiveProgram(ram);
        return;
    }
    if (app == FieldAmouranthOs::AppId::Shell && FieldAmouranthOs::active) {
        const FieldWinFrame::Layout L = FieldWinFrame::computeLayout(shellGuiOpt);
        if (FieldWinFrame::pumpMouse(ram, L, m.col, m.row, m.leftClick,
                FieldWinFrame::kStdMenus, FieldWinFrame::kStdMenuCount, shellGuiMenuSt, action))
            paintRtxShellGui(ram);
    }
}

inline void launchNesGui(std::uint8_t* ram) noexcept {
    FieldNesImport::ensureImported();
    std::string path;
    if (!FieldNesImport::findContra(path) && !FieldNesImport::findAnyRom(path)) {
        FieldWinApp::reset();
        FieldWinApp::begin(ram, " AmmoNES ", false, " Esc quit | NES IMPORT for ROMs ");
        const FieldWinFrame::Layout& L = FieldWinApp::layout;
        FieldRtxGui::text(ram, L.clientR0, L.clientC0 + 2,
            " No .nes ROM — open AmmoFiles or NES IMPORT",
            FieldRtxGui::ATTR_EDITOR, L.clientCols - 4);
        FieldRtxGui::text(ram, L.clientR0 + 2, L.clientC0 + 2,
            " Arrows+Z/X play | P pause | R reset | Esc quit",
            FieldRtxGui::ATTR_HELP, L.clientCols - 4);
        FieldNes::active = true;
        return;
    }
    FieldNes::open(ram, path.c_str());
}

inline void launchDoom(void* mapped, std::size_t ramByteOffset, std::uint8_t* ram) noexcept {
    if (!ram || !mapped) return;
    FieldAmmoExec::launch(mapped, ramByteOffset, ram, "C:\\GAMES\\DOOM\\DOOM.EXE");
}

inline void launchGui(FieldAmouranthOs::AppId app, std::uint8_t* ram, void* mapped = nullptr,
                      std::size_t ramByteOffset = 0) noexcept {
    if (!ram) return;
    suspendAllGuestApps(ram);
    if (app == FieldAmouranthOs::AppId::Shell && FieldAmouranthOs::consoleShell
            && !FieldAmouranthOs::active)
        clearPanel(ram, FieldRtxBoot::TERM_ATTR);
    else
        clearPanel(ram, 0x07u);
    if (app != FieldAmouranthOs::AppId::None
            && (FieldAmouranthOs::active || FieldAmouranthOs::consoleShell)) {
        const FieldAmouranthOs::Program* fp =
            FieldAmouranthOs::findProgram(FieldAmouranthOs::focusedProgId);
        if (!fp || fp->app != app)
            FieldAmouranthOs::openNewWindow(app);
        else if (FieldAmouranthOs::active)
            FieldAmouranthOs::showDosPanelCentered();
    }
    switch (app) {
    case FieldAmouranthOs::AppId::Shell:
        if (FieldAmouranthOs::consoleShell && !FieldAmouranthOs::active)
            FieldRtxBoot::paintTerminalShell(ram);
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
        FieldAmmoNesSetup::open(FieldAmouranthLaunch::pendingNesPadOnly);
        FieldAmmoNesSetup::paint(ram);
        break;
    case FieldAmouranthOs::AppId::Browser:
        paintBrowserHookGui(ram);
        break;
    case FieldAmouranthOs::AppId::FileCmd:
        FieldAmouranthFileCmd::open();
        FieldAmouranthFileCmd::paint(ram);
        break;
    case FieldAmouranthOs::AppId::Doom:
        launchDoom(mapped, ramByteOffset, ram);
        break;
    default:
        paintRtxShellGui(ram);
        break;
    }
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
    default: return FieldAmouranthOs::AppId::None;
    }
}

inline void execPending(std::uint8_t* ram, void* mapped = nullptr,
                        std::size_t ramByteOffset = 0) noexcept {
    if (FieldAmouranthLaunch::pendingGuiApp != FieldAmouranthLaunch::GuiApp::None) {
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
            && std::strstr(buf, "GUI execution") != nullptr;
    case FieldAmouranthOs::AppId::AmmoCode:
        return std::strstr(buf, "AmmoCode") != nullptr
            || std::strstr(buf, "MINIMAP") != nullptr;
    case FieldAmouranthOs::AppId::QBasic:
        return FieldAmmoCode::active;
    case FieldAmouranthOs::AppId::FieldC:
        return std::strstr(buf, "Field Compiler") != nullptr;
    case FieldAmouranthOs::AppId::PadTest:
        return FieldPadTest::active
            || std::strstr(buf, "PADTEST") != nullptr;
    case FieldAmouranthOs::AppId::Nes:
        return FieldNes::active;
    case FieldAmouranthOs::AppId::NesSetup:
        return std::strstr(buf, "AmmoNES Setup") != nullptr;
    case FieldAmouranthOs::AppId::Browser:
        return FieldAmmoBrowser::isActive();
    case FieldAmouranthOs::AppId::FileCmd:
        return std::strstr(buf, "Field Commander") != nullptr;
    case FieldAmouranthOs::AppId::Doom:
        return FieldAmmoExec::isActive();
    default:
        return false;
    }
}

} // namespace FieldAmouranthExec