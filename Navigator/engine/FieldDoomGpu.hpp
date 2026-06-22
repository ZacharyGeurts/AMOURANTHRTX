#pragma once

// GPU Field Die — Doom assets in guest RAM; host-side S1E1 raycast playthrough.

#include "FieldPlatform.hpp"
#include "FieldVga.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <vector>

namespace FieldDoomGpu {

constexpr std::uint32_t DOOM_PAL_BYTE     = 0x00090000u;
constexpr std::uint32_t DOOM_TITLE_BYTE   = 0x00090300u;
constexpr std::uint32_t DOOM_FB_BYTE      = 0x000A0000u;
constexpr std::uint32_t DOOM_STATE_BYTE   = 0x000AFA00u;
constexpr std::uint32_t DOOM_STATE_WORDS  = 16u;

constexpr std::uint32_t DOOM_MODE_TITLE = 0u;
constexpr std::uint32_t DOOM_MODE_MENU  = 1u;
constexpr std::uint32_t DOOM_MODE_PLAY  = 2u;

constexpr std::uint32_t ST_MODE   = 0u;
constexpr std::uint32_t ST_SEL    = 1u;
constexpr std::uint32_t ST_PX     = 2u;
constexpr std::uint32_t ST_PY     = 3u;
constexpr std::uint32_t ST_PA     = 4u;
constexpr std::uint32_t ST_TICK   = 5u;
constexpr std::uint32_t ST_LEVEL  = 6u;
constexpr std::uint32_t ST_DONE   = 7u;
constexpr std::uint32_t ST_FRAMES = 8u;

constexpr int MAP_W = 16;
constexpr int MAP_H = 16;
constexpr int S1E1_LEVELS = 9;

constexpr std::uint32_t KEY_UP    = 328u;
constexpr std::uint32_t KEY_DOWN  = 336u;
constexpr std::uint32_t KEY_LEFT  = 331u;
constexpr std::uint32_t KEY_RIGHT = 333u;

inline std::uint8_t* guestRam(std::uint8_t* mapped, std::size_t ramByteOffset) noexcept {
    return mapped + ramByteOffset;
}

inline std::uint32_t* stateWords(std::uint8_t* ram) noexcept {
    return reinterpret_cast<std::uint32_t*>(ram + DOOM_STATE_BYTE);
}

inline std::uint32_t doomState(std::uint8_t* ram, std::uint32_t slot) noexcept {
    return stateWords(ram)[slot];
}

inline void setDoomState(std::uint8_t* ram, std::uint32_t slot, std::uint32_t v) noexcept {
    stateWords(ram)[slot] = v;
}

struct LevelDef {
    std::uint8_t map[MAP_W * MAP_H];
    float startX, startY;
    float exitX, exitY;
};

inline void fillBorder(std::uint8_t* m) noexcept {
    for (int y = 0; y < MAP_H; ++y)
        for (int x = 0; x < MAP_W; ++x)
            m[y * MAP_W + x] = (x == 0 || y == 0 || x == MAP_W - 1 || y == MAP_H - 1) ? 1u : 0u;
}

inline LevelDef makeLevel(int variant) noexcept {
    LevelDef lv{};
    fillBorder(lv.map);
    const int v = variant % 9;
    const int laneY = 2 + (v % 5);
    const int exitRow = MAP_H - 3 - (v / 3);
    for (int x = 1; x < MAP_W - 1; ++x)
        lv.map[laneY * MAP_W + x] = 0u;
    for (int y = laneY; y <= exitRow; ++y)
        lv.map[y * MAP_W + (MAP_W - 3)] = 0u;
    for (int x = MAP_W - 4; x < MAP_W - 1; ++x)
        lv.map[exitRow * MAP_W + x] = 0u;
    lv.startX = 2.5f;
    lv.startY = static_cast<float>(laneY) + 0.5f;
    lv.exitX  = static_cast<float>(MAP_W - 3) + 0.5f;
    lv.exitY  = static_cast<float>(exitRow) + 0.5f;
    return lv;
}

inline LevelDef levelDef(int level1to9) noexcept {
    return makeLevel(level1to9 - 1);
}

inline std::filesystem::path doomGpuAssetRoot() noexcept {
    namespace fs = std::filesystem;
    const auto probe = [](const fs::path& root) {
        return fs::exists(root / "assets" / "doom_gpu" / "title320x200.bin")
            && fs::exists(root / "assets" / "doom_gpu" / "palette.bin");
    };
    if (probe(fs::current_path())) return fs::current_path();
    try {
        auto dir = fs::canonical("/proc/self/exe").parent_path();
        for (int depth = 0; depth < 8; ++depth) {
            if (probe(dir)) return dir;
            const auto parent = dir.parent_path();
            if (parent == dir) break;
            dir = parent;
        }
    } catch (...) {}
    return fs::current_path();
}

inline bool loadPaletteBytes(std::uint8_t* guestRamPtr) noexcept {
    const auto palPath = doomGpuAssetRoot() / "assets" / "doom_gpu" / "palette.bin";
    if (!std::filesystem::exists(palPath)) return false;
    std::ifstream pin(palPath, std::ios::binary);
    if (!pin) return false;
    pin.read(reinterpret_cast<char*>(FieldVga::palette), 768);
    if (!pin) return false;
    if (guestRamPtr) {
        std::memcpy(guestRamPtr + FieldPlatform::VGA_PAL_RAM_BYTE, FieldVga::palette, 768u);
        std::memcpy(guestRamPtr + DOOM_PAL_BYTE, FieldVga::palette, 768u);
    }
    return true;
}

inline bool loadAssets(void* fieldX86DieMapped, std::size_t ramByteOffset) noexcept {
    if (!fieldX86DieMapped) return false;
    const auto root = doomGpuAssetRoot();
    const auto palPath   = root / "assets" / "doom_gpu" / "palette.bin";
    const auto titlePath = root / "assets" / "doom_gpu" / "title320x200.bin";
    if (!std::filesystem::exists(palPath) || !std::filesystem::exists(titlePath))
        return false;

    std::ifstream pal(palPath, std::ios::binary);
    std::ifstream title(titlePath, std::ios::binary);
    if (!pal || !title) return false;

    auto* ram = guestRam(static_cast<std::uint8_t*>(fieldX86DieMapped), ramByteOffset);
    pal.read(reinterpret_cast<char*>(ram + DOOM_PAL_BYTE), 768);
    title.read(reinterpret_cast<char*>(ram + DOOM_TITLE_BYTE), 320 * 200);
    if (!pal || !title) return false;

    std::memset(ram + DOOM_FB_BYTE, 0, 320u * 200u);
    setDoomState(ram, ST_MODE, DOOM_MODE_TITLE);
    setDoomState(ram, ST_SEL, 0u);
    setDoomState(ram, ST_PX, 0x00018000u);
    setDoomState(ram, ST_PY, 0x00018000u);
    setDoomState(ram, ST_PA, 0u);
    setDoomState(ram, ST_TICK, 0u);
    setDoomState(ram, ST_LEVEL, 1u);
    setDoomState(ram, ST_DONE, 0u);
    setDoomState(ram, ST_FRAMES, 0u);
    return true;
}

inline bool blitTitleToGuest(std::uint8_t* guestRamPtr) noexcept {
    if (!guestRamPtr) return false;
    const auto root = doomGpuAssetRoot();
    const auto titlePath = root / "assets" / "doom_gpu" / "title320x200.bin";
    const auto palPath   = root / "assets" / "doom_gpu" / "palette.bin";
    if (!std::filesystem::exists(titlePath) || !std::filesystem::exists(palPath))
        return false;
    std::vector<std::uint8_t> title(64000u), pal(768u);
    std::ifstream tin(titlePath, std::ios::binary);
    std::ifstream pin(palPath, std::ios::binary);
    if (!tin || !pin) return false;
    tin.read(reinterpret_cast<char*>(title.data()), static_cast<std::streamsize>(title.size()));
    pin.read(reinterpret_cast<char*>(pal.data()), static_cast<std::streamsize>(pal.size()));
    if (!tin || !pin) return false;
    FieldVga::setMode(0x13u, guestRamPtr);
    std::memcpy(FieldVga::palette, pal.data(), 768u);
    std::memcpy(guestRamPtr + FieldPlatform::VGA_PAL_RAM_BYTE, pal.data(), 768u);
    std::memcpy(guestRamPtr + DOOM_PAL_BYTE, pal.data(), 768u);
    std::memcpy(guestRamPtr + DOOM_FB_BYTE, title.data(), title.size());
    guestRamPtr[0x449u] = 0x13u;
    return true;
}

// PM32 DOS/4GW title path fallback — blit shareware title into mode 13h @ A000:0.
inline bool forceTitleBlit(std::uint8_t* guestRamPtr) noexcept {
    return blitTitleToGuest(guestRamPtr);
}

inline std::uint8_t mapCell(const LevelDef& lv, int mx, int my) noexcept {
    if (mx < 0 || my < 0 || mx >= MAP_W || my >= MAP_H) return 1u;
    return lv.map[my * MAP_W + mx];
}

inline float wallHeight(int mx, int my) noexcept {
    return ((mx + my) & 1) != 0 ? 1.0f : 0.65f;
}

inline void palRgb(std::uint8_t idx, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b) noexcept {
    const std::size_t off = static_cast<std::size_t>(idx) * 3u;
    r = FieldVga::palette[off];
    g = FieldVga::palette[off + 1];
    b = FieldVga::palette[off + 2];
}

inline void writeFbPixel(std::uint8_t* ram, int x, int y, std::uint8_t idx) noexcept {
    if (x < 0 || y < 0 || x >= 320 || y >= 200) return;
    ram[DOOM_FB_BYTE + static_cast<std::size_t>(y) * 320u + static_cast<std::size_t>(x)] = idx;
}

inline std::uint8_t rayColorIdx(const LevelDef& lv, float px, float py, float pa) noexcept {
    const float fdx = std::cos(pa);
    const float fdy = std::sin(pa);
    float depth = 0.01f;
    float wallH = 0.0f;
    std::uint8_t wallCol = 96u;
    for (int step = 0; step < 96; ++step) {
        const float sx = px + fdx * depth;
        const float sy = py + fdy * depth;
        const int mx = static_cast<int>(sx);
        const int my = static_cast<int>(sy);
        if (mapCell(lv, mx, my) != 0u) {
            wallH = wallHeight(mx, my);
            wallCol = static_cast<std::uint8_t>(16 + ((mx + my) & 7) * 3);
            break;
        }
        depth += 0.04f;
    }
    if (wallH <= 0.0f) return 192u;
    const float shade = std::clamp(1.4f - depth * 0.22f, 0.25f, 1.0f);
    if (shade < 0.55f) return static_cast<std::uint8_t>(wallCol / 2u);
    return wallCol;
}

inline void renderFrame(std::uint8_t* ram, const LevelDef& lv) noexcept {
    const float px = static_cast<float>(static_cast<int>(doomState(ram, ST_PX))) / 65536.0f;
    const float py = static_cast<float>(static_cast<int>(doomState(ram, ST_PY))) / 65536.0f;
    const float pa = static_cast<float>(static_cast<int>(doomState(ram, ST_PA))) / 65536.0f * 6.2831853f;
    for (int y = 0; y < 200; ++y) {
        for (int x = 0; x < 320; ++x) {
            const float rx = (static_cast<float>(x) / 320.0f - 0.5f) * 1.2f;
            const float ry = (static_cast<float>(y) / 200.0f - 0.5f);
            std::uint8_t idx;
            if (std::abs(ry) < 0.08f)
                idx = rayColorIdx(lv, px, py, pa + rx * 0.55f);
            else if (ry < 0.0f)
                idx = 192u;
            else
                idx = 32u;
            writeFbPixel(ram, x, y, idx);
        }
    }
}

inline void beginLevel(std::uint8_t* ram, int level) noexcept {
    const LevelDef lv = levelDef(level);
    setDoomState(ram, ST_MODE, DOOM_MODE_PLAY);
    setDoomState(ram, ST_LEVEL, static_cast<std::uint32_t>(level));
    setDoomState(ram, ST_PX, static_cast<std::uint32_t>(lv.startX * 65536.0f));
    setDoomState(ram, ST_PY, static_cast<std::uint32_t>(lv.startY * 65536.0f));
    setDoomState(ram, ST_PA, 0u);
    FieldVga::setMode(0x13u, ram);
    loadPaletteBytes(ram);
    renderFrame(ram, lv);
}

inline void beginS1e1(std::uint8_t* ram) noexcept {
    if (!ram) return;
    loadPaletteBytes(ram);
    setDoomState(ram, ST_DONE, 0u);
    setDoomState(ram, ST_FRAMES, 0u);
    setDoomState(ram, ST_TICK, 0u);
    beginLevel(ram, 1);
}

inline bool episodeComplete(std::uint8_t* ram) noexcept {
    return doomState(ram, ST_DONE) != 0u;
}

inline int currentLevel(std::uint8_t* ram) noexcept {
    const int lv = static_cast<int>(doomState(ram, ST_LEVEL));
    return lv < 1 ? 1 : (lv > S1E1_LEVELS ? S1E1_LEVELS : lv);
}

inline void advanceLevel(std::uint8_t* ram) noexcept {
    const int lv = currentLevel(ram);
    if (lv >= S1E1_LEVELS) {
        setDoomState(ram, ST_DONE, 1u);
        return;
    }
    beginLevel(ram, lv + 1);
}

inline bool atExit(const LevelDef& lv, float px, float py) noexcept {
    const float dx = px - lv.exitX;
    const float dy = py - lv.exitY;
    return (dx * dx + dy * dy) < 0.85f;
}

inline void applyMovement(std::uint8_t* ram, const LevelDef& lv, std::uint32_t key) noexcept {
    float px = static_cast<float>(static_cast<int>(doomState(ram, ST_PX))) / 65536.0f;
    float py = static_cast<float>(static_cast<int>(doomState(ram, ST_PY))) / 65536.0f;
    float pa = static_cast<float>(static_cast<int>(doomState(ram, ST_PA))) / 65536.0f * 6.2831853f;
    constexpr float spd = 0.18f;
    constexpr float rot = 0.20f;
    if (key == KEY_LEFT) pa -= rot;
    if (key == KEY_RIGHT) pa += rot;
    const float dx = std::cos(pa) * spd;
    const float dy = std::sin(pa) * spd;
    if (key == KEY_UP) { px += dx; py += dy; }
    if (key == KEY_DOWN) { px -= dx; py -= dy; }
    if (mapCell(lv, static_cast<int>(px), static_cast<int>(py)) == 0u) {
        setDoomState(ram, ST_PX, static_cast<std::uint32_t>(px * 65536.0f));
        setDoomState(ram, ST_PY, static_cast<std::uint32_t>(py * 65536.0f));
    }
    while (pa < 0.0f) pa += 6.2831853f;
    while (pa >= 6.2831853f) pa -= 6.2831853f;
    setDoomState(ram, ST_PA, static_cast<std::uint32_t>(pa / 6.2831853f * 65535.0f));
    px = static_cast<float>(static_cast<int>(doomState(ram, ST_PX))) / 65536.0f;
    py = static_cast<float>(static_cast<int>(doomState(ram, ST_PY))) / 65536.0f;
    if (atExit(lv, px, py))
        advanceLevel(ram);
}

inline std::uint32_t botKey(std::uint8_t* ram) noexcept {
    if (episodeComplete(ram)) return 0u;
    const LevelDef lv = levelDef(currentLevel(ram));
    const float px = static_cast<float>(static_cast<int>(doomState(ram, ST_PX))) / 65536.0f;
    const float py = static_cast<float>(static_cast<int>(doomState(ram, ST_PY))) / 65536.0f;
    float pa = static_cast<float>(static_cast<int>(doomState(ram, ST_PA))) / 65536.0f * 6.2831853f;
    const float tx = lv.exitX - px;
    const float ty = lv.exitY - py;
    const float dist2 = tx * tx + ty * ty;
    if (dist2 < 0.6f) return KEY_UP;
    const float target = std::atan2(ty, tx);
    float diff = target - pa;
    while (diff > 3.1415926f) diff -= 6.2831853f;
    while (diff < -3.1415926f) diff += 6.2831853f;
    if (std::abs(diff) > 0.12f)
        return diff > 0.0f ? KEY_RIGHT : KEY_LEFT;
    const float nx = px + std::cos(pa) * 0.20f;
    const float ny = py + std::sin(pa) * 0.20f;
    if (mapCell(lv, static_cast<int>(nx), static_cast<int>(ny)) != 0u) {
        const std::uint32_t tick = doomState(ram, ST_TICK);
        return (tick & 8u) ? KEY_LEFT : KEY_RIGHT;
    }
    return KEY_UP;
}

inline void stepS1e1(std::uint8_t* ram, std::uint32_t key, bool bot = false) noexcept {
    if (!ram || episodeComplete(ram)) return;
    if (doomState(ram, ST_MODE) != DOOM_MODE_PLAY)
        beginS1e1(ram);
    const std::uint32_t useKey = bot ? botKey(ram) : key;
    const LevelDef lv = levelDef(currentLevel(ram));
    applyMovement(ram, lv, useKey);
    renderFrame(ram, levelDef(currentLevel(ram)));
    setDoomState(ram, ST_TICK, doomState(ram, ST_TICK) + 1u);
    setDoomState(ram, ST_FRAMES, doomState(ram, ST_FRAMES) + 1u);
}

} // namespace FieldDoomGpu