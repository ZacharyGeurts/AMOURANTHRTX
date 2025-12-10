#pragma once

#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"

namespace Features {

// ── RUNTIME TOGGLES — THE EMPIRE CHOOSES AT WILL
inline void toggleHypertrace() noexcept {
    if (!Options::OptionsRTX::ENABLE_HYPERTRACE) return;
    static bool enabled = false;
    enabled = !enabled;
    LOG_AMOURANTH("HYPERTRACE {}", enabled ? "IGNITED" : "EXTINGUISHED");
}

inline void toggleDenoising() noexcept {
    if (!Options::OptionsRTX::ENABLE_DENOISING) return;
    static bool enabled = true;
    enabled = !enabled;
    LOG_AMOURANTH("DENOISING {}", enabled ? "ON" : "OFF");
}

inline void toggleAccumulation() noexcept {
    static bool enabled = true;
    enabled = !enabled;
    LOG_AMOURANTH("ACCUMULATION {}", enabled ? "ON" : "OFF");
}

inline void toggleAdaptiveSampling() noexcept {
    if (!Options::OptionsRTX::ENABLE_ADAPTIVE_SAMPLING) return;
    static bool enabled = true;
    enabled = !enabled;
    LOG_AMOURANTH("ADAPTIVE SAMPLING {}", enabled ? "ON" : "OFF");
}

inline void toggleTonemap() noexcept {
    static bool enabled = true;
    enabled = !enabled;
    LOG_AMOURANTH("TONEMAP {}", enabled ? "ON" : "OFF");
}

inline void toggleEnvMap() noexcept {
    static bool enabled = true;
    enabled = !enabled;
    LOG_AMOURANTH("ENV MAP {}", enabled ? "ON" : "OFF");
}

inline void toggleBloom() noexcept {
    static bool enabled = true;
    enabled = !enabled;
    LOG_AMOURANTH("BLOOM {}", enabled ? "ON" : "OFF");
}

inline void toggleSSR() noexcept {
    static bool enabled = true;
    enabled = !enabled;
    LOG_AMOURANTH("SSR {}", enabled ? "ON" : "OFF");
}

inline void toggleVolumetrics() noexcept {
    static bool enabled = true;
    enabled = !enabled;
    LOG_AMOURANTH("VOLUMETRICS {}", enabled ? "ON" : "OFF");
}

inline void toggleGodRays() noexcept {
    static bool enabled = true;
    enabled = !enabled;
    LOG_AMOURANTH("GOD RAYS {}", enabled ? "ON" : "OFF");
}

// ── DEBUG MODES
inline void showNexus() noexcept {
    LOG_AMOURANTH("NEXUS SCORE VISUALIZER — PHOTONS EXPOSED");
}

inline void showSBT() noexcept {
    LOG_AMOURANTH("SBT DEBUG — THE ALTAR IS REVEALED");
}

inline void showTLAS() noexcept {
    LOG_AMOURANTH("TLAS VISUALIZER — THE EMPIRE'S BONES");
}

} // namespace Features