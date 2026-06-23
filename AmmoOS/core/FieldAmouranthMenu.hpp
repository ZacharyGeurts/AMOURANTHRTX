#pragma once

// AmouranthOS compact Start menu — root folders + flyout submenus → guest RAM for GPU HUD.

#include "FieldAmouranthHudRam.hpp"
#include "FieldAmouranthTextures.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace FieldAmouranthMenu {

inline void copyText(char* dst, int cap, const char* src) noexcept {
    if (!dst || cap <= 0) return;
    if (!src) { dst[0] = '\0'; return; }
    const int max = cap - 1;
    int i = 0;
    for (; i < max && src[i] != '\0'; ++i)
        dst[i] = src[i];
    dst[i] = '\0';
}

constexpr std::uint32_t MENU_RAM       = FieldAmouranthHudRam::MENU_RAM;
constexpr std::uint32_t TASKBAR_RAM    = FieldAmouranthHudRam::TASKBAR_RAM;
constexpr std::uint32_t CLOCK_DATE_RAM = FieldAmouranthHudRam::CLOCK_DATE_RAM;
constexpr int           MAX_ROWS     = FieldAmouranthHudRam::MENU_MAX_ROWS;
constexpr int           ROW_STRIDE   = FieldAmouranthHudRam::MENU_ROW_STRIDE;
constexpr std::uint32_t MENU_FLYOUT_OFF = FieldAmouranthHudRam::MENU_FLYOUT_OFF;
constexpr int           LABEL_LEN    = 36;
constexpr int           DESC_LEN     = 72;
constexpr int           MAX_TABS     = 8;
constexpr int           TAB_STRIDE   = 32;

enum class RowType : std::uint8_t {
    Category = 0,
    Item     = 1,
    Sep      = 2,
    Exit     = 3,
};

enum class Category : std::uint8_t {
    Programs = 0,
    Toolkit  = 1,
    System   = 2,
    Games    = 3,
    Count
};

enum class MenuPane : std::uint8_t {
    Root   = 0,
    Flyout = 1,
};

struct SourceEntry {
    const char* label;
    const char* desc;
    int         action;
    std::uint8_t icon;
    Category    cat;
    RowType     type;
};

struct VisibleRow {
    RowType     type = RowType::Item;
    int         action = 0;
    std::uint8_t icon = 0;
    Category    cat = Category::Programs;
    char        label[LABEL_LEN + 1]{};
    char        desc[DESC_LEN + 1]{};
};

inline VisibleRow rootRows[MAX_ROWS]{};
inline int rootCount = 0;
inline VisibleRow flyoutRows[MAX_ROWS]{};
inline int flyoutCount = 0;

inline int rootHover = -1;
inline int rootFocus = -1;
inline int flyoutHover = -1;
inline int flyoutFocus = -1;
inline int flyoutCat = -1;
inline MenuPane focusPane = MenuPane::Root;

inline void writeRamByte(std::uint8_t* ram, std::uint32_t off, std::uint8_t v) noexcept {
    if (ram) ram[off] = v;
}

inline void writeRamStr(std::uint8_t* ram, std::uint32_t off, const char* s, int maxLen) noexcept {
    if (!ram || !s) return;
    for (int i = 0; i < maxLen; ++i) {
        const char ch = s[i] ? s[i] : ' ';
        ram[off + static_cast<std::uint32_t>(i)] = static_cast<std::uint8_t>(ch);
    }
}

