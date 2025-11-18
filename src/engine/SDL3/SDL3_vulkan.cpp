// src/engine/SDL3/SDL3_vulkan.cpp
// =============================================================================
// AMOURANTH RTX — Vulkan Entry Point — APOCALYPSE v6.0
// =============================================================================

#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/GLOBAL/logging.hpp"

using namespace Logging::Color;

std::unique_ptr<VulkanRenderer> g_vulkanRenderer;  // Definition — NO static!

namespace SDL3Vulkan {

VulkanRenderer& renderer() noexcept {
    if (!g_vulkanRenderer) {
        LOG_FATAL_CAT("VULKAN", "SDL3Vulkan::renderer() called before init!");
        std::abort();
    }
    return *g_vulkanRenderer;
}

void init(int w, int h) noexcept {
    LOG_SUCCESS_CAT("VULKAN", "Creating VulkanRenderer {}x{}", w, h);
    g_vulkanRenderer = std::make_unique<VulkanRenderer>(w, h);
    LOG_SUCCESS_CAT("VULKAN", "VulkanRenderer READY — FIRST LIGHT ACHIEVED");
}

void shutdown() noexcept {
    g_vulkanRenderer.reset();
    LOG_SUCCESS_CAT("VULKAN", "VulkanRenderer destroyed — photons returned to void");
}

} // namespace SDL3Vulkan