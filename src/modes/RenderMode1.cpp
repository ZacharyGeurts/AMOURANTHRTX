// src/modes/RenderMode1.cpp
// PURE ENVMAP DISPLAY — FULL-SCREEN HDR SKY — DECEMBER 15, 2025

#include "modes/RenderMode1.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/RTXHandler.hpp"
#include "engine/GLOBAL/VulkanRenderer.hpp"
#include "engine/GLOBAL/PipelineManager.hpp"
#include "engine/GLOBAL/StoneKey.hpp"  // for stone_images()

using namespace Logging::Color;

RenderMode1::RenderMode1(uint32_t w, uint32_t h)
    : width_(w), height_(h)
{
    LOG_SUCCESS_CAT("RTX", "ENVMAP DISPLAY MODE ENGAGED — THE VOID IS ILLUMINATED");
    LOG_AMOURANTH("The sky is real. The empire gazes into the infinite.");
    LOG_CAPTAIN_N("[CAPTAIN N] \"...She looked up.\n"
                  "               The stars were true.\n"
                  "               The void had light.\n"
                  "               There is a sky.\"\n"
                  "*raises visor to the heavens*");
}

void VulkanRenderer::createSyncObjects() noexcept
{
    LOG_INFO_CAT("RENDERER", "Forging synchronization objects — 2 frames in flight — the empire beats as one");

    const uint32_t frames = 2;  // Hardcoded — 2026 MASTERMIND

    imageAvailableSemaphores_.resize(frames);
    renderFinishedSemaphores_.resize(frames);
    inFlightFences_.resize(frames);

    VkSemaphoreCreateInfo semaphoreInfo{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo fenceInfo{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT  // Start signaled so first frame doesn't wait
    };

    for (uint32_t i = 0; i < frames; ++i)
    {
        VK_CHECK(vkCreateSemaphore(stone_device(), &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]));
        VK_CHECK(vkCreateSemaphore(stone_device(), &semaphoreInfo, nullptr, &renderFinishedSemaphores_[i]));
        VK_CHECK(vkCreateFence(stone_device(), &fenceInfo, nullptr, &inFlightFences_[i]));

        LOG_TRACE_CAT("SYNC", "Sync objects forged for frame slot %u", i);
    }

    LOG_SUCCESS_CAT("RENDERER", "Synchronization objects complete — 2 semaphores + 2 fences — the rhythm is eternal");
}

void RenderMode1::renderFrame(VkCommandBuffer cmd, uint32_t frameIndex, float deltaTime)
{
    // Use dedicated envmap display pipeline if available
    if (RTX::pipeline().hasEnvMapDisplayPipeline())
    {
        g_rtx().recordEnvMapOnlyPass(cmd, frameIndex);
        LOG_TRACE_CAT("RENDERER", "Envmap display pass executed — frame %u", frameIndex);
        return;
    }

    // Fallback: solid pink if envmap pipeline not ready
    VkClearColorValue pink{{1.0f, 0.0f, 0.5f, 1.0f}};
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    // Get current swapchain image
    VkImage swapImage = StoneKey::stone_images()[frameIndex % StoneKey::stone_image_count()];

    // Transition to GENERAL for clear
    VkImageMemoryBarrier barrier{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = swapImage,
        .subresourceRange    = range
    };
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    vkCmdClearColorImage(cmd, swapImage, VK_IMAGE_LAYOUT_GENERAL, &pink, 1, &range);

    // Transition back to PRESENT
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = 0;
    barrier.oldLayout     = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    LOG_WARN_CAT("RENDERER", "Envmap display pipeline not ready — showing pink void (frame %u)", frameIndex);
}

void RenderMode1::onResize(uint32_t w, uint32_t h)
{
    width_  = w;
    height_ = h;
    LOG_INFO_CAT("RTX", "Envmap display resized → {}×{} — the heavens expand", w, h);
}