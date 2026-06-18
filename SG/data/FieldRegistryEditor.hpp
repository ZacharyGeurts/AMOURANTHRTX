#pragma once

// RTX Registry Editor 2026 — browse HKRTX sections, edit key/value pairs.

#include "FieldRegistry.hpp"
#include "FieldRtxGui.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace FieldRegistryEditor {

inline bool active = false;
inline int  secScroll = 0;
inline int  secSel = 0;
inline int  kvScroll = 0;
inline int  kvSel = 0;
inline int  pane = 0; // 0=sections 1=values
inline std::vector<std::string> sections;
inline std::vector<std::pair<std::string, std::string>> values;
inline std::string editBuf;
inline bool editing = false;

constexpr int ROW0 = 4;
constexpr int ROW1 = 19;
constexpr int ROWS = ROW1 - ROW0;
constexpr int COL_S0 = 1;
constexpr int COL_S1 = 24;
constexpr int COL_V0 = 26;
constexpr int COL_V1 = 78;

inline void refreshValues() noexcept {
    values.clear();
    if (secSel < 0 || secSel >= static_cast<int>(sections.size())) return;
    const std::string sec = FieldRegistry::normPath(
        sections[static_cast<std::size_t>(secSel)] == "(root)"
            ? FieldRegistry::kRoot
            : std::string(FieldRegistry::kRoot) + "\\"
              + sections[static_cast<std::size_t>(secSel)]);
    FieldRegistry::ensure();
    const auto it = FieldRegistry::hive.find(sec);
    if (it == FieldRegistry::hive.end()) return;
    for (const auto& [k, v] : it->second)
        values.emplace_back(k, v);
    std::sort(values.begin(), values.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
}

inline void refresh() noexcept {
    FieldRegistry::ensure();
    sections = FieldRegistry::listSectionNames();
    if (sections.empty()) sections.push_back("(root)");
    secSel = std::clamp(secSel, 0, static_cast<int>(sections.size()) - 1);
    refreshValues();
    kvSel = 0;
    kvScroll = 0;
}

inline void open() noexcept {
    FieldRegistry::ensure();
    active = true;
    secScroll = secSel = kvScroll = kvSel = pane = 0;
    editing = false;
    refresh();
}

inline void close() noexcept {
    if (FieldRegistry::dirty) FieldRegistry::save();
    FieldRegistry::syncExtensionMapFromRegistry();
    FieldRegistry::applyDosConfig();
    active = false;
    editing = false;
}

inline void addKey() noexcept {
    if (secSel < 0 || secSel >= static_cast<int>(sections.size())) return;
    const std::string sec = FieldRegistry::normPath(
        sections[static_cast<std::size_t>(secSel)] == "(root)"
            ? FieldRegistry::kRoot
            : std::string(FieldRegistry::kRoot) + "\\"
              + sections[static_cast<std::size_t>(secSel)]);
    FieldRegistry::setValue(sec, "NewKey", "Value");
    refreshValues();
    kvSel = static_cast<int>(values.size()) - 1;
    pane = 1;
    editing = true;
    editBuf = "Value";
}

inline void startEdit() noexcept {
    if (pane != 1 || kvSel < 0 || kvSel >= static_cast<int>(values.size())) return;
    editing = true;
    editBuf = values[static_cast<std::size_t>(kvSel)].second;
}

inline void commitEdit() noexcept {
    if (!editing || secSel < 0 || kvSel < 0 || kvSel >= static_cast<int>(values.size())) {
        editing = false;
        return;
    }
    const std::string sec = FieldRegistry::normPath(
        sections[static_cast<std::size_t>(secSel)] == "(root)"
            ? FieldRegistry::kRoot
            : std::string(FieldRegistry::kRoot) + "\\"
              + sections[static_cast<std::size_t>(secSel)]);
    FieldRegistry::setValue(sec, values[static_cast<std::size_t>(kvSel)].first, editBuf);
    FieldRegistry::journal("EDIT", sec.c_str());
    editing = false;
    refreshValues();
}

inline void paint(std::uint8_t* ram) noexcept {
    if (!active) return;
    FieldRtxGui::initTextMode(ram);
    FieldRtxGui::drawFrame(ram, 2, 0, 21, 79, FieldRtxGui::ATTR_RTX,
        " RTX Registry Editor 2026 — Tab pane | + key | F2 edit | Esc save+close ");
    FieldRtxGui::text(ram, ROW0, COL_S0, " Section", FieldRtxGui::ATTR_TITLE, 22);
    FieldRtxGui::text(ram, ROW0, COL_V0, " Key=Value", FieldRtxGui::ATTR_TITLE, 50);
    for (int r = 0; r < ROWS; ++r) {
        const int row = ROW0 + 1 + r;
        FieldRtxGui::fill(ram, row, ' ', FieldRtxGui::ATTR_EDITOR);
        const int si = secScroll + r;
        if (si >= 0 && si < static_cast<int>(sections.size())) {
            const bool sel = pane == 0 && si == secSel;
            char line[24];
            std::snprintf(line, sizeof line, " %-20s", sections[static_cast<std::size_t>(si)].c_str());
            FieldRtxGui::text(ram, row, COL_S0 + 1, line,
                sel ? FieldRtxGui::ATTR_MENU_SEL : FieldRtxGui::ATTR_EDITOR, 22);
        }
        const int vi = kvScroll + r;
        if (vi >= 0 && vi < static_cast<int>(values.size())) {
            const bool sel = pane == 1 && vi == kvSel;
            char line[52];
            if (editing && sel)
                std::snprintf(line, sizeof line, " %s=%s",
                    values[static_cast<std::size_t>(vi)].first.c_str(), editBuf.c_str());
            else
                std::snprintf(line, sizeof line, " %s=%s",
                    values[static_cast<std::size_t>(vi)].first.c_str(),
                    values[static_cast<std::size_t>(vi)].second.c_str());
            FieldRtxGui::text(ram, row, COL_V0 + 1, line,
                sel ? FieldRtxGui::ATTR_MENU_SEL : FieldRtxGui::ATTR_IDENT, 50);
        }
    }
    FieldRtxGui::text(ram, 20, 2,
        " C:\\TOOLS\\RTXREG.INI  |  Associations sync to EXTMAP on close ",
        FieldRtxGui::ATTR_DIM, 76);
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
                if (ch >= 32 && ch < 127 && editBuf.size() < 48u) editBuf += ch;
            }
            paint(ram); return true;
        }
    }
    switch (key) {
    case 0x011Bu: close(); return true;
    case 0x0F00u: pane = 1 - pane; paint(ram); return true;
    case 0x3C00u: startEdit(); paint(ram); return true;
    case 0x4800u:
        if (pane == 0) { if (secSel > 0) { --secSel; if (secSel < secScroll) secScroll = secSel; refreshValues(); } }
        else { if (kvSel > 0) --kvSel; if (kvSel < kvScroll) kvScroll = kvSel; }
        paint(ram); return true;
    case 0x5000u:
        if (pane == 0) {
            if (secSel + 1 < static_cast<int>(sections.size())) {
                ++secSel;
                if (secSel >= secScroll + ROWS) secScroll = secSel - ROWS + 1;
                refreshValues();
            }
        } else if (kvSel + 1 < static_cast<int>(values.size())) {
            ++kvSel;
            if (kvSel >= kvScroll + ROWS) kvScroll = kvSel - ROWS + 1;
        }
        paint(ram); return true;
    case 0x1C0Du: if (pane == 1) startEdit(); paint(ram); return true;
    default:
        if ((key & 0xFFu) == '+') { addKey(); paint(ram); return true; }
        break;
    }
    paint(ram);
    return true;
}

} // namespace FieldRegistryEditor