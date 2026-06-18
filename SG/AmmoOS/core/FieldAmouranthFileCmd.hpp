#pragma once

// AmmoFiles — Nautilus-style dual-pane browser with context menus and extension launch.

#include "FieldAmouranthLaunch.hpp"
#include "FieldExtensionMap.hpp"
#include "FieldAmmoVfs.hpp"
#include "FieldDrives.hpp"
#include "FieldDos.hpp"
#include "FieldExtensionEditor.hpp"
#include "FieldExtensionMap.hpp"
#include "FieldRtxGui.hpp"
#include "FieldRtxMouse.hpp"
#include "FieldRtxVfs.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace FieldAmouranthFileCmd {

struct Entry {
    std::string name;
    bool        dir = false;
    std::uint32_t size = 0;
    std::uint8_t attr = 0x07;
    const char* desc = nullptr;
};

inline bool active = false;
inline int  scrollTop = 0;
inline int  selRow = 0;
inline int  pane = 0; // 0=C:\ 1=E:\HOST
inline std::string pathL = "C:\\";
inline std::string pathR = "E:\\";
inline std::vector<Entry> entriesL;
inline std::vector<Entry> entriesR;

inline bool ctxOpen = false;
inline int  ctxRow = -1;
inline int  ctxSel = 0;
inline int  ctxCol = 0;
inline int  ctxMenuRow = 0;
inline std::string clipPath;

inline std::string fullPathFor(const std::string& base, const Entry& e) {
    std::string p = base;
    if (p.back() != '\\') p += '\\';
    p += e.name;
    return p;
}

constexpr int PANEL_ROW0 = 4;
constexpr int PANEL_ROW1 = 20;
constexpr int ROWS_VIS = PANEL_ROW1 - PANEL_ROW0;
constexpr int COL_L0 = 2;
constexpr int COL_L1 = 38;
constexpr int COL_R0 = 41;
constexpr int COL_R1 = 77;

inline void listPath(const std::string& path, std::vector<Entry>& out) {
    out.clear();
    out.push_back({ "..", true, 0, 0x1Bu, nullptr });
    if (path.size() >= 2 && path[0] == 'C' && path[1] == ':') {
        FieldRtxVfs::vfsInit();
        std::vector<FieldRtxVfs::RichEntry> rich;
        if (FieldRtxVfs::listPathRich(path.c_str(), false, true, rich)) {
            for (const auto& fe : rich) {
                Entry e;
                e.name = fe.name;
                e.dir = fe.isDir;
                e.size = fe.size;
                e.attr = fe.vgaAttr;
                e.desc = fe.desc;
                out.push_back(std::move(e));
            }
        }
        std::sort(out.begin() + 1, out.end(),
            [](const Entry& a, const Entry& b) {
                if (a.dir != b.dir) return a.dir > b.dir;
                return a.name < b.name;
            });
        return;
    }
    if (path.size() >= 2 && path[0] == 'E') {
        std::vector<std::string> names;
        if (FieldDrives::listHostBridge(names)) {
            for (const auto& n : names) {
                auto re = FieldRtxVfs::richFromName(n.c_str(), 0, false);
                Entry e;
                e.name = n;
                e.attr = re.vgaAttr;
                e.desc = re.desc;
                out.push_back(std::move(e));
            }
        }
        std::sort(out.begin() + 1, out.end(),
            [](const Entry& a, const Entry& b) { return a.name < b.name; });
    }
}

inline void refresh() noexcept {
    listPath(pathL, entriesL);
    listPath(pathR, entriesR);
    scrollTop = 0;
    selRow = 0;
}

inline void open() noexcept {
    active = true;
    pathL = "C:\\";
    pathR = "E:\\";
    refresh();
}

inline void close() noexcept {
    active = false;
}

inline std::vector<Entry>& activeEntries() noexcept {
    return pane == 0 ? entriesL : entriesR;
}

inline std::string& activePath() noexcept {
    return pane == 0 ? pathL : pathR;
}

inline void execSel(std::uint8_t* ram) noexcept {
    auto& ents = activeEntries();
    if (selRow < 1 || selRow >= static_cast<int>(ents.size())) return;
    const Entry& e = ents[static_cast<std::size_t>(selRow)];
    if (e.dir) return;
    const std::string p = fullPathFor(activePath(), e);
    FieldExtensionMap::launchFile(ram, p.c_str());
    close();
}