static const SourceEntry kCatalog[] = {
    { "Programs",
      "Launchers, DOS shell consoles, and field network browsing",
      0, 16, Category::Programs, RowType::Category },
    { "RTX Shell",
      "Full RTX-DOS 7.0 command console with Golden Era shell and scrollback",
      2, 4, Category::Programs, RowType::Item },
    { "DOS Terminal",
      "RTX-DOS 7.0 command console with HELP, DIR, and scrollback",
      33, 4, Category::Programs, RowType::Item },
    { "Program Launcher",
      "GUI picker for AmmoCode, Doom, PADTEST, and other desktop apps",
      32, 4, Category::Programs, RowType::Item },
    { "Web Browser",
      "Hooks your OS default browser (Firefox/Chrome) into the display panel",
      15, static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Display),
      Category::Programs, RowType::Item },
    { "Toolkit",
      "Programming tools, compilers, editors, and hardware calibration",
      0, 16, Category::Toolkit, RowType::Category },
    { "AmmoCode IDE",
      "Turbo Pascal-style IDE with minimap, ASM debug, and field layers",
      1, 5, Category::Toolkit, RowType::Item },
    { "Field Compiler",
      "Compile .fld sources to AMMO object files with Field Compiler v4",
      4, 9, Category::Toolkit, RowType::Item },
    { "QBASIC",
      "Classic BASIC interpreter and lesson mode inside AmmoCode",
      3, 8, Category::Toolkit, RowType::Item },
    { "VSCodium",
      "Launch host VSCodium or VS Code editor on the Linux desktop",
      12, 7, Category::Toolkit, RowType::Item },
    { "PADTEST",
      "Xbox 360 / SDL gamepad live view, calibration, and RTX input QA",
      5, 10, Category::Toolkit, RowType::Item },
    { "AmmoText",
      "RTX Notepad — spellcheck, themes, help file, syntax highlighting",
      28, static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::AmmoText),
      Category::Toolkit, RowType::Item },
    { "RTX DevKit",
      "AMMOASM AMMOCC AMMOLINK AMMORUN AMMODBG toolchain on C:\\TOOLS",
      25, 9, Category::Toolkit, RowType::Item },
    { "System",
      "Drive rack, optical media, CMOS setup, and RTX device layers",
      0, 16, Category::System, RowType::Category },
    { "Field Commander",
      "Dual-pane scrollable file manager with GPU commander chrome",
      11, 6, Category::System, RowType::Item },
    { "Drive Rack",
      "Mount, inspect, and switch RTX-AMMOS drive letters and FAT volumes",
      13, 12, Category::System, RowType::Item },
    { "Mount CD-ROM",
      "Attach ISO9660 CD image to guest MSCDEX redirector",
      14, 17, Category::System, RowType::Item },
    { "AmmoBIOS Setup",
      "CMOS/NVRAM configuration, boot order, and AMMO device POST",
      16, 12, Category::System, RowType::Item },
    { "Device Manager",
      "RTX PC chassis view — CPU, drives, SB16, ports, field SYS drivers",
      17, 12, Category::System, RowType::Item },
    { "Registry Editor",
      "HKRTX hierarchical config — audio, layers, file associations",
      18, 12, Category::System, RowType::Item },
    { "MEMORYUP",
      "MemMaker-class memory optimizer — conventional/XMS/UMB profile",
      23, 13, Category::System, RowType::Item },
    { "SCANDISK",
      "Deep FAT + RAID journal surface scan and volume repair",
      24, 13, Category::System, RowType::Item },
    { "Sound Rack",
      "SB16 OPL GUS ESS test tones and C:\\SOUND program launcher",
      26, 11, Category::System, RowType::Item },
    { "Extension Map",
      "File association editor — HKRTX\\Associations synced to registry",
      27, 6, Category::System, RowType::Item },
    { "RTX Themes",
      "Windowing color presets — Aqua, Golden Era, Amouranth Rose",
      29, static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Display),
      Category::System, RowType::Item },
    { "Games",
      "Shareware classics, emulators, and DOS/4GW launchers",
      0, 16, Category::Games, RowType::Category },
    { "AmmoNES",
      "iNES emulator — File>Open ROM, Options>Settings/Controls",
      6, 11, Category::Games, RowType::Item },
    { "Shareware Doom",
      "id Software DOOM shareware via DOS/4GW and RTX VGA path",
      7, 14, Category::Games, RowType::Item },
    { "Commander Keen 4",
      "Apogee Commander Keen 4 shareware — EGA platformer in new window",
      20, 11, Category::Games, RowType::Item },
    { "Wolfenstein 3D",
      "id Software Wolfenstein 3D shareware FPS in dedicated window",
      21, 14, Category::Games, RowType::Item },
    { "Cosmo's Adventure",
      "Apogee Cosmo's Cosmic Adventure shareware jump-n-run",
      22, 11, Category::Games, RowType::Item },
    { "AmmoA2600",
      "Atari VCS — File>Open .a26/.bin from C:\\A2600\\",
      34, 11, Category::Games, RowType::Item },
    { "AmmoSMS",
      "Sega Master System — File>Open .sms from C:\\SMS\\",
      36, 11, Category::Games, RowType::Item },
    { "AmmoGenesis",
      "Sega Genesis — File>Open .md/.bin from C:\\GENESIS\\",
      38, 11, Category::Games, RowType::Item },
    { "AmmoSNES",
      "Super Nintendo — File>Open .sfc/.smc, Options>SuperFX",
      40, 11, Category::Games, RowType::Item },
    { "Amiga Love",
      "Commodore Amiga 68000 — Love of EVERYTHING canvas (Paula + Denise AGA)",
      42, 11, Category::Games, RowType::Item },
    { "CHIPS Wave",
      "PS1 + Xbox360 + Amiga GPU die — one canvas, infinite field storage",
      43, 14, Category::Games, RowType::Item },
};

