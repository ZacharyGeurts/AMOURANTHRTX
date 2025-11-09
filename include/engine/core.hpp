// include/engine/core.hpp
// AMOURANTH RTX Engine – November 08 2025 – Core Systems Header
// Professional, minimal, no disposal dependencies – forward declarations only
// Global destruction counter extern – guardian removed to eliminate Context dependency

#pragma once

#include "GLOBAL/StoneKey.hpp"
#include "GLOBAL/logging.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_beta.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdint>
#include <source_location>

using namespace Logging::Color;

// Global destruction counter – defined in Dispose.cpp
extern uint64_t g_destructionCounter;

// Forward declarations
struct Context;
class VulkanRenderer;
class VulkanPipelineManager;
class VulkanRTX;

struct RTConstants;

// Render mode function declarations
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

// Global render mode dispatcher
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

    switch (renderMode) {
        [[likely]] case 1: renderMode1(imageIndex, commandBuffer, pipelineLayout, descriptorSet, pipeline, deltaTime, context); break;
        [[likely]] case 2: renderMode2(imageIndex, commandBuffer, pipelineLayout, descriptorSet, pipeline, deltaTime, context); break;
        [[likely]] case 3: renderMode3(imageIndex, commandBuffer, pipelineLayout, descriptorSet, pipeline, deltaTime, context); break;
        case 4: renderMode4(imageIndex, commandBuffer, pipelineLayout, descriptorSet, pipeline, deltaTime, context); break;
        case 5: renderMode5(imageIndex, commandBuffer, pipelineLayout, descriptorSet, pipeline, deltaTime, context); break;
        case 6: renderMode6(imageIndex, commandBuffer, pipelineLayout, descriptorSet, pipeline, deltaTime, context); break;
        case 7: renderMode7(imageIndex, commandBuffer, pipelineLayout, descriptorSet, pipeline, deltaTime, context); break;
        case 8: renderMode8(imageIndex, commandBuffer, pipelineLayout, descriptorSet, pipeline, deltaTime, context); break;
        case 9: renderMode9(imageIndex, commandBuffer, pipelineLayout, descriptorSet, pipeline, deltaTime, context); break;
        [[unlikely]] default:
            LOG_WARNING_CAT("Renderer", "Invalid render mode {} at {}:{} – Objects destroyed: {} – StoneKey: 0x{:X}-0x{:X}",
                            renderMode, loc.file_name(), loc.line(), g_destructionCounter,
                            kStone1, kStone2);
            renderMode1(imageIndex, commandBuffer, pipelineLayout, descriptorSet, pipeline, deltaTime, context);
            break;
    }
}

// Compile-time mode validation
template<int Mode>
[[nodiscard]] consteval bool is_valid_mode() noexcept {
    static_assert(Mode >= 1 && Mode <= 9, "Render mode must be in range [1,9]");
    return true;
}

// Pipeline manager accessor
inline VulkanPipelineManager* getPipelineManager() {
    static VulkanPipelineManager* mgr = nullptr;
    if (!mgr) {
        LOG_ERROR_CAT("Core", "getPipelineManager() returned nullptr – initialization required – StoneKey: 0x{:X}-0x{:X} – Destroyed: {}",
                      kStone1, kStone2, g_destructionCounter);
    }
    return mgr;
}