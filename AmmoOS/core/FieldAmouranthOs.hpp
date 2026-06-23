#pragma once

// AmouranthOS — RTX desktop shell: Start menu launches programs on demand.
// American-flag desktop at boot; windows open empty then load content.

#include "FieldAosStatusBar.hpp"
#include "FieldAmouranthFileCmd.hpp"
#include "FieldAmouranthInfo.hpp"
#include "FieldAmouranthLaunch.hpp"
#include "FieldDrives.hpp"
#include "FieldAmouranthHudRam.hpp"
#include "FieldAmouranthFolderView.hpp"
#include "FieldAmouranthMenu.hpp"
#include "FieldAmouranthFilesMenu.hpp"
#include "FieldAmouranthFileIndex.hpp"
#include "FieldAmouranthSearchFlyout.hpp"
#include "FieldAmouranthDnD.hpp"
#include "FieldAmouranthTextures.hpp"
#include "FieldExtensionMap.hpp"
#include "FieldX86Emu.hpp"
#include "FieldAmmoCode.hpp"
#include "FieldDosChrome.hpp"
#include "FieldDosViewport.hpp"
#include "FieldTaskbarLayout.hpp"
#include "FieldRtxWidgets.hpp"

#include "FieldAmmoNes.hpp"
#include "FieldAmmoA2600.hpp"
#include "FieldAmmoSms.hpp"
#include "FieldAmmoGenesis.hpp"
#include "FieldAmmoSnes.hpp"
#include "FieldWebPanel.hpp"
#include "FieldPadTest.hpp"
#include "FieldRuntimeInfo.hpp"
#include "FieldRtxBasic.hpp"
#include "FieldAmmoText.hpp"
#include "FieldMonacoEdit.hpp"
#include "FieldRtxEdit.hpp"
#include "FieldRtxThemePicker.hpp"

namespace FieldRtxShell { extern bool graphicsActive; }
#include "FieldRtxThemes.hpp"
#include "FieldRtxMemory.hpp"
#include "FieldCdRom.hpp"
#include "FieldXms.hpp"
#include "FieldEms.hpp"
#include "FieldMscdex.hpp"
#include "OptionsMenu.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace FieldAmmoExec { bool isActive() noexcept; }

namespace FieldAmouranthExitConfirm {
bool isOpen() noexcept;
void show() noexcept;
void dismiss(std::uint8_t* ram) noexcept;
void forceClose() noexcept;
void confirmYes(std::uint8_t* ram) noexcept;
bool onKey(std::uint16_t key, std::uint8_t* ram) noexcept;
bool pumpMouse(std::uint8_t* ram, int& outAction) noexcept;
}
namespace FieldAmouranthShutdown {
void closeAllGuestApps(std::uint8_t* ram) noexcept;
}

#include "FieldAosMonitor.hpp"
#include "FieldAmmoBrowser.hpp"
#include "FieldAosChipsWave.hpp"

namespace FieldAmouranthOs {

constexpr float TASKBAR_H   = 56.f;
constexpr float START_W     = 156.f;
constexpr float FOLDER_BTN_W = 44.f;
constexpr float TAB_W       = FieldTaskbarLayout::TAB_W_PX;
constexpr float CLOCK_W     = FieldTaskbarLayout::CLOCK_W_PX;
constexpr float AOS_UI_REF_W  = 3840.f;
constexpr float MENU_ROW_H  = 50.f;
constexpr float MENU_HEADER_H = 28.f;
constexpr float MENU_PAD    = 6.f;
constexpr float MENU_W      = 196.f;
constexpr float MENU_FLYOUT_W = 420.f;
constexpr float MENU_FLYOUT_GAP = 4.f;
constexpr float WIN_CASCADE = 28.f;
constexpr float UI_BOOST    = 1.35f;

constexpr std::uint32_t BUS_AOS_ACTIVE     = 4096u;
constexpr std::uint32_t BUS_AOS_WP_SHIFT   = 16u;
constexpr std::uint32_t BUS_AOS_MENU_START = 1u << 20u;
constexpr std::uint32_t BUS_AOS_MENU_FILE  = 1u << 23u;
constexpr std::uint32_t BUS_AOS_INFO_PANEL = 1u << 24u;
constexpr std::uint32_t BUS_AOS_PANEL_HIDE = 1u << 25u;
constexpr std::uint32_t BUS_AOS_CONSOLE_SHELL = 1u << 29u;
constexpr std::uint32_t BUS_AOS_MENU_SEARCH   = 1u << 26u;
constexpr std::uint32_t BUS_AOS_EXIT_CONFIRM = 1u << 27u;
constexpr std::uint32_t BUS_AOS_FOLDER_VIEW  = 1u << 30u;

/* data_bus[54-61] — AmouranthOS chrome + DOS viewport (shellChromeActive overlays [54]).
 * [54] byte0 menu hover row (0xFF=none) | byte1 taskbar tab hover (0xFF=none)
 *      byte2 menu visible row count     | byte3 focused title string index (TASKBAR_RAM tab, 0xFF=none)
 * [55..56] panel glow / sharpen (FieldDosViewport)
 * [57] conventional memory KB | extended MB   [58] DOS HUD height
 * [59] HD free bytes
 * [60] compositor: [31:24] surface count | [23:16] focused tab idx | [15:0] focused outer W (px)
 * [61] compositor: [31:24] stack revision | [23:16] surface flags   | [15:0] focused outer H (px)
 *      SURFACE_RAM @ 0xB9000 — per-surface rects for multi-window compositor (stride 16) */

constexpr std::uint32_t BUS_CHROME_MENU_HOVER_SHIFT  = 0u;
constexpr std::uint32_t BUS_CHROME_TASK_HOVER_SHIFT  = 8u;
constexpr std::uint32_t BUS_CHROME_MENU_ROWS_SHIFT   = 16u;
constexpr std::uint32_t BUS_CHROME_FOCUS_TITLE_SHIFT = 24u;
constexpr std::uint32_t BUS_CHROME_NONE              = 0xFFu;

enum class AppId : std::uint8_t {
    None = 0, Shell, AmmoCode, QBasic, FieldC, PadTest, Nes, NesSetup, Browser, Vscodium, FileCmd, Doom, Monitor,
    A2600, A2600Setup, Sms, SmsSetup, Genesis, GenesisSetup, Snes, SnesSetup
};

enum class HitZone : std::uint8_t {
    None = 0, Desktop, Taskbar, StartBtn, FilesBtn, TerminalBtn, MonitorBtn, BrowserBtn,
    FilesMenu, FolderView, TaskBtn, Clock, StartMenu, StartMenuFlyout, StartMenuSearch
};

constexpr int QUICK_LAUNCH_N = 4;

inline bool workloadHeavy(AppId app) noexcept {
    switch (app) {
    case AppId::FileCmd: case AppId::Browser: case AppId::Doom: case AppId::Monitor:
    case AppId::Nes: case AppId::NesSetup: case AppId::A2600: case AppId::A2600Setup:
    case AppId::Sms: case AppId::SmsSetup: case AppId::Genesis: case AppId::GenesisSetup:
    case AppId::Snes: case AppId::SnesSetup: case AppId::AmmoCode: case AppId::Vscodium:
        return true;
    default:
        return false;
    }
}

struct Program {
    int         id = 0;
    AppId       app = AppId::None;
    char        titleBuf[40]{};
    const char* title = titleBuf;
    const char* tooltip = "";
    std::uint8_t icon = 4u;
    bool        running = true;
    bool        minimized = false;
    float       panelOx = -1.f;
    float       panelOy = -1.f;
    float       panelScale = -1.f;
    bool        openOptionsOnRestore = false;
};

inline bool active = false;
inline bool qaHoldInfoDesktop = false;
inline bool startOpen = false;
inline bool startTextInputActive = false;
inline bool filesOpen = false;
inline bool panelVisible = false;
inline bool needsProgramCanvas = false;
inline bool infoPanelVisible = false;
inline bool pendingEmptyPanel = false;
inline int  nextProgId = 1;
inline AppId focusedApp = AppId::None;
inline int  focusedProgId = 0;
inline int  winW = 3840, winH = 2160;
inline HitZone hover = HitZone::None;
inline int taskHoverTab = -1;
inline bool filesBtnHover = false;
inline bool terminalBtnHover = false;
inline bool monitorBtnHover = false;
inline bool browserBtnHover = false;
inline int  filesDragIdx = -1;
inline float filesDragMx0 = 0.f, filesDragMy0 = 0.f;
inline float pointerMx = 0.f, pointerMy = 0.f;
inline std::uint8_t browserIconSlot = 17u;
inline bool pendingShellRestore = false;
inline bool consoleShell = false;  // diagnostics console; desktop boots active from startup
inline std::uint32_t wallpaperIndex = 8u;
inline SDL_Window* hostWindow = nullptr;

inline std::vector<Program> programs;

inline void growMemoryForApp(AppId app) noexcept {
    if (!workloadHeavy(app)) return;
    if (FieldRtxMemory::growConventional(FieldRtxMemory::maxConventionalKb, FieldX86Emu::ramHost))
        FieldRtxMemory::guestFastMb = std::max(FieldRtxMemory::guestFastMb, 8u);
    FieldRtxMemory::popGuestFast(FieldX86Emu::ramHost);
    FieldRtxMemory::growExtenders(FieldX86Emu::ramHost, FieldCdRom::ready);
    FieldXms::activate(FieldX86Emu::emu);
    FieldEms::activate(FieldX86Emu::emu);
    if (FieldRtxMemory::mscdexLive())
        FieldMscdex::install();
    FieldBios::patchConventionalKb(FieldX86Emu::emu, FieldX86Emu::ramHost);
}

inline void maybeDismissMemoryIdle(std::uint8_t* ram) noexcept {
    for (const auto& p : programs) {
        if (p.running && workloadHeavy(p.app)) return;
    }
    const bool shrunk = FieldRtxMemory::dismissConventional();
    FieldRtxMemory::dismissExtenders();
    FieldXms::deactivate();
    FieldEms::deactivate();
    FieldMscdex::dismiss();
    if (shrunk)
        FieldBios::patchConventionalKb(FieldX86Emu::emu, ram);
}

} // namespace FieldAmouranthOs

#include "FieldAosAppIdentity.hpp"
#include "FieldAosAppJournal.hpp"
#include "FieldAosAppJournalCfg.hpp"
#include "FieldRegistry.hpp"
#include "FieldAosAppSnapshot.hpp"
#include "FieldAmouranthWm.hpp"
#include "FieldAmouranthDesktop.hpp"
#include "FieldWmDock.hpp"
#include "FieldWmShell.hpp"