inline void copyEntry(VisibleRow& row, const SourceEntry& e) noexcept {
    row.type = e.type;
    row.action = e.action;
    row.icon = e.icon;
    row.cat = e.cat;
    copyText(row.label, LABEL_LEN + 1, e.label);
    copyText(row.desc, DESC_LEN + 1, e.desc);
}

inline void rebuildRoot() noexcept {
    rootCount = 0;
    for (const auto& e : kCatalog) {
        if (e.type != RowType::Category) continue;
        if (rootCount >= MAX_ROWS) break;
        copyEntry(rootRows[rootCount++], e);
    }
    if (rootCount + 2 <= MAX_ROWS) {
        auto& sep = rootRows[rootCount++];
        sep.type = RowType::Sep;
        sep.label[0] = '\0';
        auto& exitRow = rootRows[rootCount++];
        exitRow.type = RowType::Exit;
        exitRow.action = 99;
        exitRow.icon = static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Power);
        copyText(exitRow.label, LABEL_LEN + 1, "Shut Down");
        copyText(exitRow.desc, DESC_LEN + 1, "Exit AmmoOS gracefully");
    }
}

inline void rebuildFlyout() noexcept {
    flyoutCount = 0;
    if (flyoutCat < 0 || flyoutCat >= static_cast<int>(Category::Count)) return;
    const auto want = static_cast<Category>(flyoutCat);
    for (const auto& e : kCatalog) {
        if (e.type != RowType::Item || e.cat != want) continue;
        if (flyoutCount >= MAX_ROWS) break;
        copyEntry(flyoutRows[flyoutCount++], e);
    }
}

inline void rebuildVisible() noexcept {
    rebuildRoot();
    rebuildFlyout();
}

inline bool flyoutOpen() noexcept { return flyoutCat >= 0; }

inline void openFlyout(int cat) noexcept {
    if (cat < 0 || cat >= static_cast<int>(Category::Count)) return;
    flyoutCat = cat;
    flyoutHover = -1;
    rebuildFlyout();
    flyoutFocus = flyoutCount > 0 ? 0 : -1;
    if (flyoutFocus >= 0) flyoutHover = flyoutFocus;
}

inline void closeFlyout() noexcept {
    flyoutCat = -1;
    flyoutHover = -1;
    flyoutFocus = -1;
    flyoutCount = 0;
    focusPane = MenuPane::Root;
}

inline int rowAt(float my, float rowY0, float rowH, int count) noexcept {
    const int idx = static_cast<int>((my - rowY0) / rowH);
    return (idx >= 0 && idx < count) ? idx : -1;
}

inline float rootMenuHeight(float rowH, float headerH, float pad) noexcept {
    rebuildRoot();
    return pad * 2.f + headerH + rowH * static_cast<float>(rootCount);
}

inline float flyoutMenuHeight(float rowH, float pad) noexcept {
    if (!flyoutOpen()) return 0.f;
    rebuildFlyout();
    return pad * 2.f + rowH * static_cast<float>(flyoutCount);
}

inline float menuHeight(float rowH, float headerH, float pad) noexcept {
    return rootMenuHeight(rowH, headerH, pad);
}

inline int firstRootNavRow() noexcept {
    rebuildRoot();
    for (int i = 0; i < rootCount; ++i)
        if (rootRows[i].type != RowType::Sep) return i;
    return rootCount > 0 ? 0 : -1;
}

inline int firstFlyoutNavRow() noexcept {
    rebuildFlyout();
    return flyoutCount > 0 ? 0 : -1;
}

inline int nextNavRow(const VisibleRow* rows, int count, int from, int dir) noexcept {
    if (count <= 0) return -1;
    int r = from;
    for (int tries = 0; tries < count; ++tries) {
        r = (r + dir + count) % count;
        if (rows[r].type != RowType::Sep) return r;
    }
    return from;
}

