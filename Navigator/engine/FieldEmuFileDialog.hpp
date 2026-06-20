#pragma once

// In-emulator ROM file picker — VFS browser filtered by extension.

#include "FieldAmmoFat.hpp"
#include "FieldAmmoVfs.hpp"
#include "FieldDos.hpp"
#include "FieldRtxApp.hpp"
#include "FieldRtxGui.hpp"
#include "FieldRtxMouse.hpp"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace FieldEmuFileDialog {

struct Entry {
    std::string name;
    bool dir = false;
    std::uint32_t size = 0;
};

inline bool active = false;
inline std::string path = "C:\\";
inline std::string result;
inline std::vector<Entry> entries;
inline int selRow = 0;
inline int scrollTop = 0;
inline const char* title = "Open ROM";
inline const char* const* filterExts = nullptr;
inline int filterCount = 0;

inline bool extMatches(const char* name) noexcept {
    if (!name || !filterExts || filterCount <= 0) return true;
    std::string low = name;
    for (auto& c : low) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (int i = 0; i < filterCount; ++i) {
        if (!filterExts[i]) continue;
        std::string ext = filterExts[i];
        for (auto& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (low.size() >= ext.size()
                && low.compare(low.size() - ext.size(), ext.size(), ext) == 0)
            return true;
    }
    return false;
}

inline void listDir() noexcept {
    entries.clear();
    entries.push_back({"..", true, 0});
    std::vector<FieldDos::FatRootEntry> raw;
    if (!FieldAmmoVfs::listPath(path.c_str(), raw)) return;
    for (const auto& e : raw) {
        if (e.isDir) {
            entries.push_back({e.name, true, 0});
            continue;
        }
        if (extMatches(e.name))
            entries.push_back({e.name, false, e.size});
    }
    std::sort(entries.begin() + 1, entries.end(),
        [](const Entry& a, const Entry& b) {
            if (a.dir != b.dir) return a.dir > b.dir;
            return a.name < b.name;
        });
    scrollTop = 0;
    selRow = entries.size() > 1 ? 1 : 0;
}

inline void open(const char* startDir, const char* dialogTitle,
                 const char* const* exts, int extCount) noexcept {
    active = true;
    result.clear();
    title = dialogTitle ? dialogTitle : "Open ROM";
    filterExts = exts;
    filterCount = extCount;
    path = (startDir && startDir[0]) ? startDir : "C:\\";
    if (path.back() != '\\') path += '\\';
    listDir();
}

inline void close() noexcept {
    active = false;
    entries.clear();
    scrollTop = selRow = 0;
}

inline std::string fullPath(const Entry& e) noexcept {
    std::string p = path;
    if (p.back() != '\\') p += '\\';
    p += e.name;
    return p;
}

inline void enterSel() noexcept {
    if (selRow < 0 || selRow >= static_cast<int>(entries.size())) return;
    const Entry& e = entries[static_cast<std::size_t>(selRow)];
    if (e.name == "..") {
        if (path.size() > 3) {
            const auto slash = path.find_last_of('\\', path.size() - 2);
            path = (slash != std::string::npos) ? path.substr(0, slash + 1) : path.substr(0, 3);
        }
        listDir();
        return;
    }
    if (e.dir) {
        if (path.back() != '\\') path += '\\';
        path += e.name;
        if (path.back() != '\\') path += '\\';
        listDir();
        return;
    }
    result = fullPath(e);
    close();
}

inline void paint(std::uint8_t* ram) noexcept {
    if (!active || !ram) return;
    FieldRtxApp::begin(ram, 90u);
    auto& ui = FieldRtxApp::ui;
    ui.panel(120, 80, 904, 944);
    char hdr[80];
    std::snprintf(hdr, sizeof hdr, "%s", title);
    ui.label(144, 96, 640, 128, hdr, 1);

    char pathLine[96];
    std::snprintf(pathLine, sizeof pathLine, "Look in: %s", path.c_str());
    ui.label(144, 136, 860, 164, pathLine, 0);

    constexpr int kVis = 14;
    FieldRtxApp::scrollMax = std::max(0, static_cast<int>(entries.size()) - kVis);
    FieldRtxApp::scrollTop = std::clamp(scrollTop, 0, FieldRtxApp::scrollMax);
    scrollTop = FieldRtxApp::scrollTop;

    int y = 180;
    for (int i = 0; i < kVis; ++i) {
        const int ei = scrollTop + i;
        if (ei >= static_cast<int>(entries.size())) break;
        const Entry& e = entries[static_cast<std::size_t>(ei)];
        char line[72];
        if (e.dir)
            std::snprintf(line, sizeof line, "%s%s", e.name == ".." ? "" : "<DIR> ", e.name.c_str());
        else
            std::snprintf(line, sizeof line, "%-28s %8u", e.name.c_str(), e.size);
        const std::uint8_t flags = (ei == selRow)
            ? static_cast<std::uint8_t>(FieldRtxWidgets::Flag::Focus) : 0u;
        ui.button(152, y, 820, y + 34, line, 200 + ei, flags);
        y += 38;
    }
    ui.vscroll(832, 176, 856, 720, scrollTop, static_cast<int>(entries.size()));

    ui.button(144, 760, 280, 800, "Open", 1);
    ui.button(300, 760, 440, 800, "Cancel", 2);
    ui.label(144, 812, 820, 840,
        "Enter/Open load  Esc/Cancel abort  Up/Down select  Dbl-click folder", 0);
    FieldRtxApp::finish(ram);
}

inline bool pumpKey(std::uint8_t* ram, std::uint16_t key, bool keyDown) noexcept {
    if (!active) return false;
    if (!keyDown) { paint(ram); return true; }
    switch (key) {
    case 0x011Bu:
        result.clear();
        close();
        return true;
    case 0x4800u:
        if (selRow > 0) --selRow;
        if (selRow < scrollTop) scrollTop = selRow;
        paint(ram);
        return true;
    case 0x5000u: {
        const int n = static_cast<int>(entries.size());
        if (selRow + 1 < n) ++selRow;
        if (selRow >= scrollTop + 14) scrollTop = selRow - 13;
        paint(ram);
        return true;
    }
    case 0x4900u:
        scrollTop = std::max(0, scrollTop - 5);
        selRow = std::max(1, selRow - 5);
        paint(ram);
        return true;
    case 0x5100u:
        scrollTop = std::min(FieldRtxApp::scrollMax, scrollTop + 5);
        selRow = std::min(static_cast<int>(entries.size()) - 1, selRow + 5);
        paint(ram);
        return true;
    case 0x1C0Du:
        enterSel();
        paint(ram);
        return true;
    default:
        break;
    }
    paint(ram);
    return true;
}

inline bool pumpMouse(std::uint8_t* ram, int action) noexcept {
    if (!active) return false;
    if (action == 1) {
        enterSel();
        paint(ram);
        return true;
    }
    if (action == 2) {
        result.clear();
        close();
        return true;
    }
    if (action >= 200) {
        const int ei = action - 200;
        if (ei >= 0 && ei < static_cast<int>(entries.size())) {
            if (ei == selRow) enterSel();
            else {
                selRow = ei;
                if (selRow < scrollTop) scrollTop = selRow;
                if (selRow >= scrollTop + 14) scrollTop = selRow - 13;
            }
        }
        paint(ram);
        return true;
    }
    if (FieldRtxApp::pumpScroll(action)) {
        scrollTop = FieldRtxApp::scrollTop;
        paint(ram);
        return true;
    }
    return false;
}

} // namespace FieldEmuFileDialog