// src/engine/SDL3/SDL3_vulkan.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 by Zachary Geurts <gzac5314@gmail.com>
// =============================================================================
//
// SDL3 + Vulkan RAII — FINAL CLEAN GLUE LAYER — NOV 14 2025
// • Uses RTX::Handle<T> → zero manual cleanup
// • init() creates instance + surface + device → stored in global RTX context
// • VulkanRenderer owns all shaders, pipelines, buffers
// • FIRST LIGHT GUARANTEED
// =============================================================================

#include "engine/SDL3/SDL3_vulkan.hpp"
#include "engine/Vulkan/VulkanRenderer.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"

using namespace Logging::Color;

// Global renderer
std::unique_ptr<VulkanRenderer> g_vulkanRenderer;

namespace SDL3Vulkan {

VulkanRenderer& renderer() noexcept
{
    if (!g_vulkanRenderer) {
        LOG_FATAL_CAT("VULKAN", "{}SDL3Vulkan::renderer() called before init!{}", CRIMSON_MAGENTA, RESET);
        std::abort();
    }
    return *g_vulkanRenderer;
}

void init(int w, int h) noexcept
{
    LOG_INFO_CAT("VULKAN", "{}SDL3Vulkan::init({}x{}) — creating Vulkan context via RTX::initContext{}", 
                  PLASMA_FUCHSIA, w, h, RESET);

    // This does everything: instance, surface, physical device, logical device, queues
    // Uses SDL3 window from SDL3Initializer
    RTX::initContext(nullptr, w, h);  // window passed internally via SDL3Initializer

    // Now create the real renderer — it owns all shaders, pipelines, TLAS, etc.
    g_vulkanRenderer = std::make_unique<VulkanRenderer>(w, h);

    LOG_SUCCESS_CAT("VULKAN", 
        "{}Vulkan context + renderer initialized {}x{} — Validation: {} — FIRST LIGHT ACHIEVED{}", 
        EMERALD_GREEN, w, h, 
        Options::Performance::ENABLE_VALIDATION_LAYERS ? "ON" : "OFF",
        LIME_GREEN, RESET);
}

void shutdown() noexcept
{
    LOG_INFO_CAT("VULKAN", "{}Shutting down VulkanRenderer...{}", RASPBERRY_PINK, RESET);
    g_vulkanRenderer.reset();  // RAII destroys everything

    // Global RTX context cleanup (optional — Handle<T> already cleaned)
    RTX::cleanupAll();

    LOG_SUCCESS_CAT("VULKAN", "{}Vulkan shutdown complete — all photons returned to the void{}", 
                     EMERALD_GREEN, RESET);
}

} // namespace SDL3Vulkan