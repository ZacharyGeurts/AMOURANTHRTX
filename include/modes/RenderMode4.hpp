// include/modes/RenderMode4.hpp
// AMOURANTH RTX — MODE 4: MATERIAL VARIANTS
// PURE RAY TRACING. SPHERE MATERIALS. ROUGH/METAL/EMISSIVE.

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
 * @brief Render Mode 4 — Material Variants
 *
 * Features:
 *  • Three spheres: rough, metallic, emissive
 *  • 4 bounces (complex GI)
 *  • Env map + emissive glow
 *  • 16 SPP for high quality
 *  • Push constant: bounces=4, spp=16
 *  • Uses material SSBO for PBR params
 */
void renderMode4(
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