namespace FieldAmouranthOs {

inline void deactivate() noexcept;
inline void requestGracefulShutdown(std::uint8_t* ram = nullptr) noexcept;

inline std::uint8_t appIcon(AppId a) noexcept {
    using IS = FieldAmouranthTextures::IconSlot;
    switch (a) {
    case AppId::AmmoCode: return static_cast<std::uint8_t>(IS::AmmoCode);
    case AppId::QBasic:   return static_cast<std::uint8_t>(IS::QBasic);
    case AppId::FieldC:   return static_cast<std::uint8_t>(IS::FieldC);
    case AppId::PadTest:  return static_cast<std::uint8_t>(IS::PadTest);
    case AppId::Nes:      return static_cast<std::uint8_t>(IS::Nes);
    case AppId::Browser:  return static_cast<std::uint8_t>(IS::Display);
    case AppId::Vscodium: return static_cast<std::uint8_t>(IS::Vscodium);
    case AppId::FileCmd:  return static_cast<std::uint8_t>(IS::FileCmd);
    case AppId::Doom:     return static_cast<std::uint8_t>(IS::Doom);
    case AppId::Monitor:  return static_cast<std::uint8_t>(IS::Monitor);
    case AppId::A2600:
    case AppId::A2600Setup: return static_cast<std::uint8_t>(IS::Nes);
    case AppId::Sms:
    case AppId::SmsSetup: return static_cast<std::uint8_t>(IS::Nes);
    case AppId::Genesis:
    case AppId::GenesisSetup:
    case AppId::Snes:
    case AppId::SnesSetup: return static_cast<std::uint8_t>(IS::Nes);
    default:              return static_cast<std::uint8_t>(IS::Shell);
    }
}

inline const char* appTitle(AppId a) noexcept {
    switch (a) {
    case AppId::AmmoCode: return "AmmoCode IDE";
    case AppId::QBasic:   return "QBASIC";
    case AppId::FieldC:   return "Field Compiler";
    case AppId::PadTest:  return "PADTEST";
    case AppId::Nes:      return "AmmoNES";
    case AppId::NesSetup: return "AmmoNES Setup";

    case AppId::Browser:  return "Field Web";
    case AppId::Vscodium: return "VSCodium";
    case AppId::FileCmd:  return "AmmoFiles";
    case AppId::Doom:     return "Shareware Doom";
    case AppId::Monitor:  return "System Monitor";
    case AppId::A2600:      return "AmmoA2600";
    case AppId::A2600Setup: return "AmmoA2600 Setup";
    case AppId::Sms:        return "AmmoSMS";
    case AppId::SmsSetup:   return "AmmoSMS Setup";
    case AppId::Genesis:    return "AmmoGenesis";
    case AppId::GenesisSetup: return "AmmoGenesis Setup";
    case AppId::Snes:       return "AmmoSNES";
    case AppId::SnesSetup:  return "AmmoSNES Setup";
    default:              return "RTX Shell";
    }
}

inline const char* appTooltip(AppId a) noexcept {
    switch (a) {
    case AppId::AmmoCode: return "Code editor";
    case AppId::QBasic:   return "BASIC interpreter";
    case AppId::FieldC:   return "C compiler";
    case AppId::PadTest:  return "Gamepad test";
    case AppId::Nes:      return "NES emulator";
    case AppId::NesSetup: return "NES options";

    case AppId::FileCmd:  return "Files — browse and open";
    case AppId::Browser:  return "Embedded web panel";
    case AppId::Vscodium: return "Host editor";
    case AppId::Doom:     return "DOOM shareware";
    case AppId::Monitor:  return "Host runtime dashboard";
    case AppId::A2600:      return "Atari 2600 emulator";
    case AppId::A2600Setup: return "A2600 options";
    case AppId::Sms:        return "Master System emulator";
    case AppId::SmsSetup:   return "SMS options";
    case AppId::Genesis:    return "Genesis emulator";
    case AppId::GenesisSetup: return "Genesis options";
    case AppId::Snes:       return "SNES + SuperFX";
    case AppId::SnesSetup:  return "SNES / GSU options";
    case AppId::Shell:    return "Program launcher and DOS shell";
    default:              return "AmouranthOS application";
    }
}

inline bool guestAppRunning() noexcept {
    return FieldAmmoCode::active || FieldAmouranthFileCmd::active
        || FieldPadTest::active || FieldNes::active
        || FieldA2600::active || FieldSms::active
        || FieldGenesis::active || FieldSnes::active
        || FieldAmmoBrowser::isActive()
        || FieldRtxBasic::active
        || FieldAmmoText::active || FieldMonacoEdit::active || FieldRtxThemePicker::active
        || FieldAmmoExec::isActive() || FieldAosMonitor::active;
}

inline bool hasShellProgram() noexcept {
    for (const auto& p : programs)
        if (p.app == AppId::Shell && p.running) return true;
    return false;
}

inline void ensureShellTab() noexcept {
    if (!shellChromeActive() || hasShellProgram()) return;
    Program pr{};
    pr.id = nextProgId++;
    pr.app = AppId::Shell;
    pr.icon = appIcon(AppId::Shell);
    pr.tooltip = appTooltip(AppId::Shell);
    std::snprintf(pr.titleBuf, sizeof pr.titleBuf, "%s", appTitle(AppId::Shell));
    pr.title = pr.titleBuf;
    pr.running = true;
    programs.push_back(pr);
    if (focusedProgId <= 0) {
        focusedProgId = pr.id;
        focusedApp = AppId::Shell;
    }
}

inline float uiScale() noexcept;
inline float scaledTaskbarH() noexcept;
inline float desktopTopInset() noexcept;
inline float chromeViewportW() noexcept;
inline float chromeViewportH() noexcept;

inline void pauseBackgroundEmulators() noexcept {
    if (FieldNes::active) FieldNes::paused = true;
    if (FieldSnes::active) FieldSnes::paused = true;
    if (FieldGenesis::active) FieldGenesis::paused = true;
    if (FieldSms::active) FieldSms::paused = true;
    if (FieldA2600::active) FieldA2600::paused = true;
}

inline void suspendGuestTicks() noexcept {
    pauseBackgroundEmulators();
    FieldAmouranthLaunch::clear();
}

inline bool isGfxEmuApp(AppId app) noexcept {
    return app == AppId::Nes || app == AppId::NesSetup
        || app == AppId::A2600 || app == AppId::A2600Setup
        || app == AppId::Sms || app == AppId::SmsSetup
        || app == AppId::Genesis || app == AppId::GenesisSetup
        || app == AppId::Snes || app == AppId::SnesSetup
        || app == AppId::Doom;
}

inline void applyAppViewport(AppId app) noexcept {
    FieldDosViewport::clearEmuViewport();
    switch (app) {
    case AppId::Nes:
    case AppId::NesSetup:
        FieldDosViewport::setEmuViewport(640.f, 400.f);
        break;
    case AppId::A2600:
    case AppId::A2600Setup:
        FieldDosViewport::setEmuViewport(320.f, 384.f);
        break;
    case AppId::Sms:
    case AppId::SmsSetup:
        FieldDosViewport::setEmuViewport(512.f, 384.f);
        break;
    case AppId::Genesis:
    case AppId::GenesisSetup:
        FieldDosViewport::setEmuViewport(640.f, 448.f);
        break;
    case AppId::Snes:
    case AppId::SnesSetup:
        FieldDosViewport::setEmuViewport(512.f, 448.f);
        break;
    case AppId::Doom:
        FieldDosViewport::setEmuViewport(640.f, 400.f);
        break;
    case AppId::Monitor:
        FieldDosViewport::setEmuViewport(1024.f, 720.f);
        break;
    case AppId::Browser:
    case AppId::FieldC:
    case AppId::FileCmd:
    case AppId::AmmoCode:
    case AppId::PadTest:
    case AppId::Vscodium:
        FieldDosViewport::setEmuViewport(520.f, 360.f);
        break;
    case AppId::Shell:
    case AppId::QBasic:
        FieldDosViewport::setEmuViewport(FieldWmShell::COMPACT_LOGICAL_W,
            FieldWmShell::COMPACT_LOGICAL_H);
        break;
    default:
        FieldDosViewport::setEmuViewport(FieldWmShell::COMPACT_LOGICAL_W,
            FieldWmShell::COMPACT_LOGICAL_H);
        break;
    }
}

inline void hideDosPanel() noexcept {
    panelVisible = false;
    Options::Canvas::DosInputFocused = false;
    suspendGuestTicks();
    FieldDosViewport::panelOx = -8192.f;
    FieldDosViewport::panelOy = -8192.f;
    FieldDosViewport::panelPositioned = true;
    FieldDosViewport::clearEmuViewport();
    infoPanelVisible = false;
    FieldAmouranthInfo::visible = false;
    if (consoleShell) {
        FieldDosViewport::panelStretch = true;
        Options::Canvas::DosPanelStretch = true;
        Options::Canvas::ControlFlags |= Options::Canvas::ControlDosPanelStretch;
    } else {
        FieldDosViewport::panelStretch = false;
        Options::Canvas::DosPanelStretch = false;
        Options::Canvas::ControlFlags &= ~Options::Canvas::ControlDosPanelStretch;
    }
}

inline Program* findProgram(int progId) noexcept {
    for (auto& p : programs)
        if (p.id == progId) return &p;
    return nullptr;
}

inline void saveFocusedPanelPos() noexcept {
    if (focusedProgId <= 0) return;
    Program* p = findProgram(focusedProgId);
    if (!p) return;
    p->panelOx = FieldDosViewport::panelOx;
    p->panelOy = FieldDosViewport::panelOy;
    p->panelScale = FieldAmouranthWm::panelScale;
}

inline float cascadeOffset(std::size_t idx) noexcept {
    return WIN_CASCADE * uiScale() * static_cast<float>(idx % 8u);
}

inline void applyProgramPanel(const Program& pr) noexcept {
    if (pr.panelScale > 0.f) {
        FieldAmouranthWm::panelScale = pr.panelScale;
        FieldAmouranthWm::applyPanelScale();
    }
    FieldWmDock::applyToViewport(pr);
}

inline FieldAmouranthLaunch::GuiApp guiAppFor(AppId app) noexcept;

inline void placeNewWindow(Program& pr) noexcept {
    if (pr.panelScale < 0.f) {
        if (isGfxEmuApp(pr.app)) {
            applyAppViewport(pr.app);
            FieldDosViewport::panelStretch = false;
            FieldWmShell::applyDataCenterScale();
        } else {
            FieldWmShell::applyCompactViewport();
        }
        pr.panelScale = FieldWmInput::panelScale;
    }
    FieldWmDock::dockProgram(pr);
}

inline void clearStaleGuestFlags() noexcept {
    FieldAmmoText::active = false;
    FieldRtxThemePicker::close();
    FieldRtxBasic::active = false;
    FieldAmmoCode::active = false;
    FieldPadTest::active = false;

    FieldAmmoBrowser::close();
    FieldAmouranthFileCmd::close();
    FieldAosMonitor::active = false;
    FieldRtxShell::graphicsActive = false;

    if (!FieldNes::active) {
        FieldNes::optionsOpen = false;
        FieldAmmoNesSetup::active = false;
    }
    if (!FieldA2600::active) {
        FieldA2600::optionsOpen = false;
        FieldAmmoA2600Setup::active = false;
    }
    if (!FieldSms::active) {
        FieldSms::optionsOpen = false;
        FieldAmmoSmsSetup::active = false;
    }
    if (!FieldGenesis::active) {
        FieldGenesis::optionsOpen = false;
        FieldAmmoGenesisSetup::active = false;
    }
    if (!FieldSnes::active) {
        FieldSnes::optionsOpen = false;
        FieldAmmoSnesSetup::active = false;
    }
    if (!FieldNes::active && !FieldA2600::active && !FieldSms::active
            && !FieldGenesis::active && !FieldSnes::active)
        FieldEmuFileDialog::close();
}

inline void captureSnapshotFor(AppId app, char* buf, int cap) noexcept {
    FieldAosAppSnapshot::capture(
        FieldAosAppIdentity::fromOsApp(static_cast<std::uint8_t>(app)), buf, cap);
}

inline void persistOpenSession() noexcept {
    if (programs.empty()) return;
    FieldAosAppIdentity::AppId apps[FieldAosAppIdentity::MAX_TABS]{};
    const char* snaps[FieldAosAppIdentity::MAX_TABS]{};
    char snapBuf[FieldAosAppIdentity::MAX_TABS][FieldAosAppJournal::SNAPSHOT_LEN + 1]{};
    int n = 0;
    for (const auto& p : programs) {
        if (!p.running || n >= FieldAosAppIdentity::MAX_TABS) continue;
        apps[n] = FieldAosAppIdentity::fromOsApp(static_cast<std::uint8_t>(p.app));
        captureSnapshotFor(p.app, snapBuf[n], FieldAosAppJournal::SNAPSHOT_LEN + 1);
        snaps[n] = snapBuf[n];
        ++n;
    }
    FieldAosAppJournal::saveSession(apps, snaps, n,
        FieldAosAppIdentity::fromOsApp(static_cast<std::uint8_t>(focusedApp)));
}

inline void focusProgram(int progId, bool restoreContent = true) noexcept {
    if (progId <= 0) return;
    const int prevId = focusedProgId;
    if (prevId > 0 && prevId != progId)
        saveFocusedPanelPos();
    focusedProgId = progId;
    for (auto& p : programs) {
        if (p.id == progId) {
            focusedApp = p.app;
            p.minimized = false;
            applyAppViewport(p.app);
            if (p.panelOx < 0.f)
                placeNewWindow(p);
            applyProgramPanel(p);
            panelVisible = true;
            needsProgramCanvas = true;
            FieldAosAppJournal::recordFocus(progId,
                FieldAosAppIdentity::fromOsApp(static_cast<std::uint8_t>(p.app)),
                p.title);
            char snap[FieldAosAppJournal::SNAPSHOT_LEN + 1]{};
            captureSnapshotFor(p.app, snap, static_cast<int>(sizeof snap));
            if (snap[0] && !FieldAosAppJournal::restoringSession) {
                FieldAosAppJournal::recordSnapshot(progId,
                    FieldAosAppIdentity::fromOsApp(static_cast<std::uint8_t>(p.app)),
                    p.title, snap);
            }
            break;
        }
    }
    if (prevId != progId)
        clearStaleGuestFlags();
    FieldAmouranthWm::openMenu = FieldAmouranthWm::OpenMenu::None;
    FieldAmouranthWm::menuItemHover = -1;
    FieldAmouranthWm::raiseFocusedProgram();
    if (restoreContent && focusedApp != AppId::None) {
        bool openOpts = false;
        if (Program* fp = findProgram(progId))
            openOpts = fp->openOptionsOnRestore;
        if (focusedApp == AppId::Shell && consoleShell)
            FieldAmouranthLaunch::queueDosConsole(0);
        else
            FieldAmouranthLaunch::queueGui(guiAppFor(focusedApp), false, 0, openOpts);
        if (Program* fp = findProgram(progId))
            fp->openOptionsOnRestore = false;
    }
}

inline Program& appendRestoredTab(AppId app) noexcept {
    Program pr;
    pr.id = nextProgId++;
    pr.app = app;
    pr.icon = appIcon(app);
    pr.tooltip = appTooltip(app);
    int same = 0;
    for (const auto& p : programs)
        if (p.app == app && p.running) ++same;
    if (same > 0)
        std::snprintf(pr.titleBuf, sizeof pr.titleBuf, "%s #%d", appTitle(app), same + 1);
    else
        std::snprintf(pr.titleBuf, sizeof pr.titleBuf, "%s", appTitle(app));
    pr.title = pr.titleBuf;
    pr.running = true;
    pr.minimized = true;
    pr.panelOx = -1.f;
    pr.panelOy = -1.f;
    programs.push_back(pr);
    return programs.back();
}

inline Program& openNewWindow(AppId app) noexcept {
    Program pr;
    pr.id = nextProgId++;
    pr.app = app;
    pr.icon = appIcon(app);
    pr.tooltip = appTooltip(app);
    int same = 0;
    for (const auto& p : programs)
        if (p.app == app && p.running) ++same;
    if (same > 0)
        std::snprintf(pr.titleBuf, sizeof pr.titleBuf, "%s #%d", appTitle(app), same + 1);
    else
        std::snprintf(pr.titleBuf, sizeof pr.titleBuf, "%s", appTitle(app));
    pr.title = pr.titleBuf;
    pr.running = true;
    pr.minimized = false;
    programs.push_back(pr);
    applyAppViewport(app);
    if (consoleShell) {
        FieldDosViewport::panelStretch = true;
        Options::Canvas::DosPanelStretch = true;
        Options::Canvas::ControlFlags |= Options::Canvas::ControlDosPanelStretch;
        FieldDosViewport::panelOx = 0.f;
        FieldDosViewport::panelOy = 0.f;
        FieldDosViewport::panelPositioned = true;
    } else {
        FieldWmShell::applyCompactViewport();
        placeNewWindow(programs.back());
        applyProgramPanel(programs.back());
    }
    FieldWmShell::ensureWindowOpen();
    focusProgram(programs.back().id, true);
    FieldAosAppJournal::recordOpen(programs.back().id,
        FieldAosAppIdentity::fromOsApp(static_cast<std::uint8_t>(app)),
        programs.back().title);
    panelVisible = true;
    needsProgramCanvas = true;
    infoPanelVisible = false;
    FieldAmouranthInfo::visible = false;
    pendingEmptyPanel = false;
    Options::Canvas::DosInputFocused = true;
    FieldAmouranthWm::raiseFocusedProgram();
    growMemoryForApp(app);
    return programs.back();
}

inline float desktopTopInset() noexcept {
    return 0.f;
}

inline void showDosPanelDocked() noexcept {
    if (focusedProgId > 0) {
        if (Program* p = findProgram(focusedProgId)) {
            if (p->panelOx < 0.f) placeNewWindow(*p);
            applyProgramPanel(*p);
        }
    } else if (!programs.empty()) {
        Program& pr = programs.back();
        if (pr.panelOx < 0.f) placeNewWindow(pr);
        applyProgramPanel(pr);
    }
    FieldDosViewport::panelStretch = false;
    Options::Canvas::DosPanelStretch = false;
    Options::Canvas::ControlFlags &= ~Options::Canvas::ControlDosPanelStretch;
    FieldAmouranthWm::applyPanelScale();
    panelVisible = true;
    needsProgramCanvas = true;
    infoPanelVisible = false;
    FieldAmouranthInfo::visible = false;
    Options::Canvas::DosInputFocused = true;
}

inline void removeProgramById(int progId) noexcept {
    programs.erase(std::remove_if(programs.begin(), programs.end(),
        [&](const Program& p) { return p.id == progId; }), programs.end());
    ++FieldAmouranthWm::stackRevision;
}

inline void removeTopProgram() noexcept {
    const int closedId = focusedProgId;
    if (Program* p = findProgram(closedId)) {
        FieldAosAppJournal::recordClose(p->id,
            FieldAosAppIdentity::fromOsApp(static_cast<std::uint8_t>(p->app)),
            p->title);
    }
    if (closedId > 0)
        removeProgramById(closedId);
    else if (!programs.empty())
        programs.pop_back();
    focusedProgId = 0;
    focusedApp = AppId::None;
    if (!programs.empty())
        focusProgram(programs.back().id);
    else
        FieldAosAppJournal::saveSession(nullptr, nullptr, 0, FieldAosAppIdentity::AppId::None);
}

inline void markFocusedMinimized() noexcept {
    if (Program* p = findProgram(focusedProgId)) {
        FieldAosAppJournal::recordMinimize(p->id,
            FieldAosAppIdentity::fromOsApp(static_cast<std::uint8_t>(p->app)),
            p->title);
        p->minimized = true;
    }
}

inline AppId remapRestoredApp(AppId app, bool& openOptions) noexcept {
    openOptions = false;
    switch (app) {
    case AppId::NesSetup:     openOptions = true; return AppId::Nes;
    case AppId::A2600Setup:   openOptions = true; return AppId::A2600;
    case AppId::SmsSetup:     openOptions = true; return AppId::Sms;
    case AppId::GenesisSetup: openOptions = true; return AppId::Genesis;
    case AppId::SnesSetup:    openOptions = true; return AppId::Snes;
    default: return app;
    }
}

inline AppId appFromJournal(FieldAosAppIdentity::AppId app) noexcept {
    return static_cast<AppId>(static_cast<std::uint8_t>(app));
}

inline void restoreSessionFromJournal() noexcept {
    FieldAosAppJournal::loadConfigFromRegistry();
    if (!FieldAosAppJournal::restoreEnabled()) return;
    const FieldAosAppJournal::SessionPlan plan = FieldAosAppJournal::lastSession();
    if (!plan.valid) return;

    // Only restore the last focused app — not every entry in the journal strip.
    int restoreIdx = -1;
    if (plan.focusApp != FieldAosAppIdentity::AppId::None) {
        for (int i = 0; i < plan.count; ++i) {
            if (plan.apps[i] == plan.focusApp) {
                restoreIdx = i;
                break;
            }
        }
    }
    if (restoreIdx < 0) return;

    FieldAosAppJournal::restoringSession = true;
    AppId app = appFromJournal(plan.apps[restoreIdx]);
    if (app == AppId::None || app == AppId::Vscodium) {
        FieldAosAppJournal::restoringSession = false;
        return;
    }
    bool wantOpts = false;
    app = remapRestoredApp(app, wantOpts);
    if (plan.snapshots[restoreIdx][0])
        FieldAosAppJournal::setAppSnapshot(plan.apps[restoreIdx], plan.snapshots[restoreIdx]);
    Program& pr = appendRestoredTab(app);
    if (wantOpts)
        pr.openOptionsOnRestore = true;
    FieldAmouranthLaunch::clear();
    focusedProgId = 0;
    focusedApp = AppId::None;
    panelVisible = false;
    pendingEmptyPanel = false;
    hideDosPanel();
    FieldAosAppJournal::restoringSession = false;
    FieldAosAppIdentity::AppId apps[1]{ plan.apps[restoreIdx] };
    const char* snaps[1]{ plan.snapshots[restoreIdx][0] ? plan.snapshots[restoreIdx] : "" };
    FieldAosAppJournal::saveSession(apps, snaps, 1, plan.apps[restoreIdx]);
    std::fprintf(stderr,
        "[AMOURANTHOS] Session journal — restored %s on taskbar (click to open)\n",
        pr.title);
}

inline void syncDesktopState() noexcept {
    if (qaHoldInfoDesktop) return;
    if (!active && !consoleShell) return;
    if (guestAppRunning() && panelVisible) return;
    if (focusedProgId > 0 && panelVisible) return;
    if (focusedProgId > 0) {
        if (Program* p = findProgram(focusedProgId)) {
            if (!p->minimized) {
                if (p->panelOx < 0.f) placeNewWindow(*p);
                applyProgramPanel(*p);
                panelVisible = true;
                needsProgramCanvas = true;
                return;
            }
        }
    }
    if (needsProgramCanvas) return;
    hideDosPanel();
}

inline bool launchVscodium() noexcept {
    const char* bins[] = { "codium", "code", nullptr };
    for (const char* b : bins) {
        char probe[128];
        std::snprintf(probe, sizeof probe, "command -v %s >/dev/null 2>&1", b);
        const int probeRc = std::system(probe);
        if (probeRc == 0) {
            char run[160];
            std::snprintf(run, sizeof run, "%s . >/dev/null 2>&1 &", b);
            const int runRc = std::system(run);
            (void)runRc;
            openNewWindow(AppId::Vscodium);
            return true;
        }
    }
    std::fprintf(stderr, "[AMOURANTHOS] VSCodium/Code not on PATH\n");
    return false;
}

inline void launchDosConsole() noexcept {
    FieldAosAppJournal::recordAction(FieldAosAppIdentity::AppId::Shell,
        "RTX Shell", "DOS command console");
    FieldAmouranthLaunch::queueDosConsole();
    showDosPanelDocked();
}

inline bool init(SDL_Window* window) noexcept {
    hostWindow = window;
    return true;
}

inline void shutdown() noexcept { deactivate(); }

inline bool shellChromeActive() noexcept { return active || consoleShell; }

inline void packStartLabel(std::uint8_t* ram) noexcept {
    if (!ram) return;
    const char* label = "Start";
    const std::uint32_t base = FieldAmouranthHudRam::TASKBAR_RAM
        + FieldAmouranthHudRam::START_LABEL_OFF;
    for (int i = 0; label[i]; ++i)
        ram[base + static_cast<std::uint32_t>(i)] =
            static_cast<std::uint8_t>(label[i]);
    ram[base + 5u] = 0u;
}

inline bool forceTaskbarChromeClick = false;

inline bool onMouseDown(SDL_Window* window, float lx, float ly, Uint8 button, Uint8 clicks) noexcept;
inline void packChromeRam(std::uint8_t* ram) noexcept;

inline void boot() noexcept {
    active = true;
    consoleShell = false;
    Options::Canvas::DosInputFocused = false;
    FieldRtxWidgets::g.clear();
    startOpen = false;
    programs.clear();
    nextProgId = 1;
    focusedApp = AppId::None;
    focusedProgId = 0;
    pendingEmptyPanel = false;
    Options::AmouranthOs::EnableDesktop = true;
    Options::AmouranthOs::EnableTaskbar = true;
    Options::SDL3::StartFullscreen = true;
    Options::SDL3::PendingFullscreenApply = true;
    FieldDosViewport::panelStretch = false;
    Options::Canvas::DosPanelStretch = false;
    Options::Canvas::ControlFlags &= ~Options::Canvas::ControlDosPanelStretch;
    infoPanelVisible = false;
    FieldAmouranthInfo::visible = false;
    panelVisible = false;
    FieldAmouranthWm::resetScale();
    FieldAmouranthDesktop::boot();
    hideDosPanel();
    FieldAmouranthDesktop::applyDisplayScale(FieldAmouranthDesktop::displayScale);
    Options::Canvas::ColorTheme = 0u;
    Options::Canvas::DosCrispFont = true;
    FieldDosViewport::crispFont = true;
    FieldDosViewport::subpixelFont = false;
    FieldDosViewport::sharpen = 0.55f;
    FieldDosViewport::scanlines = false;
    FieldDosViewport::scanlineMix = 0.02f;
    FieldDosViewport::panelGlow = 0.04f;
    FieldRegistry::ensure();
    FieldRegistry::applyMemoryConfig();
    FieldRtxThemes::applyIndex(FieldAosAppJournal::themeIndexFromRegistry());
    FieldRuntimeInfo::refresh();
    FieldAmouranthInfo::tick();
    FieldAmouranthMenu::rebuildVisible();
    FieldAosAppJournal::loadConfigFromRegistry();
    FieldAosAppJournal::loadFromVfs();
    wallpaperIndex = 8u;
    programs.clear();
    focusedProgId = 0;
    focusedApp = AppId::None;
    panelVisible = false;
    if (FieldAosAppJournal::restoreEnabled()
            && FieldAosAppJournal::lastSession().valid) {
        restoreSessionFromJournal();
    } else {
        FieldAosAppJournal::clearStartupTaskbar();
    }
    FieldAosAppJournal::bootStamp();
    if (FieldX86Emu::ramHost) {
        FieldAmouranthHudRam::clearRegion(FieldX86Emu::ramHost);
        packChromeRam(FieldX86Emu::ramHost);
    }
    FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Shell, false, 0);
    std::fprintf(stderr,
        "[AMOURANTHOS] RTX desktop — %s | RTX Shell at boot | Start for more\n",
        FieldRtxThemes::kPresets[static_cast<std::size_t>(FieldRtxThemes::activeIndex)].name);
    std::fprintf(stderr,
        "[AMOURANTHOS] Chrome build 2026-06-19s — instant aos_load desktop, x86 hotswap background\n");
}

