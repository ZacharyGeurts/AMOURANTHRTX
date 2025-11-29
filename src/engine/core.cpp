// src/engine/core.cpp
// THE ONE FILE THAT ENDS THE LINKER'S REIGN OF TERROR
// ALL 9 MODES DEFINED — EMPIRE COMPLETE — FIRST LIGHT ETERNAL

#include "engine/core.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

using namespace Logging::Color;

void renderMode1(uint32_t i, VkCommandBuffer c, VkPipelineLayout l, VkDescriptorSet s, VkPipeline p, float d, RenderContext& x) {
    vkCmdBindPipeline(c, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, p);
    vkCmdBindDescriptorSets(c, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, l, 0, 1, &s, 0, nullptr);
    // your real trace rays call here
}

void renderMode2(uint32_t, VkCommandBuffer, VkPipelineLayout, VkDescriptorSet, VkPipeline, float, RenderContext&) { LOG_INFO("Mode 2: Debug View"); }
void renderMode3(uint32_t, VkCommandBuffer, VkPipelineLayout, VkDescriptorSet, VkPipeline, float, RenderContext&) { LOG_INFO("Mode 3: Reservoirs"); }
void renderMode4(uint32_t, VkCommandBuffer, VkPipelineLayout, VkDescriptorSet, VkPipeline, float, RenderContext&) { LOG_INFO("Mode 4: Albedo/Normal"); }
void renderMode5(uint32_t, VkCommandBuffer, VkPipelineLayout, VkDescriptorSet, VkPipeline, float, RenderContext&) { LOG_INFO("Mode 5: Motion Vectors"); }
void renderMode6(uint32_t, VkCommandBuffer, VkPipelineLayout, VkDescriptorSet, VkPipeline, float, RenderContext&) { LOG_INFO("Mode 6: Depth"); }
void renderMode7(uint32_t, VkCommandBuffer, VkPipelineLayout, VkDescriptorSet, VkPipeline, float, RenderContext&) { LOG_INFO("Mode 7: Adaptive Sampling"); }
void renderMode8(uint32_t, VkCommandBuffer, VkPipelineLayout, VkDescriptorSet, VkPipeline, float, RenderContext&) { LOG_INFO("Mode 8: HyperTrace Score"); }
void renderMode9(uint32_t i, VkCommandBuffer c, VkPipelineLayout, VkDescriptorSet, VkPipeline, float, RenderContext&) {
    VkClearColorValue pink{1.0f, 0.4f, 0.8f, 1.0f};
    VkImageSubresourceRange r{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(c, StoneKey::stone_images()[i], VK_IMAGE_LAYOUT_GENERAL, &pink, 1, &r);
}