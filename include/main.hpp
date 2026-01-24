// =============================================================================
// include/main.hpp — AMOURANTH RTX © 2025
// PURE FORWARD DECLARATIONS — NO DEFINITIONS — ZERO CIRCULAR INCLUDES
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
}

// -----------------------------------------------------------------------------
// APPLICATION — FORWARD + GLOBAL POINTER
// -----------------------------------------------------------------------------
extern std::unique_ptr<Application> g_app_ptr;

[[nodiscard]] inline Application& g_app() noexcept {
    if (!g_app_ptr) [[unlikely]] {
        LOG_FATAL_CAT("MAIN", "g_app() called before initialization");
        phase9_ballerina(std::format("FATAL ERROR → {}:{}", __FILE__, __LINE__), std::source_location::current());
    }
    return *g_app_ptr;
}