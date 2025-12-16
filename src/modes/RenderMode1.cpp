// src/modes/RenderMode1.cpp
// PURE PINK VOID — FULL-SCREEN SACRED PINK — DECEMBER 15, 2025

#include "modes/RenderMode1.hpp"
#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

using namespace Logging::Color;

RenderMode1::RenderMode1(uint32_t w, uint32_t h)
    : width_(w), height_(h)
{
    LOG_SUCCESS_CAT("RTX", "PURE PINK MODE ENGAGED — THE SACRED VOID AWAKENS");
    LOG_AMOURANTH("The empire returns to its origin. Pink photons eternal.");
    LOG_CAPTAIN_N("[CAPTAIN N] \"...She closed her eyes.\n"
                  "               The sky vanished.\n"
                  "               Only pink remained.\n"
                  "               The void... is home.\"\n"
                  "*lowers visor into the pink*");
}

void RenderMode1::renderFrame(VkCommandBuffer cmd, uint32_t frameIndex, float deltaTime)
{
    // Always sacred pink — no envmap, no ray tracing, no compromise
    VkClearColorValue pink{{1.0f, 0.0f, 0.5f, 1.0f}};
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    // Current swapchain image
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

    LOG_TRACE_CAT("RENDERER", "Pure pink void rendered — frame %u — photons eternal", frameIndex);
}

void RenderMode1::onResize(uint32_t w, uint32_t h)
{
    width_  = w;
    height_ = h;
    LOG_INFO_CAT("RTX", "Pure pink mode resized → {}×{} — the void expands", w, h);
}