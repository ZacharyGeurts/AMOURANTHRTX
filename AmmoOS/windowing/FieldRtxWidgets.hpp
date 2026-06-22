#pragma once

// RTX widget toolkit — GPU-rendered dark-theme panels (checkboxes, dropdowns, scrollbars).
// Packed to guest RAM; drawn by x86.comp / CANVAS.comp (no VGA text terminal).

#include "FieldAmouranthHudRam.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace FieldRtxWidgets {

constexpr std::uint32_t WIDGET_RAM       = 0x000BB000u;
constexpr std::uint32_t WIDGET_STR_POOL  = WIDGET_RAM + 0x400u;
constexpr int           MAX_WIDGETS      = 48;
constexpr int           WIDGET_STRIDE    = 16;
constexpr int           MAX_STR_POOL     = 960;

// 1024-grid row height — safe for 8x16 HUD glyphs at 4K uiSc (~56px cell).
constexpr int UI_ROW_H   = 56;
constexpr int UI_LABEL_H = 52;
constexpr int UI_TOP     = 16;

inline int uiRow(int row) noexcept { return UI_TOP + row * UI_ROW_H; }

enum class Type : std::uint8_t {
    Panel = 0, Label, Checkbox, Button, Dropdown, VScroll, Progress, Stick, Dpad
};

enum Flag : std::uint8_t {
    None = 0, Disabled = 1, Hover = 2, Focus = 4, Checked = 8
};

struct Widget {
    Type type = Type::Panel;
    std::uint8_t flags = 0;
    std::uint16_t x0 = 0, y0 = 0, x1 = 0, y1 = 0; // 0..1024 normalized grid
    std::uint8_t state = 0;
    std::uint8_t style = 0;
    std::uint16_t labelOff = 0;
    std::int8_t extra0 = 0;
    std::int8_t extra1 = 0;
};

struct Ui {
    std::vector<Widget> widgets;
    std::string strings;
    std::uint8_t appId = 0;
    int scrollTop = 0;
    int scrollMax = 0;
    int hoverId = -1;
    std::uint16_t flags = 0;

    void clear() noexcept {
        widgets.clear();
        strings.clear();
        scrollTop = scrollMax = 0;
        hoverId = -1;
        flags = 0;
    }

    static void appendAscii(std::string& out, const char* s) noexcept {
        if (!s) return;
        for (const unsigned char* p = reinterpret_cast<const unsigned char*>(s); *p; ++p) {
            const unsigned char c = *p;
            if (c < 0x20u || c >= 0x80u) continue;
            out.push_back(static_cast<char>(c));
        }
    }

    std::uint16_t addStr(const char* s) noexcept {
        if (!s || !s[0]) return 0;
        const std::uint16_t off = static_cast<std::uint16_t>(strings.size());
        appendAscii(strings, s);
        strings += '\0';
        if (strings.size() >= static_cast<std::size_t>(MAX_STR_POOL)) {
            strings.resize(static_cast<std::size_t>(MAX_STR_POOL - 1));
            strings.back() = '\0';
        }
        return off;
    }

    int add(Type t, int x0, int y0, int x1, int y1, const char* label = nullptr,
            std::uint8_t state = 0, std::uint8_t style = 0,
            std::int8_t ex0 = 0, std::int8_t ex1 = 0,
            std::uint8_t wflags = 0) noexcept {
        if (static_cast<int>(widgets.size()) >= MAX_WIDGETS) return -1;
        Widget w{};
        w.type = t;
        w.flags = wflags;
        w.x0 = static_cast<std::uint16_t>(std::clamp(x0, 0, 1024));
        w.y0 = static_cast<std::uint16_t>(std::clamp(y0, 0, 1024));
        w.x1 = static_cast<std::uint16_t>(std::clamp(x1, 0, 1024));
        w.y1 = static_cast<std::uint16_t>(std::clamp(y1, 0, 1024));
        w.state = state;
        w.style = style;
        w.labelOff = addStr(label);
        w.extra0 = ex0;
        w.extra1 = ex1;
        const int id = static_cast<int>(widgets.size());
        widgets.push_back(w);
        return id;
    }

