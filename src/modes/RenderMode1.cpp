// src/modes/RenderMode1.cpp
// =============================================================================
// AMOURANTH RTX Engine © 2025 — RENDER MODE 1 — SACRED PINK VOID
// =============================================================================

#include "modes/RenderMode1.hpp"

#include "engine/GLOBAL/logging.hpp"
#include "engine/GLOBAL/StoneKey.hpp"

using namespace Logging::Color;

// Standalone image transition — safe and self-contained
static void transitionImage(VkCommandBuffer cmd,
                            VkImage image,
                            VkImageLayout oldLayout,
                            VkImageLayout newLayout,
                            VkAccessFlags srcAccess,
                            VkAccessFlags dstAccess,
                            VkPipelineStageFlags srcStage,
                            VkPipelineStageFlags dstStage) noexcept
{
    if (image == VK_NULL_HANDLE || cmd == VK_NULL_HANDLE) return;

    VkImageMemoryBarrier barrier{
        .sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask       = srcAccess,
        .dstAccessMask       = dstAccess,
        .oldLayout           = oldLayout,
        .newLayout           = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image               = image,
        .subresourceRange    = {
            .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel   = 0,
            .levelCount     = 1,
            .baseArrayLayer = 0,
            .layerCount     = 1
        }
    };

    vkCmdPipelineBarrier(
        cmd,
        srcStage,
        dstStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

RenderMode1::RenderMode1(uint32_t width, uint32_t height)
    : width_(width), height_(height)
{
    LOG_AMOURANTH("RENDER MODE 1 AWAKENS — SACRED PINK VOID — THE EMPIRE RESTS IN ETERNAL LIGHT");
}

void RenderMode1::renderFrame(VkCommandBuffer cmd,
                              uint32_t frameIndex,
                              uint32_t imageIndex,
                              float /*deltaTime*/) noexcept
{
    const uint32_t imageCount = StoneKey::stone_image_count();
    if (imageCount == 0) return;

    VkImage swapImage = StoneKey::stone_images()[imageIndex];

    LOG_TRACE_CAT("MODE1", "Painting sacred pink void — frame {} → swapchain image {}", frameIndex, imageIndex);

    // Transition to transfer destination (safe from any previous layout)
    transitionImage(cmd,
                    swapImage,
                    VK_IMAGE_LAYOUT_UNDEFINED,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    0,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                    VK_PIPELINE_STAGE_TRANSFER_BIT);

    // Clear to sacred pink
    VkClearColorValue sacredPink{{1.0f, 0.0f, 0.5f, 1.0f}};
    VkImageSubresourceRange range{
        .aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel   = 0,
        .levelCount     = 1,
        .baseArrayLayer = 0,
        .layerCount     = 1
    };

    vkCmdClearColorImage(cmd, swapImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &sacredPink, 1, &range);

    // Transition back to present
    transitionImage(cmd,
                    swapImage,
                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                    VK_ACCESS_TRANSFER_WRITE_BIT,
                    0,
                    VK_PIPELINE_STAGE_TRANSFER_BIT,
                    VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
}

void RenderMode1::onResize(uint32_t newWidth, uint32_t newHeight) noexcept
{
    width_  = newWidth;
    height_ = newHeight;
    LOG_INFO_CAT("MODE1", "Sacred pink void resized → {}×{}", newWidth, newHeight);
}