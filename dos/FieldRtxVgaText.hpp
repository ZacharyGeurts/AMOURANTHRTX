#pragma once

// RTX wide VGA text layout — variable cols×rows guest buffer (Monaco-era consoles).

#include "FieldDosViewport.hpp"
#include "FieldPlatform.hpp"
#include "FieldVga.hpp"

#include <algorithm>
#include <cstdint>

namespace FieldRtxVgaText {

constexpr std::uint32_t VGA     = 0xB8000u;
constexpr std::uint32_t BDA_COL = 0x450u;
constexpr std::uint32_t BDA_ROW = 0x451u;

constexpr int CLASSIC_COLS = 80;
constexpr int CLASSIC_ROWS = 25;
constexpr int MONACO_COLS  = 120;
constexpr int MONACO_ROWS  = 40;

inline int cols() noexcept { return FieldDosViewport::textCols; }
inline int rows() noexcept { return FieldDosViewport::textRows; }

inline std::uint32_t cellOffset(int row, int col) noexcept {
    const int c = cols();
    return VGA + static_cast<std::uint32_t>((row * c + col) * 2u);
}

inline bool inBounds(int row, int col) noexcept {
    return row >= 0 && row < rows() && col >= 0 && col < cols();
}

inline void initMode(std::uint8_t* ram, int c, int r) noexcept {
    FieldDosViewport::textCols = std::clamp(c, 40, 200);
    FieldDosViewport::textRows = std::clamp(r, 20, 60);
    if (!ram) return;
    FieldVga::setMode(3u, ram);
    ram[0x449u] = 3u;
    ram[0x44Au] = static_cast<std::uint8_t>(FieldDosViewport::textCols & 0xFFu);
}

inline void initClassic(std::uint8_t* ram) noexcept {
    initMode(ram, CLASSIC_COLS, CLASSIC_ROWS);
}

inline void initMonaco(std::uint8_t* ram) noexcept {
    initMode(ram, MONACO_COLS, MONACO_ROWS);
}

} // namespace FieldRtxVgaText