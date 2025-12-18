// =============================================================================
// AMOURANTH RTX Engine © 2025 — PURE PINK VOID — SELF-CONTAINED & MINIMAL
// Renders a solid pink screen using the unified tonemap compute pipeline
// Fully integrated with core systems: VulkanRenderer, PipelineManager, UBO, BufferManager, LAS, SwapchainManager
// Ready as a development launch point and reliable fallback mode
// =============================================================================

#include "modes/RenderMode1.hpp"

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/UBO.hpp"
#include "engine/GLOBAL/BufferManager.hpp"
#include "engine/GLOBAL/LAS.hpp"
#include "engine/GLOBAL/SwapchainManager.hpp"

RenderMode1::RenderMode1(uint32_t w, uint32_t h)
    : width_(w), height_(h)
{
    LOG_INFO_CAT("RTX", "RenderMode1 initialized — pure pink void via tonemap compute pipeline");
}

void RenderMode1::renderFrame(VkCommandBuffer cmd, uint32_t frameIndex, float /*deltaTime*/)
{
    const uint32_t imageCount = StoneKey::stone_image_count();
    const uint32_t imageIdx   = frameIndex % imageCount;
    VkImage        swapImage  = StoneKey::stone_images()[imageIdx];
    VkImageView    swapView   = StoneKey::stone_views()[imageIdx];

    auto& rtx = g_rtx();  // Access to VulkanRenderer singleton

    // Transition swapchain image to GENERAL layout for compute shader write
    rtx.transitionImage(cmd,
                        swapImage,
                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        VK_IMAGE_LAYOUT_GENERAL,
                        0,
                        VK_ACCESS_SHADER_WRITE_BIT,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    // Bind the tonemap compute pipeline (created and owned by VulkanRenderer)
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, rtx.tonemapPipeline());

    // Push constant: solid pink color — w > 0 enables solid color override in shader
    struct PinkPush {
        glm::vec4 color{1.0f, 0.0f, 0.5f, 1.0f};
    } push{};

    vkCmdPushConstants(cmd,
                       rtx.tonemapLayout(),
                       VK_SHADER_STAGE_COMPUTE_BIT,
                       0,
                       sizeof(PinkPush),
                       &push);

    // Update tonemap descriptor set to write directly into the current swapchain image
    VkDescriptorSet tonemapSet = rtx.tonemapSet(frameIndex % 2);

    VkDescriptorImageInfo outputInfo{
        .imageView   = swapView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkWriteDescriptorSet write{
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = tonemapSet,
        .dstBinding      = 1,  // Storage image output (binding 1 in tonemap layout)
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo      = &outputInfo
    };
    vkUpdateDescriptorSets(StoneKey::stone_device(), 1, &write, 0, nullptr);

    // Bind the updated tonemap descriptor set
    vkCmdBindDescriptorSets(cmd,
                            VK_PIPELINE_BIND_POINT_COMPUTE,
                            rtx.tonemapLayout(),
                            0,
                            1,
                            &tonemapSet,
                            0,
                            nullptr);

    // Full-screen dispatch covering the entire swapchain image
    const uint32_t wgX = (width_  + 15) / 16;
    const uint32_t wgY = (height_ + 15) / 16;
    vkCmdDispatch(cmd, wgX, wgY, 1);

    // Transition back to PRESENT_SRC_KHR for presentation
    rtx.transitionImage(cmd,
                        swapImage,
                        VK_IMAGE_LAYOUT_GENERAL,
                        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                        VK_ACCESS_SHADER_WRITE_BIT,
                        0,
                        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}

void RenderMode1::onResize(uint32_t w, uint32_t h)
{
    width_  = w;
    height_ = h;
    LOG_INFO_CAT("RTX", "RenderMode1 resized to {}×{}", w, h);
}