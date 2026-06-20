#pragma once

// Data-center window shell — tool panels open at half viewport width by default.
// Programs: use FieldWinApp::begin() (GPU chrome); widgets stay in FieldRtxWidgets.

#include "FieldDosViewport.hpp"
#include "FieldWmDock.hpp"
#include "FieldWmInput.hpp"
#include "OptionsMenu.hpp"

namespace FieldWmShell {

constexpr float COMPACT_LOGICAL_W = 480.f;
constexpr float COMPACT_LOGICAL_H = 320.f;

inline void applyDataCenterScale() noexcept {
    const float sc = FieldWmDock::defaultPanelScale(FieldWmDock::DATA_CENTER_WIDTH_FRAC);
    FieldDosViewport::wmPanelScale = sc;
    FieldWmInput::panelScale = sc;
    FieldWmInput::applyPanelScale();
}

inline void applyCompactViewport() noexcept {
    FieldDosViewport::setEmuViewport(COMPACT_LOGICAL_W, COMPACT_LOGICAL_H);
    FieldDosViewport::panelStretch = false;
    applyDataCenterScale();
}

inline void ensureWindowOpen() noexcept {
    ::FieldAmouranthOs::panelVisible = true;
    Options::Canvas::DosInputFocused = true;
    Options::Canvas::DosPanelStretch = false;
    Options::Canvas::ControlFlags &= ~Options::Canvas::ControlDosPanelStretch;
}

} // namespace FieldWmShell