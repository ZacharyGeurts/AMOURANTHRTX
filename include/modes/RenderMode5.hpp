// include/modes/RenderMode5.hpp
// AMOURANTH RTX — MODE 5: GLOSSY REFLECTIONS + METALNESS
// Keyboard key: 5
// SOURCE OF TRUTH: core.hpp

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/RTConstants.hpp"

#include <vulkan/vulkan.h>

namespace Vulkan { class Context; }

namespace VulkanRTX {

void renderMode5(
    uint32_t imageIndex,
    VkCommandBuffer commandBuffer,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkPipeline pipeline,
    float deltaTime,
    ::Vulkan::Context& context
);

} // namespace VulkanRTX