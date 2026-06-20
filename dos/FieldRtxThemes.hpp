#pragma once

// RTX windowing themes — shared VGA palette for all DOS text panels + WM bus tint.

#include "FieldRtxGui.hpp"

#include <cstdint>
#include <cstring>

namespace FieldRtxThemes {

struct Palette {
    const char* name;
    std::uint8_t menu;
    std::uint8_t menuSel;
    std::uint8_t menuHi;
    std::uint8_t editor;
    std::uint8_t status;
    std::uint8_t frame;
    std::uint8_t title;
    std::uint8_t rtx;
    std::uint8_t gold;
    std::uint8_t dim;
    std::uint8_t kw;
    std::uint8_t str;
    std::uint8_t comment;
    std::uint8_t spell;
    std::uint8_t wmRose;   // host WM title tint hint (0-255)
    std::uint8_t wmCyan;
};

inline int activeIndex = 0;

inline const Palette kPresets[] = {
    {"Classic DOS",  0x1F,0x1E,0x1E,0x1F,0x70,0x1B,0x5E,0x5D,0x1E,0x17,0x2E,0x4C,0x18,0x4E,180,90},
    {"RTX Aqua",     0x31,0x13,0x1E,0x1F,0x70,0x3B,0x5E,0x5D,0x1E,0x17,0x2E,0x4C,0x18,0x4C,72,220},
    {"Golden Era",   0x1E,0x1A,0x1C,0x1F,0x70,0x6E,0x6E,0x5C,0x6E,0x17,0x2E,0x4C,0x18,0x4E,235,180},
    {"High Contrast",0x0F,0x70,0x0E,0x0F,0xF0,0x0B,0x0E,0x0D,0x0E,0x08,0x0E,0x0C,0x08,0x4C,255,255},
    {"Dusty Midnight",0x17,0x1B,0x1F,0x17,0x70,0x1B,0x5F,0x5D,0x1F,0x08,0x2E,0x4C,0x18,0x4C, 88,168},
    {"Amouranth Rose",0x5C,0x4C,0x5E,0x5F,0x70,0x5B,0x5E,0x5D,0x5E,0x17,0x6E,0x4C,0x18,0x4E,235,72},
    {"Monaco Dark",    0x1F,0x9F,0x1E,0x1F,0x70,0x18,0x5E,0x5D,0x1E,0x08,0x9B,0x6C,0x18,0x4E, 30,120},
    {"Ammo Void",      0x07,0x70,0x08,0x07,0x70,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,  8, 40},
    {"Dark Chrome",    0x17,0x9F,0x1E,0x17,0x70,0x18,0x5E,0x5D,0x1E,0x08,0x9B,0x6C,0x18,0x4C, 45,140},
};

constexpr int kPresetCount = static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));

inline void applyPalette(const Palette& p) noexcept {
    FieldRtxGui::ATTR_MENU      = p.menu;
    FieldRtxGui::ATTR_MENU_SEL  = p.menuSel;
    FieldRtxGui::ATTR_MENU_HI   = p.menuHi;
    FieldRtxGui::ATTR_EDITOR    = p.editor;
    FieldRtxGui::ATTR_STATUS    = p.status;
    FieldRtxGui::ATTR_FRAME     = p.frame;
    FieldRtxGui::ATTR_TITLE     = p.title;
    FieldRtxGui::ATTR_RTX       = p.rtx;
    FieldRtxGui::ATTR_GOLD      = p.gold;
    FieldRtxGui::ATTR_DIM       = p.dim;
    FieldRtxGui::ATTR_KW        = p.kw;
    FieldRtxGui::ATTR_STR       = p.str;
    FieldRtxGui::ATTR_COMMENT   = p.comment;
    FieldRtxGui::ATTR_SPELL     = p.spell;
    FieldRtxGui::ATTR_IDENT     = p.editor;
    FieldRtxGui::ATTR_IMMED     = p.menu;
    FieldRtxGui::ATTR_HELP      = p.frame;
    FieldRtxGui::ATTR_WORD      = p.gold;
    FieldRtxGui::ATTR_BLOCK     = 0x0Fu;
    FieldRtxGui::ATTR_ASM       = 0x3Au;
    FieldRtxGui::ATTR_DIRECTIVE = p.frame;
    FieldRtxGui::ATTR_TYPE      = p.rtx;
    FieldRtxGui::ATTR_CURSOR    = p.status;
    FieldRtxGui::ATTR_DEBUG     = 0x2Au;
    FieldRtxGui::ATTR_BREAK     = 0x4Cu;
}

inline void applyIndex(int idx) noexcept {
    if (idx < 0 || idx >= kPresetCount) idx = 0;
    activeIndex = idx;
    applyPalette(kPresets[static_cast<std::size_t>(idx)]);
}

inline int indexByName(const char* name) noexcept {
    if (!name || !name[0]) return -1;
    for (int i = 0; i < kPresetCount; ++i) {
        if (std::strcmp(kPresets[static_cast<std::size_t>(i)].name, name) == 0)
            return i;
    }
    return -1;
}

inline void init() noexcept { applyIndex(0); }

inline void next() noexcept { applyIndex((activeIndex + 1) % kPresetCount); }

inline void packBus(std::uint32_t* bus, int slot = 62) noexcept {
    if (!bus || slot < 0) return;
    bus[slot] = static_cast<std::uint32_t>(activeIndex & 0xFFu)
        | (static_cast<std::uint32_t>(kPresets[static_cast<std::size_t>(activeIndex)].wmRose) << 8)
        | (static_cast<std::uint32_t>(kPresets[static_cast<std::size_t>(activeIndex)].wmCyan) << 16);
}

} // namespace FieldRtxThemes