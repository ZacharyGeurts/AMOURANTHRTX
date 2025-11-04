// include/modes/RenderMode5.hpp
// AMOURANTH RTX — MODE 5: VOLUMETRIC FOG
// PURE RAY TRACING. SPHERES + DENSITY VOLUME. MISTY GLOW.

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
 * @brief Render Mode 5 — Volumetric Fog
 *
 * Features:
 *  • Spheres + volumetric density field
 *  • 4 bounces with volume sampling
 *  • Env + volumetric scattering
 *  • 12 SPP
 *  • Push constant: bounces=4, spp=12, volumetric=1
 *  • Uses density volume binding
 */
void renderMode5(
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