// =============================================================================
// AMOURANTH RTX Engine © 2026
// main.hpp — February 02, 2026 — Renderer owns creation | AMOURANTHRTX controls
// - Splash skippable with any input
// - SDL3 high-DPI pixel size respected
// - No sleeps/idles — eternal loop
// - Renderer owns eternal light — LAS, pipeline, SBT, accumulation, sealing
// - AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include "engine/GLOBAL/SDL3.hpp"
#include "engine/GLOBAL/ELLIE.hpp"
#include "engine/GLOBAL/AMOURANTHRTX.hpp"
#include "engine/GLOBAL/OptionsMenu.hpp"
#include "engine/GLOBAL/Renderer.hpp"

#include <SDL3/SDL_vulkan.h>
#include <chrono>
#include <memory>

// Global renderer — owns eternal light and all creation
extern std::unique_ptr<VulkanRenderer> renderer;

// Sacrificial Splash — skippable with any input
static inline void showSacrificialSplash() noexcept;