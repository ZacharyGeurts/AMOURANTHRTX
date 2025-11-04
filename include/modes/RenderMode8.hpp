// include/modes/RenderMode8.hpp
// AMOURANTH RTX — MODE 8: SHADOW RAYS
// PURE RAY TRACING. HARD SHADOWS. SHADOW MISS SBT.

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/RTConstants.hpp"
#include "engine/logging.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

// Forward declare for header
namespace Vulkan { class Context; }

namespace VulkanRTX {

/**
 * @brief Render Mode 8 — Shadow Rays
 *
 * Features:
 *  • Multi-materials + shadow rays
 *  • 3 bounces + hard shadows
 *  • Env + direct light shadows
 *  • 24 SPP
 *  • Push constant: bounces=3, spp=24, shadows=1
 *  • Uses shadow miss/anyhit SBT
 */
void renderMode8(
    uint32_t imageIndex,
    [[maybe_unused]] VkBuffer vertexBuffer,
    VkCommandBuffer commandBuffer,
    [[maybe_unused]] VkBuffer indexBuffer,
    float zoomLevel,
    int width,
    int height,
    float wavePhase,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkDevice device,
    [[maybe_unused]] VkDeviceMemory vertexBufferMemory,
    VkPipeline pipeline,
    float deltaTime,
    Vulkan::Context& context
);

} // namespace VulkanRTX