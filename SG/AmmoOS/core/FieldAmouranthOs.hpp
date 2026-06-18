#pragma once

// AmouranthOS — DevKit taskbar shell: Start, programs, clock. Panels launch on demand.
// Default desktop shows version/quality info; DOS panel hidden until a program opens.

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
inline bool infoPanelVisible = true;
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
    default:              return "DOS console";
    }
}

inline bool guestAppRunning() noexcept {
    return FieldAmmoCode::active || FieldAmouranthFileCmd::active
        || FieldPadTest::active || FieldNes::active || FieldAmmoNesSetup::active
        || FieldAmmoBrowser::isActive()
        || FieldRtxBasic::active
        || FieldAmmoText::active || FieldMonacoEdit::active || FieldRtxThemePicker::active
        || FieldAmmoExec::isActive();
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

inline void hideDosPanel() noexcept {
    panelVisible = false;
    FieldDosViewport::panelOx = -8192.f;
    FieldDosViewport::panelOy = -8192.f;
    FieldDosViewport::panelPositioned = true;
    if (!active) {
        if (consoleShell) {
            FieldDosViewport::panelStretch = true;
            Options::Canvas::DosPanelStretch = true;
            Options::Canvas::ControlFlags |= Options::Canvas::ControlDosPanelStretch;
        } else {
            FieldDosViewport::panelStretch = false;
            Options::Canvas::DosPanelStretch = false;
            Options::Canvas::ControlFlags &= ~Options::Canvas::ControlDosPanelStretch;
        }
        infoPanelVisible = true;
        FieldAmouranthInfo::visible = true;
    } else if (!panelVisible) {
        infoPanelVisible = true;
        FieldAmouranthInfo::visible = true;
    } else {
        infoPanelVisible = false;
        FieldAmouranthInfo::visible = false;
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
    FieldDosViewport::panelOx = pr.panelOx;
    FieldDosViewport::panelOy = pr.panelOy;
    FieldDosViewport::panelPositioned = true;
    if (pr.panelScale > 0.f) {
        FieldAmouranthWm::panelScale = pr.panelScale;
        FieldAmouranthWm::applyPanelScale();
    }
    FieldDosViewport::clampPanelPosition();
}

inline FieldAmouranthLaunch::GuiApp guiAppFor(AppId app) noexcept;

inline void placeNewWindow(Program& pr) noexcept {
    const float sw = FieldDosViewport::winW > 0 ? FieldDosViewport::winW : 1920.f;
    const float sh = FieldDosViewport::winH > 0 ? FieldDosViewport::winH : 1080.f;
    const float pw = FieldDosViewport::panelOuterW();
    const float ph = FieldDosViewport::panelOuterH();
    const float deskTop = desktopTopInset();
    const float deskH = std::max(1.f, sh - deskTop - scaledTaskbarH());
    const float baseOx = (sw - pw) * 0.5f;
    const float baseOy = deskTop + (deskH - ph) * 0.5f;
    std::size_t prior = 0;
    for (const auto& p : programs)
        if (p.id != pr.id && p.running) ++prior;
    pr.panelOx = baseOx + cascadeOffset(prior);
    pr.panelOy = baseOy + cascadeOffset(prior);
    if (pr.panelScale < 0.f)
        pr.panelScale = FieldAmouranthWm::panelScale;
}

inline void clearStaleGuestFlags() noexcept {
    FieldAmmoText::active = false;
    FieldRtxThemePicker::close();
    FieldRtxBasic::active = false;
    FieldAmmoCode::active = false;
    FieldPadTest::active = false;

    FieldAmmoBrowser::close();
    FieldAmouranthFileCmd::close();
    FieldRtxShell::graphicsActive = false;
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
            applyProgramPanel(p);
            break;
        }
    }
    if (prevId != progId)
        clearStaleGuestFlags();
    FieldAmouranthWm::openMenu = FieldAmouranthWm::OpenMenu::None;
    FieldAmouranthWm::menuItemHover = -1;
    FieldAmouranthWm::raiseFocusedProgram();
    if (restoreContent && focusedApp != AppId::None)
        FieldAmouranthLaunch::queueGui(guiAppFor(focusedApp));
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
    placeNewWindow(programs.back());
    focusProgram(programs.back().id, false);
    applyProgramPanel(programs.back());
    panelVisible = true;
    infoPanelVisible = false;
    FieldAmouranthInfo::visible = false;
    Options::Canvas::DosInputFocused = true;
    FieldAmouranthWm::raiseFocusedProgram();
    return programs.back();
}

