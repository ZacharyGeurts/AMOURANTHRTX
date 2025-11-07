// include/modes/RenderMode2.hpp
// AMOURANTH RTX — MODE 2 HEADER
// CLEAN: No namespace redeclaration, only forward decl

#pragma once

#include "engine/Vulkan/VulkanCommon.hpp"
#include "engine/Vulkan/VulkanCore.hpp"
#include "engine/RTConstants.hpp"

namespace VulkanRTX {

void renderMode2(
    uint32_t imageIndex,
    VkCommandBuffer commandBuffer,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkPipeline pipeline,
    float deltaTime,
    ::Vulkan::Context& context
);

} // namespace VulkanRTX