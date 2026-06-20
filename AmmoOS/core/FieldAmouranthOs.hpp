#pragma once

// AmouranthOS — RTX desktop shell: Start menu launches programs on demand.
// Black desktop backdrop at boot; windows open empty then load content.

#include "FieldAosStatusBar.hpp"
#include "FieldAmouranthFileCmd.hpp"
#include "FieldAmouranthInfo.hpp"
#include "FieldAmouranthLaunch.hpp"
#include "FieldDrives.hpp"
#include "FieldAmouranthHudRam.hpp"
#include "FieldAmouranthMenu.hpp"
#include "FieldAmouranthTextures.hpp"
#include "FieldX86Emu.hpp"
#include "FieldAmmoCode.hpp"
#include "FieldDosChrome.hpp"
#include "FieldDosViewport.hpp"

#include "FieldAmmoNes.hpp"
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
#include "OptionsMenu.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace FieldAmmoExec { bool isActive() noexcept; }

#include "FieldAmmoBrowser.hpp"

namespace FieldAmouranthOs {

constexpr float TASKBAR_H   = 52.f;
constexpr float START_W     = 156.f;
constexpr float FOLDER_BTN_W = 44.f;
constexpr float TAB_W       = 148.f;
constexpr float CLOCK_W     = 200.f;
constexpr float MENU_ROW_H  = 34.f;
constexpr float MENU_HEADER_H = 28.f;
constexpr float MENU_PAD    = 6.f;
constexpr float MENU_W      = 196.f;
constexpr float MENU_FLYOUT_W = 268.f;
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
    None = 0, Shell, AmmoCode, QBasic, FieldC, PadTest, Nes, NesSetup, Browser, Vscodium, FileCmd, Doom
};

enum class HitZone : std::uint8_t {
    None = 0, Desktop, Taskbar, StartBtn, FilesBtn, TaskBtn, Clock, StartMenu, StartMenuFlyout
};

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
};

inline bool active = false;
inline bool qaHoldInfoDesktop = false;
inline bool startOpen = false;
inline bool panelVisible = false;
inline bool infoPanelVisible = false;
inline bool pendingEmptyPanel = false;
inline int  nextProgId = 1;
inline AppId focusedApp = AppId::None;
inline int  focusedProgId = 0;
inline int  winW = 1920, winH = 1080;
inline HitZone hover = HitZone::None;
inline int taskHoverTab = -1;
inline bool filesBtnHover = false;
inline bool pendingShellRestore = false;
inline bool consoleShell = false;  // diagnostics console; desktop boots active from startup

inline std::vector<Program> programs;

} // namespace FieldAmouranthOs

#include "FieldAmouranthWm.hpp"
#include "FieldAmouranthDesktop.hpp"

namespace FieldAmouranthOs {

inline void deactivate() noexcept;

// PLACEHOLDER_TRUNCATED_FOR_SIZE
