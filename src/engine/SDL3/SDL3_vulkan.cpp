// engine/SDL3/SDL3_vulkan.cpp
// =============================================================================
//
// Dual Licensed:
// 1. GNU General Public License v3.0 (or later) (GPL v3)
//    https://www.gnu.org/licenses/gpl-3.0.html
// 2. Commercial licensing: gzac5314@gmail.com
//
// TRUE CONSTEXPR STONEKEY v∞ — NOVEMBER 20, 2025 — APOCALYPSE FINAL v7.0
// MAIN — FULL RTX ALWAYS — VALIDATION LAYERS FORCE-DISABLED — PINK PHOTONS ETERNAL
// =============================================================================
#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;
using namespace RTX;

namespace {
    using VulkanRendererPtr = std::unique_ptr<VulkanRenderer>;
    thread_local VulkanRendererPtr g_stonekey_renderer;
}

namespace SDL3Vulkan {
    VulkanRenderer& renderer() noexcept {
        if (!g_stonekey_renderer) std::abort();
        return *g_stonekey_renderer;
    }

    void init(int w, int h) noexcept {
        if (RTX::g_ctx().device() == VK_NULL_HANDLE) std::abort();
        g_stonekey_renderer = std::make_unique<VulkanRenderer>(w, h);
        LOG_SUCCESS_CAT("VULKAN", "{}VulkanRenderer ASCENDED — FIRST LIGHT ETERNAL{}", EMERALD_GREEN, RESET);
    }

    void shutdown() noexcept {
        g_stonekey_renderer.reset();
        RTX::cleanupAll();
    }
}