inline void bootShell() noexcept { boot(); }

inline void prepareExitConfirmUi() noexcept;
inline void applyShutdownState() noexcept;

inline void sanitizeVgaTail(std::uint8_t* ram) noexcept {
    if (!ram) return;
    constexpr std::uint32_t vga = 0x000B8000u;
    for (int row = 22; row < 25; ++row) {
        for (int col = 0; col < 80; ++col) {
            const std::uint32_t off = vga + static_cast<std::uint32_t>((row * 80 + col) * 2);
            ram[off] = ' ';
            ram[off + 1u] = 0x07u;
        }
    }
}

inline void tick(int w, int h) noexcept {
    if (!shellChromeActive()) return;
    if (FieldDosChrome::chromeUsesRenderSpace()) {
        winW = static_cast<int>(FieldDosViewport::renderW);
        winH = static_cast<int>(FieldDosViewport::renderH);
    } else {
        winW = w;
        winH = h;
    }
    FieldAmouranthInfo::tick();
    FieldAmouranthDnD::tick();
    syncDesktopState();
}

inline bool pointIn(float px, float py, float x, float y, float fw, float fh) noexcept {
    return px >= x && py >= y && px < x + fw && py < y + fh;
}

// Match aosUiScale() in shaders — max(w/3840, 0.75) * 1.35.
inline float chromeLayoutW() noexcept {
    if (FieldDosChrome::chromeUsesRenderSpace()) return FieldDosViewport::renderW;
    if (FieldDosViewport::winW > 0.f) return FieldDosViewport::winW;
    return static_cast<float>(winW > 0 ? winW : static_cast<int>(AOS_UI_REF_W));
}

