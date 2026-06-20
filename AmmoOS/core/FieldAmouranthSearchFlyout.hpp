#pragma once

// Start-menu file search — live query flyout (max 10 results, alphabetical).

#include "FieldAmouranthFileIndex.hpp"
#include "FieldAmouranthHudRam.hpp"
#include "FieldExtensionMap.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace FieldAmouranthSearchFlyout {

constexpr float PANEL_W     = 300.f;
constexpr float ROW_H       = 28.f;
constexpr float PAD         = 6.f;
constexpr float GAP         = 4.f;
constexpr int   QUERY_MAX   = FieldAmouranthFileIndex::SEARCH_QUERY_LEN;

inline char query[QUERY_MAX + 1]{};
inline int  hover = -1;
inline int  resultCount = 0;
inline std::vector<const FieldAmouranthFileIndex::Entry*> hits;

inline bool active() noexcept {
    return query[0] != '\0' && resultCount > 0;
}

inline void clearQuery() noexcept {
    query[0] = '\0';
    hover = -1;
    resultCount = 0;
    hits.clear();
}

inline void refresh() noexcept {
    FieldAmouranthFileIndex::ensureBuilt();
    hits.clear();
    const std::string q = FieldAmouranthFileIndex::lowerCopy(query);
    FieldAmouranthFileIndex::collectMatches(q, hits);
    resultCount = static_cast<int>(hits.size());
    if (hover >= resultCount) hover = resultCount > 0 ? 0 : -1;
}

inline void appendChar(char ch) noexcept {
    const int len = static_cast<int>(std::strlen(query));
    if (len >= QUERY_MAX) return;
    if (ch < 32 || ch == 127) return;
    query[len] = ch;
    query[len + 1] = '\0';
    refresh();
}

inline void backspace() noexcept {
    const int len = static_cast<int>(std::strlen(query));
    if (len <= 0) return;
    query[len - 1] = '\0';
    refresh();
}

inline float panelW(float scale) noexcept { return PANEL_W * scale; }
inline float rowH(float scale) noexcept { return ROW_H * scale; }
inline float pad(float scale) noexcept { return PAD * scale; }
inline float gap(float scale) noexcept { return GAP * scale; }

inline float panelH(float scale) noexcept {
    if (!active()) return 0.f;
    return pad(scale) * 2.f + rowH(scale) * static_cast<float>(resultCount);
}

inline float panelTop(float layoutH, float taskH, float scale) noexcept {
    return layoutH - taskH - panelH(scale);
}

inline float panelLeft(float menuTotalW, float scale) noexcept {
    return pad(scale) + menuTotalW + gap(scale);
}

inline int rowAt(float my, float rowY0, float rh, int count) noexcept {
    const int idx = static_cast<int>((my - rowY0) / rh);
    return (idx >= 0 && idx < count) ? idx : -1;
}

inline const char* pathForRow(int row) noexcept {
    if (row < 0 || row >= resultCount) return nullptr;
    return hits[static_cast<std::size_t>(row)]->path.c_str();
}

inline bool launchRow(int row, std::uint8_t* ram) noexcept {
    const char* path = pathForRow(row);
    if (!path) return false;
    return FieldExtensionMap::launchFile(ram, path);
}

inline void packRam(std::uint8_t* ram) noexcept {
    if (query[0]) refresh();
    FieldAmouranthFileIndex::packResultsRam(ram, query, hover, hits);
}

} // namespace FieldAmouranthSearchFlyout