inline void enterSel(std::uint8_t* ram) noexcept {
    auto& ents = activeEntries();
    std::string& p = activePath();
    if (selRow < 0 || selRow >= static_cast<int>(ents.size())) return;
    const Entry& e = ents[static_cast<std::size_t>(selRow)];
    if (e.name == "..") {
        if (p.size() > 3) {
            const auto slash = p.find_last_of('\\', p.size() - 2);
            if (slash != std::string::npos) p = p.substr(0, slash + 1);
            else p = p.substr(0, 3);
        }
    } else if (e.dir) {
        if (p.back() != '\\') p += '\\';
        p += e.name;
    } else {
        execSel(ram);
        return;
    }
    refresh();
}

inline bool hitPaneRow(int row, int col, int& outIndex) noexcept {
    if (row < PANEL_ROW0 + 1 || row >= PANEL_ROW1) return false;
    const int r = row - (PANEL_ROW0 + 1);
    const int ei = scrollTop + r;
    if (pane == 0) {
        if (col < COL_L0 + 1 || col >= COL_L1) return false;
    } else {
        if (col < COL_R0 + 1 || col >= COL_R1) return false;
    }
    outIndex = ei;
    return true;
}

inline void copyAcrossPanes() noexcept {
    auto& ents = activeEntries();
    if (selRow < 1 || selRow >= static_cast<int>(ents.size())) return;
    const Entry& e = ents[static_cast<std::size_t>(selRow)];
    if (e.dir) return;
    std::string srcPath = activePath();
    if (srcPath.back() != '\\') srcPath += '\\';
    srcPath += e.name;
    if (pane == 0) {
        std::vector<std::uint8_t> data;
        if (!FieldAmmoVfs::readPath(srcPath.c_str(), data) || data.empty()) return;
        FieldDrives::writeHostBridgeFile(e.name.c_str(), data.data(), data.size());
    } else {
        std::vector<std::uint8_t> data;
        if (!FieldDrives::readHostBridgeFile(e.name.c_str(), data) || data.empty()) return;
        std::string dst = pathL;
        if (dst.back() != '\\') dst += '\\';
        dst += e.name;
        FieldAmmoVfs::writePath(dst.c_str(), data.data(), data.size());
        FieldRtxVfs::vfsReload();
    }
    refresh();
}

inline void deleteSel() noexcept {
    if (pane != 0) return;
    auto& ents = activeEntries();
    if (selRow < 1 || selRow >= static_cast<int>(ents.size())) return;
    const Entry& e = ents[static_cast<std::size_t>(selRow)];
    if (e.dir) return;
    std::string p = pathL;
    if (p.back() != '\\') p += '\\';
    p += e.name;
    FieldAmmoVfs::deletePath(p.c_str());
    FieldRtxVfs::vfsReload();
    refresh();
}

inline void paintPane(std::uint8_t* ram, int c0, int c1, const std::string& title,
                      const std::vector<Entry>& ents, bool focus) noexcept {
    const std::uint8_t frameAttr = focus ? FieldRtxGui::ATTR_TITLE : FieldRtxGui::ATTR_FRAME;
    char hdr[40];
    std::snprintf(hdr, sizeof hdr, " %s ", title.c_str());
    FieldRtxGui::drawFrame(ram, PANEL_ROW0, c0, PANEL_ROW1, c1, frameAttr, hdr);
    for (int r = 0; r < ROWS_VIS; ++r) {
        const int row = PANEL_ROW0 + 1 + r;
        const int ei = scrollTop + r;
        FieldRtxGui::fill(ram, row, ' ', FieldRtxGui::ATTR_EDITOR);
        if (ei >= 0 && ei < static_cast<int>(ents.size())) {
            const Entry& e = ents[static_cast<std::size_t>(ei)];
            const bool sel = focus && ei == selRow;
            const std::uint8_t attr = sel ? FieldRtxGui::ATTR_MENU_SEL : e.attr;
            char line[36];
            if (e.dir)
                std::snprintf(line, sizeof line, " %-12s <DIR>", e.name.c_str());
            else
                std::snprintf(line, sizeof line, " %-12s %8u", e.name.c_str(), e.size);
            FieldRtxGui::text(ram, row, c0 + 1, line, attr, c1 - c0 - 2);
        }
    }
}

