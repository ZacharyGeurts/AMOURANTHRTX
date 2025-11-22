// =============================================================================
// main.hpp — AMOURANTH RTX Engine © 2025
// Central include: ALL .cpp files include this FIRST
// Defines: VK_CHECK, AI_INJECT + core engine headers
// =============================================================================
#pragma once

// -----------------------------------------------------------------------------
// 1. CORE DEPENDENCIES FOR MACROS
// -----------------------------------------------------------------------------
#include <vulkan/vulkan.h>
#include <source_location>
#include <format>
#include <iostream>
#include <random>
#include "engine/GLOBAL/OptionsMenu.hpp"   // for ENABLE_INFO

// -----------------------------------------------------------------------------
// 2. ENGINE-WIDE MACROS — DEFINED HERE, VISIBLE EVERYWHERE
// -----------------------------------------------------------------------------
// VulkanCore.hpp and logging.hpp

// -----------------------------------------------------------------------------
// 3. CORE ENGINE INCLUDES — AFTER MACROS (safe order)
// -----------------------------------------------------------------------------
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/SDL3.hpp"  // For IMG_Load, IMG_Init/Quit, and error handling
// ... add more global headers here as needed ...

// -----------------------------------------------------------------------------
// 4. FORWARD DECLARATIONS / HELPERS
// -----------------------------------------------------------------------------
namespace RTX { struct Context; Context& g_ctx(); }