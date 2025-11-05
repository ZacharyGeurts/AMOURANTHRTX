// include/modes/RenderMode7.hpp
// AMOURANTH RTX — MODE 7: CAUSTICS + WATER
// Keyboard key: 7
// SOURCE OF TRUTH: core.hpp

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/RTConstants.hpp"

#include <vulkan/vulkan.h>

namespace Vulkan { class Context; }

namespace VulkanRTX {

void renderMode7(
    uint32_t imageIndex,
    VkCommandBuffer commandBuffer,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkPipeline pipeline,
    float deltaTime,
    ::Vulkan::Context& context
);

} // namespace VulkanRTX