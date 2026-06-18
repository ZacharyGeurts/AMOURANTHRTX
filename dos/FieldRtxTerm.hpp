#pragma once

// RTX shell terminal scrollback — variable client rect, scrollbar + log minimap.

#include "FieldDosViewport.hpp"
#include "FieldDosChrome.hpp"
#include "FieldRtxVgaText.hpp"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace FieldRtxTerm {

constexpr int MAX_LINE_COLS = 200;
constexpr int MAX_SCREEN_ROWS = 60;
constexpr float SCROLLBAR_W = 14.f;
constexpr float MINIMAP_W   = 10.f;

inline int clientC0 = 0;
inline int clientR0 = 0;
inline int clientCols = 80;
inline int clientRows = 25;

struct Line {
    std::uint8_t ch[MAX_LINE_COLS]{};
    std::uint8_t attr[MAX_LINE_COLS]{};
};

inline std::vector<Line> history;
inline Line liveScreen[MAX_SCREEN_ROWS]{};
inline int scrollOffset = 0;
inline bool liveDirty = true;
inline std::vector<std::string> commandLog;

inline int screenCols() noexcept { return std::clamp(clientCols, 1, MAX_LINE_COLS); }
inline int screenRows() noexcept {
    return std::clamp(clientRows, 1, MAX_SCREEN_ROWS);
}

inline std::uint32_t cellOffset(int row, int col) noexcept {
    return FieldRtxVgaText::cellOffset(clientR0 + row, clientC0 + col);
}

inline void setClientRect(int c0, int r0, int cols, int rows) noexcept {
    clientC0 = c0;
    clientR0 = r0;
    clientCols = std::clamp(cols, 1, MAX_LINE_COLS);
    clientRows = std::max(1, rows);
    scrollOffset = 0;
    liveDirty = true;
}

inline void appendLog(const char* text) noexcept {
    if (!text || !text[0]) return;
    std::string ln = text;
    while (!ln.empty() && (ln.back() == '\r' || ln.back() == '\n')) ln.pop_back();
    if (ln.empty()) return;
    commandLog.push_back(ln);
    if (commandLog.size() > 4096u)
        commandLog.erase(commandLog.begin(), commandLog.begin() + 512);
}

inline void appendLogLine(const std::string& ln) noexcept {
    if (ln.empty()) return;
    commandLog.push_back(ln);
    if (commandLog.size() > 4096u)
        commandLog.erase(commandLog.begin(), commandLog.begin() + 512);
}

inline int totalLines() noexcept {
    return static_cast<int>(history.size()) + screenRows();
}

inline int maxScroll() noexcept {
    return std::max(0, totalLines() - screenRows());
}

inline void readVgaRow(std::uint8_t* ram, int row, Line& out) noexcept {
    if (!ram || row < 0 || row >= screenRows()) return;
    const int w = screenCols();
    for (int c = 0; c < w; ++c) {
        const std::uint32_t off = cellOffset(row, c);
        if (off + 1u >= FieldRtxVgaText::VGA + static_cast<std::uint32_t>(FieldRtxVgaText::rows()
                * FieldRtxVgaText::cols() * 2u))
            break;
        out.ch[c] = ram[off];
        out.attr[c] = ram[off + 1u];
    }
}

inline void writeVgaRow(std::uint8_t* ram, int row, const Line& ln) noexcept {
    if (!ram || row < 0 || row >= screenRows()) return;
    const int w = screenCols();
    for (int c = 0; c < w; ++c) {
        const std::uint32_t off = cellOffset(row, c);
        ram[off] = ln.ch[c];
        ram[off + 1u] = ln.attr[c];
    }
}

inline void syncLiveFromVga(std::uint8_t* ram) noexcept {
    if (!ram) return;
    const int rows = screenRows();
    for (int r = 0; r < rows; ++r)
        readVgaRow(ram, r, liveScreen[r]);
    liveDirty = false;
}

inline void restoreLiveToVga(std::uint8_t* ram) noexcept {
    if (!ram) return;
    const int rows = screenRows();
    for (int r = 0; r < rows; ++r)
        writeVgaRow(ram, r, liveScreen[r]);
}

inline void onScrollUp(std::uint8_t* ram) noexcept {
    if (!ram) return;
    Line top{};
    readVgaRow(ram, 0, top);
    history.push_back(top);
    if (history.size() > 8192u)
        history.erase(history.begin(), history.begin() + 1024);
    if (scrollOffset > 0)
        ++scrollOffset;
    liveDirty = true;
}

inline void pinLive() noexcept {
    scrollOffset = 0;
    liveDirty = true;
}

inline void applyView(std::uint8_t* ram) noexcept {
    if (!ram) return;
    if (scrollOffset == 0) {
        if (liveDirty)
            restoreLiveToVga(ram);
        return;
    }
    if (liveDirty)
        syncLiveFromVga(ram);
    scrollOffset = std::clamp(scrollOffset, 0, maxScroll());
    const int total = totalLines();
    const int rows = screenRows();
    const int startDoc = total - rows - scrollOffset;
    for (int r = 0; r < rows; ++r) {
        Line ln{};
        const int docRow = startDoc + r;
        const int h = static_cast<int>(history.size());
        if (docRow < h)
            ln = history[static_cast<std::size_t>(docRow)];
        else if (docRow < h + rows)
            ln = liveScreen[docRow - h];
        writeVgaRow(ram, r, ln);
    }
}

inline void scrollBy(int delta) noexcept {
    scrollOffset = std::clamp(scrollOffset + delta, 0, maxScroll());
}

inline void jumpToFraction(float frac) noexcept {
    frac = std::clamp(frac, 0.f, 1.f);
    scrollOffset = static_cast<int>((1.f - frac) * static_cast<float>(maxScroll()) + 0.5f);
    scrollOffset = std::clamp(scrollOffset, 0, maxScroll());
}

inline bool contentScrollbarHit(float mx, float my, float& fracOut) noexcept {
    const FieldDosViewport::Rect cr = FieldDosViewport::contentRect();
    const float barX0 = cr.x1 - SCROLLBAR_W;
    if (mx < barX0 || mx >= cr.x1 || my < cr.y0 || my >= cr.y1)
        return false;
    fracOut = (my - cr.y0) / std::max(cr.h(), 1.f);
    return true;
}

inline bool onMouseDown(SDL_Window* window, float lx, float ly) noexcept {
    float mx = 0.f, my = 0.f;
    FieldDosChrome::pointerPixels(window, lx, ly, mx, my);
    float frac = 0.f;
    if (!contentScrollbarHit(mx, my, frac)) return false;
    jumpToFraction(frac);
    return true;
}

inline void handleWheel(int delta) noexcept {
    if (delta == 0) return;
    scrollBy(delta > 0 ? 3 : -3);
}

inline void onUserInput() noexcept {
    pinLive();
}

inline void packDataBus(std::uint32_t* bus) noexcept {
    if (!bus) return;
    const std::uint32_t layers = bus[50] & 0xFFu;
    const std::uint32_t total = static_cast<std::uint32_t>(std::min(totalLines(), 65535));
    const std::uint32_t off   = static_cast<std::uint32_t>(std::min(scrollOffset, 65535));
    bus[50] = layers | (off << 8u) | (total << 24u);
}

} // namespace FieldRtxTerm