inline void formatFooter(char* buf, std::size_t len) noexcept {
    if (!buf || len < 8u) return;
    const int total = static_cast<int>(activeEntries().size());
    const Entry* sel = (selRow >= 0 && selRow < total)
        ? &activeEntries()[static_cast<std::size_t>(selRow)] : nullptr;
    if (sel && sel->desc)
        std::snprintf(buf, len, " %s | F4 run F6 extmap ", sel->desc);
    else
        std::snprintf(buf, len, " %d files row %d/%d | F4 run F6 extmap ",
            total, selRow + 1, total);
}

inline void paint(std::uint8_t* ram) noexcept {
    if (!active) return;
    FieldRtxGui::initTextMode(ram);
    FieldRtxGui::drawFrame(ram, 2, 0, 20, 79, FieldRtxGui::ATTR_RTX,
        " Field Commander — AmmoFiles | dbl-click | right-click | F4 run ");
    paintPane(ram, COL_L0, COL_L1, pathL, entriesL, pane == 0);
    paintPane(ram, COL_R0, COL_R1, pathR, entriesR, pane == 1);
    FieldRtxGui::text(ram, 20, 2,
        " Mouse: click select/dbl-run  Wheel: scroll  F4: run  F6: extension map editor ",
        FieldRtxGui::ATTR_DIM, 76);
    for (int row = 21; row < 25; ++row)
        for (int col = 0; col < 80; ++col)
            FieldRtxGui::put(ram, row, col, ' ', 0x07);
    if (ctxOpen) {
        static const char* kItems[] = {
            " Open", " Copy", " Paste", " Delete", " Properties"
        };
        const int nItems = 5;
        const int mRow0 = std::clamp(ctxMenuRow, 2, 18);
        const int mCol0 = std::clamp(ctxCol, 2, 60);
        FieldRtxGui::drawFrame(ram, mRow0, mCol0, mRow0 + nItems + 1, mCol0 + 22,
            FieldRtxGui::ATTR_MENU, " Actions ");
        for (int i = 0; i < nItems; ++i)
            FieldRtxGui::text(ram, mRow0 + 1 + i, mCol0 + 1, kItems[i],
                i == ctxSel ? FieldRtxGui::ATTR_MENU_SEL : FieldRtxGui::ATTR_EDITOR, 20);
    }
    const int total = static_cast<int>(activeEntries().size());
    if (total > ROWS_VIS) {
        const int thumb = scrollTop * (ROWS_VIS - 1) / std::max(1, total - ROWS_VIS);
        for (int r = 0; r < ROWS_VIS; ++r)
            FieldRtxGui::put(ram, PANEL_ROW0 + 1 + r,
                pane == 0 ? COL_L1 : COL_R1, r == thumb ? '\xDB' : '\xB0',
                FieldRtxGui::ATTR_GOLD);
    }
}

inline void runContextAction(std::uint8_t* ram, int action) noexcept {
    auto& ents = activeEntries();
    if (ctxRow < 0 || ctxRow >= static_cast<int>(ents.size())) return;
    const Entry& e = ents[static_cast<std::size_t>(ctxRow)];
    const std::string full = fullPathFor(activePath(), e);
    switch (action) {
    case 0: // Open
        if (e.dir) enterSel(ram);
        else execSel(ram);
        break;
    case 1: // Copy
        if (!e.dir) clipPath = full;
        break;
    case 2: // Paste
        if (!clipPath.empty() && pane == 0) {
            std::vector<std::uint8_t> data;
            if (FieldAmmoVfs::readPath(clipPath.c_str(), data) && !data.empty()) {
                const auto slash = clipPath.find_last_of('\\');
                const std::string name = slash != std::string::npos
                    ? clipPath.substr(slash + 1) : clipPath;
                std::string dst = pathL;
                if (dst.back() != '\\') dst += '\\';
                dst += name;
                FieldAmmoVfs::writePath(dst.c_str(), data.data(), data.size());
                FieldRtxVfs::vfsReload();
                refresh();
            }
        }
        break;
    case 3: // Delete
        deleteSel();
        break;
    case 4: // Properties
        break;
    default: break;
    }
    ctxOpen = false;
    paint(ram);
}

