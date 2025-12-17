// src/modes/RenderMode1.cpp
// PURE PINK VOID — FULL-SCREEN SACRED PINK — DECEMBER 16, 2025 — ETERNAL FIX

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
    VkClearColorValue pink{{1.0f, 0.0f, 0.5f, 1.0f}};
    VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    const uint32_t imageCount = StoneKey::stone_image_count();
    const uint32_t imageIdx   = frameIndex % imageCount;
    VkImage swapImage         = StoneKey::stone_images()[imageIdx];

    // CRITICAL FIX: Use UNDEFINED as old layout — Vulkan discards old contents safely
    // This works 100% reliably regardless of previous frame state
    VkImageMemoryBarrier toGeneral{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = 0,
        .dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED,           // ← SAFE & CORRECT
        .newLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = swapImage,
        .subresourceRange    = range
    };

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &toGeneral);

    // Clear to sacred pink
    vkCmdClearColorImage(cmd, swapImage, VK_IMAGE_LAYOUT_GENERAL, &pink, 1, &range);

    // Transition back to present — required for vkQueuePresentKHR
    VkImageMemoryBarrier toPresent{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask       = 0,
        .oldLayout           = VK_IMAGE_LAYOUT_GENERAL,
        .newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = swapImage,
        .subresourceRange    = range
    };

    vkCmdPipelineBarrier(cmd,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                         0, 0, nullptr, 0, nullptr, 1, &toPresent);

    LOG_TRACE_CAT("RENDERER", "Pure pink void rendered — frame {} — image {} — photons eternal", frameIndex, imageIdx);
}

void RenderMode1::onResize(uint32_t w, uint32_t h)
{
    width_  = w;
    height_ = h;
    LOG_INFO_CAT("RTX", "Pure pink mode resized → {}×{} — the void expands eternally", w, h);
}