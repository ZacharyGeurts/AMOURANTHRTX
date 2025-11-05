// include/modes/RenderMode3.hpp
// AMOURANTH RTX — MODE 3: VOLUMETRIC LIGHT + DENSITY FIELD
// FULLY MODULAR. FULLY SCALABLE. FULLY GLOWING.
// Keyboard key: 3
// SOURCE OF TRUTH: core.hpp

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/RTConstants.hpp"

#include <vulkan/vulkan.h>

namespace Vulkan { class Context; }

namespace VulkanRTX {

void renderMode3(
    uint32_t imageIndex,
    VkCommandBuffer commandBuffer,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkPipeline pipeline,
    float deltaTime,
    ::Vulkan::Context& context
);

} // namespace VulkanRTX