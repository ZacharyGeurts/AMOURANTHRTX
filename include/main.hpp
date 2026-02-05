// =============================================================================
// AMOURANTH RTX Engine (C) 2025-2026 by Zachary Geurts <gzac5314@gmail.com>
// Dual licensed: GPL v3 or commercial (gzac5314@gmail.com)
// AMOURANTH FOREVER 💖
// =============================================================================

#pragma once

#include "engine/SDL3.hpp"
#include "engine/ELLIE.hpp"
#include "engine/AMOURANTHRTX.hpp"
#include "engine/OptionsMenu.hpp"
#include "engine/Renderer.hpp"

#include <SDL3/SDL_vulkan.h>
#include <chrono>
#include <memory>

// Global renderer — owns eternal light and all creation
extern std::unique_ptr<VulkanRenderer> renderer;

// Sacrificial Splash — skippable with any input
static inline void showSacrificialSplash() noexcept;