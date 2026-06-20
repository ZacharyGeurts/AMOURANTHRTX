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

namespace FieldAmouranthOs {

inline void deactivate() noexcept;
inline void requestGracefulShutdown(std::uint8_t* ram = nullptr) noexcept;

// PLACEHOLDER_CONTINUE