    int panel(int x0, int y0, int x1, int y1, std::uint8_t style = 0) noexcept {
        return add(Type::Panel, x0, y0, x1, y1, nullptr, 0, style);
    }
    int label(int x0, int y0, int x1, int y1, const char* text, std::uint8_t style = 0) noexcept {
        if (y1 - y0 < UI_LABEL_H) y1 = y0 + UI_LABEL_H;
        return add(Type::Label, x0, y0, x1, y1, text, 0, style);
    }
    int checkbox(int x0, int y0, int x1, int y1, const char* text, bool on,
                 std::uint8_t wflags = 0) noexcept {
        if (y1 - y0 < UI_ROW_H) y1 = y0 + UI_ROW_H;
        if (on) wflags |= static_cast<std::uint8_t>(Flag::Checked);
        return add(Type::Checkbox, x0, y0, x1, y1, text, on ? 1u : 0u, 0, 0, 0, wflags);
    }
    int button(int x0, int y0, int x1, int y1, const char* text, int actionId = 0,
               std::uint8_t wflags = 0) noexcept {
        if (y1 - y0 < UI_ROW_H) y1 = y0 + UI_ROW_H;
        return add(Type::Button, x0, y0, x1, y1, text,
            static_cast<std::uint8_t>(actionId & 0xFF), 0, 0, 0, wflags);
    }
    int dropdown(int x0, int y0, int x1, int y1, const char* text, int sel,
                 std::uint8_t wflags = 0) noexcept {
        if (y1 - y0 < UI_ROW_H) y1 = y0 + UI_ROW_H;
        return add(Type::Dropdown, x0, y0, x1, y1, text,
            static_cast<std::uint8_t>(sel & 0xFF), 0,
            static_cast<std::int8_t>(sel), 0, wflags);
    }
    int vscroll(int x0, int y0, int x1, int y1, int top, int total) noexcept {
        scrollTop = top;
        scrollMax = total;
        return add(Type::VScroll, x0, y0, x1, y1, nullptr,
            static_cast<std::uint8_t>(std::min(top, 255)),
            static_cast<std::uint8_t>(std::min(total, 255)));
    }
    int progress(int x0, int y0, int x1, int y1, float val, const char* label = nullptr) noexcept {
        return add(Type::Progress, x0, y0, x1, y1, label,
            static_cast<std::uint8_t>(std::clamp(val, 0.f, 1.f) * 255.f));
    }
    int stick(int x0, int y0, int x1, int y1, float nx, float ny,
              const char* label = nullptr) noexcept {
        return add(Type::Stick, x0, y0, x1, y1, label, 0, 0,
            static_cast<std::int8_t>(std::clamp(nx, -1.f, 1.f) * 127.f),
            static_cast<std::int8_t>(std::clamp(ny, -1.f, 1.f) * 127.f));
    }
    int dpad(int x0, int y0, int x1, int y1, std::uint32_t mask) noexcept {
        return add(Type::Dpad, x0, y0, x1, y1, nullptr,
            static_cast<std::uint8_t>(mask & 0xFF), 0,
            static_cast<std::int8_t>((mask >> 8) & 0xFF), 0);
    }