inline float desktopTopInset() noexcept {
    return active ? FieldAosStatusBar::height() * uiScale() : 0.f;
}

inline void showDosPanelCentered() noexcept {
    if (focusedProgId > 0) {
        if (Program* p = findProgram(focusedProgId)) {
            if (p->panelOx < 0.f) placeNewWindow(*p);
            applyProgramPanel(*p);
        }
    } else {
        const float sw = FieldDosViewport::winW > 0 ? FieldDosViewport::winW : 1920.f;
        const float sh = FieldDosViewport::winH > 0 ? FieldDosViewport::winH : 1080.f;
        const float pw = FieldDosViewport::panelOuterW();
        const float ph = FieldDosViewport::panelOuterH();
        const float deskTop = desktopTopInset();
        const float deskH = std::max(1.f, sh - deskTop - scaledTaskbarH());
        FieldDosViewport::panelOx = (sw - pw) * 0.5f;
        FieldDosViewport::panelOy = deskTop + (deskH - ph) * 0.5f;
        FieldDosViewport::panelPositioned = true;
        FieldDosViewport::clampPanelPosition();
    }
    FieldDosViewport::panelStretch = false;
    Options::Canvas::DosPanelStretch = false;
    Options::Canvas::ControlFlags &= ~Options::Canvas::ControlDosPanelStretch;
    FieldAmouranthWm::applyPanelScale();
    panelVisible = true;
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
    if (closedId > 0)
        removeProgramById(closedId);
    else if (!programs.empty())
        programs.pop_back();
    focusedProgId = 0;
    focusedApp = AppId::None;
    if (!programs.empty())
        focusProgram(programs.back().id);
}

inline void markFocusedMinimized() noexcept {
    if (Program* p = findProgram(focusedProgId))
        p->minimized = true;
}

inline void syncDesktopState() noexcept {
    if (qaHoldInfoDesktop) return;
    if (!active && !consoleShell) return;
    if (guestAppRunning() && panelVisible) return;
    if (focusedProgId > 0 && panelVisible) return;
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

inline bool init(SDL_Window* /*window*/) noexcept { return true; }

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

// Taskbar + Start menu only — no wallpaper desktop (x86.comp default path).
inline void bootShell() noexcept {
    active = false;
    consoleShell = true;
    startOpen = false;
    programs.clear();
    nextProgId = 1;
    focusedApp = AppId::None;
    focusedProgId = 0;
    Options::AmouranthOs::EnableDesktop = false;
    Options::AmouranthOs::EnableTaskbar = true;
    Options::SDL3::StartFullscreen = true;
    Options::SDL3::PendingFullscreenAfterLoad = true;
    FieldDosViewport::panelStretch = true;
    Options::Canvas::DosPanelStretch = true;
    Options::Canvas::ControlFlags |= Options::Canvas::ControlDosPanelStretch;
    infoPanelVisible = false;
    FieldAmouranthInfo::visible = false;
    panelVisible = false;
    FieldAmouranthWm::resetScale();
    FieldDosViewport::crispFont = true;
    FieldDosViewport::subpixelFont = false;
    FieldDosViewport::sharpen = 0.72f;
    FieldDosViewport::fontScale = 1.35f;
    FieldRtxThemes::applyIndex(3);
    hideDosPanel();
    FieldAmouranthMenu::rebuildVisible();
    std::fprintf(stderr, "[AMOURANTHOS] x86 shell — Start menu | folder | no popups\n");
}

inline void boot() noexcept {
    active = true;
    consoleShell = false;
    startOpen = false;
    programs.clear();
    nextProgId = 1;
    focusedApp = AppId::None;
    focusedProgId = 0;
    Options::AmouranthOs::EnableDesktop = true;
    Options::AmouranthOs::EnableTaskbar = true;
    Options::SDL3::StartFullscreen = true;
    Options::SDL3::PendingFullscreenAfterLoad = true;
    FieldDosViewport::panelStretch = false;
    Options::Canvas::DosPanelStretch = false;
    Options::Canvas::ControlFlags &= ~Options::Canvas::ControlDosPanelStretch;
    infoPanelVisible = true;
    FieldAmouranthInfo::visible = true;
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
    FieldDosViewport::scanlineMix = 0.04f;
    FieldDosViewport::panelGlow = 0.08f;
    FieldRuntimeInfo::refresh();
    FieldAmouranthInfo::tick();
    FieldAmouranthMenu::rebuildVisible();
    std::fprintf(stderr, "[AMOURANTHOS] RTX WM desktop — Start menu categories | Exit → Diagnostics\n");
}

inline void requestGracefulShutdown() noexcept {
    startOpen = false;
    FieldAmouranthMenu::closeMenuFocus();
    FieldAmouranthFileCmd::close();
    programs.clear();
    focusedProgId = 0;
    focusedApp = AppId::None;
    panelVisible = false;
    hideDosPanel();
    Options::SDL3::RequestQuit = true;
    std::fprintf(stderr, "[AMOURANTHOS] Graceful shut down requested from Start menu\n");
}

inline void deactivate() noexcept {
    active = false;
    consoleShell = true;
    startOpen = false;
    infoPanelVisible = true;
    focusedApp = AppId::None;
    focusedProgId = 0;
    programs.clear();
    Options::AmouranthOs::EnableDesktop = false;
    Options::AmouranthOs::EnableTaskbar = true;
    FieldAmouranthFileCmd::close();
    FieldDosViewport::panelOx = 0.f;
    FieldDosViewport::panelOy = 0.f;
    FieldDosViewport::panelPositioned = false;
    FieldDosViewport::panelStretch = true;
    Options::Canvas::DosPanelStretch = true;
    Options::Canvas::ControlFlags |= Options::Canvas::ControlDosPanelStretch;
    FieldAmouranthInfo::visible = true;
    pendingShellRestore = true;
    hideDosPanel();
    FieldDosViewport::panelStretch = true;
    Options::Canvas::DosPanelStretch = true;
    Options::Canvas::ControlFlags |= Options::Canvas::ControlDosPanelStretch;
    FieldAmouranthWm::applyPanelScale();
    std::fprintf(stderr, "[AMOURANTHOS] Exit to Diagnostics — console backdrop + Start menu\n");
}

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
    winW = w;
    winH = h;
    FieldAmouranthInfo::tick();
    syncDesktopState();
}