inline float chromeLayoutH() noexcept {
    if (FieldDosChrome::chromeUsesRenderSpace()) return FieldDosViewport::renderH;
    if (winH > 0) return static_cast<float>(winH);
    if (FieldDosViewport::winH > 0.f) return FieldDosViewport::winH;
    return 2160.f;
}

// Match shader aosViewport() when chrome is live-synced; else AmouranthOS win metrics.
inline float chromeViewportW() noexcept {
    if (FieldDosChrome::chromeUsesRenderSpace() && FieldDosViewport::renderW > 1.f)
        return FieldDosViewport::renderW;
    if (winW > 0) return static_cast<float>(winW);
    if (FieldDosViewport::winW > 1.f) return FieldDosViewport::winW;
    return AOS_UI_REF_W;
}

inline float chromeViewportH() noexcept {
    if (FieldDosChrome::chromeUsesRenderSpace() && FieldDosViewport::renderH > 1.f)
        return FieldDosViewport::renderH;
    if (winH > 0) return static_cast<float>(winH);
    if (FieldDosViewport::winH > 1.f) return FieldDosViewport::winH;
    return 2160.f;
}

inline float uiScale() noexcept {
    const float base = chromeViewportW() / AOS_UI_REF_W;
    return std::max(base, 0.75f) * UI_BOOST;
}

inline float scaledTaskbarH() noexcept { return TASKBAR_H * uiScale(); }
inline float scaledStartW() noexcept { return START_W * uiScale(); }
inline float scaledFolderBtnW() noexcept { return FOLDER_BTN_W * uiScale(); }
inline float scaledQuickBtnW() noexcept { return FOLDER_BTN_W * uiScale(); }
inline float scaledQuickLaunchStripW() noexcept {
    return static_cast<float>(QUICK_LAUNCH_N) * scaledQuickBtnW()
        + static_cast<float>(QUICK_LAUNCH_N - 1) * 4.f * uiScale();
}
inline FieldTaskbarLayout::Layout taskbarLayout() noexcept {
    return FieldTaskbarLayout::compute(chromeViewportW(), chromeViewportH(), uiScale());
}

inline float taskTabsOriginX() noexcept {
    return taskbarLayout().tabX;
}
inline float quickLaunchX(int idx) noexcept {
    return FieldTaskbarLayout::quickLaunchX(taskbarLayout(), idx);
}
inline float scaledTabW() noexcept { return TAB_W * uiScale(); }
inline float scaledClockW() noexcept { return CLOCK_W * uiScale(); }
inline float scaledMenuW() noexcept { return MENU_W * uiScale(); }
inline float scaledMenuFlyoutW() noexcept { return MENU_FLYOUT_W * uiScale(); }
inline float scaledMenuFlyoutGap() noexcept { return MENU_FLYOUT_GAP * uiScale(); }
inline float scaledMenuRowH() noexcept { return MENU_ROW_H * uiScale(); }
inline float scaledMenuHeaderH() noexcept { return MENU_HEADER_H * uiScale(); }
inline float scaledMenuPad() noexcept { return MENU_PAD * uiScale(); }

inline float scaledMenuTotalW() noexcept {
    float w = scaledMenuW() + scaledMenuPad() * 2.f;
    if (FieldAmouranthMenu::flyoutOpen() && !FieldAmouranthSearchFlyout::active())
        w += scaledMenuFlyoutGap() + scaledMenuFlyoutW();
    if (FieldAmouranthSearchFlyout::active())
        w += FieldAmouranthSearchFlyout::gap(uiScale())
            + FieldAmouranthSearchFlyout::panelW(uiScale());
    return w;
}

inline float scaledSearchPanelLeft() noexcept {
    float left = scaledMenuPad() + scaledMenuW() + scaledMenuPad();
    if (FieldAmouranthMenu::flyoutOpen() && !FieldAmouranthSearchFlyout::active())
        left += scaledMenuFlyoutGap() + scaledMenuFlyoutW();
    return left + FieldAmouranthSearchFlyout::gap(uiScale());
}