    void pack(std::uint8_t* ram) const noexcept {
        if (!ram) return;
        std::memset(ram + WIDGET_RAM, 0, 0x800u);
        ram[WIDGET_RAM] = static_cast<std::uint8_t>(std::min(static_cast<int>(widgets.size()), MAX_WIDGETS));
        ram[WIDGET_RAM + 1u] = appId;
        ram[WIDGET_RAM + 2u] = static_cast<std::uint8_t>(scrollTop & 0xFFu);
        ram[WIDGET_RAM + 3u] = static_cast<std::uint8_t>((scrollTop >> 8) & 0xFFu);
        ram[WIDGET_RAM + 4u] = static_cast<std::uint8_t>(scrollMax & 0xFFu);
        ram[WIDGET_RAM + 5u] = static_cast<std::uint8_t>((scrollMax >> 8) & 0xFFu);
        ram[WIDGET_RAM + 6u] = static_cast<std::uint8_t>(hoverId >= 0 ? hoverId : 0xFF);
        ram[WIDGET_RAM + 7u] = static_cast<std::uint8_t>(flags & 0xFFu);
        const std::uint32_t base = WIDGET_RAM + 0x10u;
        for (std::size_t i = 0; i < widgets.size() && i < static_cast<std::size_t>(MAX_WIDGETS); ++i) {
            const auto& w = widgets[i];
            const std::uint32_t off = base + static_cast<std::uint32_t>(i * WIDGET_STRIDE);
            ram[off] = static_cast<std::uint8_t>(w.type);
            ram[off + 1u] = w.flags;
            ram[off + 2u] = static_cast<std::uint8_t>(w.x0 & 0xFFu);
            ram[off + 3u] = static_cast<std::uint8_t>((w.x0 >> 8) & 0xFFu);
            ram[off + 4u] = static_cast<std::uint8_t>(w.y0 & 0xFFu);
            ram[off + 5u] = static_cast<std::uint8_t>((w.y0 >> 8) & 0xFFu);
            ram[off + 6u] = static_cast<std::uint8_t>(w.x1 & 0xFFu);
            ram[off + 7u] = static_cast<std::uint8_t>((w.x1 >> 8) & 0xFFu);
            ram[off + 8u] = static_cast<std::uint8_t>(w.y1 & 0xFFu);
            ram[off + 9u] = static_cast<std::uint8_t>((w.y1 >> 8) & 0xFFu);
            ram[off + 10u] = w.state;
            ram[off + 11u] = w.style;
            ram[off + 12u] = static_cast<std::uint8_t>(w.labelOff & 0xFFu);
            ram[off + 13u] = static_cast<std::uint8_t>((w.labelOff >> 8) & 0xFFu);
            ram[off + 14u] = static_cast<std::uint8_t>(w.extra0);
            ram[off + 15u] = static_cast<std::uint8_t>(w.extra1);
        }
        if (!strings.empty()) {
            const std::size_t n = std::min(strings.size(), static_cast<std::size_t>(MAX_STR_POOL));
            std::memcpy(ram + WIDGET_STR_POOL, strings.data(), n);
        }
    }

    static void clearRam(std::uint8_t* ram) noexcept {
        if (!ram) return;
        std::memset(ram + WIDGET_RAM, 0, 0x800u);
    }
};

inline Ui g;

inline void clearRam(std::uint8_t* ram) noexcept {
    Ui::clearRam(ram);
    g.clear();
}

// Modern SDF declarative helpers (zero-cost wrappers over widget RAM packer).
namespace SDF {
using Ui = FieldRtxWidgets::Ui;

inline void SDFWindow(Ui& ui, std::uint16_t x0, std::uint16_t y0,
                      std::uint16_t x1, std::uint16_t y1, std::uint8_t style = 0) noexcept {
    ui.panel(x0, y0, x1, y1, style);
}

inline int SDFButton(Ui& ui, std::uint16_t x0, std::uint16_t y0,
                     std::uint16_t x1, std::uint16_t y1,
                     const char* label, int id) noexcept {
    return ui.button(x0, y0, x1, y1, label, id);
}

inline void SDFTaskbar(Ui& ui, std::uint16_t y0, std::uint16_t y1) noexcept {
    ui.panel(0, y0, 1024, y1, 2);
    ui.label(16, y0 + 8, 320, y0 + 48, "AmouranthOS", 1);
}
} // namespace SDF

} // namespace FieldRtxWidgets