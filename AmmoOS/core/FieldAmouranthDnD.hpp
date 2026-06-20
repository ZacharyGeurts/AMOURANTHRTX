#pragma once

// File drag-and-drop scaffold — state machine + guest RAM pack (UI not wired yet).

#include "FieldAmouranthHudRam.hpp"

#include <cstdint>
#include <cstring>
#include <string>

namespace FieldAmouranthDnD {

constexpr std::uint32_t DND_RAM = FieldAmouranthHudRam::DND_RAM;
constexpr int DND_PATH_LEN = FieldAmouranthHudRam::DND_PATH_LEN;

enum class Phase : std::uint8_t {
    Idle = 0,
    Dragging = 1,
    Dropped = 2,
};

enum class Target : std::uint8_t {
    None = 0,
    Desktop = 1,
    TaskbarTab = 2,
    FileCmdPane = 3,
    DosPanel = 4,
};

inline Phase  phase = Phase::Idle;
inline Target target = Target::None;
inline std::string sourcePath;
inline std::uint8_t iconSlot = 6u;
inline int sourceProgId = 0;
inline float ghostMx = 0.f;
inline float ghostMy = 0.f;
inline int targetTab = -1;

inline bool active() noexcept { return phase == Phase::Dragging; }

inline void cancel() noexcept {
    phase = Phase::Idle;
    target = Target::None;
    sourcePath.clear();
    sourceProgId = 0;
    targetTab = -1;
}

inline bool begin(const char* path, std::uint8_t icon, int progId,
                  float mx, float my) noexcept {
    if (!path || !path[0]) return false;
    sourcePath = path;
    iconSlot = icon;
    sourceProgId = progId;
    ghostMx = mx;
    ghostMy = my;
    phase = Phase::Dragging;
    target = Target::None;
    targetTab = -1;
    return true;
}

inline void motion(float mx, float my) noexcept {
    if (!active()) return;
    ghostMx = mx;
    ghostMy = my;
}

inline void end(Target t, int tabIdx = -1) noexcept {
    if (!active()) return;
    target = t;
    targetTab = tabIdx;
    phase = Phase::Dropped;
}

inline void tick() noexcept {
    if (phase == Phase::Dropped)
        cancel();
}

inline void writeRamStr(std::uint8_t* ram, std::uint32_t off, const char* s, int maxLen) noexcept {
    if (!ram) return;
    for (int i = 0; i < maxLen; ++i) {
        const char ch = (s && s[i]) ? s[i] : '\0';
        ram[off + static_cast<std::uint32_t>(i)] = static_cast<std::uint8_t>(ch);
    }
}

inline void packRam(std::uint8_t* ram) noexcept {
    if (!ram) return;
    ram[DND_RAM] = static_cast<std::uint8_t>(phase);
    ram[DND_RAM + 1u] = iconSlot;
    ram[DND_RAM + 2u] = static_cast<std::uint8_t>(target);
    ram[DND_RAM + 3u] = targetTab >= 0 ? static_cast<std::uint8_t>(targetTab) : 0xFFu;
    writeRamStr(ram, DND_RAM + 4u, sourcePath.c_str(), DND_PATH_LEN);
    const std::uint32_t mxBits = static_cast<std::uint32_t>(ghostMx);
    const std::uint32_t myBits = static_cast<std::uint32_t>(ghostMy);
    ram[DND_RAM + 68u] = static_cast<std::uint8_t>(mxBits & 0xFFu);
    ram[DND_RAM + 69u] = static_cast<std::uint8_t>((mxBits >> 8) & 0xFFu);
    ram[DND_RAM + 70u] = static_cast<std::uint8_t>((mxBits >> 16) & 0xFFu);
    ram[DND_RAM + 71u] = static_cast<std::uint8_t>((mxBits >> 24) & 0xFFu);
    ram[DND_RAM + 72u] = static_cast<std::uint8_t>(myBits & 0xFFu);
    ram[DND_RAM + 73u] = static_cast<std::uint8_t>((myBits >> 8) & 0xFFu);
    ram[DND_RAM + 74u] = static_cast<std::uint8_t>((myBits >> 16) & 0xFFu);
    ram[DND_RAM + 75u] = static_cast<std::uint8_t>((myBits >> 24) & 0xFFu);
}

} // namespace FieldAmouranthDnD