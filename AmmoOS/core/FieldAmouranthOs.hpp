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

namespace FieldAmouranthOs {

constexpr float TASKBAR_H   = 56.f;
constexpr float START_W     = 156.f;
constexpr float FOLDER_BTN_W = 44.f;
constexpr float TAB_W       = 148.f;
constexpr float CLOCK_W     = 200.f;
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
inline int  winW = 1920, winH = 1080;
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
inline bool consoleShell = false;
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

#include "FieldWmDock.hpp"
#include "FieldWmShell.hpp"

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
            FieldAosAppIdentity::fromOsApp(static_cast<std::uint8_t>(p.app)),
            p.title);
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
            FieldAosAppIdentity::fromOsApp(static_cast<std::uint8_t>(p.app)),
            p.title);
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

// Match aosUiScale() in x86.comp — max(w/1920, 0.75) * 1.35.
inline float chromeLayoutW() noexcept {
    if (FieldDosChrome::chromeUsesRenderSpace()) return FieldDosViewport::renderW;
    if (FieldDosViewport::winW > 0.f) return FieldDosViewport::winW;
    return static_cast<float>(winW > 0 ? winW : 1920);
}

inline float chromeLayoutH() noexcept {
    if (FieldDosChrome::chromeUsesRenderSpace()) return FieldDosViewport::renderH;
    if (winH > 0) return static_cast<float>(winH);
    if (FieldDosViewport::winH > 0.f) return FieldDosViewport::winH;
    return 1080.f;
}

// Match shader aosViewport() when chrome is live-synced; else AmouranthOS win metrics.
inline float chromeViewportW() noexcept {
    if (FieldDosChrome::chromeUsesRenderSpace() && FieldDosViewport::renderW > 1.f)
        return FieldDosViewport::renderW;
    if (winW > 0) return static_cast<float>(winW);
    if (FieldDosViewport::winW > 1.f) return FieldDosViewport::winW;
    return 1920.f;
}

inline float chromeViewportH() noexcept {
    if (FieldDosChrome::chromeUsesRenderSpace() && FieldDosViewport::renderH > 1.f)
        return FieldDosViewport::renderH;
    if (winH > 0) return static_cast<float>(winH);
    if (FieldDosViewport::winH > 1.f) return FieldDosViewport::winH;
    return 1080.f;
}

inline float uiScale() noexcept {
    const float base = chromeViewportW() / 1920.f;
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