inline float scaledSearchPanelTop() noexcept {
    return FieldAmouranthSearchFlyout::panelTop(chromeLayoutH(), scaledTaskbarH(), uiScale());
}

inline float scaledSearchPanelW() noexcept {
    return FieldAmouranthSearchFlyout::panelW(uiScale());
}

inline float scaledSearchPanelH() noexcept {
    return FieldAmouranthSearchFlyout::panelH(uiScale());
}

inline float scaledTopBarH() noexcept {
    return desktopTopInset();
}

inline float scaledMenuRootHeight() noexcept {
    return FieldAmouranthMenu::rootMenuHeight(
        scaledMenuRowH(), scaledMenuHeaderH(), scaledMenuPad());
}

inline float scaledMenuFlyoutHeight() noexcept {
    return FieldAmouranthMenu::flyoutMenuHeight(scaledMenuRowH(), scaledMenuPad());
}

inline float scaledMenuHeight() noexcept {
    return scaledMenuRootHeight();
}

inline float taskbarY() noexcept { return chromeViewportH() - scaledTaskbarH(); }

inline void closeStartMenu() noexcept {
    startOpen = false;
    FieldAmouranthSearchFlyout::clearQuery();
    FieldAmouranthMenu::closeMenuFocus();
}

inline void syncStartTextInput(SDL_Window* window) noexcept {
    if (!window) return;
    const bool want = startOpen && shellChromeActive();
    if (want == startTextInputActive) return;
    startTextInputActive = want;
    if (want)
        SDL_StartTextInput(window);
    else
        SDL_StopTextInput(window);
}

// Match aosStartButtonBounds / aosFolderButtonBounds in x86.comp (pad + lift).
inline void taskbarChromeButtonY(float& y0, float& y1) noexcept {
    const auto L = taskbarLayout();
    y0 = L.barY0 + L.pad - L.lift;
    y1 = L.barY1 - L.pad;
}

// Match aosTbStartBounds / aosStartMenu rootTop (menu hangs from Start button top).
inline float startMenuAnchorY() noexcept {
    const auto L = taskbarLayout();
    return L.barY0 + L.pad - L.lift;
}

inline float scaledMenuRootTop() noexcept {
    return startMenuAnchorY() - scaledMenuRootHeight();
}

// Root menu rows start at rootTop + headH in aos_chrome.inc (no extra pad before rows).
inline float scaledMenuRootRowY0() noexcept {
    return scaledMenuRootTop() + scaledMenuHeaderH();
}

inline void exitConfirmPanelBounds(float& x0, float& y0, float& w, float& h) noexcept;
inline int exitConfirmButtonAt(float mx, float my) noexcept;

inline float scaledMenuFlyoutTop() noexcept {
    return startMenuAnchorY() - scaledMenuFlyoutHeight();
}

inline float scaledFilesCellW() noexcept { return 88.f * uiScale(); }
inline float scaledFilesCellH() noexcept { return 96.f * uiScale(); }
inline float scaledFilesPad() noexcept { return 10.f * uiScale(); }
inline float scaledFilesHeadH() noexcept { return 30.f * uiScale(); }

inline float scaledFilesPanelW() noexcept {
    return scaledFilesPad() * 2.f
        + scaledFilesCellW() * static_cast<float>(FieldAmouranthFilesMenu::GRID_COLS);
}

inline float scaledFilesPanelH() noexcept {
    const int rows = FieldAmouranthFilesMenu::gridRows();
    return scaledFilesHeadH() + scaledFilesPad() * 2.f
        + scaledFilesCellH() * static_cast<float>(rows);
}

inline float scaledFilesPanelX() noexcept {
    return quickLaunchX(0);
}

inline std::uint8_t browserIconSlotFromId(const std::string& id) noexcept {
    using IS = FieldAmouranthTextures::IconSlot;
    if (id.find("firefox") != std::string::npos)
        return static_cast<std::uint8_t>(IS::BrowserFirefox);
    if (id.find("chrome") != std::string::npos || id.find("chromium") != std::string::npos)
        return static_cast<std::uint8_t>(IS::BrowserChrome);
    if (id.find("edge") != std::string::npos)
        return static_cast<std::uint8_t>(IS::BrowserEdge);
    return static_cast<std::uint8_t>(IS::Display);
}

inline void syncBrowserIconSlot() noexcept {
    if (FieldBrowserHook::hooked && !FieldBrowserHook::browserId.empty()) {
        browserIconSlot = browserIconSlotFromId(FieldBrowserHook::browserId);
        return;
    }
    std::string bin, id, label;
    if (FieldBrowserHook::resolveDefault(bin, id, label))
        browserIconSlot = browserIconSlotFromId(id);
    else
        browserIconSlot = static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Display);
}

inline float scaledFilesPanelTop() noexcept {
    float btnY0 = 0.f, btnY1 = 0.f;
    taskbarChromeButtonY(btnY0, btnY1);
    return btnY0 - scaledFilesPanelH() - 6.f * uiScale();
}

constexpr float TASKBAR_HIT_SLOP = 8.f;

// Start/quick-launch buttons extend above the taskbar strip (shader lift); test full bounds.
inline HitZone hitTaskbarButtons(float mx, float my, float slop = 0.f) noexcept {
    if (!Options::AmouranthOs::EnableTaskbar) return HitZone::None;
    float btnY0 = 0.f, btnY1 = 0.f;
    taskbarChromeButtonY(btnY0, btnY1);
    btnY0 -= slop;
    btnY1 += slop;
    if (my < btnY0 || my >= btnY1) return HitZone::None;
    const float btnH = btnY1 - btnY0;
    const float pad = 6.f * uiScale() - slop;
    if (pointIn(mx, my, pad, btnY0, scaledStartW() + slop * 2.f, btnH))
        return HitZone::StartBtn;
    const float btnW = scaledQuickBtnW() + slop * 2.f;
    for (int qi = 0; qi < QUICK_LAUNCH_N; ++qi) {
        if (!pointIn(mx, my, quickLaunchX(qi) - slop, btnY0, btnW, btnH)) continue;
        switch (qi) {
        case 0: return HitZone::FilesBtn;
        case 1: return HitZone::TerminalBtn;
        case 2: return HitZone::MonitorBtn;
        case 3: return HitZone::BrowserBtn;
        default: break;
        }
    }
    return HitZone::None;
}

inline bool taskbarChromeCapturesPointer(float mx, float my) noexcept {
    if (!shellChromeActive() || !Options::AmouranthOs::EnableTaskbar)
        return false;
    if (my < scaledTopBarH()) return false;
    if (hitTaskbarButtons(mx, my) != HitZone::None) return true;
    return my >= taskbarY();
}

inline bool isTaskbarChromeHit(HitZone zone) noexcept {
    switch (zone) {
    case HitZone::StartBtn:
    case HitZone::FilesBtn:
    case HitZone::TerminalBtn:
    case HitZone::MonitorBtn:
    case HitZone::BrowserBtn:
    case HitZone::TaskBtn:
    case HitZone::Clock:
    case HitZone::Taskbar:
    case HitZone::FilesMenu:
    case HitZone::StartMenu:
    case HitZone::StartMenuFlyout:
    case HitZone::StartMenuSearch:
        return true;
    default:
        return false;
    }
}

inline HitZone hitTest(float mx, float my) noexcept {
    if (my < scaledTopBarH()) return HitZone::None;
    const float slop = TASKBAR_HIT_SLOP * uiScale();
    if (HitZone btn = hitTaskbarButtons(mx, my, slop); btn != HitZone::None)
        return btn;
    if (HitZone btn = hitTaskbarButtons(mx, my); btn != HitZone::None)
        return btn;
    const auto L = taskbarLayout();
    if (my >= L.barY0) {
        int tabSlot = 0;
        for (const auto& p : programs) {
            if (!p.running) continue;
            const float tx = FieldTaskbarLayout::tabX(L, tabSlot);
            if (pointIn(mx, my, tx, L.tabY, L.tabW, L.tabH))
                return HitZone::TaskBtn;
            ++tabSlot;
        }
        if (pointIn(mx, my, L.clockX0, L.barY0 + 5.f * uiScale(),
                L.clockW, L.taskH - 10.f * uiScale()))
            return HitZone::Clock;
        return HitZone::Taskbar;
    }
    if (filesOpen) {
        if (pointIn(mx, my, scaledFilesPanelX(), scaledFilesPanelTop(),
                scaledFilesPanelW(), scaledFilesPanelH()))
            return HitZone::FilesMenu;
    }
    if (FieldAmouranthFolderView::open) {
        const float scale = uiScale();
        const float pw = 640.f * scale;
        const float ph = 480.f * scale;
        const float px = static_cast<float>(winW) * 0.5f - pw * 0.5f;
        const float py = static_cast<float>(winH) * 0.38f - ph * 0.5f;
        if (pointIn(mx, my, px, py, pw, ph))
            return HitZone::FolderView;
    }
    if (startOpen) {
        const float rootSy = scaledMenuRootTop();
        const float rootH = scaledMenuRootHeight();
        if (FieldAmouranthSearchFlyout::active()) {
            const float sx = scaledSearchPanelLeft();
            const float sy = scaledSearchPanelTop();
            const float sw = scaledSearchPanelW();
            const float sh = scaledSearchPanelH();
            if (pointIn(mx, my, sx, sy, sw, sh))
                return HitZone::StartMenuSearch;
        }
        if (FieldAmouranthMenu::flyoutOpen() && !FieldAmouranthSearchFlyout::active()) {
            const float flySy = scaledMenuFlyoutTop();
            const float flyH = scaledMenuFlyoutHeight();
            const float fx = scaledMenuPad() + scaledMenuW() + scaledMenuFlyoutGap();
            if (pointIn(mx, my, fx, flySy, scaledMenuFlyoutW(), flyH))
                return HitZone::StartMenuFlyout;
        }
        if (pointIn(mx, my, scaledMenuPad(), rootSy, scaledMenuW(), rootH))
            return HitZone::StartMenu;
    }
    if (FieldAmouranthDesktop::iconAt(mx, my) >= 0)
        return HitZone::Desktop;
    return HitZone::Desktop;
}

inline int programIndexFromTaskTab(int tabSlot) noexcept {
    if (tabSlot < 0) return -1;
    int slot = 0;
    for (std::size_t i = 0; i < programs.size(); ++i) {
        if (!programs[i].running) continue;
        if (slot == tabSlot) return static_cast<int>(i);
        ++slot;
    }
    return -1;
}

// Tab slot index (running programs only) — matches packTaskbarRam / shader tabCount order.
inline int taskBtnIndex(float mx, float my) noexcept {
    const auto L = taskbarLayout();
    int tabSlot = 0;
    for (std::size_t i = 0; i < programs.size(); ++i) {
        if (!programs[i].running) continue;
        const float tx = FieldTaskbarLayout::tabX(L, tabSlot);
        if (pointIn(mx, my, tx, L.tabY, L.tabW, L.tabH))
            return tabSlot;
        ++tabSlot;
    }
    return -1;
}

