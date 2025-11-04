// include/modes/RenderMode9.hpp
// AMOURANTH RTX — MODE 9: ACCUMULATION + FULL GI
// PURE RAY TRACING. INFINITE ACCUM. REAL-TIME GI.

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
 * @brief Render Mode 9 — Accumulation + Full GI
 *
 * Features:
 *  • All materials + infinite accumulation
 *  • 1024+ frames accum for noise-free
 *  • Full path + volumetric GI
 *  • 64 SPP base + accum
 *  • Push constant: bounces=∞, spp=64, accum=1
 *  • Ping-pong accum images
 */
void renderMode9(
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