inline bool pointIn(float px, float py, float x, float y, float fw, float fh) noexcept {
    return px >= x && py >= y && px < x + fw && py < y + fh;
}

inline float uiScale() noexcept {
    const float base = winW > 0 ? static_cast<float>(winW) / 1920.f : 1.f;
    return std::max(base * UI_BOOST, 0.85f);
}

inline float scaledTaskbarH() noexcept { return TASKBAR_H * uiScale(); }
inline float scaledStartW() noexcept { return START_W * uiScale(); }
inline float scaledFolderBtnW() noexcept { return FOLDER_BTN_W * uiScale(); }
inline float taskTabsOriginX() noexcept {
    return 6.f * uiScale() + scaledStartW() + 4.f * uiScale() + scaledFolderBtnW() + 16.f * uiScale();
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
    if (FieldAmouranthMenu::flyoutOpen())
        w += scaledMenuFlyoutGap() + scaledMenuFlyoutW();
    return w;
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

inline float taskbarY() noexcept { return static_cast<float>(winH) - scaledTaskbarH(); }

// Match aosStartButtonBounds / aosFolderButtonBounds in x86.comp (pad + lift).
inline void taskbarChromeButtonY(float& y0, float& y1) noexcept {
    const float pad = 6.f * uiScale();
    const float lift = 10.f * uiScale();
    const float ty = taskbarY();
    y0 = ty + pad - lift;
    y1 = static_cast<float>(winH) - pad;
}

inline float scaledMenuRootTop() noexcept {
    return taskbarY() - scaledMenuRootHeight();
}

inline float scaledMenuFlyoutTop() noexcept {
    return taskbarY() - scaledMenuFlyoutHeight();
}

inline HitZone hitTest(float mx, float my) noexcept {
    if (my < scaledTopBarH()) return HitZone::None;
    const float ty = taskbarY();
    const float th = scaledTaskbarH();
    const float sw = scaledStartW();
    if (my >= ty) {
        float btnY0 = 0.f, btnY1 = 0.f;
        taskbarChromeButtonY(btnY0, btnY1);
        const float btnH = btnY1 - btnY0;
        if (pointIn(mx, my, 6.f * uiScale(), btnY0, sw, btnH))
            return HitZone::StartBtn;
        const float folderX = 6.f * uiScale() + sw + 4.f * uiScale();
        const float folderW = scaledFolderBtnW();
        if (pointIn(mx, my, folderX, btnY0, folderW, btnH))
            return HitZone::FilesBtn;
        float tx = taskTabsOriginX();
        for (const auto& p : programs) {
            if (p.running && pointIn(mx, my, tx, ty + 7.f * uiScale(),
                    scaledTabW(), th - 14.f * uiScale()))
                return HitZone::TaskBtn;
            tx += scaledTabW() + 6.f * uiScale();
        }
        if (pointIn(mx, my, static_cast<float>(winW) - scaledClockW() - 8.f * uiScale(),
                ty + 5.f * uiScale(), scaledClockW(), th - 10.f * uiScale()))
            return HitZone::Clock;
        return HitZone::Taskbar;
    }
    if (startOpen) {
        const float rootSy = scaledMenuRootTop();
        const float rootH = scaledMenuRootHeight();
        if (FieldAmouranthMenu::flyoutOpen()) {
            const float flySy = scaledMenuFlyoutTop();
            const float flyH = scaledMenuFlyoutHeight();
            const float fx = scaledMenuPad() + scaledMenuW() + scaledMenuFlyoutGap();
            if (pointIn(mx, my, fx, flySy, scaledMenuFlyoutW(), flyH))
                return HitZone::StartMenuFlyout;
        }
        if (pointIn(mx, my, scaledMenuPad(), rootSy, scaledMenuW(), rootH))
            return HitZone::StartMenu;
    }
    return HitZone::Desktop;
}

inline int taskBtnIndex(float mx, float my) noexcept {
    const float ty = taskbarY();
    const float th = scaledTaskbarH();
    float tx = taskTabsOriginX();
    for (std::size_t i = 0; i < programs.size(); ++i) {
        if (programs[i].running && pointIn(mx, my, tx, ty + 7.f * uiScale(),
                scaledTabW(), th - 14.f * uiScale()))
            return static_cast<int>(i);
        tx += scaledTabW() + 6.f * uiScale();
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
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::Nes);
        break;
    case 30:
        openNewWindow(AppId::NesSetup);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::NesSetup);
        break;
    case 31:
        openNewWindow(AppId::NesSetup);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::NesSetup, true);
        break;

    case 11:
        openNewWindow(AppId::FileCmd);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::FileCmd);
        break;
    case 12:
        launchVscodium();
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
    case 99:
        requestGracefulShutdown();
        break;
    default: break;
    }
    startOpen = false;
    FieldAmouranthMenu::closeMenuFocus();
    syncDesktopState();
}

