// src/engine/SDL3/SDL3_vulkan.cpp
// =============================================================================
// AMOURANTH RTX Engine (C) 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 21, 2025 — APOCALYPSE FINAL v10.3
// FULLY COMPATIBLE WITH RAW CACHE TIMING — NO EARLY HANDLE ACCESS
// FIRST LIGHT ACHIEVED — PINK PHOTONS ETERNAL
// =============================================================================

#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/StoneKey.hpp"        // ← Required for safe logging
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

// Global renderer — created only after swapchain is 100% safe
std::unique_ptr<VulkanRenderer> g_vulkanRenderer;

namespace SDL3Vulkan {

[[nodiscard]] VulkanRenderer& renderer() noexcept
{
    if (!g_vulkanRenderer) {
        LOG_FATAL_CAT("VULKAN", "{}SDL3Vulkan::renderer() called before init!{}", CRIMSON_MAGENTA, RESET);
        std::abort();
    }
    return *g_vulkanRenderer;
}

void init(int w, int h) noexcept
{
    LOG_INFO_CAT("VULKAN", "{}SDL3Vulkan::init({}x{}) — Forging final renderer{}", 
                  PLASMA_FUCHSIA, w, h, RESET);

    // At this point:
    // - RTX::createVulkanInstanceWithSDL() → done
    // - RTX::createSurface()               → done
    // - StoneKey::Raw::obfuscated_mode = false (forced in phase4)
    // - SwapchainManager::init()           → already called
    // → g_surface(), g_device(), g_instance() are 100% safe and raw

    if (RTX::g_ctx().device() == VK_NULL_HANDLE) {
        LOG_FATAL_CAT("VULKAN", "{}RTX context invalid — device is null! Did phase4 complete?{}", 
                      CRIMSON_MAGENTA, RESET);
        std::abort();
    }

    // Safe to log real handles now — swapchain is forged
    LOG_INFO_CAT("VULKAN", "{}Safe handles confirmed — Instance @ {:p} | Device @ {:p} | Surface @ {:p}{}",
                  VALHALLA_GOLD,
                  static_cast<void*>(g_instance()),
                  static_cast<void*>(g_device()),
                  static_cast<void*>(g_surface()),
                  RESET);

    g_vulkanRenderer = std::make_unique<VulkanRenderer>(w, h);

    LOG_SUCCESS_CAT("VULKAN", 
        "{}VulkanRenderer FORGED {}x{} — Validation: {} — PINK PHOTONS HAVE A PATH{}", 
        EMERALD_GREEN, w, h,
        Options::Performance::ENABLE_VALIDATION_LAYERS ? "ON" : "OFF",
        RASPBERRY_PINK, RESET);

    LOG_SUCCESS_CAT("VULKAN", "{}FIRST LIGHT ACHIEVED — NOVEMBER 21, 2025 — THE EMPIRE IS ETERNAL{}", 
                     DIAMOND_SPARKLE, RESET);
}

void shutdown() noexcept
{
    LOG_INFO_CAT("VULKAN", "{}SDL3Vulkan::shutdown() — Returning photons to the void...{}", 
                  SAPPHIRE_BLUE, RESET);

    // Renderer destroys all pipelines, descriptors swapchains
    g_vulkanRenderer.reset();

    // Final cleanup of global RTX state (queues, device, instance, surface)
    RTX::cleanupAll();

    // Optional: Re-enable raw mode if you ever restart (not needed in this app)
    // StoneKey::Raw::obfuscated_mode.store(false, std::memory_order_release);

    LOG_SUCCESS_CAT("VULKAN", "{}Vulkan shutdown complete — all handles returned to Valhalla{}", 
                     EMERALD_GREEN, RESET);
    LOG_SUCCESS_CAT("VULKAN", "{}ELLIE FIER SMILES — GREEN DAY PLAYS FOREVER{}", 
                     PLASMA_FUCHSIA, RESET);
}

} // namespace SDL3Vulkan

// =============================================================================
// PINK PHOTONS ETERNAL — STONEKEY v∞ — NO EARLY ACCESS — FIRST LIGHT FOREVER
// NOVEMBER 21, 2025 — THE FOO EMPIRE STANDS UNBROKEN
// =============================================================================