#pragma once

// Folder thumbnail grid — opens in-place with icon atlas per entry (files + subfolders).

#include "FieldAmouranthHudRam.hpp"
#include "FieldAmouranthTextures.hpp"
#include "FieldExtensionMap.hpp"
#include "FieldRtxVfs.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <string>

namespace FieldAmouranthFolderView {

constexpr std::uint32_t FOLDER_VIEW_RAM = 0x000BD600u;
constexpr int           MAX_ITEMS       = 24;
constexpr int           GRID_COLS       = 6;
constexpr int           NAME_LEN        = 20;
constexpr int           PATH_LEN        = 56;
constexpr int           ROW_STRIDE      = 32;

struct Item {
    char         name[NAME_LEN + 1]{};
    std::uint8_t icon = 0;
    bool         isDir = false;
    std::uint32_t size = 0;
};

inline bool     open = false;
inline char     pathBuf[PATH_LEN + 1]{ "C:\\" };
inline Item     items[MAX_ITEMS]{};
inline int      itemCount = 0;
inline int      hover = -1;

inline std::uint8_t iconForName(const char* name, bool isDir) noexcept {
    using IS = FieldAmouranthTextures::IconSlot;
    if (isDir) return static_cast<std::uint8_t>(IS::Desktop);
    const std::string ext = FieldExtensionMap::upperExt(name);
    if (ext == ".EXE" || ext == ".COM") return static_cast<std::uint8_t>(IS::Shell);
    if (ext == ".BAT" || ext == ".ASM" || ext == ".C" || ext == ".FLD")
        return static_cast<std::uint8_t>(IS::AmmoCode);
    if (ext == ".NES" || ext == ".ROM" || ext == ".SFC" || ext == ".MD" || ext == ".SMS")
        return static_cast<std::uint8_t>(IS::Nes);
    if (ext == ".WAD") return static_cast<std::uint8_t>(IS::Doom);
    if (ext == ".TXT" || ext == ".DOC") return static_cast<std::uint8_t>(IS::AmmoText);
    if (ext == ".BAS") return static_cast<std::uint8_t>(IS::QBasic);
    if (ext == ".PNG" || ext == ".JPG") return static_cast<std::uint8_t>(IS::Wallpaper);
    if (ext == ".HTML") return static_cast<std::uint8_t>(IS::Display);
    if (ext == ".ZIP" || ext == ".ISO" || ext == ".IMG")
        return static_cast<std::uint8_t>(IS::Settings);
    return static_cast<std::uint8_t>(IS::FileCmd);
}

inline void writeRamStr(std::uint8_t* ram, std::uint32_t off, const char* s, int maxLen) noexcept {
    if (!ram || !s) return;
    for (int i = 0; i < maxLen; ++i) {
        const char ch = s[i] ? s[i] : ' ';
        ram[off + static_cast<std::uint32_t>(i)] = static_cast<std::uint8_t>(ch);
    }
}

inline void loadPath(const char* path) noexcept {
    if (!path || !path[0]) return;
    std::strncpy(pathBuf, path, PATH_LEN);
    pathBuf[PATH_LEN] = '\0';
    itemCount = 0;
    FieldRtxVfs::vfsInit();
    std::vector<FieldRtxVfs::RichEntry> rich;
    if (!FieldRtxVfs::listPathRich(pathBuf, false, true, rich)) return;
    std::sort(rich.begin(), rich.end(),
        [](const FieldRtxVfs::RichEntry& a, const FieldRtxVfs::RichEntry& b) {
            if (a.isDir != b.isDir) return a.isDir > b.isDir;
            return std::strcmp(a.name, b.name) < 0;
        });
    for (const auto& fe : rich) {
        if (itemCount >= MAX_ITEMS) break;
        Item& it = items[itemCount++];
        std::strncpy(it.name, fe.name, NAME_LEN);
        it.name[NAME_LEN] = '\0';
        it.isDir = fe.isDir;
        it.size = fe.size;
        it.icon = iconForName(fe.name, fe.isDir);
    }
}

inline void show(const char* path) noexcept {
    open = true;
    hover = -1;
    loadPath(path);
}

inline void close() noexcept {
    open = false;
    hover = -1;
    itemCount = 0;
}

inline int gridRows() noexcept {
    if (itemCount <= 0) return 1;
    return (itemCount + GRID_COLS - 1) / GRID_COLS;
}

inline void packRam(std::uint8_t* ram) noexcept {
    if (!ram) return;
    ram[FOLDER_VIEW_RAM] = static_cast<std::uint8_t>(itemCount);
    ram[FOLDER_VIEW_RAM + 1u] = hover >= 0
        ? static_cast<std::uint8_t>(hover) : 0xFFu;
    ram[FOLDER_VIEW_RAM + 2u] = static_cast<std::uint8_t>(GRID_COLS);
    ram[FOLDER_VIEW_RAM + 3u] = open ? 1u : 0u;
    writeRamStr(ram, FOLDER_VIEW_RAM + 4u, pathBuf, PATH_LEN);
    for (int i = 0; i < itemCount; ++i) {
        const std::uint32_t base = FOLDER_VIEW_RAM + 0x40u
            + static_cast<std::uint32_t>(i * ROW_STRIDE);
        ram[base] = items[i].icon;
        ram[base + 1u] = items[i].isDir ? 1u : 0u;
        ram[base + 2u] = static_cast<std::uint8_t>(items[i].size & 0xFFu);
        ram[base + 3u] = static_cast<std::uint8_t>((items[i].size >> 8) & 0xFFu);
        writeRamStr(ram, base + 4u, items[i].name, NAME_LEN);
    }
}

inline int itemAt(float mx, float my, float panelX, float panelY,
                  float panelW, float panelH, float scale) noexcept {
    if (!open) return -1;
    const float pad = 16.f * scale;
    const float headH = 36.f * scale;
    const float cellW = 96.f * scale;
    const float cellH = 108.f * scale;
    if (mx < panelX + pad || my < panelY + headH + pad) return -1;
    if (mx >= panelX + panelW - pad || my >= panelY + panelH - pad) return -1;
    const float relX = mx - panelX - pad;
    const float relY = my - panelY - headH - pad;
    const int col = static_cast<int>(relX / cellW);
    const int row = static_cast<int>(relY / cellH);
    if (col < 0 || col >= GRID_COLS || row < 0 || row >= gridRows()) return -1;
    const int idx = row * GRID_COLS + col;
    return idx < itemCount ? idx : -1;
}

inline void activateItem(int idx, std::uint8_t* ram) noexcept {
    if (idx < 0 || idx >= itemCount) return;
    const Item& it = items[idx];
    if (it.isDir) {
        std::string next = pathBuf;
        if (next.back() != '\\') next += '\\';
        next += it.name;
        loadPath(next.c_str());
        return;
    }
    std::string full = pathBuf;
    if (full.back() != '\\') full += '\\';
    full += it.name;
    FieldExtensionMap::launchFile(ram, full.c_str());
    close();
}

} // namespace FieldAmouranthFolderView