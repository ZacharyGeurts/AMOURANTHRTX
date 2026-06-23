#pragma once

// DOS die viewport — panel rect, content rect, scaling, mouse remap, HUD stats.

#include "FieldAmmoFat.hpp"
#include "FieldDrives.hpp"
#include "FieldMscdex.hpp"
#include "FieldPlatform.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace FieldDosViewport {

constexpr float DOS_ASPECT       = 4.f / 3.f;
constexpr float DOS_TEXT_W       = 640.f;
constexpr float DOS_TEXT_H       = 400.f;  // 80×25 @ 8×16
constexpr float DOS_GFX_W        = 640.f;
constexpr float DOS_GFX_H        = 480.f;  // 320×200 @ 2× + border
constexpr float DOS_HUD_H        = 24.f;   // single-line gray status bar (Notepad-style)
constexpr int   TEXT_COLS_DEFAULT = 120;
constexpr int   TEXT_ROWS_DEFAULT = 40;

/* data_bus[54] — DOS: bit0 FAT mounted; AOS overlay (FieldAmouranthOs::packDataBus):
 *   [7:0] menu hover, [15:8] task-tab hover, [23:16] menu row count,
 *   [31:24] focused title tab index into TASKBAR_RAM
 * data_bus[60-61] — compositor header (FieldAmouranthWm::packCompositorBus); legacy low16 = W/H px */
/* data_bus[50-51] — infinite field layer strip (shader HUD) */
constexpr std::uint32_t FIELD_LAYER_VGA    = 1u << 0;
constexpr std::uint32_t FIELD_LAYER_FAT    = 1u << 1;
constexpr std::uint32_t FIELD_LAYER_MSCDEX = 1u << 2;

inline std::uint32_t activeFieldLayerMask() noexcept {
    std::uint32_t mask = FIELD_LAYER_VGA;
    if (FieldAmmoFat::mounted) mask |= FIELD_LAYER_FAT;
    if (FieldMscdex::numDrives > 0) mask |= FIELD_LAYER_MSCDEX;
    return mask;
}

inline std::uint32_t popcountLayers(std::uint32_t mask) noexcept {
    std::uint32_t n = 0u;
    while (mask != 0u) {
        n += mask & 1u;
        mask >>= 1u;
    }
    return n;
}

inline bool panelFullscreen = false;
inline bool panelStretch    = false;  /* false = postage stamp; true = fullscreen zoom */
inline bool focused         = true;
inline float winW           = 3840.f;
inline float winH           = 2160.f;
inline float windowAspect   = 16.f / 9.f;
inline float displayScale   = 1.f;
inline std::uint8_t guestMode = 3u;
inline int textCols         = TEXT_COLS_DEFAULT;
inline int textRows         = TEXT_ROWS_DEFAULT;
inline std::uint32_t chromeHover = 0u;
inline float panelOx        = 0.f;
inline float panelOy        = 0.f;
inline bool panelPositioned = false;

inline float renderW        = 3840.f;
inline float renderH        = 2160.f;
inline float fontScale      = 1.25f;
inline bool crispFont       = true;
inline bool scanlines       = false;
inline bool subpixelFont    = false; /* business terminal — no LCD fringe */
inline bool autoZoom4K      = true;
inline float scanlineMix    = 0.04f;
inline float panelGlow      = 0.08f;
inline float sharpen        = 0.55f;
inline float wmPanelScale   = 1.f;
inline float chromeTitleH   = 0.f;
inline float emuLogicalW    = 0.f;
inline float emuLogicalH    = 0.f;

inline void setEmuViewport(float w, float h) noexcept {
    emuLogicalW = std::max(w, 1.f);
    emuLogicalH = std::max(h, 1.f);
}

inline void clearEmuViewport() noexcept {
    emuLogicalW = emuLogicalH = 0.f;
}

struct Rect {
    float x0 = 0.f, y0 = 0.f, x1 = 0.f, y1 = 0.f;
    float w() const noexcept { return x1 - x0; }
    float h() const noexcept { return y1 - y0; }
    bool contains(float x, float y) const noexcept {
        return x >= x0 && y >= y0 && x < x1 && y < y1;
    }
};

inline bool isGraphicsMode() noexcept {
    return guestMode == 0x13u || guestMode == 0x0Du || guestMode == 0x12u
        || guestMode == 0xA0u || guestMode == 0xFEu
        || guestMode == 4u || guestMode == 5u || guestMode == 6u;
}

