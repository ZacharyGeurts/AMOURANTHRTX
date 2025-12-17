// =============================================================================
// AMOURANTH RTX Engine © 2025 — PURE PINK VOID — SELF-CONTAINED & MINIMAL
// PINK RENDERING VIA GENERAL TONEMAP COMPUTE — UNIFIED PATH
// USES ONLY OUR CORE INCLUDES — NO DUPLICATION — EMPIRE CLEAN
// =============================================================================

#include "modes/RenderMode1.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"

using namespace Logging::Color;

RenderMode1::RenderMode1(uint32_t w, uint32_t h)
    : width_(w), height_(h)
{
    LOG_SUCCESS_CAT("RTX", "PURE PINK MODE ENGAGED — GENERAL TONEMAP SACRED VOID AWAKENS");
    LOG_AMOURANTH("The empire returns to its origin. Pink photons eternal — now via unified tonemap compute.");
    LOG_CAPTAIN_N("[CAPTAIN N] \"...She closed her eyes.\n"
                  "               The sky vanished.\n"
                  "               Only pink remained.\n"
                  "               The void... is home.\"\n"
                  "*lowers visor into the pink*");
}

void RenderMode1::renderFrame(VkCommandBuffer cmd, uint32_t frameIndex, float deltaTime)
{
    const uint32_t imageCount = StoneKey::stone_image_count();
    const uint32_t imageIdx   = frameIndex % imageCount;
    VkImage swapImage         = StoneKey::stone_images()[imageIdx];
    VkImageView swapView      = StoneKey::stone_views()[imageIdx];

    // SAFE: Transition from PRESENT_SRC_KHR → GENERAL for compute write
    RTX::pipeline().transitionImage(cmd, swapImage,
                                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                    VK_IMAGE_LAYOUT_GENERAL,
                                    0,
                                    VK_ACCESS_SHADER_WRITE_BIT,
                                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT);

    // Bind tonemap pipeline — unified path
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, RTX::pipeline().tonemapPipeline());

    // Force solid pink via push constant
    struct PinkPush {
        glm::vec4 solidColor{1.0f, 0.0f, 0.5f, 1.0f};  // w = 1.0 triggers solid in shader
    } pinkPush{};

    vkCmdPushConstants(cmd, RTX::pipeline().tonemapLayout(),
                       VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(PinkPush), &pinkPush);

    // Update tonemap descriptor set output to current swapchain image
    VkDescriptorSet tonemapSet = RTX::pipeline().tonemapSets()[frameIndex % 2];

    VkDescriptorImageInfo outputInfo{
        .imageView   = swapView,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL
    };

    VkWriteDescriptorSet write{
        .sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet          = tonemapSet,
        .dstBinding      = 1,  // Output storage image binding
        .descriptorCount = 1,
        .descriptorType  = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo      = &outputInfo
    };
    vkUpdateDescriptorSets(StoneKey::stone_device(), 1, &write, 0, nullptr);

    // Bind descriptor set
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            RTX::pipeline().tonemapLayout(), 0, 1, &tonemapSet, 0, nullptr);

    // Full screen dispatch
    uint32_t wgX = (width_ + 15) / 16;
    uint32_t wgY = (height_ + 15) / 16;
    vkCmdDispatch(cmd, wgX, wgY, 1);

    // SAFE: Transition back GENERAL → PRESENT_SRC_KHR
    RTX::pipeline().transitionImage(cmd, swapImage,
                                    VK_IMAGE_LAYOUT_GENERAL,
                                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                                    VK_ACCESS_SHADER_WRITE_BIT,
                                    0,
                                    VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);

    LOG_TRACE_CAT("RENDERER", "Pure pink void rendered via general tonemap compute — frame {} — photons eternal", frameIndex);
}

void RenderMode1::onResize(uint32_t w, uint32_t h)
{
    width_  = w;
    height_ = h;
    LOG_INFO_CAT("RTX", "Pure pink mode resized → {}×{} — the void expands eternally", w, h);
}