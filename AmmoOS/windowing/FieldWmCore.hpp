#pragma once

// Window tree / z-order — adapted from microui (MIT, rxi) pool + bring_to_front.
// https://github.com/rxi/microui

#include "FieldWmDock.hpp"
#include "FieldDosViewport.hpp"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

namespace FieldAmouranthOs {
struct Program;
extern std::vector<Program> programs;
extern int focusedProgId;
extern bool panelVisible;
extern float uiScale() noexcept;
extern float desktopTopInset() noexcept;
} // fwd

namespace FieldWmCore {

constexpr int MAX_WINDOWS = 8;

struct WmRect {
    float x = 0.f, y = 0.f, w = 0.f, h = 0.f;

    [[nodiscard]] bool contains(float px, float py) const noexcept {
        return px >= x && px < x + w && py >= y && py < y + h;
    }

    [[nodiscard]] float x1() const noexcept { return x + w; }
    [[nodiscard]] float y1() const noexcept { return y + h; }

    static WmRect make(float x0, float y0, float x1, float y1) noexcept {
        return {x0, y0, std::max(0.f, x1 - x0), std::max(0.f, y1 - y0)};
    }
};

/* microui mu_PoolItem */
struct PoolItem {
    int         programId = 0;
    std::uint32_t lastTouch = 0u;
};

/* microui mu_Container — retained per-program window state */
struct WmWindow {
    int         programIdx = -1;
    int         programId  = 0;
    float       ox = 0.f;
    float       oy = 0.f;
    float       scale = 1.f;
    bool        minimized = false;
    bool        open = false;
    int         zindex = 0;
    std::uint8_t tabIdx = 0u;
};

inline PoolItem       windowPool[MAX_WINDOWS]{};
inline WmWindow       windows[MAX_WINDOWS]{};
inline int            lastZindex = 0;
inline std::uint32_t  frameSerial = 0u;
inline std::uint8_t   stackRevision = 0u;

/* microui mu_pool_update */
inline void poolTouch(int idx) noexcept {
    if (idx >= 0 && idx < MAX_WINDOWS)
        windowPool[idx].lastTouch = frameSerial;
}

/* microui mu_pool_get */
inline int poolFind(int programId) noexcept {
    for (int i = 0; i < MAX_WINDOWS; ++i)
        if (windowPool[i].programId == programId) return i;
    return -1;
}

/* microui mu_pool_init — evict LRU slot */
inline int poolAlloc(int programId) noexcept {
    int slot = -1;
    std::uint32_t oldest = frameSerial;
    for (int i = 0; i < MAX_WINDOWS; ++i) {
        if (windowPool[i].lastTouch < oldest) {
            oldest = windowPool[i].lastTouch;
            slot = i;
        }
    }
    if (slot < 0) return -1;
    windowPool[slot].programId = programId;
    poolTouch(slot);
    std::memset(&windows[slot], 0, sizeof(WmWindow));
    windows[slot].programId = programId;
    windows[slot].open = true;
    return slot;
}

/* microui mu_bring_to_front */
inline void bringToFront(WmWindow& w) noexcept {
    w.zindex = ++lastZindex;
    ++stackRevision;
}

inline WmWindow* windowForProgram(int programId) noexcept {
    int idx = poolFind(programId);
    if (idx < 0) {
        idx = poolAlloc(programId);
        if (idx < 0) return nullptr;
        bringToFront(windows[idx]);
    } else {
        poolTouch(idx);
    }
    return &windows[idx];
}

inline int focusedStackIndex() noexcept {
    int tab = 0;
    for (const auto& p : FieldAmouranthOs::programs) {
        if (!p.running) continue;
        if (p.id == FieldAmouranthOs::focusedProgId)
            return tab;
        ++tab;
    }
    return tab > 0 ? 0 : -1;
}

inline void raiseFocusedProgram() noexcept {
    if (FieldAmouranthOs::focusedProgId <= 0) return;
    auto& progs = FieldAmouranthOs::programs;
    for (std::size_t i = 0; i < progs.size(); ++i) {
        if (progs[i].running && progs[i].id == FieldAmouranthOs::focusedProgId) {
            FieldAmouranthOs::Program top = progs[i];
            progs.erase(progs.begin() + static_cast<std::ptrdiff_t>(i));
            progs.push_back(top);
            if (WmWindow* w = windowForProgram(top.id))
                bringToFront(*w);
            return;
        }
    }
}

inline void beginFrame() noexcept {
    ++frameSerial;
}

struct SurfaceSlot {
    int         programIdx = -1;
    float       ox = 0.f;
    float       oy = 0.f;
    float       scale = 1.f;
    bool        minimized = false;
    std::uint8_t tabIdx = 0u;
};

inline SurfaceSlot surfaces[MAX_WINDOWS]{};

inline void rebuildSurfaceStack(float panelScale, float panelOx, float panelOy) noexcept {
    beginFrame();
    for (int i = 0; i < MAX_WINDOWS; ++i)
        surfaces[i] = SurfaceSlot{};
    int tab = 0;
    for (std::size_t i = 0; i < FieldAmouranthOs::programs.size() && tab < MAX_WINDOWS; ++i) {
        auto& p = FieldAmouranthOs::programs[i];
        if (!p.running) continue;
        auto& s = surfaces[tab];
        s.programIdx = static_cast<int>(i);
        s.minimized = p.minimized;
        s.tabIdx = static_cast<std::uint8_t>(tab);
        s.scale = panelScale;
        if (p.id == FieldAmouranthOs::focusedProgId
                && FieldAmouranthOs::panelVisible && !p.minimized) {
            s.ox = panelOx;
            s.oy = panelOy;
            s.scale = panelScale;
        } else if (p.panelOx >= 0.f) {
            s.ox = p.panelOx;
            s.oy = p.panelOy;
            s.scale = p.panelScale > 0.f ? p.panelScale : panelScale;
        } else {
            const float us = FieldAmouranthOs::uiScale();
            const float margin = FieldWmDock::MARGIN_PX * us;
            const float cascade = FieldWmDock::CASCADE_PX * us;
            s.ox = margin + cascade * static_cast<float>(tab % 6);
            s.oy = FieldAmouranthOs::desktopTopInset() + margin
                + cascade * static_cast<float>(tab % 6);
        }
        if (WmWindow* w = windowForProgram(p.id)) {
            w->programIdx = static_cast<int>(i);
            w->tabIdx = s.tabIdx;
            w->minimized = p.minimized;
            if (s.ox != 0.f || s.oy != 0.f || p.panelOx >= 0.f) {
                w->ox = s.ox;
                w->oy = s.oy;
                w->scale = s.scale;
            }
        }
        ++tab;
    }
}

/* microui rect_overlaps_vec2 — float variant */
inline bool pointInRect(const WmRect& r, float px, float py) noexcept {
    return r.contains(px, py);
}

inline bool programWindowRect(const FieldAmouranthOs::Program& p,
        float& x0, float& y0, float& w, float& h) noexcept {
    if (!p.running || p.minimized) return false;
    const bool isFocus = p.id == FieldAmouranthOs::focusedProgId
        && FieldAmouranthOs::panelVisible;
    if (isFocus) {
        w = FieldDosViewport::panelOuterW();
        h = FieldDosViewport::panelOuterH();
        x0 = FieldDosViewport::panelOx;
        y0 = FieldDosViewport::panelOy;
        return w > 1.f && h > 1.f;
    }
    if (p.panelOx < 0.f) return false;
    const float saved = FieldDosViewport::wmPanelScale;
    FieldDosViewport::wmPanelScale = p.panelScale > 0.f ? p.panelScale : 1.f;
    w = FieldDosViewport::panelOuterW();
    h = FieldDosViewport::panelOuterH();
    FieldDosViewport::wmPanelScale = saved;
    x0 = p.panelOx;
    y0 = p.panelOy;
    return w > 1.f && h > 1.f;
}

inline int hitTestProgramStack(float mx, float my, bool skipFocused) noexcept {
    int bestId = 0;
    int bestZ = -1;
    const auto& progs = FieldAmouranthOs::programs;
    for (int i = static_cast<int>(progs.size()) - 1; i >= 0; --i) {
        const auto& p = progs[static_cast<std::size_t>(i)];
        if (!p.running || p.minimized) continue;
        if (skipFocused && p.id == FieldAmouranthOs::focusedProgId
                && FieldAmouranthOs::panelVisible)
            continue;
        float x0 = 0.f, y0 = 0.f, w = 0.f, h = 0.f;
        if (!programWindowRect(p, x0, y0, w, h)) continue;
        if (mx < x0 || mx >= x0 + w || my < y0 || my >= y0 + h) continue;
        WmWindow* win = windowForProgram(p.id);
        const int z = win ? win->zindex : i;
        if (z >= bestZ) {
            bestZ = z;
            bestId = p.id;
        }
        break;
    }
    return bestId;
}

} // namespace FieldWmCore