inline FieldAmouranthLaunch::GuiApp guiAppFor(AppId app) noexcept {
    switch (app) {
    case AppId::Shell:    return FieldAmouranthLaunch::GuiApp::Shell;
    case AppId::AmmoCode: return FieldAmouranthLaunch::GuiApp::AmmoCode;
    case AppId::QBasic:   return FieldAmouranthLaunch::GuiApp::QBasic;
    case AppId::FieldC:   return FieldAmouranthLaunch::GuiApp::FieldC;
    case AppId::PadTest:  return FieldAmouranthLaunch::GuiApp::PadTest;
    case AppId::Nes:      return FieldAmouranthLaunch::GuiApp::Nes;
    case AppId::NesSetup: return FieldAmouranthLaunch::GuiApp::NesSetup;
    case AppId::Browser:  return FieldAmouranthLaunch::GuiApp::Browser;
    case AppId::FileCmd:  return FieldAmouranthLaunch::GuiApp::FileCmd;
    case AppId::Doom:     return FieldAmouranthLaunch::GuiApp::Doom;
    case AppId::Monitor:  return FieldAmouranthLaunch::GuiApp::Monitor;
    case AppId::A2600:      return FieldAmouranthLaunch::GuiApp::A2600;
    case AppId::A2600Setup: return FieldAmouranthLaunch::GuiApp::A2600Setup;
    case AppId::Sms:        return FieldAmouranthLaunch::GuiApp::Sms;
    case AppId::SmsSetup:   return FieldAmouranthLaunch::GuiApp::SmsSetup;
    case AppId::Genesis:    return FieldAmouranthLaunch::GuiApp::Genesis;
    case AppId::GenesisSetup: return FieldAmouranthLaunch::GuiApp::GenesisSetup;
    case AppId::Snes:       return FieldAmouranthLaunch::GuiApp::Snes;
    case AppId::SnesSetup:  return FieldAmouranthLaunch::GuiApp::SnesSetup;
    default: return FieldAmouranthLaunch::GuiApp::None;
    }
}

inline void dispatchAction(int action) noexcept {
    switch (action) {
    case 1:
        openNewWindow(AppId::AmmoCode);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::AmmoCode);
        break;
    case 2:
        FieldAmouranthLaunch::queueDosConsole();
        break;
    case 32:
        openNewWindow(AppId::Shell);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Shell);
        break;
    case 3:
        openNewWindow(AppId::QBasic);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::QBasic);
        break;
    case 4:
        openNewWindow(AppId::FieldC);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::FieldC);
        break;
    case 5:
        openNewWindow(AppId::PadTest);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::PadTest);
        break;
    case 6:
        openNewWindow(AppId::Nes);
        showDosPanelDocked();
        break;
    case 30:
        openNewWindow(AppId::Nes);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Nes, false, 2, true);
        break;
    case 31:
        openNewWindow(AppId::Nes);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Nes, true, 2, true);
        break;

    case 11:
        openNewWindow(AppId::FileCmd);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::FileCmd);
        showDosPanelDocked();
        break;
    case 12:
        launchVscodium();
        break;
    case 33:
        launchDosConsole();
        break;
    case 15:
        openNewWindow(AppId::Browser);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Browser);
        break;
    case 7:
        openNewWindow(AppId::Doom);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Doom);
        break;
    case 20:
        openNewWindow(AppId::Shell);
        FieldAmouranthLaunch::queue("C:\\GAMES\\KEEN4\\KEEN4E.EXE");
        break;
    case 21:
        openNewWindow(AppId::Shell);
        FieldAmouranthLaunch::queue("C:\\GAMES\\WOLF3D\\WOLF3D.EXE");
        break;
    case 22:
        openNewWindow(AppId::Shell);
        FieldAmouranthLaunch::queue("C:\\GAMES\\COSMO\\COSMO1.EXE");
        break;
    case 13:
        openNewWindow(AppId::Shell);
        FieldAmouranthLaunch::queue("DRIVES");
        break;
    case 14:
        openNewWindow(AppId::Shell);
        FieldAmouranthLaunch::queue("MOUNT CD");
        break;
    case 16:
        openNewWindow(AppId::Shell);
        FieldAmouranthLaunch::queue("BIOS");
        break;
    case 17:
        openNewWindow(AppId::Shell);
        FieldAmouranthLaunch::queue("DEVICES");
        break;
    case 18:
        openNewWindow(AppId::Shell);
        FieldAmouranthLaunch::queue("REGEDIT");
        break;
    case 19:
    case 28:
        openNewWindow(AppId::Shell);
        FieldAmouranthLaunch::queue("AMMOTEXT");
        break;
    case 29:
        openNewWindow(AppId::Shell);
        FieldAmouranthLaunch::queue("THEMES");
        break;
    case 23:
        openNewWindow(AppId::Shell);
        FieldAmouranthLaunch::queue("MEMORYUP /S");
        break;
    case 24:
        openNewWindow(AppId::Shell);
        FieldAmouranthLaunch::queue("SCANDISK");
        break;
    case 25:
        openNewWindow(AppId::Shell);
        FieldAmouranthLaunch::queue("TOOLS");
        break;
    case 26:
        openNewWindow(AppId::Shell);
        FieldAmouranthLaunch::queue("SOUND");
        break;
    case 27:
        openNewWindow(AppId::Shell);
        FieldAmouranthLaunch::queue("EXTMAP");
        break;
    case 34:
        openNewWindow(AppId::A2600);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::A2600);
        break;
    case 35:
        openNewWindow(AppId::A2600);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::A2600, false, 2, true);
        break;
    case 36:
        openNewWindow(AppId::Sms);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Sms);
        break;
    case 37:
        openNewWindow(AppId::Sms);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Sms, false, 2, true);
        break;
    case 38:
        openNewWindow(AppId::Genesis);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Genesis);
        break;
    case 39:
        openNewWindow(AppId::Genesis);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Genesis, false, 2, true);
        break;
    case 40:
        openNewWindow(AppId::Snes);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Snes);
        break;
    case 41:
        openNewWindow(AppId::Snes);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Snes, false, 2, true);
        break;
    case 42:
        openNewWindow(AppId::Shell);
        showDosPanelDocked();
        FieldAosChipsWave::openAmigaLove();
        break;
    case 43:
        openNewWindow(AppId::Shell);
        showDosPanelDocked();
        FieldAosChipsWave::openLoveOfEverything();
        break;
    case 99:
        FieldAmouranthExitConfirm::show();
        break;
    default: break;
    }
    FieldAmouranthLaunch::deferGuiFrames = 0;
    closeStartMenu();
    filesOpen = false;
    FieldAmouranthFilesMenu::hover = -1;
    syncDesktopState();
}

inline bool onTextInput(const char* text) noexcept {
    if (!shellChromeActive() || !startOpen || !text || !text[0]) return false;
    for (const char* p = text; *p; ++p)
        FieldAmouranthSearchFlyout::appendChar(*p);
    return true;
}

inline bool onKeyDown(SDL_Scancode sc) noexcept {
    if (!shellChromeActive()) return false;
    if (FieldAmouranthExitConfirm::isOpen()) {
        std::uint16_t key = 0;
        if (sc == SDL_SCANCODE_ESCAPE) key = 0x011Bu;
        else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) key = 0x1C0Du;
        if (key && FieldAmouranthExitConfirm::onKey(key, FieldX86Emu::ramHost))
            return true;
        return true;
    }
    if (!startOpen) return false;

    if (sc == SDL_SCANCODE_BACKSPACE) {
        FieldAmouranthSearchFlyout::backspace();
        return true;
    }
    if (sc == SDL_SCANCODE_ESCAPE) {
        if (FieldAmouranthSearchFlyout::query[0]) {
            FieldAmouranthSearchFlyout::clearQuery();
            return true;
        }
        closeStartMenu();
        return true;
    }

    std::uint16_t key = 0;
    switch (sc) {
    case SDL_SCANCODE_UP:     key = 0x4800u; break;
    case SDL_SCANCODE_DOWN:   key = 0x5000u; break;
    case SDL_SCANCODE_LEFT:   key = 0x4B00u; break;
    case SDL_SCANCODE_RIGHT:  key = 0x4D00u; break;
    case SDL_SCANCODE_RETURN:
    case SDL_SCANCODE_KP_ENTER:
        key = 0x1C0Du;
        break;
    default: return false;
    }

    if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) {
        if (FieldAmouranthSearchFlyout::active()) {
            int row = FieldAmouranthSearchFlyout::hover;
            if (row < 0) row = 0;
            FieldAmouranthSearchFlyout::launchRow(row, FieldX86Emu::ramHost);
            closeStartMenu();
            filesOpen = false;
            FieldAmouranthFilesMenu::hover = -1;
            return true;
        }
    }
    if (!FieldAmouranthMenu::handleMenuKey(key)) return false;
    if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) {
        int action = 0;
        if (FieldAmouranthMenu::focusPane == FieldAmouranthMenu::MenuPane::Flyout
            && FieldAmouranthMenu::flyoutOpen())
            action = FieldAmouranthMenu::actionForFlyoutRow(FieldAmouranthMenu::flyoutFocus);
        else
            action = FieldAmouranthMenu::actionForRootRow(FieldAmouranthMenu::rootFocus);
        if (action > 0)
            dispatchAction(action);
    }
    return true;
}

