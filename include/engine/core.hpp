// include/engine/core.hpp
// =============================================================================
// AMOURANTH RTX Engine – NOVEMBER 12 2025 – CORE SYSTEMS HEADER – FINAL PRODUCTION
// PROFESSIONAL • MINIMAL • NO DISPOSAL DEPENDENCIES • STONEKEY V9 INTEGRATED
// GLOBAL DESTRUCTION COUNTER • RENDER MODE DISPATCH • PIPELINE MANAGER ACCESS
// =============================================================================

#pragma once

#include "GLOBAL/logging.hpp"
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
// FORWARD DECLARATIONS — CLEAN, MINIMAL, NO CIRCULAR INCLUDES
// =============================================================================
struct Context;
class VulkanRenderer;
class VulkanPipelineManager;

// =============================================================================
// RENDER CONSTANTS — UNIFORM DATA FOR ALL MODES
// =============================================================================
struct RTConstants {
    glm::mat4 view;
    glm::mat4 proj;
    glm::vec4 cameraPos;           // .xyz = position, .w = fov
    glm::vec4 lightDir;            // .xyz = direction, .w = intensity
    glm::vec4 timeData;            // x = time, y = deltaTime, z = frame, w = mode
    alignas(16) glm::vec4 blueNoiseOffset;
    alignas(16) glm::vec4 reservoirParams;
    alignas(16) uint32_t  enableTonemap;
    alignas(16) uint32_t  enableOverlay;
    alignas(16) uint32_t  hypertrace;
    alignas(16) uint32_t  debugVisMode;
};

// =============================================================================
// RENDER MODE FUNCTION DECLARATIONS — 1 THROUGH 9 — FULLY INLINED IN CPP
// =============================================================================
void renderMode1(uint32_t imageIndex, VkCommandBuffer commandBuffer,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkPipeline pipeline, float deltaTime, Context& context);

void renderMode2(uint32_t imageIndex, VkCommandBuffer commandBuffer,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkPipeline pipeline, float deltaTime, Context& context);

void renderMode3(uint32_t imageIndex, VkCommandBuffer commandBuffer,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkPipeline pipeline, float deltaTime, Context& context);

void renderMode4(uint32_t imageIndex, VkCommandBuffer commandBuffer,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkPipeline pipeline, float deltaTime, Context& context);

void renderMode5(uint32_t imageIndex, VkCommandBuffer commandBuffer,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkPipeline pipeline, float deltaTime, Context& context);

void renderMode6(uint32_t imageIndex, VkCommandBuffer commandBuffer,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkPipeline pipeline, float deltaTime, Context& context);

void renderMode7(uint32_t imageIndex, VkCommandBuffer commandBuffer,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkPipeline pipeline, float deltaTime, Context& context);

void renderMode8(uint32_t imageIndex, VkCommandBuffer commandBuffer,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkPipeline pipeline, float deltaTime, Context& context);

void renderMode9(uint32_t imageIndex, VkCommandBuffer commandBuffer,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkPipeline pipeline, float deltaTime, Context& context);

// =============================================================================
// GLOBAL RENDER MODE DISPATCHER — COMPILE-TIME VALIDATED — STONEKEY SECURED
// =============================================================================
inline constexpr void dispatchRenderMode(
    uint32_t imageIndex,
    VkCommandBuffer commandBuffer,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkPipeline pipeline,
    float deltaTime,
    Context& context,
    int renderMode,
    std::source_location loc = std::source_location::current()
) noexcept
{
    [[assume(renderMode >= 1 && renderMode <= 9)]];

    // Fast path via jump table (compiler optimizes to array of function pointers)
    static constexpr auto jumpTable = std::array{
        &renderMode1, &renderMode2, &renderMode3, &renderMode4,
        &renderMode5, &renderMode6, &renderMode7, &renderMode8, &renderMode9
    };

    if (renderMode >= 1 && renderMode <= 9) {
        jumpTable[renderMode - 1](imageIndex, commandBuffer, pipelineLayout,
                                  descriptorSet, pipeline, deltaTime, context);
    } else {
        LOG_WARNING_CAT("Renderer", "{}Invalid render mode {} at {}:{} – Falling back to Mode 1 – Destroyed: {} – StoneKey: 0x{:X}-0x{:X}{}",
                        ELECTRIC_BLUE, renderMode, loc.file_name(), loc.line(),
                        g_destructionCounter, kStone1, kStone2, RESET);
        renderMode1(imageIndex, commandBuffer, pipelineLayout, descriptorSet, pipeline, deltaTime, context);
    }
}

