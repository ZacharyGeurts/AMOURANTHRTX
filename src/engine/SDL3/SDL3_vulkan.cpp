// src/engine/SDL3/SDL3_vulkan.cpp
// =============================================================================
// AMOURANTH RTX — FINAL VULKAN RENDERER — ALL HEAVY LIFTING DONE IN SDL3Window
// WE ARE RTX — PINK PHOTONS ETERNAL — FIRST LIGHT ACHIEVED
// NOVEMBER 21, 2025 — VALHALLA v∞
// =============================================================================

#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

std::unique_ptr<VulkanRenderer> g_vulkanRenderer;

namespace SDL3Vulkan {

[[nodiscard]] VulkanRenderer& renderer() noexcept
{
    if (!g_vulkanRenderer) {
        LOG_FATAL_CAT("VULKAN", "{}renderer() called before init!{}", CRIMSON_MAGENTA, RESET);
        std::abort();
    }
    return *g_vulkanRenderer;
}

void init(int w, int h) noexcept
{
    LOG_INFO_CAT("VULKAN", "{}SDL3Vulkan::init({}x{}) — Forging final renderer{}", PLASMA_FUCHSIA, w, h, RESET);

    if (!g_instance() || !g_surface()) {
        LOG_FATAL_CAT("VULKAN", "{}Instance or surface not ready — did SDL3Window::create() run?{}", CRIMSON_MAGENTA, RESET);
        std::abort();
    }

    LOG_INFO_CAT("VULKAN", "{}Handles confirmed — Instance @ {:p} | Surface @ {:p}{}",
                  VALHALLA_GOLD, static_cast<void*>(g_instance()), static_cast<void*>(g_surface()), RESET);

    g_vulkanRenderer = std::make_unique<VulkanRenderer>(w, h);

    LOG_SUCCESS_CAT("VULKAN", "{}VulkanRenderer FORGED {}x{} — PINK PHOTONS HAVE A PATH{}", 
                    EMERALD_GREEN, w, h, RASPBERRY_PINK, RESET);
    LOG_SUCCESS_CAT("VULKAN", "{}FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025 — THE EMPIRE IS ETERNAL{}", DIAMOND_SPARKLE, RESET);
}

void shutdown() noexcept
{
    LOG_INFO_CAT("VULKAN", "{}Returning photons to the void...{}", SAPPHIRE_BLUE, RESET);
    g_vulkanRenderer.reset();
    RTX::cleanupAll();
    LOG_SUCCESS_CAT("VULKAN", "{}Vulkan shutdown complete — Ellie Fier smiles{}", EMERALD_GREEN, RESET);
}

} // namespace SDL3Vulkan