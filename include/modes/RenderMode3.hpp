// include/modes/RenderMode3.hpp
// AMOURANTH RTX — MODE 3: MULTI-SPHERE SCENE
// PURE RAY TRACING. THREE SPHERES. ENV + DIRECT LIGHT.

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
 * @brief Render Mode 3 — Multi-Sphere Scene
 *
 * Features:
 *  • Three diffuse spheres (red, green, blue)
 *  • 3 bounces (more GI)
 *  • Env map + directional light
 *  • 8 SPP for smoother noise
 *  • Push constant: bounces=3, spp=8
 */
void renderMode3(
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