inline bool onKeyDown(SDL_Scancode sc) noexcept {
    if (!shellChromeActive() || !startOpen) return false;

    std::uint16_t key = 0;
    switch (sc) {
    case SDL_SCANCODE_ESCAPE: key = 0x011Bu; break;
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

    if (sc == SDL_SCANCODE_ESCAPE) {
        startOpen = false;
        FieldAmouranthMenu::closeMenuFocus();
        return true;
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
    FieldDosChrome::pointerPixels(window, lx, ly, mx, my);
    if (panelVisible || consoleShell) {
        const auto wh = FieldAmouranthWm::hitTest(mx, my);
        if (wh != FieldAmouranthWm::ChromeHit::None
                && wh != FieldAmouranthWm::ChromeHit::Content)
            return false;
    }
    hover = hitTest(mx, my);

    if (hover == HitZone::StartBtn) {
        startOpen = !startOpen;
        if (startOpen) FieldAmouranthMenu::rebuildVisible();
        else FieldAmouranthMenu::closeMenuFocus();
        return true;
    }
    if (hover == HitZone::FilesBtn) {
        startOpen = false;
        FieldAmouranthMenu::closeMenuFocus();
        openNewWindow(AppId::FileCmd);
        FieldAmouranthLaunch::queueGui(FieldAmouranthLaunch::GuiApp::FileCmd);
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
            const float rowY0 = scaledMenuRootTop() + scaledMenuPad() + scaledMenuHeaderH();
            const int idx = FieldAmouranthMenu::rowAt(my, rowY0, scaledMenuRowH(),
                FieldAmouranthMenu::rootCount);
            const int action = FieldAmouranthMenu::actionForRootRow(idx);
            if (action > 0)
                dispatchAction(action);
        }
        return true;
    }
    if (hover == HitZone::TaskBtn) {
        const int ti = taskBtnIndex(mx, my);
        if (ti >= 0 && ti < static_cast<int>(programs.size())) {
            auto& pr = programs[static_cast<std::size_t>(ti)];
            if (focusedProgId == pr.id && panelVisible && !pr.minimized) {
                pr.minimized = true;
                hideDosPanel();
            } else {
                focusProgram(pr.id);
                showDosPanelCentered();
            }
        }
        return true;
    }
    if (hover == HitZone::Desktop) {
        startOpen = false;
        FieldAmouranthMenu::closeMenuFocus();
        return true;
    }
    if (hover == HitZone::Clock || hover == HitZone::Taskbar) {
        startOpen = false;
        FieldAmouranthMenu::closeMenuFocus();
    }
    return hover != HitZone::None;
}

inline void onMouseMotion(SDL_Window* window, float lx, float ly) noexcept {
    if (!shellChromeActive()) return;
    float mx = 0.f, my = 0.f;
    FieldDosChrome::pointerPixels(window, lx, ly, mx, my);
    hover = hitTest(mx, my);
    taskHoverTab = -1;
    filesBtnHover = (hover == HitZone::FilesBtn);
    if (hover == HitZone::TaskBtn)
        taskHoverTab = taskBtnIndex(mx, my);
    FieldAmouranthMenu::rootHover = -1;
    FieldAmouranthMenu::flyoutHover = -1;
    if (startOpen && hover == HitZone::StartMenuFlyout) {
        const float rowY0 = scaledMenuFlyoutTop() + scaledMenuPad();
        FieldAmouranthMenu::flyoutHover = FieldAmouranthMenu::rowAt(my, rowY0,
            scaledMenuRowH(), FieldAmouranthMenu::flyoutCount);
        FieldAmouranthMenu::focusPane = FieldAmouranthMenu::MenuPane::Flyout;
    } else if (startOpen && hover == HitZone::StartMenu) {
        const float rowY0 = scaledMenuRootTop() + scaledMenuPad() + scaledMenuHeaderH();
        FieldAmouranthMenu::rootHover = FieldAmouranthMenu::rowAt(my, rowY0,
            scaledMenuRowH(), FieldAmouranthMenu::rootCount);
        FieldAmouranthMenu::focusPane = FieldAmouranthMenu::MenuPane::Root;
        if (FieldAmouranthMenu::rootHover >= 0
            && FieldAmouranthMenu::rootRows[FieldAmouranthMenu::rootHover].type
                == FieldAmouranthMenu::RowType::Category)
            FieldAmouranthMenu::openFlyout(
                static_cast<int>(FieldAmouranthMenu::rootRows[FieldAmouranthMenu::rootHover].cat));
    }
}

inline void onMouseUp() noexcept {}

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

inline void packChromeRam(std::uint8_t* ram) noexcept {
    if (!ram) ram = FieldX86Emu::ramHost;
    if (!ram || !shellChromeActive()) return;
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
    ram[FieldAmouranthMenu::TASKBAR_RAM + 4u] = filesBtnHover ? 1u : 0u;
    packStartLabel(ram);
    FieldAmouranthWm::packSurfaceRam(ram);
    char dateBuf[24]{};
    FieldAmouranthInfo::formatDateLine(dateBuf, sizeof dateBuf);
    FieldAmouranthMenu::packClockDateRam(ram, dateBuf);
}

inline void packDataBus(std::uint32_t* bus, std::uint8_t* ram = nullptr) noexcept {
    if (!bus) return;
    bus[42] &= ~(BUS_AOS_ACTIVE | (0xFu << BUS_AOS_WP_SHIFT)
               | BUS_AOS_MENU_START | BUS_AOS_MENU_FILE | BUS_AOS_INFO_PANEL | BUS_AOS_PANEL_HIDE
               | BUS_AOS_CONSOLE_SHELL);
    if (!shellChromeActive()) return;
    if (active) bus[42] |= BUS_AOS_ACTIVE;
    if (consoleShell && Options::AmouranthOs::EnableTaskbar)
        bus[42] |= BUS_AOS_CONSOLE_SHELL;
    if (startOpen) bus[42] |= BUS_AOS_MENU_START;
    if (FieldAmouranthFileCmd::active) bus[42] |= BUS_AOS_MENU_FILE;
    if (infoPanelVisible) bus[42] |= BUS_AOS_INFO_PANEL;
    const bool consoleStretch = consoleShell && Options::Canvas::DosPanelStretch;
    if (!panelVisible && !consoleStretch)
        bus[42] |= BUS_AOS_PANEL_HIDE;
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

} // namespace FieldAmouranthOs