inline float dosLogicalW() noexcept {
    if (emuLogicalW > 1.f) return emuLogicalW;
    if (guestMode == 0x13u) return 640.f;
    return isGraphicsMode() ? DOS_GFX_W : static_cast<float>(textCols) * 8.f;
}

inline float dosLogicalH() noexcept {
    if (emuLogicalH > 1.f) return emuLogicalH;
    if (guestMode == 0x13u) return 400.f;
    return isGraphicsMode() ? DOS_GFX_H : static_cast<float>(textRows) * 16.f;
}

inline float stampDisplayScale() noexcept {
    if (panelStretch) return 1.f;
    if (emuLogicalW > 1.f) return 1.f;
    const float refW = std::max(winW, renderW);
    const float refH = std::max(winH, renderH);
    if (refW < 1920.f) return 1.f;
    return std::clamp(std::min(refW / 1280.f, refH / 800.f), 1.f, 3.5f);
}

inline float panelOuterW() noexcept {
    return dosLogicalW() * stampDisplayScale() * wmPanelScale;
}

inline float panelOuterH() noexcept {
    return (dosLogicalH() + DOS_HUD_H) * stampDisplayScale() * wmPanelScale;
}

inline void dockPanelTopLeft() noexcept {
    constexpr float margin = 32.f;
    panelOx = margin;
    panelOy = margin;
    panelPositioned = true;
}

inline void clampPanelPosition() noexcept {
    if (panelStretch) return;
    const float pw = panelOuterW();
    const float ph = panelOuterH();
    panelOx = std::clamp(panelOx, 0.f, std::max(0.f, winW - pw));
    panelOy = std::clamp(panelOy, 0.f, std::max(0.f, winH - ph));
}

/* Outer DOS window rect (terminal + bottom HUD). */
inline Rect panelRect() noexcept {
    if (panelStretch)
        return {0.f, 0.f, winW, winH};
    if (!panelPositioned)
        dockPanelTopLeft();
    clampPanelPosition();
    const float pw = panelOuterW();
    const float ph = panelOuterH();
    return {panelOx, panelOy, panelOx + pw, panelOy + ph};
}

/* Terminal content only — above HUD strip. */
inline Rect contentRect() noexcept {
    const Rect w = panelRect();
    return {w.x0, w.y0 + chromeTitleH, w.x1, w.y1 - DOS_HUD_H};
}

inline float computeFontScale(const Rect& content) noexcept {
    const float logicalH = isGraphicsMode() ? DOS_GFX_H : DOS_TEXT_H;
    if (!panelStretch)
        return std::clamp(stampDisplayScale(), 1.f, 12.f);
    const float sx = content.w() / DOS_GFX_W;
    const float sy = content.h() / logicalH;
    float scale = std::min(sx, sy);
    scale = std::max(1.f, scale);
    if (displayScale > 1.01f)
        scale = std::max(scale, displayScale * 0.85f);
    return std::clamp(scale, 1.f, 12.f);
}

inline void applyDesktopDefaults() noexcept {
    if (!autoZoom4K || panelPositioned) return;
    if (winW >= 2560.f || winH >= 1440.f)
        panelStretch = true;
}

inline float coordScaleX() noexcept {
    return renderW / std::max(winW, 1.f);
}

inline float coordScaleY() noexcept {
    return renderH / std::max(winH, 1.f);
}

/* Panel bounds in render framebuffer space (matches chromePointerPixels + shader viewport). */
inline Rect panelRectRender() noexcept {
    if (panelStretch)
        return {0.f, 0.f, std::max(renderW, 1.f), std::max(renderH, 1.f)};
    const Rect r = panelRect();
    const float sx = coordScaleX();
    const float sy = coordScaleY();
    return {r.x0 * sx, r.y0 * sy, r.x1 * sx, r.y1 * sy};
}

inline Rect contentRectRender() noexcept {
    const Rect w = panelRectRender();
    return {w.x0, w.y0 + chromeTitleH, w.x1, w.y1 - DOS_HUD_H};
}

inline void syncFromGuest(const std::uint8_t* ram) noexcept {
    if (!ram) return;
    guestMode = ram[0x449u];
    const std::uint8_t cols = ram[0x44Au];
    if (cols >= 40u && cols <= 200u) textCols = static_cast<int>(cols);
    if (guestMode == 0x13u || guestMode == 0x0Du || guestMode == 0xA0u
        || guestMode == 4u || guestMode == 5u || guestMode == 6u
        || guestMode == 0x12u || guestMode == 0xFEu) {
        textCols = 80;
        textRows = 25;
    }
}