inline bool onMouseDown(SDL_Window* window, float lx, float ly, Uint8 /*button*/, Uint8 /*clicks*/) noexcept {
    if (!shellChromeActive()) return false;
    float mx = 0.f, my = 0.f;
    FieldDosChrome::chromePointerPixels(window, lx, ly, mx, my);
    hover = hitTest(mx, my);
    if (FieldAmouranthExitConfirm::isOpen()) {
        std::uint8_t* ram = FieldX86Emu::ramHost;
        const int exitBtn = exitConfirmButtonAt(mx, my);
        if (exitBtn == 1) {
            FieldAmouranthExitConfirm::confirmYes(ram);
            return true;
        }
        if (exitBtn == 2) {
            FieldAmouranthExitConfirm::dismiss(ram);
            return true;
        }
        int action = 0;
        if (ram && FieldAmouranthExitConfirm::pumpMouse(ram, action))
            return true;
        if (!isTaskbarChromeHit(hover) && !forceTaskbarChromeClick)
            return true;
        FieldAmouranthExitConfirm::dismiss(ram);
    }
    const float slop = TASKBAR_HIT_SLOP * uiScale();
    if (HitZone btn = hitTaskbarButtons(mx, my, slop); btn != HitZone::None)
        hover = btn;
    else if (HitZone tight = hitTaskbarButtons(mx, my); tight != HitZone::None)
        hover = tight;
    if (isTaskbarChromeHit(hover))
        Options::Canvas::DosInputFocused = false;
    if (!forceTaskbarChromeClick
            && !isTaskbarChromeHit(hover) && !taskbarChromeCapturesPointer(mx, my)
            && (panelVisible || consoleShell)) {
        const auto wh = FieldAmouranthWm::hitTest(mx, my);
        if (wh == FieldAmouranthWm::ChromeHit::Content) {
            Options::Canvas::DosInputFocused = true;
            closeStartMenu();
            filesOpen = false;
            FieldAmouranthFilesMenu::hover = -1;
            syncStartTextInput(window);
            return true;
        }
        if (wh != FieldAmouranthWm::ChromeHit::None)
            return false;
        const int hitProg = FieldAmouranthWm::hitTestProgramStack(mx, my, true);
        if (hitProg > 0 && hitProg != focusedProgId) {
            focusProgram(hitProg, false);
            Options::Canvas::DosInputFocused = false;
            closeStartMenu();
            filesOpen = false;
            FieldAmouranthFilesMenu::hover = -1;
            return true;
        }
    }

    if (hover == HitZone::StartBtn) {
        startOpen = !startOpen;
        filesOpen = false;
        FieldAmouranthFilesMenu::hover = -1;
        if (startOpen) {
            FieldAmouranthMenu::rebuildVisible();
        } else {
            FieldAmouranthSearchFlyout::clearQuery();
            FieldAmouranthMenu::closeMenuFocus();
        }
        syncStartTextInput(window);
        return true;
    }
    if (hover == HitZone::FilesBtn) {
        closeStartMenu();
        syncStartTextInput(window);
        filesOpen = !filesOpen;
        if (filesOpen) FieldAmouranthFilesMenu::rebuild();
        else FieldAmouranthFilesMenu::hover = -1;
        return true;
    }
    if (hover == HitZone::TerminalBtn) {
        closeStartMenu();
        syncStartTextInput(window);
        filesOpen = false;
        launchDosConsole();
        return true;
    }
    if (hover == HitZone::MonitorBtn) {
        closeStartMenu();
        syncStartTextInput(window);
        filesOpen = false;
        FieldAosAppJournal::recordAction(FieldAosAppIdentity::AppId::Monitor,
            "System Monitor", "quick-launch monitor");
        openNewWindow(AppId::Monitor);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Monitor);
        showDosPanelDocked();
        return true;
    }
    if (hover == HitZone::BrowserBtn) {
        closeStartMenu();
        syncStartTextInput(window);
        filesOpen = false;
        FieldAosAppJournal::recordAction(FieldAosAppIdentity::AppId::Browser,
            "Field Web", "quick-launch browser");
        openNewWindow(AppId::Browser);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Browser);
        showDosPanelDocked();
        return true;
    }
    if (hover == HitZone::FilesMenu) {
        const int idx = FieldAmouranthFilesMenu::itemAt(
            mx, my, scaledFilesPanelX(), scaledFilesPanelTop(),
            scaledFilesCellW(), scaledFilesCellH(), scaledFilesPad());
        if (idx >= 0) {
            filesDragIdx = idx;
            filesDragMx0 = mx;
            filesDragMy0 = my;
        }
        return true;
    }
    if (hover == HitZone::StartMenuSearch) {
        const float rowY0 = scaledSearchPanelTop()
            + FieldAmouranthSearchFlyout::pad(uiScale());
        const int idx = FieldAmouranthSearchFlyout::rowAt(my, rowY0,
            FieldAmouranthSearchFlyout::rowH(uiScale()),
            FieldAmouranthSearchFlyout::resultCount);
        if (idx >= 0) {
            FieldAmouranthSearchFlyout::launchRow(idx, FieldX86Emu::ramHost);
            closeStartMenu();
            syncStartTextInput(window);
            filesOpen = false;
            FieldAmouranthFilesMenu::hover = -1;
        }
        return true;
    }
    if (hover == HitZone::StartMenu || hover == HitZone::StartMenuFlyout) {
        if (hover == HitZone::StartMenuFlyout) {
            const float rowY0 = scaledMenuFlyoutTop() + scaledMenuPad();
            const int idx = FieldAmouranthMenu::rowAt(my, rowY0, scaledMenuRowH(),
                FieldAmouranthMenu::flyoutCount);
            const int action = FieldAmouranthMenu::actionForFlyoutRow(idx);
            if (action > 0)
                dispatchAction(action);
        } else {
            const float rowY0 = scaledMenuRootRowY0();
            const int idx = FieldAmouranthMenu::rowAt(my, rowY0, scaledMenuRowH(),
                FieldAmouranthMenu::rootCount);
            const int action = FieldAmouranthMenu::actionForRootRow(idx);
            if (action > 0)
                dispatchAction(action);
        }
        return true;
    }
    if (hover == HitZone::TaskBtn) {
        const int tabSlot = taskBtnIndex(mx, my);
        const int pi = programIndexFromTaskTab(tabSlot);
        if (pi >= 0 && pi < static_cast<int>(programs.size())) {
            auto& pr = programs[static_cast<std::size_t>(pi)];
            if (focusedProgId == pr.id && panelVisible && !pr.minimized) {
                pr.minimized = true;
                hideDosPanel();
            } else {
                if (pr.minimized)
                    FieldAosAppJournal::recordRestore(pr.id,
                        FieldAosAppIdentity::fromOsApp(static_cast<std::uint8_t>(pr.app)),
                        pr.title);
                focusProgram(pr.id);
                panelVisible = true;
                needsProgramCanvas = true;
                Options::Canvas::DosInputFocused = true;
            }
        }
        return true;
    }
    if (hover == HitZone::Desktop) {
        const int icon = FieldAmouranthDesktop::iconAt(mx, my);
        if (icon >= 0) {
            FieldAmouranthFolderView::show(FieldAmouranthDesktop::kIconPaths[icon]);
            closeStartMenu();
            filesOpen = false;
            return true;
        }
        closeStartMenu();
        syncStartTextInput(window);
        filesOpen = false;
        FieldAmouranthFilesMenu::hover = -1;
        return true;
    }
    if (hover == HitZone::FolderView) {
        const float scale = uiScale();
        const float pw = 640.f * scale;
        const float ph = 480.f * scale;
        const float px = static_cast<float>(winW) * 0.5f - pw * 0.5f;
        const float py = static_cast<float>(winH) * 0.38f - ph * 0.5f;
        const int idx = FieldAmouranthFolderView::itemAt(mx, my, px, py, pw, ph, scale);
        if (idx >= 0)
            FieldAmouranthFolderView::activateItem(idx, FieldX86Emu::ramHost);
        return true;
    }
    if (hover == HitZone::Clock || hover == HitZone::Taskbar) {
        closeStartMenu();
        syncStartTextInput(window);
        filesOpen = false;
        FieldAmouranthFilesMenu::hover = -1;
    }
    return hover != HitZone::None;
}

inline bool onTaskbarMouseDown(SDL_Window* window, float lx, float ly) noexcept {
    if (!shellChromeActive()) return false;
    float mx = 0.f, my = 0.f;
    FieldDosChrome::chromePointerPixels(window, lx, ly, mx, my);
    if (!taskbarChromeCapturesPointer(mx, my)) return false;
    forceTaskbarChromeClick = true;
    const bool handled = onMouseDown(window, lx, ly, 1, 1);
    forceTaskbarChromeClick = false;
    return handled;
}

inline void onMouseMotion(SDL_Window* window, float lx, float ly) noexcept {
    if (!shellChromeActive()) return;
    float mx = 0.f, my = 0.f;
    FieldDosChrome::chromePointerPixels(window, lx, ly, mx, my);
    pointerMx = mx;
    pointerMy = my;
    hover = hitTest(mx, my);
    taskHoverTab = -1;
    filesBtnHover = (hover == HitZone::FilesBtn);
    terminalBtnHover = (hover == HitZone::TerminalBtn);
    monitorBtnHover = (hover == HitZone::MonitorBtn);
    browserBtnHover = (hover == HitZone::BrowserBtn);
    if (hover == HitZone::TaskBtn)
        taskHoverTab = taskBtnIndex(mx, my);
    FieldAmouranthMenu::rootHover = -1;
    FieldAmouranthMenu::flyoutHover = -1;
    FieldAmouranthSearchFlyout::hover = -1;
    FieldAmouranthFilesMenu::hover = -1;
    FieldAmouranthFolderView::hover = -1;
    FieldAmouranthDesktop::onMouseMotion(window, mx, my);
    if (FieldAmouranthFolderView::open && hover == HitZone::FolderView) {
        const float scale = uiScale();
        const float pw = 640.f * scale;
        const float ph = 480.f * scale;
        const float px = static_cast<float>(winW) * 0.5f - pw * 0.5f;
        const float py = static_cast<float>(winH) * 0.38f - ph * 0.5f;
        FieldAmouranthFolderView::hover = FieldAmouranthFolderView::itemAt(
            mx, my, px, py, pw, ph, scale);
    }
    if (filesOpen && hover == HitZone::FilesMenu) {
        FieldAmouranthFilesMenu::hover = FieldAmouranthFilesMenu::itemAt(
            mx, my, scaledFilesPanelX(), scaledFilesPanelTop(),
            scaledFilesCellW(), scaledFilesCellH(), scaledFilesPad());
    } else if (startOpen && hover == HitZone::StartMenuFlyout) {
        const float rowY0 = scaledMenuFlyoutTop() + scaledMenuPad();
        FieldAmouranthMenu::flyoutHover = FieldAmouranthMenu::rowAt(my, rowY0,
            scaledMenuRowH(), FieldAmouranthMenu::flyoutCount);
        FieldAmouranthMenu::focusPane = FieldAmouranthMenu::MenuPane::Flyout;
    } else if (startOpen && hover == HitZone::StartMenuSearch) {
        const float rowY0 = scaledSearchPanelTop()
            + FieldAmouranthSearchFlyout::pad(uiScale());
        FieldAmouranthSearchFlyout::hover = FieldAmouranthSearchFlyout::rowAt(my, rowY0,
            FieldAmouranthSearchFlyout::rowH(uiScale()),
            FieldAmouranthSearchFlyout::resultCount);
    } else if (startOpen && hover == HitZone::StartMenu) {
        const float rowY0 = scaledMenuRootRowY0();
        FieldAmouranthMenu::rootHover = FieldAmouranthMenu::rowAt(my, rowY0,
            scaledMenuRowH(), FieldAmouranthMenu::rootCount);
        FieldAmouranthMenu::focusPane = FieldAmouranthMenu::MenuPane::Root;
        if (!FieldAmouranthSearchFlyout::active()
            && FieldAmouranthMenu::rootHover >= 0
            && FieldAmouranthMenu::rootRows[FieldAmouranthMenu::rootHover].type
                == FieldAmouranthMenu::RowType::Category)
            FieldAmouranthMenu::openFlyout(
                static_cast<int>(FieldAmouranthMenu::rootRows[FieldAmouranthMenu::rootHover].cat));
    }
    if (filesDragIdx >= 0 && !FieldAmouranthDnD::active()) {
        const float dx = mx - filesDragMx0;
        const float dy = my - filesDragMy0;
        if (dx * dx + dy * dy > 64.f) {
            char path[72]{};
            std::snprintf(path, sizeof path, "FORMAT:%s",
                FieldAmouranthFilesMenu::labelFor(filesDragIdx));
            FieldAmouranthDnD::begin(path, FieldAmouranthFilesMenu::iconFor(filesDragIdx),
                0, mx, my);
        }
    }
    if (FieldAmouranthDnD::active())
        FieldAmouranthDnD::motion(mx, my);
}

