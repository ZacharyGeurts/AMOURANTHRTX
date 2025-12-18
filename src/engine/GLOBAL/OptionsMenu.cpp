// src/engine/GLOBAL/OptionsMenu.cpp
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;
using namespace RTX;

namespace Options {

void ApplyAll() noexcept
{
    LOG_AMOURANTH("OPTIONS MENU v2025 — THE LAW IS READ — THE CREW ENFORCES");

    // Present mode
    if (Performance::PREFER_MAILBOX_PRESENT) {
        SwapchainManager::setPresentMode(VK_PRESENT_MODE_MAILBOX_KHR);
        LOG_NICK("Mailbox present decreed — tear-free, low latency.");
    }
    else if (Performance::ALLOW_IMMEDIATE_PRESENT) {
        SwapchainManager::setPresentMode(VK_PRESENT_MODE_IMMEDIATE_KHR);
        LOG_NICK("Immediate present — uncapped, raw.");
    }
    else {
        SwapchainManager::setPresentMode(VK_PRESENT_MODE_FIFO_KHR);
        LOG_NICK("FIFO locked — vsync absolute.");
    }

    SwapchainManager::setMinImageCount(Performance::MAX_FRAMES_IN_FLIGHT);
    LOG_BLONDIE("Swapchain forged with {} images — smoothness eternal.", Performance::MAX_FRAMES_IN_FLIGHT);

    if (Performance::ENABLE_FRAME_PREDICTION) {
        SwapchainManager::initializeFramePacing();
        LOG_GROK("Perfect frame prediction armed.");
    }

    SwapchainManager::setShadingRate(Performance::DYNAMIC_SHADING_RATE);
    LOG_BLONDIE("Shading rate: {:.2f}x — balance divine.", Performance::DYNAMIC_SHADING_RATE);

    SwapchainManager::enableDirectDisplay(Performance::ENABLE_DIRECT_DISPLAY);
    if (Performance::ENABLE_DIRECT_DISPLAY) {
        LOG_GROK("Direct display bypass — compositor slain.");
    }

    const bool hdrActive = SwapchainManager::supportsHDR();
    LOG_AMOURANTH("HDR AUTO-IGNITION: {} ★ FIRST LIGHT ETERNAL", hdrActive ? "IGNITED" : "awaiting worthy display");

    // Apply runtime options to the living renderer
    if (VulkanRenderer* renderer = VulkanRenderer::get()) {
        renderer->denoisingEnabled_ = OptionsRTX::ENABLE_DENOISING;
        renderer->hypertraceEnabled_ = OptionsRTX::ENABLE_HYPERTRACE;
        renderer->adaptiveSamplingEnabled_ = OptionsRTX::ENABLE_ADAPTIVE_SAMPLING;
        renderer->tonemapEnabled_ = Tonemap::ENABLE_TONEMAPPING;

        renderer->overclockMode_ = Performance::OVERCLOCK_RENDERER;

        // Uncapped mode — no VSYNC enum needed
        if (Display::UNCAPPED_MODE_ACTIVE) {
            renderer->fpsTarget_ = FpsTarget::FPS_UNLIMITED;
        }

        renderer->showOverlay_ = Debug::SHOW_FPS_OVERLAY ||
                                 Debug::SHOW_ACCUMULATION_COUNT ||
                                 Debug::SHOW_NEXUS_SCORE ||
                                 Debug::SHOW_SPP_HEATMAP ||
                                 Debug::SHOW_GPU_TIMESTAMPS;

        // Reset accumulation if temporal features changed
        if (!OptionsRTX::ENABLE_ACCUMULATION ||
            renderer->hypertraceEnabled_ != OptionsRTX::ENABLE_HYPERTRACE ||
            renderer->denoisingEnabled_ != OptionsRTX::ENABLE_DENOISING) {
            renderer->resetAccumNextFrame_ = true;
        }

        LOG_GROK("Runtime options enforced upon the renderer — alignment complete.");
    }

    LOG_AMOURANTH("VALHALLA v100 — ALL DECREES EXECUTED — PINK PHOTONS ETERNAL");
}

static const bool law_enforced = ([]() noexcept {
    ApplyAll();
    return true;
})();

} // namespace Options