inline void mapMouse(float mx, float my, std::int32_t& outX, std::int32_t& outY) noexcept {
    const Rect r = contentRect();
    if (!focused || !r.contains(mx, my)) {
        outX = -1;
        outY = -1;
        return;
    }
    const float lx = (mx - r.x0) / std::max(r.w(), 1.f);
    const float ly = (my - r.y0) / std::max(r.h(), 1.f);
    const float lw = dosLogicalW();
    const float lh = dosLogicalH();
    const float ax = lx * lw;
    const float ay = ly * lh;
    if (isGraphicsMode()) {
        float gw = 320.f, gh = 200.f, maxX = 319.f, maxY = 199.f;
        if (guestMode == 0x12u || guestMode == 0xFEu) {
            gw = 640.f; gh = 480.f; maxX = 639.f; maxY = 479.f;
        }
        outX = static_cast<std::int32_t>(std::clamp(ax * gw / DOS_GFX_W, 0.f, maxX));
        outY = static_cast<std::int32_t>(std::clamp(ay * gh / DOS_GFX_H, 0.f, maxY));
    } else {
        outX = static_cast<std::int32_t>(std::clamp(ax, 0.f, lw - 1.f));
        outY = static_cast<std::int32_t>(std::clamp(ay, 0.f, lh - 1.f));
    }
}

inline void packHudStats(const std::uint8_t* ram, std::uint32_t* bus,
                          std::size_t busCount) noexcept {
    if (!bus) return;
    std::uint32_t convKb = 640u;
    if (ram) {
        convKb = static_cast<std::uint32_t>(ram[0x413u])
               | (static_cast<std::uint32_t>(ram[0x414u]) << 8u);
        if (convKb == 0u) convKb = 640u;
    }
    constexpr std::uint32_t extMb = 4096u;
    bus[57] = (convKb & 0xFFFFu) | (extMb << 16);

    FieldDrives::refresh();
    std::uint32_t hdTotal = FieldPlatform::HD_SIZE_BYTES32;
    std::uint32_t hdFree = 0u;
    if (FieldAmmoFat::mounted) {
        hdFree = FieldAmmoFat::freeClusters()
            * static_cast<std::uint32_t>(FieldAmmoFat::geo.spc)
            * static_cast<std::uint32_t>(FieldAmmoFat::geo.bps);
    }
    bus[59] = hdFree;
    if (busCount > 63u) bus[63] = hdTotal;
}

inline void packDataBus(std::uint32_t* bus, std::size_t count,
                          const std::uint8_t* ram = nullptr) noexcept {
    if (count < 57) return;
    const Rect win = panelRect();
    const std::uint32_t layerMask = activeFieldLayerMask();
    bus[42] = (panelFullscreen ? 1u : 0u) | (panelStretch ? 2u : 0u);
    bus[43] = static_cast<std::uint32_t>(guestMode);
    bus[44] = static_cast<std::uint32_t>(textCols);
    bus[45] = static_cast<std::uint32_t>(textRows);
    const float csx = coordScaleX();
    const float csy = coordScaleY();
    bus[46] = static_cast<std::uint32_t>(win.x0 * csx);
    bus[47] = static_cast<std::uint32_t>(win.y0 * csy);
    bus[48] = static_cast<std::uint32_t>(fontScale * 256.f);
    bus[49] = (crispFont ? 1u : 0u) | (scanlines ? 2u : 0u) | (subpixelFont ? 16u : 0u)
            | ((chromeHover & 0xFu) << 4u);
    bus[50] = popcountLayers(layerMask);
    bus[51] = layerMask;
    bus[52] = static_cast<std::uint32_t>(FieldMscdex::numDrives);
    bus[53] = FieldMscdex::numDrives ? static_cast<std::uint32_t>(FieldMscdex::driveLetters[0]) : 0u;
    bus[54] = FieldAmmoFat::mounted ? 1u : 0u;
    bus[55] = static_cast<std::uint32_t>(panelGlow * 256.f);
    bus[56] = static_cast<std::uint32_t>(sharpen * 256.f);
    bus[58] = static_cast<std::uint32_t>(DOS_HUD_H);
    packHudStats(ram, bus, count);
    if (count >= 62) {
        /* FieldAmouranthWm::packIntoBus overwrites [60-61] with compositor packing when shell chrome is on. */
        bus[60] = static_cast<std::uint32_t>(win.w() * csx) & 0xFFFFu;
        bus[61] = static_cast<std::uint32_t>(win.h() * csy) & 0xFFFFu;
    }
}

} // namespace FieldDosViewport