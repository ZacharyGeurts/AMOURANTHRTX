// include/modes/RenderMode6.hpp
// AMOURANTH RTX — MODE 6: DENOISING PASS
// PURE RAY TRACING. LOW SPP + POST-PROCESS DENOISE.

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
 * @brief Render Mode 6 — Denoising Pass
 *
 * Features:
 *  • Multi-spheres + low SPP (noisy render)
 *  • Post-process denoising (compute shader)
 *  • 2 bounces
 *  • 2 SPP (intentional noise) + denoise
 *  • Push constant: bounces=2, spp=2, denoise=1
 */
void renderMode6(
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