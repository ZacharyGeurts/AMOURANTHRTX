#pragma once

// AmmoFiles — Nautilus-style dual-pane browser with context menus and extension launch.

#include "FieldAmouranthLaunch.hpp"
#include "FieldEmuRomPaths.hpp"
#include "FieldExtensionMap.hpp"
#include "FieldAmmoVfs.hpp"
#include "FieldDrives.hpp"
#include "FieldDos.hpp"
#include "FieldExtensionEditor.hpp"
#include "FieldExtensionMap.hpp"
#include "FieldAosAppIdentity.hpp"
#include "FieldAosAppJournal.hpp"

#include "FieldRtxApp.hpp"
#include "FieldRtxGui.hpp"
#include "FieldRtxMouse.hpp"
#include "FieldRtxVfs.hpp"
#include "FieldAmouranthFolderView.hpp"

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

inline void captureSnapshot(char* buf, int cap) noexcept {
    if (!buf || cap <= 0) return;
    std::snprintf(buf, static_cast<std::size_t>(cap), "L=%.20s,R=%.12s,p=%d",
        pathL.c_str(), pathR.c_str(), pane);
}

inline void applyJournalSnapshot() noexcept {
    const char* snap = FieldAosAppJournal::getAppSnapshot(
        FieldAosAppIdentity::AppId::FileCmd);
    if (!snap || !snap[0]) return;
    char left[64]{};
    char right[64]{};
    int p = 0;
    if (std::sscanf(snap, "L=%63[^,],R=%63[^,],p=%d", left, right, &p) < 3) {
        if (const char* lp = std::strstr(snap, "L="))
            std::sscanf(lp + 2, "%63[^,]", left);
        if (const char* rp = std::strstr(snap, "R="))
            std::sscanf(rp + 2, "%63[^,]", right);
        if (const char* pp = std::strrchr(snap, 'p'))
            if (pp[1] == '=')
                p = std::atoi(pp + 2);
    }
    if (left[0]) pathL = left;
    if (right[0]) pathR = right;
    pane = std::clamp(p, 0, 1);
}

inline void open() noexcept {
    active = true;
    if (!FieldAosAppJournal::getAppSnapshot(FieldAosAppIdentity::AppId::FileCmd)[0]) {
        pathL = "C:\\";
        pathR = "E:\\";
    } else {
        applyJournalSnapshot();
    }
    refresh();
}

inline void openRomLibrary(FieldAmmoEmu::Kind kind) noexcept {
    active = true;
    pathL = FieldAmmoEmu::profile(kind).romDir;
    pathR = "E:\\HOST";
    pane = 0;
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
    char detail[FieldAosAppJournal::TEXT_LEN + 1];
    std::snprintf(detail, sizeof detail, ">%s", e.name.c_str());
    FieldAosAppJournal::recordLinkedAction(FieldAosAppJournal::journalProgId,
        FieldAosAppIdentity::AppId::FileCmd, "AmmoFiles", detail);
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
        std::string folder = fullPathFor(p, e);
        FieldAmouranthFolderView::show(folder.c_str());
        return;
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
        std::snprintf(buf, len, " %s | F4 launch  F6 extmap ", sel->desc);
    else
        std::snprintf(buf, len, " %d files row %d/%d | F4 launch  F6 extmap  NES/SNES/GEN quick paths ",
            total, selRow + 1, total);
}

