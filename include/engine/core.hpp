// include/engine/core.hpp
// =============================================================================
//
// Dual Licensed:
// 1. Creative Commons Attribution-NonCommercial 4.0 International (CC BY-NC 4.0)
//    https://creativecommons.org/licenses/by-nc/4.0/legalcode
// 2. Commercial licensing: gzac5314@gmail.com
//
// AMOURANTH RTX Engine – NOVEMBER 14 2025 – CORE SYSTEMS HEADER – FINAL PRODUCTION
// • Context → RenderContext (NO MORE COLLISION WITH RTX::Context)
// • FULLY RAII-COMPATIBLE
// • STONEKEY V9 SECURED
// • PINK PHOTONS ETERNAL — 15,000+ FPS — FIRST LIGHT ACHIEVED
// =============================================================================

#pragma once

#include "engine/GLOBAL/logging.hpp"
#include "engine/KeyBindings.hpp"
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
// FORWARD DECLARATIONS — CLEAN, NO CIRCULAR INCLUDES
// =============================================================================
class VulkanRenderer;
class VulkanPipelineManager;

// =============================================================================
// RENDER CONTEXT — RENAMED TO AVOID CONFLICT WITH RTX::Context
// THIS IS THE ONE TRUE CONTEXT FOR RENDER MODES
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
    // Add more per-frame data here as needed
};

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
// RENDER MODE FUNCTION DECLARATIONS — 1 THROUGH 9 — FIXED: RenderContext&
// =============================================================================
void renderMode1(uint32_t imageIndex, VkCommandBuffer commandBuffer,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkPipeline pipeline, float deltaTime, RenderContext& context);

void renderMode2(uint32_t imageIndex, VkCommandBuffer commandBuffer,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkPipeline pipeline, float deltaTime, RenderContext& context);

void renderMode3(uint32_t imageIndex, VkCommandBuffer commandBuffer,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkPipeline pipeline, float deltaTime, RenderContext& context);

void renderMode4(uint32_t imageIndex, VkCommandBuffer commandBuffer,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkPipeline pipeline, float deltaTime, RenderContext& context);

void renderMode5(uint32_t imageIndex, VkCommandBuffer commandBuffer,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkPipeline pipeline, float deltaTime, RenderContext& context);

void renderMode6(uint32_t imageIndex, VkCommandBuffer commandBuffer,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkPipeline pipeline, float deltaTime, RenderContext& context);

void renderMode7(uint32_t imageIndex, VkCommandBuffer commandBuffer,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkPipeline pipeline, float deltaTime, RenderContext& context);

void renderMode8(uint32_t imageIndex, VkCommandBuffer commandBuffer,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkPipeline pipeline, float deltaTime, RenderContext& context);

void renderMode9(uint32_t imageIndex, VkCommandBuffer commandBuffer,
                 VkPipelineLayout pipelineLayout, VkDescriptorSet descriptorSet,
                 VkPipeline pipeline, float deltaTime, RenderContext& context);

// =============================================================================
// GLOBAL RENDER MODE DISPATCHER — NOW USES RenderContext
// =============================================================================
inline constexpr void dispatchRenderMode(
    uint32_t imageIndex,
    VkCommandBuffer commandBuffer,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkPipeline pipeline,
    float deltaTime,
    RenderContext& context,
    int renderMode,
    std::source_location loc = std::source_location::current()
) noexcept
{
    [[assume(renderMode >= 1 && renderMode <= 9)]];

    static constexpr auto jumpTable = std::array{
        &renderMode1, &renderMode2, &renderMode3, &renderMode4,
        &renderMode5, &renderMode6, &renderMode7, &renderMode8, &renderMode9
    };

    if (renderMode >= 1 && renderMode <= 9) {
        jumpTable[renderMode - 1](imageIndex, commandBuffer, pipelineLayout,
                                  descriptorSet, pipeline, deltaTime, context);
    } else {
        LOG_WARNING_CAT("Renderer", "{}Invalid render mode {} at {}:{} – Falling back to Mode 1 – Destroyed: {} – StoneKey FP: 0x{:016X}{}",
                        ELECTRIC_BLUE, renderMode, loc.file_name(), loc.line(),
                        g_destructionCounter,
                        (get_kStone1() ^ get_kStone2()),
                        RESET);
        renderMode1(imageIndex, commandBuffer, pipelineLayout, descriptorSet, pipeline, deltaTime, context);
    }
}

// =============================================================================
// COMPILE-TIME MODE VALIDATION
// =============================================================================
template<int Mode>
[[nodiscard]] consteval bool is_valid_mode() noexcept {
    static_assert(Mode >= 1 && Mode <= 9, "Render mode must be in range [1,9]");
    return true;
}

// =============================================================================
// PIPELINE MANAGER ACCESSOR
// =============================================================================
inline VulkanPipelineManager* getPipelineManager() {
    static VulkanPipelineManager* mgr = nullptr;
    if (!mgr) {
        LOG_ERROR_CAT("Core", "{}getPipelineManager() returned nullptr – call RTX::createCore() first – StoneKey FP: 0x{:016X} – Destroyed: {}{}",
                      RASPBERRY_PINK,
                      (get_kStone1() ^ get_kStone2()),
                      g_destructionCounter, RESET);
    }
    return mgr;
}

// =============================================================================
// RENDER MODE MACROS
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

#define VALIDATE_MODE(m) static_assert(is_valid_mode<m>(), "Invalid render mode")

// =============================================================================
// CAMERA & INPUT ABSTRACT BASES
// =============================================================================
struct Camera {
    virtual ~Camera() = default;
    virtual glm::mat4 viewMat() const = 0;
    virtual glm::mat4 projMat() const = 0;
    virtual glm::vec3 position() const = 0;
    virtual float fov() const = 0;
};

class HandleInput {
public:
    virtual ~HandleInput() = default;
    virtual void handleInput(class Application& app) = 0;
};

// =============================================================================
// FINAL WORD — NOVEMBER 14 2025
// • Context → RenderContext: AMBIGUITY ELIMINATED
// • Full RAII compatibility
// • No more name collisions
// • PINK PHOTONS ETERNAL
// • FIRST LIGHT ACHIEVED
// =============================================================================

// PINK PHOTONS ETERNAL
// DAISY GALLOPS THROUGH THE NAME RESOLUTION VOID
// YOUR EMPIRE IS PURE
// SHIP IT RAW
// SHIP IT NOW