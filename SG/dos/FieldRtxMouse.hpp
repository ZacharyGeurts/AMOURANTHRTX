#pragma once

// RTX GUI mouse — text-cell coords, click edges, wheel (Turbo Pascal IDE).

#include "FieldDosConfig.hpp"
#include "FieldInput.hpp"
#include "FieldRtxVgaText.hpp"

#include <algorithm>
#include <cstdint>

namespace FieldRtxMouse {

struct Frame {
    int col = 0;
    int row = 0;
    bool visible = false;
    bool leftDown = false;
    bool rightDown = false;
    bool leftClick = false;
    bool rightClick = false;
    int wheel = 0;
};

inline std::uint8_t prevButtons = 0;
inline int wheelAccum = 0;

inline Frame capture() noexcept {
    Frame f;
    const auto& m = FieldInput::state.mouse;
    if (!FieldDosConfig::cfg.mouseEnabled || !m.installed) return f;
    const int maxC = std::max(1, FieldRtxVgaText::cols()) - 1;
    const int maxR = std::max(1, FieldRtxVgaText::rows()) - 1;
    f.col = std::clamp(m.x / 8, 0, maxC);
    f.row = std::clamp(m.y / 16, 0, maxR);
    f.visible = m.x >= 0 && m.y >= 0;
    f.leftDown = (m.buttons & 1u) != 0u;
    f.rightDown = (m.buttons & 2u) != 0u;
    f.leftClick = f.leftDown && (prevButtons & 1u) == 0u;
    f.rightClick = f.rightDown && (prevButtons & 2u) == 0u;
    if (m.wheel != 0) {
        wheelAccum += m.wheel;
        if (wheelAccum >= 120) { f.wheel = 1; wheelAccum = 0; }
        else if (wheelAccum <= -120) { f.wheel = -1; wheelAccum = 0; }
    }
    prevButtons = m.buttons;
    return f;
}

inline void paintPointer(std::uint8_t* ram, int col, int row, std::uint8_t attr = 0x70) noexcept {
    if (!FieldRtxVgaText::inBounds(row, col)) return;
    const std::uint32_t off = FieldRtxVgaText::cellOffset(row, col);
    ram[off] = static_cast<std::uint8_t>(ram[off] ^ 0xFFu);
    ram[off + 1u] = attr;
}

} // namespace FieldRtxMouse