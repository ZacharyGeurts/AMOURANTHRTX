// include/modes/RenderMode1.hpp
// AMOURANTH RTX — MODE 1: ENVIRONMENT MAP ONLY
// PURE RAY TRACING. NO GEOMETRY. NO BOUNCES. JUST GLOW.

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/RTConstants.hpp"
#include "engine/logging.hpp"

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

// Forward declare for header (no full include needed)
namespace Vulkan { class Context; }

namespace VulkanRTX {

/**
 * @brief Render Mode 1 — Environment Map Only
 *
 * Features:
 *  • Pure environment map sampling
 *  • No geometry, no TLAS, no hit shaders
 *  • Direct equirectangular lookup
 *  • Perfect for skyboxes, HDRIs, and GI-only preview
 *  • Ultra-fast dispatch (1 ray per pixel)
 *  • Full push constant control
 */
void renderMode1(
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
    Vulkan::Context& context  // QUALIFIED: Vulkan::Context&
);

} // namespace VulkanRTX