inline void paint(std::uint8_t* ram) noexcept {
    if (!active) return;
    FieldRtxApp::begin(ram, 7u);
    auto& ui = FieldRtxApp::ui;
    using namespace FieldRtxWidgets;
    ui.panel(8, 8, 1016, 1016);
    ui.label(24, uiRow(0), 560, uiRow(0) + UI_LABEL_H, "Field Commander - AmmoFiles", 1);
    ui.button(560, uiRow(0), 640, uiRow(0) + UI_ROW_H, "NES", 60);
    ui.button(648, uiRow(0), 728, uiRow(0) + UI_ROW_H, "SNES", 61);
    ui.button(736, uiRow(0), 816, uiRow(0) + UI_ROW_H, "GEN", 62);
    ui.button(824, uiRow(0), 888, uiRow(0) + UI_ROW_H, pane == 0 ? "C:" : "E:", 50);
    ui.button(896, uiRow(0), 968, uiRow(0) + UI_ROW_H, "Run F4", 52);
    FieldAosAppIdentity::paintAbout(ui, FieldAosAppIdentity::AppId::FileCmd, uiRow(1), uiRow(4));
    FieldAosAppJournal::paintRecent(ui, FieldAosAppJournal::journalProgId, uiRow(4), uiRow(6));

    char snap[FieldAosAppJournal::SNAPSHOT_LEN + 1];
    captureSnapshot(snap, static_cast<int>(sizeof snap));
    if (snap[0])
        FieldAosAppJournal::setAppSnapshot(FieldAosAppIdentity::AppId::FileCmd, snap);

    char pathLine[96];
    std::snprintf(pathLine, sizeof pathLine, "Path: %s", activePath().c_str());
    ui.label(24, uiRow(7), 1000, uiRow(7) + UI_LABEL_H, pathLine, 0);

    auto& ents = activeEntries();
    constexpr int kVis = 14;
    FieldRtxApp::scrollMax = std::max(0, static_cast<int>(ents.size()) - kVis);
    FieldRtxApp::scrollTop = std::clamp(scrollTop, 0, FieldRtxApp::scrollMax);
    scrollTop = FieldRtxApp::scrollTop;

    int y = uiRow(8);
    for (int i = 0; i < kVis; ++i) {
        const int ei = scrollTop + i;
        if (ei >= static_cast<int>(ents.size())) break;
        const Entry& e = ents[static_cast<std::size_t>(ei)];
        char line[72];
        if (e.dir)
            std::snprintf(line, sizeof line, "%s", e.name.c_str());
        else
            std::snprintf(line, sizeof line, "%-20s  %8u", e.name.c_str(), e.size);
        const std::uint8_t selFlags = (ei == selRow)
            ? static_cast<std::uint8_t>(FieldRtxWidgets::Flag::Focus) : 0u;
        ui.button(32, y, 960, y + UI_ROW_H, line, 100 + ei, selFlags);
        y += UI_ROW_H;
    }
    ui.vscroll(992, uiRow(8), 1016, uiRow(16), scrollTop, static_cast<int>(ents.size()));

    char footer[96];
    formatFooter(footer, sizeof footer);
    ui.label(24, uiRow(17), 960, uiRow(17) + UI_LABEL_H, footer, 0);
    FieldRtxApp::finish(ram);
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
    int action = 0;
    if (FieldRtxApp::pumpMouse(ram, action)) {
        if (action == 109) { close(); return; }
        if (action == 60) { openRomLibrary(FieldAmmoEmu::Kind::Nes); }
        else if (action == 61) { openRomLibrary(FieldAmmoEmu::Kind::Snes); }
        else if (action == 62) { openRomLibrary(FieldAmmoEmu::Kind::Genesis); }
        else if (action == 50 || action == 51) {
            pane = 1 - pane;
            scrollTop = 0;
            selRow = 0;
        } else if (action == 52) {
            execSel(ram);
        } else if (action >= 100) {
            const int ei = action - 100;
            if (ei == selRow) {
                enterSel(ram);
            } else {
                selRow = ei;
                if (selRow < scrollTop) scrollTop = selRow;
                if (selRow >= scrollTop + 18) scrollTop = selRow - 17;
            }
        } else if (FieldRtxApp::pumpScroll(action)) {
            scrollTop = FieldRtxApp::scrollTop;
            selRow = std::clamp(selRow, scrollTop, scrollTop + 17);
        }
        paint(ram);
        return;
    }
    const auto mf = FieldRtxMouse::capture();
    if (mf.wheel != 0) {
        scroll(mf.wheel > 0 ? -1 : 1);
        paint(ram);
    }
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