inline int actionForRootRow(int row) noexcept {
    if (row < 0 || row >= rootCount) return 0;
    const auto& r = rootRows[row];
    if (r.type == RowType::Category) {
        if (flyoutCat == static_cast<int>(r.cat))
            closeFlyout();
        else
            openFlyout(static_cast<int>(r.cat));
        return -1;
    }
    if (r.type == RowType::Sep) return 0;
    return r.action;
}

inline int actionForFlyoutRow(int row) noexcept {
    if (row < 0 || row >= flyoutCount) return 0;
    return flyoutRows[row].action;
}

inline int actionForRow(int row) noexcept {
    return actionForRootRow(row);
}

inline void openMenuFocus() noexcept {
    focusPane = MenuPane::Root;
    rootFocus = firstRootNavRow();
    rootHover = rootFocus;
    closeFlyout();
}

inline void closeMenuFocus() noexcept {
    rootFocus = -1;
    rootHover = -1;
    closeFlyout();
}

inline bool handleMenuKey(std::uint16_t key) noexcept {
    const std::uint16_t sc = static_cast<std::uint16_t>(key & 0xFFu);
    rebuildRoot();
    if (rootCount <= 0) return false;

    if (sc == 0x01u) return true;
    if (sc == 0x4Bu) {
        if (focusPane == MenuPane::Flyout && flyoutOpen()) {
            focusPane = MenuPane::Root;
            flyoutHover = flyoutFocus;
            if (rootFocus < 0) rootFocus = firstRootNavRow();
            rootHover = rootFocus;
        }
        return true;
    }
    if (sc == 0x4Du) {
        if (focusPane == MenuPane::Root && rootFocus >= 0
            && rootRows[rootFocus].type == RowType::Category) {
            openFlyout(static_cast<int>(rootRows[rootFocus].cat));
            focusPane = MenuPane::Flyout;
            if (flyoutFocus < 0) flyoutFocus = firstFlyoutNavRow();
            flyoutHover = flyoutFocus;
        }
        return true;
    }
    if (sc == 0x48u) {
        if (focusPane == MenuPane::Flyout && flyoutOpen()) {
            if (flyoutFocus < 0) flyoutFocus = firstFlyoutNavRow();
            else flyoutFocus = nextNavRow(flyoutRows, flyoutCount, flyoutFocus, -1);
            flyoutHover = flyoutFocus;
        } else {
            if (rootFocus < 0) rootFocus = firstRootNavRow();
            else rootFocus = nextNavRow(rootRows, rootCount, rootFocus, -1);
            rootHover = rootFocus;
        }
        return true;
    }
    if (sc == 0x50u) {
        if (focusPane == MenuPane::Flyout && flyoutOpen()) {
            if (flyoutFocus < 0) flyoutFocus = firstFlyoutNavRow();
            else flyoutFocus = nextNavRow(flyoutRows, flyoutCount, flyoutFocus, 1);
            flyoutHover = flyoutFocus;
        } else {
            if (rootFocus < 0) rootFocus = firstRootNavRow();
            else rootFocus = nextNavRow(rootRows, rootCount, rootFocus, 1);
            rootHover = rootFocus;
        }
        return true;
    }
    if (sc == 0x0Du || key == 0x1C0Du) {
        if (focusPane == MenuPane::Flyout && flyoutOpen()) {
            if (flyoutFocus < 0) flyoutFocus = firstFlyoutNavRow();
            flyoutHover = flyoutFocus;
        } else {
            if (rootFocus < 0) rootFocus = firstRootNavRow();
            rootHover = rootFocus;
        }
        return true;
    }
    return false;
}

inline void packRowRam(std::uint8_t* ram, std::uint32_t base, const VisibleRow& r) noexcept {
    writeRamByte(ram, base, static_cast<std::uint8_t>(r.type));
    std::uint8_t rowIcon = r.icon;
    if (r.type == RowType::Category) {
        const bool open = flyoutOpen() && flyoutCat == static_cast<int>(r.cat);
        rowIcon = open ? static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Settings)
                       : static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Desktop);
    } else if (r.type == RowType::Exit) {
        rowIcon = static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Power);
    }
    writeRamByte(ram, base + 1u, rowIcon);
    writeRamByte(ram, base + 2u, 0u);
    writeRamByte(ram, base + 3u, static_cast<std::uint8_t>(r.cat));
    writeRamStr(ram, base + 4u, r.label, LABEL_LEN);
    writeRamStr(ram, base + 4u + LABEL_LEN, r.desc, DESC_LEN);
}

