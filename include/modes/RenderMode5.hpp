// include/modes/RenderMode5.hpp
// AMOURANTH RTX — MODE 5: FLOATING FLAME
// CLEAN HEADER: No namespace conflicts, pure forward decl

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"

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