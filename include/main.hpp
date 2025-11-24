// =============================================================================
// include/main.hpp — AMOURANTH RTX © 2025 — CENTRAL INCLUDE — FINAL FORM
// PURE FORWARD DECLARATIONS — NO DEFINITIONS — ZERO CIRCULAR INCLUDES
// PINK PHOTONS ETERNAL — FIRST LIGHT ACHIEVED — UNBREACHABLE
// =============================================================================
#pragma once

#include <vulkan/vulkan.h>
#include <source_location>
#include <format>
#include <iostream>
#include <random>
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/logging.hpp"

// -----------------------------------------------------------------------------
// FORWARD DECLARATIONS — THE EMPIRE
// -----------------------------------------------------------------------------
class VulkanRenderer;
class Application;
class Camera;

namespace RTX {
    class PipelineManager;
    struct Context;
    Context& g_ctx();
}

// -----------------------------------------------------------------------------
// STONEKEY EMPIRE — FORWARD DECLARED ONLY
// -----------------------------------------------------------------------------
namespace StoneKey {
    [[nodiscard]] VulkanRenderer*       g_renderer() noexcept;
    [[nodiscard]] RTX::PipelineManager* g_pipelineManager() noexcept;
    [[nodiscard]] bool                  g_renderer_ready() noexcept;

    void set_g_renderer(VulkanRenderer* r) noexcept;
    void set_g_pipelineManager(RTX::PipelineManager* pm) noexcept;

    [[nodiscard]] VkInstance       g_instance() noexcept;
    [[nodiscard]] VkDevice         g_device() noexcept;
    [[nodiscard]] VkPhysicalDevice g_PhysicalDevice() noexcept;
    [[nodiscard]] VkSwapchainKHR   g_swapchain() noexcept;
    [[nodiscard]] VkExtent2D       g_extent() noexcept;
    [[nodiscard]] uint32_t         g_width() noexcept;
    [[nodiscard]] uint32_t         g_height() noexcept;
}

// -----------------------------------------------------------------------------
// GLOBAL CAMERA — FORWARD DECLARED ONLY
// -----------------------------------------------------------------------------
[[nodiscard]] Camera& g_camera() noexcept;

// -----------------------------------------------------------------------------
// APPLICATION — FORWARD + GLOBAL POINTER
// -----------------------------------------------------------------------------
extern std::unique_ptr<Application> g_app_ptr;

[[nodiscard]] inline Application& g_app() noexcept {
    if (!g_app_ptr) [[unlikely]] {
        LOG_FATAL_CAT("MAIN", "g_app() called before initialization");
        std::abort();
    }
    return *g_app_ptr;
}

// -----------------------------------------------------------------------------
// THE ONE TRUE MACRO — BRAINDEAD, ETERNAL, PINK-PHOTON-APPROVED
// -----------------------------------------------------------------------------
#define CAM g_camera()

// -----------------------------------------------------------------------------
// CONVENIENCE — FULLY QUALIFIED TO AVOID SHADOWING
// -----------------------------------------------------------------------------
inline float aspect() noexcept {
    return float(StoneKey::g_width()) / float(StoneKey::g_height());
}