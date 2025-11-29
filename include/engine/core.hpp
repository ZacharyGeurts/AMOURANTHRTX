// =============================================================================
// include/engine/core.hpp
// AMOURANTH RTX © 2025 — VALHALLA v999 — FIRST LIGHT ETERNAL — C++23 — SDL3 — Vulkan 1.4+
// RESPECTS ORIGINAL kStone1/kStone2 FROM logging.hpp — NO CONFLICTS — PURE EMPIRE
// =============================================================================

#pragma once

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/camera.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdint>
#include <source_location>
#include <array>

using namespace Logging::Color;

// =============================================================================
// GLOBAL DESTRUCTION COUNTER — DEFINED IN Dispose.cpp
// =============================================================================
extern uint64_t g_destructionCounter;

// =============================================================================
// FORWARD DECLARATIONS — CLEAN AND ETERNAL
// =============================================================================
class VulkanRenderer;
class VulkanPipelineManager;

// =============================================================================
// RENDER CONTEXT — THE ONE TRUE CONTEXT FOR ALL RENDER MODES
// =============================================================================
struct RenderContext {
    glm::vec3 cameraPos{};
    float     fov = 75.0f;
    float     deltaTime = 0.0f;
    uint32_t  frame = 0;
    uint32_t  renderMode = 1;
    uint32_t  enableTonemap = 1;
    uint32_t  enableOverlay = 0;
    uint32_t  hypertrace = 1;
    uint32_t  debugVisMode = 0;
    glm::vec2 blueNoiseOffset{};
    glm::vec4 reservoirParams{};
};

// =============================================================================
// RT CONSTANTS — UNIFORM BLOCK FOR ALL SHADERS
// =============================================================================
struct RTConstants {
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 cameraPos;           // .xyz = pos, .w = fov
    glm::vec4 lightDir;            // .xyz = dir, .w = intensity
    glm::vec4 timeData;            // x = time, y = deltaTime, z = frame, w = mode
    alignas(16) glm::vec4 blueNoiseOffset;
    alignas(16) glm::vec4 reservoirParams;
    alignas(16) uint32_t  enableTonemap;
    alignas(16) uint32_t  enableOverlay;
    alignas(16) uint32_t  hypertrace;
    alignas(16) uint32_t  debugVisMode;
};

// =============================================================================
// RENDER MODE DECLARATIONS — 1 THROUGH 9
// =============================================================================
void renderMode1(uint32_t imageIndex, VkCommandBuffer cmd,
                 VkPipelineLayout layout, VkDescriptorSet set,
                 VkPipeline pipeline, float dt, RenderContext& ctx);

void renderMode2(uint32_t imageIndex, VkCommandBuffer cmd,
                 VkPipelineLayout layout, VkDescriptorSet set,
                 VkPipeline pipeline, float dt, RenderContext& ctx);

void renderMode3(uint32_t imageIndex, VkCommandBuffer cmd,
                 VkPipelineLayout layout, VkDescriptorSet set,
                 VkPipeline pipeline, float dt, RenderContext& ctx);

void renderMode4(uint32_t imageIndex, VkCommandBuffer cmd,
                 VkPipelineLayout layout, VkDescriptorSet set,
                 VkPipeline pipeline, float dt, RenderContext& ctx);

void renderMode5(uint32_t imageIndex, VkCommandBuffer cmd,
                 VkPipelineLayout layout, VkDescriptorSet set,
                 VkPipeline pipeline, float dt, RenderContext& ctx);

void renderMode6(uint32_t imageIndex, VkCommandBuffer cmd,
                 VkPipelineLayout layout, VkDescriptorSet set,
                 VkPipeline pipeline, float dt, RenderContext& ctx);

void renderMode7(uint32_t imageIndex, VkCommandBuffer cmd,
                 VkPipelineLayout layout, VkDescriptorSet set,
                 VkPipeline pipeline, float dt, RenderContext& ctx);

void renderMode8(uint32_t imageIndex, VkCommandBuffer cmd,
                 VkPipelineLayout layout, VkDescriptorSet set,
                 VkPipeline pipeline, float dt, RenderContext& ctx);

void renderMode9(uint32_t imageIndex, VkCommandBuffer cmd,
                 VkPipelineLayout layout, VkDescriptorSet set,
                 VkPipeline pipeline, float dt, RenderContext& ctx);

// =============================================================================
// GLOBAL RENDER MODE DISPATCHER — C++23 — RESPECTS YOUR kStone1/kStone2
// =============================================================================
inline constexpr void dispatchRenderMode(
    uint32_t imageIndex,
    VkCommandBuffer cmd,
    VkPipelineLayout layout,
    VkDescriptorSet set,
    VkPipeline pipeline,
    float dt,
    RenderContext& ctx,
    int mode,
    std::source_location loc = std::source_location::current()
) noexcept
{
    [[assume(mode >= 1 && mode <= 9)]];

    using ModeFn = void(*)(uint32_t, VkCommandBuffer, VkPipelineLayout, VkDescriptorSet, VkPipeline, float, RenderContext&);

    static constexpr std::array<ModeFn, 9> jumpTable{
        renderMode1, renderMode2, renderMode3, renderMode4,
        renderMode5, renderMode6, renderMode7, renderMode8, renderMode9
    };

    if (mode >= 1 && mode <= 9) [[likely]] {
        jumpTable[mode - 1](imageIndex, cmd, layout, set, pipeline, dt, ctx);
    } else {
        LOG_WARNING_CAT("Renderer",
            "{}Invalid render mode {} at {}:{} — falling back to Mode 1 — StoneKey: 0x{:016X}{}",
            ELECTRIC_BLUE, mode, loc.file_name(), loc.line(),
            (kStone1 ^ kStone2), RESET);
        renderMode1(imageIndex, cmd, layout, set, pipeline, dt, ctx);
    }
}

// =============================================================================
// COMPILE-TIME MODE VALIDATION
// =============================================================================
template<int Mode>
[[nodiscard]] consteval bool is_valid_mode() noexcept {
    static_assert(Mode >= 1 && Mode <= 9, "Render mode must be 1–9");
    return true;
}

// =============================================================================
// RENDER MODE CONSTANTS
// =============================================================================
inline constexpr int RENDER_MODE_1 = 1;
inline constexpr int RENDER_MODE_2 = 2;
inline constexpr int RENDER_MODE_3 = 3;
inline constexpr int RENDER_MODE_4 = 4;
inline constexpr int RENDER_MODE_5 = 5;
inline constexpr int RENDER_MODE_6 = 6;
inline constexpr int RENDER_MODE_7 = 7;
inline constexpr int RENDER_MODE_8 = 8;
inline constexpr int RENDER_MODE_9 = 9;

#define VALIDATE_MODE(m) static_assert(is_valid_mode<m>())

// =============================================================================
// FINAL WORD — NOVEMBER 29 2025
// • Respects your original kStone1/kStone2 from logging.hpp
// • No redefinition conflicts
// • dispatchRenderMode uses your real StoneKey values
// • Ready for CAM + RTX::Handle<T>
// =============================================================================

// PINK PHOTONS ETERNAL
// FIRST LIGHT ACHIEVED
// YOUR EMPIRE IS WHOLE AGAIN
// SHIP IT RAW
// SHIP IT NOW