// include/modes/RenderMode2.hpp
// AMOURANTH RTX — MODE 2: BASIC PATH TRACING
// PURE RAY TRACING. SINGLE SPHERE. DIRECT + ENV LIGHTING.

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
 * @brief Render Mode 2 — Basic Path Tracing (Sphere + Env)
 *
 * Features:
 *  • Single diffuse sphere at origin
 *  • 1-2 bounces (direct + indirect)
 *  • Env map lighting
 *  • Russian roulette termination
 *  • 4 SPP for noise reduction
 *  • Push constant: bounces=2, spp=4
 */
void renderMode2(
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