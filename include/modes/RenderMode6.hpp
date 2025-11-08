// include/modes/RenderMode6.hpp
// AMOURANTH RTX — MODE 6: PATH TRACED GLOBAL ILLUMINATION
// FULLY MODULAR. FULLY SCALABLE. FULLY GLOWING.
// Keyboard key: 6
// SOURCE OF TRUTH: core.hpp

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"

#include <vulkan/vulkan.h>

namespace Vulkan { class Context; }

namespace VulkanRTX {

void renderMode6(
    uint32_t imageIndex,
    VkCommandBuffer commandBuffer,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkPipeline pipeline,
    float deltaTime,
    ::Vulkan::Context& context
);

} // namespace VulkanRTX