inline void packMenuRam(std::uint8_t* ram) noexcept {
    if (!ram) return;
    writeRamByte(ram, MENU_RAM, static_cast<std::uint8_t>(rootCount));
    const int menuRootHover = rootHover >= 0 ? rootHover : rootFocus;
    writeRamByte(ram, MENU_RAM + 1u,
        menuRootHover >= 0 ? static_cast<std::uint8_t>(menuRootHover) : 0xFFu);
    writeRamByte(ram, MENU_RAM + 2u,
        flyoutOpen() ? static_cast<std::uint8_t>(flyoutCat) : 0xFFu);
    const int menuFlyHover = flyoutHover >= 0 ? flyoutHover : flyoutFocus;
    writeRamByte(ram, MENU_RAM + 3u,
        menuFlyHover >= 0 ? static_cast<std::uint8_t>(menuFlyHover) : 0xFFu);
    writeRamByte(ram, MENU_RAM + 4u, static_cast<std::uint8_t>(flyoutCount));
    writeRamByte(ram, MENU_RAM + 5u, static_cast<std::uint8_t>(focusPane));

    for (int i = 0; i < MAX_ROWS; ++i) {
        const std::uint32_t base = MENU_RAM + 0x10u + static_cast<std::uint32_t>(i * ROW_STRIDE);
        if (i >= rootCount) {
            writeRamByte(ram, base, 0xFFu);
            continue;
        }
        packRowRam(ram, base, rootRows[i]);
    }
    for (int i = 0; i < MAX_ROWS; ++i) {
        const std::uint32_t base = MENU_RAM + MENU_FLYOUT_OFF
            + static_cast<std::uint32_t>(i * ROW_STRIDE);
        if (i >= flyoutCount) {
            writeRamByte(ram, base, 0xFFu);
            continue;
        }
        packRowRam(ram, base, flyoutRows[i]);
    }
}

struct ProgramTab {
    const char* title;
    std::uint8_t icon;
};

inline void packTaskbarRam(std::uint8_t* ram, const ProgramTab* tabs, int count,
        int focused, int hover, std::uint32_t minimizedMask) noexcept {
    if (!ram) return;
    int n = std::min(count, MAX_TABS);
    if (tabs) {
        while (n > 0 && tabs[n - 1].icon == 0u)
            --n;
    }
    writeRamByte(ram, TASKBAR_RAM, static_cast<std::uint8_t>(n));
    writeRamByte(ram, TASKBAR_RAM + 1u, static_cast<std::uint8_t>(focused));
    writeRamByte(ram, TASKBAR_RAM + 2u, static_cast<std::uint8_t>(hover));
    writeRamByte(ram, TASKBAR_RAM + 3u, static_cast<std::uint8_t>(minimizedMask & 0xFFu));
    for (int i = 0; i < MAX_TABS; ++i) {
        const std::uint32_t base = TASKBAR_RAM + 0x10u + static_cast<std::uint32_t>(i * TAB_STRIDE);
        if (i < n && tabs) {
            writeRamStr(ram, base, tabs[i].title, TAB_STRIDE - 1);
            writeRamByte(ram, base + TAB_STRIDE - 1u, tabs[i].icon);
        } else {
            writeRamByte(ram, base + TAB_STRIDE - 1u, 0u);
            for (int j = 0; j < TAB_STRIDE - 1; ++j)
                writeRamByte(ram, base + static_cast<std::uint32_t>(j), 0u);
        }
    }
}

inline void packClockDateRam(std::uint8_t* ram, const char* dateStr) noexcept {
    if (!ram || !dateStr) return;
    int len = 0;
    while (dateStr[len] != '\0' && len < 24) ++len;
    writeRamByte(ram, CLOCK_DATE_RAM, static_cast<std::uint8_t>(len));
    for (int i = 0; i < 24; ++i) {
        const char ch = (i < len) ? dateStr[i] : '\0';
        writeRamByte(ram, CLOCK_DATE_RAM + 1u + static_cast<std::uint32_t>(i),
            static_cast<std::uint8_t>(ch));
    }
}

} // namespace FieldAmouranthMenu