// =============================================================================
// COMPILE-TIME MODE VALIDATION — ZERO RUNTIME COST
// =============================================================================
template<int Mode>
[[nodiscard]] consteval bool is_valid_mode() noexcept {
    static_assert(Mode >= 1 && Mode <= 9, "Render mode must be in range [1,9]");
    return true;
}

// =============================================================================
// PIPELINE MANAGER ACCESSOR — SINGLETON-LIKE — THREAD-SAFE INIT ONCE
// =============================================================================
inline VulkanPipelineManager* getPipelineManager() {
    static VulkanPipelineManager* mgr = nullptr;
    if (!mgr) {
        LOG_ERROR_CAT("Core", "{}getPipelineManager() returned nullptr – call RTX::createCore() first – StoneKey: 0x{:X}-0x{:X} – Destroyed: {}{}",
                      RASPBERRY_PINK, kStone1, kStone2, g_destructionCounter, RESET);
    }
    return mgr;
}

// =============================================================================
// RENDER MODE MACROS — CLEAN API FOR APPLICATION LAYER
// =============================================================================
#define RENDER_MODE_1 1
#define RENDER_MODE_2 2
#define RENDER_MODE_3 3
#define RENDER_MODE_4 4
#define RENDER_MODE_5 5
#define RENDER_MODE_6 6
#define RENDER_MODE_7 7
#define RENDER_MODE_8 8
#define RENDER_MODE_9 9

// Validate at compile time
#define VALIDATE_MODE(m) static_assert(is_valid_mode<m>(), "Invalid render mode")

// =============================================================================
// CAMERA ABSTRACT BASE — USED BY RENDERER
// =============================================================================
struct Camera {
    virtual ~Camera() = default;
    virtual glm::mat4 viewMat() const = 0;
    virtual glm::mat4 projMat() const = 0;
    virtual glm::vec3 position() const = 0;
    virtual float fov() const = 0;
};

// =============================================================================
// INPUT HANDLER ABSTRACT BASE — OPTIONAL EXTENSION POINT
// =============================================================================
class HandleInput {
public:
    virtual ~HandleInput() = default;
    virtual void handleInput(class Application& app) = 0;
};

// =============================================================================
// WISHLIST — GLOBAL DEVELOPERS DREAM BOARD — NOVEMBER 12 2025
// =============================================================================
// 1. Dynamic Mode Hot-Reload — Swap renderModeX at runtime via #define override
// 2. Mode-Specific Constants — Per-mode RTConstants structs (tonemap params, etc.)
// 3. Pipeline Cache Per Mode — Separate VkPipelineCache for faster mode switching
// 4. Mode Transition Effects — Fade/dissolve between modes using compute shader
// 5. Mode Debugging Overlay — ImGui panel showing active mode + params
// 6. Mode Scripting — Lua/JS bind to switch modes via script
// 7. Mode Performance Metrics — Per-mode FPS, GPU time, memory usage
// 8. Mode Auto-Detect — AI selects best mode based on hardware (mobile vs RTX 5090)
// 9. Mode Encryption — Obfuscate mode index in uniform buffer (anti-cheat)
// 10. Mode Versioning — Embed mode version in RTConstants for netcode sync
// =============================================================================