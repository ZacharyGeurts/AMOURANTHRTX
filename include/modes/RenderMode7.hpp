// include/modes/RenderMode7.hpp
// AMOURANTH RTX — MODE 7: GLOBAL ILLUMINATION
// PURE RAY TRACING. FULL PATH. INFINITE BOUNCES. REALISTIC LIGHT.

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
 * @brief Render Mode 7 — Global Illumination
 *
 * Features:
 *  • Multi-materials + full path tracing
 *  • Infinite bounces (russian roulette)
 *  • Env + emissive GI
 *  • 32 SPP high quality
 *  • Push constant: bounces=∞, spp=32, gi=1
 */
void renderMode7(
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