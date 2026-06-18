#pragma once

// WM compositor — SURFACE_RAM layout and data_bus packing for GPU chrome.

#include "FieldWmChrome.hpp"
#include "FieldWmInput.hpp"

#include <algorithm>
#include <cstdint>

namespace FieldAmouranthOs {
extern bool panelVisible;
extern int focusedProgId;
bool shellChromeActive() noexcept;
} // fwd

namespace FieldWmCompositor {

constexpr int           MAX_SURFACES = FieldWmCore::MAX_WINDOWS;
constexpr std::uint32_t SURFACE_RAM  = 0x000B9000u;
constexpr int           SURF_STRIDE  = 16;

constexpr std::uint32_t BUS_SURF_COUNT_SHIFT  = 24u;
constexpr std::uint32_t BUS_SURF_FOCUS_SHIFT  = 16u;
constexpr std::uint32_t BUS_SURF_STACK_SHIFT  = 24u;
constexpr std::uint32_t BUS_SURF_FLAGS_SHIFT  = 16u;
constexpr std::uint32_t SURF_FLAG_MINIMIZED   = 1u << 0u;
constexpr std::uint32_t SURF_FLAG_FOCUSED     = 1u << 1u;
constexpr std::uint32_t SURF_FLAG_VISIBLE     = 1u << 2u;

constexpr std::uint32_t BUS_CHROME_SKIN_SHIFT = 15u;
constexpr std::uint32_t BUS_CHROME_SKIN_MASK  = 1u << BUS_CHROME_SKIN_SHIFT;

inline void writeRamU16(std::uint8_t* ram, std::uint32_t off, std::uint16_t v) noexcept {
    if (!ram) return;
    ram[off] = static_cast<std::uint8_t>(v & 0xFFu);
    ram[off + 1u] = static_cast<std::uint8_t>((v >> 8) & 0xFFu);
}

inline void packSurfaceRam(std::uint8_t* ram) noexcept {
    if (!ram) return;
    FieldWmCore::rebuildSurfaceStack(
        FieldWmInput::panelScale,
        FieldDosViewport::panelOx,
        FieldDosViewport::panelOy);
    int surfCount = 0;
    const int focusIdx = FieldWmCore::focusedStackIndex();
    for (const auto& p : FieldAmouranthOs::programs) {
        if (p.running) ++surfCount;
    }
    ram[SURFACE_RAM] = static_cast<std::uint8_t>(surfCount);
    ram[SURFACE_RAM + 1u] = static_cast<std::uint8_t>(focusIdx >= 0 ? focusIdx : 0xFFu);
    ram[SURFACE_RAM + 2u] = FieldWmCore::stackRevision;
    ram[SURFACE_RAM + 3u] = FieldAmouranthOs::panelVisible ? 1u : 0u;

    int tab = 0;
    for (const auto& p : FieldAmouranthOs::programs) {
        if (!p.running || tab >= MAX_SURFACES) continue;
        const std::uint32_t base = SURFACE_RAM + 0x10u
            + static_cast<std::uint32_t>(tab * SURF_STRIDE);
        float ox = 0.f, oy = 0.f;
        float sc = FieldWmInput::panelScale;
        const bool isFocus = p.id == FieldAmouranthOs::focusedProgId;
        const bool visible = FieldAmouranthOs::panelVisible && isFocus && !p.minimized;
        if (visible) {
            ox = FieldDosViewport::panelOx;
            oy = FieldDosViewport::panelOy;
            sc = FieldWmInput::panelScale;
        } else if (p.panelOx >= 0.f) {
            ox = p.panelOx;
            oy = p.panelOy;
            sc = p.panelScale > 0.f ? p.panelScale : FieldWmInput::panelScale;
        }
        const float pw = FieldDosViewport::panelOuterW();
        const float ph = FieldDosViewport::panelOuterH();
        writeRamU16(ram, base, static_cast<std::uint16_t>(std::clamp(ox, 0.f, 65534.f)));
        writeRamU16(ram, base + 2u, static_cast<std::uint16_t>(std::clamp(oy, 0.f, 65534.f)));
        writeRamU16(ram, base + 4u, static_cast<std::uint16_t>(std::clamp(pw, 1.f, 65535.f)));
        writeRamU16(ram, base + 6u, static_cast<std::uint16_t>(std::clamp(ph, 1.f, 65535.f)));
        writeRamU16(ram, base + 8u, static_cast<std::uint16_t>(sc * 256.f) & 0xFFFFu);
        ram[base + 10u] = static_cast<std::uint8_t>(tab);
        std::uint8_t flags = 0u;
        if (p.minimized) flags |= 1u;
        if (isFocus) flags |= 2u;
        if (visible) flags |= 4u;
        ram[base + 11u] = flags;
        ram[base + 12u] = static_cast<std::uint8_t>(tab);
        ++tab;
    }
}

inline void packCompositorBus(std::uint32_t* bus) noexcept {
    if (!bus) return;
    int surfCount = 0;
    const int focusIdx = FieldWmCore::focusedStackIndex();
    std::uint32_t flags = 0u;
    for (const auto& p : FieldAmouranthOs::programs) {
        if (p.running) ++surfCount;
    }
    if (!FieldAmouranthOs::panelVisible || surfCount <= 0) {
        bus[60] = 0u;
        bus[61] = 0u;
        return;
    }
    const FieldDosViewport::Rect win = FieldWmChrome::windowRect();
    const float csx = FieldDosViewport::coordScaleX();
    const float csy = FieldDosViewport::coordScaleY();
    const std::uint32_t rw = static_cast<std::uint32_t>(win.w() * csx) & 0xFFFFu;
    const std::uint32_t rh = static_cast<std::uint32_t>(win.h() * csy) & 0xFFFFu;
    for (const auto& p : FieldAmouranthOs::programs) {
        if (p.id == FieldAmouranthOs::focusedProgId) {
            if (p.minimized) flags |= SURF_FLAG_MINIMIZED;
            break;
        }
    }
    flags |= SURF_FLAG_FOCUSED | SURF_FLAG_VISIBLE;
    bus[60] = (static_cast<std::uint32_t>(std::min(surfCount, MAX_SURFACES))
                << BUS_SURF_COUNT_SHIFT)
            | (static_cast<std::uint32_t>(std::max(focusIdx, 0)) & 0xFFu
                << BUS_SURF_FOCUS_SHIFT)
            | rw;
    bus[61] = (static_cast<std::uint32_t>(FieldWmCore::stackRevision) << BUS_SURF_STACK_SHIFT)
            | (flags << BUS_SURF_FLAGS_SHIFT)
            | rh;
}

inline void packIntoBus(std::uint32_t* bus) noexcept {
    if (!bus || !FieldAmouranthOs::shellChromeActive()) return;
    FieldWmChrome::syncViewport(FieldWmInput::panelScale);
    bus[52] = static_cast<std::uint32_t>(FieldWmChrome::hover) & 0xFu;
    if (FieldWmInput::dragging) bus[52] |= 1u << 4;
    if (FieldWmInput::resizing) bus[52] |= 1u << 5;
    if (Options::Canvas::DosPanelStretch) bus[52] |= 1u << 6;
    if (FieldWmChrome::openMenu != FieldWmChrome::OpenMenu::None) {
        bus[52] |= 1u << 7;
        bus[52] |= (static_cast<std::uint32_t>(FieldWmChrome::openMenu) & 7u) << 8;
        if (FieldWmChrome::menuItemHover >= 0)
            bus[52] |= (static_cast<std::uint32_t>(FieldWmChrome::menuItemHover) & 0xFu) << 11;
    }
    if (FieldWmChrome::chromeSkin == FieldWmChrome::ChromeSkin::Gnome)
        bus[52] |= BUS_CHROME_SKIN_MASK;
    bus[53] = static_cast<std::uint32_t>(FieldWmInput::panelScale * 256.f) & 0xFFFFu;
    packCompositorBus(bus);
}

} // namespace FieldWmCompositor