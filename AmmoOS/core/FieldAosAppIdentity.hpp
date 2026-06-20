#pragma once

// Per-app identity copy — taglines packed for GPU background windows + RTX widget headers.

#include "FieldAmouranthHudRam.hpp"
#include "FieldRtxWidgets.hpp"

#include <cstdint>
#include <cstring>

namespace FieldAosAppIdentity {

// Mirrors FieldAmouranthOs::AppId values — kept local to avoid circular includes.
enum class AppId : std::uint8_t {
    None = 0, Shell = 1, AmmoCode = 2, QBasic = 3, FieldC = 4, PadTest = 5,
    Nes = 6, NesSetup = 7, Browser = 8, Vscodium = 9, FileCmd = 10, Doom = 11, Monitor = 12,
    A2600 = 13, A2600Setup = 14, Sms = 15, SmsSetup = 16, Genesis = 17, GenesisSetup = 18,
    Snes = 19, SnesSetup = 20
};

constexpr std::uint32_t IDENTITY_RAM   = 0x000BB800u;
constexpr int           MAX_TABS       = 8;
constexpr int           TAB_STRIDE     = 160;
constexpr int           TAGLINE_LEN    = 44;
constexpr int           ABOUT_LEN      = 48;
constexpr int           RECENT_LEN     = 32;
constexpr int           APP_ID_OFF     = TAGLINE_LEN + ABOUT_LEN;
constexpr int           PROG_ID_OFF    = APP_ID_OFF + 1;
constexpr int           RECENT1_OFF    = PROG_ID_OFF + 1;
constexpr int           RECENT2_OFF    = RECENT1_OFF + RECENT_LEN;
constexpr int           SNAP_OFF       = RECENT2_OFF + RECENT_LEN;
constexpr int           SNAP_PREVIEW   = 24;

struct Copy {
    const char* tagline;
    const char* about;
    const char* about2;
    const char* footer;
};

inline Copy copyFor(AppId app) noexcept {
    switch (app) {
    case AppId::Shell:
        return {
            "Golden Era command center for the Field Die",
            "Pick a program below or open the DOS shell for commands.",
            "Taskbar Terminal = C:\\>  |  Start menu has the full catalog.",
            "Launch selected opens app | DOS Console = RTX-DOS shell"
        };
    case AppId::AmmoCode:
        return {
            "Turbo Pascal-style IDE with minimap and field layers",
            "Edit ASM and BASIC on C:\\AMMOCODE with syntax colors.",
            "F5 run  F6 step  minimap scroll  Esc closes the window.",
            "Shell: AMMOCODE /HELP  |  QBASIC /RUN file.bas"
        };
    case AppId::QBasic:
        return {
            "Classic QBASIC interpreter inside AmouranthOS",
            "Write and run BASIC lessons on the emulated C: drive.",
            "Type RUN to execute  |  shell QBASIC /HELP for syntax.",
            "Esc closes window  |  lessons live in C:\\BASIC\\"
        };
    case AppId::FieldC:
        return {
            "Field Compiler v4 — .fld to AMMO object files",
            "Compile field-language sources for PADTEST and tools.",
            "Syntax: print return field era any  |  sample on screen.",
            "Shell: FIELDC /?  |  BUILD compiles .ASM to .COM"
        };
    case AppId::PadTest:
        return {
            "Live gamepad lab — Xbox 360 / SDL controllers",
            "Press buttons and move sticks to verify host input routing.",
            "Toggle raw axis view for calibration  |  Rescan finds pads.",
            "Plug controller before opening  |  Esc closes window"
        };
    case AppId::Nes:
        return {
            "AmmoNES — cycle-accurate NES on the RTX die",
            "Load .nes ROMs from C:\\NES  |  NES IMPORT from shell.",
            "Arrows + Z/X play  |  P pause  |  R reset  |  Esc quit.",
            "AmmoNES Setup configures keys and audio quality"
        };
    case AppId::NesSetup:
        return {
            "AmmoNES configuration — video, audio, and pads",
            "Set region, sprite limits, sound quality, and key binds.",
            "F2 saves settings  |  F3 restores factory defaults.",
            "Tab switches pages  |  Esc closes window"
        };
    case AppId::Browser:
        return {
            "Field Web — hooks your OS browser into this panel",
            "Detects Firefox, Chrome, or Edge on the Linux host.",
            "Click Refresh, then Open — nothing auto-launches.",
            "Shell: BROWSER url  |  Open Amouranth.com button"
        };
    case AppId::FileCmd:
        return {
            "AmmoFiles — dual-pane commander for C:\\ and E:\\HOST",
            "Browse guest FAT and host bridge  |  click row to select.",
            "Run F4 opens by extension  |  Switch pane toggles C: / E:.",
            "Copy across panes via context menu  |  Esc closes"
        };
    case AppId::Doom:
        return {
            "Shareware DOOM on the GPU x86 die",
            "Chocolate Doom path inside the emulated filesystem.",
            "Full-screen game viewport with DOS HUD chrome.",
            "Esc closes window when you exit the game"
        };
    case AppId::Monitor:
        return {
            "Host runtime dashboard — GPU, Vulkan, and quality flags",
            "Live renderer state, viewport scale, and AmouranthOS opts.",
            "Checkboxes mirror Options menu quality toggles.",
            "Refresh button updates every frame  |  Esc closes"
        };
    case AppId::A2600:
        return {
            "AmmoA2600 — Atari VCS on the RTX die",
            "Load .bin/.a26 ROMs from C:\\A2600\\",
            "TIA graphics + Classic/Modern audio  |  Esc quit",
            "AmmoA2600 Setup configures audio style"
        };
    case AppId::A2600Setup:
        return {
            "AmmoA2600 configuration — TIA audio options",
            "Classic vs Modern TIA tone output.",
            "F2 saves  F3 defaults  Tab switches pages",
            "Esc closes window"
        };
    case AppId::Sms:
        return {
            "AmmoSMS — Sega Master System emulator",
            "Load .sms ROMs from C:\\SMS\\",
            "SN76489 PSG — Classic or Modern mix  |  Esc quit",
            "AmmoSMS Setup for audio and paths"
        };
    case AppId::SmsSetup:
        return {
            "AmmoSMS configuration — audio and system",
            "Classic vs Modern SN76489 output.",
            "F2 saves  F3 defaults  Tab switches pages",
            "Esc closes window"
        };
    case AppId::Genesis:
        return {
            "AmmoGenesis — Mega Drive / Genesis emulator",
            "Load .md/.bin ROMs from C:\\GENESIS\\",
            "YM2612 + SN76489 dual audio  |  Esc quit",
            "AmmoGenesis Setup for FM/PSG options"
        };
    case AppId::GenesisSetup:
        return {
            "AmmoGenesis configuration — FM + PSG audio",
            "Classic vs Modern on YM2612 and SN76489.",
            "F2 saves  F3 defaults  Tab switches pages",
            "Esc closes window"
        };
    case AppId::Snes:
        return {
            "AmmoSNES — Super Nintendo on the RTX die",
            "Load .sfc/.smc ROMs from C:\\SNES\\",
            "Super FX (GSU) thermo-governed RISC coprocessor",
            "P pause  Esc quit  |  AmmoSNES Setup for GSU options"
        };
    case AppId::SnesSetup:
        return {
            "AmmoSNES configuration — SuperFX + audio",
            "Thermo governor batches GSU plot/rpix steps per frame.",
            "F2 saves  F3 defaults  Tab switches pages",
            "Esc closes window"
        };
    default:
        return {
            "AmouranthOS RTX application",
            "Dark-theme GPU widgets rendered on the Field Die.",
            "Use the taskbar to switch between open programs.",
            "Esc closes window  |  Start menu for more apps"
        };
    }
}

inline AppId fromOsApp(std::uint8_t osApp) noexcept {
    return static_cast<AppId>(osApp);
}

inline void writeRamStr(std::uint8_t* ram, std::uint32_t off, const char* s, int maxLen) noexcept {
    if (!ram) return;
    int j = 0;
    for (int i = 0; s && s[i] && j < maxLen; ++i) {
        const unsigned char ch = static_cast<unsigned char>(s[i]);
        if (ch < 0x20u || ch >= 0x80u) continue;
        ram[off + static_cast<std::uint32_t>(j++)] = ch;
    }
    while (j < maxLen)
        ram[off + static_cast<std::uint32_t>(j++)] = static_cast<std::uint8_t>(' ');
}

inline void packTab(std::uint8_t* ram, int tab, AppId app, int progId = 0) noexcept {
    if (!ram || tab < 0 || tab >= MAX_TABS) return;
    const Copy c = copyFor(app);
    const std::uint32_t base = IDENTITY_RAM + static_cast<std::uint32_t>(tab * TAB_STRIDE);
    writeRamStr(ram, base, c.tagline, TAGLINE_LEN);
    writeRamStr(ram, base + static_cast<std::uint32_t>(TAGLINE_LEN), c.about, ABOUT_LEN);
    ram[base + static_cast<std::uint32_t>(APP_ID_OFF)] = static_cast<std::uint8_t>(app);
    ram[base + static_cast<std::uint32_t>(PROG_ID_OFF)] =
        static_cast<std::uint8_t>(progId & 0xFF);
}

inline void paintAbout(FieldRtxWidgets::Ui& ui, AppId app,
                       int y0 = 64, int y1 = 168) noexcept {
    const Copy c = copyFor(app);
    const int lh = FieldRtxWidgets::UI_LABEL_H;
    const int gap = 4;
    ui.panel(16, y0, 1008, y1);
    int y = y0 + 8;
    ui.label(32, y, 992, y + lh, c.tagline, 1);
    y += lh + gap;
    ui.label(32, y, 992, y + lh, c.about, 0);
    y += lh + gap;
    if (c.about2 && c.about2[0]) {
        ui.label(32, y, 992, y + lh, c.about2, 0);
        y += lh + gap;
    }
    if (c.footer && c.footer[0])
        ui.label(32, y1 - lh - 8, 992, y1 - 8, c.footer, 0);
}

} // namespace FieldAosAppIdentity