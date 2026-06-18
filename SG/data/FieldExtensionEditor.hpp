#pragma once

// Extension Map editor — scrollable table, + add row, F2 save, Esc close.

#include "FieldExtensionMap.hpp"
#include "FieldRegistry.hpp"
#include "FieldRtxGui.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace FieldExtensionEditor {

inline bool active = false;
inline int  scrollTop = 0;
inline int  selRow = 0;
inline int  editField = 0; // 0=ext 1=handler 2=args 3=desc
inline std::string editBuf;
inline bool editing = false;

constexpr int ROW0 = 3;
constexpr int ROW1 = 20;
constexpr int ROWS_VIS = ROW1 - ROW0;

inline void open() noexcept {
    FieldExtensionMap::ensure();
    active = true;
    scrollTop = 0;
    selRow = 0;
    editField = 0;
    editing = false;
    editBuf.clear();
}

inline void close() noexcept {
    FieldRegistry::syncExtensionMapToRegistry();
    FieldRegistry::save();
    FieldRegistry::syncExtensionMapFromRegistry();
    active = false;
    editing = false;
}

inline void startEdit() noexcept {
    if (!active) return;
    FieldExtensionMap::ensure();
    if (selRow < 0 || selRow >= static_cast<int>(FieldExtensionMap::entries.size())) return;
    const auto& e = FieldExtensionMap::entries[static_cast<std::size_t>(selRow)];
    editing = true;
    switch (editField) {
    case 0: editBuf = e.ext; break;
    case 1: editBuf = e.handler; break;
    case 2: editBuf = e.args; break;
    default: editBuf = e.description; break;
    }
}

inline void commitEdit() noexcept {
    if (!editing) return;
    FieldExtensionMap::ensure();
    if (selRow < 0 || selRow >= static_cast<int>(FieldExtensionMap::entries.size())) {
        editing = false;
        return;
    }
    auto& e = FieldExtensionMap::entries[static_cast<std::size_t>(selRow)];
    switch (editField) {
    case 0: e.ext = editBuf; break;
    case 1: e.handler = editBuf; break;
    case 2: e.args = editBuf; break;
    default: e.description = editBuf; break;
    }
    FieldExtensionMap::save();
    editing = false;
}

inline void addRow() noexcept {
    FieldExtensionMap::ensure();
    FieldExtensionMap::Entry ne{".NEW", "", "%1", "New association"};
    FieldExtensionMap::entries.push_back(ne);
    selRow = static_cast<int>(FieldExtensionMap::entries.size()) - 1;
    editField = 0;
    startEdit();
}

inline void deleteRow() noexcept {
    FieldExtensionMap::ensure();
    if (selRow >= 0 && selRow < static_cast<int>(FieldExtensionMap::entries.size())) {
        FieldExtensionMap::removeAt(static_cast<std::size_t>(selRow));
        selRow = std::min(selRow, static_cast<int>(FieldExtensionMap::entries.size()) - 1);
    }
}

inline void paint(std::uint8_t* ram) noexcept {
    if (!active) return;
    FieldRtxGui::initTextMode(ram);
    FieldRtxGui::drawFrame(ram, 2, 0, 22, 79, FieldRtxGui::ATTR_RTX,
        " Extension Map — F2 save row | + add | Del remove | Tab field | Esc close ");
    FieldRtxGui::text(ram, ROW0, 2,
        " Ext      Handler                    Args       Description",
        FieldRtxGui::ATTR_TITLE, 76);
    FieldExtensionMap::ensure();
    const int n = static_cast<int>(FieldExtensionMap::entries.size());
    for (int r = 0; r < ROWS_VIS; ++r) {
        const int row = ROW0 + 1 + r;
        const int ei = scrollTop + r;
        FieldRtxGui::fill(ram, row, ' ', FieldRtxGui::ATTR_EDITOR);
        if (ei < 0 || ei >= n) continue;
        const auto& e = FieldExtensionMap::entries[static_cast<std::size_t>(ei)];
        const bool sel = ei == selRow;
        char line[78];
        if (editing && sel) {
            std::snprintf(line, sizeof line, " %-8s %-26s %-10s %s",
                editField == 0 ? editBuf.c_str() : e.ext.c_str(),
                editField == 1 ? editBuf.c_str() : e.handler.c_str(),
                editField == 2 ? editBuf.c_str() : e.args.c_str(),
                editField == 3 ? editBuf.c_str() : e.description.c_str());
        } else {
            std::snprintf(line, sizeof line, " %-8s %-26s %-10s %s",
                e.ext.c_str(), e.handler.c_str(), e.args.c_str(), e.description.c_str());
        }
        const std::uint8_t attr = sel ? FieldRtxGui::ATTR_MENU_SEL : FieldRtxGui::ATTR_EDITOR;
        FieldRtxGui::text(ram, row, 2, line, attr, 76);
        if (sel && !editing) {
            const int marks[] = {10, 37, 48, 76};
            FieldRtxGui::put(ram, row, marks[editField], '\xDB', FieldRtxGui::ATTR_GOLD);
        }
    }
    char st[72];
    std::snprintf(st, sizeof st, " %d associations | HKRTX\\Associations | RTXREG.INI ",
        n);
    FieldRtxGui::text(ram, 21, 2, st, FieldRtxGui::ATTR_STATUS, 76);
}

inline void scroll(int delta) noexcept {
    const int n = static_cast<int>(FieldExtensionMap::entries.size());
    const int maxScroll = std::max(0, n - ROWS_VIS);
    scrollTop = std::clamp(scrollTop + delta, 0, maxScroll);
    selRow = std::clamp(selRow, scrollTop, scrollTop + ROWS_VIS - 1);
}

inline bool handleKey(std::uint8_t* ram, std::uint16_t key) noexcept {
    if (!active) return false;
    if (editing) {
        switch (key & 0xFFu) {
        case 0x08: if (!editBuf.empty()) editBuf.pop_back(); paint(ram); return true;
        case 0x0D: commitEdit(); paint(ram); return true;
        case 0x1B: editing = false; paint(ram); return true;
        default:
            if ((key & 0xFF00u) == 0) {
                const char ch = static_cast<char>(key & 0xFFu);
                if (ch >= 32 && ch < 127 && editBuf.size() < 60u) editBuf += ch;
            }
            paint(ram); return true;
        }
    }
    switch (key) {
    case 0x011Bu: close(); return true;
    case 0x3B00u: addRow(); paint(ram); return true; // F1 -> use + key 0x2B
    case 0x3C00u: startEdit(); paint(ram); return true; // F2 edit
    case 0x5300u: deleteRow(); paint(ram); return true; // Del
    case 0x0F00u: editField = (editField + 1) % 4; paint(ram); return true;
    case 0x4900u: scroll(-1); if (selRow > scrollTop) --selRow; paint(ram); return true;
    case 0x5100u: scroll(1); paint(ram); return true;
    case 0x4800u:
        if (selRow > 0) --selRow;
        if (selRow < scrollTop) scrollTop = selRow;
        paint(ram); return true;
    case 0x5000u: {
        const int n = static_cast<int>(FieldExtensionMap::entries.size());
        if (selRow + 1 < n) ++selRow;
        if (selRow >= scrollTop + ROWS_VIS) scrollTop = selRow - ROWS_VIS + 1;
        paint(ram); return true;
    }
    case 0x1C0Du: startEdit(); paint(ram); return true;
    default:
        if ((key & 0xFFu) == '+') { addRow(); paint(ram); return true; }
        break;
    }
    paint(ram);
    return true;
}

} // namespace FieldExtensionEditor