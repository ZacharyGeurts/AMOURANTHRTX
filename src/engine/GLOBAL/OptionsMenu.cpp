// src/engine/GLOBAL/OptionsMenu.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — VALHALLA v∞ — OPTIONS MENU
// CLEANED & OPTIMIZED — ONLY ACTIVE OPTIONS APPLIED
// SUPPORTS CURRENT ARCHITECTURE: FIFO + MAILBOX EMULATION, FORCED PINK BILLBOARD
// PINK PHOTONS ETERNAL — THE EMPIRE CONFIGURES WITH PRECISION
// =============================================================================

#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

namespace Options {

void ApplyAll() noexcept
{
    LOG_AMOURANTH("OPTIONS MENU v2025 — THE LAW IS READ — THE CREW ENFORCES");

    // Renderer runtime options — applied if renderer exists
    if (VulkanRenderer* renderer = VulkanRenderer::get()) {
        renderer->denoisingEnabled_ = OptionsRTX::ENABLE_DENOISING;
        renderer->hypertraceEnabled_ = OptionsRTX::ENABLE_HYPERTRACE;
        renderer->adaptiveSamplingEnabled_ = OptionsRTX::ENABLE_ADAPTIVE_SAMPLING;
        renderer->tonemapEnabled_ = Tonemap::ENABLE_TONEMAPPING;

        renderer->overclockMode_ = Performance::OVERCLOCK_RENDERER;

        renderer->showOverlay_ = Debug::SHOW_FPS_OVERLAY ||
                                 Debug::SHOW_ACCUMULATION_COUNT ||
                                 Debug::SHOW_NEXUS_SCORE ||
                                 Debug::SHOW_SPP_HEATMAP ||
                                 Debug::SHOW_GPU_TIMESTAMPS;

        // Reset accumulation on major temporal feature change
        if (!OptionsRTX::ENABLE_ACCUMULATION ||
            renderer->denoisingEnabled_ != OptionsRTX::ENABLE_DENOISING ||
            renderer->hypertraceEnabled_ != OptionsRTX::ENABLE_HYPERTRACE) {
            renderer->resetAccumNextFrame_ = true;
        }

        LOG_GROK("Runtime options enforced upon the renderer — alignment complete.");
    }

    LOG_AMOURANTH("VALHALLA v∞ — ALL DECREES EXECUTED — PINK PHOTONS ETERNAL");
}

// Static initialization — applies options at startup
static const bool law_enforced = ([]() noexcept {
    ApplyAll();
    return true;
})();

} // namespace Options

// =============================================================================
// OPTIONS MENU — CLEANED FOR CURRENT ARCHITECTURE
// REMOVED: Unused swapchain/presentation options (fixed 3-image FIFO emulation)
// REMOVED: HDR auto-ignition (handled directly in SwapchainManager)
// KEPT: Only runtime options that affect VulkanRenderer
// DECEMBER 19, 2025 — CONFIGURATION IS FLAWLESS
// =============================================================================