inline void scroll(int delta) noexcept {
    auto& ents = activeEntries();
    const int maxScroll = std::max(0, static_cast<int>(ents.size()) - ROWS_VIS);
    scrollTop = std::clamp(scrollTop + delta, 0, maxScroll);
    selRow = std::clamp(selRow, scrollTop, scrollTop + ROWS_VIS - 1);
}

inline void handleMouseFrame(std::uint8_t* ram) noexcept {
    if (!active) return;
    const auto mf = FieldRtxMouse::capture();
    if (mf.visible) {
        if (mf.col >= COL_L0 && mf.col < COL_L1) pane = 0;
        else if (mf.col >= COL_R0 && mf.col < COL_R1) pane = 1;
        int ei = 0;
        if (hitPaneRow(mf.row, mf.col, ei)) {
            selRow = ei;
            if (selRow < scrollTop) scrollTop = selRow;
            if (selRow >= scrollTop + ROWS_VIS) scrollTop = selRow - ROWS_VIS + 1;
            if (mf.leftClick) {
                if (ctxOpen) {
                    runContextAction(ram, ctxSel);
                } else {
                    auto& ents = activeEntries();
                    if (selRow >= 0 && selRow < static_cast<int>(ents.size()) && !ents[static_cast<std::size_t>(selRow)].dir)
                        execSel(ram);
                    else
                        enterSel(ram);
                }
            }
            if (mf.rightClick) {
                ctxOpen = true;
                ctxRow = selRow;
                ctxSel = 0;
                ctxMenuRow = mf.row;
                ctxCol = mf.col;
                paint(ram);
            }
        }
        FieldRtxMouse::paintPointer(ram, mf.col, mf.row);
    }
    if (mf.wheel != 0) scroll(mf.wheel > 0 ? -1 : 1);
}

inline bool handleKey(std::uint8_t* ram, std::uint16_t key) noexcept {
    if (!active) return false;
    switch (key) {
    case 0x011Bu:
        if (ctxOpen) { ctxOpen = false; paint(ram); return true; }
        close(); return true;
    case 0x4800u:
        if (ctxOpen && ctxSel > 0) { --ctxSel; paint(ram); return true; }
        if (ctxOpen) return true;
        if (selRow > 0) --selRow;
        if (selRow < scrollTop) scrollTop = selRow;
        paint(ram); return true;
    case 0x5000u: {
        if (ctxOpen && ctxSel < 4) { ++ctxSel; paint(ram); return true; }
        if (ctxOpen) return true;
        const int n = static_cast<int>(activeEntries().size());
        if (selRow + 1 < n) ++selRow;
        if (selRow >= scrollTop + ROWS_VIS) scrollTop = selRow - ROWS_VIS + 1;
        paint(ram); return true;
    }
    case 0x4900u:
        if (ctxOpen) return true;
        scroll(-1); if (selRow > scrollTop) --selRow; paint(ram); return true;
    case 0x5100u: scroll(1); if (selRow < scrollTop + ROWS_VIS - 1) ++selRow; paint(ram); return true;
    case 0x0F00u: pane = 1 - pane; scrollTop = 0; selRow = 0; paint(ram); return true;
    case 0x1C0Du:
        if (ctxOpen) { runContextAction(ram, ctxSel); return true; }
        enterSel(ram); paint(ram); return true;
    case 0x3E00u: execSel(ram); paint(ram); return true; // F4 run
    case 0x4000u: FieldExtensionEditor::open(); paint(ram); return true; // F6 extmap
    case 0x3D00u: {
        auto& ents = activeEntries();
        if (selRow >= 0 && selRow < static_cast<int>(ents.size()) && !ents[static_cast<std::size_t>(selRow)].dir) {
            std::string p = activePath();
            if (p.back() != '\\') p += '\\';
            p += ents[static_cast<std::size_t>(selRow)].name;
            FieldAmouranthLaunch::queue("AMMOCODE " + p);
            close();
        }
        return true;
    }
    case 0x3F00u: copyAcrossPanes(); paint(ram); return true;
    case 0x4200u: deleteSel(); paint(ram); return true;
    default: break;
    }
    paint(ram);
    return true;
}

inline bool handleWheel(int delta) noexcept {
    if (!active) return false;
    scroll(delta > 0 ? -1 : 1);
    return true;
}

} // namespace FieldAmouranthFileCmd