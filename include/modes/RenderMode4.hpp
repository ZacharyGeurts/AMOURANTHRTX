// include/modes/RenderMode4.hpp
// AMOURANTH RTX — MODE 4: VOLUMETRIC FOG + GOD RAYS
// C++23: Full Vulkan types, namespace-qualified Context, proper includes
// FIXED: Missing <cstdint>, <vulkan/vulkan.h>, ::Vulkan::Context forward decl
// @ZacharyGeurts — 11:05 PM EST, Nov 6 2025

#pragma once
#ifndef RENDERMODE4_HPP
#define RENDERMODE4_HPP

#include <vulkan/vulkan.h>
#include <cstdint>

namespace Vulkan { struct Context; }

namespace VulkanRTX {

void renderMode4(
    uint32_t imageIndex,
    VkCommandBuffer commandBuffer,
    VkPipelineLayout pipelineLayout,
    VkDescriptorSet descriptorSet,
    VkPipeline pipeline,
    float deltaTime,
    ::Vulkan::Context& context
);

} // namespace VulkanRTX

#endif // RENDERMODE4_HPP