inline void onMouseUp() noexcept {
    if (!shellChromeActive()) return;
    const float mx = pointerMx;
    const float my = pointerMy;
    if (FieldAmouranthDnD::active()) {
        FieldAmouranthDnD::Target drop = FieldAmouranthDnD::Target::None;
        const HitZone hz = hitTest(mx, my);
        if (hz == HitZone::Desktop)
            drop = FieldAmouranthDnD::Target::Desktop;
        else if (hz == HitZone::TaskBtn)
            drop = FieldAmouranthDnD::Target::TaskbarTab;
        else if (panelVisible)
            drop = FieldAmouranthDnD::Target::DosPanel;
        FieldAmouranthDnD::end(drop, taskHoverTab);
    }
    if (filesDragIdx >= 0) {
        const int action = FieldAmouranthFilesMenu::actionFor(filesDragIdx);
        if (action > 0)
            dispatchAction(action);
        filesDragIdx = -1;
    }
}

inline bool shellWindowFocused() noexcept {
    if (!shellChromeActive()) return true;
    if (!panelVisible) return false;
    return Options::Canvas::DosInputFocused;
}

inline bool shouldPumpGuestInput() noexcept {
    if (!panelVisible || !Options::Canvas::DosInputFocused) return false;
    if (FieldAmouranthWm::dragging || FieldAmouranthWm::resizing) return false;
    if (FieldAmouranthWm::hover != FieldAmouranthWm::ChromeHit::None
            && FieldAmouranthWm::hover != FieldAmouranthWm::ChromeHit::Content)
        return false;
    return true;
}

// Only advance emulator frames when the focused panel is visible and desktop chrome is idle.
inline bool emuAdvancesFrames(AppId app) noexcept {
    if (!panelVisible || startOpen || filesOpen) return false;
    if (focusedApp != app) return false;
    if (!shouldPumpGuestInput()) return false;
    if (FieldEmuFileDialog::active) return false;
    return true;
}

inline int focusedTabTitleIndex() noexcept {
    int tabSlot = 0;
    for (std::size_t i = 0; i < programs.size() && tabSlot < FieldAmouranthMenu::MAX_TABS; ++i) {
        if (!programs[i].running) continue;
        if (programs[i].id == focusedProgId)
            return tabSlot;
        ++tabSlot;
    }
    return -1;
}

inline void seedChromeRam(std::uint8_t* ram) noexcept {
    if (!ram || !shellChromeActive()) return;
    packChromeRam(ram);
}

inline void packChromeRam(std::uint8_t* ram) noexcept {
    if (!ram) ram = FieldX86Emu::ramHost;
    if (!ram || !shellChromeActive()) return;
    FieldAmouranthInfo::tick();
    sanitizeVgaTail(ram);
    FieldAmouranthMenu::packMenuRam(ram);
    FieldAmouranthMenu::ProgramTab tabs[FieldAmouranthMenu::MAX_TABS]{};
    int tabCount = 0;
    std::uint32_t minMask = 0u;
    int focusedIdx = -1;
    for (std::size_t i = 0; i < programs.size() && tabCount < FieldAmouranthMenu::MAX_TABS; ++i) {
        if (!programs[i].running) continue;
        tabs[tabCount].title = programs[i].title;
        tabs[tabCount].icon = programs[i].icon;
        if (programs[i].minimized)
            minMask |= 1u << tabCount;
        if (programs[i].id == focusedProgId)
            focusedIdx = tabCount;
        ++tabCount;
    }
    FieldAmouranthMenu::packTaskbarRam(ram, tabs, tabCount, focusedIdx, taskHoverTab, minMask);
    syncBrowserIconSlot();
    ram[FieldAmouranthMenu::TASKBAR_RAM + 4u] = filesBtnHover ? 1u : 0u;
    ram[FieldAmouranthMenu::TASKBAR_RAM + 5u] = terminalBtnHover ? 1u : 0u;
    ram[FieldAmouranthMenu::TASKBAR_RAM + 6u] = monitorBtnHover ? 1u : 0u;
    ram[FieldAmouranthMenu::TASKBAR_RAM + 7u] = browserBtnHover ? 1u : 0u;
    ram[FieldAmouranthMenu::TASKBAR_RAM + 8u] = browserIconSlot;
    packStartLabel(ram);
    FieldAmouranthWm::packSurfaceRam(ram);
    int idTab = 0;
    for (const auto& p : programs) {
        if (!p.running || idTab >= FieldAosAppIdentity::MAX_TABS) continue;
        FieldAosAppIdentity::packTab(ram, idTab,
            FieldAosAppIdentity::fromOsApp(static_cast<std::uint8_t>(p.app)), p.id);
        char lines[2][FieldAosAppJournal::RECENT_LEN + 1]{};
        const int n = FieldAosAppJournal::recentLinesForProg(p.id, lines, 2);
        if (n > 0)
            FieldAosAppJournal::setRecentForTab(idTab, lines[0]);
        else
            FieldAosAppJournal::setRecentForTab(idTab, p.title);
        if (n > 1)
            FieldAosAppJournal::setRecent2ForTab(idTab, lines[1]);
        const char* snap = FieldAosAppJournal::getAppSnapshot(
            FieldAosAppIdentity::fromOsApp(static_cast<std::uint8_t>(p.app)));
        if (snap && snap[0])
            FieldAosAppJournal::setSnapPreviewForTab(idTab, snap);
        ++idTab;
    }
    FieldAosAppJournal::sync(ram);
    char dateBuf[24]{};
    FieldAmouranthInfo::formatDateLine(dateBuf, sizeof dateBuf);
    FieldAmouranthMenu::packClockDateRam(ram, dateBuf);
    FieldAmouranthFilesMenu::packRam(ram);
    FieldAmouranthFolderView::packRam(ram);
    FieldAmouranthDesktop::packDesktopRam(ram);
    FieldAmouranthSearchFlyout::packRam(ram);
    FieldAmouranthDnD::packRam(ram);
}

inline void packDataBus(std::uint32_t* bus, std::uint8_t* ram = nullptr) noexcept {
    if (!bus) return;
    bus[42] &= ~(BUS_AOS_ACTIVE | (0xFu << BUS_AOS_WP_SHIFT)
               | BUS_AOS_MENU_START | BUS_AOS_MENU_FILE | BUS_AOS_MENU_SEARCH
               | BUS_AOS_INFO_PANEL | BUS_AOS_PANEL_HIDE | BUS_AOS_CONSOLE_SHELL
               | BUS_AOS_EXIT_CONFIRM);
    if (!shellChromeActive()) return;
    if (active) bus[42] |= BUS_AOS_ACTIVE;
    if (consoleShell && Options::AmouranthOs::EnableTaskbar)
        bus[42] |= BUS_AOS_CONSOLE_SHELL;
    if (startOpen) bus[42] |= BUS_AOS_MENU_START;
    if (filesOpen) bus[42] |= BUS_AOS_MENU_FILE;
    if (FieldAmouranthSearchFlyout::active()) bus[42] |= BUS_AOS_MENU_SEARCH;
    if (infoPanelVisible) bus[42] |= BUS_AOS_INFO_PANEL;
    const bool consoleStretch = consoleShell && Options::Canvas::DosPanelStretch;
    if (!panelVisible && !consoleStretch)
        bus[42] |= BUS_AOS_PANEL_HIDE;
    if (FieldAmouranthExitConfirm::isOpen())
        bus[42] |= BUS_AOS_EXIT_CONFIRM;
    if (FieldAmouranthFolderView::open)
        bus[42] |= BUS_AOS_FOLDER_VIEW;
    bus[42] |= (wallpaperIndex & 0xFu) << BUS_AOS_WP_SHIFT;
    FieldAmouranthInfo::packDataBus(bus);
    FieldAmouranthWm::packIntoBus(bus);
    FieldRtxThemes::packBus(bus);
    FieldAmouranthDesktop::packDataBus(bus);
    packChromeRam(ram);
    const int focusTab = focusedTabTitleIndex();
    const int menuHover = FieldAmouranthMenu::rootHover >= 0
        ? FieldAmouranthMenu::rootHover : FieldAmouranthMenu::rootFocus;
    bus[54] = ((menuHover >= 0
                    ? static_cast<std::uint32_t>(menuHover)
                    : BUS_CHROME_NONE)
                << BUS_CHROME_MENU_HOVER_SHIFT)
            | ((taskHoverTab >= 0
                    ? static_cast<std::uint32_t>(taskHoverTab)
                    : BUS_CHROME_NONE)
                << BUS_CHROME_TASK_HOVER_SHIFT)
            | ((static_cast<std::uint32_t>(FieldAmouranthMenu::rootCount) & 0xFFu)
                << BUS_CHROME_MENU_ROWS_SHIFT)
            | ((focusTab >= 0
                    ? static_cast<std::uint32_t>(focusTab)
                    : BUS_CHROME_NONE)
                << BUS_CHROME_FOCUS_TITLE_SHIFT);
}

inline void prepareExitConfirmUi() noexcept {
    startOpen = false;
    filesOpen = false;
    // Chrome-layer exit modal only — do not raise DOS panel (that blocks the UI thread on x86 hotswap).
}

inline void exitConfirmPanelBounds(float& x0, float& y0, float& w, float& h) noexcept {
    const float scale = uiScale();
    w = 420.f * scale;
    h = 200.f * scale;
    const float vw = chromeViewportW();
    const float vh = chromeViewportH();
    x0 = (vw - w) * 0.5f;
    y0 = (vh - scaledTaskbarH() - h) * 0.45f;
}

inline int exitConfirmButtonAt(float mx, float my) noexcept {
    if (!FieldAmouranthExitConfirm::isOpen()) return 0;
    float x0 = 0.f, y0 = 0.f, w = 0.f, h = 0.f;
    exitConfirmPanelBounds(x0, y0, w, h);
    const float scale = uiScale();
    const float btnW = 148.f * scale;
    const float btnH = 40.f * scale;
    const float btnY = y0 + h - btnH - 24.f * scale;
    const float mid = x0 + w * 0.5f;
    const float gap = 12.f * scale;
    if (pointIn(mx, my, mid - gap - btnW, btnY, btnW, btnH)) return 1;
    if (pointIn(mx, my, mid + gap, btnY, btnW, btnH)) return 2;
    return 0;
}

inline void applyShutdownState() noexcept {
    clearStaleGuestFlags();
    startOpen = false;
    FieldAmouranthMenu::closeMenuFocus();
    filesOpen = false;
    FieldAmouranthFilesMenu::hover = -1;
    consoleShell = false;
    persistOpenSession();
    programs.clear();
    focusedProgId = 0;
    focusedApp = AppId::None;
    panelVisible = false;
    hideDosPanel();
}

} // namespace FieldAmouranthOs