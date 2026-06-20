#pragma once

// AmouranthOS folder-button popout — file-type thumbnails backed by icon atlas slots.

#include "FieldAmouranthHudRam.hpp"
#include "FieldAmouranthTextures.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace FieldAmouranthFilesMenu {

constexpr std::uint32_t FILES_MENU_RAM   = FieldAmouranthHudRam::FILES_MENU_RAM;
constexpr int           MAX_ITEMS        = FieldAmouranthHudRam::FILES_MENU_MAX;
constexpr int           ROW_STRIDE       = FieldAmouranthHudRam::FILES_ROW_STRIDE;
constexpr int           GRID_COLS        = 4;
constexpr int           LABEL_LEN        = 8;

struct FileTypeEntry {
    const char* ext;
    std::uint8_t icon;
    int         action;
};

struct Item {
    char        ext[LABEL_LEN + 1]{};
    std::uint8_t icon = 0;
    int         action = 0;
};

inline Item items[MAX_ITEMS]{};
inline int  itemCount = 0;
inline int  hover = -1;

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

static const FileTypeEntry kCatalog[] = {
    { "Folder", static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Desktop), 11 },
    { ".EXE",   static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Shell),    11 },
    { ".COM",   static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Shell),    11 },
    { ".BAT",   static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::AmmoCode),  11 },
    { ".NES",   static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Nes),       6  },
    { ".ROM",   static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Nes),       6  },
    { ".SFC",   static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Nes),       6  },
    { ".MD",    static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Nes),       6  },
    { ".SMS",   static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Nes),       6  },
    { ".A26",   static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::PadTest),   6  },
    { ".WAD",   static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Doom),      7  },
    { ".ZIP",   static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Settings),  11 },
    { ".ISO",   static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Settings),  11 },
    { ".IMG",   static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Settings),  11 },
    { ".TXT",   static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::AmmoText),  19 },
    { ".DOC",   static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::AmmoText),  19 },
    { ".ASM",   static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::AmmoCode),  4  },
    { ".C",     static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::FieldC),     4  },
    { ".FLD",   static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::FieldC),     4  },
    { ".BAS",   static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::QBasic),     3  },
    { ".PNG",   static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Wallpaper),  11 },
    { ".JPG",   static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Wallpaper),  11 },
    { ".HTML",  static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Display),   11 },
    { ".WAV",   static_cast<std::uint8_t>(FieldAmouranthTextures::IconSlot::Settings),  11 },
};

inline void rebuild() noexcept {
    itemCount = static_cast<int>(sizeof(kCatalog) / sizeof(kCatalog[0]));
    if (itemCount > MAX_ITEMS) itemCount = MAX_ITEMS;
    for (int i = 0; i < itemCount; ++i) {
        std::strncpy(items[i].ext, kCatalog[i].ext, LABEL_LEN);
        items[i].icon = kCatalog[i].icon;
        items[i].action = kCatalog[i].action;
    }
}

inline int gridRows() noexcept {
    if (itemCount <= 0) return 1;
    return (itemCount + GRID_COLS - 1) / GRID_COLS;
}

inline void packRam(std::uint8_t* ram) noexcept {
    if (!ram) return;
    writeRamByte(ram, FILES_MENU_RAM, static_cast<std::uint8_t>(itemCount));
    writeRamByte(ram, FILES_MENU_RAM + 1u,
        hover >= 0 ? static_cast<std::uint8_t>(hover) : 0xFFu);
    writeRamByte(ram, FILES_MENU_RAM + 2u, static_cast<std::uint8_t>(GRID_COLS));
    for (int i = 0; i < itemCount; ++i) {
        const std::uint32_t base = FILES_MENU_RAM + 0x10u
            + static_cast<std::uint32_t>(i * ROW_STRIDE);
        writeRamByte(ram, base, items[i].icon);
        writeRamByte(ram, base + 1u, static_cast<std::uint8_t>(items[i].action & 0xFF));
        writeRamByte(ram, base + 2u, static_cast<std::uint8_t>((items[i].action >> 8) & 0xFF));
        writeRamStr(ram, base + 3u, items[i].ext, LABEL_LEN);
    }
}

inline int itemAt(float mx, float my, float panelX, float panelY,
        float cellW, float cellH, float pad) noexcept {
    if (mx < panelX + pad || my < panelY + pad) return -1;
    const float relX = mx - panelX - pad;
    const float relY = my - panelY - pad;
    const int col = static_cast<int>(relX / cellW);
    const int row = static_cast<int>(relY / cellH);
    if (col < 0 || col >= GRID_COLS || row < 0 || row >= gridRows()) return -1;
    const float inCellX = relX - static_cast<float>(col) * cellW;
    const float inCellY = relY - static_cast<float>(row) * cellH;
    if (inCellX > cellW - 4.f || inCellY > cellH - 4.f) return -1;
    const int idx = row * GRID_COLS + col;
    return idx < itemCount ? idx : -1;
}

inline int actionFor(int idx) noexcept {
    if (idx < 0 || idx >= itemCount) return 0;
    return items[idx].action;
}

inline std::uint8_t iconFor(int idx) noexcept {
    if (idx < 0 || idx >= itemCount) return 0;
    return items[idx].icon;
}

inline const char* labelFor(int idx) noexcept {
    if (idx < 0 || idx >= itemCount) return "";
    return items[idx].ext;
}

} // namespace FieldAmouranthFilesMenu