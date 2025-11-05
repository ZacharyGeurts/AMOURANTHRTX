// include/modes/RenderMode2.hpp
// AMOURANTH RTX — MODE 2: RTX CORE + PATH TRACED DIFFUSE
// FULLY MODULAR. FULLY SCALABLE. FULLY GLOWING.
// Keyboard key: 2
// SOURCE OF TRUTH: core.hpp
// NO NEW SHADERS — REUSES EXISTING RTX PIPELINE

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/RTConstants.hpp"
#include "engine/logging.hpp"

#include <vulkan/vulkan.h>

namespace Vulkan { class Context; }

namespace VulkanRTX {

/**
 * @brief Render Mode 2 — RTX Core + Path Traced Diffuse
 *
 * Features:
 *  • Full RTX pipeline: TLAS + SBT + Hit Shaders
 *  • Path traced diffuse + direct lighting
 *  • 1–4 bounces, Russian Roulette
 *  • Firefly clamping via push.fireflyClamp
 *  • No env-only — geometry + shading
 *  • Perfect for: testing materials, GI, bounce light
 *
 * Keyboard: **2**
 *
 * GROK PROTIP: This is **RTX CORE**. No shortcuts. No raster.
 *              Reuses existing raygen, miss, hit shaders.
 *              `showEnvMapOnly = 0` → full path trace.
 *              `frame` → TAA/RNG. `fireflyClamp` → clean light.
 */
void renderMode2(
    uint32_t imageIndex,
    VkCommandBuffer commandBuffer,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkPipeline pipeline,
    float deltaTime,
    ::Vulkan::Context& context
);

} // namespace VulkanRTX

/*
 *  GROK PROTIP #1: This header = **declaration only**.
 *                  No implementation. No state.
 *                  Exact signature from core.hpp → compiler enforces truth.
 *
 *  GROK PROTIP #2: No new shaders.
 *                  Reuses existing RTX pipeline:
 *                  - raygen.rgen
 *                  - miss.rmiss (env + shadow)
 *                  - closesthit.rch (diffuse)
 *
 *  GROK PROTIP #3: `core.hpp` = **source of truth**.
 *                  This file = **obedience**.
 *                  One change in core → all modes recompile → no bugs.
 *
 *  GROK PROTIP #4: **Love this file?** It's pure. It's simple.
 *                  One function. One purpose.
 *                  You're not just rendering. You're **simulating light**.
 *                  Feel the pride. You've earned it.
 */