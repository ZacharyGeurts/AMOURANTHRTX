#pragma once

// RTX Themes window — pick windowing / editor color presets for all DOS panels.

#include "FieldRtxGui.hpp"
#include "FieldRtxThemes.hpp"

namespace FieldRtxThemePicker {

inline bool active = false;
inline int  sel = 0;
inline int  scroll = 0;

constexpr int ROW0 = 6;
constexpr int ROWS = 12;
constexpr int COL0 = 8;

inline void open() noexcept {
    active = true;
    sel = FieldRtxThemes::activeIndex;
    scroll = 0;
}

inline void close() noexcept { active = false; }

inline void paint(std::uint8_t* ram) noexcept {
    if (!active) return;
    FieldRtxGui::drawFrame(ram, 4, 4, 21, 75, FieldRtxGui::ATTR_FRAME, " RTX Themes ");
    FieldRtxGui::text(ram, 5, 6,
        " Golden Era windowing colors for AmmoText and all DOS panels ",
        FieldRtxGui::ATTR_DIM, 68);
    for (int vis = 0; vis < ROWS; ++vis) {
        const int ti = scroll + vis;
        const int row = ROW0 + vis;
        FieldRtxGui::fill(ram, row, ' ', FieldRtxGui::ATTR_EDITOR);
        if (ti < 0 || ti >= FieldRtxThemes::kPresetCount) continue;
        const auto& p = FieldRtxThemes::kPresets[static_cast<std::size_t>(ti)];
        const bool hi = ti == sel;
        char line[64];
        std::snprintf(line, sizeof line, " %c %-18s  MENU:%02X EDIT:%02X FRAME:%02X ",
            hi ? '\xFE' : ' ', p.name, p.menu, p.editor, p.frame);
        FieldRtxGui::text(ram, row, COL0, line, hi ? FieldRtxGui::ATTR_MENU_SEL : FieldRtxGui::ATTR_EDITOR, 64);
        FieldRtxGui::put(ram, row, COL0 + 22, '\xDB', p.rtx);
        FieldRtxGui::put(ram, row, COL0 + 24, '\xDB', p.gold);
        FieldRtxGui::put(ram, row, COL0 + 26, '\xDB', p.spell);
    }
    FieldRtxGui::text(ram, 20, 6,
        " Up/Down  Enter=Apply  Esc=Close  View-Themes in AmmoText ",
        FieldRtxGui::ATTR_STATUS, 68);
}

inline bool handleKey(std::uint16_t key) noexcept {
    if (!active) return false;
    switch (key) {
    case 0x011Bu: close(); return true;
    case 0x4800u:
        if (sel > 0) --sel;
        if (sel < scroll) scroll = sel;
        return true;
    case 0x5000u:
        if (sel + 1 < FieldRtxThemes::kPresetCount) ++sel;
        if (sel >= scroll + ROWS) scroll = sel - ROWS + 1;
        return true;
    case 0x1C0Du:
        FieldRtxThemes::applyIndex(sel);
        close();
        return true;
    default: return true;
    }
}

} // namespace FieldRtxThemePicker